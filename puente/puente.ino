#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "MQTT.hpp"

Scheduler userScheduler;
painlessMesh mesh;

IPAddress stationIP(0, 0, 0, 0);

void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());
  if (msg.startsWith("{"))
  {
    Serial.println("enviando a pi");
    PublishReadings(from, msg);
  }
}

void OnMqttReceived(char* topic, byte* payload, unsigned int length)
{
  String content;
  content.reserve(length);
  for (unsigned int i = 0; i < length; i++)
  {
    content.concat((char)payload[i]);
  }
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
  Serial.println("\nBaliza -> rol: PUENTE");

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(BRIDGE_HOSTNAME);
  mesh.setRoot(true);
  mesh.setContainsRoot(true);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("WiFi STA desconectada, motivo=%u\n", info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  InitMqtt(mesh.getNodeId());
}

void loop()
{
  mesh.update();

  IPAddress ip(mesh.getStationIP());
  if (ip != stationIP)
  {
    stationIP = ip;
    Serial.printf("WiFi %s: IP %s\n", STATION_SSID, ip.toString().c_str());
  }
  HandleMqtt(ip != IPAddress(0, 0, 0, 0));
}
