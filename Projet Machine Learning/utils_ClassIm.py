# -*- coding: utf-8 -*-
"""
Created in 2019
@author: BELLET ALEXANDRE
Guideline followed PEP-0008
"""

import numpy as np
import pandas as pd
from enum import Enum
import time
import os
from skimage.transform import resize 

from tensorflow.keras.preprocessing.image import load_img, img_to_array

class Labels(Enum):
    ARTIFICIEL = 0
    NATUREL = 1 
    COTE = 2
    FORET = 3
    AUTOROUTE = 4
    VILLE = 5
    MONTAGNE = 6
    OPEN_COUNTRY = 7
    RUE = 8
    GRANDBATIMENT = 9

def get_labels():
    lst = []
    for name,value in Labels.__members__.items():
        lst.append(name)
    return lst

def set_labels(d,col2,col8):
    data = np.array(d, dtype=object)
    data[data[:,col2]==0,col2] = Labels.ARTIFICIEL.name
    data[data[:,col2]==1,col2] = Labels.NATUREL.name    
    data[data[:,col8]==2,col8] = Labels.COTE.name
    data[data[:,col8]==3,col8] = Labels.FORET.name
    data[data[:,col8]==4,col8] = Labels.AUTOROUTE.name
    data[data[:,col8]==5,col8] = Labels.VILLE.name
    data[data[:,col8]==6,col8] = Labels.MONTAGNE.name
    data[data[:,col8]==7,col8] = Labels.OPEN_COUNTRY.name
    data[data[:,col8]==8,col8] = Labels.RUE.name
    data[data[:,col8]==9,col8] = Labels.GRANDBATIMENT.name
    return pd.DataFrame(data,columns=d.columns)

def unset_labels(d,col2,col8):
    data = np.array(d, dtype=object)
    data[data[:,col2]=='ARTIFICIEL',col2] = Labels.ARTIFICIEL.value
    data[data[:,col2]=='NATUREL',col2] = Labels.NATUREL.value    
    data[data[:,col8]=='COTE',col8] = Labels.COTE.value
    data[data[:,col8]=='FORET',col8] = Labels.FORET.value
    data[data[:,col8]=='AUTOROUTE',col8] = Labels.AUTOROUTE.value
    data[data[:,col8]=='VILLE',col8] = Labels.VILLE.value
    data[data[:,col8]=='MONTAGNE',col8] = Labels.MONTAGNE.value
    data[data[:,col8]=='OPEN_COUNTRY',col8] = Labels.OPEN_COUNTRY.value
    data[data[:,col8]=='RUE',col8] = Labels.RUE.value
    data[data[:,col8]=='GRANDBATIMENT',col8] = Labels.GRANDBATIMENT.value
    return pd.DataFrame(data,columns=d.columns)

record_time = np.zeros((10,1))
def start_time(index):
    record_time[index] = time.time()
    
def stop_time(index):
    record_time[index] = time.time() - record_time[index]
    return str(float(record_time[index]))[0:7]

def shake_database(path):
    database = pd.read_csv(path)
    database = database.sample(frac=1)
    database.to_csv(path,index=False)
    
