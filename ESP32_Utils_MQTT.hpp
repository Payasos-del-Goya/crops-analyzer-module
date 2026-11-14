const char* MQTT_USER = "baliza";
const char* MQTT_PASSWORD = "pitochico";
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
unsigned long lastMqttAttempt = 0;

void InitMqtt() 
{
  mqttClient.setServer(MQTT_BROKER_ADRESS, MQTT_PORT);
  mqttClient.setSocketTimeout(1);
  mqttClient.setCallback(OnMqttReceived);
}

bool ConnectMqtt()
{
  if (mqttClient.connected())
  {
    return true;
  }

  unsigned long now = millis();
  if (lastMqttAttempt != 0 &&
      now - lastMqttAttempt < MQTT_RECONNECT_INTERVAL)
  {
    return false;
  }

  lastMqttAttempt = now;
  Serial.print("Starting MQTT connection...");
  if (mqttClient.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD))
  {
    Serial.println(" connected");
    SuscribeMqtt();
    return true;
  }

  Serial.print(" failed, rc=");
  Serial.println(mqttClient.state());
  return false;
}

void HandleMqtt()
{
  if (ConnectMqtt())
  {
    mqttClient.loop();
  }
}