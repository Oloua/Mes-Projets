# -*- coding: utf-8 -*-
"""
Created on Fri Dec  3 14:55:42 2021

@author: ladretp
"""

# Standard library imports
import numpy as np
import pandas as pd

import matplotlib.pyplot as plt


import pickle
from sklearn.model_selection import train_test_split

from sklearn.metrics import confusion_matrix




from sklearn.metrics import balanced_accuracy_score,classification_report

import utils_ClassIm as utils

# import pour la partie CNN


from tensorflow.keras.layers import Dense, Flatten, Dropout
from tensorflow.keras.utils import to_categorical

from tensorflow.keras import backend as K 

#import transfer

from tensorflow.keras.applications.vgg16 import VGG16

from tensorflow.keras.models import Model
#########################################################################""




plt.close('all')

# Main

# Partie 1.1 Recuperation de la base de données type DataFrame
database = pd.read_csv('./database.csv')
# Paramètres

# Etude sur catégorie 2 classes == 0 ou 8 classes == 1
ColLabels = 1

#remettre les données dans l'ordre des images

base_ordre=database.sort_values(by=['Numero'])

# Etude sur catégorie 2 classes ColLabels=0 ou 8 classes ColLabels= 1 uniquement
ColLabels = 1

# Partie 1.2
utils.show_database(database,1,2) # pour verifier les bases et afficher un exemple images


#Base_im,label1,label2,Caract=utils.lire_images_et_carac("./images",'./database.csv',2688,sous_ech=2)

# with open('data.tout_128', 'wb') as f:
#     pickle.dump([Base_im,label1,label2,Caract],f)

# # Pour charger toutes les données de la base "à la matlab" comme un load
# # Ca suppose que le fichier data.pickle a déjà été fait
 
with open('data.tout_128', 'rb') as f:
    Base_im, label1, label2,caract = pickle.load(f)

Base_im=np.array(Base_im)   
caract=np.array(caract) 
    
Base_im=(Base_im/255)-0.5


Base_tot=list(zip(Base_im, caract))

datatrain, datatest, datalabeltrain, datalabeltest = train_test_split(Base_tot, label2-2, test_size=0.2, random_state=None)

imtrain,caractrain=zip(*datatrain)
imtest,caractest=zip(*datatest)


mc=[]
titre=[]
report=[]
resu_acc=[]
resu_acc_class=[]
int_conf=[]




I=datatrain[0][0]
(img_width, img_height, nb_plan)=np.shape(I)

#vérification pour que les entrées du réseau soient correctes
if K.image_data_format() == 'channels_first': 
    input_shape = (3, img_width, img_height) 
else: 
    input_shape = (img_width, img_height, 3) 
    
imdata=[]
imdata2=[]
for i in np.arange(len(datalabeltrain)):
    imdata.append(datatrain[i][0])
for i in np.arange(len(datalabeltest)):   
    imdata2.append(datatest[i][0])
imdatatrain=np.array(imdata)
imdatatest=np.array(imdata2)



#############################################################################################################
# Le transfert learning
#############################################################################################################

###################################################################################################################################
# CNN definition transfert learning
# load model without classifier layers
    
base_model= VGG16(include_top=False, input_shape=input_shape) #choisir la taille qui correspond à la taille des images étudiées
#base_model = InceptionV3(weights='imagenet', include_top=False)

# add a global spatial average pooling layer
x = base_model.output
x=Flatten()(x) #pour VGG16
#x = GlobalAveragePooling2D()(x) # A la place de Flatten()
# let's add a fully-connected layer
x = Dense(500, activation='relu')(x)
x = Dropout(0.2)(x)
#x = Dense(20, activation='relu')(x)
# and a logistic layer -- On a 8 classes en sortie
predictions = Dense(8, activation='softmax')(x)


# this is the model we will train
model = Model(inputs=base_model.input, outputs=predictions)

#####################################################################################################################
#Etape de fine-Tuning

#Pour inception
#Nbr_couches_gelees=249

#Pour VGG16
Nbr_couches_gelees=18
#####################################################################################################################

for layer in model.layers[:Nbr_couches_gelees]:
    layer.trainable = False
for layer in model.layers[Nbr_couches_gelees:]:
    layer.trainable = True

# on recompile le nouveau modèle
#model.compile(optimizer='rmsprop', loss='categorical_crossentropy',metrics=['accuracy'])
model.compile(optimizer='sgd', loss='categorical_crossentropy',metrics=['accuracy'])
n_epochs=3
  #nombre epoch pour le fine-tuning 
 

H=model.fit(
            imdatatrain, to_categorical(datalabeltrain),
            epochs=n_epochs,
            validation_split=0.3, verbose=1) #ici verbose = 2 
    #pour pouvoir ressortir les informations de loss et d'accuracy 
    #dans une variable obtenue avec la methode history : H.history
print(H.history)



N = np.arange(0, n_epochs)
plt.figure()
plt.plot(N, H.history["loss"], label="train_loss")
plt.plot(N, H.history["val_loss"], label="val_loss")
plt.legend(loc="lower left")
plt.xlabel("Epoch #")
plt.ylabel("Loss")
plt.title("Training Loss on Dataset")
  

labels_predit = model.predict(imdatatest)


mc.append(confusion_matrix(datalabeltest,np.argmax(labels_predit,axis=1)))


acc=balanced_accuracy_score(datalabeltest, np.argmax(labels_predit,axis=1))
resu_acc.append(acc)
titre.append(f'Transfert avec Inception, accuracy={acc:.2f}]')
resu=classification_report(datalabeltest,np.argmax(labels_predit,axis=1),target_names=['2','3','4','5','6','7','8','9'],output_dict=True)
report.append(classification_report(datalabeltest,np.argmax(labels_predit,axis=1),target_names=['2','3','4','5','6','7','8','9'],output_dict=True))

acc_class=np.arange(8)*0.0
for i in np.arange(0,8):
    acc_class[i]=np.around(resu[str(i+2)]['precision'],3)
resu_acc_class.append(acc_class)