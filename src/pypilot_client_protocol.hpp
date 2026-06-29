#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ArduinoJson.h>

#if defined(ARDUINO)
#include <Arduino.h>
#include <Client.h>
#else
#include <string>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <cstdio>
#include <map>
#include <memory>
#endif

namespace pypilot_client_protocol {

static const int DEFAULT_PORT = 23322;
static const int DEFAULT_UDP_CONTROL_PORT = 43822;

#ifndef PYPILOT_CLIENT_PROTOCOL_JSON_CAPACITY
#if defined(ARDUINO)
#define PYPILOT_CLIENT_PROTOCOL_JSON_CAPACITY 2048
#else
#define PYPILOT_CLIENT_PROTOCOL_JSON_CAPACITY 8192
#endif
#endif

#if ARDUINOJSON_VERSION_MAJOR >= 7
typedef JsonDocument ProtocolJsonDocument;
#define PYPILOT_CLIENT_PROTOCOL_DOC_INIT doc_()
#else
typedef DynamicJsonDocument ProtocolJsonDocument;
#define PYPILOT_CLIENT_PROTOCOL_DOC_INIT doc_(PYPILOT_CLIENT_PROTOCOL_JSON_CAPACITY)
#endif

class JsonValue {
public:
    JsonValue() : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.clear(); }
    JsonValue(bool value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value); }
    JsonValue(int value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value); }
    JsonValue(long value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value); }
    JsonValue(double value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value); }
    JsonValue(const char* value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value ? value : ""); }

#if !defined(ARDUINO)
    JsonValue(const std::string& value) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT { doc_.set(value); }
#endif

    JsonValue(const JsonValue& other) : PYPILOT_CLIENT_PROTOCOL_DOC_INIT {
        doc_.set(other.variant());
    }

    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            doc_.clear();
            doc_.set(other.variant());
            clear_cache();
        }
        return *this;
    }

    static JsonValue object() {
        JsonValue value;
        value.doc_.to<JsonObject>();
        return value;
    }

    static JsonValue array() {
        JsonValue value;
        value.doc_.to<JsonArray>();
        return value;
    }

    bool parse(const char* text, size_t len, const char** error = 0) {
        doc_.clear();
        clear_cache();
        DeserializationError err = deserializeJson(doc_, text ? text : "", len);
        if (err) {
            if (error) *error = err.c_str();
            doc_.clear();
            return false;
        }
        return true;
    }

    bool parse(const char* text, const char** error = 0) {
        return parse(text, text ? strlen(text) : 0, error);
    }

    bool is_null() const { return variant().isNull(); }
    bool is_bool() const { return variant().is<bool>(); }
    bool is_number() const { return variant().is<double>() || variant().is<long>() || variant().is<int>(); }
    bool is_string() const { return variant().is<const char*>(); }
    bool is_object() const { return variant().is<JsonObjectConst>(); }
    bool is_array() const { return variant().is<JsonArrayConst>(); }

    bool as_bool(bool fallback = false) const { return is_bool() ? variant().as<bool>() : fallback; }
    double as_number(double fallback = 0.0) const { return is_number() ? variant().as<double>() : fallback; }
    const char* as_c_str(const char* fallback = "") const {
        const char* value = variant().as<const char*>();
        return value ? value : fallback;
    }

    JsonObject as_object() {
        ensure_object();
        return doc_.as<JsonObject>();
    }

    JsonObjectConst as_object() const {
        return doc_.as<JsonObjectConst>();
    }

    JsonArray as_array() {
        ensure_array();
        return doc_.as<JsonArray>();
    }

    JsonArrayConst as_array() const {
        return doc_.as<JsonArrayConst>();
    }

    bool set_member(const char* key, const JsonValue& value) {
        if (!key || !key[0]) return false;
        ensure_object();
        doc_[key].set(value.variant());
        clear_cache();
        return true;
    }

    JsonVariant variant() { return doc_.as<JsonVariant>(); }
    JsonVariantConst variant() const { return doc_.as<JsonVariantConst>(); }

    size_t dump(char* out, size_t cap) const {
        if (!out || cap == 0) return 0;
        size_t n = serializeJson(variant(), out, cap);
        if (n >= cap) out[cap - 1] = '\0';
        else out[n] = '\0';
        return n;
    }

