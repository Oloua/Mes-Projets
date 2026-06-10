/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
*/

#include "CCTR.h"

#ifndef CCTR_STEERING_H
#define CCTR_STEERING_H

// A MODIFIER après calibrage de la direction droite
#define STEERING_RELATIVE_OFFSET 758
#define STEERING_RELATIVE_AMPLITUDE 93

// A ne PAS modifier
#define STEERING_ABSOLUTE_ZERO 750 

// Attention 100 pour les anciennes voiture, 200 pour les nouvelles
#define STEERING_AMPLITUDE  150
#define STEERING_ABSOLUTE_MIN  (STEERING_ABSOLUTE_ZERO - STEERING_AMPLITUDE) 
#define STEERING_ABSOLUTE_MAX  (STEERING_ABSOLUTE_ZERO + STEERING_AMPLITUDE)



void Steering_Relative_Zero(void);
void Steering_Absolute_Zero(void);


int16_t Steering_UpdatePwm_Relative(int16_t direction);

uint16_t Steering_UpdatePwm_Absolute(uint16_t direction);


void Steering_Init(void);


void TPM_UpdatePwmDutycycle_10000(TPM_Type *base,
                            tpm_chnl_t chnlNumber,
                            tpm_pwm_mode_t currentPwmMode,
                            uint16_t dutyCycle);

/* Board steering port map */
#define STEERING_TIMER_BASEADDR TPM1
#define STEERING_CHANNEL 0U

/* Get source clock for TPM driver */
#define STEERING_TIMER_SOURCE_CLOCK  CLOCK_GetFreq(kCLOCK_PllFllSelClk)
#define STEERING_CLOCK_MODE 1U
#define STEERING_PWM_FREQ 50U /* 50 Hz = 20 ms */

#endif
