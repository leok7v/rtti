#include "client.h"
#include "client.rtti.h"
#include "codable.h"
#include "models.h"
#include "models.rtti.h"
#include "server.h"
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct str {
    char *data;
    size_t count;
    size_t capacity;
};

static void str_escape(char *dst, const char *src, size_t max) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 3 < max; si++) {
        if (src[si] == '\n') {
            dst[di++] = '\\'; dst[di++] = 'n';
        } else if (src[si] == '"') {
            dst[di++] = '\\'; dst[di++] = '"';
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static void str_append(struct str *s, const char *text) {
    if (text && *text) {
        size_t len = strlen(text);
        if (s->count + len + 1 > s->capacity) {
            s->capacity = (s->count + len + 1) * 2 + 64;
            s->data = realloc(s->data, s->capacity);
        }
        if (s->data) {
            memcpy(s->data + s->count, text, len);
            s->count += len;
            s->data[s->count] = '\0';
        }
    }
}

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec * 1e-9;
}

static int select_model(const char *backend, const char *id) {
    int r = 0;
    if (id) {
        if (strcmp(backend, "simulated") == 0) {
            r = strncmp(id, "simulated/gpt-oss", 17) == 0;
        } else if (strcmp(backend, "ollama") == 0) {
            r = strncmp(id, "gpt-oss:", 8) == 0;
        } else if (strcmp(backend, "gemini") == 0) {
            r = strstr(id, "gemma-3-27b") != NULL;
        } else if (strcmp(backend, "groq") == 0) {
            r = (strstr(id, "gpt-oss-120b") != NULL ||
                 strstr(id, "gpt-oss-20b") != NULL) &&
                strstr(id, "safeguard") == NULL;
        } else if (strcmp(backend, "openrouter") == 0) {
            r = strstr(id, "gpt-oss") != NULL &&
                strstr(id, "safeguard") == NULL;
        } else if (strcmp(backend, "github") == 0) {
            r = strstr(id, "gpt-4o") != NULL;
        } else {
            r = strstr(id, "gpt-oss") != NULL;
        }
    }
    return r;
}

struct model_response {
    struct str content;
    struct str reasoning;
    int reasoning_token_count;
    int content_token_count;
    struct completion_usage usage;
    double ttft;
    double start_time;
};

struct free_tier_result {
    char backend[32];
    char model_id[128];
    double ttft;
    double pp;
    double tg;
    char reason[128];
    char content[128];
};

static struct free_tier_result results[512];
static int n_results = 0;

static int is_free(const char *backend, struct model *m) {
    if (strcmp(backend, "gemini") == 0) {
        return strstr(m->id, "gemini-2.5-flash") ||
               strstr(m->id, "gemini-robotics") ||
               strstr(m->id, "gemma");
    }
    if (strcmp(backend, "openrouter") == 0) {
        if (strstr(m->id, ":free")) { return 1; }
        return m->pricing.prompt && strcmp(m->pricing.prompt, "0") == 0 &&
               m->pricing.completion && strcmp(m->pricing.completion, "0") == 0 &&
               m->pricing.request && strcmp(m->pricing.request, "0") == 0 &&
               m->pricing.image && strcmp(m->pricing.image, "0") == 0 &&
               m->pricing.web_search && strcmp(m->pricing.web_search, "0") == 0 &&
               m->pricing.internal_reasoning &&
               strcmp(m->pricing.internal_reasoning, "0") == 0;
    }
    if (strcmp(backend, "github") == 0) {
        return strncmp(m->id, "openai/gpt-", 11) == 0 ||
               strncmp(m->id, "meta/llama-", 11) == 0 ||
               strncmp(m->id, "mistral-ai/", 11) == 0 ||
               strncmp(m->id, "ai21-labs/", 10) == 0;
    }
    if (strcmp(backend, "simulated") == 0) {
        return strstr(m->id, "gpt-oss") || strstr(m->id, "gemma");
    }
    if (strcmp(backend, "ollama") == 0 ||
        strncmp(backend, "http://", 7) == 0 ||
        strncmp(backend, "https://", 8) == 0)
    {
        return strstr(m->id, "gpt-oss-20b") || strstr(m->id, "gemma");
    }
    return strcmp(backend, "groq") == 0 || strcmp(backend, "cerebras") == 0;
}

static void try_model(const char *backend, const char *id,
                       struct model_response *r) {
    memset(r, 0, sizeof(*r));
    printf("  Model: %s\n", id);
    const char *prm = "Say hello in one sentence.";
    FILE *fp = server_open_stream(backend, id, prm);
    if (fp) {
        r->start_time = seconds();
        char line[8192];
        int first = 1;
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "data: ", 6) == 0) {
                char *json = line + 6;
                size_t len = strlen(json);
                while (len > 0 &&
                       (json[len-1] == '\n' || json[len-1] == '\r'))
                {
                    json[--len] = '\0';
                }
                if (strcmp(json, "[DONE]") == 0) { break; }
                struct response_chunk chunk = {0};
                void *m = decode(&chunk, (void *)response_chunk_rtti, json);
                if (m && chunk.choices) {
                    struct choice *ch = &chunk.choices[0];
                    const char *rc = ch->delta.reasoning_content;
                    if (!rc) { rc = ch->delta.reasoning; }
                    const char *c = ch->delta.content;
                    if (rc) {
                        str_append(&r->reasoning, rc);
                        r->reasoning_token_count++;
                    }
                    if (c) {
                        if (first) {
                            r->ttft = seconds() - r->start_time;
                            first = 0;
                        }
                        str_append(&r->content, c);
                        r->content_token_count++;
                        if (r->content.data) {
                            printf("\r  > %-72.72s", r->content.data);
                            fflush(stdout);
                        }
                    }
                    if (chunk.usage.total_tokens > 0) {
                        r->usage = chunk.usage;
                    }
                    free(chunk.choices);
                    free(m);
                }
            }
        }
        pclose(fp);
    }
    printf("\r%80s\r", "");
}

