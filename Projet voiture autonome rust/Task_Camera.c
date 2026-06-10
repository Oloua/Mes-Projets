/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/

#include "CCTR.h"
#include "CCTR_img_proc.h"
#define STEERING_RELATIVE_OFFSET 750

static uint16_t buffer[128];
unsigned char buff_brut[128];
Bord bord;



/* test CCD Camera */
void task_Camera(void *pvParameters) {
	const TickType_t xDelay= Ms(5);
	const TickType_t xDelay_2= Ms(40);
	PRINTF("Task Camera GO\r\n");
	PRINTF("\n\r");
	for (;;)
	{
		Camera_ImageCapture(1, NULL);
		vTaskDelay(xDelay);
		Camera_ImageCapture(1, buffer);


        /* Pour régler le problème du changement de niveau sur le bord droit, a modifier si nécessaire  */
        buffer[127]=buffer[125];
        buffer[126]=buffer[125];

		for(int ii=0; ii<128; ii++) buff_brut[ii]= 0.5*buffer[ii];
		vTaskDelay(xDelay_2);
	}
}
int cam_test=0;
unsigned char img[128];
void task_Camera_Display(void *pvParameters) {
	const TickType_t xDelay= Ms(200);
		PRINTF("Task Camera Display GO\r\n");
		PRINTF("\n\r");

		for (;;)
		{
			Ligne_Filtre_Gaussien(img, buff_brut);
			Ligne_Bord_Detecte(&bord, img);

			//PRINTF("camera ");
	//		for(int i=0;i<128;i++)
		//		PRINTF("%d ", img[i]);
			//PRINTF("\r\n");
			//PRINTF("cas %d \r\n", bord.cas);
			//PRINTF("BG %d \r\n", bord.gauche);
			//PRINTF("BD %d \r\n", bord.droite);

			vTaskDelay(xDelay);
		}
}

int contador = 0;
void task_direction(void *pvParameters) {

 const TickType_t xDelay= Ms(40);
 float dir_k;
 float dir_k_prec = 0;
 int centre=0,centre_prec=0;
 float K_gain=0.7;
 int tmp;
 for (;;)
	{
	 Ligne_Filtre_Gaussien(img, buff_brut);
	 Ligne_Bord_Detecte(&bord, img);


	if(contador != 0){
	centre_prec =centre;
	}else{
	centre_prec =(bord.droite+bord.gauche)/2;
	contador=1;
	}
	centre=(bord.droite+bord.gauche)/2;
	dir_k = dir_k_prec - K_gain*(centre-centre_prec);
	dir_k_prec = dir_k;
	tmp =  (int)dir_k;
	PRINTF("\r\n dir_k == %d", tmp);
	Steering_UpdatePwm_Absolute(-dir_k*10+STEERING_RELATIVE_OFFSET);
	vTaskDelay(xDelay);
	}
}
