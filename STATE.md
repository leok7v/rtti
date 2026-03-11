# Project State and Architecture

## Current Architecture

### File Layout (flat — all sources at repo root)
- **`rtti.c`**: Meta-compiler. Parses C headers, emits `*.rtti.h` for every `codable struct`.
- **`codable.c` / `codable.h`**: JSON encode/decode engine using `char kind` (`'s'`, `'i'`, `'d'`, `'b'`, `'['`, `'{'`).
- **`models.h`**: `codable` structs for LLM model-list API responses (`model_list`, `model`, nested sub-structs). No `char**` — all string-array fields omitted; optional numbers are plain primitives (zero when absent).
- **`client.h`**: Structs for chat API: `request`, `message`, `reasoning`, `response_chunk`, `delta`, `choice`.
- **`client.c`**: Main program. Iterates all backends, filters models, fires a single non-streaming turn, prints the reply.
- **`server.h` / `server.c`**: Backend abstraction using `popen("curl -s ...")`. Exposes:
  - `server_get_models(backend)` → malloc'd JSON string of model list.
  - `server_chat_completion(backend, model, prompt)` → malloc'd raw JSON response.
  - `sim_chat_request/reply/free` — retained simulated streaming for tests.
- **`Build/`**: Generated binaries (`rtti`, `client`) and generated headers (`client.rtti.h`, `models.rtti.h`).

### Supported Backends
| Backend | Models endpoint | Model filter |
|---|---|---|
| `simulated` | hardcoded | `simulated/gpt-oss-*` |
| `ollama` | `http://localhost:11434/v1/models` | `gpt-oss:*` |
| `gemini` | Googleapis OpenAI-compat | smallest `gemma` |
| `groq` | `api.groq.com` | `gpt-oss*` |
| `openrouter` | `openrouter.ai` | `gpt-oss*` (incl. `:free`) |
| `cerebras` | `api.cerebras.ai` | `gpt-oss*` |

API keys are read from env: `GEMINI_API_KEY`, `GROQ_API_KEY`, `OPENROUTER_API_KEY`, `CEREBRAS_API_KEY`.

### Memory Management
- `decode()` allocates the parsed copy of JSON and any dynamic inner arrays. Caller must `free(list.data)` then `free(parsed)`.
- `server_get_models()` and `server_chat_completion()` return malloc'd strings — caller frees.

### Known Limitations
- `char**` (array of strings) not supported by `codable` — omit those fields from `codable` structs.
- `codable` numbers must be plain primitives, not pointers; missing JSON fields default to `0`.

### Build
```bash
make clean && make && Build/client
```

### macOS / IDE Sandbox Note
Do **not** add `Build/` or `*.rtti.h` to `.gitignore`. Doing so causes macOS to sandbox the IDE's background shell and block write access to those paths.

