// Nodo de prueba. Formato: FORMATO_DATOS.md
//   - Luz: sensor analógico REAL en el pin 34 -> atmósfera, mensaje propio SIN "simulado"
//   - Suelo y el resto de la atmósfera: SIMULADOS (marcados), hasta que haya sensores
//   - Estado real (RSSI, memoria...), sensores_ok = 1 (el de luz)
#include "painlessMesh.h"
#include <WiFi.h>

#include "comun.h"
#include "esp_random.h"

Scheduler userScheduler;
painlessMesh mesh;

void enviarLecturas();
void enviarEstado();
void logPeriodico();
Task tLecturas(PERIODO_LECTURAS_MS, TASK_FOREVER, &enviarLecturas);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);

static int ultimaLuz = -1;
static int ultimaHumedad = -1;
static int ultimaTemperatura = -1;
static int ultimaHumedadAire = -1;
static double ultimoPh = -1;

float random_01(float min, float max) {
    int a = min * 10;
    int b = max * 10;
    return (a + (esp_random() % (b - a + 1))) / 10.0f;
}

int random_int(int min, int max) {
    return min + (esp_random() % (max - min + 1));
}

static float leerPh()
{
  return random_01(6.0, 8.0);
}

static int leerLuz()
{
  return random_int(500, 550);
}

static int leerHumedadTierra()
{
  return random_int(33, 40);
}

static int leerHumedadAire()
{
  return random_int(30, 35);
}

static int leerTemperatura()
{
  return random_int(20, 25);
}

void enviarLecturas()
{
  char json[TAM_JSON];

  ultimaTemperatura = leerTemperatura();
  ultimaLuz = leerLuz();
  ultimaHumedadAire = leerHumedadAire();
  ultimoPh = leerPh();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%d,\"luz_lux\":%d,\"hr\":%d}",
    FINCA, g_nombre, ultimaTemperatura, ultimaLuz, ultimaHumedadAire);
  Serial.println(json);
  enviarAlPuente(mesh, "atmosfera", json);

  ultimaHumedad = leerHumedadTierra();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"fc28\",\"profundidad_cm\":%d,\"humedad_vol\":%d,\"ph\":%.1f}",
    FINCA, g_nombre, 5, ultimaHumedad, ultimoPh);
  enviarAlPuente(mesh, "suelo", json);

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
  snprintf(extra, sizeof(extra), "puente=%s nodos=%u enviados=%lu | ph %.1f",
           g_puenteId ? "ok" : "NO", (unsigned)mesh.getNodeList().size(), (unsigned long)g_seq, ultimoPh);
  logMemoria(extra);
}

void receivedCallback(uint32_t from, String& msg)
{
  if (procesarAnuncioPuente(from, msg)) return;
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());   // p. ej. lo enviado a plot-advisor/c001/to/broadcast
}

void setup()
{
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // rango completo 0-3,3 V en el pin del sensor de luz
  identificarPlaca();

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
  vigilarPuente(mesh);
}
