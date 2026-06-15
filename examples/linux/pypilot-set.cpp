#include <cstdlib>
#include <iostream>
#include <pypilot_client_protocol.hpp>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0] << " host port name json-value\n";
        return 2;
    }
    pypilot_client_protocol::JsonValue value;
    std::string error;
    if (!pypilot_client_protocol::parse_json(argv[4], value, &error)) {
        std::cerr << "bad json value: " << error << "\n";
        return 2;
    }
    pypilot_client_protocol::LinuxTcpTransport transport;
    if (!transport.connect_to(argv[1], std::atoi(argv[2]))) {
        std::cerr << "connect failed\n";
        return 1;
    }
    pypilot_client_protocol::ClientProtocol client(transport);
    if (!client.set(argv[3], value)) return 1;
    return 0;
}
