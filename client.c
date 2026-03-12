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

struct point foo(struct point param) {
  (void)param;
  return (struct point){.x = 1, .y = 2};
}

struct str {
  char *data;
  int32_t count;
  int32_t capacity;
};

static void str_push(struct str *s, const char *text) {
  if (!text || !*text) {
    return;
  }
  int32_t len = (int32_t)strlen(text);
  if (s->count + len + 1 > s->capacity) {
    s->capacity = (s->count + len + 1) * 2 + 64;
    s->data = realloc(s->data, (size_t)s->capacity);
  }
  if (s->data) {
    memcpy(s->data + s->count, text, (size_t)len);
    s->count += len;
    s->data[s->count] = '\0';
  }
}

static double mono_sec(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + t.tv_nsec * 1e-9;
}

static int select_model(const char *backend, const char *id) {
  if (!id) {
    return 0;
  }
  if (strcmp(backend, "simulated") == 0) {
    return strncmp(id, "simulated/gpt-oss", 17) == 0;
  }
  if (strcmp(backend, "ollama") == 0) {
    return strncmp(id, "gpt-oss:", 8) == 0;
  }
  if (strcmp(backend, "gemini") == 0) {
    return strstr(id, "gemma") != NULL;
  }
  if (strcmp(backend, "openrouter") == 0) {
    return strstr(id, "gpt-oss") != NULL && strstr(id, "safeguard") == NULL;
  }
  return strstr(id, "gpt-oss") != NULL;
}

static void run_backend(const char *backend) {
  printf("=== %s ===\n", backend);
  char *models_json = server_get_models(backend);
  if (!models_json) {
    printf("  [no response]\n\n");
    return;
  }
  struct model_list list = {0};
  char *parsed = decode(&list, (void *)model_list_rtti, models_json);
  free(models_json);
  if (!parsed) {
    printf("  [failed to decode model list]\n\n");
    return;
  }
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
    free(parsed);
    if (list.data) {
      free(list.data);
    }
    return;
  }
  struct str content = {0}, reasoning = {0};
  int32_t r_tok = 0, c_tok = 0;
  struct completion_usage last_usage = {0};
  double t0 = mono_sec(), ttft = 0;
  printf("  Warmup...\n");
  server_probe(backend, candidates[0]);
  for (int ci = 0; ci < n_cand; ci++) {
    content = (struct str){0}; reasoning = (struct str){0};
    r_tok = 0; c_tok = 0; last_usage = (struct completion_usage){0};
    t0 = mono_sec(); ttft = 0;
    int first_c = 1;
    printf("  Model: %s\n", candidates[ci]);
    const char *prompt = "Say hello in one sentence.";
    FILE *fp = server_open_stream(backend, candidates[ci], prompt);
  if (fp) {
    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
      if (strncmp(line, "data: ", 6) != 0) {
        continue;
      }
      char *json = line + 6;
      int len = (int)strlen(json);
      while (len > 0 && (json[len - 1] == '\n' || json[len - 1] == '\r')) {
        json[--len] = '\0';
      }
      if (strcmp(json, "[DONE]") == 0) {
        break;
      }
      struct response_chunk chunk = {0};
      void *mem = decode(&chunk, (void *)response_chunk_rtti, json);
      if (mem && chunk.choices) {
        const char *rc = chunk.choices[0].delta.reasoning_content;
        if (!rc) {
          rc = chunk.choices[0].delta.reasoning;
        }
        const char *c = chunk.choices[0].delta.content;
        if (rc) {
          str_push(&reasoning, rc);
          r_tok++;
        }
        if (c) {
          if (first_c) {
            ttft = mono_sec() - t0;
            first_c = 0;
          }
          str_push(&content, c);
          c_tok++;
          if (content.data) {
            printf("\r  > %-72.72s", content.data);
            fflush(stdout);
          }
        }
        free(chunk.choices);
      }
      if (mem) {
        if (chunk.usage.total_tokens > 0) {
          last_usage = chunk.usage;
        }
        free(mem);
      }
    }
    pclose(fp);
    }
    printf("\r%80s\r", ""); /* clear progress line */
    if (c_tok > 0) {
      break;
    }
    free(content.data); content = (struct str){0};
    free(reasoning.data); reasoning = (struct str){0};
  }
  double elapsed = mono_sec() - t0;
  double tg = (elapsed > ttft && c_tok > 0) ? c_tok / (elapsed - ttft) : 0;
  double pp = 0;
  int pp_exact = 0;
  if (last_usage.prompt_time > 0) {
    pp = last_usage.prompt_tokens / last_usage.prompt_time;
    pp_exact = 1;
  } else if (ttft > 0 && last_usage.prompt_tokens > 0) {
    pp = last_usage.prompt_tokens / ttft;
  } else if (ttft > 0) {
    pp = (strlen("Say hello in one sentence.") / 3.5) / ttft;
  }
  if (content.data && content.count > 0) {
    printf("  reply:   %s\n", content.data);
  }
  if (reasoning.data && reasoning.count > 0) {
    printf("  reason:  %.70s%s\n", reasoning.data,
           reasoning.count > 70 ? "..." : "");
  }
  printf("  tokens:  reasoning=%'d content=%'d\n", r_tok, c_tok);
  printf("  ttft:    %.2fs (time to first token)", ttft);
  if (pp > 0) {
    printf("  pp: %s%'d", pp_exact ? "" : "~", (int)pp);
  }
  printf("  tg: %'d t/s\n", (int)tg);
  free(content.data);
  free(reasoning.data);
  free(parsed);
  if (list.data) {
    free(list.data);
  }
  printf("\n");
}

int main(void) {
  setlocale(LC_NUMERIC, "en_US.UTF-8"); // setlocale(LC_NUMERIC, ""); fails
  const char *backends[] = {"simulated", "ollama",     "gemini",
                            "groq",      "openrouter", "cerebras"};
  for (int i = 0; i < 6; i++) {
    run_backend(backends[i]);
  }
  return 0;
}
