#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/pcnt.h>
#include <limits.h>     // pour INT16_MAX, INT16_MIN
#include <string>

// ----------------------- ** GLOBAL VARIABLE DEFINITIONS ** -----------------------

// #define DEBUG  // Uncomment this line to enable debug mode                       

// ------------------------ BATTERY MANAGEMENT -------------------------------------

// Define I2C pins used to communicate with the INA219 power monitor
#define I2C_SDA 16
#define I2C_SCL 17

#define ESCLAVE_ADDR 0x08   // même adresse que sur l’ATmega

// Initialize I2C communication on Wire bus 0
TwoWire I2CBME = TwoWire(1);

// Battery voltage thresholds (in millivolts)
#define VOLTAGE_MAX 8400
#define VOLTAGE_MIN 5000

float filteredVoltage = VOLTAGE_MIN;
float filterConstant = 0.1; // The smoothing factor for the low-pass filter


// ----------------------- I2C SCAN FUNCTION -------------------------------------
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;



// This function scans the I2C bus for devices and prints their addresses
void scanI2C(TwoWire &wirePort) {
  ina219.begin();
  byte error, address;
  int count = 0;
  Serial.println("Scanning I2C devices...");
  for(address = 1; address < 127; address++ ) {
    wirePort.beginTransmission(address);
    error = wirePort.endTransmission();
    if (error == 0) {
      if (address == ESCLAVE_ADDR) {
        Serial.print("ATmega trouvé à 0x");
      }
      else if (address == 0x40) {
        Serial.print("INA219 trouvé à 0x");
      }
      else {
        Serial.print("I2C device found at 0x");
      }
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      count++;
    }
  }
  if (count == 0)
    Serial.println("No I2C devices found.");
}

// ----------------------- BLUETOOTH LOW ENERGY  -----------------------

// Global pointer to the BLE server
BLEServer *pServer = nullptr;

// Custom BLE GATT Service and Characteristic UUIDs
// You can generate your own UUIDs at: https://www.uuidgenerator.net/
#define SERVICE_UUID           "52d1987a-22ef-4edc-b61a-0e5c6fb74c5f" // Custom service
#define CHARACTERISTIC_UUID_TX "e38bc399-2c3b-4787-96c7-b0b0ea1b0da7" // Notify characteristic (device -> client)
#define CHARACTERISTIC_UUID_RX "a4c47587-7ff9-41d0-a9e3-90c85c41c80a" // Write characteristic (client -> device)

// Standard BLE GATT Service and Characteristic UUIDs
#define BATTERY_SERVICE_UUID      "180F" // Standard Battery Service UUID
#define BATTERY_LEVEL_CHAR_UUID   "2A19" // Standard Battery Level Characteristic UUID

BLEService *pBatteryService = nullptr;
BLECharacteristic *pBatteryLevelCharacteristic = nullptr;

