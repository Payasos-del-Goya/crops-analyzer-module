#pragma once
// Configuración de la baliza. El MISMO firmware se graba en todas las placas:
// la que tenga la MAC BRIDGE_MAC hace de puente malla <-> WiFi fieldshift <-> MQTT,
// el resto son nodos normales de la malla.

// ---- Malla painlessMesh (idéntico en todas las placas) ----
#define MESH_PREFIX     "MyMeshNetwork"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
// Canal WiFi: la malla y la WiFi de la Raspberry Pi (fieldshift) DEBEN estar en el mismo,
// porque el puente usa una sola radio para las dos redes.
#define MESH_CHANNEL    1

// ---- Nodo puente ----
// MAC (la que imprime esptool / la de la pegatina) de la placa que hace de puente.
#define BRIDGE_MAC      "E4:65:B8:54:34:D4"

// WiFi de la Raspberry Pi
#define STATION_SSID     "fieldshift"
#define STATION_PASSWORD "somethingSneaky"
#define BRIDGE_HOSTNAME  "fieldshift-bridge"   // sin '_': no es válido en nombres de host

// ---- MQTT (broker Mosquitto en la Raspberry Pi) ----
#define MQTT_BROKER     IPAddress(10, 42, 0, 1)
#define MQTT_PORT       1883
#define MQTT_USER       "baliza"
#define MQTT_PASSWORD   "pitochico"
#define MQTT_KEEPALIVE  60      // s
#define MQTT_RECONNECT_INTERVAL 5000UL  // ms entre intentos de reconexión

// Topics
#define MQTT_TOPIC_BASE      "fieldshift/c001"           // lecturas: fieldshift/c001/<nodeId>/lecturas
#define MQTT_TOPIC_BROADCAST "fieldshift/c001/to/broadcast"  // lo que se publique aquí se reenvía a toda la malla

// ---- Lecturas ----
// Aún no hay sensores conectados: las lecturas son aleatorias y se marcan "simulado": true.
#define READINGS_SIMULATED true
