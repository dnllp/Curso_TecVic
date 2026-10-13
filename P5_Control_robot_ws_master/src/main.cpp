#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// Pines del Puente H
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12

// LED integrado de monitoreo
#define LED_PIN 2

const char* ssid = "Robot_Movil_AP1";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Control de concurrencia
uint32_t activeClientId = 0;          // ID del piloto con control activo

// Variables globales de movimiento
int currentSpeed = 200;               // Rango de velocidad (0 - 255)
String currentCommand = "STOP";       // Comando activo

void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  if (leftSpeed > 0) {
    analogWrite(IN1, leftSpeed);
    analogWrite(IN2, 0);
  } else if (leftSpeed < 0) {
    analogWrite(IN1, 0);
    analogWrite(IN2, abs(leftSpeed));
  } else {
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
  }

  rightSpeed = constrain(rightSpeed, -255, 255);
  if (rightSpeed > 0) {
    analogWrite(IN3, rightSpeed);
    analogWrite(IN4, 0);
  } else if (rightSpeed < 0) {
    analogWrite(IN3, 0);
    analogWrite(IN4, abs(rightSpeed));
  } else {
    analogWrite(IN3, 0);
    analogWrite(IN4, 0);
  }
}

// Aplica el movimiento y actualiza el LED monitor
void applyMovement() {
  if (currentCommand == "FWD") {
    digitalWrite(LED_PIN, HIGH);
    setMotors(currentSpeed, currentSpeed);
  } else if (currentCommand == "BWD") {
    digitalWrite(LED_PIN, HIGH);
    setMotors(-currentSpeed, -currentSpeed);
  } else if (currentCommand == "LEFT") {
    digitalWrite(LED_PIN, HIGH);
    int turnVal = constrain(currentSpeed * 0.85, 80, 255);
    setMotors(-turnVal, turnVal);
  } else if (currentCommand == "RIGHT") {
    digitalWrite(LED_PIN, HIGH);
    int turnVal = constrain(currentSpeed * 0.85, 80, 255);
    setMotors(turnVal, -turnVal);
  } else {
    digitalWrite(LED_PIN, LOW);
    setMotors(0, 0);
  }
}

