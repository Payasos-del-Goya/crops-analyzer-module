#pragma once
// Lectura del sensor Xiaomi MiFlora por Bluetooth LE.
// - El cliente BLE se crea UNA vez y se reutiliza (crearlo/borrarlo en cada lectura pierde memoria).
// - Compatible con el núcleo ESP32 2.x (readValue -> std::string) y 3.x (readValue -> String).
// - No imprime nada: devuelve el motivo del fallo y quien llama decide qué mostrar.
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

static BLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static BLEUUID writeUUID("00001a00-0000-1000-8000-00805f9b34fb");
static BLEUUID sensorDataUUID("00001a01-0000-1000-8000-00805f9b34fb");

struct LecturaMiFlora {
  float temperatura;        // °C
  uint32_t luz_lux;         // lux
  uint8_t humedad;          // % (humedad del SUELO)
  uint16_t conductividad;   // µS/cm (el sensor la da así; en la base de datos va en dS/m)
};

static BLEClient* clienteBLE = nullptr;

// Devuelve true solo si la lectura es válida. Si falla, NO hay datos (no se envían ceros)
// y en 'error' queda el motivo.
bool leerMiFlora(LecturaMiFlora& out, const char*& error)
{
  if (clienteBLE == nullptr)
  {
    BLEDevice::init("");
    clienteBLE = BLEDevice::createClient();
  }

  BLEAddress direccion(MIFLORA_MAC);
  if (!clienteBLE->connect(direccion))
  {
    error = "no conecta por BLE (¿fuera de alcance o sin pila?)";
    return false;
  }

  bool ok = false;
  error = nullptr;
  BLERemoteService* servicio = clienteBLE->getService(serviceUUID);
  BLERemoteCharacteristic* escritura = servicio ? servicio->getCharacteristic(writeUUID) : nullptr;
  BLERemoteCharacteristic* datos = servicio ? servicio->getCharacteristic(sensorDataUUID) : nullptr;

  if (escritura && datos)
  {
    uint8_t cmd[] = {0xA0, 0x1F};            // activar lectura en tiempo real
    escritura->writeValue(cmd, sizeof(cmd), true);
    delay(100);                               // en la tarea del sensor, no bloquea la malla

    auto valor = datos->readValue();          // std::string (núcleo 2.x) o String (3.x)
    const uint8_t* raw = (const uint8_t*)valor.c_str();
    size_t len = valor.length();

    if (len >= 10)
    {
      int16_t t = (int16_t)(raw[0] | (raw[1] << 8));
      out.temperatura = t / 10.0f;
      out.luz_lux = (uint32_t)raw[3] | ((uint32_t)raw[4] << 8) | ((uint32_t)raw[5] << 16) | ((uint32_t)raw[6] << 24);
      out.humedad = raw[7];
      out.conductividad = (uint16_t)(raw[8] | (raw[9] << 8));

      // Valores imposibles = lectura corrupta
      ok = out.temperatura > -40 && out.temperatura < 70 && out.humedad <= 100 &&
           out.conductividad <= 20000 && out.luz_lux <= 200000;
      if (!ok) error = "valores fuera de rango (lectura corrupta)";
    }
    else
    {
      error = "respuesta demasiado corta";
    }
  }
  else
  {
    error = "servicio o características no encontrados";
  }

  clienteBLE->disconnect();
  return ok;
}