// BLE characteristics setup
BLECharacteristic commandCharacteristics(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
BLECharacteristic gonioCharacteristics(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

// Descriptor to enable notifications on the client side (standard CCCD descriptor)
BLEDescriptor gonioDescriptor(BLEUUID((uint16_t)0x2902)); // Client Characteristic Configuration Descriptor

// BLE device name to be advertised
#define bleServerName "Gonio_BLE"

// Flags to manage BLE connection state
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool advertisingStarted = false;

// Buffers for message handling (received from BLE client)
String message;
String received_message;

// ----------------------- REALTIME PARAMETERS  ------------------------
unsigned long TimeT;       // Current timestamp
unsigned long Time0;       // Timestamp at start of measurement
unsigned long K = 1;       // The variable K is an integer and is called discrete time. It is initialized to 1.
unsigned long TCycle = 20; // Sampling time in milliseconds; 100 ms is stable, while shorter intervals like 20 ms may cause issues
float TimeK;               // Duration since measurement start (in seconds or milliseconds as needed)

// ----------------------- ENCODER (PCNT) SETUP -----------------------

// Definition des broches
const int pinA[6]     = {36, 35, 25,  4,  15, 21};
const int pinB[6]     = {39, 9,  26,  5,  18, 22};
const int CaptHall[6] = {34, 10, 27, 13, 19, 23};

// PCNT unit and channel per encoder
pcnt_unit_t pcnt_unit[6] = {PCNT_UNIT_0, PCNT_UNIT_1, PCNT_UNIT_2, PCNT_UNIT_3, PCNT_UNIT_4, PCNT_UNIT_5};

// Constants
const float degParPulse = 360.0 / (4 * 5000.0);  // 0.018 deg per pulse (assuming 5000 pulses per revolution)
const float mmParPulse  = 0.01;             // 1 mm per pulse (assuming 10 mm per revolution)

float offsetsDeg[6] = {
  3.60005f,  // codeur 1
  3.60005f,  // codeur 2
  3.60005f,  // codeur 3
  3.60005f,  // codeur 4
  0.0f,      // codeur 5 (codeur linéaire, pas de décalage)
  3.60005f   // codeur 6
};

// Variables d’état
float angles[6]       = {0};
long lastCounts[6]    = {0};  // Last counts for each encoder
bool lastZStates[6]   = {HIGH};  // Last Z index states for each encoder
bool resetDone[6]     = {false};  // Indicates if the Z index reset has been done for each encoder
long startCounts[6] = {0};  // Start counts for each encoder (used for initial offset)


volatile long zeroCount[6] = {0};  // Count of Z index resets for each encoder

void IRAM_ATTR onZ5() {  // Interrupt handler for encoder 5 Z index
  if (!resetDone[4]) {
    // remet a zero le compteur PCNT 4
    pcnt_counter_clear(pcnt_unit[4]);   // Clear the counter for PCNT 4
    zeroCount[4] = 0;
    resetDone[4] = true; // Indique que le reset a été effectué
    detachInterrupt(digitalPinToInterrupt(CaptHall[4]));
  }
}

// Initialize the PCNT units for all encoders
void initPCNT() {
  for (int i = 0; i < 6; i++) {
    pcnt_config_t pcnt_config = {
      // configure the first channel : pulse = A, ctrl = B
      .pulse_gpio_num = pinA[i],
      .ctrl_gpio_num  = pinB[i],
      .lctrl_mode     = PCNT_MODE_KEEP,
      .hctrl_mode     = PCNT_MODE_REVERSE,
      .pos_mode       = PCNT_COUNT_INC,  // Front montant sur A
      .neg_mode       = PCNT_COUNT_DEC,  // Front descendant sur A
      .counter_h_lim  = INT16_MAX,   // 32767
      .counter_l_lim  = INT16_MIN,   // -32768
      .unit           = pcnt_unit[i],
      .channel        = PCNT_CHANNEL_0
    };
    pcnt_unit_config(&pcnt_config);  // Configure the first channel for A-B counting
    // configure second channel : pulse = B, ctrl = A
    pcnt_config.pulse_gpio_num = pinB[i];
    pcnt_config.ctrl_gpio_num  = pinA[i];
    // ici on inverse pos/neg pour que le second canal compte dans l'autre sens
    pcnt_config.pos_mode       = PCNT_COUNT_DEC;
    pcnt_config.neg_mode       = PCNT_COUNT_INC;
    pcnt_config.unit           = pcnt_unit[i];
    pcnt_config.channel        = PCNT_CHANNEL_1;

    pcnt_unit_config(&pcnt_config); // Configure the second channel for B-A counting

    pcnt_counter_pause(pcnt_unit[i]);
    pcnt_counter_clear(pcnt_unit[i]);
    pcnt_counter_resume(pcnt_unit[i]);

    if (i == 4) {
      // pour l'encodeur 5, on attache l'interruption sur le Z index
      attachInterrupt(
        digitalPinToInterrupt(CaptHall[4]),
        onZ5,
        RISING
      );
    }
    pcnt_counter_resume(pcnt_unit[i]);  // Start counting
    pinMode(CaptHall[i], INPUT_PULLUP);  // Set the Z index pin as input with pull-up resistor
  }
}

// Function to get the current count from a specific encoder
// Returns the count as an unsigned 32-bit integer
uint32_t getCount(int idx) {
  int16_t count;
  pcnt_get_counter_value(pcnt_unit[idx], &count);
  return count;
}

// -------------- ** CLASS AND FUNCTION DECLARATIONS  ** --------------- 

// ------------------------ BATTERY MANAGEMENT  ------------------------ 

// Reads voltage from the INA219 sensor and calculates battery percentage
uint8_t getBatteryLevel() {
  uint8_t batteryPct = 0;
  
  // Check if the I2C bus is initialized and the device is connected
  I2CBME.beginTransmission(ESCLAVE_ADDR);
  if (I2CBME.endTransmission() != 0) {
    Serial.println("Erreur I2C : ATmega n'a pas répondu !");
    return 0;
  }
  // Request the battery percentage from the ATmega
  if (I2CBME.requestFrom((uint8_t)ESCLAVE_ADDR, (uint8_t)1) == 0) {
    Serial.println("Erreur I2C : requestFrom n'a rien reçu !");
    return 0;
  }

  if (I2CBME.available()) {  // Check if data is available
    batteryPct = I2CBME.read();                // lecture du pourcentage
  } else {
    Serial.println("Erreur I2C : pas de réponse de l’ATmega !");
  }

  Serial.print("\n\nNiveau batterie recu de l’ATmega : ");
  Serial.print(batteryPct);
  Serial.println(" %");

  return batteryPct; // Return the battery percentage
}


// ----------------------- BLUETOOTH LOW ENERGY  ----------------------- 

// Security handler: manages pairing and encryption callbacks
class MySecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest(){ return 199012; }
  void onPassKeyNotify(uint32_t pass_key) {}
  bool onConfirmPIN(uint32_t pass_key){ return true; }
  bool onSecurityRequest(){ return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl){}
};

// Server callbacks to monitor BLE client connection state
// Called when a BLE client connects or disconnects
class MyServerCallbacks: public BLEServerCallbacks { 
  void onConnect(BLEServer* pServer) override { deviceConnected = true; oldDeviceConnected = false; } 
  void onDisconnect(BLEServer* pServer) override { deviceConnected = false; } 
};

// Callback class to handle received BLE write commands
// Called when the client writes a message to the RX characteristic
class MyCommandCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    received_message = String(val.c_str());
    Serial.println(received_message);
  }
};

