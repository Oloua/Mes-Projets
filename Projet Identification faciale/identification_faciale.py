#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Auteur ladretp
# Détection des visages présents sous la webcam et 
#enregistrement des visages détectés en sortant de la vidéo par la touche q
#

import cv2

import numpy as np

import matplotlib.pyplot as plt
import PIL as pil

import os
# this has to be set before importing tensorflow
#os.environ["TF_ENABLE_ONEDNN_OPTS"] = "0"
os.environ["TF_USE_LEGACY_KERAS"] = "1"
from deepface import DeepFace


detectors = [
  'opencv', 
  'ssd', 
  'dlib', #faut installer dlib
  'mtcnn', 
  'fastmtcnn',# faut installer facenet-pytorch
  'retinaface', #lent...
  'mediapipe', #faut installer mediapipe
  'yolov8',#faut installer pip install ultralytics
  'yunet',
  'centerface',
]

"""
cap = cv2.VideoCapture(0)
plt.show()

# Definir le codec et créer l'objet VideoWriter si on veut sauvegarder la vidéo
# pas fait ici, on ne sauvegarde que l'image finale.

while(cap.isOpened()):
    ret, frame = cap.read()
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    if ret==True:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results=DeepFace.extract_faces(img_path=frame,detector_backend=detectors[9],enforce_detection=False)
        taille=len(results)

        for i in np.arange(0,taille):
            bounding_box = results[i]['facial_area']
            x=bounding_box['x']
            y=bounding_box['y']
            w=bounding_box['w']
            h=bounding_box['h']

            cv2.rectangle(frame,
              (x, y),
              (x+w, y + h),
              (0,155,255),
              2)
           
        cv2.imshow('img',frame)
             
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    else:
        break

nbr_visage=np.shape(results)[0]

for i in np.arange(0,nbr_visage):
    plt.figure()
    plt.imshow(results[i]['face'])
    print(DeepFace.analyze(frame,enforce_detection=False))

    I=np.uint8(results[i]['face']*255)

    monIm=pil.Image.fromarray(I, mode='RGB')
    monIm.save(f'mon_visage_detect{i}.jpeg',quality=75)

cap.release()
cv2.destroyAllWindows()
"""
from os import listdir
from save_embeddings import sauvegarder_embeddings, charger_embeddings, sauvegarder_embeddings_npz, charger_embeddings_npz

def redimensionner_image(img_path, max_size=1024):
    """Redimensionne l'image si elle est trop grande, sinon beaucoup plus long à traiter"""
    img = cv2.imread(img_path)
    if img is None:
        return None
    
    height, width = img.shape[:2]
    
    # Si l'image est trop grande, la redimensionner à l'aide de cv2.resize
    if max(height, width) > max_size:
        scale = max_size / max(height, width)
        new_width = int(width * scale)
        new_height = int(height * scale)
        img = cv2.resize(img, (new_width, new_height), interpolation=cv2.INTER_AREA)
    
    return img

pictures=[]
# Directory des images où sont les visages à identifier
IMAGE_DIR = r"C:\Users\prich\Documents\Identification_faciale\base2026\base2026"

# On récupère les noms des dossiers dynamiquement au lieu de les écrire en dur
liste_employee=np.array([d for d in listdir(IMAGE_DIR) if os.path.isdir(os.path.join(IMAGE_DIR, d))])

N=len(liste_employee) 

for i in np.arange(0,N) :
 pictures.append(os.path.join(IMAGE_DIR,liste_employee[i]))
visages=dict()

for i in np.arange(0,N):
   visages[liste_employee[i]] = []
   for file in listdir(pictures[i]):
      path_img = os.path.join(pictures[i], file)
      try:
          # Redimensionner l'image d'abord
          img_resized = redimensionner_image(path_img, max_size=1024)
          
          if img_resized is not None:
              # Extraire le visage
              face_objs = DeepFace.extract_faces(img_path=img_resized, detector_backend=detectors[9], enforce_detection=False)
              
              # Pour chaque visage détecté dans l'image
              if len(face_objs) > 0:
                  face_img = face_objs[0]['face']
                  
                  # Calculer l'embedding du visage extrait
                  embedding = DeepFace.represent(img_path=face_img, model_name="VGG-Face", enforce_detection=False)[0]["embedding"]
                  visages[liste_employee[i]].append(embedding)
                  print(f"{liste_employee[i]}/{file}")
              else:
                  print(f"Aucun visage détecté: {liste_employee[i]}/{file}")
      except Exception as e:
         print(f"Erreur sur {liste_employee[i]}/{file}: {str(e)}")

print("\n=== Résumé du chargement ===")
print(f"Nombre de personnes : {len(visages)}")
print("\nDétail par personne :")
for nom, embeddings in visages.items():
    print(f"  • {nom} : {len(embeddings)} image(s)")
print("\nBase de données chargée avec succès!")

# Sauvegarder les embeddings
sauvegarder_embeddings(visages, 'embeddings_database.pkl')

# Pour charger plus tard (fait dans le code identification_temps_reel.py):
# visages = charger_embeddings('embeddings_database.pkl')