static void run_backend(const char *backend, int bench) {
    printf("=== %s === %s\n", backend, server_get_url(backend));
    char *models_json = server_get_models(backend);
    if (!models_json) {
        printf("  [no response]\n\n");
    } else {
        struct model_list list = {0};
        char *r_decoded = decode(&list, (void *)model_list_rtti, models_json);
        free(models_json);
        if (!r_decoded) {
            printf("  [failed to decode model list]\n\n");
        } else {
            const char *candidates[128] = {0};
            int n_cand = 0;
            if (list.data) {
                for (struct model *m = list.data; m->id && n_cand < 128; m++) {
                    if (bench) {
                        if (is_free(backend, m)) {
                            candidates[n_cand++] = m->id;
                        }
                    } else if (select_model(backend, m->id)) {
                        candidates[n_cand++] = m->id;
                    }
                }
            }
            if (!n_cand) {
                printf("  [no matching model]\n\n");
            } else {
                int cycles = bench ? 3 : 1;
                for (int ci = 0; ci < n_cand && (bench || ci == 0); ci++) {
                    double t_ttft = 0, t_pp = 0, t_tg = 0;
                    int valid_cycles = 0;
                    struct model_response r = {0};
                    if (!bench) { printf("  Warmup...\n");
                                  server_probe(backend, candidates[ci]); }
                    for (int cycle = 0; cycle < cycles; cycle++) {
                        try_model(backend, candidates[ci], &r);
                        if (r.content_token_count > 0) {
                            double elap = seconds() - r.start_time;
                            double tg = (elap > r.ttft) ?
                                        r.content_token_count / (elap - r.ttft)
                                        : 0;
                            double pp = 0;
                            if (r.usage.prompt_time > 0) {
                                pp = r.usage.prompt_tokens / r.usage.prompt_time;
                            } else if (r.ttft > 0 && r.usage.prompt_tokens > 0) {
                                pp = r.usage.prompt_tokens / r.ttft;
                            } else if (r.ttft > 0) {
                                pp = (strlen("Say hello...") / 3.5) / r.ttft;
                            }
                            t_ttft += r.ttft; t_pp += pp; t_tg += tg;
                            valid_cycles++;
                        }
                        if (cycle < cycles - 1) {
                            free(r.content.data); free(r.reasoning.data);
                        }
                    }
                    if (valid_cycles > 0) {
                        if (bench && n_results < 512 &&
                            strcmp(backend, "simulated") != 0 &&
                            r.content_token_count > 0)
                        {
                            struct free_tier_result *res = &results[n_results++];
                            strncpy(res->backend, backend, 31);
                            strncpy(res->model_id, candidates[ci], 127);
                            res->ttft = t_ttft / valid_cycles;
                            res->pp = t_pp / valid_cycles;
                            res->tg = t_tg / valid_cycles;
                            if (r.reasoning.data) {
                                strncpy(res->reason, r.reasoning.data, 127);
                            }
                            if (r.content.data) {
                                strncpy(res->content, r.content.data, 127);
                            }
                        }
                        if (r.reasoning.data && r.reasoning.count > 0) {
                            printf("  reason:  %.70s%s\n", r.reasoning.data,
                                   r.reasoning.count > 70 ? "..." : "");
                        }
                        if (r.content.data && r.content.count > 0) {
                            printf("  reply:   %s\n", r.content.data);
                        }
                        printf("  tokens:  reasoning=%'d content=%'d",
                               r.reasoning_token_count, r.content_token_count);
                        if (t_pp > 0) {
                            printf("   pp: ~%'d", (int)(t_pp / valid_cycles));
                        }
                        printf("  tg: %'d t/s\n", (int)(t_tg / valid_cycles));
                        printf("  ttft:    %.2fs (time to first token)\n\n",
                               t_ttft / valid_cycles);
                        if (!bench) { break; }
                    }
                    free(r.content.data);
                    free(r.reasoning.data);
                }
            }
            free(r_decoded);
            if (list.data) { free(list.data); }
        }
    }
}

int main(int argc, char **argv) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");
    int bench = (argc > 1 && strcmp(argv[1], "--all") == 0);
    const char *backends[] = {
        "simulated", "ollama", "gemini", "groq", "openrouter", "cerebras",
        "github"
    };
    for (int i = 0; i < 7; i++) { run_backend(backends[i], bench); }
    if (bench) {
        for (int i = 1; i <= 16; i++) {
            char name[32];
            snprintf(name, sizeof(name), "LOCAL_HOST_%d", i);
            char *val = getenv(name);
            if (!val) { break; }
            run_backend(val, bench);
        }
    }
    if (bench && n_results > 0) {
        FILE *f = fopen("free.csv", "w");
        if (f) {
            fprintf(f, "model_id,ttft,pp,tg,backend,content,reason\n");
            for (int i = 0; i < n_results; i++) {
                char esc_c[256], esc_r[256];
                str_escape(esc_c, results[i].content, 32);
                str_escape(esc_r, results[i].reason, 32);
                fprintf(f, "%s,%.2f,%.0f,%.0f,%s,\"%s\",\"%s\"\n",
                        results[i].model_id, results[i].ttft, results[i].pp,
                        results[i].tg, results[i].backend, esc_c, esc_r);
            }
            fclose(f);
            printf("Saved results to free.csv\n");
        }
    }
    return 0;
}
