/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/

#include "CCTR.h"

/*
 * Test des capteurs de vitesse sur les roues
 */

void task_Speed(void *pvParameters) {
	const TickType_t xDelay= Ms(200);
	PRINTF("Task Speed\n\r");
    /* Initialise Speed Counters */
		//LED_RED_OFF();
		//LED_BLUE_OFF();
		//LED_GREEN_OFF();
    Speed_Get_1();
	Speed_Get_2();
	float pwm_ref = 3;
	float pwm_duty_cycle_left = 0;
	float pwm_duty_cycle_right = 0;
	int sens = 1; // aller devant
	int cont  =  0;
	float Ks = 0.07;
	float dist = 0;
	float vitesse_gauche = 0 ;
	float vitesse_droite = 0 ;
	float v_moyenne = 0 ;
        for (;;)
	{
        //	LED_GREEN_OFF();
        //	LED_RED_OFF();
      	//	LED_BLUE_ON();

		int s1n= Speed_Get_1(); //Gauche
		int s2n= Speed_Get_2();	//Droit

		PRINTF("S1 %d\r\n", s1n); /* To display Speed */
		PRINTF("S2 %d\r\n", s2n);
		pwm_duty_cycle_left =  pwm_duty_cycle_left - Ks*(s1n-pwm_ref);
		pwm_duty_cycle_right =  pwm_duty_cycle_right - Ks*(s2n-pwm_ref);
		Motor_UpdatePwm(pwm_duty_cycle_left, sens, pwm_duty_cycle_right, sens);
		//PRINTF("MG %d\r\n",pwm_duty_cycle_left); /* To display PWM */
		//PRINTF("MD %d\r\n",pwm_duty_cycle_right);
		cont ++;
		vTaskDelay(xDelay);
		vitesse_gauche =((float)s1n/12)*5*2*3.14*3.25;
		vitesse_droite =((float)s2n/12)*5*2*3.14*3.25;
		v_moyenne=(vitesse_gauche+vitesse_droite)/2;
		dist += v_moyenne*0.2;
		PRINTF("distance %f\r\n", dist);
		PRINTF("vitesse_gauche %f\r\n",vitesse_gauche);
		PRINTF("vitesse_droite %f\r\n",vitesse_droite);
		PRINTF("v_moyenne %f\r\n",v_moyenne);
		if(dist > 200  ){
			//LED_GREEN_OFF();

			//LED_RED_ON();
			//LED_BLUE_OFF();
			Motor_UpdatePwm(0, sens, 0, sens);
			break;
		}
		if(cont == 150 ){
			//LED_GREEN_ON();
			//LED_RED_OFF();
			//LED_BLUE_OFF();
			Motor_UpdatePwm(0, sens, 0, sens);
			break;
		}

	}
}

void task_Speed_const(void *pvParameters) {
	const TickType_t xDelay= Ms(200);
	PRINTF("Task Speed\n\r");
    /* Initialise Speed Counters */
		//LED_RED_OFF();
		//LED_BLUE_OFF();
		//LED_GREEN_OFF();
    Speed_Get_1();
	Speed_Get_2();
	float pwm_ref = 10;
	float pwm_duty_cycle_left = 0;
	float pwm_duty_cycle_right = 0;
	int sens = 1; // aller devant
	int cont  =  0;
	float Ks = 0.07;
	float dist = 0;
	float vitesse_gauche = 0 ;
	float vitesse_droite = 0 ;
	float v_moyenne = 0 ;
        for (;;)
	{
        //	LED_GREEN_OFF();
        //	LED_RED_OFF();
      	//	LED_BLUE_ON();

		int s1n= Speed_Get_1(); //Gauche
		int s2n= Speed_Get_2();	//Droit

	//	PRINTF("S1 %d\r\n", s1n); /* To display Speed */
	//	PRINTF("S2 %d\r\n", s2n);
		pwm_duty_cycle_left =  pwm_duty_cycle_left - Ks*(s1n-pwm_ref);
		pwm_duty_cycle_right =  pwm_duty_cycle_right - Ks*(s2n-pwm_ref);
		Motor_UpdatePwm(pwm_duty_cycle_left, sens, pwm_duty_cycle_right, sens);
		//PRINTF("MG %d\r\n",pwm_duty_cycle_left); /* To display PWM */
		//PRINTF("MD %d\r\n",pwm_duty_cycle_right);
		cont ++;
		vTaskDelay(xDelay);
		vitesse_gauche =((float)s1n/12)*5*2*3.14*3.25;
		vitesse_droite =((float)s2n/12)*5*2*3.14*3.25;
		v_moyenne=(vitesse_gauche+vitesse_droite)/2;
		dist += v_moyenne*0.2;

		if(cont == 400 ){
			//LED_GREEN_ON();
			//LED_RED_OFF();
			//LED_BLUE_OFF();
			Motor_UpdatePwm(0, sens, 0, sens);
			break;
		}

	}
}

