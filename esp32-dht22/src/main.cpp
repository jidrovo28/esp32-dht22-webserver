#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

const char* ssid     = "Wokwi-GUEST";
const char* password = "";

#define DHTPIN  4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

struct SensorData {
  float temperatura;
  float humedad;
  bool  valido;
};

SensorData sensorData = { 0.0f, 0.0f, false };
SemaphoreHandle_t mutex = NULL;

void handleRoot() {
  SensorData local;
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(200))) {
    local = sensorData;
    xSemaphoreGive(mutex);
  } else {
    server.send(503, "text/plain", "Sensor ocupado, intenta de nuevo.");
    return;
  }

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>ESP32 HTTP Server</title>
    <meta http-equiv="refresh" content="3">
    <style>
      body { font-family: Arial; text-align: center; background-color: #f2f2f2; }
      .card {
        background: white; width: 350px; margin: auto;
        margin-top: 40px; padding: 20px;
        border-radius: 15px; box-shadow: 0px 0px 10px gray;
      }
      h1   { color: #333; }
      .temp { color: red;  font-size: 35px; }
      .hum  { color: blue; font-size: 35px; }
      .warn { color: orange; font-size: 14px; }
    </style>
  </head>
  <body>
    <div class="card">
      <h1>ESP32 HTTP Server</h1>
)rawliteral";

  if (!local.valido) {
    html += "<p class='warn'>Esperando primera lectura del sensor...</p>";
  } else {
    html += "<h2>Temperatura</h2><div class='temp'>";
    html += String(local.temperatura, 1);
    html += " °C</div>";
    html += "<h2>Humedad</h2><div class='hum'>";
    html += String(local.humedad, 1);
    html += " %</div>";
  }

  html += "<br><p>Servidor Web funcionando correctamente</p></div></body></html>";
  server.send(200, "text/html", html);
}

void TaskSensor(void* parameter) {
  while (true) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
        sensorData.temperatura = t;
        sensorData.humedad     = h;
        sensorData.valido      = true;
        xSemaphoreGive(mutex);
      }
      Serial.printf("[Sensor] T=%.1f°C  H=%.1f%%\n", t, h);
    } else {
      Serial.println("[Sensor] ERROR: lectura invalida del DHT22");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskWeb(void* parameter) {
  while (true) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("================================");
  Serial.println("PROGRAMA INICIADO");
  Serial.println("================================");

  dht.begin();

  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("ERROR CRITICO: no se pudo crear el mutex");
    ESP.restart();
  }

  Serial.println("Conectando WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor Web iniciado");

  xTaskCreatePinnedToCore(TaskSensor, "SensorTask", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskWeb,    "WebTask",    8192, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}