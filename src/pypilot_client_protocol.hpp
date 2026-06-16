#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <cstdio>
#endif

namespace pypilot_client_protocol {

static const int DEFAULT_PORT = 23322;
static const int DEFAULT_UDP_CONTROL_PORT = 43822;

#if defined(ARDUINO)

struct Record {
    char name[64];
    char value[192];
    void clear() { name[0] = '\0'; value[0] = '\0'; }
};

inline void safe_copy(char* dst, size_t dst_size, const char* src, size_t src_len) {
    if (!dst || dst_size == 0) return;
    size_t n = src_len < dst_size - 1 ? src_len : dst_size - 1;
    if (src && n) memcpy(dst, src, n);
    dst[n] = '\0';
}

inline void trim_number(char* s) {
    if (!s) return;
    char* first = s;
    while (*first == ' ') ++first;
    if (first != s) memmove(s, first, strlen(first) + 1);
    char* end = s + strlen(s);
    while (end > s && end[-1] == '0') --end;
    if (end > s && end[-1] == '.') --end;
    if (end == s || (end == s + 1 && s[0] == '-')) *end++ = '0';
    *end = '\0';
}

inline void format_number(double value, char* out, size_t out_size, unsigned decimals = 6) {
    if (!out || out_size == 0) return;
    dtostrf(value, 0, decimals, out);
    trim_number(out);
}

class LineParser {
public:
    explicit LineParser(size_t max_line = sizeof(buffer_)) : max_line_(max_line), pos_(0) { buffer_[0] = '\0'; }
    void clear() { pos_ = 0; buffer_[0] = '\0'; }

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
        if (!line || !line[0]) { if (error) *error = "empty line"; return false; }
        const char* eq = strchr(line, '=');
        if (!eq || eq == line) { if (error) *error = "record has no name or '='"; return false; }
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
        if (c == '"' || c == '\\') { out[pos++] = '\\'; out[pos++] = c; }
        else if (c == '\n') { out[pos++] = '\\'; out[pos++] = 'n'; }
        else if (c == '\r') { out[pos++] = '\\'; out[pos++] = 'r'; }
        else if (c == '\t') { out[pos++] = '\\'; out[pos++] = 't'; }
        else out[pos++] = c;
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

inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) { return make_record_raw(out, cap, name, value ? "true" : "false"); }
inline size_t make_set_number(char* out, size_t cap, const char* name, double value, unsigned decimals = 6) { char num[32]; format_number(value, num, sizeof(num), decimals); return make_record_raw(out, cap, name, num); }
inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) { char json[128]; json[0] = '\0'; append_quoted(json, sizeof(json), 0, value); return make_record_raw(out, cap, name, json); }
inline size_t make_watch_raw(char* out, size_t cap, const char* name, const char* period_json) { size_t pos = 0; pos = append_raw(out, cap, pos, "watch={\""); pos = append_raw(out, cap, pos, name); pos = append_raw(out, cap, pos, "\":"); pos = append_raw(out, cap, pos, period_json); pos = append_raw(out, cap, pos, "}\n"); return pos; }
inline size_t make_watch_continuous(char* out, size_t cap, const char* name) { return make_watch_raw(out, cap, name, "true"); }
inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds, unsigned decimals = 6) { char num[32]; format_number(seconds, num, sizeof(num), decimals); return make_watch_raw(out, cap, name, num); }
inline size_t make_unwatch(char* out, size_t cap, const char* name) { return make_watch_raw(out, cap, name, "false"); }
inline size_t make_watch_values(char* out, size_t cap) { return make_watch_raw(out, cap, "values", "true"); }
inline size_t make_udp_port(char* out, size_t cap, int port) { char num[16]; ltoa(port, num, 10); return make_record_raw(out, cap, "udp_port", num); }
inline size_t make_keepalive(char* out, size_t cap) { if (!out || cap == 0) return 0; out[0] = '\n'; if (cap > 1) out[1] = '\0'; return 1; }

#else

class JsonValue {
public:
    enum Type { Null, Boolean, Number, String, Object };
    typedef std::map<std::string, JsonValue> ObjectType;

