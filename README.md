# pypilot-client-protocol

Header-only C++ implementation of the **pypilot TCP client value protocol** for Linux and Arduino-style targets.

This is **not** NMEA 0183 and it is **not** the pypilot binary servo UART protocol. It implements the pypilot client/server value protocol used on TCP port `23322`:

```text
name=json-value\n
watch={"name":period}\n
values={...}
```

## Features

- newline-delimited `name=json` frame parser
- ArduinoJson-based JSON parsing and serialization
- JSON values handled through a small `JsonValue` wrapper around an owned ArduinoJson document
- helpers for `set`, `watch`, `unwatch`, `watch={"values":true}`, and `udp_port`
- incoming update polling model
- value metadata parsing as ArduinoJson objects
- Linux POSIX TCP transport
- Arduino `Client` transport wrapper
- Linux examples and unit tests

## pypilot protocol summary

The pypilot server listens on TCP port `23322` by default. Each record is one newline-terminated assignment:

```text
ap.enabled=true
ap.mode="compass"
ap.heading_command=90.0
watch={"ap.heading":0.5,"ap.enabled":true}
```

Only the right-hand side is JSON. The whole line is not a JSON object.

## Dependency

Arduino builds declare this library dependency:

```text
ArduinoJson
```

CMake builds fetch ArduinoJson by default through `FetchContent`. To use a locally installed/include-path-provided copy instead, configure with:

```bash
cmake -S . -B build -DPYPILOT_CLIENT_PROTOCOL_FETCH_ARDUINOJSON=OFF
```

## Build on Linux

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Examples

```bash
./build/pypilot-list-values 127.0.0.1 23322
./build/pypilot-monitor 127.0.0.1 23322 ap.heading 0.5
./build/pypilot-set 127.0.0.1 23322 ap.enabled true
```

## Optional UDP output

The helper `make_udp_port(port)` sends `udp_port=<port>`. The TCP connection remains the control channel.

## Arduino

Include:

```cpp
#include <pypilot_client_protocol.hpp>
```

The Arduino protocol wrapper accepts any object derived from Arduino `Client`:

```cpp
Client& stream = client;
pypilot_client_protocol::ClientProtocol protocol(stream);
protocol.watch_periodic("ap.heading", 0.5);
```

Incoming records parse their JSON value into an owned ArduinoJson-backed `JsonValue`:

```cpp
pypilot_client_protocol::Record record;
if (protocol.poll(record)) {
  const char* name = record.name;
  double value = record.value.as_number();
}
```

## License

GPL-3.0-or-later.
