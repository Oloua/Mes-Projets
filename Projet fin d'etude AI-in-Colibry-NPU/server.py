# -*- coding: utf-8 -*-
import asyncio
import threading
import time
import cv2
import torch
import torch.nn as nn
from torchvision import transforms
from PIL import Image
import numpy as np
import subprocess
from typing import Any
from flask import Flask, Response, jsonify
from flask_cors import CORS
from gpiozero import AngularServo, LED, Device
from gpiozero.pins.lgpio import LGPIOFactory
from picamera2 import Picamera2
from bless import (
    BlessServer,
    BlessGATTCharacteristic,
    GATTCharacteristicProperties,
    GATTAttributePermissions
)

# ==========================================
# 1. CONFIGURATION MATERIEL
# ==========================================
try:
    Device.pin_factory = LGPIOFactory()
except:
    pass

led = LED(27)
servo = AngularServo(17, min_angle=-90, max_angle=90)

# --- WEB ---
app = Flask(__name__)
CORS(app) # Indispensable pour que le navigateur accepte de discuter

# --- BLUETOOTH (UUIDs Validés ...AF99) ---
SERVICE_UUID = "A07498CA-AD5B-474E-9781-5138352BAF99"
CHAR_UUID    = "5138352B-AF96-474E-9781-5138352BAF99"

# --- VARIABLES PARTAGÉES (Le Cerveau Commun) ---
output_frame = None
lock = threading.Lock()
etat_porte = "FERMEE"
mode_auto = True  # True = IA décide | False = Humain décide
last_chicken_seen_time = 0
CONFIDENCE_THRESHOLD = 0.50

# ==========================================
# 2. IA & VISION
# ==========================================
class BlazeBlock(nn.Module):
    def __init__(self, in_channels, out_channels, stride=1):
        super().__init__()
        self.conv_dw = nn.Conv2d(in_channels, in_channels, kernel_size=5, stride=stride, padding=2, groups=in_channels)
        self.conv_pw = nn.Conv2d(in_channels, out_channels, kernel_size=1)
        self.pool = nn.MaxPool2d(2, 2)
        self.activation = nn.ReLU()
    def forward(self, x):
        x = self.conv_dw(x)
        x = self.conv_pw(x)
        x = self.pool(x)
        return self.activation(x)

class PouleDetector(nn.Module):
    def __init__(self):
        super().__init__()
        self.conv1 = nn.Conv2d(3, 24, kernel_size=5, stride=2, padding=2)
        self.block1 = BlazeBlock(24, 24)
        self.block2 = BlazeBlock(24, 48)
        self.block3 = BlazeBlock(48, 48)
        self.block4 = BlazeBlock(48, 96)
        self.block5 = BlazeBlock(96, 96)
        self.global_pool = nn.AdaptiveAvgPool2d(1)
        self.fc = nn.Linear(96, 1)
        self.sigmoid = nn.Sigmoid()
    def forward(self, x):
        x = self.conv1(x)
        x = self.block1(x)
        x = self.block2(x)
        x = self.block3(x)
        x = self.block4(x)
        x = self.block5(x)
        x = self.global_pool(x)
        x = torch.flatten(x, 1)
        x = self.fc(x)
        return self.sigmoid(x)

print("🧠 Chargement IA...")
device = torch.device("cpu")
model = PouleDetector().to(device)
try:
    model.load_state_dict(torch.load('poule_detector_best.pth', map_location=device))
    model.eval()
    print("✅ Modèle chargé !")
except:
    print("❌ ERREUR : Modèle introuvable.")
    exit()

