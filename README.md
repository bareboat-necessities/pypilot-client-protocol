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
- JSON value parser/serializer covering null, booleans, numbers, strings, arrays, and objects
- helpers for `set`, `watch`, `unwatch`, `watch={"values":true}`, and `udp_port`
- incoming update callback model
- value metadata parsing as generic JSON objects
- Linux POSIX TCP transport
- optional Linux UDP receiver for `udp_port` watched output
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

The helper `make_udp_port(port)` sends `udp_port=<port>`. On Linux, `LinuxUdpReceiver` can bind that port and parse incoming newline-delimited update records from UDP datagrams. The TCP connection remains the control channel.

## Arduino

Include:

```cpp
#include <pypilot_client_protocol.hpp>
```

The Arduino wrapper accepts any object compatible with Arduino `Client`/`Stream` methods: `connect`, `connected`, `available`, `read`, `write`, and `stop`.

## License

GPL-3.0-or-later.
