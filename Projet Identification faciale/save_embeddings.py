#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Module pour sauvegarder et charger les embeddings
#

import pickle
import os
import json
import numpy as np

def sauvegarder_embeddings(visages, fichier='embeddings_database.pkl'):
    """
    Sauvegarde les embeddings dans un fichier pickle
    
    Args:
        visages: dictionnaire {nom_personne: [liste_embeddings]}
        fichier: nom du fichier de sortie
    """
    with open(fichier, 'wb') as f:
        pickle.dump(visages, f)
    print(f"Embeddings sauvegardés dans {fichier}")
    
    # Sauvegarder aussi les métadonnées en JSON
    metadata = {
        nom: len(embeddings) 
        for nom, embeddings in visages.items()
    }
    with open(fichier.replace('.pkl', '_metadata.json'), 'w') as f:
        json.dump(metadata, f, indent=2)

def charger_embeddings(fichier='embeddings_database.pkl'):
    """
    Charge les embeddings depuis un fichier pickle
    
    Args:
        fichier: nom du fichier à charger
        
    Returns:
        dictionnaire {nom_personne: [liste_embeddings]}
    """
    if not os.path.exists(fichier):
        print(f"Fichier {fichier} non trouvé")
        return {}
    
    with open(fichier, 'rb') as f:
        visages = pickle.load(f)
    
    print(f"\n=== Embeddings chargés depuis {fichier} ===")
    print(f"Nombre de personnes : {len(visages)}")
    for nom, embeddings in visages.items():
        print(f"  • {nom} : {len(embeddings)} embedding(s)")
    
    return visages
