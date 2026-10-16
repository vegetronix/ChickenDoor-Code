// Hc595.c

/*************************************************
Copyright 2017 - Daycounter, Inc.

Contact Info:
Daycounter, Inc. -  Contract Electronics and Software Engineering.
www.daycounter.com
*************************************************/

#include "SpiMsp.h"
#include "Hc595.h"

typedef struct hc595_t {
	BYTE spi_port;
	BYTE rate;
} HC595;
HC595	sHc595;

/********************* 74HC595 Functions *************************************/
void Hc595Init(BYTE spi_port,WORD rate) {
	sHc595.spi_port= spi_port;
	sHc595.rate= rate;
	
	HC595_1_DIR |=		RCLK_1;
	HC595_1_SEL0 &=		~RCLK_1;
	HC595_1_SEL1 &=		~RCLK_1;
	HC595_1_OUT |=		RCLK_1;
}

void Hc595SpiTake(){
	// DATA is latched in on rising edge of clock.
	while(SpiBusy(sHc595.spi_port)) WATCHDOG_CLEAR;				// CHECK BUSY before switching over!!!!!!!!!!!!!!!!
	SpiPortSetCtrl(sHc595.spi_port,sHc595.rate,(UCCKPH|UCMSB|UCMST|UCMODE_0|UCSYNC));
	SpiPortSet(sHc595.spi_port,(WORD*)&HC595_1_OUT,RCLK_1,0x00);		// Make sure we null out the port and CS.
}

void Hc595Write(BYTE* buf,int length) {
	SpiXfer(sHc595.spi_port,buf,NULL,length,true);	
	while(SpiBusy(sHc595.spi_port)) WATCHDOG_CLEAR;		// must complete before exiting since buffer is on stack
}
