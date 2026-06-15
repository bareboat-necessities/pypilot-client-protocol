#include <Arduino.h>
#include <pypilot_client_protocol.hpp>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.print(pypilot_client_protocol::make_watch_periodic("ap.heading", 0.5).c_str());
  Serial.print(pypilot_client_protocol::make_set_string("ap.mode", "compass").c_str());
  Serial.print(pypilot_client_protocol::make_set_number("ap.heading_command", 90.0).c_str());
}

void loop() {}
