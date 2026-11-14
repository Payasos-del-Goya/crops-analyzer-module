// Puente de la malla: recibe los mensajes de las balizas por painlessMesh y los publica por MQTT
// en la Raspberry Pi (WiFi fieldshift). Formato: FORMATO_DATOS.md
#include "painlessMesh.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include "comun.h"
#include "config.h"
#include "MQTT.hpp"

Scheduler userScheduler;
painlessMesh mesh;

static IPAddress stationIP(0, 0, 0, 0);
static unsigned long ultimoConIP = 0;

void anunciar();
void enviarEstado();
void logPeriodico();
Task tAnuncio(PERIODO_ANUNCIO_MS, TASK_FOREVER, &anunciar);
Task tEstado(PERIODO_ESTADO_MS, TASK_FOREVER, &enviarEstado);
Task tLog(PERIODO_LOG_MS, TASK_FOREVER, &logPeriodico);

static bool tipoValido(const char* t, size_t n)
{
  return (n == 5 && strncmp(t, "suelo", 5) == 0) ||
         (n == 9 && strncmp(t, "atmosfera", 9) == 0) ||
         (n == 6 && strncmp(t, "estado", 6) == 0);
}

// Mensajes de la malla: "<tipo>|<dispositivo>|<json>". Se valida la cabecera; el JSON se reenvía tal cual.
void receivedCallback(uint32_t from, String& msg)
{
  const char* m = msg.c_str();
  if (m[0] == 'P' && m[1] == '|') return;            // anuncio de otro puente (no debería haberlo)

  const char* p1 = strchr(m, '|');
  const char* p2 = p1 ? strchr(p1 + 1, '|') : nullptr;
  if (!p1 || !p2 || p2[1] != '{') { nDescartados++; return; }

  size_t nt = p1 - m, nd = p2 - p1 - 1;
  if (!tipoValido(m, nt) || nd == 0 || nd >= 8) { nDescartados++; return; }

  char tipo[10], disp[8];
  memcpy(tipo, m, nt); tipo[nt] = '\0';
  memcpy(disp, p1 + 1, nd); disp[nd] = '\0';
  for (size_t i = 0; i < nd; i++)
    if (!isalnum((unsigned char)disp[i])) { nDescartados++; return; }

  Publicar(tipo, disp, p2 + 1, strlen(p2 + 1));
}

// Lo publicado en MQTT_TOPIC_BROADCAST se reenvía a toda la malla
void OnMqttReceived(char* topic, byte* payload, unsigned int length)
{
  if (strcmp(topic, MQTT_TOPIC_BROADCAST) != 0 || length == 0 || length > 200) return;
  char buf[201];
  memcpy(buf, payload, length);
  buf[length] = '\0';
  Serial.printf("MQTT -> malla: %s\n", buf);
  mesh.sendBroadcast(String(buf));
}

void anunciar()
{
  char a[12];
  snprintf(a, sizeof(a), "P|%s", g_nombre);
  mesh.sendBroadcast(String(a));
}

void enviarEstado()
{
  char json[TAM_JSON];
  int n = jsonEstado(json, sizeof(json), "puente", WiFi.RSSI(), 0);
  if (n > 0 && n < (int)sizeof(json))
  {
    g_seq++;
    Publicar("estado", g_nombre, json, n);
  }
}

void logPeriodico()
{
  char extra[96];
  snprintf(extra, sizeof(extra), "mqtt=%d nodos=%u publicados=%lu fallos=%lu descartados=%lu",
           mqttClient.state(), (unsigned)mesh.getNodeList().size(),
           (unsigned long)nPublicados, (unsigned long)nFallos, (unsigned long)nDescartados);
  logMemoria(extra);
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("Malla: nueva conexión, nodeId = %u\n", nodeId);
  anunciar();   // que el nodo nuevo sepa enseguida quién es el puente
}

void setup()
{
  Serial.begin(115200);
  identificarPlaca();
  Serial.printf("\nBaliza %s (%s) -> rol: PUENTE\n", g_nombre, g_mac);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(BRIDGE_HOSTNAME);
  mesh.setRoot(true);
  mesh.setContainsRoot(true);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("WiFi STA desconectada, motivo=%u\n", info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);

  InitMqtt();
  ultimoConIP = millis();

  userScheduler.addTask(tAnuncio);  tAnuncio.enable();
  userScheduler.addTask(tEstado);   tEstado.enableDelayed(15000);
  userScheduler.addTask(tLog);      tLog.enable();

  iniciarWatchdog();
}

void loop()
{
  esp_task_wdt_reset();
  mesh.update();

  IPAddress ip(mesh.getStationIP());
  bool hayIP = ip != IPAddress(0, 0, 0, 0);
  if (ip != stationIP)
  {
    stationIP = ip;
    Serial.printf("WiFi %s: IP %s\n", STATION_SSID, ip.toString().c_str());
  }
  if (hayIP) ultimoConIP = millis();
  else ultimoMqttOk = millis();   // sin IP manda la regla MAX_SIN_IP_MS, no la de MQTT

  HandleMqtt(hayIP);

  // Reinicio de seguridad: mejor reiniciar que quedarse mudo durante la demo
  if (hayIP && millis() - ultimoMqttOk > MAX_SIN_MQTT_MS)
  {
    Serial.println("REINICIO: con IP pero sin MQTT demasiado tiempo");
    delay(100);
    ESP.restart();
  }
  if (millis() - ultimoConIP > MAX_SIN_IP_MS)
  {
    Serial.println("REINICIO: sin IP en la WiFi de la Raspberry demasiado tiempo");
    delay(100);
    ESP.restart();
  }
}
