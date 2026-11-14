#pragma once
// ============================================================================
// comun.h — COPIA IDÉNTICA en puente/, nodo_miflora/ y nodo_sensor/.
// Arduino IDE solo compila los archivos de la carpeta del sketch, así que si
// cambias algo aquí, cópialo a las tres carpetas.
//
// Formato de los datos: ver FORMATO_DATOS.md en la raíz del repositorio.
// ============================================================================
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "painlessMesh.h"

// ---- Malla painlessMesh (idéntico en todas las placas) ----
#define MESH_PREFIX     "MyMeshNetwork"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MESH_CHANNEL    1        // el mismo canal que la WiFi fieldshift de la Raspberry

// ---- Datos ----
#define FINCA                    "demo"
#define ZONA                     "A"
#define PERIODO_LECTURAS_MS      30000UL   // suelo + atmósfera
#define PERIODO_ESTADO_MS        60000UL   // estado de la placa
#define PERIODO_ANUNCIO_MS       10000UL   // el puente anuncia quién es
#define PERIODO_LOG_MS           10000UL   // línea de memoria por el puerto serie
#define WDT_TIMEOUT_S            30        // si loop() se atasca 30 s, la placa se reinicia

// Nombre corto de cada placa según su MAC (se usa como "dispositivo" en la base de datos)
struct Dispositivo { const char* mac; const char* nombre; };
static const Dispositivo DISPOSITIVOS[] = {
  {"E4:65:B8:54:34:D4", "b01"},   // ESP32 blanca: puente
  {"E0:5A:1B:A7:8B:10", "b02"},   // nodo MiFlora
  {"E0:5A:1B:A6:FD:A8", "b03"},   // nodo de prueba (datos simulados)
  {"E0:5A:1B:A7:F8:F9", "b04"},
};

// Mensajes por la malla (texto plano, sin librería JSON):
//   lecturas/estado:   "<tipo>|<dispositivo>|<json>"   tipo = suelo | atmosfera | estado
//   anuncio del puente: "P|<dispositivo>"
#define TAM_JSON 320

static char g_mac[18];
static char g_nombre[8];
static uint32_t g_seq = 0;      // mensajes enviados por esta placa desde que arrancó

// Rellena g_mac y g_nombre. Una placa que no esté en la tabla se llama "x" + 3 últimos bytes de la MAC.
inline void identificarPlaca()
{
  uint64_t mac = ESP.getEfuseMac();   // byte 0 de la MAC en los bits bajos
  snprintf(g_mac, sizeof(g_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  for (const auto& d : DISPOSITIVOS)
  {
    if (strcasecmp(d.mac, g_mac) == 0)
    {
      strlcpy(g_nombre, d.nombre, sizeof(g_nombre));
      return;
    }
  }
  snprintf(g_nombre, sizeof(g_nombre), "x%02X%02X%02X",
           (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
}

// Watchdog de la tarea loop(): compatible con el núcleo ESP32 2.x y 3.x
inline void iniciarWatchdog()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WDT_TIMEOUT_S * 1000;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
}

// JSON del mensaje de estado (el puente añade "gateway" al publicarlo)
inline int jsonEstado(char* buf, size_t n, const char* tipo, int rssi, int sensoresOk)
{
  return snprintf(buf, n,
    "{\"finca\":\"%s\",\"dispositivo\":\"%s\",\"tipo\":\"%s\",\"rssi\":%d,\"seq\":%lu,"
    "\"sensores_ok\":%d,\"heap_libre\":%lu,\"heap_bloque\":%lu,\"uptime_s\":%lu}",
    FINCA, g_nombre, tipo, rssi, (unsigned long)g_seq, sensoresOk,
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap(),
    (unsigned long)(millis() / 1000));
}

// Línea periódica por el puerto serie para vigilar la memoria
inline void logMemoria(const char* extra)
{
  Serial.printf("[%lus] %s heap libre=%u min=%u bloque=%u %s\n",
                (unsigned long)(millis() / 1000), g_nombre,
                ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), extra);
}

// ---- Solo nodos: envío al puente ----
static uint32_t g_puenteId = 0;        // nodeId del puente (lo aprendemos de su anuncio)

// Procesa un anuncio "P|<nombre>" del puente. Devuelve true si el mensaje era un anuncio.
inline bool procesarAnuncioPuente(uint32_t from, const String& msg)
{
  if (!msg.startsWith("P|")) return false;
  if (g_puenteId != from) Serial.printf("Puente: %s (nodeId %u)\n", msg.c_str() + 2, from);
  g_puenteId = from;
  return true;
}

// Envía "<tipo>|<dispositivo>|<json>" directamente al puente (no a toda la malla)
inline void enviarAlPuente(painlessMesh& mesh, const char* tipo, const char* json)
{
  char msg[TAM_JSON + 24];
  int n = snprintf(msg, sizeof(msg), "%s|%s|%s", tipo, g_nombre, json);
  if (n <= 0 || n >= (int)sizeof(msg))
  {
    Serial.println("ERROR: mensaje demasiado largo, no se envía");
    return;
  }
  String s(msg);
  bool ok = g_puenteId != 0 && mesh.sendSingle(g_puenteId, s);
  if (!ok) mesh.sendBroadcast(s);   // aún no sabemos quién es el puente (o ha cambiado)
  g_seq++;
}