preprocess = transforms.Compose([
    transforms.Resize((128, 128)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

# --- CAMÉRA ---
picam2 = Picamera2()
config = picam2.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
picam2.configure(config)
picam2.start()

# ==========================================
# 3. ACTIONNEURS
# ==========================================
def action_porte(action):
    global etat_porte
    if action == "OUVRIR":
        if etat_porte == "OUVERTE": return
        print("🚪 MOTEUR : OUVERTURE")
        servo.angle = 90
        led.on()
        etat_porte = "OUVERTE"
    elif action == "FERMER":
        if etat_porte == "FERMEE": return
        print("🚪 MOTEUR : FERMETURE")
        servo.angle = -90
        led.off()
        etat_porte = "FERMEE"
    time.sleep(0.5)
    servo.detach()

# ==========================================
# 4. BLUETOOTH (BLE)
# ==========================================
def start_auto_pairing_agent():
    print("🛡️ Agent Bluetooth activé...")
    subprocess.run(["sudo", "pkill", "-f", "bt-agent"], stderr=subprocess.DEVNULL)
    subprocess.Popen(
        ["sudo", "bt-agent", "--capability", "NoInputNoOutput"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    time.sleep(1)

def read_request(characteristic: BlessGATTCharacteristic, **kwargs):
    return characteristic.value

def write_request(characteristic: BlessGATTCharacteristic, value: Any, **kwargs):
    global mode_auto
    data = bytes(value)
    msg = data.decode('utf-8', errors='ignore').lower().strip()

    # --- VISUALISATION TERMINAL ---
    print("\n" + "⬇" * 30)
    print(f"📱 BLE REÇU : Hex={data.hex()} | Texte='{msg}'")

    # --- LOGIQUE DE COMMANDE (CORRIGÉE ET ROBUSTE) ---

    # 1. Changement de Mode
    if 'm' in msg or b'\x02' in data:
        print("✅ MODE : PASSAGE EN MANUEL (Synchro Web OK)")
        mode_auto = False
        print("⬆" * 30 + "\n")
        return

    elif 'a' in msg or 'auto' in msg or b'\x01' in data:
        print("✅ MODE : PASSAGE EN AUTOMATIQUE (Synchro Web OK)")
        mode_auto = True
        print("⬆" * 30 + "\n")
        return

    # 2. Action (Sécurisée)
    if mode_auto:
        print("⛔ ACTION REFUSÉE : Mode AUTO actif.")
        print("   -> Envoyez 'm' pour passer en manuel.")
    else:
        # Priorité Fermeture
        if 'f' in msg or '0' in msg or b'\x00' in data:
            print("✅ COMMANDE : FERMER")
            action_porte("FERMER")
        # Ouverture
        elif 'o' in msg or '1' in msg or b'\xff' in data:
            print("✅ COMMANDE : OUVRIR")
            action_porte("OUVRIR")
        else:
            print("❓ Commande inconnue")

    print("⬆" * 30 + "\n")
    characteristic.value = value

async def run_ble_server():
    subprocess.run(["sudo", "hciconfig", "hci0", "up"])
    start_auto_pairing_agent()

    server = BlessServer(name="PouleGate-BLE")
    server.read_request_func = read_request
    server.write_request_func = write_request

    await server.add_new_service(SERVICE_UUID)
    await server.add_new_characteristic(
        SERVICE_UUID, CHAR_UUID,
        (GATTCharacteristicProperties.read | GATTCharacteristicProperties.write | GATTCharacteristicProperties.write_without_response | GATTCharacteristicProperties.notify),
        None,
        (GATTAttributePermissions.readable | GATTAttributePermissions.writeable)
    )
    print(f"📡 Serveur BLE PRÊT (UUID ...AF99)")
    await server.start()
    while True: await asyncio.sleep(1)

def ble_thread_entry():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(run_ble_server())

# ==========================================
# 5. BOUCLE PRINCIPALE (IA + STREAMING VIDEO)
# ==========================================
def processing_thread():
    global output_frame, last_chicken_seen_time, etat_porte, mode_auto
    print("👁️ IA en cours d'analyse...")
    while True:
        try:
            frame_rgb = picam2.capture_array()
            pil_img = Image.fromarray(frame_rgb)
            input_tensor = preprocess(pil_img).unsqueeze(0).to(device)
            with torch.no_grad():
                score = model(input_tensor).item()
            is_poule = score > CONFIDENCE_THRESHOLD
            now = time.time()

            # --- Logique Automatique ---
            if is_poule:
                last_chicken_seen_time = now
                if mode_auto and etat_porte == "FERMEE":
                    print("🤖 AUTO : POULE VUE -> OUVERTURE")
                    action_porte("OUVRIR")
            elif mode_auto and etat_porte == "OUVERTE" and (now - last_chicken_seen_time) > 10.0:     
                print("🤖 AUTO : TIMEOUT -> FERMETURE")
                action_porte("FERMER")

            # --- Incrustation pour le Web ---
            frame_bgr = frame_rgb.copy()
            # Affichage des Infos sur la vidéo
            cv2.putText(frame_bgr, f"IA Score: {score:.2f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
            cv2.putText(frame_bgr, f"Mode: {'AUTO' if mode_auto else 'MANUEL'}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0) if mode_auto else (0, 0, 255), 2)
            cv2.putText(frame_bgr, f"Porte: {etat_porte}", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

            if is_poule:
                cv2.rectangle(frame_bgr, (10, 10), (630, 470), (0, 255, 0), 4)

            with lock: output_frame = frame_bgr.copy()
        except: time.sleep(0.1)

# ==========================================
# 6. ROUTES WEB (FLASK)
# ==========================================
@app.route('/video')
def video_feed():
    def generate():
        while True:
            with lock:
                if output_frame is None: continue
                _, encoded = cv2.imencode(".jpg", output_frame)
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + encoded.tobytes() + b'\r\n')      
            time.sleep(0.05)
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/status')
def get_status():
    # C'est ici que le site Web vient lire l'état actuel (modifié par le Bluetooth ou l'IA)
    return jsonify({"etat_porte": etat_porte, "mode_auto": mode_auto})

@app.route('/command/<cmd>')
def command(cmd):
    global mode_auto
    print(f"🌍 WEB REÇU : {cmd}")

    # Gestion Mode (Prioritaire)
    if cmd == "AUTO_ON":
        mode_auto = True
        return jsonify({"status": "OK", "mode_auto": True})
    elif cmd == "AUTO_OFF":
        mode_auto = False
        return jsonify({"status": "OK", "mode_auto": False})

    # SÉCURITÉ WEB (Même logique que le Bluetooth)
    if mode_auto:
        return jsonify({
            "status": "ERROR",
            "message": "Action impossible: Le poulailler est en mode AUTOMATIQUE. Passez en manuel d'abord."
        })

    # Action Manuel
    if cmd in ["OUVRIR", "OPEN"]: action_porte("OUVRIR")
    elif cmd in ["FERMER", "CLOSE"]: action_porte("FERMER")

    return jsonify({"status": "OK", "etat_porte": etat_porte})

# ==========================================
# 7. LANCEMENT
# ==========================================
if __name__ == '__main__':
    # 1. Lancer IA (Daemon pour ne pas bloquer)
    t_ia = threading.Thread(target=processing_thread, daemon=True)
    t_ia.start()

    # 2. Lancer Bluetooth (Daemon)
    t_bt = threading.Thread(target=ble_thread_entry, daemon=True)
    t_bt.start()

    # 3. Lancer Serveur Web (Bloquant - Garde le script en vie)
    print("🌍 Serveur Web prêt sur http://<IP_DU_PI>:5000")
    app.run(host='0.0.0.0', port=5000, threaded=True)