#if !defined(ARDUINO)
    const std::string& as_string() const {
        string_cache_ = as_c_str("");
        return string_cache_;
    }

    std::string dump() const {
        std::string out;
        serializeJson(variant(), out);
        return out;
    }

    static std::string quote(const std::string& text) {
        JsonValue value(text);
        return value.dump();
    }

    JsonValue& set(const std::string& value) {
        doc_.clear();
        doc_.set(value);
        clear_cache();
        return *this;
    }

    class MemberProxy {
    public:
        MemberProxy(JsonValue& parent, const std::string& key) : parent_(parent), key_(key) {}
        MemberProxy& operator=(const JsonValue& value) {
            parent_.set_member(key_.c_str(), value);
            return *this;
        }
        operator JsonValue() const {
            JsonValue out;
            JsonObjectConst obj = parent_.as_object();
            if (!obj.isNull() && obj.containsKey(key_.c_str())) {
                out.doc_.set(obj[key_.c_str()]);
            }
            return out;
        }
    private:
        JsonValue& parent_;
        std::string key_;
    };

    MemberProxy operator[](const std::string& key) {
        return MemberProxy(*this, key);
    }

    const JsonValue* find(const std::string& key) const {
        JsonObjectConst obj = as_object();
        if (obj.isNull() || !obj.containsKey(key.c_str())) return 0;
        std::shared_ptr<JsonValue> child(new JsonValue());
        child->doc_.set(obj[key.c_str()]);
        find_cache_[key] = child;
        return child.get();
    }
#endif

private:
    ProtocolJsonDocument doc_;

#if !defined(ARDUINO)
    mutable std::string string_cache_;
    mutable std::map<std::string, std::shared_ptr<JsonValue> > find_cache_;
#endif

    void ensure_object() {
        if (!is_object()) {
            doc_.clear();
            doc_.to<JsonObject>();
            clear_cache();
        }
    }

    void ensure_array() {
        if (!is_array()) {
            doc_.clear();
            doc_.to<JsonArray>();
            clear_cache();
        }
    }

    void clear_cache() const {
#if !defined(ARDUINO)
        string_cache_.clear();
        find_cache_.clear();
#endif
    }
};

#undef PYPILOT_CLIENT_PROTOCOL_DOC_INIT

inline bool parse_json(const char* text, JsonValue& out, const char** error = 0) {
    return out.parse(text, error);
}

inline bool parse_json(const char* text, size_t len, JsonValue& out, const char** error = 0) {
    return out.parse(text, len, error);
}

inline size_t append_raw(char* out, size_t cap, size_t pos, const char* s) {
    if (!out || cap == 0 || !s) return pos;
    while (*s && pos + 1 < cap) out[pos++] = *s++;
    out[pos] = '\0';
    return pos;
}

inline size_t make_record(char* out, size_t cap, const char* name, const JsonValue& value) {
    if (!out || cap == 0) return 0;
    size_t pos = 0;
    pos = append_raw(out, cap, pos, name ? name : "");
    pos = append_raw(out, cap, pos, "=");
    if (pos < cap) {
        size_t n = serializeJson(value.variant(), out + pos, cap - pos);
        pos += n;
        if (pos >= cap) pos = cap - 1;
        out[pos] = '\0';
    }
    pos = append_raw(out, cap, pos, "\n");
    return pos;
}

inline size_t make_record_raw(char* out, size_t cap, const char* name, const char* json_value) {
    JsonValue value;
    const char* err = 0;
    if (!value.parse(json_value ? json_value : "null", &err)) value = JsonValue();
    return make_record(out, cap, name, value);
}

inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) {
    return make_record(out, cap, name, JsonValue(value));
}

inline size_t make_set_number(char* out, size_t cap, const char* name, double value, unsigned decimals = 6) {
    (void)decimals;
    return make_record(out, cap, name, JsonValue(value));
}

inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) {
    return make_record(out, cap, name, JsonValue(value ? value : ""));
}

inline size_t make_watch(char* out, size_t cap, const char* name, const JsonValue& period) {
    JsonValue value = JsonValue::object();
    value.set_member(name, period);
    return make_record(out, cap, "watch", value);
}

