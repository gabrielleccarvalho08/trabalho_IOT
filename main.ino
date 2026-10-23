// Contador 00-20 em dois displays de 7 segmentos + publicação MQTT
// Funciona no Wokwi (rede "Wokwi-GUEST") e em uma placa ESP32 real.

#include <WiFi.h>
#include <PubSubClient.h>

// ======== CONFIGURAÇÃO ========
const char* WIFI_SSID = "WIFI-IOT";   // troque pela sua rede se usar a placa real
const char* WIFI_PASS = "ac5ce1ss0@IOT";

const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;

// Troque "gabriel-d7" por algo único seu (o broker é público!)
// O site precisa usar exatamente o mesmo prefixo.
const char* TOPICO_ESTADO = "ana-b-d7/display7/estado";
const char* TOPICO_ONLINE = "ana-b-d7/display7/online";

const unsigned long INTERVALO_MS = 1000;  // tempo entre cada número
const int VALOR_MAXIMO = 20;
// ==============================

// Pinos (a, b, c, d, e, f, g) — conforme o diagram.json
// Obs.: o segmento C da unidade está no GPIO 17 no diagrama.
const int pinosUnidade[7] = {22, 23, 17, 5, 18, 21, 19};
const int pinosDezena[7]  = {13, 4, 25, 26, 27, 32, 33};

// Cada bit = um segmento (bit0 = a ... bit6 = g)
const uint8_t digitos[10] = {
  0b0111111, // 0
  0b0000110, // 1
  0b1011011, // 2
  0b1001111, // 3
  0b1100110, // 4
  0b1101101, // 5
  0b1111101, // 6
  0b0000111, // 7
  0b1111111, // 8
  0b1101111  // 9
};

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

int valor = 0;
unsigned long ultimoPasso = 0;
unsigned long ultimaTentativaMqtt = 0;
String clientId;

void mostrarDigito(const int pinos[7], int d) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(pinos[i], (digitos[d] >> i) & 1 ? HIGH : LOW);
  }
}

void mostrarValor(int v) {
  mostrarDigito(pinosDezena, v / 10);
  mostrarDigito(pinosUnidade, v % 10);
}

void conectarWifi() {
  Serial.printf("Conectando ao WiFi %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nSem WiFi (o contador continua rodando).");
  }
}

// Reconexão sem travar o contador
void garantirMqtt() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimaTentativaMqtt > 10000) {
      ultimaTentativaMqtt = millis();
      WiFi.reconnect();
    }
    return;
  }
  if (millis() - ultimaTentativaMqtt < 5000) return;
  ultimaTentativaMqtt = millis();

  Serial.print("Conectando ao MQTT... ");
  // Last Will: se o ESP cair, o broker publica "offline"
  if (mqtt.connect(clientId.c_str(), nullptr, nullptr, TOPICO_ONLINE, 0, true, "offline")) {
    Serial.println("OK");
    mqtt.publish(TOPICO_ONLINE, "online", true);
  } else {
    Serial.printf("falhou (rc=%d)\n", mqtt.state());
  }
}

void publicarEstado() {
  if (!mqtt.connected()) return;
  char json[160];
  snprintf(json, sizeof(json),
           "{\"valor\":%d,\"dezena\":%d,\"unidade\":%d,\"uptime\":%lu,\"rssi\":%d}",
           valor, valor / 10, valor % 10, millis() / 1000, WiFi.RSSI());
  mqtt.publish(TOPICO_ESTADO, json, true);  // retained: o site recebe o último valor ao abrir
  Serial.printf("Publicado: %s\n", json);
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 7; i++) {
    pinMode(pinosUnidade[i], OUTPUT);
    pinMode(pinosDezena[i], OUTPUT);
  }
  mostrarValor(0);

  clientId = "esp32-d7-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  conectarWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  garantirMqtt();
  mqtt.loop();

  if (millis() - ultimoPasso >= INTERVALO_MS) {
    ultimoPasso = millis();
    mostrarValor(valor);
    publicarEstado();
    valor = (valor + 1) % (VALOR_MAXIMO + 1);
  }
}
