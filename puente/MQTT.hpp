#pragma once
// Cliente MQTT del puente. Sin String ni JSON dinámico: búferes fijos.

WiFiClient espClient;
PubSubClient mqttClient(espClient);
static char mqttClientId[32];
static unsigned long lastMqttAttempt = 0;
static unsigned long ultimoMqttOk = 0;     // última vez que la conexión MQTT estaba viva

// Contadores para el log
static uint32_t nPublicados = 0, nFallos = 0, nDescartados = 0;

void OnMqttReceived(char* topic, byte* payload, unsigned int length);

void InitMqtt()
{
  snprintf(mqttClientId, sizeof(mqttClientId), "baliza-puente-%s", g_nombre);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
  mqttClient.setSocketTimeout(2);
  mqttClient.setCallback(OnMqttReceived);
  ultimoMqttOk = millis();
}

// Un intento como mucho cada MQTT_RECONNECT_INTERVAL, para no bloquear la malla
void HandleMqtt(bool hasIp)
{
  if (mqttClient.connected())
  {
    mqttClient.loop();
    ultimoMqttOk = millis();
    return;
  }
  if (!hasIp) return;

  unsigned long now = millis();
  if (lastMqttAttempt != 0 && now - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) return;
  lastMqttAttempt = now;

  Serial.printf("MQTT: conectando como %s... ", mqttClientId);
  if (mqttClient.connect(mqttClientId, MQTT_USER, MQTT_PASSWORD))
  {
    Serial.println("conectado");
    mqttClient.subscribe(MQTT_TOPIC_BROADCAST);
    ultimoMqttOk = millis();
  }
  else
  {
    Serial.printf("fallo, rc=%d\n", mqttClient.state());
  }
}

// Publica en fieldshift/c001/<dispositivo>/<tipo>. En "estado" añade "gateway" (el nombre del puente).
void Publicar(const char* tipo, const char* dispositivo, const char* json, size_t len)
{
  static char topic[64];
  static char payload[TAM_JSON + 40];

  if (!mqttClient.connected()) { nDescartados++; return; }
  snprintf(topic, sizeof(topic), "%s/%s/%s", MQTT_TOPIC_BASE, dispositivo, tipo);

  const char* p = json;
  if (strcmp(tipo, "estado") == 0)
  {
    if (len < 2 || json[len - 1] != '}') { nDescartados++; return; }
    int n = snprintf(payload, sizeof(payload), "%.*s,\"gateway\":\"%s\"}", (int)(len - 1), json, g_nombre);
    if (n <= 0 || n >= (int)sizeof(payload)) { nDescartados++; return; }
    p = payload;
  }
  if (mqttClient.publish(topic, p)) nPublicados++;
  else { nFallos++; Serial.printf("MQTT: publish fallido en %s\n", topic); }
}
