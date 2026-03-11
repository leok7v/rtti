#ifndef LLM_H
#define LLM_H

void *chat_completion_request(const char *json);
char *chat_completion_reply(void *handle);
void chat_completion_free(void *handle);

const char *llm_get_models(const char *backend);

#endif
