#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <pypilot_client_protocol.hpp>

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::atoi(argv[2]) : pypilot_client_protocol::DEFAULT_PORT;
    pypilot_client_protocol::LinuxTcpTransport transport;
    if (!transport.connect_to(host, port)) {
        std::cerr << "failed to connect to " << host << ":" << port << "\n";
        return 1;
    }
    pypilot_client_protocol::ClientProtocol client(transport);
    client.watch_values();
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        pypilot_client_protocol::Record record;
        if (client.poll(record)) {
            if (record.name == "values") {
                JsonObjectConst values = record.value.as_object();
                for (JsonPairConst kv : values) std::cout << kv.key().c_str() << "\n";
                return 0;
            }
            if (record.name == "error") std::cerr << "server error: " << record.value.dump() << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::cerr << "timeout waiting for values list\n";
    return 2;
}
