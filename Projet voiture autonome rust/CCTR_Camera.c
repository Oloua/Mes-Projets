/*
  Auteur :
  Stéphane MANCINI
  Grenoble INP
 */

#include "CCTR_Camera.h"
// Defines for LineScan Camera
#define TAOS_DELAY				asm ("nop")				// minimal delay time
#define	CAM_RIGHT_SI_HIGH			CCD_SI_ON()
#define	CAM_RIGHT_SI_LOW				CCD_SI_OFF()
#define	CAM_RIGHT_CLK_HIGH			CCD_CK_ON()
#define	CAM_RIGHT_CLK_LOW			CCD_CK_OFF()

#define	CAM_LEFT_SI_HIGH			CCD_2_SI_ON()
#define	CAM_LEFT_SI_LOW			CCD_2_SI_OFF()
#define	CAM_LEFT_CLK_HIGH			CCD_2_CK_ON()
#define	CAM_LEFT_CLK_LOW			CCD_2_CK_OFF()

void Camera_ImageCapture_Right(uint16_t  *buffer);
void Camera_ImageCapture_Left(uint16_t  *buffer);

// Capture LineScan Image
/*
 * Captures the data from the camera, stored in buffer array (of size 128)
 * If  buffer==NULL, do not store the data, the adc is not performed (saves conversion time)
 *
 * @param camera  camera slot
 *    1= right camera (default)
 *    0= left camera
 * @param buffer address to store the data; must be preallocated
 *
 */

void Camera_ImageCapture(int camera, uint16_t  *buffer)
{
	uint16_t value=0;

	if (camera == 1)
		Camera_ImageCapture_Right(buffer);
	else
		Camera_ImageCapture_Left(buffer);
}

void Camera_ImageCapture_Right(uint16_t  *buffer) {

	unsigned char i;
	CAM_RIGHT_CLK_LOW;
	CAM_RIGHT_SI_LOW;
	TAOS_DELAY;
	CAM_RIGHT_CLK_HIGH;
	TAOS_DELAY;
	CAM_RIGHT_CLK_LOW;
	CAM_RIGHT_SI_HIGH;
	TAOS_DELAY;
	CAM_RIGHT_CLK_HIGH;
	TAOS_DELAY;
	CAM_RIGHT_SI_LOW;
	TAOS_DELAY;
	//value= CCD_ADC_Value(1);						// return value
	if (buffer!=NULL)
		buffer[0] =  CCD_ADC_Value(1);
	CAM_RIGHT_CLK_LOW;
	for(i=1;i<128;i++)
	{
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_RIGHT_CLK_HIGH;
		TAOS_DELAY;
		TAOS_DELAY;
		// inputs data from camera (one pixel each time through loop)
		//value= CCD_ADC_Value(1);
		if (buffer!=NULL)
			buffer[i] = CCD_ADC_Value(1);						// return value
		CAM_RIGHT_CLK_LOW;
	}
	for(i=0;i<16;i++) {
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_RIGHT_CLK_HIGH;
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_RIGHT_CLK_LOW;
	}
}
void Camera_ImageCapture_Left(uint16_t  *buffer) {

	unsigned char i;
	CAM_LEFT_CLK_LOW;
	CAM_LEFT_SI_LOW;
	TAOS_DELAY;
	CAM_LEFT_CLK_HIGH;
	TAOS_DELAY;
	CAM_LEFT_CLK_LOW;
	CAM_LEFT_SI_HIGH;
	TAOS_DELAY;
	CAM_LEFT_CLK_HIGH;
	TAOS_DELAY;
	CAM_LEFT_SI_LOW;
	TAOS_DELAY;
	//value = CCD_ADC_Value(2);						// return value
	if (buffer!=NULL)
		buffer[0]=CCD_ADC_Value(2);
	CAM_LEFT_CLK_LOW;
	for(i=1;i<128;i++)
	{
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_LEFT_CLK_HIGH;
		TAOS_DELAY;
		TAOS_DELAY;
		// inputs data from camera (one pixel each time through loop)
		//value = CCD_ADC_Value(2);						// return value
		if (buffer != NULL)
			buffer[i] =  CCD_ADC_Value(2);						// return value
		CAM_LEFT_CLK_LOW;
	}
	for(i=0;i<16;i++) {
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_LEFT_CLK_HIGH;
		TAOS_DELAY;
		TAOS_DELAY;
		CAM_LEFT_CLK_LOW;
	}
}



adc16_channel_config_t ccd_config_channel;
adc16_channel_config_t ccd_2_config_channel;


void CCD_InitPin(void)
{
	gpio_pin_config_t led_config = {
			kGPIO_DigitalOutput, 0,
	};
	/* Init output LED GPIO. */
	GPIO_PinInit(CCD_SI_GPIO, CCD_SI_GPIO_PIN, &led_config);
	GPIO_PinInit(CCD_CK_GPIO, CCD_CK_GPIO_PIN, &led_config);
	// Camera 2 :
	GPIO_PinInit(CCD_SI_GPIO, CCD_2_SI_GPIO_PIN, &led_config);
	GPIO_PinInit(CCD_CK_GPIO, CCD_2_CK_GPIO_PIN, &led_config);

	CCD_SI_INIT(0);
	CCD_CK_INIT(0);

	// Camera 2 :
	CCD_2_SI_INIT(0);
	CCD_2_CK_INIT(0);


	adc16_config_t config;
	ADC16_GetDefaultConfig(&config);
	//config.enableHighSpeed = true;
	config.resolution= kADC16_ResolutionSE10Bit;
	config.clockDivider = kADC16_ClockDivider4;
	//config.longSampleMode= kADC16_LongSampleCycle24;
	ADC16_Init(CCD_ADC16_BASEADDR, &config);


	ccd_config_channel.channelNumber = CCD_ADC16_USER_CHANNEL;
	ccd_config_channel.enableInterruptOnConversionCompleted = false;
	ADC16_SetChannelConfig(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP, &ccd_config_channel);

	ADC16_SetHardwareAverage(CCD_ADC16_BASEADDR, kADC16_HardwareAverageCount4);

	// Camera 2 :
	ccd_2_config_channel.channelNumber = CCD_2_ADC16_USER_CHANNEL;
	ccd_2_config_channel.enableInterruptOnConversionCompleted = false;
	ADC16_SetChannelConfig(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP, &ccd_2_config_channel);
	ADC16_SetHardwareAverage(CCD_ADC16_BASEADDR, kADC16_HardwareAverageCount4);
}

int CCD_ADC_Value(int which) {
	if (which == 1){
		ADC16_SetChannelConfig(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP, &ccd_config_channel);
	}
	else{
		ADC16_SetChannelConfig(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP, &ccd_2_config_channel);
	}

	while ( ! (ADC16_GetChannelStatusFlags(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP) & kADC16_ChannelConversionDoneFlag));
	return ADC16_GetChannelConversionValue(CCD_ADC16_BASEADDR, CCD_ADC16_CHANNEL_GROUP);
}



