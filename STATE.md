# Project State and Architecture

## File Layout (flat — all sources at repo root)
- **`rtti.c`**: Meta-compiler. Parses C headers, emits `*.rtti.h` for every `codable struct`.
- **`codable.c` / `codable.h`**: JSON encode/decode using `char kind` (`'s'`, `'i'`, `'d'`, `'b'`, `'['`, `'{'`). API: `encode(ptr, pointer_to_rtti)` / `decode(ptr, pointer_to_rtti, json)`. `struct type_info` carries rtti metadata.
- **`models.h`**: `codable` structs for model-list responses (`model_list`, `model`).
- **`client.h`**: Chat API structs. Two families:
  - **Streaming**: `delta`, `choice`, `response_chunk` (field: `delta.content`, `delta.reasoning`, `delta.reasoning_content`). `response_chunk` includes `completion_usage` for backends that emit usage in the final SSE chunk.
  - **Non-streaming**: `completion_message`, `completion_choice`, `completion` — superset of all fields across all backends (`system_fingerprint`, `provider`, `service_tier`, `x_groq`, `time_info`, nested `completion_usage` with `prompt_time`, `completion_time`, token details).
- **`client.c`**: Main program. For each backend: fetches models, warm-up probe, collects up to 16 candidate models, tries them in order until content is received. Prints reply, reason (truncated), token counts, ttft, pp, tg.
- **`server.h` / `server.c`**: Backend abstraction via `popen("curl ...")`. Exposes:
  - `server_get_models(backend)` → malloc'd JSON string.
  - `server_chat_completion(backend, model, prompt)` → malloc'd JSON (non-streaming).
  - `server_probe(backend, model)` → silent warmup (fires non-streaming request, discards output).
  - `server_open_stream(backend, model, prompt)` → `FILE*` for SSE line-by-line streaming (`curl -s -N`, `"stream":true`, `"stream_options":{"include_usage":true}`).
- **`Build/`**: Binaries and generated `*.rtti.h` headers.

## Supported Backends
| Backend | Model filter |
|---|---|
| `simulated` | `simulated/gpt-oss-*` |
| `ollama` | `gpt-oss:*` |
| `gemini` | smallest `gemma` |
| `groq` | `gpt-oss*` |
| `openrouter` | `gpt-oss*` excluding `safeguard`; `:free` preferred, falls back |
| `cerebras` | `gpt-oss*` |

API keys from env: `GEMINI_API_KEY`, `GROQ_API_KEY`, `OPENROUTER_API_KEY`, `CEREBRAS_API_KEY`.

## client.c Output Format
```
=== backend ===
  Warmup...
  Model: <id>
  reply:   <streamed content>        (suppressed if empty)
  reason:  <first 70 chars>...       (suppressed if empty)
  tokens:  reasoning=N content=N
  ttft:    0.52s (time to first token)  pp: ~N  tg: N t/s
```
- **ttft**: wall-clock time from curl call to first content token (accurate after warmup).
- **pp** (prompt processing t/s): exact when backend reports `prompt_time` (Groq); `~` estimated from `prompt_tokens / ttft` if usage available; `~` guestimated from `strlen(prompt)/3.5 / ttft` otherwise.
- **tg** (token generation t/s): `content_tokens / (elapsed - ttft)`.
- Thousands separator via `setlocale(LC_NUMERIC, "en_US.UTF-8")` + `%'d`.

## struct str (client.c local)
```c
struct str { char *data; int32_t count; int32_t capacity; };
```
Used to accumulate streaming content and reasoning via `str_push()`.

## Memory Management
- `decode()` allocates a copy of JSON and dynamic inner arrays. Caller: `free(inner_array)` then `free(parsed_copy)`.
- `server_*` functions return malloc'd strings — caller frees.

## Known Limitations
- `char**` not supported by `codable` — omit from `codable` structs.
- Missing JSON fields decode to `0` / `NULL`.

## Build
```bash
make clean && make && Build/client
```

## macOS / IDE Note
Do **not** add `Build/` or `*.rtti.h` to `.gitignore` — macOS sandboxes the IDE shell and blocks writes to those paths.
