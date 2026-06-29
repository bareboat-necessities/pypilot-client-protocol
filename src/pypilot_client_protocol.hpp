#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ArduinoJson.h>

#if ARDUINOJSON_VERSION_MAJOR < 7
#error "pypilot-client-protocol requires ArduinoJson 7 or newer"
#endif

namespace pypilot_client_protocol {

#ifndef PYPILOT_CLIENT_PROTOCOL_MAX_NAME
#define PYPILOT_CLIENT_PROTOCOL_MAX_NAME 64
#endif

#ifndef PYPILOT_CLIENT_PROTOCOL_MAX_LINE
#define PYPILOT_CLIENT_PROTOCOL_MAX_LINE 512
#endif

inline void copy_text(char* out, size_t cap, const char* text, size_t len) {
    if (!out || cap == 0) return;
    size_t n = len < cap - 1 ? len : cap - 1;
    if (text && n) memcpy(out, text, n);
    out[n] = '\0';
}

inline size_t append_text(char* out, size_t cap, size_t pos, const char* text) {
    if (!out || cap == 0 || !text) return pos;
    while (*text && pos + 1 < cap) out[pos++] = *text++;
    out[pos] = '\0';
    return pos;
}

inline bool parse_json_value(const char* text, size_t len, JsonDocument& out, const char** error = 0) {
    DeserializationError err = deserializeJson(out, text ? text : "", len);
    if (err) {
        if (error) *error = err.c_str();
        return false;
    }
    return true;
}

inline bool parse_record(const char* line, size_t len, char* name_out, size_t name_cap, JsonDocument& value_out, const char** error = 0) {
    if (!line || len == 0) {
        if (error) *error = "empty record";
        return false;
    }
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) --len;

    const char* eq = 0;
    for (size_t i = 0; i < len; ++i) {
        if (line[i] == '=') {
            eq = line + i;
            break;
        }
    }
    if (!eq || eq == line) {
        if (error) *error = "record must be name=json";
        return false;
    }

    copy_text(name_out, name_cap, line, static_cast<size_t>(eq - line));
    const char* rhs = eq + 1;
    size_t rhs_len = len - static_cast<size_t>(rhs - line);
    return parse_json_value(rhs, rhs_len, value_out, error);
}

inline bool parse_record(const char* line, char* name_out, size_t name_cap, JsonDocument& value_out, const char** error = 0) {
    return parse_record(line, line ? strlen(line) : 0, name_out, name_cap, value_out, error);
}

inline size_t make_record(char* out, size_t cap, const char* name, JsonVariantConst value) {
    if (!out || cap == 0) return 0;
    size_t pos = 0;
    pos = append_text(out, cap, pos, name ? name : "");
    pos = append_text(out, cap, pos, "=");
    if (pos + 1 < cap) {
        size_t n = serializeJson(value, out + pos, cap - pos);
        pos += n;
        if (pos >= cap) pos = cap - 1;
        out[pos] = '\0';
    }
    pos = append_text(out, cap, pos, "\n");
    return pos;
}

inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) {
    JsonDocument doc;
    doc.set(value);
    return make_record(out, cap, name, doc.as<JsonVariantConst>());
}

inline size_t make_set_number(char* out, size_t cap, const char* name, double value) {
    JsonDocument doc;
    doc.set(value);
    return make_record(out, cap, name, doc.as<JsonVariantConst>());
}

inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) {
    JsonDocument doc;
    doc.set(value ? value : "");
    return make_record(out, cap, name, doc.as<JsonVariantConst>());
}

inline size_t make_watch(char* out, size_t cap, const char* name, JsonVariantConst period) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[name ? name : ""].set(period);
    return make_record(out, cap, "watch", doc.as<JsonVariantConst>());
}

inline size_t make_watch_continuous(char* out, size_t cap, const char* name) {
    JsonDocument period;
    period.set(true);
    return make_watch(out, cap, name, period.as<JsonVariantConst>());
}

inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds) {
    JsonDocument period;
    period.set(seconds);
    return make_watch(out, cap, name, period.as<JsonVariantConst>());
}

inline size_t make_unwatch(char* out, size_t cap, const char* name) {
    JsonDocument period;
    period.set(false);
    return make_watch(out, cap, name, period.as<JsonVariantConst>());
}

inline size_t make_watch_values(char* out, size_t cap) {
    return make_watch_continuous(out, cap, "values");
}

class LineDecoder {
public:
    LineDecoder() : pos_(0) { buffer_[0] = '\0'; }
    void clear() { pos_ = 0; buffer_[0] = '\0'; }

    bool push(char c, char* name, size_t name_cap, JsonDocument& value, const char** error = 0) {
        if (c == '\r') return false;
        if (c == '\n') {
            bool ok = parse_record(buffer_, pos_, name, name_cap, value, error);
            clear();
            return ok;
        }
        if (pos_ + 1 >= sizeof(buffer_)) {
            clear();
            if (error) *error = "line too long";
            return false;
        }
        buffer_[pos_++] = c;
        buffer_[pos_] = '\0';
        return false;
    }

private:
    char buffer_[PYPILOT_CLIENT_PROTOCOL_MAX_LINE];
    size_t pos_;
};

} // namespace pypilot_client_protocol