// ----------------------- ** SETUP FUNCTION  ** ----------------------- 

void setup() {
  // ------------->> SERIAL COMMUNICATION FOR DEBUGGING ----------------
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate

  // ------------->> BATTERY MONITORING SETUP (INA219) -----------------

  // Initialize the I2C communication pins for the INA219 sensor
  I2CBME.begin(I2C_SDA, I2C_SCL, 100000); // SDA = GPIO16, SCL = GPIO17, 100 kHz
  scanI2C(I2CBME);                // <-- on doit avoir 0x40
  
  // Attendre 500 ms pour que l’ATmega ait le temps de mesurer la batterie
  delay(500);

  // Initialize INA219 sensor on custom I2C interface
  I2CBME.beginTransmission(0x40);
  if (I2CBME.endTransmission() != 0)
    Serial.println("Failed to find INA219 chip by ATMEGA");
  else
    Serial.println("INA219 initialization successful by ATEMGA");

  // Optional: Uncomment one of the following to set a more précise current/voltage range
  // ina219.setCalibration_32V_1A();       // Higher precision on current
  // ina219.setCalibration_16V_400mA();    // Higher precision on voltage and current


  // --------------->> BLUETOOTH LOW ENERGY (BLE) SETUP ----------------
  BLEDevice::init(bleServerName); // Initialize BLE and set the device name

  // Set up BLE security (encryption and authentication)
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new MySecurity());

  // Create the BLE server and assign server-level connection callbacks
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // ----------------- Custom Goniometer Service Setup -----------------
  // Create a custom service using a custom UUID for the Goniometer
  BLEService *gonioService = pServer->createService(SERVICE_UUID);
  
  // Add a notify characteristic to the Goniometer service
  gonioService->addCharacteristic(&gonioCharacteristics);
  gonioDescriptor.setValue("PCNT"); // Optional descriptor label
  gonioCharacteristics.addDescriptor(&gonioDescriptor);

  // Add a write characteristic for receiving client commands
  gonioService->addCharacteristic(&commandCharacteristics);
  commandCharacteristics.setCallbacks(new MyCommandCallbacks());
  
  // Start the Goniometer service
  gonioService->start();
  advertisingStarted = true;

  // ------------- Battery Service Setup (Standard UUIDs) --------------
  // Create the standard Battery Service
  pBatteryService = pServer->createService(BLEUUID(BATTERY_SERVICE_UUID));
  
  // Create a Battery Level characteristic with read and notify properties
  pBatteryLevelCharacteristic = pBatteryService->createCharacteristic(
    BLEUUID(BATTERY_LEVEL_CHAR_UUID),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  // Add CCCD descriptor to allow clients to enable notifications
  pBatteryLevelCharacteristic->addDescriptor(new BLE2902()); // Enable notifications
  
  // Set initial battery level as a single-byte value
  uint8_t level = getBatteryLevel();
  pBatteryLevelCharacteristic->setValue(&level,1);

  // Start the Battery Service
  pBatteryService->start();

  // --------------------------- Advertising ---------------------------
  pServer->getAdvertising()->start();

  // --------------------- Security Configuration ----------------------
  BLESecurity *pSecurity = new BLESecurity();
  
  // Set a fixed passkey for pairing
  uint32_t passkey = 199012;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));

  // Set authentication mode to Secure Connections with MITM protection and bonding
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT); // Device has display output
  pSecurity->setKeySize(16); // Maximum encryption key size

  // Accept all authenticated connections (not restricted to just specified ones)
  uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));

  // Set response key mask for encryption and identity keys
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  
  Serial.println("BLE setup complete. Waiting for client connection...");

  // ---------------------->> module pcnt setup ------------------------
  initPCNT();

  Serial.printf("\n");

  // ----------------------- FLASH MEMORY INFO -----------------------------
  // Print flash memory information
  uint32_t flashChipSize = ESP.getFlashChipSize();
  Serial.printf("Flash physique : %u bytes (%.2f MB)\n", flashChipSize, flashChipSize / (1024.0 * 1024.0));

  // Free sketch space available for the firmware
  uint32_t sketchSpace = ESP.getFreeSketchSpace();
  Serial.printf("Espace libre pour le sketch : %u bytes (%.2f MB)\n", sketchSpace, sketchSpace / (1024.0 * 1024.0));

  // Used sketch size
  uint32_t sketchSize = ESP.getSketchSize();
  Serial.printf("Taille du sketch utilisé : %u bytes (%.2f MB)\n", sketchSize, sketchSize / (1024.0 * 1024.0));

  // Calculate and print flash usage percentage
  float usagePercent = (sketchSize * 100.0) / (sketchSize + sketchSpace);
  Serial.printf("Utilisation du flash pour le code : %.2f%%\n", usagePercent);
  
  Serial.printf("\n");

  // ----------------------- SERIAL COMMANDS -----------------------------
  Serial.println("Envoyez une commande par le moniteur série :");
  Serial.println(" - Init");
  Serial.println(" - Check");
  Serial.println(" - Start");
}

