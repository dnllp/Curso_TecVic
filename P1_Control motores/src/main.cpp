#include <Arduino.h>

// Pines del Puente H (ajusta según el cableado de tu robot)
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12

// Control de velocidad individual por motor (-255 a 255)
void setMotors(int velIzq, int velDer) {
  //  Motor Izquierdo 
  velIzq = constrain(velIzq, -255, 255);
  if (velIzq > 0) {
    analogWrite(IN1, velIzq);
    analogWrite(IN2, 0);
  } else if (velIzq < 0) {
    analogWrite(IN1, 0);
    analogWrite(IN2, abs(velIzq));
  } else {
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
  }

  //  Motor Derecho 
  velDer = constrain(velDer, -255, 255);
  if (velDer > 0) {
    analogWrite(IN3, velDer);
    analogWrite(IN4, 0);
  } else if (velDer < 0) {
    analogWrite(IN3, 0);
    analogWrite(IN4, abs(velDer));
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
  Serial.begin(115200);
}

void loop() {
  // Rutina de calibración y prueba
  setMotors(180, 180);   // Avanza a media velocidad
  Serial.println("Avanzando");
  delay(3500);

  setMotors(0, 0);       // Alto
  Serial.println("Detenido");
  delay(1000);

  setMotors(-180, -180); // Reversa
  Serial.println("Reversa");
  delay(3500);

  setMotors(0, 0);       // Alto
  Serial.println("Detenido");
  delay(1000);

  setMotors(-150, 150);  // Giro sobre su eje (izquierda)
  Serial.println("Girando izquierda");
  delay(3500);

  setMotors(0, 0);       // Alto
  Serial.println("Detenido");
  delay(2000);
  //  Completa las siguientes rutinas de prueba 

  // Completa la rutina de prueba con un giro a la derecha

  // Gira a la izquierda, moviendo solo el motor derecho hacia adelante y el izquierdo detenido

  //  Gira a la derecha, moviendo solo el motor izquierdo hacia adelante y el derecho detenido

  
}