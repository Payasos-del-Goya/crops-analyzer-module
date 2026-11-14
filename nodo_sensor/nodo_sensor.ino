// Nodo de prueba (b03). Formato: FORMATO_DATOS.md
//   - Luz: sensor analógico REAL en el pin 34 -> atmósfera, mensaje propio SIN "simulado"
//   - Suelo y el resto de la atmósfera: SIMULADOS (marcados), hasta que haya sensores
//   - Estado real (RSSI, memoria...), sensores_ok = 1 (el de luz)
#include "painlessMesh.h"
#include <WiFi.h>

#include "comun.h"

// Sensor de luz analógico. Pin 34 = ADC1: funciona con la WiFi activa (el ADC2 no).
#define LIGHT_SENSOR_PIN 34
#define LUX_POR_CUENTA   (286 / 1150.0f)   // calibración aproximada (lineal) del sensor

Scheduler userScheduler;
painlessMesh mesh;

void enviarLecturas();
void enviarEstado();
void logPeriodico();
Task tLecturas(PERIODO_LECTURAS_MS, TASK_FOREVER, &enviarLecturas);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);

static float ultimaLuz = -1;

static float leerLuz()
{
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  return analogValue * LUX_POR_CUENTA;
}

static float aleatorio(float minimo, float maximo)   // random() usa el generador hardware del ESP32
{
  return minimo + (maximo - minimo) * (random(0, 10001) / 10000.0f);
}

void enviarLecturas()
{
  char json[TAM_JSON];

  // Luz: dato real, en su propio mensaje (sin "simulado")
  ultimaLuz = leerLuz();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"luz_lux\":%.0f}",
    FINCA, g_nombre, ultimaLuz);
  enviarAlPuente(mesh, "atmosfera", json);

  // Resto: simulado y marcado
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"zona\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"simulado\",\"profundidad_cm\":20,"
    "\"humedad_vol\":%.1f,\"conductividad\":%.2f,\"temperatura\":%.1f}",
    FINCA, ZONA, g_nombre, aleatorio(15, 35), aleatorio(0.2, 1.5), aleatorio(12, 22));
  enviarAlPuente(mesh, "suelo", json);

  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%.1f,\"hr\":%.1f,\"temp_helada\":%.1f,"
    "\"temp_cubierta\":%.1f,\"lluvia_mm\":0.0,\"hoja_mojada_min\":0,\"simulado\":true}",
    FINCA, g_nombre, aleatorio(10, 28), aleatorio(40, 90), aleatorio(0, 10), aleatorio(10, 28));
  enviarAlPuente(mesh, "atmosfera", json);
}

void enviarEstado()
{
  char json[TAM_JSON];
  int n = jsonEstado(json, sizeof(json), "baliza", WiFi.RSSI(), 1);   // 1 sensor real: luz
  if (n > 0 && n < (int)sizeof(json)) enviarAlPuente(mesh, "estado", json);
}

void logPeriodico()
{
  char extra[96];
  snprintf(extra, sizeof(extra), "puente=%s nodos=%u enviados=%lu | luz %.0f lux (resto simulado)",
           g_puenteId ? "ok" : "NO", (unsigned)mesh.getNodeList().size(), (unsigned long)g_seq, ultimaLuz);
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
  analogSetAttenuation(ADC_11db);   // rango completo 0-3,3 V en el pin del sensor de luz
  identificarPlaca();
  Serial.printf("\nBaliza %s (%s) -> rol: NODO DE PRUEBA (luz real, resto simulado)\n", g_nombre, g_mac);

  mesh.setDebugMsgTypes(MESH_DEBUG);
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
