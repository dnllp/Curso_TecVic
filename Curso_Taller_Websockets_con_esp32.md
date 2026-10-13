# Taller Práctico: Control de Robot Móvil Diferencial mediante WebSockets con ESP32

**Duración estimada:** 3 Horas  
**Entorno de desarrollo:** Visual Studio Code + PlatformIO  
**Hardware base:** Chasis diferencial 2WD preensamblado con ESP32 y Puente H (L298N / TB6612FNG).

---

## Índice de Contenidos

1. [Módulo 0: ¿Qué es WebSockets? (Ejemplo Python + HTML)](#módulo-0-qué-es-websockets-ejemplo-python--html)
2. [Configuración Inicial de PlatformIO](#configuración-inicial-de-platformio)
3. [Práctica 1: Tracción y Control Motor Diferencial (Sin WebSockets)](#práctica-1-tracción-y-control-motor-diferencial-sin-websockets)
4. [Práctica 2: Fundamentos de WebSockets en ESP32 (Control de LED)](#práctica-2-fundamentos-de-websockets-en-esp32-control-de-led)
5. [Práctica 3: Bidireccionalidad y Telemetría en Tiempo Real](#práctica-3-bidireccionalidad-y-telemetría-en-tiempo-real)
6. [Práctica 4: Control Remoto del Robot Móvil (4 Botones + Slider)](#práctica-4-control-remoto-del-robot-móvil-4-botones--slider)
7. [Práctica 5: Control Exclusivo con Transferencia de Mando (Single Pilot)](#práctica-5-control-exclusivo-con-transferencia-de-mando-single-pilot)

---

## Módulo 0: ¿Qué es WebSockets? (Ejemplo Python + HTML)

### Objetivo Teórico
Comprender la diferencia entre el modelo tradicional **HTTP (Petición - Respuesta)** y el protocolo **WebSockets (Conexión Full-Duplex persistente)**. En HTTP, el cliente debe solicitar periódicamente datos al servidor (*polling*). Con WebSockets, se establece un único canal TCP bidireccional donde cliente y servidor pueden transmitirse paquetes en cualquier instante con una latencia insignificante.

```
       HTTP Tradicional                  WebSocket (Full-Duplex)
   Cliente           Servidor           Cliente           Servidor
      |                 |                  |                 |
      |--- GET /data -->|                  |--- Handshake -->|
      |<-- HTML Resp ---|                  |<-- 101 Switch --|
      |                 |                  |=================|
      |--- GET /data -->|                  |<--- Evento -----|  (Servidor envía solo)
      |<-- Data Resp ---|                  |---- Comando --->|  (Cliente envía solo)
```

### 1. Servidor en Python (`server.py`)
Requiere la librería `websockets`. Si no la tienes instalada en tu equipo:
```bash
pip install websockets
```

Guarda y ejecuta el siguiente script:

```python
import asyncio
import websockets

async def echo(websocket):
    print(f"[NUEVA CONEXIÓN] Cliente conectado desde {websocket.remote_address}")
    try:
        async for message in websocket:
            print(f"[MENSAJE RECIBIDO] {message}")
            # Respuesta instantánea sobre el mismo canal
            await websocket.send(f"Servidor Python confirma: '{message}' procesado.")
    except websockets.exceptions.ConnectionClosed:
        print("[DESCONEXIÓN] Cliente desconectado.")

async def main():
    async with websockets.serve(echo, "localhost", 8765):
        print("Servidor WebSocket escuchando en ws://localhost:8765")
        await asyncio.Future()  # Mantener servidor en ejecución

if __name__ == "__main__":
    asyncio.run(main())
```

### 2. Cliente Web (`index.html`)
Abre este archivo directamente en cualquier navegador:

```html
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Prueba de Concepto: WebSockets</title>
  <style>
    body { font-family: sans-serif; margin: 40px; background: #f0f2f5; }
    .card { background: white; padding: 20px; border-radius: 8px; max-width: 500px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
    input { width: 70%; padding: 8px; margin-right: 5px; }
    button { padding: 8px 16px; cursor: pointer; }
    #log { margin-top: 15px; padding: 10px; background: #1e1e24; color: #00ff88; border-radius: 4px; font-family: monospace; height: 120px; overflow-y: auto; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Demostración WebSockets</h2>
    <input type="text" id="msgInput" placeholder="Escribe un mensaje...">
    <button onclick="sendData()">Enviar</button>
    <div id="log"></div>
  </div>

  <script>
    const logBox = document.getElementById('log');
    const socket = new WebSocket('ws://localhost:8765');

    socket.onopen = () => {
      logBox.innerHTML += `<div>[INFO] Conectado al servidor WebSocket.</div>`;
    };

    socket.onmessage = (event) => {
      logBox.innerHTML += `<div>[RX] ${event.data}</div>`;
      logBox.scrollTop = logBox.scrollHeight;
    };

    socket.onclose = () => {
      logBox.innerHTML += `<div>[INFO] Desconectado.</div>`;
    };

    function sendData() {
      const input = document.getElementById('msgInput');
      if (socket.readyState === WebSocket.OPEN && input.value.trim() !== '') {
        socket.send(input.value);
        input.value = '';
      }
    }
  </script>
</body>
</html>
```

---

## Configuración Inicial de PlatformIO

Crea un proyecto en VS Code con PlatformIO para el microcontrolador ESP32:

* **Name:** `Taller_Robot_WS`
* **Board:** `Espressif ESP32 Dev Module` (o tu modelo específico)
* **Framework:** `Arduino`

Abre el archivo `platformio.ini` y utiliza la versión mantenida de `ESPAsyncWebServer` para evitar fallos de compatibilidad con las versiones modernas del core de Arduino:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = 
    mathieucarbou/ESPAsyncWebServer @ ^3.1.5
    mathieucarbou/AsyncTCP @ ^3.1.4
```

---

## Práctica 1: Tracción y Control Motor Diferencial (Sin WebSockets)

### Objetivo
Diseñar y validar la capa de hardware y potencia. Se implementa la función `setMotors(int leftSpeed, int rightSpeed)` que recibe rangos de **-255 a 255** para gobernar dirección y velocidad de cada oruga/rueda de forma independiente.

### Diagrama de Conexiones Habitual
* `IN1` -> GPIO 26
* `IN2` -> GPIO 27
* `IN3` -> GPIO 14
* `IN4` -> GPIO 12
* Masa/GND del puente H conectada a masa/GND del ESP32.

### Código (`src/main.cpp`)

```cpp
#include <Arduino.h>

// Definición de Pines del Puente H
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12

// Control de velocidad individual (-255 a 255)
void setMotors(int leftSpeed, int rightSpeed) {
  // --- Motor Izquierdo ---
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

  // --- Motor Derecho ---
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

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Inicializar detenidos
  setMotors(0, 0);
}

void loop() {
  // 1. Marcha adelante a media velocidad
  setMotors(180, 180);
  delay(1500);

  // 2. Alto
  setMotors(0, 0);
  delay(1000);

  // 3. Marcha atrás
  setMotors(-180, -180);
  delay(1500);

  // 4. Alto
  setMotors(0, 0);
  delay(1000);

  // 5. Giro cerrado (sobre su propio eje)
  setMotors(-160, 160);
  delay(1000);

  // 6. Alto
  setMotors(0, 0);
  delay(2000);
}
```

---

## Práctica 2: Fundamentos de WebSockets en ESP32 (Control de LED)

### Objetivo
Configurar el ESP32 en modo **Punto de Acceso Wi-Fi (AP)** independiente y habilitar un servidor HTTP asíncrono con un *endpoint* WebSocket para encender y apagar el LED integrado (`GPIO 2`) desde un smartphone.

### Código (`src/main.cpp`)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LED_PIN 2

const char* ssid = "ESP32_Taller_LED";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control de LED - WebSockets</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; user-select: none; }
    .btn { padding: 15px 30px; font-size: 18px; border: none; border-radius: 8px; margin: 10px; cursor: pointer; }
    .btn-on { background-color: #28a745; color: white; }
    .btn-off { background-color: #dc3545; color: white; }
    #status { color: #666; font-size: 14px; margin-top: 20px; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>Control LED con WebSockets</h2>
  <button class="btn btn-on" onclick="sendCmd('LED_ON')">Encender LED</button>
  <button class="btn btn-off" onclick="sendCmd('LED_OFF')">Apagar LED</button>
  <div id="status">Conectando...</div>

  <script>
    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = () => { document.getElementById('status').innerText = 'Conectado al ESP32'; };
      websocket.onclose = () => {
        document.getElementById('status').innerText = 'Desconectado. Reconectando...';
        setTimeout(initWebSocket, 2000);
      };
    }

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
  Serial.print("Red Wi-Fi: ");
  Serial.println(ssid);
  Serial.print("Ingresa a: http://");
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
```

---

## Práctica 3: Bidireccionalidad y Telemetría en Tiempo Real

### Objetivo
Demostrar la capacidad dúplex simultánea:
1. **Cliente → Servidor:** El navegador controla el estado del actuador (LED).
2. **Servidor → Cliente:** El ESP32 mide periódicamente su temperatura interna con `temperatureRead()` y hace *broadcast* hacia los clientes sin que la página web deba solicitarlo.

### Código (`src/main.cpp`)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LED_PIN 2

const char* ssid = "ESP32_Telemetria_AP";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastTelemetry = 0;
const unsigned long telemetryInterval = 1000;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Telemetría en Vivo</title>
  <style>
    body { font-family: sans-serif; text-align: center; background: #1a1a24; color: white; margin: 0; padding: 20px; user-select: none; }
    .card { background: #282838; border-radius: 12px; padding: 20px; max-width: 320px; margin: 20px auto; }
    .val { font-size: 44px; font-weight: bold; color: #00d2d3; margin: 10px 0; }
    .btn { padding: 12px 24px; font-size: 16px; border: none; border-radius: 6px; margin: 5px; cursor: pointer; }
    .btn-on { background: #10ac84; color: white; }
    .btn-off { background: #ee5253; color: white; }
    #status { color: #888; font-size: 13px; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>Telemetría y Control Bidireccional</h2>

  <div class="card">
    <small>TEMPERATURA INTERNA ESP32</small>
    <div class="val"><span id="temp">--</span> °C</div>
  </div>

  <div class="card">
    <small>CONTROL DE ACTUADOR</small><br><br>
    <button class="btn btn-on" onclick="sendCmd('LED_ON')">ON</button>
    <button class="btn btn-off" onclick="sendCmd('LED_OFF')">OFF</button>
  </div>

  <div id="status">Conectando...</div>

  <script>
    document.addEventListener('contextmenu', e => e.preventDefault());
    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = () => { document.getElementById('status').innerText = 'Canal WebSocket Abierto'; };
      websocket.onclose = () => {
        document.getElementById('status').innerText = 'Desconectado. Reconectando...';
        setTimeout(initWebSocket, 2000);
      };
      
      // Recepción asíncrona de telemetría (Servidor -> Cliente)
      websocket.onmessage = (event) => {
        let msg = event.data;
        if (msg.startsWith("TEMP:")) {
          document.getElementById('temp').innerText = msg.split(":")[1];
        }
      };
    }

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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    if (message == "LED_ON") digitalWrite(LED_PIN, HIGH);
    else if (message == "LED_OFF") digitalWrite(LED_PIN, LOW);
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
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();

  // Envío no bloqueante cada 1 segundo
  if (millis() - lastTelemetry >= telemetryInterval) {
    lastTelemetry = millis();
    float t = temperatureRead();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "TEMP:%.1f", t);
    ws.textAll(buffer);
  }
}
```

---

## Práctica 4: Control Remoto del Robot Móvil (4 Botones + Slider)

### Objetivo
Integrar la tracción diferencial con la interfaz de control táctil.
* Los botones activan movimiento continuo mientras se mantengan pulsados (`touchstart` / `mousedown`).
* Al soltar (`touchend` / `mouseup`), se emite de forma inmediata `STOP`.
* Un control deslizante (*slider*) ajusta la velocidad en tiempo real de 100 a 255.
* El menú contextual está desactivado globalmente para prevenir que el navegador abra menús al mantener presionado.
* **LED Testigo:** Comienza encendido (esperando conexión), se apaga cuando el cliente se conecta, y se enciende cada vez que se pulsa un botón de dirección.

### Código (`src/main.cpp`)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12
#define LED_PIN 2

const char* ssid = "Robot_Movil_AP";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int currentSpeed = 200;
String currentCommand = "STOP";

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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Control Robot Móvil</title>
  <style>
    * { -webkit-touch-callout: none; -webkit-user-select: none; user-select: none; box-sizing: border-box; }
    body { font-family: Arial, sans-serif; text-align: center; background: #1e1e24; color: #fff; margin: 0; padding: 15px; touch-action: none; }
    .slider-box { background: #2b2b36; border-radius: 12px; padding: 15px; max-width: 320px; margin: 15px auto; }
    .slider { -webkit-appearance: none; width: 100%; height: 12px; border-radius: 6px; background: #444456; outline: none; touch-action: pan-x; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 28px; height: 28px; border-radius: 50%; background: #4caf50; cursor: pointer; }
    .grid { display: grid; grid-template-columns: repeat(3, 85px); grid-gap: 15px; justify-content: center; margin: 25px auto; }
    .btn { width: 85px; height: 85px; font-size: 30px; border: none; border-radius: 14px; background: #3f51b5; color: white; touch-action: manipulation; }
    .btn:active { background: #1a237e; }
    .status { font-size: 13px; color: #888; margin-top: 10px; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>Control del Robot</h2>

  <div class="slider-box">
    <div style="display:flex; justify-content:space-between; margin-bottom:8px;">
      <span>Velocidad:</span>
      <span id="speed-val">200</span>
    </div>
    <input type="range" min="100" max="255" value="200" class="slider" id="speed-slider">
  </div>

  <div class="grid">
    <div></div><button class="btn" id="btn-fwd">&#9650;</button><div></div>
    <button class="btn" id="btn-left">&#9664;</button><div></div><button class="btn" id="btn-right">&#9654;</button>
    <div></div><button class="btn" id="btn-bwd">&#9660;</button><div></div>
  </div>

  <div class="status" id="ws-status">Conectando...</div>

  <script>
    document.addEventListener('contextmenu', e => e.preventDefault());
    let gateway = `ws://${window.location.hostname}/ws`;
    let websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = () => { document.getElementById('ws-status').innerText = 'Conectado al Robot'; };
      websocket.onclose = () => {
        document.getElementById('ws-status').innerText = 'Desconectado. Reconectando...';
        setTimeout(initWebSocket, 2000);
      };
    }

    function sendCommand(cmd) {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
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
      slider.addEventListener('input', (e) => {
        document.getElementById('speed-val').innerText = e.target.value;
        sendCommand(`SPEED:${e.target.value}`);
      });
    });
  </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    if (msg.startsWith("SPEED:")) {
      currentSpeed = constrain(msg.substring(6).toInt(), 0, 255);
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
      digitalWrite(LED_PIN, LOW);
      currentCommand = "STOP";
      applyMovement();
      break;
    case WS_EVT_DISCONNECT:
      currentCommand = "STOP";
      applyMovement();
      digitalWrite(LED_PIN, HIGH);
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
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
  digitalWrite(LED_PIN, HIGH);

  WiFi.softAP(ssid, password);
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
```

---

## Práctica 5: Control Exclusivo con Transferencia de Mando (Single Pilot)

### Objetivo
Evitar conflictos cuando varios alumnos se conectan a la misma red Wi-Fi:
1. **Solo el primer cliente conectado tiene el control (`ROLE:MASTER`).**
2. Los clientes que se conecten después entran en **modo espectador** (`ROLE:LOCKED`), atenuando su pantalla y deshabilitando botones.
3. Si el cliente maestro apaga su Wi-Fi o cierra la página, el robot se detiene de inmediato por seguridad, busca automáticamente al siguiente cliente conectado en la lista y le transfiere el mando (`ROLE:MASTER`) sin necesidad de recargar la página.

### Código (`src/main.cpp`)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12
#define LED_PIN 2

const char* ssid = "Robot_Movil_AP";
const char* password = "tecvictoria";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

uint32_t activeClientId = 0;          // ID del piloto con control exclusivo
int currentSpeed = 200;
String currentCommand = "STOP";

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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Control Robot Móvil - Single Pilot</title>
  <style>
    * { -webkit-touch-callout: none; -webkit-user-select: none; user-select: none; box-sizing: border-box; }
    body { font-family: Arial, sans-serif; text-align: center; background: #1e1e24; color: #fff; margin: 0; padding: 15px; touch-action: none; }
    .slider-box { background: #2b2b36; border-radius: 12px; padding: 15px; max-width: 320px; margin: 15px auto; }
    .slider { -webkit-appearance: none; width: 100%; height: 12px; border-radius: 6px; background: #444456; outline: none; touch-action: pan-x; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 28px; height: 28px; border-radius: 50%; background: #4caf50; cursor: pointer; }
    .grid { display: grid; grid-template-columns: repeat(3, 85px); grid-gap: 15px; justify-content: center; margin: 25px auto; }
    .btn { width: 85px; height: 85px; font-size: 30px; border: none; border-radius: 14px; background: #3f51b5; color: white; touch-action: manipulation; }
    .btn:active { background: #1a237e; }
    .status { font-size: 13px; color: #888; margin-top: 10px; }
    .locked { opacity: 0.30; pointer-events: none; }
    .banner { display: none; background: #c0392b; color: white; padding: 10px; border-radius: 8px; margin: 10px auto; max-width: 320px; font-weight: bold; }
  </style>
</head>
<body oncontextmenu="return false;">
  <h2>Control del Robot</h2>

  <div id="lock-banner" class="banner">Robot en uso por otro piloto</div>

  <div id="controls-container">
    <div class="slider-box">
      <div style="display:flex; justify-content:space-between; margin-bottom:8px;">
        <span>Velocidad:</span>
        <span id="speed-val">200</span>
      </div>
      <input type="range" min="100" max="255" value="200" class="slider" id="speed-slider">
    </div>

    <div class="grid">
      <div></div><button class="btn" id="btn-fwd">&#9650;</button><div></div>
      <button class="btn" id="btn-left">&#9664;</button><div></div><button class="btn" id="btn-right">&#9654;</button>
      <div></div><button class="btn" id="btn-bwd">&#9660;</button><div></div>
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
      websocket.onopen = () => { document.getElementById('ws-status').innerText = 'Conectado. Verificando rol...'; };
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
          setControlLock(true, 'Robot en espera (en uso por otro piloto)');
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
      slider.addEventListener('input', (e) => {
        document.getElementById('speed-val').innerText = e.target.value;
        sendCommand(`SPEED:${e.target.value}`);
      });
    });
  </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  // Descartar comandos de clientes que no sean el piloto asignado
  if (client->id() != activeClientId) return;

  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    if (msg.startsWith("SPEED:")) {
      currentSpeed = constrain(msg.substring(6).toInt(), 0, 255);
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
        digitalWrite(LED_PIN, LOW);
        currentCommand = "STOP";
        applyMovement();
      } else {
        client->text("ROLE:LOCKED");
      }
      break;

    case WS_EVT_DISCONNECT:
      if (client->id() == activeClientId) {
        currentCommand = "STOP";
        applyMovement();
        activeClientId = 0;

        // Búsqueda del sucesor dentro de los clientes en línea
        for (auto& c : server->getClients()) {
          uint32_t cid = c.id();
          if (c.status() == WS_CONNECTED && cid != client->id()) {
            activeClientId = cid;
            c.text("ROLE:MASTER");
            digitalWrite(LED_PIN, LOW);
            break;
          }
        }

        if (activeClientId == 0) {
          digitalWrite(LED_PIN, HIGH); // Sin clientes: modo espera
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
  digitalWrite(LED_PIN, HIGH);

  WiFi.softAP(ssid, password);
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
```

---

## Solución de Problemas Frecuentes

1. **El motor zumba y no gira a bajas velocidades:** Los motores TT con reductora tienen una zona muerta de fricción estática; no uses valores de PWM inferiores a 100.
2. **Una rueda gira al revés:** Invierte los pines de control correspondientes en el código (`IN1` por `IN2` o `IN3` por `IN4`) o invierte los cables en el borne del puente H.
3. **Pérdida de paquetes en el navegador móvil:** Asegúrate de que el teléfono permanezca conectado a la red Wi-Fi del ESP32 y no salte a datos móviles al detectar que la red "No tiene conexión a Internet" (desactivar datos móviles en el celular durante la práctica si ocurre este problema).