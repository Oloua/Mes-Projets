import torch
import torch.nn as nn

# Chargement du modèle
# Définition de la classe PouleDetector
model = PouleDetector()
model.load_state_dict(torch.load("poule_detector_best.pth"))
model.eval() # mode évaluation car on ne veut pas que les paramètres soient mis à jour

# Configuration de la quantification (Pour mobile/ARM, on utilise 'qnnpack')
model.qconfig = torch.quantization.get_default_qconfig('qnnpack')

# Préparation du modèle (Insertion des observateurs)
# On fusionne souvent Conv+ReLU pour gagner en vitesse (facultatif mais recommandé)
torch.quantization.prepare(model, inplace=True)

# Phase de Calibration (CRUCIAL)
# Il faut passer quelques vraies images pour que le modèle ajuste ses échelles
print("Calibration en cours...")
# (Remplace ceci par une boucle avec notre dataset de validation)
for i in range(50):
    input_data = torch.randn(1, 3, 128, 128) # Simulation d'une image
    model(input_data)

# Conversion finale
quantized_model = torch.quantization.convert(model, inplace=True)

# Sauvegarde du nouveau modèle léger
torch.save(quantized_model.state_dict(), "poule_detector_quantized.pth")
print("Modèle quantifié sauvegardé !")