void detectAndResetZIndexes() {
  // pour gerer l'encodeur 5
  if (resetDone[4]) {
    resetDone[4] = false;  // une seule notification
    // Remise a zero (deja fait en onZ5), on affiche juste
    Serial.println("Encodeur 5: reset Z détecté (interruption)");
    
    String tmp5 = String("UDPVAL 5 changed!");
    char buf5[60];
    tmp5.toCharArray(buf5, sizeof(buf5));
    gonioCharacteristics.setValue(buf5);
    gonioCharacteristics.notify();
    delay(1);
  }

  for (int i = 0; i < 6; i++) {
    if (i == 4) continue;  // on saute l'encodeur 5, interrupt gere deja    
    bool z = digitalRead(CaptHall[i]);
    long count = getCount(i);
    String sens;
    if (count > lastCounts[i]) sens = "horaire";
    else if (count < lastCounts[i]) sens = "antihoraire";
    else sens = "aucun";

    if (!resetDone[i]) {  // Check if reset is not done
      if ((lastZStates[i] == LOW && z == HIGH && sens == "antihoraire") ||
          (lastZStates[i] == HIGH && z == LOW && sens == "horaire")) {
        pcnt_counter_clear(pcnt_unit[i]);
        resetDone[i] = true;
        lastCounts[i] = 0;
        Serial.printf("Encodeur %d: reset Z détecté (%s)\n", i+1, sens.c_str());

        String temp = String("UDPVAL ") + String(i+1) + " changed!"; //notif BLE ou UDP
        char Buf[60];
        temp.toCharArray(Buf, sizeof(Buf));
        gonioCharacteristics.setValue(Buf);
        gonioCharacteristics.notify();
        delay(1);

        detachInterrupt(digitalPinToInterrupt(CaptHall[i])); // detachement de l'interruption sur Z
      }
    }
    lastCounts[i]  = count;
    lastZStates[i] = z;
  }
}

