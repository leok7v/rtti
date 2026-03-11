# rtti meta compiler for encodable/decodable

This repository provides a fast, zero-dependency C meta-compiler `rtti` that generates reflection schemas (`type_info`) for `codable` structs, natively solving JSON encoding and decoding in strict ANSI C (MacOS Sandbox Compliant).

## Architecture

- **`src/rtti.c`**: The meta compiler that parses C headers tracking structs tagged with `#define codable`.
- **`src/codable.c` & `src/codable.h`**: The JSON serialization engine utilizing mapping outputs!
  - `encode(void* pointer_to_struct, void* pointer_to_meta)`
  - `decode(void* pointer_to_struct, void* pointer_to_meta, char* json)`
- **`client.h`**: Defines the data models for the LLM generation requests alongside streaming schemas (`message`, `request`, `reasoning`, `response_chunk`). 
- **`client.c`**: An example user agent initializing a simulated context stream pointing towards abstract completions interfaces.
- **`llm.h` & `llm.c`**: A localized mock LLM server engine intercepting the encoding payloads yielding chunked `delta` simulated AI replies.

## Example JSON structure mapping:
```json
{
  "model": "reasoning-model-v1",
  "messages": [
    {"role": "user", "content": "Explain quantum computing."}
  ],
  "reasoning": {
      "effort": "medium"
   },
  "stream": true
}
```

## Build and execution

Outputs orchestrate carefully into `Build/` separating dynamic binaries from the `src/` hierarchy logic natively. 
```bash
make
make test
```

## Advanced Formatting Logic

- **JSON Null Parsing**: Primitive strings correctly identify underlying `null` payloads mapping pointer overrides natively back to the referenced struct headers ensuring non-segmenting safety values.
- **Continuous Memory**: Non-primitive arrays infer termination sequentially across standard byte zero'd stacks avoiding explicit array length variables safely (E.G `struct reasoning res[2] = {{0}}`).
