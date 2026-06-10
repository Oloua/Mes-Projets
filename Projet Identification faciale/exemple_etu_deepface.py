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


cap = cv2.VideoCapture(0)
plt.show()

# Definir le codec et créer l'objet VideoWriter si on veut sauvegarder la vidéo
# pas fait ici, on ne sauvegarde que l'image finale.

while(cap.isOpened()):
    ret, frame = cap.read()
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    if ret==True:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results=DeepFace.extract_faces(img_path=frame,detector_backend=detectors[1],enforce_detection=False)
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
