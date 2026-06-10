#ifndef CCTR_IMG_PROC
#define CCTR_IMG_PROC
#include "CCTR_Camera.h"
typedef struct Bord {
int cas; 
/* 
0 = > 2 bords
1 => bord gauche
2 => bord droite
3 =>  pas de bord 
*/
int gauche, droite; /* les indices auxquels les bords sont vus */
} Bord;

/* img est le résultat du filtrage de buffer */
int Ligne_Filtre_Gaussien(unsigned char img[128], unsigned char buffer[128]);
int Ligne_Bord_Detecte(Bord *bord, unsigned char img[128]);
/* retourne le milieu entre les deux bords */
int Bord_Milieu(Bord bord);
/* Calcul le defilement du bord */
int Bord_Defilement(int *perdu, Bord bord_old, Bord bord);

#endif

