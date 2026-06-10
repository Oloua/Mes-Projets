#include "Pins.h"
#include "PinChangeInt.h"

volatile long L = 0, R = 0;

void countL(){ L++; }
void countR(){ R++; }

void setup(){
  Serial.begin(9600);

  pinMode(ENCODER_LEFT_A_PIN, INPUT);
  pinMode(ENCODER_RIGHT_A_PIN, INPUT);

  // Encodeur gauche → interruption externe OK
  attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A_PIN), countL, CHANGE);

  // Encodeur droit → PinChangeInt correct
  PCintPort::attachInterrupt(ENCODER_RIGHT_A_PIN, countR, CHANGE);
}

void loop(){
  Serial.print("L="); Serial.print(L);
  Serial.print("  R="); Serial.println(R);
  delay(200);
}
