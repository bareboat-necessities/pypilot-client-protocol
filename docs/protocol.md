# pypilot TCP client protocol

The pypilot client protocol is a newline-delimited value protocol, normally on TCP port 23322.

Every record is:

```text
name=json-value\n
```

Examples:

```text
ap.enabled=true
ap.mode="compass"
ap.heading_command=90.0
watch={"ap.heading":0.5,"ap.enabled":true}
watch={"values":true}
udp_port=43822
```

Only the right-hand side is JSON. The whole record is not a JSON object.

Watch values:

- `true`: continuous/change-driven watch
- `false`: unwatch
- number: periodic update interval in seconds

The server returns the same assignment record format for updates and errors:

```text
ap.heading=123.4000
error="..." or error=...
values={...}
```
