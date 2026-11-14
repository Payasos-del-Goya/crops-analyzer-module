const char* MQTT_BROKER_ADRESS = "10.42.0.1";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_NAME = "ESP32Client_1";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void SuscribeMqtt()
{
  mqttClient.subscribe("hello/world");
}

String payload;
void PublisMqtt(const char* payload)
{
  if (!mqttClient.connected())
  {
    return;
  }

  if (!mqttClient.publish("hello/world", payload))
  {
    Serial.println("MQTT publish failed");
  }
}

String content = "";
void OnMqttReceived(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Received on ");
  Serial.print(topic);
  Serial.print(": ");

  content = "";  
  for (size_t i = 0; i < length; i++) {
    content.concat((char)payload[i]);
  }
  Serial.print(content);
  Serial.println();
}