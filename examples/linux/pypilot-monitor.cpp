#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <pypilot_client_protocol.hpp>

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::atoi(argv[2]) : pypilot_client_protocol::DEFAULT_PORT;
    std::string name = argc > 3 ? argv[3] : "ap.heading";
    double period = argc > 4 ? std::atof(argv[4]) : 0.5;
    pypilot_client_protocol::LinuxTcpTransport transport;
    if (!transport.connect_to(host, port)) return 1;
    pypilot_client_protocol::ClientProtocol client(transport);
    client.watch_periodic(name, period);
    while (transport.connected()) {
        pypilot_client_protocol::Record record;
        if (client.poll(record)) std::cout << record.name << "=" << record.value.dump() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}