inline size_t make_watch_raw(char* out, size_t cap, const char* name, const char* period_json) {
    JsonValue period;
    const char* err = 0;
    if (!period.parse(period_json ? period_json : "null", &err)) period = JsonValue();
    return make_watch(out, cap, name, period);
}

inline size_t make_watch_continuous(char* out, size_t cap, const char* name) {
    return make_watch(out, cap, name, JsonValue(true));
}

inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds, unsigned decimals = 6) {
    (void)decimals;
    return make_watch(out, cap, name, JsonValue(seconds));
}

inline size_t make_unwatch(char* out, size_t cap, const char* name) {
    return make_watch(out, cap, name, JsonValue(false));
}

inline size_t make_watch_values(char* out, size_t cap) {
    return make_watch(out, cap, "values", JsonValue(true));
}

inline size_t make_udp_port(char* out, size_t cap, int port) {
    return make_record(out, cap, "udp_port", JsonValue(port));
}

inline size_t make_keepalive(char* out, size_t cap) {
    if (!out || cap == 0) return 0;
    out[0] = '\n';
    if (cap > 1) out[1] = '\0';
    return 1;
}

#if defined(ARDUINO)

struct Record {
    char name[64];
    JsonValue value;
    void clear() { name[0] = '\0'; value = JsonValue(); }
};

inline void safe_copy(char* dst, size_t dst_size, const char* src, size_t src_len) {
    if (!dst || dst_size == 0) return;
    size_t n = src_len < dst_size - 1 ? src_len : dst_size - 1;
    if (src && n) memcpy(dst, src, n);
    dst[n] = '\0';
}

inline bool parse_record(const char* line, Record& out, const char** error = 0) {
    out.clear();
    if (!line || !line[0]) { if (error) *error = "empty line"; return false; }
    const char* eq = strchr(line, '=');
    if (!eq || eq == line) { if (error) *error = "record has no name or '='"; return false; }
    safe_copy(out.name, sizeof(out.name), line, static_cast<size_t>(eq - line));
    const char* rhs = eq + 1;
    size_t rhs_len = strlen(rhs);
    while (rhs_len && (rhs[rhs_len - 1] == '\n' || rhs[rhs_len - 1] == '\r')) --rhs_len;
    return out.value.parse(rhs, rhs_len, error);
}

class LineParser {
public:
    explicit LineParser(size_t max_line = sizeof(buffer_)) : max_line_(max_line), pos_(0) { buffer_[0] = '\0'; }
    void clear() { pos_ = 0; buffer_[0] = '\0'; }

