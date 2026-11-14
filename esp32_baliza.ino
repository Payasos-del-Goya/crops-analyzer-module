#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "MQTT.hpp"

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

bool isBridge = false;          // se decide en setup() comparando la MAC con BRIDGE_MAC
uint32_t seq = 0;               // contador de mensajes de este nodo
IPAddress stationIP(0, 0, 0, 0);

void sendMessage();
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

String macString()
{
  uint64_t mac = ESP.getEfuseMac();   // byte 0 de la MAC en los bits bajos
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  return String(buf);
}

// Lecturas en JSON válido, con el id del nodo DENTRO del JSON.
String getReadings()
{
  JSONVar j;
  j["node"] = String(mesh.getNodeId());
  j["seq"] = (double)seq++;
  j["temp"] = (int)random(0, 51);      // random() usa el generador hardware del ESP32
  j["hum"] = (int)random(0, 101);
  j["simulado"] = READINGS_SIMULATED;
  return JSON.stringify(j);
}

void sendMessage()
{
  String msg = getReadings();
  if (isBridge)
  {
    PublishReadings(mesh.getNodeId(), msg);   // sendBroadcast no se entrega a uno mismo
  }
  mesh.sendBroadcast(msg);
  taskSendMessage.setInterval(random(TASK_SECOND * 3, TASK_SECOND * 7));
}

// Mensajes recibidos por la malla
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());
  if (isBridge && msg.startsWith("{"))
  {
    PublishReadings(from, msg);
  }
}

// Mensajes MQTT recibidos por el puente: lo publicado en MQTT_TOPIC_BROADCAST se reenvía a la malla
void OnMqttReceived(char* topic, byte* payload, unsigned int length)
{
  String content;
  content.reserve(length);
  for (unsigned int i = 0; i < length; i++) content.concat((char)payload[i]);
  Serial.printf("MQTT: %s: %s\n", topic, content.c_str());
  if (String(topic) == MQTT_TOPIC_BROADCAST)
  {
    mesh.sendBroadcast(content);
  }
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("Malla: nueva conexión, nodeId = %u\n", nodeId);
}

void changedConnectionCallback()
{
  Serial.printf("Malla: conexiones cambiadas, nodos = %u\n", mesh.getNodeList().size());
}

void setup()
{
  Serial.begin(115200);

  String mac = macString();
  isBridge = mac.equalsIgnoreCase(BRIDGE_MAC);
  Serial.printf("\nBaliza %s -> rol: %s\n", mac.c_str(), isBridge ? "PUENTE" : "nodo");

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);

  if (isBridge)
  {
    // Solo el puente reserva su interfaz STA para la WiFi de la Raspberry Pi.
    // Los demás nodos la usan para engancharse a otros nodos y formar la malla.
    mesh.stationManual(STATION_SSID, STATION_PASSWORD);
    mesh.setHostname(BRIDGE_HOSTNAME);
    mesh.setRoot(true);

    // Muestra el motivo de cada desconexión de la WiFi fieldshift (diagnóstico)
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.printf("WiFi STA desconectada, motivo=%u\n", info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    InitMqtt(mesh.getNodeId());
  }
  mesh.setContainsRoot(true);   // todos saben que la malla tiene un puente

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop()
{
  mesh.update();

  if (isBridge)
  {
    IPAddress ip(mesh.getStationIP());
    if (ip != stationIP)
    {
      stationIP = ip;
      Serial.printf("WiFi %s: IP %s\n", STATION_SSID, ip.toString().c_str());
    }
    // Se llama SIEMPRE (no solo con WiFi.status()==WL_CONNECTED) para mantener vivo el keepalive
    HandleMqtt(ip != IPAddress(0, 0, 0, 0));
  }
}
