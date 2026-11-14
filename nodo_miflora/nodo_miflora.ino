#include "painlessMesh.h"
#include <Arduino_JSON.h>

#include "config.h"
#include "miflora.hpp"

Scheduler userScheduler;
painlessMesh mesh;
uint32_t seq = 0;
datosMiflora latestData = {0.0f, 0, 0, 0};
portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

void sensorTask(void *parameter)
{
  vTaskDelay(pdMS_TO_TICKS(10000));
  while (true)
  {
    datosMiflora newData = leerSensor();
    portENTER_CRITICAL(&sensorMux);
    latestData = newData;
    portEXIT_CRITICAL(&sensorMux);
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

String getReadings()
{
  datosMiflora datos;
  portENTER_CRITICAL(&sensorMux);
  datos = latestData;
  portEXIT_CRITICAL(&sensorMux);
  JSONVar reading;
  reading["node"] = String(mesh.getNodeId());
  reading["seq"] = (double)seq++;
  reading["temp_aire"] = datos.tempAire;
  reading["hr"] = datos.hr;
  reading["conductividad"] = datos.conductividad;
  reading["luz_lux"] = datos.luz_lux;
  reading["simulado"] = false;
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
  Serial.printf("\nBaliza -> rol: NODO MIFLORA, nodeId = %u\n", mesh.getNodeId());

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
  xTaskCreatePinnedToCore(sensorTask, "MiFlora", 8192, nullptr, 1, nullptr, 0);
}

void loop()
{
  mesh.update();
}
