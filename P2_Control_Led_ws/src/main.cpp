#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LED_PIN 2  // LED integrado habitual en ESP32

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* ssid = "Robot_Taller_LED";
const char* password = "tecvictoria";

// Página Web embebida con prevención de menú contextual
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control LED WebSocket</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; user-select: none; }
    .btn { padding: 15px 30px; font-size: 20px; border: none; border-radius: 8px; margin: 10px; cursor: pointer; }
    .btn-on { background-color: #28a745; color: white; }
    .btn-off { background-color: #dc3545; color: white; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>ESP32 WebSocket - Control LED</h2>
  <button class="btn btn-on" onclick="sendCmd('LED_ON')">Encender LED</button>
  <button class="btn btn-off" onclick="sendCmd('LED_OFF')">Apagar LED</button>
  <p id="status">Conectando...</p>

  <script>
    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = () => { document.getElementById('status').innerText = 'Conectado'; };
      websocket.onclose = () => {
        document.getElementById('status').innerText = 'Desconectado, reintentando...';
        setTimeout(initWebSocket, 2000);
      };
    }

    function sendCmd(cmd) {
      if (websocket.readyState === WebSocket.OPEN) {
        websocket.send(cmd);
      }
    }

    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    if (message == "LED_ON") {
      digitalWrite(LED_PIN, HIGH);
    } else if (message == "LED_OFF") {
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.softAP(ssid, password);
  Serial.print("IP del Punto de Acceso: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
}