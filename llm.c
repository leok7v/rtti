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

const char *llm_get_models(const char *backend) {
  if (strcmp(backend, "gemini") == 0) {
    return "{\"object\": \"list\", \"data\": [ {\"id\": \"models/gemini-2.5-flash\", \"object\": \"model\", \"owned_by\": \"google\", \"display_name\": \"Gemini 2.5 Flash\"} ]}";
  } else if (strcmp(backend, "groq") == 0) {
    return "{\"object\":\"list\",\"data\":[{\"id\":\"openai/gpt-oss-120b\",\"object\":\"model\",\"created\":1754408224,\"owned_by\":\"OpenAI\",\"active\":true,\"context_window\":131072,\"public_apps\":null,\"max_completion_tokens\":65536}]}";
  } else if (strcmp(backend, "openrouter") == 0) {
    return "{\"object\":\"list\",\"data\":[{\"id\":\"openrouter/hunter-alpha\",\"canonical_slug\":\"openrouter/hunter-alpha\",\"hugging_face_id\":\"\",\"name\":\"Hunter Alpha\",\"created\":1773260671,\"description\":\"Hunter Alpha is a 1 Trillion parameter + 1M token context frontier intelligence model built for agentic use.\",\"context_length\":1048576,\"architecture\":{\"modality\":\"text->text\",\"tokenizer\":\"Other\",\"instruct_type\":null},\"pricing\":{\"prompt\":\"0\",\"completion\":\"0\",\"request\":\"0\",\"image\":\"0\",\"web_search\":\"0\",\"internal_reasoning\":\"0\"},\"top_provider\":{\"context_length\":1048576,\"max_completion_tokens\":65536,\"is_moderated\":false},\"per_request_limits\":null,\"default_parameters\":{\"temperature\":1,\"top_p\":0.95,\"top_k\":null,\"frequency_penalty\":null,\"presence_penalty\":null,\"repetition_penalty\":null},\"expiration_date\":null}]}";
  } else if (strcmp(backend, "cerebras") == 0) {
    return "{\"object\":\"list\",\"data\":[{\"id\":\"llama3.1-8b\",\"object\":\"model\",\"created\":0,\"owned_by\":\"Cerebras\"},{\"id\":\"gpt-oss-120b\",\"object\":\"model\",\"created\":0,\"owned_by\":\"Cerebras\"}]}";
  }
  return "[]";
}

void chat_completion_free(void *handle) { free(handle); }
