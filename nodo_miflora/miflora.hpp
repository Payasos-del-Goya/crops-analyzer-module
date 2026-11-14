#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

static BLEAddress sensorAddress("5C:85:7E:13:C7:31");
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
  datosMiflora datos = {0.0f, 0, 0, 0};

  if (!pClient->connect(sensorAddress))
  {
    Serial.println("Error: No se pudo conectar al sensor.");
    delete pClient;
    return datos;
  }
  Serial.println("Conectado al sensor.");

  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.println("Error: Servicio no encontrado.");
    pClient->disconnect();
    delete pClient;
    return datos;
  }

  BLERemoteCharacteristic *pWriteChar = pRemoteService->getCharacteristic(writeUUID);
  BLERemoteCharacteristic *pDataChar = pRemoteService->getCharacteristic(sensorDataUUID);
  if (pWriteChar == nullptr || pDataChar == nullptr)
  {
    Serial.println("Error: Características no encontradas.");
    pClient->disconnect();
    delete pClient;
    return datos;
  }

  uint8_t cmd[] = {0xA0, 0x1F};
  pWriteChar->writeValue(cmd, sizeof(cmd), true);
  delay(100);

  std::string rawStr = pDataChar->readValue();
  const uint8_t *raw = (const uint8_t *)rawStr.c_str();
  size_t len = rawStr.length();
  if (len < 10)
  {
    Serial.printf("Error: Respuesta inesperada (longitud %d)\n", len);
  }
  else
  {
    int16_t rawTemp = (int16_t)(raw[0] | (raw[1] << 8));
    datos.tempAire = rawTemp / 10.0f;
    datos.luz_lux = (uint32_t)raw[3] | ((uint32_t)raw[4] << 8) |
                    ((uint32_t)raw[5] << 16) | ((uint32_t)raw[6] << 24);
    datos.hr = raw[7];
    datos.conductividad = (uint16_t)(raw[8] | (raw[9] << 8));
  }

  pClient->disconnect();
  delete pClient;
  Serial.println("Desconectado.\n");
  return datos;
}
