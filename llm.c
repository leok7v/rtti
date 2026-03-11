#include "llm.h"
#include "client.h"
#include "src/codable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct llm_state {
  int step;
};

extern const struct type_info request_rtti[];

void *chat_completion_request(const char *json) {
  struct request req = {0};
  void *parsed = decode(&req, (void *)request_rtti, (char *)json);
  if (parsed) {
    struct llm_state *s = calloc(1, sizeof(struct llm_state));
    if (s) {
      s->step = 0;
    }
    free(req.messages);
    free(parsed);
    return s;
  }
  return NULL;
}

char *chat_completion_reply(void *handle) {
  struct llm_state *s = handle;
  if (!s) {
    return NULL;
  }
  char *res = NULL;
  if (s->step == 0) {
    res = strdup("data: {\"choices\": [{\"delta\": "
                 "{\"reasoning_content\": "
                 "\"First, I need to define...\"}}]}");
  } else if (s->step == 1) {
    res = strdup("data: {\"choices\": [{\"delta\": "
                 "{\"content\": \"Quantum computing is...\"}}]}");
  } else if (s->step == 2) {
    res = strdup("data: [DONE]");
  }
  s->step++;
  return res;
}

void chat_completion_free(void *handle) { free(handle); }