// Interfaz Web Embebida
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Control Robot Móvil</title>
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
      background-color: #1e1e24;
      color: #fff;
      margin: 0;
      padding: 15px;
      touch-action: none;
    }
    h2 { margin: 10px 0; font-size: 22px; }
    
    .slider-box {
      background: #2b2b36;
      border-radius: 12px;
      padding: 15px;
      max-width: 320px;
      margin: 15px auto;
    }
    .slider-label {
      font-size: 16px;
      margin-bottom: 8px;
      display: flex;
      justify-content: space-between;
    }
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 12px;
      border-radius: 6px;
      background: #444456;
      outline: none;
      touch-action: pan-x;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 28px;
      height: 28px;
      border-radius: 50%;
      background: #4caf50;
      cursor: pointer;
    }
    
    .grid {
      display: grid;
      grid-template-columns: repeat(3, 85px);
      grid-gap: 15px;
      justify-content: center;
      margin: 25px auto;
    }
    .btn {
      width: 85px;
      height: 85px;
      font-size: 30px;
      border: none;
      border-radius: 14px;
      background-color: #3f51b5;
      color: white;
      touch-action: manipulation;
      transition: background 0.1s;
    }
    .btn:active { background-color: #1a237e; }
    .status { font-size: 13px; color: #888; margin-top: 10px; }

    /* Estado bloqueado cuando no es el cliente maestro */
    .locked {
      opacity: 0.35;
      pointer-events: none;
    }
    .banner {
      display: none;
      background: #c0392b;
      color: white;
      padding: 10px;
      border-radius: 8px;
      margin: 10px auto;
      max-width: 320px;
      font-size: 14px;
      font-weight: bold;
    }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>Control del Robot</h2>

  <div id="lock-banner" class="banner">Robot en uso por otro piloto</div>

  <div id="controls-container">
    <div class="slider-box">
      <div class="slider-label">
        <span>Velocidad:</span>
        <span id="speed-val">200</span>
      </div>
      <input type="range" min="100" max="255" value="200" class="slider" id="speed-slider">
    </div>

    <div class="grid">
      <div></div>
      <button class="btn" id="btn-fwd">&#9650;</button>
      <div></div>

      <button class="btn" id="btn-left">&#9664;</button>
      <div></div>
      <button class="btn" id="btn-right">&#9654;</button>

      <div></div>
      <button class="btn" id="btn-bwd">&#9660;</button>
      <div></div>
    </div>
  </div>

  <div class="status" id="ws-status">Conectando...</div>

  <script>
    document.addEventListener('contextmenu', e => e.preventDefault());

    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;
    let isMaster = false;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      
      websocket.onopen = () => { 
        document.getElementById('ws-status').innerText = 'Conectado. Verificando disponibilidad...'; 
      };
      
      websocket.onclose = () => {
        document.getElementById('ws-status').innerText = 'Desconectado. Reconectando...';
        setControlLock(true, 'Conexión perdida');
        setTimeout(initWebSocket, 2000);
      };

      websocket.onmessage = (event) => {
        let msg = event.data;
        if (msg === "ROLE:MASTER") {
          isMaster = true;
          setControlLock(false);
          document.getElementById('ws-status').innerText = 'Eres el Piloto Activo';
        } else if (msg === "ROLE:LOCKED") {
          isMaster = false;
          setControlLock(true, 'Robot en espera (ocupado por otro piloto)');
          document.getElementById('ws-status').innerText = 'Modo Espectador';
        }
      };
    }

    function setControlLock(locked, message = "") {
      const container = document.getElementById('controls-container');
      const banner = document.getElementById('lock-banner');
      if (locked) {
        container.classList.add('locked');
        banner.style.display = 'block';
        if (message) banner.innerText = message;
      } else {
        container.classList.remove('locked');
        banner.style.display = 'none';
      }
    }

    function sendCommand(cmd) {
      if (isMaster && websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(cmd);
      }
    }

    function attachControl(id, cmd) {
      const el = document.getElementById(id);
      const start = (e) => { e.preventDefault(); sendCommand(cmd); };
      const stop = (e) => { e.preventDefault(); sendCommand('STOP'); };

      el.addEventListener('mousedown', start);
      el.addEventListener('mouseup', stop);
      el.addEventListener('touchstart', start, { passive: false });
      el.addEventListener('touchend', stop, { passive: false });
      el.addEventListener('touchcancel', stop, { passive: false });
    }

    window.addEventListener('load', () => {
      initWebSocket();
      attachControl('btn-fwd', 'FWD');
      attachControl('btn-bwd', 'BWD');
      attachControl('btn-left', 'LEFT');
      attachControl('btn-right', 'RIGHT');

      const slider = document.getElementById('speed-slider');
      const speedVal = document.getElementById('speed-val');
      
      slider.addEventListener('input', (e) => {
        const val = e.target.value;
        speedVal.innerText = val;
        sendCommand(`SPEED:${val}`);
      });
    });
  </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  // Ignorar comandos si no provienen del piloto maestro
  if (client->id() != activeClientId) {
    return;
  }

  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;

    if (msg.startsWith("SPEED:")) {
      int newSpeed = msg.substring(6).toInt();
      currentSpeed = constrain(newSpeed, 0, 255);
      applyMovement();
    } else {
      currentCommand = msg;
      applyMovement();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      if (activeClientId == 0) {
        activeClientId = client->id();
        client->text("ROLE:MASTER");
        
        digitalWrite(LED_PIN, LOW); // LED apagado: piloto activo en línea
        currentCommand = "STOP";
        applyMovement();
        Serial.printf("[WS] Piloto asignado a ID: %u\n", activeClientId);
      } else {
        client->text("ROLE:LOCKED");
        Serial.printf("[WS] Cliente en espera ID: %u\n", client->id());
      }
      break;

    case WS_EVT_DISCONNECT:
      if (client->id() == activeClientId) {
        Serial.printf("[WS] Piloto ID: %u desconectado. Buscando sucesor...\n", activeClientId);
        
        currentCommand = "STOP";
        applyMovement();
        activeClientId = 0;

        // Búsqueda del nuevo piloto mediante auto& compatible con la librería actual
        for (auto& c : server->getClients()) {
          // c puede ser un puntero o una referencia según la variante interna
          // Usamos la comprobación universal:
          uint32_t cid = c.id();
          if (c.status() == WS_CONNECTED && cid != client->id()) {
            activeClientId = cid;
            c.text("ROLE:MASTER");
            digitalWrite(LED_PIN, LOW);
            Serial.printf("[WS] Mando transferido a cliente ID: %u\n", activeClientId);
            break;
          }
        }

        if (activeClientId == 0) {
          digitalWrite(LED_PIN, HIGH);
          Serial.println("[WS] Sin clientes conectados. Esperando nuevo piloto...");
        }
      }
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;

    default:
      break;
  }
}

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  setMotors(0, 0);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Encendido al arrancar (sin clientes)

  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Acceso: http://");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
}