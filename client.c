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

static void run_backend(const char *backend) {
    printf("=== %s ===\n", backend);
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
            const char *candidates[16] = {0};
            int n_cand = 0;
            if (list.data) {
                for (struct model *m = list.data; m->id && n_cand < 16; m++) {
                    if (select_model(backend, m->id)) {
                        candidates[n_cand++] = m->id;
                    }
                }
            }
            if (!n_cand) {
                printf("  [no matching model]\n\n");
            } else {
                struct model_response r = {0};
                printf("  Warmup...\n");
                server_probe(backend, candidates[0]);
                for (int ci = 0; ci < n_cand && r.content_token_count == 0;
                     ci++)
                {
                    try_model(backend, candidates[ci], &r);
                    if (r.content_token_count == 0) {
                        free(r.content.data);
                        free(r.reasoning.data);
                        memset(&r, 0, sizeof(r));
                    }
                }
                double elapsed = seconds() - r.start_time;
                double tg = (elapsed > r.ttft && r.content_token_count > 0) ?
                            r.content_token_count / (elapsed - r.ttft) : 0;
                double pp = 0;
                int exact = 0;
                if (r.usage.prompt_time > 0) {
                    pp = r.usage.prompt_tokens / r.usage.prompt_time;
                    exact = 1;
                } else if (r.ttft > 0 && r.usage.prompt_tokens > 0) {
                    pp = r.usage.prompt_tokens / r.ttft;
                } else if (r.ttft > 0) {
                    pp = (strlen("Say hello in one sentence.") / 3.5) / r.ttft;
                }
                if (r.content.data && r.content.count > 0) {
                    printf("  reply:   %s\n", r.content.data);
                }
                if (r.reasoning.data && r.reasoning.count > 0) {
                    printf("  reason:  %.70s%s\n", r.reasoning.data,
                           r.reasoning.count > 70 ? "..." : "");
                }
                printf("  tokens:  reasoning=%'d content=%'d",
                       r.reasoning_token_count, r.content_token_count);
                if (pp > 0) {
                    printf("   pp: %s%'d", exact ? "" : "~", (int)pp);
                }
                printf("  tg: %'d t/s\n", (int)tg);
                printf("  ttft:    %.2fs (time to first token)\n\n", r.ttft);
                free(r.content.data);
                free(r.reasoning.data);
            }
            free(r_decoded);
            if (list.data) { free(list.data); }
        }
    }
}

int main(void) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");
    const char *backends[] = {
        "simulated", "ollama", "gemini", "groq", "openrouter", "cerebras"
    };
    for (int i = 0; i < 6; i++) { run_backend(backends[i]); }
    return 0;
}
