// === ATMEGA328 en esclave I2C + mesure INA219 + LEDs + DEBUG SERIAL ===
#include <Wire.h>
#include <Adafruit_INA219.h>

// Adafruit INA219 (adresse 0x40)
Adafruit_INA219 ina219(0x40);

// LEDs
#define LED_VERT   6
#define LED_JAUNE  7
#define LED_ROUGE  8

// Adresse I2C esclave (qu’on interrogera depuis l’ESP32)
#define ESCLAVE_ADDR 0x08

// Stocke le dernier % batterie calculé
volatile uint8_t lastBatteryPct = 0;

// Quand l’ESP32 demande une donnée I2C, on renvoie lastBatteryPct
void onRequest() {
  Serial.print("Requete I2C recue -> envoi du pourcentage : ");
  Serial.print(lastBatteryPct);
  Serial.println(" %");
  Wire.write(lastBatteryPct);
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Laisse le temps au moniteur série de s’ouvrir
  Serial.println("Initialisation de l'ATmega328 + INA219...");

  // LEDs
  pinMode(LED_VERT,   OUTPUT);
  pinMode(LED_JAUNE,  OUTPUT);
  pinMode(LED_ROUGE,  OUTPUT);

  // I2C esclave
  Wire.begin(ESCLAVE_ADDR);
  Wire.onRequest(onRequest);
  Serial.println("I2C esclave actif à l'adresse 0x08");

  // Initialisation de l’INA219
  if (!ina219.begin()) {
    Serial.println("Erreur : INA219 non détecté !");
    digitalWrite(LED_VERT, HIGH);
    digitalWrite(LED_ROUGE, HIGH);
    while (1); // Blocage
  }
  Serial.println("INA219 detecte et initialise correctement.");
}

uint8_t mesureBatterie() {
  float shunt  = ina219.getShuntVoltage_mV();
  float bus    = ina219.getBusVoltage_V() * 1000.0f;
  float loadmV = bus + shunt;
  const int VMIN = 4600, VMAX = 8400;
  int pct = (int)(100.0f * (loadmV - VMIN) / (VMAX - VMIN));
  pct = constrain(pct, 0, 100);

  Serial.print("Mesure batterie : ");
  Serial.print(loadmV);
  Serial.print(" mV -> ");
  Serial.print(pct);
  Serial.println(" %");

  return pct;
}

void loop() {
  lastBatteryPct = mesureBatterie();

  // Mise à jour des LEDs 
  if (lastBatteryPct > 60) {
    digitalWrite(LED_VERT,  HIGH);
    digitalWrite(LED_JAUNE, HIGH);
    digitalWrite(LED_ROUGE, HIGH);
    Serial.println("LEDs : VERT+JAUNE+ROUGE allumees");
  }
  else if (lastBatteryPct > 30) {
    digitalWrite(LED_VERT,  LOW);
    digitalWrite(LED_JAUNE, HIGH);
    digitalWrite(LED_ROUGE, HIGH);
    Serial.println("LEDs : JAUNE+ROUGE allumees");
  }
  else {
    digitalWrite(LED_VERT,  LOW);
    digitalWrite(LED_JAUNE, LOW);
    digitalWrite(LED_ROUGE, HIGH);
    Serial.println("LEDs : ROUGE seule allumee");
  }

  delay(1000);
}
