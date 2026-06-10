# -*- coding: utf-8 -*-
"""
Created on Fri Feb 16 16:57:59 2024

@author: charbons
"""
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

plt.close('all')

#création de la base d'apprentissage
#%%
l=50
C1=np.zeros((l,3))
C1=np.random.normal(4.5, 1.5, size=(l,3))
C1[:,2]=C1[:,2]*0

C2=np.zeros((l,3))
C2[:,1]=np.random.normal(0, 1.5, size=(l,))
C2[:,0]=1.5*C2[:,1]+np.random.normal(0, 2.5, size=(l,))
C2[:,2]=C1[:,2]*0+1

A=np.concatenate((C1,C2))
plt.scatter(A[:,0],A[:,1],c=A[:,2])
plt.grid()

#%%
# fonction calculant la ddp de x avec m la moyenne et co la matrice de co variance
def densité_proba(m,co,x):
    ci=np.linalg.inv(co)
    det=np.sqrt(np.linalg.det(co))
    xt=np.reshape(x,(2,1))
    mt=np.reshape(m,(2,1))
    p=np.exp(-0.5*(x-m)@ci@(xt-mt))/(2*np.pi*det)
    return p


#%%
###################
# Calcul de la moyenne et de la matrice de covariance des 2 classes
# On trie les exemples de classe 0
F0=A[A[:,2]==0,:2]
m0=np.mean(F0,axis=0)
co0=np.cov(np.transpose(F0))

# On trie les exemples de classe 1
F1=A[A[:,2]==1,:2]
m1=np.mean(F1,axis=0)
co1=np.cov(np.transpose(F1)) 

######
# calcul de la matrice de covariance des 2 classes, les 2 matrices sont diagonales
co0d = np.zeros((2, 2))
np.fill_diagonal(co0d, [np.diag(co0)[0],np.diag(co0)[1]])

co1d = np.zeros((2, 2))
np.fill_diagonal(co1d, [np.diag(co1)[0],np.diag(co1)[1]])

###############
# calcul de la matrice de covariance en fusionnant les exemples des 2 classes
co=np.cov(np.transpose(np.concatenate((F0-m0,F1-m1))))


#%% Tracé des frontières
x=np.arange(np.min(A[:,0]),np.max(A[:,0]),0.5)
y=np.arange(np.min(A[:,1]),np.max(A[:,1]),0.5)

trace=[]
exemple=np.zeros((2,1))
for i in x:
    for j in y:
        exemple=[i, j]
        #donner le code pour affecter une  classe à exemple
        classe = 0
        p0 = densité_proba(m0, co0, exemple)
        p1 = densité_proba(m1, co1, exemple)
        if p1 > p0:
            classe = 1
        trace.append([exemple[0], exemple[1],classe])

trace=np.asarray(trace)
plt.figure()
plt.scatter(trace[:,0],trace[:,1],c=trace[:,2],marker='.')

plt.scatter(A[:,0],A[:,1],c=A[:,2],marker='o')