def show_database(data,col2,col8):
    data = np.array(data)
    print("Contenu Total: "+str(len(data)))
    print("Classée Artificielle: "+str(len(data[data[:,col2]==Labels.ARTIFICIEL.name,col2])))
    print("Classée Naturelle: "+str(len(data[data[:,col2]==Labels.NATUREL.name])))
    print("Classée Côte: "+str(len(data[data[:,col8]==Labels.COTE.name])))
    print("Classée Forêt: "+str(len(data[data[:,col8]==Labels.FORET.name])))
    print("Classée Autoroute: "+str(len(data[data[:,col8]==Labels.AUTOROUTE.name])))
    print("Classée Ville: "+str(len(data[data[:,col8]==Labels.VILLE.name])))
    print("Classée Montagne: "+str(len(data[data[:,col8]==Labels.MONTAGNE.name])))
    print("Classée Paysage Ouvert: "+str(len(data[data[:,col8]==Labels.OPEN_COUNTRY.name])))
    print("Classée Rue: "+str(len(data[data[:,col8]==Labels.RUE.name])))
    print("Classée Grand Batiment: "+str(len(data[data[:,col8]==Labels.GRANDBATIMENT.name])))
    print(" ")
    X=[len(data[data[:,col2]==Labels.ARTIFICIEL.name,col2]), len(data[data[:,col2]==Labels.NATUREL.name])]
    Y=[len(data[data[:,col8]==Labels.COTE.name]),len(data[data[:,col8]==Labels.FORET.name]),len(data[data[:,col8]==Labels.AUTOROUTE.name]),len(data[data[:,col8]==Labels.VILLE.name]),len(data[data[:,col8]==Labels.MONTAGNE.name]),len(data[data[:,col8]==Labels.OPEN_COUNTRY.name]),len(data[data[:,col8]==Labels.RUE.name]),len(data[data[:,col8]==Labels.GRANDBATIMENT.name])]
    
    return X,Y

def lire_images(root_dir, path_cvs ,num, sous_ech=2):
    T=[]
    C1=np.zeros(num+1)
    C2=np.zeros(num+1)
    print('Lecture base')
    for i in np.arange(0,num):
        n_Im, classe1, classe2, gabor =lire_database(path_cvs,i)
        img_name = os.path.join(root_dir,str(n_Im)+".jpg")
        I=img_to_array(load_img(img_name))
        I=resize(I,(I.shape[0] // sous_ech, I.shape[1] // sous_ech),anti_aliasing=True)
        T.append(I)
        if classe1=='ARTIFICIEL':
            C1[i]=0
        else:
            C1[i]=1
            
        if classe2 == 'COTE':
            C2[i]=2
        elif classe2 == 'FORET':
            C2[i]=3
        elif classe2 == 'AUTOROUTE':
            C2[i]=4
        elif classe2 == 'VILLE':
            C2[i]=5
        elif classe2 == 'MONTAGNE':
            C2[i]=6
        elif classe2 == 'OPEN_COUNTRY':
            C2[i]=7
        elif classe2 == 'RUE':
            C2[i]=8
        elif classe2 == 'GRANDBATIMENT':
            C2[i]=9
    print('Fin Lecture base')
    return T,C1,C2



def lire_database(path,num):
    database = pd.read_csv(path)
    #on récupère le nuero d'image et les classes pour la valeur num de la base
    n_Im=database.iloc[num,0]
    classe1=database.iloc[num,1]
    classe2=database.iloc[num,2]
    gabor=database.iloc[num,3:]
    
    return n_Im,classe1,classe2,gabor

def lire_images_et_carac(root_dir, path_cvs ,num, sous_ech=2):
    T=[]
    Gab=[]
    C1=np.zeros(num)
    C2=np.zeros(num)
    for i in np.arange(0,num):
        n_Im, classe1, classe2, gabor =lire_database(path_cvs,i)
        img_name = os.path.join(root_dir,str(n_Im)+".jpg")
        I=img_to_array(load_img(img_name))
        I=resize(I,(I.shape[0] // sous_ech, I.shape[1] // sous_ech),anti_aliasing=True)
        T.append(I)
        Gab.append(gabor)
        if classe1=='ARTIFICIEL':
            C1[i]=0
        else:
            C1[i]=1
            
        if classe2 == 'COTE':
            C2[i]=2
        elif classe2 == 'FORET':
            C2[i]=3
        elif classe2 == 'AUTOROUTE':
            C2[i]=4
        elif classe2 == 'VILLE':
            C2[i]=5
        elif classe2 == 'MONTAGNE':
            C2[i]=6
        elif classe2 == 'OPEN_COUNTRY':
            C2[i]=7
        elif classe2 == 'RUE':
            C2[i]=8
        elif classe2 == 'GRANDBATIMENT':
            C2[i]=9

    return T,C1,C2,Gab