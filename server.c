#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_popen(const char *cmd) {
    char *r = NULL;
    FILE *fp = popen(cmd, "r");
    if (fp) {
        size_t capacity = 4096, bytes = 0;
        char *text = malloc(capacity);
        if (text) {
            int c;
            while ((c = fgetc(fp)) != EOF) {
                if (bytes + 1 >= capacity) {
                    capacity *= 2;
                    char *grown = realloc(text, capacity);
                    if (!grown) {
                        free(text);
                        text = NULL;
                        break;
                    }
                    text = grown;
                }
                text[bytes++] = (char)c;
            }
            if (text) {
                text[bytes] = '\0';
                r = text;
            }
        }
        pclose(fp);
    }
    return r;
}

static int is_url(const char *s) {
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

static char *make_get_cmd(const char *backend, const char *url,
                           const char *key_env) {
    char cmd[2048];
    const char *key = key_env ? getenv(key_env) : NULL;
    char auth[512] = {0};
    if (is_url(backend)) {
        key = "ollama";
    }
    if (key && key[0]) {
        snprintf(auth, sizeof(auth), " -H \"Authorization: Bearer %s\"", key);
    }
    if (strcmp(backend, "github") == 0) {
        snprintf(cmd, sizeof(cmd),
            "curl -s%s -H \"Accept: application/vnd.github+json\" "
            "-H \"X-GitHub-Api-Version: 2022-11-28\" \"%s\"", auth, url);
    } else {
        snprintf(cmd, sizeof(cmd), "curl -s%s \"%s\"", auth, url);
    }
    return strdup(cmd);
}

static const char *sim_models =
    "{\"object\":\"list\",\"data\":["
    "{\"id\":\"simulated/gpt-oss-20b\",\"object\":\"model\","
      "\"owned_by\":\"simulated\"},"
    "{\"id\":\"simulated/gemma-2b\",\"object\":\"model\","
      "\"owned_by\":\"simulated\"}"
    "]}";

char *server_get_models(const char *backend) {
    char *r = NULL;
    if (strcmp(backend, "simulated") == 0) {
        r = strdup(sim_models);
    } else {
        char *cmd = NULL;
        if (is_url(backend)) {
            char url[512];
            snprintf(url, sizeof(url), "%s/v1/models", backend);
            cmd = make_get_cmd(backend, url, NULL);
        } else if (strcmp(backend, "ollama") == 0) {
            cmd = make_get_cmd(backend, "http://localhost:11434/v1/models",
                               NULL);
        } else if (strcmp(backend, "gemini") == 0) {
            cmd = make_get_cmd(backend, "https://generativelanguage."
                               "googleapis.com/v1beta/openai/models",
                               "GEMINI_API_KEY");
        } else if (strcmp(backend, "groq") == 0) {
            cmd = make_get_cmd(backend, "https://api.groq.com/openai/v1/models",
                               "GROQ_API_KEY");
        } else if (strcmp(backend, "openrouter") == 0) {
            cmd = make_get_cmd(backend, "https://openrouter.ai/api/v1/models",
                               "OPENROUTER_API_KEY");
        } else if (strcmp(backend, "cerebras") == 0) {
            cmd = make_get_cmd(backend, "https://api.cerebras.ai/v1/models",
                               "CEREBRAS_API_KEY");
        } else if (strcmp(backend, "github") == 0) {
            cmd = make_get_cmd(backend, "https://models.inference.ai.azure.com/"
                               "models", "GITHUB_API_KEY");
        }
        if (cmd) {
            r = read_popen(cmd);
            free(cmd);
        }
    }
    return r;
}

const char *server_get_url(const char *backend) {
    if (is_url(backend)) { return backend; }
    if (strcmp(backend, "ollama") == 0) { return "http://localhost:11434"; }
    if (strcmp(backend, "gemini") == 0) {
        return "https://generativelanguage.googleapis.com";
    }
    if (strcmp(backend, "groq") == 0) { return "https://api.groq.com"; }
    if (strcmp(backend, "openrouter") == 0) {
        return "https://openrouter.ai";
    }
    if (strcmp(backend, "cerebras") == 0) { return "https://api.cerebras.ai"; }
    if (strcmp(backend, "github") == 0) {
        return "https://models.inference.ai.azure.com";
    }
    return "";
}

char *server_chat_completion(const char *backend, const char *model,
                             const char *prompt) {
    char *r = NULL;
    if (strcmp(backend, "simulated") == 0) {
        r = strdup(
            "{\"id\":\"sim-1\",\"object\":\"chat.completion\","
            "\"created\":0,\"model\":\"simulated/gpt-oss-20b\","
            "\"choices\":[{\"index\":0,\"finish_reason\":\"stop\","
            "\"message\":{\"role\":\"assistant\","
            "\"content\":\"Hello! I am a simulated assistant.\"}}],"
            "\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":8,"
            "\"total_tokens\":10}}"
        );
    } else {
        char url[256] = {0};
        const char *key_env = NULL;
        if (is_url(backend)) {
            snprintf(url, sizeof(url), "%s/v1/chat/completions", backend);
        } else if (strcmp(backend, "ollama") == 0) {
            snprintf(url, sizeof(url),
                     "http://localhost:11434/v1/chat/completions");
        } else if (strcmp(backend, "gemini") == 0) {
            snprintf(url, sizeof(url), "https://generativelanguage."
                     "googleapis.com/v1beta/openai/chat/completions");
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
        } else if (strcmp(backend, "github") == 0) {
            snprintf(url, sizeof(url),
                     "https://models.inference.ai.azure.com/chat/completions");
            key_env = "GITHUB_API_KEY";
        }
        if (url[0]) {
            char esc[2048];
            size_t ei = 0;
            for (size_t pi = 0; prompt[pi] && ei + 3 < sizeof(esc); pi++) {
                if (prompt[pi] == '"') { esc[ei++] = '\\'; }
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
            if (is_url(backend)) { key = "ollama"; }
            char extra[256] = {0};
            if (strcmp(backend, "github") == 0) {
                snprintf(extra, sizeof(extra),
                    " -H \"Accept: application/vnd.github+json\" "
                    "-H \"X-GitHub-Api-Version: 2022-11-28\"");
            }
            if (key && key[0]) {
                snprintf(cmd, sizeof(cmd),
                    "curl -s -X POST -H \"Content-Type: application/json\"%s "
                    "-H \"Authorization: Bearer %s\" "
                    "-d '%s' \"%s\"", extra, key, body, url);
            } else {
                snprintf(cmd, sizeof(cmd),
                    "curl -s -X POST -H \"Content-Type: application/json\"%s "
                    "-d '%s' \"%s\"", extra, body, url);
            }
            r = read_popen(cmd);
        }
    }
    return r;
}

void server_probe(const char *backend, const char *model) {
    char *r = server_chat_completion(backend, model, "hi");
    free(r);
}

FILE *server_open_stream(const char *backend, const char *model,
                         const char *prompt) {
    FILE *r = NULL;
    if (strcmp(backend, "simulated") == 0) {
        r = popen(
            "echo 'data: {\"choices\":[{\"delta\":"
            "{\"reasoning_content\":\"Thinking...\"}}]}';"
            "echo 'data: {\"choices\":[{\"delta\":"
            "{\"content\":\"Hello! I am a simulated assistant.\"}}]}';"
            "echo 'data: [DONE]'", "r");
    } else {
        char url[256] = {0};
        const char *key_env = NULL;
        if (is_url(backend)) {
            snprintf(url, sizeof(url), "%s/v1/chat/completions", backend);
        } else if (strcmp(backend, "ollama") == 0) {
            snprintf(url, sizeof(url),
                     "http://localhost:11434/v1/chat/completions");
        } else if (strcmp(backend, "gemini") == 0) {
            snprintf(url, sizeof(url), "https://generativelanguage."
                     "googleapis.com/v1beta/openai/chat/completions");
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
        } else if (strcmp(backend, "github") == 0) {
            snprintf(url, sizeof(url),
                     "https://models.inference.ai.azure.com/chat/completions");
            key_env = "GITHUB_API_KEY";
        }
        if (url[0]) {
            char esc[2048];
            size_t ei = 0;
            for (size_t pi = 0; prompt[pi] && ei + 3 < sizeof(esc); pi++) {
                if (prompt[pi] == '"') { esc[ei++] = '\\'; }
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
            if (is_url(backend)) { key = "ollama"; }
            char extra[256] = {0};
            if (strcmp(backend, "github") == 0) {
                snprintf(extra, sizeof(extra),
                    " -H \"Accept: application/vnd.github+json\" "
                    "-H \"X-GitHub-Api-Version: 2022-11-28\"");
            }
            if (key && key[0]) {
                snprintf(cmd, sizeof(cmd),
                    "curl -s -N -X POST -H \"Content-Type: application/json\"%s "
                    "-H \"Authorization: Bearer %s\" "
                    "-d '%s' \"%s\"", extra, key, body, url);
            } else {
                snprintf(cmd, sizeof(cmd),
                    "curl -s -N -X POST -H \"Content-Type: application/json\"%s "
                    "-d '%s' \"%s\"", extra, body, url);
            }
            r = popen(cmd, "r");
        }
    }
    return r;
}

struct sim_state { int step; };

void *sim_chat_request(const char *json) {
    (void)json;
    struct sim_state *s = calloc(1, sizeof(struct sim_state));
    return s;
}

char *sim_chat_reply(void *handle) {
    char *r = NULL;
    struct sim_state *s = handle;
    if (s) {
        if (s->step == 0) {
            r = strdup("data: {\"choices\": [{\"delta\": "
                       "{\"reasoning_content\": "
                       "\"First, I need to define...\"}}]}");
        } else if (s->step == 1) {
            r = strdup("data: {\"choices\": [{\"delta\": "
                       "{\"content\": \"Quantum computing is...\"}}]}");
        } else if (s->step == 2) {
            r = strdup("data: [DONE]");
        }
        s->step++;
    }
    return r;
}

void sim_chat_free(void *handle) { free(handle); }
