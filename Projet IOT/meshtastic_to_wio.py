import serial
import meshtastic
import meshtastic.serial_interface as serial_interface
from meshtastic import mesh_pb2

# Configure ici ton port série vers le Wio Terminal
PORT_WIO = "COM4"   # adapte ce port à ton cas (Windows COM7, ou /dev/ttyACM0 sous Linux)
BAUD_WIO = 115200

# Connexion au Wio Terminal
try:
    wio = serial.Serial(PORT_WIO, BAUD_WIO, timeout=1)
    print(f"Connecté à Wio Terminal sur {PORT_WIO}")
except Exception as e:
    print(f"Erreur connexion Wio Terminal : {e}")
    exit(1)

# Connexion au module Meshtastic (en USB)
try:
    iface = serial_interface.SerialInterface()
    print("Connecté à Meshtastic")
except Exception as e:
    print(f"Erreur connexion Meshtastic : {e}")
    exit(1)

# Callback de réception de messages
def onReceive(packet, interface):
    try:
        user = packet.get('fromId', '???')
        text = packet.get('decoded', {}).get('text', '')
        if text:
            print(f"[{user}] {text}")
            wio.write(f"{user}: {text}\n".encode())
    except Exception as e:
        print(f"Erreur réception: {e}")

iface.onReceive = onReceive

print("En attente de messages Meshtastic... (Ctrl+C pour quitter)")
try:
    while True:
        pass
except KeyboardInterrupt:
    iface.close()
    wio.close()
    print("\nFermeture propre.")
