// Nodo de prueba: sin sensores. Envía suelo y atmósfera SIMULADOS (marcados como tales)
// y su estado real (RSSI, memoria...). Formato: FORMATO_DATOS.md
#include "painlessMesh.h"
#include <WiFi.h>

#include "comun.h"

Scheduler userScheduler;
painlessMesh mesh;

void enviarLecturas();
void enviarEstado();
void logPeriodico();
Task tLecturas(PERIODO_LECTURAS_MS, TASK_FOREVER, &enviarLecturas);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);

static float aleatorio(float minimo, float maximo)   // random() usa el generador hardware del ESP32
{
  return minimo + (maximo - minimo) * (random(0, 10001) / 10000.0f);
}

void enviarLecturas()
{
  char json[TAM_JSON];

  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"zona\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"simulado\",\"profundidad_cm\":20,"
    "\"humedad_vol\":%.1f,\"conductividad\":%.2f,\"temperatura\":%.1f}",
    FINCA, ZONA, g_nombre, aleatorio(15, 35), aleatorio(0.2, 1.5), aleatorio(12, 22));
  enviarAlPuente(mesh, "suelo", json);

  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%.1f,\"hr\":%.1f,\"temp_helada\":%.1f,"
    "\"temp_cubierta\":%.1f,\"lluvia_mm\":0.0,\"hoja_mojada_min\":0,\"luz_lux\":%.0f,\"simulado\":true}",
    FINCA, g_nombre, aleatorio(10, 28), aleatorio(40, 90), aleatorio(0, 10),
    aleatorio(10, 28), aleatorio(0, 60000));
  enviarAlPuente(mesh, "atmosfera", json);
}

void enviarEstado()
{
  char json[TAM_JSON];
  int n = jsonEstado(json, sizeof(json), "baliza", WiFi.RSSI(), 0);   // 0 sensores reales
  if (n > 0 && n < (int)sizeof(json)) enviarAlPuente(mesh, "estado", json);
}

void logPeriodico()
{
  char extra[64];
  snprintf(extra, sizeof(extra), "puente=%u nodos=%u", g_puenteId, (unsigned)mesh.getNodeList().size());
  logMemoria(extra);
}

void receivedCallback(uint32_t from, String& msg)
{
  if (procesarAnuncioPuente(from, msg)) return;
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());   // p. ej. lo enviado a fieldshift/c001/to/broadcast
}

void setup()
{
  Serial.begin(115200);
  identificarPlaca();
  Serial.printf("\nBaliza %s (%s) -> rol: NODO DE PRUEBA (datos simulados)\n", g_nombre, g_mac);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(tLecturas); tLecturas.enableDelayed(10000);
  userScheduler.addTask(tEstado);   tEstado.enableDelayed(15000);
  userScheduler.addTask(tLog);      tLog.enable();

  iniciarWatchdog();
}

void loop()
{
  esp_task_wdt_reset();
  mesh.update();
}
