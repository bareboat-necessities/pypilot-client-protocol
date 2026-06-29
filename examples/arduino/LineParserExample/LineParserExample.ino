#include <Arduino.h>
#include <ArduinoJson.h>
#include <pypilot_client_protocol.hpp>

pypilot_client_protocol::LineDecoder decoder;
char record_name[PYPILOT_CLIENT_PROTOCOL_MAX_NAME];
JsonDocument record_value;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("paste pypilot records like ap.heading=123.4");
}

void loop() {
  while (Serial.available()) {
    const char* error = nullptr;
    if (decoder.push((char)Serial.read(), record_name, sizeof(record_name), record_value, &error)) {
      Serial.print("name=");
      Serial.print(record_name);
      Serial.print(" value=");
      serializeJson(record_value, Serial);
      Serial.println();
      record_value.clear();
    } else if (error) {
      Serial.print("error=");
      Serial.println(error);
      record_value.clear();
    }
  }
}
