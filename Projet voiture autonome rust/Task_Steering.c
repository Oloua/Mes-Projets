/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/


#include "CCTR.h"
/*
  Lire TOUS LES COMMENTAIRES de ce fichier AVANT de commencer
*/
  
/*
  BEWARE - DANGER
  Demander confirmation avant de faire la suite. Le reglage est souvent deja fait.
  Avant d'activer cette tache,
  UNIQUEMENT si ce n'est pas deja fait, regler le servo:
  - Mettre hors tension le CPU et la carte
  - Eteindre l'alim servo,
  - devisser l'axe du servo
  - Mettre tous les potentiometres au minimum (sens horaire a fond)
  - Mettre le processeur sous tension
  - Verifier que la LED verte clignotte
  - Allumer le servo, il tourne tout seul
  - Revisser le bras
  - Passer en mode réglage de l'offset (Lire TOUS LES COMMENTAIRES de ce fichier AVANT de commencer)

*/


void task_Steering(void *pvParameters) {
  const TickType_t xDelay=100/portTICK_PERIOD_MS;
  int demo_start= 1;
  int demo_angle;
  int amplitude_start= 1;
  int amplitude_ref= 0;
  int amplitude_min= 10000;
  int amplitude_max= -10000;
  int amplitude_to_set= 10000;

  int POT_1_VALUE, POT_2_VALUE, POT_3_VALUE;
  PRINTF("Task Steering\n\r");
  for (;;)
	{
      vTaskDelay(xDelay);
      /*
        Lire TOUTE la suite ainsi que les commentaires pour CHAQUE mode de réglage 
        Réglage de  la direction
        ===> Pour eviter des conflits, desactiver la tache LED
        La direction est exprimee en 10 000 eme
           Prend une valeur signee d'angle de direction autour de la ligne droite
        
        POT 2 et POT 3 servent à définir le mode de réglage
        
        N° de potentiomètre : 1 2 3, de la gauche vers la droite de la voiture
        ATTENTION = >POT Max= sens anti-horaire, POT Min= sens horaire

        Reglage POT 2 & POT 3 => Mode de réglage
        POT 2 Min , POT 3 Min => Zero absolu  - LED VERT
        POT 2 Min,  POT 3 Max => Reglage Steering STEERING_RELATIVE_OFFSET avec POT 1, - LED ROUGE
        POT 2 Max , POT 3 Min => Reglage Amplitude max, -  LED VIOLETTE
        POT 2 Max , POT 3 Max => Demonstration direction, - LED BLEUE

        VERT => Positionne le servo sur la position ZERO ABSOLU
        ROUGE => Reglage correction ZERO RELATIF
        VIOLET => Réglage amplitude MAX
        BLEU => Demonstration orientation des roues
      */
      POT_2_VALUE =  get_ADC_Value(POT_2);
      POT_3_VALUE =  get_ADC_Value(POT_3);
      if (POT_2_VALUE <128 ) { 
          demo_start= 1;
          amplitude_start= 1;
          /* Si POT_3_VALUE < 128, regle le zero absolu du servo, vert clignote
           * Sinon, regle la ligne droite avec POT_1
           */
          if (POT_3_VALUE < 128) {
              /* POT 2 Min , POT 3 Max 
               * Zero absolu du servomoteur - LED VERT clignotte
               */
              demo_start= 1;
              LED_GREEN_TOGGLE();
              LED_RED_OFF();
              LED_BLUE_OFF();
              Steering_UpdatePwm_Absolute(STEERING_ABSOLUTE_ZERO);
			}
          else	{
            /* POT 2 Min , POT 3 Max
             * Reglage de la ligne droite - LED ROUGE clignotte
             * LIRE TOUTE LA SUITE
             * Pour regler le zero de la ligne droite, tourner le potentiometre P1 jusqu'a ce que les roues soient droites
             * Reporter la valeur de Steering offset dans CCTR_steering.h, en modifiant la valeur de la macro STEERING_OFFSET
             * Si le réglage ne permet pas d'atteindre le zero,
             * - si steering_trim n'est pas sature : modifier la macro suivante  (TRIM_MIN) pour ajouter ce qu'il manque
             * - si steering_trim est sature : dans CCTR_steering.h, modifier STEERING_ABSOLUTE_ZERO pour ajouter ce qu'il manque
             */
#define TRIM_MIN 100
              
            demo_start= 1;
            LED_GREEN_OFF();
            LED_RED_TOGGLE();
            LED_BLUE_OFF();

            POT_1_VALUE =  get_ADC_Value(POT_1);
            //PRINTF("P1: %d \n\r", POT_1_VALUE);
            int steering_trim= STEERING_ABSOLUTE_ZERO - TRIM_MIN + POT_1_VALUE/4;
            PRINTF("Steering Offset : %d\n\r", steering_trim);
            steering_trim= Steering_UpdatePwm_Absolute(steering_trim);
            PRINTF("Steering Offset Saturated: %d\n\r", steering_trim);
              
          }
		}
      else {
        /* POT 2 Max , POT 3 Min
         * Reglage de l'amplitude -  LED VIOLETTE clignotte
         * BIEN LIRE LA SUITE EN ENTIER avant de commencer
         * Réglage relatif par rapport à la position de POT_1 au démarrage du réglage
         * Avant d'arriver ici,
         *  pour regler l'amplitude, commencer par positionner POT_1 au milieu de sa course
         *  Puis regler POT_2 ou POT_3 pour arriver ici
         *  ou bien , une fois POT_2 et POT_3 regles, positionnez POT_1 en gardant le reset appuye, puis relachez
         *  Regler POT_1 dans un sens puis dans l'autre de facon a LAISSER UN MILLIMETRE entre les tiges et le chassis
         *  S'il n'est pas possible d'arriver a  un millimetre dans un sens ET dans l'autre,
         *         dans CCTR_steering.h,  augmentez STEERING_RELATIVE_AMPLITUDE et recommencez
         *  Pour terminer, dans CCTR_steering.h, reportez la valeur indiquee de STEERING_RELATIVE_AMPLITUDE sur le terminal
         *  Si la direction touche le chassis, recommencer toute la manip
         */
        if (POT_3_VALUE< 128) {
          demo_start= 1;
          LED_RED_TOGGLE();
          LED_GREEN_OFF();
          LED_BLUE_TOGGLE();
          if (amplitude_start) {
            LED_RED_OFF();
            LED_BLUE_OFF();
            amplitude_start= 0;
            /* At start, get the value of POT_1 as reference to relative zero */
            amplitude_ref =  get_ADC_Value(POT_1)/2;
          }
          int steering_amplitude= ((int)get_ADC_Value(POT_1)/2) - amplitude_ref;
          /* Clamped steering */
          steering_amplitude= Steering_UpdatePwm_Relative(steering_amplitude);
          
          if (steering_amplitude < amplitude_min) amplitude_min= steering_amplitude;
          if (steering_amplitude > amplitude_max) amplitude_max= steering_amplitude;
          /* Get minimum amplitude to set in CCTR_steering.h */
          if (abs(amplitude_min) < amplitude_max) amplitude_to_set= abs(amplitude_min);
          if (amplitude_max <= abs(amplitude_min)) amplitude_to_set= amplitude_max;

          PRINTF("Actual %d  ;   Min : %d  ; Max %d\n\r",steering_amplitude, amplitude_min, amplitude_max);
          PRINTF("STEERING_RELATIVE_AMPLITUDE  to set %d\n\r", amplitude_to_set);
        }
        else {
          /*  POT 2 Max , POT 3 Max
           * Demonstration de  la direction -  LED Bleue clignotte
           * Fait evoluer automatiquement la direction en modifiant steering_cpt regulierement
           * Puis met la direction au Zero relatif pendant 5 secondes
           * Pendant les mouvements, verifiez que la direction ne touche pas le chassis
           * Si c'est le cas, recommencez le reglage de l'amplitude max
           */
          LED_RED_OFF();
          LED_GREEN_OFF();
          LED_BLUE_TOGGLE();
          amplitude_start= 1;

          /* Fait evoluer le steering en dent de scie, d'un cote a l'autre */
          const demo_amplitude= 200;
          if (demo_start) {
            demo_start= 0;
            demo_angle= demo_amplitude/2; /* to start at relative zero */
            Steering_Relative_Zero();
            vTaskDelay(1000); /* Attend 1 secondes */
          }
          /* iter_dir vaut -1 puis 1, tous les iter_amplitude */
          int demo_dir= ((demo_angle % (2*demo_amplitude))/demo_amplitude)*2 - 1;
          int steering_command= -demo_dir*demo_amplitude/2 + demo_dir*(demo_angle % demo_amplitude);
          demo_angle += 20;
           
            
          if (demo_angle < demo_amplitude*10)  { // Fait 10 rotations
            PRINTF("Steering %d\n\r", steering_command);
            Steering_UpdatePwm_Relative(steering_command);
          }
          else {
            PRINTF("Steering Zero\n\r");
            Steering_Relative_Zero();
            vTaskDelay(5000); /* Attend 5 secondes */
            demo_start= 1;
          }
        }
      }

	}
}



