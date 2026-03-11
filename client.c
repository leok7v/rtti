#include "client.h"
#include "client.rtti.h"
#include "models.h"
#include "models.rtti.h"
#include "codable.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct point foo(struct point param) {
  (void)param;
  return (struct point){.x = 1, .y = 2};
}

/* Returns 1 if the model id is selected for the given backend. */
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
  /* groq / openrouter / cerebras: gpt-oss variants */
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
  const char *chosen = NULL;
  if (list.data) {
    for (struct model *m = list.data; m->id; m++) {
      if (select_model(backend, m->id)) {
        printf("  Selected model: %s\n", m->id);
        chosen = m->id;
        break;
      }
    }
  }
  if (!chosen) {
    printf("  [no matching model found]\n\n");
    free(parsed);
    if (list.data) {
      free(list.data);
    }
    return;
  }
  char *resp = server_chat_completion(backend, chosen,
                                      "Say hello in one sentence.");
  if (resp) {
    struct completion comp = {0};
    char *comp_parsed = decode(&comp, (void *)completion_rtti, resp);
    if (comp_parsed) {
      if (comp.choices && comp.choices[0].message.content) {
        printf("  Model:  %s\n", comp.model ? comp.model : chosen);
        printf("  Reply:  %s\n", comp.choices[0].message.content);
        if (comp.choices[0].message.reasoning) {
          printf("  Reason: %.80s...\n", comp.choices[0].message.reasoning);
        }
        printf("  Tokens: prompt=%lld completion=%lld total=%lld\n",
               comp.usage.prompt_tokens,
               comp.usage.completion_tokens,
               comp.usage.total_tokens);
      } else {
        printf("  [no content in decoded response]\n");
        printf("  Raw: %.200s\n", resp);
      }
      if (comp.choices) {
        free(comp.choices);
      }
      free(comp_parsed);
    } else {
      printf("  Raw reply: %.200s\n", resp);
    }
    free(resp);
  } else {
    printf("  [chat completion failed]\n");
  }
  free(parsed);
  if (list.data) {
    free(list.data);
  }
  printf("\n");
}

int main(void) {
  const char *backends[] = {
    "simulated", "ollama", "gemini", "groq", "openrouter", "cerebras"
  };
  for (int i = 0; i < 6; i++) {
    run_backend(backends[i]);
  }
  return 0;
}
