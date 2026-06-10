#include <Wire.h>

#define ESCLAVE_ADDR 0x08    // même adresse que sur l'ATmega

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP32 I2C Master initialisation...");

  // Initialisation I2C avec les bonnes broches : SDA = 16, SCL = 17
  Wire.begin(16, 17);            
  Serial.println("I2C maitre actif");
}

void loop() {
  uint8_t batteryPct = 0;

  // On demande 1 octet à l'esclave
  Wire.requestFrom(ESCLAVE_ADDR, (uint8_t)1);
  
  // Si un octet est dispo, on le lit
  if (Wire.available()) {
    batteryPct = Wire.read();
    Serial.print("Pourcentage batterie recu : ");
    Serial.print(batteryPct);
    Serial.println(" %");
  } else {
    Serial.println("Aucune reponse de l'esclave I2C !");
  }

  delay(5000);  // attendre 1s avant la prochaine requête
}
