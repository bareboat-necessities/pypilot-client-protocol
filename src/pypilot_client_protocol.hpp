#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARDUINO
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <cstdio>
#else
#include <Arduino.h>
#include <vector>
#include <map>
#include <string>
#endif

namespace pypilot_client_protocol {

static const int DEFAULT_PORT = 23322;
static const int DEFAULT_UDP_CONTROL_PORT = 43822;

class JsonValue {
public:
    enum Type { Null, Boolean, Number, String, Array, Object };
    typedef std::vector<JsonValue> ArrayType;
    typedef std::map<std::string, JsonValue> ObjectType;

    JsonValue() : type_(Null), boolean_(false), number_(0.0) {}
    JsonValue(std::nullptr_t) : type_(Null), boolean_(false), number_(0.0) {}
    JsonValue(bool b) : type_(Boolean), boolean_(b), number_(0.0) {}
    JsonValue(int n) : type_(Number), boolean_(false), number_(static_cast<double>(n)) {}
    JsonValue(double n) : type_(Number), boolean_(false), number_(n) {}
    JsonValue(const char* s) : type_(String), boolean_(false), number_(0.0), string_(s ? s : "") {}
    JsonValue(const std::string& s) : type_(String), boolean_(false), number_(0.0), string_(s) {}

    static JsonValue array() { JsonValue v; v.type_ = Array; return v; }
    static JsonValue object() { JsonValue v; v.type_ = Object; return v; }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Null; }
    bool is_bool() const { return type_ == Boolean; }
    bool is_number() const { return type_ == Number; }
    bool is_string() const { return type_ == String; }
    bool is_array() const { return type_ == Array; }
    bool is_object() const { return type_ == Object; }

    bool as_bool(bool fallback = false) const { return type_ == Boolean ? boolean_ : fallback; }
    double as_number(double fallback = 0.0) const { return type_ == Number ? number_ : fallback; }
    const std::string& as_string() const { return string_; }
    const ArrayType& as_array() const { return array_; }
    const ObjectType& as_object() const { return object_; }
    ArrayType& as_array() { ensure_array(); return array_; }
    ObjectType& as_object() { ensure_object(); return object_; }

    JsonValue& operator[](const std::string& key) { ensure_object(); return object_[key]; }
    const JsonValue* find(const std::string& key) const {
        if (type_ != Object) return nullptr;
        ObjectType::const_iterator it = object_.find(key);
        return it == object_.end() ? nullptr : &it->second;
    }

    void push_back(const JsonValue& v) { ensure_array(); array_.push_back(v); }

    std::string dump() const {
        switch (type_) {
        case Null: return "null";
        case Boolean: return boolean_ ? "true" : "false";
        case Number: return dump_number(number_);
        case String: return quote(string_);
        case Array: {
            std::string out = "[";
            for (size_t i = 0; i < array_.size(); ++i) { if (i) out += ","; out += array_[i].dump(); }
            out += "]";
            return out;
        }
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
            unsigned char c = static_cast<unsigned char>(s[i]);
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { const char hex[] = "0123456789abcdef"; out += "\\u00"; out += hex[(c >> 4) & 0xf]; out += hex[c & 0xf]; }
                else out += static_cast<char>(c);
                break;
            }
        }
        out += "\"";
        return out;
    }

