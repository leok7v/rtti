#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#define codable

struct point { int x; int y; };

struct point foo(struct point param);

codable struct message {
    const char* role;
    const char* content;
    const char* reasoning_content;
};

codable struct reasoning {
    const char* effort;
};

codable struct request {
    const char* model;
    struct message* messages;
    struct reasoning* reasoning;
    const char* reasoning_effort;
    bool stream;
};

codable struct delta {
    const char* content;
    const char* reasoning_content;
};

codable struct choice {
    int index;
    struct delta delta;
    const char* finish_reason;
};

codable struct response_chunk {
    const char* id;
    const char* object;
    long long created;
    const char* model;
    struct choice* choices;
};

#endif
