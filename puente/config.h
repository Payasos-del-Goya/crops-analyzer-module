#pragma once
// Configuración solo del puente. La malla y los nombres de las placas están en comun.h.

// ---- WiFi de la Raspberry Pi ----
#define STATION_SSID     "fieldshift"
#define STATION_PASSWORD "somethingSneaky"
#define BRIDGE_HOSTNAME  "fieldshift-bridge"

// ---- MQTT ----
#define MQTT_BROKER     IPAddress(10, 42, 0, 1)
#define MQTT_PORT       1883
#define MQTT_USER       "baliza"
#define MQTT_PASSWORD   "pitochico"
#define MQTT_KEEPALIVE_S 60
#define MQTT_RECONNECT_INTERVAL 5000UL

#define MQTT_TOPIC_BASE      "fieldshift/c001"
#define MQTT_TOPIC_BROADCAST "fieldshift/c001/to/broadcast"

// ---- Reinicio de seguridad ----
#define MAX_SIN_MQTT_MS  90000UL    // con IP pero sin MQTT durante 90 s -> reinicio
#define MAX_SIN_IP_MS   180000UL    // sin IP en fieldshift durante 3 min -> reinicio
