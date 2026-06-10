#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Identification faciale en temps réel avec embeddings
#

import cv2
import numpy as np
from deepface import DeepFace
from save_embeddings import charger_embeddings
import os
from concurrent.futures import ThreadPoolExecutor

os.environ["TF_USE_LEGACY_KERAS"] = "1"

# Détecteurs disponibles
detectors = [
  'opencv', 
  'ssd', 
  'dlib',
  'mtcnn', 
  'fastmtcnn',
  'retinaface',
  'mediapipe',
  'yolov8',
  'yunet',
  'centerface',
]

def calculer_similarite_cosinus(emb1, emb2):
    """Calcule la similarité cosinus entre deux embeddings"""
    return np.dot(emb1, emb2) / (np.linalg.norm(emb1) * np.linalg.norm(emb2))

def identifier_visage(embedding_inconnu, visages_db, seuil=0.40):
    """
    Identifie un visage en comparant son embedding avec la base de données
    
    Args:
        embedding_inconnu: embedding du visage à identifier
        visages_db: dictionnaire {nom: [liste_embeddings]}
        seuil: seuil de similarité pour accepter une identification
        
    Returns:
        (nom_identifié, score_confiance) ou (None, 0)
    """
    meilleur_score = -1
    meilleur_nom = None
    
    for nom, liste_embeddings in visages_db.items():
        for emb_ref in liste_embeddings:
            similarite = calculer_similarite_cosinus(embedding_inconnu, emb_ref)
            
            if similarite > meilleur_score:
                meilleur_score = similarite
                meilleur_nom = nom
    
    # Retourner le résultat seulement si le score dépasse le seuil
    if meilleur_score >= seuil:
        return meilleur_nom, meilleur_score
    else:
        return "Inconnu", meilleur_score

def traiter_visage(face_obj, visages_db, idx):
    """Traite un visage individuellement"""
    try:
        bbox = face_obj['facial_area']
        x, y, w, h = bbox['x'], bbox['y'], bbox['w'], bbox['h']
        
        face_img = face_obj['face']
        embedding_result = DeepFace.represent(
            img_path=face_img,
            model_name="VGG-Face",
            enforce_detection=False
        )
        
        if embedding_result:
            embedding = embedding_result[0]["embedding"]
            nom, score = identifier_visage(embedding, visages_db, seuil=0.40)
            
            return {
                'idx': idx,
                'bbox': (x, y, w, h),
                'nom': nom,
                'score': score
            }
    except Exception as e:
        print(f"Erreur traitement visage: {e}")
    return None

def main():
    # Charger la base de données d'embeddings
    print("Chargement de la base de données")
    visages_db = charger_embeddings('embeddings_database.pkl')
    
    if not visages_db:
        print("Erreur: Base de données vide. Exécutez d'abord identification_faciale.py pour créer la base de données")
        return
    
    # Initialiser la webcam
    cap = cv2.VideoCapture(0)
    
    if not cap.isOpened():
        print("Erreur: Impossible d'ouvrir la webcam")
        return
    
    print("\n=== Identification en temps réel ===")
    print("Appuyez sur 'q' pour quitter")
    print("Appuyez sur 's' pour sauvegarder l'image actuelle")
    
    # Variables pour optimiser les performances
    frame_count = 0
    process_every_n_frames = 10  # Traiter 1 frame sur 10
    last_identifications = {}  # Cache des dernières identifications
    
    DETECTOR_BACKEND = detectors[8]
    
    # ThreadPoolExecutor pour le multithreading
    executor = ThreadPoolExecutor(max_workers=os.cpu_count())
    
    while cap.isOpened():
        ret, frame = cap.read()
        
        if not ret:
            break
        
        frame_count += 1
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # Ne traiter qu'une frame sur N pour améliorer les performances
        if frame_count % process_every_n_frames == 0:
            try:
                # Détecter les visages
                face_objs = DeepFace.extract_faces(
                    img_path=frame_rgb, 
                    detector_backend=DETECTOR_BACKEND,
                    enforce_detection=False
                )
                
                # Traiter tous les visages en parallèle
                futures = [executor.submit(traiter_visage, face_obj, visages_db, idx) 
                          for idx, face_obj in enumerate(face_objs)]
                
                # Récupérer les résultats
                last_identifications = {}
                for future in futures:
                    result = future.result()
                    if result:
                        last_identifications[result['idx']] = result
                
            except Exception as e:
                print(f"Erreur de traitement: {e}")
        
        # Afficher les résultats (même sur les frames non traitées)
        for idx, info in last_identifications.items():
            x, y, w, h = info['bbox']
            nom = info['nom']
            score = info['score']
            
            # Couleur du rectangle selon l'identification
            if nom == "Inconnu":
                couleur = (0, 0, 255)  # Rouge
            else:
                couleur = (0, 255, 0)  # Vert
            
            # Dessiner le rectangle
            cv2.rectangle(frame, (x, y), (x + w, y + h), couleur, 2)
            
            # Afficher le nom et le score
            texte = f"{nom} ({score:.2f})"
            cv2.putText(
                frame,
                texte,
                (x, y - 10),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                couleur,
                2
            )
        
        # Afficher le frame
        cv2.imshow('Identification Faciale', frame)
        
        # Gestion des touches
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('s'):
            cv2.imwrite('capture_identification.jpg', frame)
            print("Image sauvegardée: capture_identification.jpg")
    
    # Libérer les ressources
    executor.shutdown(wait=True)
    cap.release()
    cv2.destroyAllWindows()
    print("\nIdentification terminée.")

if __name__ == "__main__":
    main()