    JsonValue() : type_(Null), boolean_(false), number_(0.0) {}
    JsonValue(bool b) : type_(Boolean), boolean_(b), number_(0.0) {}
    JsonValue(int n) : type_(Number), boolean_(false), number_(static_cast<double>(n)) {}
    JsonValue(double n) : type_(Number), boolean_(false), number_(n) {}
    JsonValue(const char* s) : type_(String), boolean_(false), number_(0.0), string_(s ? s : "") {}
    JsonValue(const std::string& s) : type_(String), boolean_(false), number_(0.0), string_(s) {}

    static JsonValue object() { JsonValue v; v.type_ = Object; return v; }

    bool is_null() const { return type_ == Null; }
    bool is_bool() const { return type_ == Boolean; }
    bool is_number() const { return type_ == Number; }
    bool is_string() const { return type_ == String; }
    bool is_object() const { return type_ == Object; }

    bool as_bool(bool fallback = false) const { return type_ == Boolean ? boolean_ : fallback; }
    double as_number(double fallback = 0.0) const { return type_ == Number ? number_ : fallback; }
    const std::string& as_string() const { return string_; }
    const ObjectType& as_object() const { return object_; }
    ObjectType& as_object() { ensure_object(); return object_; }

    JsonValue& operator[](const std::string& key) { ensure_object(); return object_[key]; }
    const JsonValue* find(const std::string& key) const { if (type_ != Object) return nullptr; ObjectType::const_iterator it = object_.find(key); return it == object_.end() ? nullptr : &it->second; }

    std::string dump() const {
        switch (type_) {
        case Null: return "null";
        case Boolean: return boolean_ ? "true" : "false";
        case Number: return dump_number(number_);
        case String: return quote(string_);
        case Object: {
            std::string out = "{";
            bool first = true;
            for (ObjectType::const_iterator it = object_.begin(); it != object_.end(); ++it) {
                if (!first) out += ",";
                first = false;
                out += quote(it->first);
                out += ":";
                out += it->second.dump();
            }
            out += "}";
            return out;
        }
        }
        return "null";
    }

    static std::string quote(const std::string& s) {
        std::string out = "\"";
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            switch (c) { case '"': out += "\\\""; break; case '\\': out += "\\\\"; break; case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break; default: out += c; break; }
        }
        out += "\"";
        return out;
    }
private:
    Type type_;
    bool boolean_;
    double number_;
    std::string string_;
    ObjectType object_;

    void ensure_object() { if (type_ != Object) { boolean_ = false; number_ = 0.0; string_.clear(); object_.clear(); type_ = Object; } }
    static std::string dump_number(double n) { std::ostringstream ss; ss << std::setprecision(15) << n; std::string s = ss.str(); if (s.find('.') != std::string::npos) { while (!s.empty() && s[s.size() - 1] == '0') s.erase(s.size() - 1); if (!s.empty() && s[s.size() - 1] == '.') s.erase(s.size() - 1); } return s.empty() ? "0" : s; }
};

