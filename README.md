# pypilot-client-protocol

Header-only C++ helpers for pypilot client value records:

```text
name=json-value\n
watch={"name":period}\n
values={...}
```

This repository is only protocol framing and JSON handling. Byte transport, connection lifetime, reconnect policy, polling, and event-loop integration belong in other modules.

## Scope

This library provides:

- newline-delimited `name=json` record parsing
- right-hand-side JSON parsing with ArduinoJson
- set/watch record construction with ArduinoJson
- incremental line decoding from caller-supplied bytes

It does not own connections or streams.

## Dependency

Arduino builds declare:

```text
ArduinoJson
```

CMake builds fetch ArduinoJson by default through `FetchContent`. To provide ArduinoJson yourself:

```bash
cmake -S . -B build -DPYPILOT_CLIENT_PROTOCOL_FETCH_ARDUINOJSON=OFF
```

## Build

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Usage

```cpp
#include <pypilot_client_protocol.hpp>

char line[128];
pypilot_client_protocol::make_watch_periodic(line, sizeof(line), "ap.heading", 0.5);

char name[64];
JsonDocument value;
pypilot_client_protocol::parse_record("ap.enabled=true\n", name, sizeof(name), value);
```

## License

GPL-3.0-or-later.
