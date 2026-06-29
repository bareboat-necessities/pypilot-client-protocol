#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

#include <pypilot_client_protocol.hpp>

using namespace pypilot_client_protocol;

int main() {
    const char* error = 0;
    char name[PYPILOT_CLIENT_PROTOCOL_MAX_NAME];
    JsonDocument value;

    assert(parse_record("ap.enabled=true\n", name, sizeof(name), value, &error));
    assert(std::strcmp(name, "ap.enabled") == 0);
    assert(value.as<bool>() == true);

    value.clear();
    assert(parse_record("ap.heading=123.4\n", name, sizeof(name), value, &error));
    assert(std::strcmp(name, "ap.heading") == 0);
    assert(std::fabs(value.as<double>() - 123.4) < 0.001);

    char out[128];
    make_set_bool(out, sizeof(out), "ap.enabled", false);
    assert(std::string(out) == "ap.enabled=false\n");

    make_watch_periodic(out, sizeof(out), "ap.heading", 0.5);
    assert(std::string(out).find("watch=") == 0);
    assert(std::string(out).find("ap.heading") != std::string::npos);

    LineDecoder decoder;
    value.clear();
    bool got = false;
    const char* line = "ap.mode=123\n";
    for (const char* p = line; *p && !got; ++p) {
        got = decoder.push(*p, name, sizeof(name), value, &error);
    }
    assert(got);
    assert(std::strcmp(name, "ap.mode") == 0);
    assert(value.as<int>() == 123);

    return 0;
}