private:
    Type type_;
    bool boolean_;
    double number_;
    std::string string_;
    ArrayType array_;
    ObjectType object_;

    void ensure_array() { if (type_ != Array) { clear(); type_ = Array; } }
    void ensure_object() { if (type_ != Object) { clear(); type_ = Object; } }
    void clear() { boolean_ = false; number_ = 0.0; string_.clear(); array_.clear(); object_.clear(); }

    static std::string dump_number(double n) {
        std::ostringstream ss; ss << std::setprecision(15) << n;
        std::string s = ss.str();
        if (s.find('.') != std::string::npos) { while (!s.empty() && s[s.size()-1] == '0') s.erase(s.size()-1); if (!s.empty() && s[s.size()-1] == '.') s.erase(s.size()-1); }
        return s.empty() ? "0" : s;
    }
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
    bool parse_value(JsonValue& out) {
        skip_ws(); if (pos_ >= len_) { error_ = "unexpected end"; return false; }
        char c = text_[pos_];
        if (c == 'n') { if (match("null")) { out = JsonValue(); return true; } error_ = "invalid null"; return false; }
        if (c == 't') { if (match("true")) { out = JsonValue(true); return true; } error_ = "invalid true"; return false; }
        if (c == 'f') { if (match("false")) { out = JsonValue(false); return true; } error_ = "invalid false"; return false; }
        if (c == '"') return parse_string_value(out);
        if (c == '[') return parse_array(out);
        if (c == '{') return parse_object(out);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
        error_ = "unexpected character"; return false;
    }
    bool parse_string_raw(std::string& out) {
        if (!consume('"')) { error_ = "expected string"; return false; }
        out.clear();
        while (pos_ < len_) {
            char c = text_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= len_) { error_ = "bad escape"; return false; }
                char e = text_[pos_++];
                switch (e) { case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break; case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break; case 'u': if (pos_ + 4 > len_) { error_ = "short unicode escape"; return false; } out += '?'; pos_ += 4; break; default: error_ = "unknown escape"; return false; }
            } else out += c;
        }
        error_ = "unterminated string"; return false;
    }
    bool parse_string_value(JsonValue& out) { std::string s; if (!parse_string_raw(s)) return false; out = JsonValue(s); return true; }
    bool parse_number(JsonValue& out) { const char* start = text_ + pos_; char* end = nullptr; double value = strtod(start, &end); if (end == start) { error_ = "invalid number"; return false; } pos_ += static_cast<size_t>(end - start); out = JsonValue(value); return true; }
    bool parse_array(JsonValue& out) { if (!consume('[')) return false; out = JsonValue::array(); skip_ws(); if (consume(']')) return true; while (true) { JsonValue item; if (!parse_value(item)) return false; out.push_back(item); skip_ws(); if (consume(']')) return true; if (!consume(',')) { error_ = "expected comma in array"; return false; } } }
    bool parse_object(JsonValue& out) { if (!consume('{')) return false; out = JsonValue::object(); skip_ws(); if (consume('}')) return true; while (true) { skip_ws(); std::string key; if (!parse_string_raw(key)) return false; skip_ws(); if (!consume(':')) { error_ = "expected colon in object"; return false; } JsonValue value; if (!parse_value(value)) return false; out[key] = value; skip_ws(); if (consume('}')) return true; if (!consume(',')) { error_ = "expected comma in object"; return false; } } }
};

inline bool parse_json(const std::string& text, JsonValue& out, std::string* error = nullptr) { JsonParser parser(text.c_str(), text.size()); bool ok = parser.parse(out); if (!ok && error) *error = parser.error(); return ok; }

struct Record { std::string name; JsonValue value; };

inline bool parse_record(const std::string& line, Record& out, std::string* error = nullptr) {
    std::string s = line; while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r')) s.erase(s.size()-1);
    size_t eq = s.find('='); if (eq == std::string::npos) { if (error) *error = "record has no '='"; return false; }
    out.name = s.substr(0, eq); std::string rhs = s.substr(eq + 1);
    if (out.name.empty()) { if (error) *error = "empty name"; return false; }
    return parse_json(rhs, out.value, error);
}

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

class LineParser {
public:
    explicit LineParser(size_t max_line = 8192) : max_line_(max_line) {}
    void clear() { buffer_.clear(); }
    bool push(char c, Record& out, std::string* error = nullptr) {
        if (c == '\n') { std::string line = buffer_; buffer_.clear(); if (line.empty()) { if (error) *error = "empty line"; return false; } return parse_record(line, out, error); }
        if (c != '\r') buffer_ += c;
        if (buffer_.size() > max_line_) { buffer_.clear(); if (error) *error = "line too long"; return false; }
        return false;
    }
private:
    std::string buffer_; size_t max_line_;
};

