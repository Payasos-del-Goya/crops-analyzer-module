# Formato de los datos (contrato firmware ↔ base de datos)

Este documento define **exactamente** qué tiene que publicar la malla por MQTT para que Telegraf
lo guarde en InfluxDB sin traducciones. Si el firmware cumple esto, los datos aparecen solos en
Grafana. Si algo no cuadra (un campo que no existe, otro nombre, otra unidad), avisad antes de
cambiarlo, porque hay que tocar también la configuración de Telegraf.

## Resumen

| Topic MQTT                                   | Measurement en InfluxDB | Bucket        | Cada cuánto |
|----------------------------------------------|-------------------------|---------------|-------------|
| `fieldshift/c001/<dispositivo>/suelo`        | `suelo`                 | `c001_campo`  | 30 s (demo) |
| `fieldshift/c001/<dispositivo>/atmosfera`    | `atmosfera`             | `c001_campo`  | 30 s (demo) |
| `fieldshift/c001/<dispositivo>/estado`       | `dispositivo`           | `c001_estado` | 60 s        |

- `c001` es el cliente de la demo. `<dispositivo>` es el nombre corto de la baliza (ver abajo).
- Todo lo publica **el nodo puente** en nombre de cada baliza: las balizas mandan el JSON por la
  malla y el puente lo publica en el topic de esa baliza.
- QoS 0, sin `retain`.
- **Un mensaje = un JSON = un punto.** JSON válido, sin nada antes ni después.
- Números como números (`23.4`, no `"23.4"`); etiquetas (tags) siempre como texto.
- Tamaño máximo: **512 bytes**. PubSubClient trae 256 por defecto, así que hay que poner
  `mqttClient.setBufferSize(512)`.

## Nombres de los dispositivos

Cada placa tiene un nombre corto fijo, configurado en `config.h` según su MAC, en lugar del nodeId
de painlessMesh (que es un número largo e ilegible):

| MAC                 | `dispositivo` | Notas                       |
|---------------------|---------------|-----------------------------|
| `E4:65:B8:54:34:D4` | `b01`         | ESP32 blanca, **puente**    |
| `E0:5A:1B:A7:8B:10` | `b02`         |                             |
| `E0:5A:1B:A6:FD:A8` | `b03`         |                             |
| `E0:5A:1B:A7:F8:F9` | `b04`         | MAC deducida del nodeId; comprobadla |

Valores comunes para la demo: `finca = "demo"`, `zona = "A"`.

## 1. Suelo → `c001_campo` / `suelo`

Un mensaje **por sonda y profundidad**. Si una baliza tiene sondas a 10 y 30 cm, manda dos mensajes.

```json
{"finca":"demo","zona":"A","dispositivo":"b02","tipo":"simulado","profundidad_cm":20,
 "humedad_vol":23.4,"conductividad":0.85,"temperatura":15.2}
```

| Clave            | Tipo       | En InfluxDB | Unidad / valores                                              |
|------------------|------------|-------------|---------------------------------------------------------------|
| `finca`          | texto      | tag         |                                                               |
| `zona`           | texto      | tag         |                                                               |
| `dispositivo`    | texto      | tag         | `b01`, `b02`…                                                 |
| `tipo`           | texto      | tag         | tipo de sonda: `capacitivo`, `tdr`… **`simulado` mientras no haya sensor** |
| `profundidad_cm` | entero     | tag         | cm desde la superficie                                         |
| `humedad_vol`    | decimal    | field       | % de humedad volumétrica (0–100)                               |
| `conductividad`  | decimal    | field       | dS/m (conductividad eléctrica del suelo)                       |
| `temperatura`    | decimal    | field       | °C                                                             |

## 2. Atmósfera → `c001_campo` / `atmosfera`

```json
{"finca":"demo","dispositivo":"b01","temp_aire":18.6,"hr":64.0,"temp_helada":2.1,
 "temp_cubierta":17.9,"lluvia_mm":0.2,"hoja_mojada_min":5,"simulado":true}
```

