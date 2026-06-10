alert("JS chargé");

const PI_URL = "http://172.20.10.2:5000";

let autoMode = false;

// --- Commandes porte
function openDoor() {
    sendCommand("OUVRIR");
}

function closeDoor() {
    sendCommand("FERMER");
}

function toggleMode() {
    sendCommand(!autoMode ? "AUTO_ON" : "AUTO_OFF");
}

// Envoie une commande a Flask
function sendCommand(cmd) {
    console.log("Commande envoyée :", cmd);

    // On ajoute un timestamp pour eviter le cache
    fetch(`${PI_URL}/command/${cmd}?t=${Date.now()}`)
        .then(res => res.json())
        .then(data => {
            console.log("Réponse commande :", data);

            if (data.status === "REFUSE_MODE_AUTO") {
                alert("Action impossible : Le poulailler est en mode AUTOMATIQUE.\nCliquez sur 'Mode Auto' pour repasser en manuel d'abord.");
            }
            
            // Mise à jour immédiate
            refreshStatus(); 
        })
        .catch(err => {
            console.error("Erreur fetch command :", err);
            // alert("Erreur de communication avec la Pi ! Vérifiez l'IP.");
        });
}

// Actualise l'état de la porte / mode toutes les secondes
function refreshStatus() {
    fetch(`${PI_URL}/status?t=${Date.now()}`) 
        .then(res => res.json())
        .then(data => {
            // Python renvoie "mode_auto" et "etat_porte" 
            
            autoMode = data.mode_auto; 

            const doorText = document.getElementById("door");
            const modeText = document.getElementById("mode");
            
            if(doorText) doorText.innerText = data.etat_porte;
            if(modeText) modeText.innerText = data.mode_auto ? "AUTOMATIQUE (IA)" : "Manuel";
            
            if(modeText) modeText.style.color = data.mode_auto ? "green" : "black";
        })
        .catch(err => console.error("Erreur fetch status :", err));
}

refreshStatus();
setInterval(refreshStatus, 1000);