class Transport {
public:
    virtual ~Transport() {}
    virtual bool connected() const = 0;
    virtual int read_byte() = 0;
    virtual bool write_data(const uint8_t* data, size_t len) = 0;
    bool write_string(const std::string& s) { return write_data(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
};

class ClientProtocol {
public:
    explicit ClientProtocol(Transport& t) : transport_(t), line_parser_(), last_error_() {}
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
    bool poll(Record& out) { int b = transport_.read_byte(); if (b < 0) return false; std::string err; bool ok = line_parser_.push(static_cast<char>(b), out, &err); if (!ok && !err.empty() && err != "empty line") last_error_ = err; return ok; }
private:
    Transport& transport_; LineParser line_parser_; std::string last_error_;
};

#ifdef ARDUINO
template <typename ClientLike>
class ArduinoClientTransport : public Transport {
public:
    explicit ArduinoClientTransport(ClientLike& client) : client_(client) {}
    bool connect(const char* host, uint16_t port = DEFAULT_PORT) { return client_.connect(host, port); }
    bool connected() const override { return client_.connected(); }
    int read_byte() override { return client_.available() ? client_.read() : -1; }
    bool write_data(const uint8_t* data, size_t len) override { return client_.write(data, len) == len; }
    void stop() { client_.stop(); }
private:
    ClientLike& client_;
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
    ~LinuxTcpTransport() override { close(); }
    bool connect_to(const char* host, int port = DEFAULT_PORT) {
        close(); char service[16]; std::snprintf(service, sizeof(service), "%d", port);
        addrinfo hints; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr; if (getaddrinfo(host, service, &hints, &res) != 0) return false;
        for (addrinfo* p = res; p; p = p->ai_next) { fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol); if (fd_ < 0) continue; if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) break; ::close(fd_); fd_ = -1; }
        freeaddrinfo(res); if (fd_ < 0) return false; int flags = fcntl(fd_, F_GETFL, 0); if (flags >= 0) fcntl(fd_, F_SETFL, flags | O_NONBLOCK); return true;
    }
    bool connected() const override { return fd_ >= 0; }
    int read_byte() override { if (fd_ < 0) return -1; uint8_t b = 0; ssize_t n = ::recv(fd_, &b, 1, 0); if (n == 1) return b; if (n == 0) { close(); return -1; } if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return -1; close(); return -1; }
    bool write_data(const uint8_t* data, size_t len) override { if (fd_ < 0) return false; size_t written = 0; while (written < len) { ssize_t n = ::send(fd_, data + written, len - written, MSG_NOSIGNAL); if (n < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; close(); return false; } written += static_cast<size_t>(n); } return true; }
    void close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }
private:
    int fd_;
};

class LinuxUdpReceiver {
public:
    LinuxUdpReceiver() : fd_(-1), pos_(0), len_(0) {}
    ~LinuxUdpReceiver() { close(); }
    bool bind_port(int port = DEFAULT_UDP_CONTROL_PORT) {
        close(); fd_ = ::socket(AF_INET, SOCK_DGRAM, 0); if (fd_ < 0) return false;
        sockaddr_in addr; memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(static_cast<uint16_t>(port));
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { close(); return false; }
        int flags = fcntl(fd_, F_GETFL, 0); if (flags >= 0) fcntl(fd_, F_SETFL, flags | O_NONBLOCK); return true;
    }
    bool connected() const { return fd_ >= 0; }
    bool poll(Record& out, std::string* error = nullptr) {
        while (true) {
            if (pos_ >= len_) { ssize_t n = ::recv(fd_, datagram_, sizeof(datagram_) - 1, 0); if (n <= 0) return false; datagram_[n] = 0; len_ = static_cast<size_t>(n); pos_ = 0; }
            char c = static_cast<char>(datagram_[pos_++]); if (parser_.push(c, out, error)) return true;
        }
    }
    void close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } pos_ = len_ = 0; parser_.clear(); }
private:
    int fd_; uint8_t datagram_[2048]; size_t pos_; size_t len_; LineParser parser_;
};

} // namespace pypilot_client_protocol
#endif
