#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <DHTesp.h>
#include <FS.h>
#include <time.h>

#define DHT_PIN 4
#define READ_INTERVAL 3000 // ms

WebServer server(80);
DHTesp dht;

// Variables globales
float lastTemp = NAN;
float lastHum = NAN;
unsigned long lastReadMs = 0;

// ---- CONFIGURAR HORA ----
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // Argentina GMT-3
const int daylightOffset_sec = 0;

// -------------------- Función para obtener fecha y hora actual --------------------
String getDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "1970-01-01";
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00";
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

// -------------------- Guardar dato en historial --------------------
void saveToHistory(float temp, float hum) {
  if (!SPIFFS.exists("/history.json")) {
    File file = SPIFFS.open("/history.json", "w");
    if (!file) return;
    file.print("{}");
    file.close();
  }

  File file = SPIFFS.open("/history.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();

  String today = getDate();

  JsonArray dayArray = doc[today];
  if (dayArray.isNull()) {
    dayArray = doc.createNestedArray(today);
  }

  JsonObject entry = dayArray.createNestedObject();
  entry["time"] = getTime();
  entry["temp"] = temp;
  entry["hum"] = hum;

  // Guardar nuevamente
  File fileW = SPIFFS.open("/history.json", "w");
  if (serializeJson(doc, fileW) == 0) {
    Serial.println("Error guardando history.json");
  }
  fileW.close();
}

// -------------------- Servir archivo SPIFFS --------------------
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
void handleApiData() {
  StaticJsonDocument<128> doc;
  if (isnan(lastTemp) || isnan(lastHum)) {
    doc["error"] = "sensor";
  } else {
    doc["temp"] = lastTemp;
    doc["hum"] = lastHum;
  }
  doc["ts"] = millis();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleHistory() {
  if (!server.hasArg("date")) {
    server.send(400, "application/json", "{\"error\":\"missing date\"}");
    return;
  }
  String date = server.arg("date");

  if (!SPIFFS.exists("/history.json")) {
    server.send(200, "application/json", "[]");
    return;
  }

  File file = SPIFFS.open("/history.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();

  if (!doc.containsKey(date)) {
    server.send(200, "application/json", "[]");
    return;
  }

  String out;
  serializeJson(doc[date], out);
  server.send(200, "application/json", out);
}

void handleNotFound() {
  String path = server.uri();
  if (serveFile(path)) return;
  server.send(404, "text/plain", "404 Not Found");
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Iniciando ESP32 DHT22 con historial");

  // Montar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
  }

  // Configurar WiFi con WiFiManager
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.autoConnect("ESP32-DHT_AP");

  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());

  // Configurar NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Inicializar DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Configurar rutas
  server.on("/", HTTP_GET, []() { serveFile("/index.html"); });
  server.on("/index.html", HTTP_GET, []() { serveFile("/index.html"); });
  server.on("/styles.css", HTTP_GET, []() { serveFile("/styles.css"); });
  server.on("/script.js", HTTP_GET, []() { serveFile("/script.js"); });
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado");

  lastReadMs = millis();
}

// -------------------- Loop --------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastReadMs >= READ_INTERVAL) {
    lastReadMs = now;

    TempAndHumidity th = dht.getTempAndHumidity();
    if (!isnan(th.temperature) && !isnan(th.humidity)) {
      lastTemp = th.temperature;
      lastHum = th.humidity;
      Serial.printf("T=%.1f°C H=%.1f%%\n", lastTemp, lastHum);

      // Guardar en historial
      saveToHistory(lastTemp, lastHum);
    } else {
      Serial.println("Error leyendo DHT22");
    }
  }
}
