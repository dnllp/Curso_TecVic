#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LED_PIN 2

const char* ssid = "Robot_Telemetria_AP";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Temporizador no bloqueante para el envío de telemetría (cada 1 segundo)
unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 1000;

// Interfaz Web Embebida
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Telemetría y Control WebSocket</title>
  <style>
    * {
      -webkit-touch-callout: none;
      -webkit-user-select: none;
      user-select: none;
      box-sizing: border-box;
    }
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #1a1a24;
      color: #ffffff;
      margin: 0;
      padding: 20px;
    }
    h2 { margin-bottom: 20px; font-size: 22px; }
    
    .card {
      background: #282838;
      border-radius: 12px;
      padding: 20px;
      max-width: 320px;
      margin: 15px auto;
      box-shadow: 0 4px 10px rgba(0,0,0,0.3);
    }
    .metric-title { font-size: 14px; color: #a0a0b0; text-transform: uppercase; letter-spacing: 1px; }
    .metric-value { font-size: 44px; font-weight: bold; color: #00d2d3; margin: 10px 0; }
    .metric-unit { font-size: 20px; color: #888; }
    
    .btn {
      padding: 14px 28px;
      font-size: 16px;
      font-weight: bold;
      border: none;
      border-radius: 8px;
      margin: 8px;
      cursor: pointer;
      transition: transform 0.1s, opacity 0.2s;
    }
    .btn:active { transform: scale(0.96); }
    .btn-on { background-color: #10ac84; color: white; }
    .btn-off { background-color: #ee5253; color: white; }
    
    .status { font-size: 13px; color: #888; margin-top: 15px; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>ESP32: Bidireccionalidad WebSocket</h2>

  <!-- Bloque Servidor -> Cliente (Telemetría de Temperatura) -->
  <div class="card">
    <div class="metric-title">Temperatura Interna ESP32</div>
    <div class="metric-value">
      <span id="temp-val">--</span><span class="metric-unit"> °C</span>
    </div>
  </div>

  <!-- Bloque Cliente -> Servidor (Control de Actuador) -->
  <div class="card">
    <div class="metric-title">Control del Actuador (LED)</div>
    <div style="margin-top: 15px;">
      <button class="btn btn-on" onclick="sendCmd('LED_ON')">Encender</button>
      <button class="btn btn-off" onclick="sendCmd('LED_OFF')">Apagar</button>
    </div>
  </div>

  <div class="status" id="ws-status">Conectando...</div>

  <script>
    document.addEventListener('contextmenu', e => e.preventDefault());

    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      
      websocket.onopen = () => { 
        document.getElementById('ws-status').innerText = 'Enlace WebSocket Activo'; 
      };
      
      websocket.onclose = () => {
        document.getElementById('ws-status').innerText = 'Desconectado. Reconectando...';
        setTimeout(initWebSocket, 2000);
      };

      // Recepción de datos Servidor -> Cliente
      websocket.onmessage = (event) => {
        let msg = event.data;
        if (msg.startsWith("TEMP:")) {
          let temp = msg.split(":")[1];
          document.getElementById('temp-val').innerText = temp;
        }
      };
    }

    // Envío de datos Cliente -> Servidor
    function sendCmd(cmd) {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(cmd);
      }
    }

    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)rawliteral";

// Procesa mensajes entrantes del Cliente
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
  Serial.print("Punto de acceso: ");
  Serial.println(ssid);
  Serial.print("IP: http://");
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

  // Emisión periódica de telemetría (Servidor -> Todos los Clientes)
  if (millis() - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = millis();

    // Lectura del sensor térmico on-chip del ESP32
    float tempC = temperatureRead();

    // Mensaje en formato "TEMP:XX.X"
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "TEMP:%.1f", tempC);

    // Difusión vía WebSocket a los navegadores conectados
    ws.textAll(buffer);
  }
}