class JsonParser {
public:
    JsonParser(const char* text, size_t len) : text_(text), len_(len), pos_(0), error_() {}
    bool parse(JsonValue& out) { skip_ws(); if (!parse_value(out)) return false; skip_ws(); if (pos_ != len_) { error_ = "trailing data"; return false; } return true; }
    const std::string& error() const { return error_; }
private:
    const char* text_; size_t len_; size_t pos_; std::string error_;
    void skip_ws() { while (pos_ < len_ && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\r' || text_[pos_] == '\n')) ++pos_; }
    bool consume(char c) { if (pos_ < len_ && text_[pos_] == c) { ++pos_; return true; } return false; }
    bool match(const char* s) { size_t n = strlen(s); if (pos_ + n > len_ || strncmp(text_ + pos_, s, n) != 0) return false; pos_ += n; return true; }
    bool parse_value(JsonValue& out) { skip_ws(); if (pos_ >= len_) { error_ = "unexpected end"; return false; } char c = text_[pos_]; if (c == 'n') { if (match("null")) { out = JsonValue(); return true; } error_ = "invalid null"; return false; } if (c == 't') { if (match("true")) { out = JsonValue(true); return true; } error_ = "invalid true"; return false; } if (c == 'f') { if (match("false")) { out = JsonValue(false); return true; } error_ = "invalid false"; return false; } if (c == '"') return parse_string_value(out); if (c == '{') return parse_object(out); if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out); error_ = "unexpected character"; return false; }
    bool parse_string_raw(std::string& out) { if (!consume('"')) { error_ = "expected string"; return false; } out.clear(); while (pos_ < len_) { char c = text_[pos_++]; if (c == '"') return true; if (c == '\\') { if (pos_ >= len_) { error_ = "bad escape"; return false; } char e = text_[pos_++]; switch (e) { case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break; case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break; case 'u': if (pos_ + 4 > len_) { error_ = "short unicode escape"; return false; } out += '?'; pos_ += 4; break; default: error_ = "unknown escape"; return false; } } else out += c; } error_ = "unterminated string"; return false; }
    bool parse_string_value(JsonValue& out) { std::string s; if (!parse_string_raw(s)) return false; out = JsonValue(s); return true; }
    bool parse_number(JsonValue& out) { const char* start = text_ + pos_; char* end = nullptr; double value = strtod(start, &end); if (end == start) { error_ = "invalid number"; return false; } pos_ += static_cast<size_t>(end - start); out = JsonValue(value); return true; }
    bool parse_object(JsonValue& out) { if (!consume('{')) return false; out = JsonValue::object(); skip_ws(); if (consume('}')) return true; while (true) { std::string key; if (!parse_string_raw(key)) return false; skip_ws(); if (!consume(':')) { error_ = "expected colon in object"; return false; } JsonValue value; if (!parse_value(value)) return false; out[key] = value; skip_ws(); if (consume('}')) return true; if (!consume(',')) { error_ = "expected comma in object"; return false; } skip_ws(); } }
};

inline bool parse_json(const std::string& text, JsonValue& out, std::string* error = nullptr) { JsonParser parser(text.c_str(), text.size()); bool ok = parser.parse(out); if (!ok && error) *error = parser.error(); return ok; }
struct Record { std::string name; JsonValue value; };
inline bool parse_record(const std::string& line, Record& out, std::string* error = nullptr) { std::string s = line; while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1); size_t eq = s.find('='); if (eq == std::string::npos) { if (error) *error = "record has no '='"; return false; } out.name = s.substr(0, eq); if (out.name.empty()) { if (error) *error = "empty name"; return false; } return parse_json(s.substr(eq + 1), out.value, error); }

inline std::string make_record(const std::string& name, const JsonValue& value) { return name + "=" + value.dump() + "\n"; }
inline std::string make_set_bool(const std::string& name, bool value) { return make_record(name, JsonValue(value)); }
inline std::string make_set_number(const std::string& name, double value) { return make_record(name, JsonValue(value)); }
inline std::string make_set_string(const std::string& name, const std::string& value) { return make_record(name, JsonValue(value)); }
inline std::string make_watch(const std::string& name, const JsonValue& period) { JsonValue obj = JsonValue::object(); obj[name] = period; return make_record("watch", obj); }
inline std::string make_watch_continuous(const std::string& name) { return make_watch(name, JsonValue(true)); }
inline std::string make_watch_periodic(const std::string& name, double seconds) { return make_watch(name, JsonValue(seconds)); }
inline std::string make_unwatch(const std::string& name) { return make_watch(name, JsonValue(false)); }
inline std::string make_watch_values() { return make_watch("values", JsonValue(true)); }
inline std::string make_udp_port(int port) { return make_record("udp_port", JsonValue(port)); }
inline std::string make_keepalive() { return "\n"; }

inline size_t copy_string(char* out, size_t cap, const std::string& s) { if (!out || cap == 0) return 0; size_t n = s.size() < cap - 1 ? s.size() : cap - 1; memcpy(out, s.data(), n); out[n] = '\0'; return n; }
inline size_t make_set_bool(char* out, size_t cap, const char* name, bool value) { return copy_string(out, cap, make_set_bool(std::string(name ? name : ""), value)); }
inline size_t make_set_number(char* out, size_t cap, const char* name, double value) { return copy_string(out, cap, make_set_number(std::string(name ? name : ""), value)); }
inline size_t make_set_string(char* out, size_t cap, const char* name, const char* value) { return copy_string(out, cap, make_set_string(std::string(name ? name : ""), std::string(value ? value : ""))); }
inline size_t make_watch_periodic(char* out, size_t cap, const char* name, double seconds) { return copy_string(out, cap, make_watch_periodic(std::string(name ? name : ""), seconds)); }
inline size_t make_watch_continuous(char* out, size_t cap, const char* name) { return copy_string(out, cap, make_watch_continuous(std::string(name ? name : ""))); }
inline size_t make_unwatch(char* out, size_t cap, const char* name) { return copy_string(out, cap, make_unwatch(std::string(name ? name : ""))); }
inline size_t make_watch_values(char* out, size_t cap) { return copy_string(out, cap, make_watch_values()); }
inline size_t make_udp_port(char* out, size_t cap, int port) { return copy_string(out, cap, make_udp_port(port)); }
inline size_t make_keepalive(char* out, size_t cap) { return copy_string(out, cap, make_keepalive()); }

