#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#define codable

codable struct message {
  const char *role;
  const char *content;
  const char *reasoning_content;
};

codable struct reasoning {
  const char *effort;
};

codable struct request {
  const char *model;
  struct message *messages;
  struct reasoning *reasoning;
  const char *reasoning_effort;
  bool stream;
};

codable struct delta {
  const char *content;
  const char *reasoning_content;
  const char *reasoning;
};

codable struct choice {
  int index;
  struct delta delta;
  const char *finish_reason;
};

codable struct completion_tokens_details {
  long long reasoning_tokens;
  long long accepted_prediction_tokens;
  long long rejected_prediction_tokens;
  long long image_tokens;
  long long audio_tokens;
};

codable struct prompt_tokens_details {
  long long cached_tokens;
  long long cache_write_tokens;
  long long audio_tokens;
};

codable struct completion_usage {
  long long prompt_tokens;
  long long completion_tokens;
  long long total_tokens;
  double cost;
  double queue_time;
  double prompt_time;
  double completion_time;
  double total_time;
  struct completion_tokens_details completion_tokens_details;
  struct prompt_tokens_details prompt_tokens_details;
};

codable struct response_chunk {
  const char *id;
  const char *object;
  long long created;
  const char *model;
  struct choice *choices;
  struct completion_usage usage;
};

codable struct time_info {
  double queue_time;
  double prompt_time;
  double completion_time;
  double total_time;
};

codable struct x_groq {
  const char *id;
};

codable struct completion_message {
  const char *role;
  const char *content;
  const char *reasoning_content;
  const char *reasoning;
  const char *refusal;
};

codable struct completion_choice {
  int index;
  struct completion_message message;
  const char *finish_reason;
  const char *native_finish_reason;
};

codable struct completion {
  const char *id;
  const char *object;
  long long created;
  const char *model;
  const char *system_fingerprint;
  const char *provider;
  const char *service_tier;
  struct completion_choice *choices;
  struct completion_usage usage;
  struct time_info time_info;
  struct x_groq x_groq;
};

#endif
