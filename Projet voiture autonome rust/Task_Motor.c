/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/

#include "CCTR.h"
int motor_cpt=20;

/*
 * Pour le test des moteurs
 * Démonstration à modifier pour un fonctionnement sur piste
 */
 void task_Motor(void *pvParameters) {
	 const TickType_t xDelay= Ms(7000);
	 int distance_parcourue = 0;
     for (;;)
     {
    	 /*
    	 int pwm_duty_cycle= (motor_cpt/5) % 20;
    	 int sens;
    	 if (motor_cpt < 100)
    		 sens = 1;
    	 else
    		 sens = -1;
    	  */

    	 vTaskDelay(xDelay);
    	 int sens = 1;
    	 int pwm_duty_cycle_left= 15;
    	 int pwm_duty_cycle_right= 15;
    	 if(distance_parcourue<100){
			 	 	 Motor_UpdatePwm(pwm_duty_cycle_left, sens,
					 pwm_duty_cycle_right, sens);
    	 }
    	 vTaskDelay(xDelay);
    	 Motor_UpdatePwm(0, sens,
    	 					 0, sens);
    	 break;
     }
 }