class LineParser { public: explicit LineParser(size_t max_line = 8192) : max_line_(max_line) {} void clear() { buffer_.clear(); } bool push(char c, Record& out, std::string* error = nullptr) { if (c == '\n') { std::string line = buffer_; buffer_.clear(); if (line.empty()) { if (error) *error = "empty line"; return false; } return parse_record(line, out, error); } if (c != '\r') buffer_ += c; if (buffer_.size() > max_line_) { buffer_.clear(); if (error) *error = "line too long"; return false; } return false; } private: std::string buffer_; size_t max_line_; };
class Transport { public: virtual ~Transport() {} virtual bool connected() const = 0; virtual int read_byte() = 0; virtual bool write_data(const uint8_t* data, size_t len) = 0; bool write_string(const std::string& s) { return write_data(reinterpret_cast<const uint8_t*>(s.data()), s.size()); } };
class ClientProtocol { public: explicit ClientProtocol(Transport& t) : transport_(t), line_parser_(), last_error_() {} bool connected() const { return transport_.connected(); } const std::string& last_error() const { return last_error_; } bool send_raw(const std::string& line) { return transport_.write_string(line); } bool set(const std::string& name, const JsonValue& value) { return send_raw(make_record(name, value)); } bool set_bool(const std::string& name, bool value) { return send_raw(make_set_bool(name, value)); } bool set_number(const std::string& name, double value) { return send_raw(make_set_number(name, value)); } bool set_string(const std::string& name, const std::string& value) { return send_raw(make_set_string(name, value)); } bool watch(const std::string& name, const JsonValue& period) { return send_raw(make_watch(name, period)); } bool watch_continuous(const std::string& name) { return send_raw(make_watch_continuous(name)); } bool watch_periodic(const std::string& name, double seconds) { return send_raw(make_watch_periodic(name, seconds)); } bool unwatch(const std::string& name) { return send_raw(make_unwatch(name)); } bool watch_values() { return send_raw(make_watch_values()); } bool udp_port(int port) { return send_raw(make_udp_port(port)); } bool keepalive() { return send_raw(make_keepalive()); } bool poll(Record& out) { int b = transport_.read_byte(); if (b < 0) return false; std::string err; bool ok = line_parser_.push(static_cast<char>(b), out, &err); if (!ok && !err.empty() && err != "empty line") last_error_ = err; return ok; } private: Transport& transport_; LineParser line_parser_; std::string last_error_; };

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
class LinuxTcpTransport : public Transport { public: LinuxTcpTransport() : fd_(-1) {} virtual ~LinuxTcpTransport() { close(); } bool connect_to(const char* host, int port = DEFAULT_PORT) { close(); char service[16]; std::snprintf(service, sizeof(service), "%d", port); addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; addrinfo* res = nullptr; if (getaddrinfo(host, service, &hints, &res) != 0) return false; for (addrinfo* p = res; p; p = p->ai_next) { fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol); if (fd_ < 0) continue; if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) break; ::close(fd_); fd_ = -1; } freeaddrinfo(res); if (fd_ < 0) return false; int flags = fcntl(fd_, F_GETFL, 0); if (flags >= 0) fcntl(fd_, F_SETFL, flags | O_NONBLOCK); return true; } bool connected() const { return fd_ >= 0; } int read_byte() { if (fd_ < 0) return -1; uint8_t b = 0; ssize_t n = ::recv(fd_, &b, 1, 0); if (n == 1) return b; if (n == 0) { close(); return -1; } if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return -1; close(); return -1; } bool write_data(const uint8_t* data, size_t len) { if (fd_ < 0) return false; size_t written = 0; while (written < len) { ssize_t n = ::send(fd_, data + written, len - written, MSG_NOSIGNAL); if (n < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; close(); return false; } written += static_cast<size_t>(n); } return true; } void close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } } private: int fd_; };
} // namespace pypilot_client_protocol
#endif
