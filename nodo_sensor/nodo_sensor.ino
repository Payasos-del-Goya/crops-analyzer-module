// Nodo de prueba (b03). Formato: FORMATO_DATOS.md
//   - Luz: sensor analógico REAL en el pin 34 -> atmósfera, mensaje propio SIN "simulado"
//   - Suelo y el resto de la atmósfera: SIMULADOS (marcados), hasta que haya sensores
//   - Estado real (RSSI, memoria...), sensores_ok = 1 (el de luz)
#include "painlessMesh.h"
#include <WiFi.h>
#include <DHT11.h>

#include "comun.h"

// Sensor de luz analógico. Pin 34 = ADC1: funciona con la WiFi activa (el ADC2 no).
#define LIGHT_SENSOR_PIN 34
#define DHT_SENSOR_PIN 21
#define LUX_POR_CUENTA   (286 / 1150.0f)   // calibración aproximada (lineal) del sensor

#define HUMEDAD_SENSOR_PIN 35

#define VOLTAJE_SECO_MV    2500.0f
#define VOLTAJE_MOJADO_MV  200.0f

DHT11 dht11(DHT_SENSOR_PIN);

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


static int leerLuz()
{
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  return analogValue * LUX_POR_CUENTA;
}

static int leerHumedadTierra()
{
  int analogValue = analogRead(HUMEDAD_SENSOR_PIN);
  int moisturePercentage = map(analogValue, VOLTAJE_SECO_MV, VOLTAJE_MOJADO_MV, 0, 100);
  return moisturePercentage;
}

static int leerHumedadAire()
{
  return dht11.readHumidity();
}

static int leerTemperatura()
{
  return dht11.readTemperature();
}

void enviarLecturas()
{
  char json[TAM_JSON];

  ultimaTemperatura = leerTemperatura();
  ultimaLuz = leerLuz();
  ultimaHumedadAire = leerHumedadAire();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%d,\"luz_lux\":%d,\"hum_aire\":%d}",
    FINCA, g_nombre, ultimaTemperatura, ultimaLuz, ultimaHumedadAire);
  Serial.println(json);
  enviarAlPuente(mesh, "atmosfera", json);

  ultimaHumedad = leerHumedadTierra();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"humedad_vol\":%d}",
    FINCA, g_nombre, ultimaHumedad);
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
  snprintf(extra, sizeof(extra), "puente=%s nodos=%u enviados=%lu | luz %d lux | humedad tierra %d | humedad aire %d | temperatura aire %d",
           g_puenteId ? "ok" : "NO", (unsigned)mesh.getNodeList().size(), (unsigned long)g_seq, ultimaLuz, ultimaHumedad, ultimaHumedadAire, ultimaTemperatura);
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
