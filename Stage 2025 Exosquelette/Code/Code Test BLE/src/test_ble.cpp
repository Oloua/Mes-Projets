// test d'envoi des données des codeurs via BLE
#include <Arduino.h>
#include <ESP32Encoder.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

// Bornes encodeurs
#define pinA1 36
#define pinB1 39
#define pinZ1 34

#define pinA2 35
#define pinB2 9
#define pinZ2 10

ESP32Encoder encoder1;
ESP32Encoder encoder2;

// resolution : 2*5000 pulse/tour
const float degParPulse = 360.0 / (2.0 * 5000.0);

// Flag et debounce
volatile bool resetFlag1 = false;
volatile bool resetFlag2 = false;
volatile uint32_t lastReset1 = 0;
volatile uint32_t lastReset2 = 0;

// BLE callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer)   { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer){ deviceConnected = false; }
};

// ISR pour Z1 (avec remise à zéro compteur)
void IRAM_ATTR resetAngle1() {
  uint32_t now = millis();
  if (now - lastReset1 < 50) return;  // debounce 50 ms
  lastReset1 = now;
  encoder1.setCount(0);
  resetFlag1 = true;
}

// ISR pour Z2 (avec remise à zéro compteur)
void IRAM_ATTR resetAngle2() {
  uint32_t now = millis();
  if (now - lastReset2 < 50) return;  // debounce 50 ms
  lastReset2 = now;
  encoder2.setCount(0);
  resetFlag2 = true;
}

void afficherAngles(float a1, float a2) {
  Serial.print("Codeur 1 : ");
  Serial.print(a1, 4);
  Serial.print("° | Codeur 2 : ");
  Serial.print(a2, 4);
  Serial.println("°");
}

void setup() {
  Serial.begin(115200);

  // --- Initialisation BLE
  BLEDevice::init("ESP32_Angle_BLE");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEDevice::getAdvertising()->start();
  Serial.println("BLE prêt, en attente de connexion…");

  // codeurs
  ESP32Encoder::useInternalWeakPullResistors = puType::up;  // Pull-up interne
  encoder1.attachHalfQuad(pinA1, pinB1);  // Codeur 1
  encoder2.attachHalfQuad(pinA2, pinB2);
  encoder1.setFilter(1023);  // Filtre anti-rebond
  encoder2.setFilter(1023);
  encoder1.setCount(0);  // Initialisation compteur codeur 1
  encoder2.setCount(0);

  // Broches Z en pull-up interne
  pinMode(pinZ1, INPUT_PULLUP);  // Broche Z1
  pinMode(pinZ2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinZ1), resetAngle1, FALLING);  // Interruption sur front descendant
  attachInterrupt(digitalPinToInterrupt(pinZ2), resetAngle2, FALLING);
}

void loop() {
  // Lecture  des codeurs
  long count1 = encoder1.getCount();  // Compteur codeur 1
  long count2 = encoder2.getCount();

  // Angle codeur 1
  float angle1 = count1 * degParPulse;  // Conversion en degrés
  // Angle codeur 2
  float angle2 = count2 * degParPulse;

  afficherAngles(angle1, angle2);  // Affichage sur le moniteur série

  // Détection du passage par Z1
  if (resetFlag1) {resetFlag1 = false;}
  // Détection du passage par Z2
  if (resetFlag2) {resetFlag2 = false;}

  // Envoi BLE si connecté
  if (deviceConnected) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Codeur1=%.4f° | Codeur2=%.4f°", angle1, angle2);
    pCharacteristic->setValue(msg);
    pCharacteristic->notify();
  }

  delay(25);
}
