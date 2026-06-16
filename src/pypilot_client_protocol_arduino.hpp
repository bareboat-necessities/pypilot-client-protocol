#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace pypilot_client_protocol {

static const int DEFAULT_PORT = 23322;
static const int DEFAULT_UDP_CONTROL_PORT = 43822;

struct Record {
    char name[64];
    char value[192];

    void clear() {
        name[0] = '\0';
        value[0] = '\0';
    }
};

inline void safe_copy(char* dst, size_t dst_size, const char* src, size_t src_len) {
    if (!dst || dst_size == 0) return;
    size_t n = src_len < dst_size - 1 ? src_len : dst_size - 1;
    if (src && n) memcpy(dst, src, n);
    dst[n] = '\0';
}

inline void trim_number(char* s) {
    if (!s) return;
    while (*s == ' ') ++s;
    char* end = s + strlen(s);
    while (end > s && end[-1] == '0') --end;
    if (end > s && end[-1] == '.') --end;
    if (end == s || (end == s + 1 && s[0] == '-')) *end++ = '0';
    *end = '\0';
}

inline void format_number(double value, char* out, size_t out_size, unsigned decimals = 6) {
    if (!out || out_size == 0) return;
    dtostrf(value, 0, decimals, out);
    char* first = out;
    while (*first == ' ') ++first;
    if (first != out) memmove(out, first, strlen(first) + 1);
    trim_number(out);
}

class LineParser {
public:
    explicit LineParser(size_t max_line = sizeof(buffer_)) : max_line_(max_line), pos_(0) {
        buffer_[0] = '\0';
    }

    void clear() {
        pos_ = 0;
        buffer_[0] = '\0';
    }

    bool push(char c, Record& out, const char** error = nullptr) {
        if (c == '\r') return false;
        if (c == '\n') {
            buffer_[pos_] = '\0';
            bool ok = parse_line(buffer_, out, error);
            clear();
            return ok;
        }
        if (pos_ + 1 >= sizeof(buffer_) || pos_ + 1 >= max_line_) {
            clear();
            if (error) *error = "line too long";
            return false;
        }
        buffer_[pos_++] = c;
        buffer_[pos_] = '\0';
        return false;
    }

    static bool parse_line(const char* line, Record& out, const char** error = nullptr) {
        out.clear();
        if (!line || !line[0]) {
            if (error) *error = "empty line";
            return false;
        }
        const char* eq = strchr(line, '=');
        if (!eq || eq == line) {
            if (error) *error = "record has no name or '='";
            return false;
        }
        safe_copy(out.name, sizeof(out.name), line, static_cast<size_t>(eq - line));
        safe_copy(out.value, sizeof(out.value), eq + 1, strlen(eq + 1));
        return true;
    }

private:
    char buffer_[256];
    size_t max_line_;
    size_t pos_;
};

inline size_t append_raw(char* out, size_t cap, size_t pos, const char* s) {
    if (!out || cap == 0 || !s) return pos;
    while (*s && pos + 1 < cap) out[pos++] = *s++;
    out[pos] = '\0';
    return pos;
}

inline size_t append_quoted(char* out, size_t cap, size_t pos, const char* s) {
    if (!out || cap == 0) return pos;
    if (pos + 1 < cap) out[pos++] = '"';
    while (s && *s && pos + 2 < cap) {
        char c = *s++;
        if (c == '"' || c == '\\') {
            out[pos++] = '\\';
            out[pos++] = c;
        } else if (c == '\n') {
            out[pos++] = '\\'; out[pos++] = 'n';
        } else if (c == '\r') {
            out[pos++] = '\\'; out[pos++] = 'r';
        } else if (c == '\t') {
            out[pos++] = '\\'; out[pos++] = 't';
        } else {
            out[pos++] = c;
        }
    }
    if (pos + 1 < cap) out[pos++] = '"';
    out[pos] = '\0';
    return pos;
}

inline size_t make_record_raw(char* out, size_t cap, const char* name, const char* json_value) {
    if (!out || cap == 0) return 0;
    size_t pos = 0;
    pos = append_raw(out, cap, pos, name);
    pos = append_raw(out, cap, pos, "=");
    pos = append_raw(out, cap, pos, json_value);
    pos = append_raw(out, cap, pos, "\n");
    return pos;
}

inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) {
    return make_record_raw(out, cap, name, value ? "true" : "false");
}

inline size_t make_set_number(char* out, size_t cap, const char* name, double value, unsigned decimals = 6) {
    char num[32];
    format_number(value, num, sizeof(num), decimals);
    return make_record_raw(out, cap, name, num);
}

inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) {
    char json[128];
    json[0] = '\0';
    append_quoted(json, sizeof(json), 0, value);
    return make_record_raw(out, cap, name, json);
}

inline size_t make_watch_raw(char* out, size_t cap, const char* name, const char* period_json) {
    if (!out || cap == 0) return 0;
    size_t pos = 0;
    pos = append_raw(out, cap, pos, "watch={\"");
    pos = append_raw(out, cap, pos, name);
    pos = append_raw(out, cap, pos, "\":");
    pos = append_raw(out, cap, pos, period_json);
    pos = append_raw(out, cap, pos, "}\n");
    return pos;
}

inline size_t make_watch_continuous(char* out, size_t cap, const char* name) {
    return make_watch_raw(out, cap, name, "true");
}

inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds, unsigned decimals = 6) {
    char num[32];
    format_number(seconds, num, sizeof(num), decimals);
    return make_watch_raw(out, cap, name, num);
}

inline size_t make_unwatch(char* out, size_t cap, const char* name) {
    return make_watch_raw(out, cap, name, "false");
}

inline size_t make_watch_values(char* out, size_t cap) {
    return make_watch_raw(out, cap, "values", "true");
}

inline size_t make_udp_port(char* out, size_t cap, int port) {
    char num[16];
    ltoa(port, num, 10);
    return make_record_raw(out, cap, "udp_port", num);
}

inline size_t make_keepalive(char* out, size_t cap) {
    if (!out || cap == 0) return 0;
    out[0] = '\n';
    if (cap > 1) out[1] = '\0';
    return 1;
}

} // namespace pypilot_client_protocol
