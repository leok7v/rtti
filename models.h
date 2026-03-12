#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <stddef.h>

#define codable

codable struct model_architecture {
    const char *modality;
    const char *tokenizer;
    const char *instruct_type;
};

codable struct model_pricing {
    const char *prompt;
    const char *completion;
    const char *request;
    const char *image;
    const char *web_search;
    const char *internal_reasoning;
};

codable struct model_top_provider {
    long long context_length;
    long long max_completion_tokens;
    bool is_moderated;
};

codable struct model_default_parameters {
    double temperature;
    double top_p;
    int top_k;
    double frequency_penalty;
    double presence_penalty;
    double repetition_penalty;
};

codable struct model {
    const char *id;
    const char *object;
    const char *owned_by;
    const char *display_name;
    const char *name;
    long long created;
    bool active;
    long long context_window;
    long long max_completion_tokens;
    const char *public_apps;
    const char *canonical_slug;
    const char *hugging_face_id;
    const char *description;
    long long context_length;
    struct model_architecture architecture;
    struct model_pricing pricing;
    struct model_top_provider top_provider;
    const char *per_request_limits;
    struct model_default_parameters default_parameters;
    long long expiration_date;
};

codable struct model_list {
    const char *object;
    struct model *data;
};

#endif
