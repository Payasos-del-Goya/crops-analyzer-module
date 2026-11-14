// Nodo con sensor Xiaomi MiFlora (datos REALES). Formato: FORMATO_DATOS.md
//   suelo:     humedad_vol (%), conductividad µS/cm, tipo "miflora"
//   atmósfera: temp_aire (°C), luz_lux
//   estado:    sensores_ok = 1 si hay una lectura reciente del MiFlora, 0 si no
#include "painlessMesh.h"
#include <WiFi.h>

#include "comun.h"
#include "config.h"
#include "miflora.hpp"

Scheduler userScheduler;
painlessMesh mesh;

// Última lectura válida (la escribe la tarea del sensor, la lee loop())
static LecturaMiFlora ultima;
static unsigned long ultimaMs = 0;     // 0 = nunca ha habido lectura válida
static portMUX_TYPE muxSensor = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t nFallosMiFlora = 0;

void enviarLecturas();
void enviarEstado();
void logPeriodico();
Task tLecturas(PERIODO_LECTURAS_MS, TASK_FOREVER, &enviarLecturas);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);

// Tarea aparte (núcleo 0): la conexión BLE tarda segundos y no debe bloquear la malla
void tareaSensor(void*)
{
  vTaskDelay(pdMS_TO_TICKS(10000));   // dejar que la malla arranque primero
  bool estabaOk = false, primera = true;
  for (;;)
  {
    LecturaMiFlora l;
    const char* error = nullptr;
    if (leerMiFlora(l, error))
    {
      portENTER_CRITICAL(&muxSensor);
      ultima = l;
      ultimaMs = millis();
      portEXIT_CRITICAL(&muxSensor);
      if (!estabaOk)   // solo se avisa al recuperar la lectura; los valores van en el resumen periódico
        Serial.printf("MiFlora: leyendo OK (%.1f °C, %u %%, %u µS/cm, %lu lux)\n",
                      l.temperatura, l.humedad, l.conductividad, (unsigned long)l.luz_lux);
      estabaOk = true;
    }
    else
    {
      nFallosMiFlora++;
      if (estabaOk || primera)   // solo el primer fallo de cada racha
        Serial.printf("MiFlora: fallo (%s); no se envían datos de campo hasta que vuelva\n", error);
      estabaOk = false;
    }
    primera = false;
    vTaskDelay(pdMS_TO_TICKS(MIFLORA_PERIODO_MS));
  }
}

// Devuelve true si hay una lectura reciente y la copia en l
static bool lecturaReciente(LecturaMiFlora& l)
{
  portENTER_CRITICAL(&muxSensor);
  l = ultima;
  unsigned long t = ultimaMs;
  portEXIT_CRITICAL(&muxSensor);
  return t != 0 && millis() - t <= MIFLORA_MAX_EDAD_MS;
}

void enviarLecturas()
{
  LecturaMiFlora l;
  if (!lecturaReciente(l)) return;   // el aviso lo da la tarea del sensor al fallar
  char json[TAM_JSON];

  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"zona\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"miflora\",\"profundidad_cm\":%d,"
    "\"humedad_vol\":%u,\"conductividad\":%d}",
    FINCA, ZONA, g_nombre, MIFLORA_PROFUNDIDAD_CM, l.humedad, l.conductividad);
  enviarAlPuente(mesh, "suelo", json);

  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%.1f,\"luz_lux\":%lu}",
    FINCA, g_nombre, l.temperatura, (unsigned long)l.luz_lux);
  enviarAlPuente(mesh, "atmosfera", json);
}

void enviarEstado()
{
  LecturaMiFlora l;
  char json[TAM_JSON];
  int n = jsonEstado(json, sizeof(json), "baliza", WiFi.RSSI(), lecturaReciente(l) ? 1 : 0);
  if (n > 0 && n < (int)sizeof(json)) enviarAlPuente(mesh, "estado", json);
}

void logPeriodico()
{
  LecturaMiFlora l;
  bool ok = lecturaReciente(l);
  char extra[128];
  if (ok)
    snprintf(extra, sizeof(extra), "puente=%s nodos=%u enviados=%lu | miflora %.1f°C %u%% %uµS/cm %lulux fallos=%lu",
             g_puenteId ? "ok" : "NO", (unsigned)mesh.getNodeList().size(), (unsigned long)g_seq,
             l.temperatura, l.humedad, l.conductividad, (unsigned long)l.luz_lux, (unsigned long)nFallosMiFlora);
  else
    snprintf(extra, sizeof(extra), "puente=%s nodos=%u enviados=%lu | miflora SIN DATOS fallos=%lu",
             g_puenteId ? "ok" : "NO", (unsigned)mesh.getNodeList().size(), (unsigned long)g_seq,
             (unsigned long)nFallosMiFlora);
  logMemoria(extra);
}

void receivedCallback(uint32_t from, String& msg)
{
  if (procesarAnuncioPuente(from, msg)) return;
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());
}

void setup()
{
  Serial.begin(115200);
  identificarPlaca();
  Serial.printf("\nBaliza %s (%s) -> rol: NODO MIFLORA (%s)\n", g_nombre, g_mac, MIFLORA_MAC);

  mesh.setDebugMsgTypes(MESH_DEBUG);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(tLecturas); tLecturas.enableDelayed(20000);
  userScheduler.addTask(tEstado);   tEstado.enableDelayed(15000);
  userScheduler.addTask(tLog);      tLog.enable();

  xTaskCreatePinnedToCore(tareaSensor, "MiFlora", 8192, nullptr, 1, nullptr, 0);
  iniciarWatchdog();
}

void loop()
{
  esp_task_wdt_reset();
  mesh.update();
}
