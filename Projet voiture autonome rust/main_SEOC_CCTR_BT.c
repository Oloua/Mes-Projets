/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/

/**
 * @file    main_SEOC_CCTR_base.c
 * @brief   Application entry point.
 */
#include "CCTR.h"
//#include CCTR_steering.<c|h>.
void task_LED(void *pvParameters);
#define task_LED_PRIORITY 0
void task_BT_Init(void *pvParameters);
void task_Motor(void *pvParameters);
void task_Speed_const(void *pvParameters);
void task_Steering(void *pvParameters);
void task_Camera(void *pvParameters);
void task_Camera_Display(void *pvParameters);
void task_direction(void *pvParameters);
#define task_BT_Init_PRIORITY 0
#define task_MOTOR_PRIORITY 0
#define task_Sterring_PRIORITY 0
#define task_Camera_PRIORITY 0

#if 1
/*
 * @brief   Application entry point.
 */
int main(void) {
  /*
    Fonctions d'initialisation des différentes entrées/sorties
    et des protocoles de communication
  */

  BOARD_init_all();
  PRINTF("Hello World CCTR SEOC Base LED+ Motor\n\r");
  /* Création d'une tache avec pile statique, priorité statique */
 // xTaskCreate(task_LED, "Task_L",
   //           configMINIMAL_STACK_SIZE + 100, NULL, task_LED_PRIORITY, NULL);

  xTaskCreate(task_Speed_const, "Task_S",
                configMINIMAL_STACK_SIZE + 100, NULL, task_MOTOR_PRIORITY+1, NULL);
  //xTaskCreate(task_Steering, "Task_St",
  //               configMINIMAL_STACK_SIZE + 100, NULL, task_Sterring_PRIORITY, NULL);

  xTaskCreate(task_Camera, "Task_C",
                   configMINIMAL_STACK_SIZE + 100, NULL, task_Camera_PRIORITY+2, NULL);
 // xTaskCreate(task_Camera_Display, "Task_CD",
   //                  configMINIMAL_STACK_SIZE + 100, NULL, task_Camera_PRIORITY, NULL);
  xTaskCreate(task_direction, "Task_D",
                       configMINIMAL_STACK_SIZE + 100, NULL, task_Camera_PRIORITY+3, NULL);

 // xTaskCreate(task_Motor, "Task_M",
                  //configMINIMAL_STACK_SIZE + 100, NULL, task_MOTOR_PRIORITY, NULL);

  /*xTaskCreate(task_BT_Init, "Task_BT_init",
		  	  configMINIMAL_STACK_SIZE + 100, NULL, task_BT_Init_PRIORITY, NULL);
   */
  vTaskStartScheduler();
  /* Enter an infinite loop, but should never arrive here */
  for (;;)
    ;
  return 0 ;
}
#endif
/*
 * Tache périodique pour faire clignoter la LED
 */
int led_cpt=0;
void task_LED(void *pvParameters) {
	const TickType_t xDelay= Ms(250);
	led_cpt=0;
	LED_RED_OFF();
	LED_BLUE_OFF();
	LED_GREEN_OFF();
	//LED_RED_TOGGLE();

	for (;;)
	{

		LED_RED_OFF();
		LED_BLUE_OFF();
		LED_GREEN_OFF();
		vTaskDelay(xDelay);

		LED_RED_OFF();
		LED_BLUE_OFF();
		LED_GREEN_ON();
		vTaskDelay(xDelay);

		LED_RED_OFF();
		LED_BLUE_ON();
		LED_GREEN_OFF();
		vTaskDelay(xDelay);

		LED_RED_OFF();
		LED_BLUE_ON();
		LED_GREEN_ON();
		vTaskDelay(xDelay);

		LED_RED_ON();
		LED_BLUE_OFF();
		LED_GREEN_OFF();
		vTaskDelay(xDelay);

		LED_RED_ON();
		LED_BLUE_OFF();
		LED_GREEN_ON();
		vTaskDelay(xDelay);

		LED_RED_ON();
		LED_BLUE_ON();
		LED_GREEN_OFF();
		vTaskDelay(xDelay);

		LED_RED_ON();
		LED_BLUE_ON();
		LED_GREEN_ON();
		PRINTF("%d\r\n", led_cpt);
		PRINTF("y %d\r\nz %d\r\n", led_cpt % 100, 100-(led_cpt %100));
		vTaskDelay(xDelay);

		led_cpt++;
	}
}

