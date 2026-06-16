#include <Arduino.h>
#include <pypilot_client_protocol.hpp>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  char line[128];

  pypilot_client_protocol::make_watch_periodic(line, sizeof(line), "ap.heading", 0.5);
  Serial.print(line);

  pypilot_client_protocol::make_set_string(line, sizeof(line), "ap.mode", "compass");
  Serial.print(line);

  pypilot_client_protocol::make_set_number(line, sizeof(line), "ap.heading_command", 90.0);
  Serial.print(line);
}

void loop() {}
