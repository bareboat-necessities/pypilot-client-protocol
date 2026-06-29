#include <cassert>
#include <cmath>
#include <string>
#include <pypilot_client_protocol.hpp>

using namespace pypilot_client_protocol;

struct MemoryTransport : public Transport {
    std::string input;
    std::string output;
    size_t pos = 0;
    bool connected_value = true;
    bool connected() const override { return connected_value; }
    int read_byte() override { return pos < input.size() ? static_cast<unsigned char>(input[pos++]) : -1; }
    bool write_data(const uint8_t* data, size_t len) override { output.append(reinterpret_cast<const char*>(data), len); return true; }
};

int main() {
    JsonValue v;
    std::string error;

    assert(parse_json("true", v, &error));
    assert(v.is_bool() && v.as_bool() == true);

    assert(parse_json("123.5", v, &error));
    assert(v.is_number() && std::fabs(v.as_number() - 123.5) < 0.0001);

    assert(parse_json("\"compass\"", v, &error));
    assert(v.is_string() && v.as_string() == "compass");

    JsonValue object_value = JsonValue::object();
    object_value.set_member("ap.heading", JsonValue(0.5));
    object_value.set_member("ap.enabled", JsonValue(true));
    assert(object_value.is_object());
    assert(object_value.find("ap.heading"));
    assert(std::fabs(object_value.find("ap.heading")->as_number() - 0.5) < 0.0001);
    assert(object_value.find("ap.enabled")->as_bool() == true);

    Record r;
    assert(parse_record("ap.enabled=true\n", r, &error));
    assert(r.name == "ap.enabled");
    assert(r.value.as_bool() == true);

    std::string watch = make_watch_periodic("ap.heading", 0.25);
    assert(watch == "watch={\"ap.heading\":0.25}\n");
    assert(make_set_string("ap.mode", "compass") == "ap.mode=\"compass\"\n");
    assert(make_set_bool("ap.enabled", false) == "ap.enabled=false\n");
    assert(make_watch_values() == "watch={\"values\":true}\n");

    char line[128];
    make_set_string(line, sizeof(line), "ap.mode", "wind");
    assert(std::string(line) == "ap.mode=\"wind\"\n");

    MemoryTransport mt;
    mt.input = "ap.heading=123.4000\nap.mode=\"compass\"\n";
    ClientProtocol client(mt);
    bool got1 = false;
    for (int i = 0; i < 100 && !got1; ++i) got1 = client.poll(r);
    assert(got1);
    assert(r.name == "ap.heading");
    assert(std::fabs(r.value.as_number() - 123.4) < 0.001);
    bool got2 = false;
    for (int i = 0; i < 100 && !got2; ++i) got2 = client.poll(r);
    assert(got2);
    assert(r.name == "ap.mode");
    assert(r.value.as_string() == "compass");

    assert(client.watch_periodic("ap.heading", 0.5));
    assert(client.set_number("ap.heading_command", 90.0));
    assert(mt.output == "watch={\"ap.heading\":0.5}\nap.heading_command=90\n");

    return 0;
}
