# LoRa Gateway (TTGO LoRa32 v1)

LoRa → MQTT gateway firmware for the LILYGO TTGO LoRa32 v1 (ESP32 + SX1276 + OLED), built with PlatformIO (Arduino). It listens for LoRa packets, shows brief status on the OLED, and publishes received data as JSON to an MQTT broker over TLS.

This repo is intended for development and education; see the License section for terms.

**Highlights**
- LoRa receiver using SX1276 at `915 MHz` (adjustable for your region)
- Simple address filtering: accepts packets for `myAddress` or broadcast `0xFF`
- Parses sensor messages like `Solo=...,Umidade=...,Inclinacao=...`
- Publishes JSON to MQTT over TLS (`WiFiClientSecure`)
- Onboard OLED status via `U8g2`
- Basic offline queue for MQTT publishes when the broker is unreachable

## Repositories
- Gateway (this repo)
- Node (transmitter): https://github.com/diegohat/lora-node

## Hardware
- Board: `ttgo-lora32-v1` (LILYGO TTGO LoRa32 v1, ESP32 + SX1276 + 0.96" OLED)
- LoRa pins (used in firmware): `SS` 18, `RST` 14, `DIO0` 26
- OLED: I2C (`SCL`, `SDA`) via default ESP32 pins
- Frequency: `915E6` set in code (change for 433/868/915 MHz as needed)

If you use a different board or revision, verify pin mapping and antenna connection before powering on.

## Project Layout
- `src/main.cpp`: firmware logic (Wi‑Fi + MQTT TLS, LoRa RX, OLED UI)
- `platformio.ini`: PlatformIO environment for `ttgo-lora32-v1` + libs
- `include/`, `lib/`, `test/`: standard PlatformIO folders

Dependencies (`platformio.ini`):
- `Wire`, `sandeepmistry/LoRa`, `olikraus/U8g2`, `knolleary/PubSubClient`, `bblanchon/ArduinoJson`, `WiFiClientSecure`

## Build & Flash (macOS/zsh)
Prerequisites: VS Code + PlatformIO extension or PlatformIO Core CLI, and a working USB cable/driver for the ESP32.

1) Connect the TTGO LoRa32 v1 via USB.
2) From the repo root, build and upload:

```sh
# Build for the defined env
pio run -e ttgo-lora32-v1

# Flash the firmware
pio run -t upload -e ttgo-lora32-v1

# Optional: open serial monitor (baud 9600)
pio device monitor -b 9600
```

With PlatformIO in VS Code, select `ttgo-lora32-v1` and use Build/Upload/Monitor.

## Configuration
Edit `src/main.cpp` before flashing. Set:
- Wi‑Fi: `ssid`, `password`
- MQTT: `mqttServer`, `mqttPort` (default `8883` for TLS), `mqttClientID`
- Certificates/Keys: set `awsRootCA`, `deviceCert`, `devicePrivateKey` (PEM). You can adapt to load from SPIFFS/LittleFS.
- Address filter: `myAddress` (default `0x01`). Packets for `myAddress` or `0xFF` (broadcast) are processed.
- LoRa radio: frequency (`LoRa.begin(915E6)`), spreading factor (`12`), coding rate (`4/8`). Adjust for region/range/airtime.

Topic
- The publish topic is currently hardcoded as `"diegohat/lora"` inside `loop()`. Change it there if you need a different topic (the `mqttTopic` variable is not used).

## How It Works
At startup the device:
- Initializes serial (9600), OLED, Wi‑Fi, MQTT client, and the LoRa radio.
- Sets the MQTT callback to display incoming messages on the OLED.
- Uses TLS via `WiFiClientSecure` with the provided certificates.

Main loop:
- Keeps the MQTT session alive and flushes queued messages when the broker becomes available.
- Reads LoRa packets. Expected payload format is UTF‑8 with keys in Portuguese: `Solo=...`, `Umidade=...`, `Inclinacao=...`.
- Builds a JSON document with fields: `sender`, `solo`, `umidade`, `inclinacao`, `rssi`, `snr`.
- Publishes JSON to MQTT (TLS). If publish fails or MQTT is disconnected, the message is queued and retried later.

Example JSON published to MQTT:
```json
{
  "sender": 1,
  "solo": "SECO",
  "umidade": "73.00%",
  "inclinacao": "NORMAL",
  "rssi": -87,
  "snr": 7.5
}
```

OLED
- Displays brief status: connecting to Wi‑Fi/MQTT, and most recent LoRa/MQTT messages.

## Interoperability (Node ↔ Gateway)
- Addressing: node must prepend a single address byte. Gateway processes packets if the first byte equals `myAddress` (default `0x01`) or `0xFF` (broadcast).
- Payload format: UTF‑8 text with comma‑separated key/value fields the gateway parses: `Solo=...,Umidade=...,Inclinacao=...`.
- Radio settings: node and gateway must match frequency, spreading factor, and coding rate (default here: 915 MHz, SF12, CR 4/8).
- Topic: gateway publishes JSON to MQTT topic `diegohat/lora` (change in `src/main.cpp` if needed).

## Troubleshooting
- LoRa init fails: ensure correct frequency for your region/board, antenna attached, and pins (`SS=18`, `RST=14`, `DIO0=26`) match your hardware.
- No packets received: verify your sender uses the same frequency/SF/CR and that the first byte is the address (`myAddress`) or `0xFF` for broadcast.
- MQTT not connecting: check broker address/port, client ID, and certificate chain. Check `client.state()` in serial logs for error codes.
- TLS issues: confirm PEM blocks are complete and correct. Some brokers require SNI or specific cipher suites; `WiFiClientSecure` on ESP32 supports a subset.
- Wi‑Fi: double‑check `ssid`/`password` and proximity to the AP. Power cycle after changes.
- Serial: monitor at `9600` baud. If nothing prints, press reset.

## Notes & Customization
- Region: change `LoRa.begin(915E6)` to `868E6` or `433E6` where legally required.
- Performance: `SF12` and `CR 4/8` maximize link budget but increase airtime. Tune for your use case.
- Storage: for production, consider moving certificates to SPIFFS/LittleFS.
- Topics: replace the hardcoded `"diegohat/lora"` with your own topic, or wire up the unused `mqttTopic` variable.

## License
This project is licensed under the GNU GPLv3. See `LICENSE` for details.

## Acknowledgements
- Libraries: `LoRa`, `U8g2`, `PubSubClient`, `ArduinoJson`, `WiFiClientSecure`, `Wire`
- Built with PlatformIO
