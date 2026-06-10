# ce bou de code sert de test rapide et permet de gérer un serveur Flask pour le streaming vidéo en direct et le contrôle d'une porte via des commandes HTTP.

from flask import Flask, request, Response, jsonify
from flask_cors import CORS
import cv2

app = Flask(__name__)
CORS(app)

#                                   ###### Camera ######
camera = cv2.VideoCapture(0)

def gen():
    while True:
        ret, frame = camera.read()
        if not ret: # si la capture échoue
            continue
        _, jpeg = cv2.imencode('.jpg', frame)
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + jpeg.tobytes() + b'\r\n')

@app.route('/video')
def video(): # Flux vidéo en direct
    return Response(gen(), mimetype='multipart/x-mixed-replace; boundary=frame')

#                                   ##### Porte / Mode ##### 
door_state = "FERMEE"
auto_mode = False

@app.route('/command/<cmd>', methods=['GET'])
def command(cmd):
    global door_state, auto_mode
    cmd = cmd.upper()
    response = "OK"

    if cmd == "OPEN":
        door_state = "OUVERTE"
    elif cmd == "CLOSE":
        door_state = "FERMEE"
    elif cmd == "AUTO_ON":
        auto_mode = True
    elif cmd == "AUTO_OFF":
        auto_mode = False
    else:
        response = "COMMANDE INCONNUE"

    print(f"Commande reçue : {cmd}")
    return jsonify({
        "status": response,
        "door_state": door_state,
        "auto_mode": auto_mode
    })

@app.route('/status', methods=['GET'])
def status():  # retourne l'état actuel de la porte et du mode
    """Retourne l'état actuel de la porte et du mode"""
    return jsonify({
        "door_state": door_state,
        "auto_mode": auto_mode
    })

#                                   ##### Lancer le serveur #####
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
