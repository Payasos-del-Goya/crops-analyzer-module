#pragma once

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttAttempt = 0;
String mqttClientId;

void OnMqttReceived(char* topic, byte* payload, unsigned int length);

void InitMqtt(uint32_t nodeId)
{
  mqttClientId = "baliza-puente-" + String(nodeId);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE);
  mqttClient.setSocketTimeout(2);
  mqttClient.setCallback(OnMqttReceived);
  Serial.println("mqtt iniciado");
}

void HandleMqtt(bool hasIp)
{
  if (mqttClient.connected())
  {
    mqttClient.loop();
    return;
  }
  if (!hasIp) return;

  unsigned long now = millis();
  if (lastMqttAttempt != 0 && now - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) return;
  lastMqttAttempt = now;

  Serial.printf("MQTT: conectando a %s:%d como %s... ",
                MQTT_BROKER.toString().c_str(), MQTT_PORT, mqttClientId.c_str());
  if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD))
  {
    Serial.println("conectado");
    mqttClient.subscribe(MQTT_TOPIC_BROADCAST);
  }
  else
  {
    Serial.printf("fallo, rc=%d\n", mqttClient.state());
  }
}

void PublishReadings(uint32_t nodeId, const String& json)
{
  if (!mqttClient.connected()) return;
  String topic = String(MQTT_TOPIC_BASE) + "/" + String(nodeId) + "/lecturas";
  if (!mqttClient.publish(topic.c_str(), json.c_str()))
  {
    Serial.println("MQTT: publish fallido");
  }
}