    bool push(char c, Record& out, const char** error = 0) {
        if (c == '\r') return false;
        if (c == '\n') {
            buffer_[pos_] = '\0';
            bool ok = parse_record(buffer_, out, error);
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

private:
    char buffer_[256];
    size_t max_line_;
    size_t pos_;
};

class ClientProtocol {
public:
    explicit ClientProtocol(Client& client) : client_(client), line_parser_(), last_error_(0) {}

    bool connected() const { return client_.connected(); }
    const char* last_error() const { return last_error_ ? last_error_ : ""; }

    bool send_raw(const char* line) {
        if (!line) return false;
        size_t len = strlen(line);
        return client_.write(reinterpret_cast<const uint8_t*>(line), len) == len;
    }

    bool set(const char* name, const JsonValue& value) {
        char line[256];
        size_t n = make_record(line, sizeof(line), name, value);
        return client_.write(reinterpret_cast<const uint8_t*>(line), n) == n;
    }

    bool set_bool(const char* name, bool value) { return set(name, JsonValue(value)); }
    bool set_number(const char* name, double value) { return set(name, JsonValue(value)); }
    bool set_string(const char* name, const char* value) { return set(name, JsonValue(value ? value : "")); }

    bool watch(const char* name, const JsonValue& period) {
        char line[256];
        size_t n = make_watch(line, sizeof(line), name, period);
        return client_.write(reinterpret_cast<const uint8_t*>(line), n) == n;
    }

    bool watch_continuous(const char* name) { return watch(name, JsonValue(true)); }
    bool watch_periodic(const char* name, double seconds) { return watch(name, JsonValue(seconds)); }
    bool unwatch(const char* name) { return watch(name, JsonValue(false)); }
    bool watch_values() { return watch("values", JsonValue(true)); }
    bool udp_port(int port) { return set("udp_port", JsonValue(port)); }
    bool keepalive() { return send_raw("\n"); }

    bool poll(Record& out) {
        if (!client_.available()) return false;
        int b = client_.read();
        if (b < 0) return false;
        const char* err = 0;
        bool ok = line_parser_.push(static_cast<char>(b), out, &err);
        if (!ok && err && strcmp(err, "empty line") != 0) last_error_ = err;
        return ok;
    }

private:
    Client& client_;
    LineParser line_parser_;
    const char* last_error_;
};

#else

inline bool parse_json(const std::string& text, JsonValue& out, std::string* error = 0) {
    const char* err = 0;
    bool ok = out.parse(text.c_str(), text.size(), &err);
    if (!ok && error) *error = err ? err : "json parse error";
    return ok;
}

struct Record {
    std::string name;
    JsonValue value;
};

inline bool parse_record(const std::string& line, Record& out, std::string* error = 0) {
    std::string s = line;
    while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
    size_t eq = s.find('=');
    if (eq == std::string::npos) { if (error) *error = "record has no '='"; return false; }
    out.name = s.substr(0, eq);
    if (out.name.empty()) { if (error) *error = "empty name"; return false; }
    return parse_json(s.substr(eq + 1), out.value, error);
}

inline std::string make_record(const std::string& name, const JsonValue& value) {
    return name + "=" + value.dump() + "\n";
}

inline std::string make_set_bool(const std::string& name, bool value) {
    return make_record(name, JsonValue(value));
}

inline std::string make_set_number(const std::string& name, double value) {
    return make_record(name, JsonValue(value));
}

inline std::string make_set_string(const std::string& name, const std::string& value) {
    return make_record(name, JsonValue(value));
}

inline std::string make_watch(const std::string& name, const JsonValue& period) {
    JsonValue value = JsonValue::object();
    value.set_member(name.c_str(), period);
    return make_record("watch", value);
}

inline std::string make_watch_continuous(const std::string& name) {
    return make_watch(name, JsonValue(true));
}

inline std::string make_watch_periodic(const std::string& name, double seconds) {
    return make_watch(name, JsonValue(seconds));
}

inline std::string make_unwatch(const std::string& name) {
    return make_watch(name, JsonValue(false));
}

inline std::string make_watch_values() {
    return make_watch("values", JsonValue(true));
}

inline std::string make_udp_port(int port) {
    return make_record("udp_port", JsonValue(port));
}

inline std::string make_keepalive() {
    return "\n";
}

inline size_t copy_string(char* out, size_t cap, const std::string& s) {
    if (!out || cap == 0) return 0;
    size_t n = s.size() < cap - 1 ? s.size() : cap - 1;
    memcpy(out, s.data(), n);
    out[n] = '\0';
    return n;
}

inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) {
    return copy_string(out, cap, make_set_bool(std::string(name ? name : ""), value));
}

inline size_t make_set_number(char* out, size_t cap, const char* name, double value) {
    return copy_string(out, cap, make_set_number(std::string(name ? name : ""), value));
}

inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) {
    return copy_string(out, cap, make_set_string(std::string(name ? name : ""), std::string(value ? value : "")));
}

inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds) {
    return copy_string(out, cap, make_watch_periodic(std::string(name ? name : ""), seconds));
}

inline size_t make_watch_continuous(char* out, size_t cap, const char* name) {
    return copy_string(out, cap, make_watch_continuous(std::string(name ? name : "")));
}

inline size_t make_unwatch(char* out, size_t cap, const char* name) {
    return copy_string(out, cap, make_unwatch(std::string(name ? name : "")));
}

inline size_t make_watch_values(char* out, size_t cap) {
    return copy_string(out, cap, make_watch_values());
}

inline size_t make_udp_port(char* out, size_t cap, int port) {
    return copy_string(out, cap, make_udp_port(port));
}

inline size_t make_keepalive(char* out, size_t cap) {
    return copy_string(out, cap, make_keepalive());
}

