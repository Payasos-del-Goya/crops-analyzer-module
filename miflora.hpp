#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// Dirección MAC del sensor
static BLEAddress sensorAddress("5C:85:7E:13:C7:31");

// UUIDs del servicio y características
// Nota: En BLE, 00001a00... pertenece al servicio base 00001204...
static BLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static BLEUUID writeUUID("00001a00-0000-1000-8000-00805f9b34fb");
static BLEUUID sensorDataUUID("00001a01-0000-1000-8000-00805f9b34fb");

struct datosMiflora {
  float tempAire;
  int hr;
  int conductividad;
  int luz_lux;
};

datosMiflora leerSensor()
{
    BLEDevice::init("ESP32_PlantSensor");
    Serial.println("Iniciando conexión BLE...");
    BLEClient *pClient = BLEDevice::createClient();

    struct datosMiflora datos = {0.0f, 0, 0, 0};

    // Conectar al sensor
    if (!pClient->connect(sensorAddress))
    {
        Serial.println("Error: No se pudo conectar al sensor.");
        delete pClient;
        return datos;
    }
    Serial.println("Conectado al sensor.");

    // Obtener el servicio principal
    BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
        Serial.println("Error: Servicio no encontrado.");
        pClient->disconnect();
        delete pClient;
        return datos;
    }

    // Obtener las características de escritura y lectura
    BLERemoteCharacteristic *pWriteChar = pRemoteService->getCharacteristic(writeUUID);
    BLERemoteCharacteristic *pDataChar = pRemoteService->getCharacteristic(sensorDataUUID);

    if (pWriteChar == nullptr || pDataChar == nullptr)
    {
        Serial.println("Error: Características no encontradas.");
        pClient->disconnect();
        delete pClient;
        return datos;
    }

    // 1. Enviar comando [0xA0, 0x1F] para activar lectura en tiempo real
    uint8_t cmd[] = {0xA0, 0x1F};
    pWriteChar->writeValue(cmd, sizeof(cmd), true); // true = esperar respuesta (write with response)
    delay(100);                                     // Pequeña pausa de seguridad para que el sensor prepare el dato

    // 2. Leer los datos crudos
    std::string rawStr = pDataChar->readValue();
    const uint8_t *raw = (const uint8_t *)rawStr.c_str();
    size_t len = rawStr.length();

    if (len < 10)
    {
        Serial.printf("Error: Respuesta inesperada (longitud %d)\n", len);
    }
    else
    {
        // 3. Desempaquetar bytes (Little-Endian, igual que struct.unpack_from en Python)
        int16_t rawTemp = (int16_t)(raw[0] | (raw[1] << 8));
        float temperature = rawTemp / 10.0f;

        uint32_t light = (uint32_t)raw[3] | ((uint32_t)raw[4] << 8) |
                         ((uint32_t)raw[5] << 16) | ((uint32_t)raw[6] << 24);

        uint8_t moisture = raw[7];

        uint16_t conductivity = (uint16_t)(raw[8] | (raw[9] << 8));

        // Mostrar resultados
        Serial.println("--- LECTURA DEL SENSOR ---");
        Serial.printf("Temperatura:   %.1f °C\n", temperature);
        Serial.printf("Luz:           %u lux\n", light);
        Serial.printf("Humedad:       %u %%\n", moisture);
        Serial.printf("Conductividad: %u µS/cm\n", conductivity);
        Serial.println("--------------------------");

        datos.conductividad = conductivity;
        datos.hr = moisture;
        datos.luz_lux = light;
        datos.tempAire = temperature;

    }

    // Desconectar y liberar memoria
    pClient->disconnect();
    delete pClient;
    Serial.println("Desconectado.\n");
    return datos;
}
