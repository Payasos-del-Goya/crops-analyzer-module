#include "painlessMesh.h"
#include <Arduino_JSON.h>

#include "config.h"

#define LIGHT_SENSOR_PIN 34

Scheduler userScheduler;
painlessMesh mesh;
uint32_t seq = 0;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

int readLight() {
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  float lux = analogValue * (286 / 1150.0);
  return lux;
}

String getReadings()
{
  JSONVar reading;
  reading["node"] = String(mesh.getNodeId());
  reading["seq"] = (double)seq++;
  reading["temp_aire"] = 1;
  reading["hr"] = 2;
  reading["conductividad"] = 3;
  reading["luz_lux"] = readLight();
  reading["simulado"] = true;
  return JSON.stringify(reading);
}

void sendMessage()
{
  String message = getReadings();
  Serial.println("Envio: " + message);
  mesh.sendBroadcast(message);
  taskSendMessage.setInterval(random(TASK_SECOND * 3, TASK_SECOND * 7));
}

void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());
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

  analogSetAttenuation(ADC_11db);

  Serial.printf("\nBaliza -> rol: NODO, nodeId = %u\n", mesh.getNodeId());

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop()
{
  mesh.update();
}
