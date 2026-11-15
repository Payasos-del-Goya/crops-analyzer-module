// Nodo de prueba. Formato: FORMATO_DATOS.md
//   - Luz: sensor analógico REAL en el pin 34 -> atmósfera, mensaje propio SIN "simulado"
//   - Suelo y el resto de la atmósfera: SIMULADOS (marcados), hasta que haya sensores
//   - Estado real (RSSI, memoria...), sensores_ok = 1 (el de luz)
#include "painlessMesh.h"
#include <WiFi.h>
#include <DHT11.h>
#include "esp_random.h"
#include "esp32-hal-cpu.h"
#include "esp_chip_info.h"
#include "esp_wifi.h"

#include "comun.h"

// Sensor de luz analógico. Pin 34 = ADC1: funciona con la WiFi activa (el ADC2 no).
#define LIGHT_SENSOR_PIN 34
#define DHT_SENSOR_PIN 21
#define LUX_POR_CUENTA   (286 / 1150.0f)   // calibración aproximada (lineal) del sensor

#define HUMEDAD_SENSOR_PIN 35

#define VOLTAJE_SECO_MV    2500.0f
#define VOLTAJE_MOJADO_MV  200.0f
#define PROFUNDIDAD_FC28_CM 5   // AJUSTAR: cm a los que está clavado el FC-28
#define CPU_FREQUENCY_MHZ  80   // Frecuencia objetivo: 80, 160 o 240 MHz
#define RF_EVENTO_ESTIMADO_MS 10UL
#define RF_INFORME_MS         60000UL

DHT11 dht11(DHT_SENSOR_PIN);

Scheduler userScheduler;
painlessMesh mesh;

void enviarLecturas();
void enviarEstado();
void logPeriodico();
void medirActividadRf();
Task tLecturas(PERIODO_LECTURAS_MS, TASK_FOREVER, &enviarLecturas);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);
Task tRf(RF_INFORME_MS, TASK_FOREVER, &medirActividadRf);

static int ultimaLuz = -1;
static int ultimaHumedad = -1;
static int ultimaTemperatura = -1;
static int ultimaHumedadAire = -1;
static double ultimoPh = -1;
static uint32_t rfTxPaquetes = 0;
static uint32_t rfRxPaquetes = 0;
static uint64_t rfTxMs = 0;
static uint64_t rfRxMs = 0;
static uint32_t rfInicioMs = 0;

static void marcarRfTx()
{
  rfTxPaquetes++;
  rfTxMs += RF_EVENTO_ESTIMADO_MS;
}

static void marcarRfRx()
{
  rfRxPaquetes++;
  rfRxMs += RF_EVENTO_ESTIMADO_MS;
}

void medirActividadRf()
{
  uint32_t ahora = millis();
  uint64_t ventanaMs = ahora - rfInicioMs;
  uint64_t ocupadoMs = rfTxMs + rfRxMs;
  uint64_t reposoMs = ocupadoMs < ventanaMs ? ventanaMs - ocupadoMs : 0;
  Serial.printf("RF estimado: tx=%llu ms (%u paquetes), rx=%llu ms (%u paquetes), reposo=%llu ms, ventana=%llu ms\n",
                (unsigned long long)rfTxMs, rfTxPaquetes,
                (unsigned long long)rfRxMs, rfRxPaquetes,
                (unsigned long long)reposoMs, (unsigned long long)ventanaMs);
}

static void configurarCpu()
{
  bool configurada = setCpuFrequencyMhz(CPU_FREQUENCY_MHZ);
  if (!configurada)
  {
    Serial.printf("ERROR: no se pudo fijar la CPU a %u MHz\n", CPU_FREQUENCY_MHZ);
  }

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  Serial.printf("CPU inicio: %u MHz (objetivo %u MHz), %s-core, cores=%u\n",
                getCpuFrequencyMhz(), CPU_FREQUENCY_MHZ,
                chip.cores == 1 ? "single" : "dual", chip.cores);
}

static void imprimirModoRf()
{
  wifi_ps_type_t modo;
  esp_err_t resultado = esp_wifi_get_ps(&modo);
  if (resultado != ESP_OK)
  {
    Serial.printf("ERROR: no se pudo leer WiFi PS mode, codigo=%d\n", resultado);
    return;
  }

  const char* nombre = "DESCONOCIDO";
  if (modo == WIFI_PS_NONE) nombre = "NONE (RF activo)";
  else if (modo == WIFI_PS_MIN_MODEM) nombre = "MIN_MODEM (modem-sleep minimo)";
  else if (modo == WIFI_PS_MAX_MODEM) nombre = "MAX_MODEM (modem-sleep maximo)";
  Serial.printf("WiFi PS mode inicio: %s (%d)\n", nombre, modo);
}

float random_01(float min, float max) {
    int a = min * 10;
    int b = max * 10;
    return (a + (esp_random() % (b - a + 1))) / 10.0f;
}

static float leerPh()
{
  return random_01(6.0, 8.0);
}

static int leerLuz()
{
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  return analogValue * LUX_POR_CUENTA;
}

static int leerHumedadTierra()
{
  int analogValue = analogRead(HUMEDAD_SENSOR_PIN);
  int moisturePercentage = map(analogValue, VOLTAJE_SECO_MV, VOLTAJE_MOJADO_MV, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);   // map() se sale si la lectura pasa de los límites
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
  ultimoPh = leerPh();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"temp_aire\":%d,\"luz_lux\":%d,\"hr\":%d}",
    FINCA, g_nombre, ultimaTemperatura, ultimaLuz, ultimaHumedadAire);
  Serial.println(json);
  marcarRfTx();
  enviarAlPuente(mesh, "atmosfera", json);

  ultimaHumedad = leerHumedadTierra();
  snprintf(json, sizeof(json),
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"fc28\",\"profundidad_cm\":%d,\"humedad_vol\":%d,\"ph\":%.1f}",
    FINCA, g_nombre, PROFUNDIDAD_FC28_CM, ultimaHumedad, ultimoPh);
  marcarRfTx();
  enviarAlPuente(mesh, "suelo", json);

}

void enviarEstado()
{
  char json[TAM_JSON];
  int n = jsonEstado(json, sizeof(json), "baliza", WiFi.RSSI(), 1);   // 1 sensor real: luz
  if (n > 0 && n < (int)sizeof(json))
  {
    marcarRfTx();
    enviarAlPuente(mesh, "estado", json);
  }
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
  marcarRfRx();
  if (procesarAnuncioPuente(from, msg)) return;
  Serial.printf("Malla: de %u: %s\n", from, msg.c_str());   // p. ej. lo enviado a plot-advisor/c001/to/broadcast
}

void setup()
{
  Serial.begin(115200);
  configurarCpu();
  analogSetAttenuation(ADC_11db);   // rango completo 0-3,3 V en el pin del sensor de luz
  identificarPlaca();

  mesh.setDebugMsgTypes(MESH_DEBUG);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  imprimirModoRf();
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(tLecturas); tLecturas.enableDelayed(10000);
  userScheduler.addTask(tEstado);   tEstado.enableDelayed(15000);
  userScheduler.addTask(tLog);      tLog.enable();
  rfInicioMs = millis();
  userScheduler.addTask(tRf);       tRf.enableDelayed(RF_INFORME_MS);

  iniciarWatchdog();
}

void loop()
{
  esp_task_wdt_reset();
  mesh.update();
  vigilarPuente(mesh);
}
