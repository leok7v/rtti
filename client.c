#include "client.h"
#include "client.rtti.h"
#include "codable.h"
#include "llm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct point foo(struct point param) {
  (void)param;
  return (struct point){.x = 1, .y = 2};
}

static void dump_response(struct response_chunk *res) {
  if (res->choices) {
    if (res->choices[0].delta.reasoning_content) {
      printf("[Reasoning]: %s\n", res->choices[0].delta.reasoning_content);
    }
    if (res->choices[0].delta.content) {
      printf("[Content]  : %s\n", res->choices[0].delta.content);
    }
  }
}

int main(void) {
  struct request req = {0};
  req.model = "reasoning-model-v1";
  req.stream = true;
  struct message msgs[2] = {{0}};
  msgs[0].role = "user";
  msgs[0].content = "Explain quantum computing.";
  req.messages = msgs;
  struct reasoning res[2] = {{0}};
  res[0].effort = "medium";
  req.reasoning = res;
  char *json = encode(&req, (void *)request_rtti);
  if (json) {
    printf("Issuing request:\n%s\n\n", json);
    void *handle = chat_completion_request(json);
    if (handle) {
      printf("Responses:\n");
      while (1) {
        char *reply = chat_completion_reply(handle);
        if (!reply) {
          break;
        }
        if (strcmp(reply, "data: [DONE]") == 0) {
          free(reply);
          break;
        }
        if (strncmp(reply, "data: ", 6) == 0) {
          char *json_str = reply + 6;
          struct response_chunk chunk = {0};
          void *parsed = decode(&chunk, (void *)response_chunk_rtti, json_str);
          if (parsed) {
            dump_response(&chunk);
            if (chunk.choices) {
              free(chunk.choices);
            }
            free(parsed);
          } else {
            printf("Failed to decode: %s\n", reply);
          }
        } else {
          printf("%s\n", reply);
        }
        free(reply);
      }
      chat_completion_free(handle);
    } else {
      printf("Failed to create chat completion request.\n");
    }
    free(json);
  } else {
    printf("Encode failed.\n");
  }
  return 0;
}
