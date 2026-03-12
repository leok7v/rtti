#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

static char *read_popen(const char *cmd) {
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    return NULL;
  }
  size_t cap = 4096, len = 0;
  char *buf = malloc(cap);
  if (!buf) {
    pclose(fp);
    return NULL;
  }
  int c;
  while ((c = fgetc(fp)) != EOF) {
    if (len + 1 >= cap) {
      cap *= 2;
      char *tmp = realloc(buf, cap);
      if (!tmp) {
        free(buf);
        pclose(fp);
        return NULL;
      }
      buf = tmp;
    }
    buf[len++] = (char)c;
  }
  buf[len] = '\0';
  pclose(fp);
  return buf;
}

/* Build a curl command that sets Authorization header when key is set. */
static char *make_get_cmd(const char *url, const char *key_env) {
  char cmd[1024];
  const char *key = key_env ? getenv(key_env) : NULL;
  if (key && key[0]) {
    snprintf(cmd, sizeof(cmd),
             "curl -s -H \"Authorization: Bearer %s\" \"%s\"",
             key, url);
  } else {
    snprintf(cmd, sizeof(cmd), "curl -s \"%s\"", url);
  }
  return strdup(cmd);
}

/* ---------- simulated data ---------- */

static const char *sim_models =
  "{\"object\":\"list\",\"data\":["
  "{\"id\":\"simulated/gpt-oss-20b\",\"object\":\"model\","
   "\"owned_by\":\"simulated\"},"
  "{\"id\":\"simulated/gemma-2b\",\"object\":\"model\","
   "\"owned_by\":\"simulated\"}"
  "]}";

/* ---------- server_get_models ---------- */

char *server_get_models(const char *backend) {
  if (strcmp(backend, "simulated") == 0) {
    return strdup(sim_models);
  }
  char *cmd = NULL;
  if (strcmp(backend, "ollama") == 0) {
    cmd = make_get_cmd("http://localhost:11434/v1/models", NULL);
  } else if (strcmp(backend, "gemini") == 0) {
    cmd = make_get_cmd(
      "https://generativelanguage.googleapis.com/v1beta/openai/models",
      "GEMINI_API_KEY");
  } else if (strcmp(backend, "groq") == 0) {
    cmd = make_get_cmd(
      "https://api.groq.com/openai/v1/models", "GROQ_API_KEY");
  } else if (strcmp(backend, "openrouter") == 0) {
    cmd = make_get_cmd(
      "https://openrouter.ai/api/v1/models", "OPENROUTER_API_KEY");
  } else if (strcmp(backend, "cerebras") == 0) {
    cmd = make_get_cmd(
      "https://api.cerebras.ai/v1/models", "CEREBRAS_API_KEY");
  } else {
    return NULL;
  }
  char *result = read_popen(cmd);
  free(cmd);
  return result;
}

/* ---------- server_chat_completion ---------- */

char *server_chat_completion(const char *backend,
                             const char *model,
                             const char *prompt) {
  char url[256];
  const char *key_env = NULL;
  if (strcmp(backend, "simulated") == 0) {
    return strdup(
      "{\"id\":\"sim-1\",\"object\":\"chat.completion\","
      "\"created\":0,\"model\":\"simulated/gpt-oss-20b\","
      "\"choices\":[{\"index\":0,\"finish_reason\":\"stop\","
      "\"message\":{\"role\":\"assistant\","
      "\"content\":\"Hello! I am a simulated assistant.\"}}],"
      "\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":8,"
      "\"total_tokens\":10}}"
    );
  } else if (strcmp(backend, "ollama") == 0) {
    snprintf(url, sizeof(url),
             "http://localhost:11434/v1/chat/completions");
  } else if (strcmp(backend, "gemini") == 0) {
    snprintf(url, sizeof(url),
      "https://generativelanguage.googleapis.com/v1beta/openai/"
      "chat/completions");
    key_env = "GEMINI_API_KEY";
  } else if (strcmp(backend, "groq") == 0) {
    snprintf(url, sizeof(url),
             "https://api.groq.com/openai/v1/chat/completions");
    key_env = "GROQ_API_KEY";
  } else if (strcmp(backend, "openrouter") == 0) {
    snprintf(url, sizeof(url),
             "https://openrouter.ai/api/v1/chat/completions");
    key_env = "OPENROUTER_API_KEY";
  } else if (strcmp(backend, "cerebras") == 0) {
    snprintf(url, sizeof(url),
             "https://api.cerebras.ai/v1/chat/completions");
    key_env = "CEREBRAS_API_KEY";
  } else {
    return NULL;
  }
  /* Escape prompt for shell: replace " with \" */
  char esc[2048];
  size_t ei = 0;
  for (size_t pi = 0; prompt[pi] && ei + 3 < sizeof(esc); pi++) {
    if (prompt[pi] == '"') {
      esc[ei++] = '\\';
    }
    esc[ei++] = prompt[pi];
  }
  esc[ei] = '\0';
  char body[4096];
  snprintf(body, sizeof(body),
    "{\"model\":\"%s\",\"stream\":false,"
    "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
    model, esc);
  char cmd[8192];
  const char *key = key_env ? getenv(key_env) : NULL;
  if (key && key[0]) {
    snprintf(cmd, sizeof(cmd),
      "curl -s -X POST -H \"Content-Type: application/json\" "
      "-H \"Authorization: Bearer %s\" "
      "-d '%s' \"%s\"",
      key, body, url);
  } else {
    snprintf(cmd, sizeof(cmd),
      "curl -s -X POST -H \"Content-Type: application/json\" "
      "-d '%s' \"%s\"",
      body, url);
  }
  return read_popen(cmd);
}

