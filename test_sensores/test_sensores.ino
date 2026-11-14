#define FC28_PIN 35

// Ajusta estos valores después de calibrar tu sensor
#define VALOR_SECO 3200
#define VALOR_MOJADO 1200

void setup()
{
  Serial.begin(115200);

  analogReadResolution(12);                 // Valores de 0 a 4095
  analogSetPinAttenuation(FC28_PIN, ADC_11db);
}

void loop()
{
  int lectura = analogRead(FC28_PIN);

  int humedad = map(
    lectura,
    VALOR_SECO,
    VALOR_MOJADO,
    0,
    100
  );

  humedad = constrain(humedad, 0, 100);

  Serial.print("Lectura ADC: ");
  Serial.print(lectura);

  Serial.print(" | Humedad aproximada: ");
  Serial.print(humedad);
  Serial.println("%");

  delay(1000);
}