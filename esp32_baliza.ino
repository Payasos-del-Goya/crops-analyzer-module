#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <PubSubClient.h>

#include "mqtt_config.h"
#include "MQTT.hpp"
#include "ESP32_Utils.hpp"
#include "ESP32_Utils_MQTT.hpp"

#define   MESH_PREFIX     "MyMeshNetwork"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

int nodeNumber = 1;
const int MQTT_NODE_NUMBER = 1;

String readings;

// User stub
void sendMessage(); // Prototype so PlatformIO doesn't complain
String getReadings();

Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);

int random_read(int min, int max) {
  return rand() % (max - min + 1) + min;
}

String getReadings() {
  JSONVar jsonReadings;
  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = random_read(0, 50);
  jsonReadings["hum"] = random_read(0, 100);
  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage() {
  String msg = getReadings();
  msg += mesh.getNodeId();
  mesh.sendBroadcast(msg);
  taskSendMessage.setInterval(random(TASK_SECOND * 1, TASK_SECOND * 5));
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  if (nodeNumber == MQTT_NODE_NUMBER) {
    PublisMqtt(msg.c_str());
  }
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
  Serial.begin(115200);

  // 1. Formatear si falla el montaje (o eliminar si no se usa)
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
  }

  // 2. Configurar debug e inicializar Mesh (indicando el canal de tu router, ej. 6)
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, 1);
  
  // 3. Conectar el nodo puente al router a través de painlessMesh
  mesh.stationManual("fieldshift", "somethingSneaky");
  mesh.setHostname("ESP32_Mesh_Bridge");

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  // 4. Inicializar configuración del servidor MQTT
  if (nodeNumber == MQTT_NODE_NUMBER) {
    InitMqtt();
  }

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
  
  // Mantener viva la conexión MQTT solo cuando el ESP32 tenga IP del router
  if (nodeNumber == MQTT_NODE_NUMBER && WiFi.status() == WL_CONNECTED) {
    HandleMqtt();
  }
}
