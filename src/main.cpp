#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <queue>

// Configuração da tela OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCL, /* data=*/SDA);

// LoRa Pins
#define SS 18
#define RST 14
#define DIO0 26

// WiFi Configuration
const char *ssid = "";    // Substitua pelo seu SSID
const char *password = ""; // Substitua pela sua senha WiFi

// MQTT Configuration
const char *mqttServer = "";
const int mqttPort = 8883;                // Porta para TLS (SSL)
const char *mqttClientID = ""; // ID do cliente (pode ser o nome do dispositivo)
const char *mqttTopic = "";

// Certificados (você precisa carregar os certificados na memória ou no SPIFFS)
const char *awsRootCA = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

const char *deviceCert = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

const char *devicePrivateKey = R"EOF(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----
)EOF";

// Definir o cliente Wi-Fi e MQTT
WiFiClientSecure espClient;
PubSubClient client(espClient);

byte myAddress = 0x01; // Endereço deste dispositivo
std::queue<String> messageQueue;

void displayMessage(String line1, String line2 = "", String line3 = "")
{
  u8g2.clearBuffer();                 // Limpa o buffer
  u8g2.setFont(u8g2_font_ncenB08_tr); // Define a fonte

  // Exibe as mensagens nas linhas
  u8g2.drawStr(0, 10, line1.c_str());
  if (!line2.isEmpty())
    u8g2.drawStr(0, 30, line2.c_str());
  if (!line3.isEmpty())
    u8g2.drawStr(0, 50, line3.c_str());

  u8g2.sendBuffer(); // Envia o buffer para a tela
}

void connectToWiFi()
{
  displayMessage("Conectando ao", "WiFi...");
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  displayMessage("WiFi conectado!", "IP:", WiFi.localIP().toString());
  delay(2000);
}

void connectToMQTT()
{
  displayMessage("Conectando ao", "MQTT...");
  Serial.print("Conectando ao MQTT...");

  // Configura os certificados para conexão segura
  espClient.setCACert(awsRootCA);
  espClient.setCertificate(deviceCert);
  espClient.setPrivateKey(devicePrivateKey);

  while (!client.connected())
  {
    Serial.print("Tentando conexão...");
    if (client.connect(mqttClientID))
    {
      Serial.println("Conectado ao MQTT!");
      displayMessage("MQTT conectado!");
    }
    else
    {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      displayMessage("Erro MQTT", "Tentando...", String(client.state()));
      delay(5000);
    }
  }
}

void setup()
{
  // Inicialização serial
  Serial.begin(9600);
  while (!Serial)
    ;

  // Inicializa OLED
  u8g2.begin();
  displayMessage("Inicializando", "TTGO LoRa32...");

  // Inicializa WiFi e MQTT
  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback([](char *topic, byte *payload, unsigned int length)
                     {
    String message;
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Mensagem MQTT: ");
    Serial.println(message);
    displayMessage("MQTT Msg:", message); });

  // Inicializa LoRa
  Serial.println("Iniciando LoRa...");
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(915E6))
  {
    Serial.println("Falha ao inicializar LoRa!");
    displayMessage("Erro ao iniciar", "LoRa!");
    while (1)
      ;
  }

  LoRa.setSpreadingFactor(12); // Maximum sensitivity
  LoRa.setCodingRate4(8);      // Maximum error correction

  Serial.println("LoRa iniciado!");
  displayMessage("LoRa iniciado!");
  delay(2000);
}

void loop()
{
  if (!client.connected())
  {
    connectToMQTT();
  }
  else
  {
    // Envia mensagens pendentes na fila
    while (!messageQueue.empty())
    {
      String pendingMessage = messageQueue.front();
      if (client.publish("diegohat/lora", pendingMessage.c_str()))
      {
        Serial.println("Mensagem pendente enviada:");
        Serial.println(pendingMessage);
        messageQueue.pop(); // Remove a mensagem da fila após o envio
      }
      else
      {
        Serial.println("Falha ao enviar mensagem pendente. Mantendo na fila.");
        break; // Para o envio se falhar
      }
    }
  }
  client.loop();

  // Receber pacotes LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    byte sender = LoRa.read();                  // Primeiro byte é o endereço do remetente
    String receivedMessage = LoRa.readString(); // Resto é a mensagem

    if (sender == myAddress || sender == 0xFF)
    { // Verificar endereço ou broadcast
      Serial.println("Mensagem recebida via LoRa:");
      Serial.println(receivedMessage);

      // Obter dados do sinal
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();

      // Exibe mensagem na tela
      displayMessage("LoRa Msg:", receivedMessage);

      // Divide a string de entrada pelos delimitadores
      int soloIndex = receivedMessage.indexOf("Solo=");
      int umidadeIndex = receivedMessage.indexOf("Umidade=");
      int inclinacaoIndex = receivedMessage.indexOf("Inclinacao=");

      // Extrai os valores
      String soloValor = receivedMessage.substring(soloIndex + 5, receivedMessage.indexOf(",", soloIndex));
      String umidadeValor = receivedMessage.substring(umidadeIndex + 8, receivedMessage.indexOf(",", umidadeIndex));
      String inclinacaoValor = receivedMessage.substring(inclinacaoIndex + 11, receivedMessage.indexOf(",", inclinacaoIndex));

      // Criar objeto JSON
      DynamicJsonDocument jsonDoc(256); // Ajuste o tamanho conforme necessário
      jsonDoc["sender"] = sender;       // Adiciona o endereço do remetente
      jsonDoc["solo"] = soloValor;
      jsonDoc["umidade"] = umidadeValor;
      jsonDoc["inclinacao"] = inclinacaoValor;
      jsonDoc["rssi"] = rssi; // Adiciona o RSSI
      jsonDoc["snr"] = snr;   // Adiciona o SNR

      // Serializar o JSON para uma string
      String jsonString;
      serializeJson(jsonDoc, jsonString);

      // Publicar o JSON no MQTT ou salvar na fila
      if (client.connected())
      {
        if (client.publish("diegohat/lora", jsonString.c_str()))
        {
          Serial.println("Mensagem publicada no MQTT como JSON:");
          Serial.println(jsonString);
        }
        else
        {
          Serial.println("Falha ao publicar mensagem no MQTT. Adicionando à fila.");
          messageQueue.push(jsonString);
        }
      }
      else
      {
        Serial.println("MQTT desconectado. Adicionando mensagem à fila.");
        messageQueue.push(jsonString);
      }
    }
    else
    {
      Serial.println("Mensagem ignorada (não é para mim).");
    }
  }

  delay(10); // Reduz uso de CPU
}