void server_probe(const char *backend, const char *model) {
  char *res = server_chat_completion(backend, model, "hi");
  free(res);
}

FILE *server_open_stream(const char *backend, const char *model,
                         const char *prompt) {
  if (strcmp(backend, "simulated") == 0) {
    return popen(
      "echo 'data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"Thinking...\"}}]}';"
      "echo 'data: {\"choices\":[{\"delta\":{\"content\":\"Hello! \"}}]}';"
      "echo 'data: {\"choices\":[{\"delta\":{\"content\":\"I am a simulated assistant.\"}}]}';"
      "echo 'data: [DONE]'", "r");
  }
  char url[256];
  const char *key_env = NULL;
  if (strcmp(backend, "ollama") == 0) {
    snprintf(url, sizeof(url), "http://localhost:11434/v1/chat/completions");
  } else if (strcmp(backend, "gemini") == 0) {
    snprintf(url, sizeof(url),
      "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions");
    key_env = "GEMINI_API_KEY";
  } else if (strcmp(backend, "groq") == 0) {
    snprintf(url, sizeof(url),
      "https://api.groq.com/openai/v1/chat/completions");
    key_env = "GROQ_API_KEY";
  } else if (strcmp(backend, "openrouter") == 0) {
    snprintf(url, sizeof(url),
      "https://openrouter.ai/api/v1/chat/completions");
    key_env = "OPENROUTER_API_KEY";
  } else if (strcmp(backend, "cerebras") == 0) {
    snprintf(url, sizeof(url),
      "https://api.cerebras.ai/v1/chat/completions");
    key_env = "CEREBRAS_API_KEY";
  } else {
    return NULL;
  }
  char esc[2048];
  size_t ei = 0;
  for (size_t pi = 0; prompt[pi] && ei + 3 < sizeof(esc); pi++) {
    if (prompt[pi] == '"') {
      esc[ei++] = '\\';
    }
    esc[ei++] = prompt[pi];
  }
  esc[ei] = '\0';
  char body[4096];
  snprintf(body, sizeof(body),
    "{\"model\":\"%s\",\"stream\":true,"
    "\"stream_options\":{\"include_usage\":true},"
    "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
    model, esc);
  char cmd[8192];
  const char *key = key_env ? getenv(key_env) : NULL;
  if (key && key[0]) {
    snprintf(cmd, sizeof(cmd),
      "curl -s -N -X POST -H \"Content-Type: application/json\" "
      "-H \"Authorization: Bearer %s\" "
      "-d '%s' \"%s\"",
      key, body, url);
  } else {
    snprintf(cmd, sizeof(cmd),
      "curl -s -N -X POST -H \"Content-Type: application/json\" "
      "-d '%s' \"%s\"",
      body, url);
  }
  return popen(cmd, "r");
}

/* ---------- simulated streaming (kept for tests) ---------- */

struct sim_state { int step; };

void *sim_chat_request(const char *json) {
  (void)json;
  struct sim_state *s = calloc(1, sizeof(struct sim_state));
  return s;
}

char *sim_chat_reply(void *handle) {
  struct sim_state *s = handle;
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

void sim_chat_free(void *handle) { free(handle); }