// ----------------------- ** LOOP FUNCTION  ** ------------------------ 

void loop() {

  // Handle serial commands  -------------------------------------------------------------------------  A decommenter si test via moniteur série
  
  /*
  if (Serial.available()) {
    received_message = Serial.readStringUntil('\n');
    received_message.trim(); 
    Serial.print("Commande reçue via série : ");
    Serial.println(received_message);

    // on force la connexion BLE
    deviceConnected = true;
    oldDeviceConnected = false;
  }
  */

  // Handle BLE connection  -------------------------------------------------------------------------
  if (deviceConnected && !oldDeviceConnected) {
    advertisingStarted = false;

    // Respond to received BLE commands
    if (received_message == "Init") {
      received_message = "Wait";
    
      #ifdef DEBUG
        Serial.println("Init mode ...");
        Serial.println(received_message);
      #endif

      // Check INA219 by ATMEGA
      I2CBME.beginTransmission(0x40);
      if (I2CBME.endTransmission() != 0) {
        Serial.println("Failed to find INA219 chip");
      } else {
        #ifdef DEBUG
          Serial.println("Check INA219 chip OK");
        #endif
        uint8_t level = getBatteryLevel();
        pBatteryLevelCharacteristic->setValue(&level, 1);
        pBatteryLevelCharacteristic->notify();  // Optional: only if notifications enabled by client
      }

      // Reset all PCNT counters and Hall sync
      for (int i = 0; i < 6; i++) {
        pcnt_counter_clear(pcnt_unit[i]);
        resetDone[i]    = false;
        lastCounts[i]   = 0;
        lastZStates[i]  = digitalRead(CaptHall[i]);
      }

      // Checks connection status of a PCNT and logs the result
      for (int i = 1; i <= 6; i++) {
        String temp = String("PCNT ") + String(i) + " CheckDevice OK";
        char Buf[60];
        temp.toCharArray(Buf, sizeof(Buf));
        gonioCharacteristics.setValue(Buf);
        gonioCharacteristics.notify();
        delay(1);
      }

      // Wait for user input to set initial offsets
      int rest = 6;
      while (rest > 0) {
        detectAndResetZIndexes();

        // calcul du nombre de codeur encore en attente
        rest = 0;
        for (int i = 0; i < 6; i++)
          if (!resetDone[i])
            rest++;

        // Lecture moniteur serie en prio
        if (Serial.available()) {
          String cmd = Serial.readStringUntil('\n');
          cmd.trim();
          received_message = cmd; // quitter la boucle pour traiter la nouvelle commande
          break;
        }
        
        delay(10);
      }
      
      // Notify the client that the encoders are ready
      for (int i = 1; i <= 6; i++) {
        String temp = String("PCNT ") + String(i) + " CheckDevice OK";
        char Buf[60];
        temp.toCharArray(Buf, sizeof(Buf));
        gonioCharacteristics.setValue(Buf);
        gonioCharacteristics.notify();
        delay(1);
      }

      delay(1);
      String temp;

      if (rest == 0) {
        temp = "Initializing encoders terminated!";
      }
      else {
        temp = "Encoders initialization terminated without success!";
      }
      
      char BufFin[60];
      temp.toCharArray(BufFin, 60);
      gonioCharacteristics.setValue(BufFin);
      gonioCharacteristics.notify();

      delay(100);
    }  // <-- ferme if (received_message == "Init")

    // si on est toujours en Wait après Init, on renvoie la notif
    if (received_message == "Wait") {
      delay(1);
      String temp = "Initializing encoders terminated!";
      char BufFin[60];
      temp.toCharArray(BufFin, 60);
      gonioCharacteristics.setValue(BufFin);
      gonioCharacteristics.notify();
      delay(100);
    }

    if (received_message == "Check") {
      delay(2000); // Allow time for GUI to stabilize connection

      received_message = "Wait";
      
      #ifdef DEBUG
        Serial.println("Check mode ...");
        Serial.println(received_message);
      #endif

      for (int i = 0; i < 6; i++) {
        long count = getCount(i) - startCounts[i];
        float value;
        if (i == 4) {
          value = count * mmParPulse;     //codeur linéaire en mm
        } else {
          // en degré avec application du décalage propre à chaque codeur
          value = count * degParPulse;
          if (resetDone[i]) {
            value -= offsetsDeg[i];
          }
        }

        String temp = String("Enc") + String(i+1) + ": " + String(value, 3) + ((i == 4) ? " mm" : " deg");

        // Affichage série
        Serial.println(temp);
        
        // Envoi BLE
        char Buf[60];
        temp.toCharArray(Buf, sizeof(Buf));
        gonioCharacteristics.setValue(Buf);
        gonioCharacteristics.notify();
        delay(1);
      }
      
      {
        // Notification de fin de check
        String temp = "Checking encoders terminated!";
        char Buff[60];
        temp.toCharArray(Buff, sizeof(Buff));
        gonioCharacteristics.setValue(Buff);
        gonioCharacteristics.notify();
        #ifdef DEBUG
          Serial.println(temp);
        #endif
        delay(100);
      }
    }

    if (received_message == "Start") {
      delay(2000);  // Allow time for GUI to stabilize connection
      K = 1;
      received_message = "StartMeasurements";
      #ifdef DEBUG
        Serial.println(received_message);
      #endif
    }

    if (K == 1) {
      delay(2000);  // Allow time for GUI to stabilize connection
      Time0 = millis();
    }

    if (received_message == "StartMeasurements") {
      TimeK = float(K-1)*TCycle/1000;

      static char timeChar[10];
      dtostrf(TimeK, 9, 3, timeChar);

      TimeT = millis() - Time0;

      char a1[10], a2[10], a3[10], a4[10], d5[10], a6[10];
      for (int i = 0; i < 6; i++) {
        long count = getCount(i);
        float v = (i == 4) ? count * mmParPulse : count * degParPulse;
        dtostrf(v, 9, 2, 
            (i == 0 ? a1 :
             i == 1 ? a2 :
             i == 2 ? a3 :
             i == 3 ? a4 :
             i == 4 ? d5 : a6)
        );  
      }  

      String s1 = "a";
      String s2 = ",";

      //pour respecter le format de la chaîne à envoyer
      String temp = s1 + s2 + String(timeChar) + s2 + String(a1) + s2 + String(a2) + s2 + String(a3) + s2 + String(a4) + s2 + String(d5) + s2 + String(a6);

      char BufMes[60];
      temp.toCharArray(BufMes, sizeof(BufMes));

      gonioCharacteristics.setValue(BufMes);
      gonioCharacteristics.notify();

      // Battery update every 1000 samples
      if ((K % 1000) == 0){
        uint8_t level = getBatteryLevel();
        pBatteryLevelCharacteristic->setValue(&level, 1);
        pBatteryLevelCharacteristic->notify();   // Optional: only if notifications enabled by client
      }

      delay(1);  // bluetooth stack will go into congestion, si trop de paquets : précaution

      TimeT = millis() - Time0;

      String warning = "";
      if (TimeT > K*TCycle) {
        warning = "  Sampling time trop petit...";
      }
      else{
        while (millis() - Time0 < K * TCycle){
          // Waiting loop for real time application
        }
        Serial.println();
      }

      { // Print the current encoder values to the serial monitor
        String line = "";
        for (int i = 0; i < 6; i++) {
          long count = getCount(i);
          
          // calcul et application de l’offset initial pour chaque codeur
          float raw = (i == 4)
            ? count * mmParPulse
            : count * degParPulse;
          float value = (i == 4)
            ? raw  // pas d’offset pour le linéaire
            : raw - offsetsDeg[i]; //si non, application de l’offset

          String unit  = (i == 4) ? " mm" : " deg";
          line += "Enc" + String(i+1) + " = " + String(value, 2) + unit;
          if (i < 5) line += "  ";
        }
        line += warning;             // Add warning if sampling time is too small
        Serial.print('\r');
        Serial.print(line);
        Serial.print("     ");
      }

      K++;
    }
  } // end if (deviceConnected && !oldDeviceConnected)

  // Handle BLE disconnection and restart advertising
  if (!deviceConnected && !advertisingStarted) {
    delay(500);  // Allow BLE stack cleanup
    pServer->startAdvertising();
    #ifdef DEBUG
      Serial.println("Restarting BLE advertising"); 
    #endif
    oldDeviceConnected = true;
    advertisingStarted = true;  // Prevent re-execution
    K = 1;
  }

  // Handle BLE reconnection
  if (deviceConnected && oldDeviceConnected) {
    oldDeviceConnected = false;
    advertisingStarted = false;
    // Optional: perform setup after new connection
  }
}
