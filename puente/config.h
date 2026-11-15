#pragma once
// Configuración solo del puente. La malla y los nombres de las placas están en comun.h.

// ---- WiFi de la Raspberry Pi ----
#define STATION_SSID     "GatewayPA"
#define STATION_PASSWORD "secretoDeGateway123"
#define BRIDGE_HOSTNAME  "plot-advisor-bridge"

// ---- MQTT ----
#define MQTT_BROKER     IPAddress(10, 42, 0, 1)
#define MQTT_PORT       1883
#define MQTT_USER       "plot-advisor"
#define MQTT_PASSWORD   "secretoDeMQTT123"
#define MQTT_KEEPALIVE_S 60
#define MQTT_RECONNECT_INTERVAL 5000UL

#define MQTT_TOPIC_BASE      "plot-advisor/c001"
#define MQTT_TOPIC_BROADCAST "plot-advisor/c001/to/broadcast"

// ---- Reinicio de seguridad ----
#define MAX_SIN_MQTT_MS  90000UL    // con IP pero sin MQTT durante 90 s -> reinicio
#define MAX_SIN_IP_MS   180000UL    // sin IP en plot-advisor durante 3 min -> reinicio
