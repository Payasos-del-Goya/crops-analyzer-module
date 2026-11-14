#pragma once

// ---- Malla painlessMesh ----
#define MESH_PREFIX     "MyMeshNetwork"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MESH_CHANNEL    1

// ---- WiFi de la Raspberry Pi ----
#define STATION_SSID     "fieldshift"
#define STATION_PASSWORD "somethingSneaky"
#define BRIDGE_HOSTNAME  "fieldshift-bridge"

// ---- MQTT ----
#define MQTT_BROKER     IPAddress(10, 42, 0, 1)
#define MQTT_PORT       1883
#define MQTT_USER       "baliza"
#define MQTT_PASSWORD   "pitochico"
#define MQTT_KEEPALIVE  60
#define MQTT_RECONNECT_INTERVAL 5000UL

#define MQTT_TOPIC_BASE      "fieldshift/c001"
#define MQTT_TOPIC_BROADCAST "fieldshift/c001/to/broadcast"
