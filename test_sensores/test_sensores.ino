// Monitor para calibrar un FC-28 comparándolo con un MiFlora.
// La humedad calculada es relativa a los dos voltajes configurados abajo;
// no es una medida de humedad volumétrica ni de conductividad en µS/cm.

#define FC28_PIN 35

// Anota los voltajes observados y ajusta estos dos valores.
// Normalmente el FC-28 entrega más voltaje cuando está más seco.
#define VOLTAJE_SECO_MV    2500.0f
#define VOLTAJE_MOJADO_MV  200.0f

#define NUM_MUESTRAS 32

static float calcularPorcentaje(float voltajeMv)
{
  float porcentaje = (VOLTAJE_SECO_MV - voltajeMv) * 100.0f /
                     (VOLTAJE_SECO_MV - VOLTAJE_MOJADO_MV);

  if (porcentaje < 0.0f) porcentaje = 0.0f;
  if (porcentaje > 100.0f) porcentaje = 100.0f;
  return porcentaje;
}

void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(FC28_PIN, ADC_11db);

  Serial.println();
  Serial.println("Monitor FC-28");
  Serial.println("Compara el porcentaje con el MiFlora y ajusta");
  Serial.println("VOLTAJE_SECO_MV y VOLTAJE_MOJADO_MV.");
}

void loop()
{
  uint32_t sumaAdc = 0;
  uint32_t sumaMv = 0;

  for (int i = 0; i < NUM_MUESTRAS; i++)
  {
    sumaAdc += analogRead(FC28_PIN);
    sumaMv += analogReadMilliVolts(FC28_PIN);
    delay(2);
  }

  float adc = sumaAdc / (float)NUM_MUESTRAS;
  float voltajeMv = sumaMv / (float)NUM_MUESTRAS;
  float porcentaje = calcularPorcentaje(voltajeMv);

  Serial.print("ADC: ");
  Serial.print(adc, 0);
  Serial.print(" | Voltaje: ");
  Serial.print(voltajeMv, 0);
  Serial.print(" mV | FC-28: ");
  Serial.print(porcentaje, 1);
  Serial.println(" %");

  delay(1000);
}
