// Lecture des encodeurs - sans interruption

#include <Arduino.h>
#include <ESP32Encoder.h>

// Définition des broches
const int pinA[6]     = {36, 35, 25,  4,  15, 21};
const int pinB[6]     = {39, 9,  26,  5,  18, 22};
const int CaptHall[6] = {34, 10, 27, 13, 19, 23};

// Creation des objets encodeurs
ESP32Encoder encoders[6];

// Constantes pour les encodeurs
const float degParPulse = 360.0 / (4 * 5000.0); // pour encodeurs rotatifs - degré par pulsion - quadrature ×4
const float mmParPulse  = 1.0 / 1000.0;         // pour encodeur linéaire

// Variables d’état
float angles[6]       = {0};
long lastCounts[6]    = {0};  // comptage de l'encodeur au cycle précédent
bool lastZStates[6]   = {HIGH};  // dernier état du capteur Z (HIGH ou LOW)
bool resetDone[6]     = {false};  // indique si le reset de position a déjà été effectué

void setup() {
  Serial.begin(115200);
  ESP32Encoder::useInternalWeakPullResistors = puType::up; // Activation des pull-up internes

  // Initialisation des encodeurs
  for (int i = 0; i < 6; i++) {
    encoders[i].attachFullQuad(pinA[i], pinB[i]);
    encoders[i].setFilter(1023);
    encoders[i].setCount(0);
    pinMode(CaptHall[i], INPUT_PULLUP);
  }
}

void loop() {
  // Lecture des états des capteurs Z et des encodeurs
  for (int i = 0; i < 6; i++) {
    bool currentZ = digitalRead(CaptHall[i]);
    long count = encoders[i].getCount();

    // Calcul de l'angle pour les encodeurs rotatifs
    if (i != 4) {
      angles[i] = count * degParPulse - 3.60005;  // 3.60005 est un offset pour corriger la position initiale
    }

    // detection du sens de rotation/deplacement
    String sens;
    if (count > lastCounts[i]) sens = "antihoraire";
    else if (count < lastCounts[i]) sens = "horaire";
    else sens = "aucun mouvement";

    // Remise a zero
    if (!resetDone[i]) {
      if (lastZStates[i] == LOW && currentZ == HIGH && sens == "horaire") {
        encoders[i].setCount(0);
        resetDone[i] = true;
        count = lastCounts[i] = 0;
        angles[i] = 0.0;
        Serial.printf("Encodeur %d: Front montant + sens horaire | remise à zéro\n", i+1);
      } else if (lastZStates[i] == HIGH && currentZ == LOW && sens == "antihoraire") {
        encoders[i].setCount(0);
        resetDone[i] = true;
        count = lastCounts[i] = 0;
        angles[i] = 0.0;
        Serial.printf("Encodeur %d: Front descendant + sens antihoraire | remise à zéro\n", i+1);
      }
    }

    // affichage des informations quand le compteur a changé
    if (count != lastCounts[i]) {
      if (i == 4) {
        // Encodeur linéaire
        float distMM = count * mmParPulse;
        Serial.printf("Encodeur %d | Distance: %.2f mm | Sens: %s\n", i+1, distMM, sens.c_str());
      } else {
        Serial.printf("Encodeur %d | Angle: %.3f° | Sens: %s\n", i+1, angles[i], sens.c_str());
      }
    }

    // Mise à jour des états pour le prochain cycle
    lastZStates[i] = currentZ;
    lastCounts[i] = count;
  }

  delay(10);
}






/*
//AVEC INTERRUPTION // PAS totalement FONCTIONNEL

#include <Arduino.h>
#include <ESP32Encoder.h>

#define pinA1 36
#define pinB1 39
#define pinZ1 34

// Encodeur
ESP32Encoder encoder1;
const float degParPulse = 360.0 / (4 * 5000.0);  // quadrature ×4 → 20000 increments/tour

// ISR Z
volatile uint8_t zEvent = 0;
enum { Z_NONE = 0, Z_RISING, Z_FALLING };

// Debounce hardware
volatile uint32_t lastZus = 0;
const uint32_t DEBOUNCE_US = 50000;  // 50 ms

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR handleZ() {
  uint32_t now = micros();
  if (now - lastZus < DEBOUNCE_US) return;  // ignorer les rebonds
  lastZus = now;

  bool z = digitalRead(pinZ1);
  portENTER_CRITICAL_ISR(&mux);
    zEvent = z ? Z_RISING : Z_FALLING;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);

  // Init encodeur
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder1.attachFullQuad(pinA1, pinB1);
  encoder1.setFilter(1023);
  encoder1.setCount(0);

  // Init capteur Z + interruption CHANGE
  pinMode(pinZ1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinZ1), handleZ, CHANGE);
}

void loop() {
  static long lastCount = 0;
  static bool  zHandled = false;      // one-shot flag
  static long  lastResetCount = 0;          // position du dernier reset
  const long   THRESHOLD = 1200;       // increment quadrature a dépasser avant nouveau reset - soit apres 20° environ (aimant 10mm, si aimant change le modifier)

  // on lit l'encodeur
  long count = encoder1.getCount();
  float angle = count * degParPulse;

  // determination du sens
  enum { STOP=0, CW, CCW } dir;
  if      (count >  lastCount) dir = CCW;   // antihoraire
  else if (count <  lastCount) dir = CW;    // horaire
  else                          dir = STOP;

  // recuperation et remise à zero de l’evnement Z
  uint8_t eventZ;
  portENTER_CRITICAL(&mux);
    eventZ = zEvent;
    zEvent  = Z_NONE;
  portEXIT_CRITICAL(&mux);

  // gestion du one-shot via la distance depuis lastResetCount
  if (zHandled) {
    if (labs(count - lastResetCount) > THRESHOLD) {
      zHandled = false;  // on a tourner assez -> on est donc prêt pour un nouveau reset
    }
  }
  else if (eventZ == Z_RISING && dir == CW) {
    // reset horaire (front montant)
    encoder1.setCount(0);
    Serial.println("→ Reset FRONT MONTANT (horaire)");
    zHandled = true;
    lastResetCount = 0;      // puisque setCount(0), la position de reset passe à 0
    count =  0;
    angle = 0.0;
    lastCount = 0;
  }
  else if (eventZ == Z_FALLING && dir == CCW) {
    // reset antihoraire (front descendant)
    encoder1.setCount(0);
    Serial.println("→ Reset FRONT DESCENDANT (antihoraire)");
    zHandled = true;
    lastResetCount = 0;
    count = 0;
    angle = 0.0;
    lastCount = 0;
  }

  // affichage que si mouvement
  if (count != lastCount) {
    Serial.print("Sens : ");

    if (dir == CW)
      Serial.print("horaire");
    else if (dir == CCW)
      Serial.print("antihoraire");
    else
      Serial.print("aucun mouvement");
    
    Serial.print(" | Angle : ");
    Serial.println(angle, 3);
  }

  lastCount = count;
  delay(10);
}

*/