class LineParser {
public:
    explicit LineParser(size_t max_line = 8192) : max_line_(max_line) {}
    void clear() { buffer_.clear(); }

    bool push(char c, Record& out, std::string* error = 0) {
        if (c == '\n') {
            std::string line = buffer_;
            buffer_.clear();
            if (line.empty()) { if (error) *error = "empty line"; return false; }
            return parse_record(line, out, error);
        }
        if (c != '\r') buffer_ += c;
        if (buffer_.size() > max_line_) {
            buffer_.clear();
            if (error) *error = "line too long";
            return false;
        }
        return false;
    }

private:
    std::string buffer_;
    size_t max_line_;
};

class Transport {
public:
    virtual ~Transport() {}
    virtual bool connected() const = 0;
    virtual int read_byte() = 0;
    virtual bool write_data(const uint8_t* data, size_t len) = 0;
    bool write_string(const std::string& s) {
        return write_data(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
};

class ClientProtocol {
public:
    explicit ClientProtocol(Transport& transport) : transport_(transport), line_parser_(), last_error_() {}

    bool connected() const { return transport_.connected(); }
    const std::string& last_error() const { return last_error_; }

    bool send_raw(const std::string& line) { return transport_.write_string(line); }
    bool set(const std::string& name, const JsonValue& value) { return send_raw(make_record(name, value)); }
    bool set_bool(const std::string& name, bool value) { return send_raw(make_set_bool(name, value)); }
    bool set_number(const std::string& name, double value) { return send_raw(make_set_number(name, value)); }
    bool set_string(const std::string& name, const std::string& value) { return send_raw(make_set_string(name, value)); }
    bool watch(const std::string& name, const JsonValue& period) { return send_raw(make_watch(name, period)); }
    bool watch_continuous(const std::string& name) { return send_raw(make_watch_continuous(name)); }
    bool watch_periodic(const std::string& name, double seconds) { return send_raw(make_watch_periodic(name, seconds)); }
    bool unwatch(const std::string& name) { return send_raw(make_unwatch(name)); }
    bool watch_values() { return send_raw(make_watch_values()); }
    bool udp_port(int port) { return send_raw(make_udp_port(port)); }
    bool keepalive() { return send_raw(make_keepalive()); }

    bool poll(Record& out) {
        int b = transport_.read_byte();
        if (b < 0) return false;
        std::string err;
        bool ok = line_parser_.push(static_cast<char>(b), out, &err);
        if (!ok && !err.empty() && err != "empty line") last_error_ = err;
        return ok;
    }

private:
    Transport& transport_;
    LineParser line_parser_;
    std::string last_error_;
};

#endif

} // namespace pypilot_client_protocol

#if defined(PYPILOT_CLIENT_PROTOCOL_ENABLE_LINUX_TCP) && !defined(ARDUINO)
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>

namespace pypilot_client_protocol {

class LinuxTcpTransport : public Transport {
public:
    LinuxTcpTransport() : fd_(-1) {}
    virtual ~LinuxTcpTransport() { close(); }

    bool connect_to(const char* host, int port = DEFAULT_PORT) {
        close();
        char service[16];
        std::snprintf(service, sizeof(service), "%d", port);

        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res = 0;
        if (getaddrinfo(host, service, &hints, &res) != 0) return false;

        for (addrinfo* p = res; p; p = p->ai_next) {
            fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd_ < 0) continue;
            if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) break;
            ::close(fd_);
            fd_ = -1;
        }

        freeaddrinfo(res);
        if (fd_ < 0) return false;

        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        return true;
    }

    bool connected() const { return fd_ >= 0; }

    int read_byte() {
        if (fd_ < 0) return -1;
        uint8_t b = 0;
        ssize_t n = ::recv(fd_, &b, 1, 0);
        if (n == 1) return b;
        if (n == 0) {
            close();
            return -1;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return -1;
        close();
        return -1;
    }

    bool write_data(const uint8_t* data, size_t len) {
        if (fd_ < 0) return false;
        size_t written = 0;
        while (written < len) {
            ssize_t n = ::send(fd_, data + written, len - written, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                close();
                return false;
            }
            written += static_cast<size_t>(n);
        }
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_;
};

} // namespace pypilot_client_protocol
#endif
