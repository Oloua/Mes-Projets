#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Identification faciale avec classification SVM
#

import cv2
import numpy as np
import os
from os import listdir
from deepface import DeepFace
from sklearn.svm import SVC
from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix, ConfusionMatrixDisplay
import matplotlib.pyplot as plt
import pickle

os.environ["TF_USE_LEGACY_KERAS"] = "1"

def redimensionner_image(img_path, max_size=1024):
    """Redimensionne l'image si elle est trop grande"""
    img = cv2.imread(img_path)
    if img is None:
        return None
    
    height, width = img.shape[:2]
    
    if max(height, width) > max_size:
        scale = max_size / max(height, width)
        new_width = int(width * scale)
        new_height = int(height * scale)
        img = cv2.resize(img, (new_width, new_height), interpolation=cv2.INTER_AREA)
    
    return img

# Directory des images
IMAGE_DIR = r"C:\Users\prich\Documents\Identification_faciale\base2026\base2026"

# Récupérer les noms des personnes
pictures = np.array([d for d in listdir(IMAGE_DIR) if os.path.isdir(os.path.join(IMAGE_DIR, d))])
N = len(pictures)

print(f"Chargement de {N} personnes...")

# Extraire les embeddings et les labels
X_embeddings = []  # Vecteurs de caractéristiques
y_labels = []      # Labels (noms des personnes)

for i, nom_personne in enumerate(pictures):
    chemin_personne = os.path.join(IMAGE_DIR, nom_personne)
    
    for file in listdir(chemin_personne):
        path_img = os.path.join(chemin_personne, file)
        
        try:
            # Redimensionner et extraire le visage
            img_resized = redimensionner_image(path_img, max_size=1024)
            
            if img_resized is not None:
                face_objs = DeepFace.extract_faces(
                    img_path=img_resized, 
                    detector_backend='yunet', 
                    enforce_detection=False
                )
                
                if len(face_objs) > 0:
                    face_img = face_objs[0]['face']
                    
                    # Calculer l'embedding (vecteur de caractéristiques)
                    embedding = DeepFace.represent(
                        img_path=face_img, 
                        model_name="VGG-Face", 
                        enforce_detection=False
                    )[0]["embedding"]
                    
                    X_embeddings.append(embedding)
                    y_labels.append(nom_personne)
                    
                    print(f"{nom_personne}/{file}")
                else:
                    print(f"Aucun visage: {nom_personne}/{file}")
                    
        except Exception as e:
            print(f"Erreur {nom_personne}/{file}: {str(e)}")

# Convertir en arrays numpy
X = np.array(X_embeddings)
y = np.array(y_labels)

print(f"\n=== Dataset ===")
print(f"Nombre total d'images: {len(X)}")
print(f"Dimension des embeddings: {X.shape[1]}")
print(f"Nombre de classes: {len(np.unique(y))}")

print("\nRépartition par personne:")
unique, counts = np.unique(y, return_counts=True)
for nom, count in zip(unique, counts):
    print(f"  • {nom}: {count} images")

# Encoder les labels en nombres
label_encoder = LabelEncoder()
y_encoded = label_encoder.fit_transform(y)

# Diviser en train/test (80/20)
X_train, X_test, y_train, y_test = train_test_split(X, y_encoded, test_size=0.2, stratify=y_encoded)

print(f"\n=== Split ===")
print(f"Training set: {len(X_train)} images")
print(f"Test set: {len(X_test)} images")

# Entraîner le classifieur SVM
print("\n=== Entraînement du SVM ===")
svm_classifier = SVC(kernel='linear', C=1.0, probability=True, random_state=42)
svm_classifier.fit(X_train, y_train)

# Évaluer sur le test set
y_pred = svm_classifier.predict(X_test)
accuracy = np.mean(y_pred == y_test)

print(f"\n=== Résultats ===")
print(f"Accuracy: {accuracy * 100:.2f}%")

print("\nClassification Report:")
print(classification_report(y_test, y_pred, target_names=label_encoder.classes_))

# Afficher et sauvegarder la matrice de confusion
cm = confusion_matrix(y_test, y_pred)
disp = ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=label_encoder.classes_)

fig, ax = plt.subplots(figsize=(12, 10))
disp.plot(ax=ax, cmap='Blues', values_format='d')
plt.title('Matrice de Confusion - Identification Faciale', fontsize=16, pad=20)
plt.xlabel('Classe Prédite', fontsize=12)
plt.ylabel('Classe Réelle', fontsize=12)
plt.xticks(rotation=45, ha='right')
plt.tight_layout()
plt.savefig('matrice_confusion.png', dpi=300, bbox_inches='tight')
print("\nMatrice de confusion sauvegardée: matrice_confusion.png")
plt.show()

# Sauvegarder le modèle et le label encoder
print("\n=== Sauvegarde ===")
with open('svm_model_confusion.pkl', 'wb') as f:
    pickle.dump(svm_classifier, f)
print("Modèle SVM sauvegardé: svm_model_confusion.pkl")

with open('label_encoder_confusion.pkl', 'wb') as f:
    pickle.dump(label_encoder, f)
print("Label encoder sauvegardé: label_encoder_confusion.pkl")

# Sauvegarder aussi les embeddings bruts (pour compatibilité)
visages_dict = {}
for nom in pictures:
    visages_dict[nom] = []

for embedding, label in zip(X_embeddings, y_labels):
    visages_dict[label].append(embedding)

with open('embeddings_database_confusion.pkl', 'wb') as f:
    pickle.dump(visages_dict, f)
print("Embeddings sauvegardés: embeddings_database_confusion.pkl")

print("\nEntraînement terminé avec succès!")

