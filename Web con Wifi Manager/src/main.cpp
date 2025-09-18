#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <DHTesp.h>

const int DHT_PIN = 4;
const unsigned long READ_INTERVAL = 2000; // ms entre lecturas

// ---- Aquí defines tu red WiFi ----
const char* WIFI_SSID = "Tobar_2";      // Cambia aquí el nombre de tu red
const char* WIFI_PASS = "Homb0549";     // Cambia aquí la contraseña de tu red
// -----------------------------------

WebServer server(80);
DHTesp dht;

float lastTemp = NAN;
float lastHum = NAN;
unsigned long lastReadMs = 0;

// -------------------- Utilidades SPIFFS --------------------
String contentType(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool serveFile(const String &path) {
  String p = path;
  if (p == "/") p = "/index.html";
  if (!SPIFFS.exists(p)) return false;
  File f = SPIFFS.open(p, "r");
  if (!f) return false;
  server.streamFile(f, contentType(p));
  f.close();
  return true;
}

// -------------------- Handlers --------------------
void handleRoot() {
  if (!serveFile("/index.html"))
    server.send(404, "text/plain", "index.html not found");
}

void handleApiData() {
  unsigned long now = millis();
  if (now - lastReadMs >= READ_INTERVAL) {
    lastReadMs = now;
    TempAndHumidity th = dht.getTempAndHumidity();
    if (!isnan(th.temperature) && !isnan(th.humidity)) {
      lastTemp = th.temperature;
      lastHum  = th.humidity;
      Serial.printf("DHT read: T=%.1f C  H=%.1f %%\n", lastTemp, lastHum);
    } else {
      Serial.println("DHT read failed");
    }
  }

  StaticJsonDocument<128> doc;
  if (isnan(lastTemp) || isnan(lastHum))
    doc["error"] = "sensor";
  else {
    doc["temp"] = round(lastTemp * 10.0) / 10.0;
    doc["hum"]  = round(lastHum * 10.0) / 10.0;
  }
  doc["ts"] = millis();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleNotFound() {
  String path = server.uri();
  if (serveFile(path)) return;
  server.send(404, "text/plain", "404");
}

// -------------------- Función para AP automático --------------------
String makeApName() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[9];
  sprintf(buf, "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]); 
  String mactail = String(buf + 4); 
  return "ESP32_" + mactail;
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nStarting ESP32 DHT22 WebServer");

  // Montar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted");
  }

  // Intento directo de conexión con red definida
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Intentando conectar a WiFi SSID: %s\n", WIFI_SSID);

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000; // 10s timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado directamente a la red configurada.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFallo la conexión directa. Activando WiFiManager...");

    WiFiManager wm;
    String apName = makeApName();
    wm.setTimeout(180); // 3 minutos
    if (!wm.autoConnect(apName.c_str())) {
      Serial.println("WiFiManager no logró conectar. Continuando en modo AP.");
    } else {
      Serial.println("Conectado mediante WiFiManager.");
    }
  }

  // Iniciar mDNS
  if (MDNS.begin("ESP32-DHT")) {
    Serial.println("mDNS disponible en: http://ESP32-DHT.local");
  } else {
    Serial.println("Error iniciando mDNS");
  }

  // Inicializar DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Configurar rutas
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, [](){ serveFile("/index.html"); });
  server.on("/style.css", HTTP_GET, [](){ serveFile("/style.css"); });
  server.on("/script.js", HTTP_GET, [](){ serveFile("/script.js"); });
  server.on("/api/data", HTTP_GET, handleApiData);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado");

  lastReadMs = millis() - READ_INTERVAL;
}

// -------------------- Loop --------------------
void loop() {
  server.handleClient();
}
