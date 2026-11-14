#pragma once
// Configuración solo del nodo MiFlora. La malla y los nombres de las placas están en comun.h.

// Sensor Xiaomi MiFlora (Bluetooth LE)
#define MIFLORA_MAC            "5C:85:7E:13:C7:31"
#define MIFLORA_PROFUNDIDAD_CM 5        // la sonda mide los primeros ~5-10 cm del suelo
#define MIFLORA_PERIODO_MS     30000UL  // cada cuánto se lee el sensor
#define MIFLORA_MAX_EDAD_MS    90000UL  // una lectura más vieja que esto no se envía
