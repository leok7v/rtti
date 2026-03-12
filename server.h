#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>

/* Fetch model list JSON for a backend.
      Returns malloc'd string (caller frees), or NULL on error. */
char *server_get_models(const char *backend);

/* Non-streaming single-turn completion.
   Returns malloc'd JSON (caller frees). */
char *server_chat_completion(const char *backend, const char *model,
                             const char *prompt);

/* Silent warmup: fires a minimal request and discards output so the model
      is loaded into RAM before actual timing begins. */
void server_probe(const char *backend, const char *model);

/* Streaming completion. Returns popen FILE* for SSE line-by-line reading.
      Caller must pclose() when done. */
FILE *server_open_stream(const char *backend, const char *model,
                         const char *prompt);

const char *server_get_url(const char *backend);

void *sim_chat_request(const char *json);
char *sim_chat_reply(void *handle);
void  sim_chat_free(void *handle);

#endif
