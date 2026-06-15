#include <Arduino.h>
#include <pypilot_client_protocol.hpp>

pypilot_client_protocol::LineParser parser;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("paste pypilot records like ap.heading=123.4");
}

void loop() {
  while (Serial.available()) {
    pypilot_client_protocol::Record record;
    std::string error;
    if (parser.push((char)Serial.read(), record, &error)) {
      Serial.print("name=");
      Serial.print(record.name.c_str());
      Serial.print(" value=");
      Serial.println(record.value.dump().c_str());
    }
  }
}
