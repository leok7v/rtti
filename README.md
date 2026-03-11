# rtti — C meta-compiler for JSON-codable structs

Zero-dependency C meta-compiler that generates reflection schemas (`*.rtti.h`) for `codable` structs, enabling native JSON encoding and decoding in strict ANSI C.

## Quick Start

```bash
make clean && make && Build/client
```

## Architecture

All sources live at the repo root.

| File | Purpose |
|---|---|
| `rtti.c` | Meta-compiler: parses headers, emits `*.rtti.h` |
| `codable.c` / `codable.h` | JSON encode / decode engine |
| `models.h` | `codable` structs for LLM model-list responses |
| `client.h` | `codable` structs for chat API (request, response_chunk, …) |
| `client.c` | Main program: fetches models, filters, runs chat |
| `server.h` / `server.c` | Backend abstraction via `popen("curl …")` |
| `Build/` | Generated binaries and `*.rtti.h` headers |

## codable API

Tag any struct with `codable` and run `Build/rtti header.h > Build/header.rtti.h`:

```c
#define codable

codable struct message {
  const char *role;
  const char *content;
};
```

Then encode/decode:

```c
#include "header.rtti.h"
#include "codable.h"

char *json = encode(&msg, (void *)message_rtti);
void *mem  = decode(&msg, (void *)message_rtti, json_str);
free(mem);
```

## Supported LLM backends

`client.c` loops over all backends, picks the right model, and sends one non-streaming chat turn:

| Backend | Model filter |
|---|---|
| simulated | `simulated/gpt-oss-*` |
| ollama (`localhost:11434`) | `gpt-oss:*` |
| gemini | smallest `gemma` model |
| groq | `gpt-oss*` |
| openrouter | `gpt-oss*` (incl. `:free`) |
| cerebras | `gpt-oss*` |

Set env vars: `GEMINI_API_KEY`, `GROQ_API_KEY`, `OPENROUTER_API_KEY`, `CEREBRAS_API_KEY`.

## Limitations

- `char**` (array of strings) is not supported by `codable` — omit those fields.
- Optional numeric fields must be plain primitives (not pointers); absent JSON values default to `0`.
- Do **not** add `Build/` or `*.rtti.h` to `.gitignore` — macOS sandboxes the IDE shell and blocks writes to those paths.