| Clave             | Tipo    | En InfluxDB | Unidad / significado                                              |
|-------------------|---------|-------------|-------------------------------------------------------------------|
| `finca`           | texto   | tag         |                                                                   |
| `dispositivo`     | texto   | tag         |                                                                   |
| `temp_aire`       | decimal | field       | °C, temperatura del aire                                          |
| `hr`              | decimal | field       | % de humedad relativa (0–100)                                      |
| `temp_helada`     | decimal | field       | °C, sensor de helada (a ras de suelo)                              |
| `temp_cubierta`   | decimal | field       | °C, temperatura de la cubierta vegetal / hoja                      |
| `lluvia_mm`       | decimal | field       | mm caídos **desde el mensaje anterior** (incremento, no acumulado) |
| `hoja_mojada_min` | entero  | field       | minutos con la hoja mojada **desde el mensaje anterior**           |
| `simulado`        | bool    | tag         | `true` si los valores son inventados. Omitir con sensores reales   |

`lluvia_mm` y `hoja_mojada_min` van como incremento para que en Grafana se puedan sumar por
hora o día sin que un reinicio de la placa estropee el total.

## 3. Estado de la baliza → `c001_estado` / `dispositivo`

Datos reales de la propia placa (no dependen de sensores).

```json
{"finca":"demo","dispositivo":"b02","tipo":"baliza","bateria_v":3.92,"rssi":-61,"seq":152,
 "sensores_ok":0,"gateway":"b01"}
```

| Clave         | Tipo    | En InfluxDB | Unidad / significado                                                      |
|---------------|---------|-------------|---------------------------------------------------------------------------|
| `finca`       | texto   | tag         |                                                                           |
| `dispositivo` | texto   | tag         |                                                                           |
| `tipo`        | texto   | tag         | `puente` o `baliza`                                                       |
| `bateria_v`   | decimal | field       | V. **Omitir la clave** si la placa no puede medirla (no mandar 0)          |
| `rssi`        | entero  | field       | dBm. Puente: `WiFi.RSSI()` de la conexión a `fieldshift`. Baliza: RSSI del enlace de malla |
| `snr`         | decimal | field       | dB. Solo aplica a LoRa: **omitir** con WiFi                                |
| `seq`         | entero  | field       | contador de mensajes de esa baliza; vuelve a 0 al reiniciar               |
| `sensores_ok` | entero  | field       | nº de sensores que han respondido bien en esta lectura (0 sin sensores)   |
| `gateway`     | texto   | field       | `dispositivo` del puente que lo publicó. **Lo rellena el puente**         |

## Hora de la lectura

Las ESP32 no tienen reloj real, así que **por ahora no se manda hora** y Telegraf usa la hora de
llegada al broker (el retraso por la malla es de menos de un segundo).

Si más adelante se quiere la hora real de la lectura (por ejemplo, para guardar lecturas cuando
el puente esté caído), se añade a cualquier mensaje:

```json
"ts": 1794672000123
```

en **milisegundos UTC desde 1970**. El puente puede sincronizar la hora por NTP con la Raspberry
(`10.42.0.1`) y repartirla por la malla con `mesh.getNodeTime()`. Hay que avisar antes de
activarlo, porque Telegraf tiene que pasar a leer ese campo.

## Reglas para no romper nada

- Una clave que no esté en estas tablas **se descarta** (no da error, pero no se guarda).
- Un JSON mal formado se descarta entero.
- No cambiéis el nombre ni la unidad de una clave existente: los datos antiguos y los nuevos
  quedarían en columnas distintas.
- Los datos simulados van **siempre** marcados (`tipo: "simulado"` en suelo, `simulado: true` en
  atmósfera) para poder filtrarlos o borrarlos cuando haya sensores reales.

## Firmware

Hay dos sketches independientes. Abrir y cargar cada carpeta como un proyecto Arduino:

- `puente/puente.ino`: único puente de la malla; conecta a `fieldshift` y publica los mensajes
  recibidos por MQTT. No usa BLE ni sensores.
- `nodo_miflora/nodo_miflora.ino`: nodo de la malla; lee el MiFlora configurado en `miflora.hpp`
  y difunde sus lecturas por la malla.

Cada sketch contiene sus propios headers (`config.h` y los headers específicos) para que Arduino
IDE pueda compilarlo abriendo directamente esa carpeta, sin depender de archivos de su directorio
padre.
