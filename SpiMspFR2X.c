// SpiMsp.c

#include "SpiMsp.h"

/*************************************************
Copyright 2012 - Vegetronix, Inc.

*************************************************/

/*	 define one or more of these in SysDep.h
#define SPI_A0		0x01	
#define SPI_A1		0x02
#define SPI_B0		0x04	
#define SPI_B1		0x08

// must define these, for example, for SPI on USCIB1
#define SPIB1_SEL
#define	SPIB1_DIR
#define SCLKB1
#define MOSIB1
#define MISOB1
#define SPIB1_ALL (SCLKB1|MOSIB1|MISOB1)
*/


/******************************************************************************* 
NOTE:
	The memory for the buffers must persist until the routine completes.  If the 
	calling function allocates a buffer	on the stack, and then returns before the
	Spi routine has gathered data, it will wipe out the memory!!!

********************************************************************************/




typedef struct spi_t {
    WORD *port;			// address of the GPIO port for the current chip select.
	WORD cs;			// CHIP select mask
	BOOL busy;
	BOOL deassert;		// set if you want a transfer command to deassert when done. 
	int length;			// length of data to be transfered	
	int rxidx;
	int txidx;
	BYTE* txbuf;
	BYTE* rxbuf;		// can be NULL. 
	BYTE filler;
} Spi;



#ifdef SPI_A0
Spi	sSpiA0;
void SpiA0TxIsr(){
	//UCTXIFG is automatically reset if a character is written to UCA0TXBU
	if(sSpiA0.txidx < sSpiA0.length){
		if(sSpiA0.txbuf) UCA0TXBUF = sSpiA0.txbuf[sSpiA0.txidx];		
		else  UCA0TXBUF = sSpiA0.filler;
		sSpiA0.txidx++;	
	}
}
void SpiA0RxIsr() {
	// Reading UCA0RXBUF resets UCRXIFG and the receive error bits.
	// For some unknown reason it loses interrupts if we don't save RXBUF0 to a temporary variable "bogus" first. 
	BYTE ch;
	ch= UCA0RXBUF; // clears the interrupt
	if(sSpiA0.rxidx < sSpiA0.length){
		if(sSpiA0.rxbuf) sSpiA0.rxbuf[sSpiA0.rxidx]= ch;
		sSpiA0.rxidx++;	
		if(sSpiA0.rxidx==sSpiA0.length) { // done
			sSpiA0.busy= false;
			if(sSpiA0.deassert && sSpiA0.port){ // sSpi0.port could be null. 
				(*sSpiA0.port)|= sSpiA0.cs;
			}
		} else {
		    while (!(UCA0IFG & UCTXIFG));
			if(sSpiA0.txbuf) UCA0TXBUF = sSpiA0.txbuf[sSpiA0.txidx];		
			else  UCA0TXBUF = sSpiA0.filler;
			sSpiA0.txidx++;
		}
	} 
}
#endif // SPI_A0

#ifdef SPI_B0
Spi	sSpiB0;
void SpiB0TxIsr(){
	//UCTXIFG is automatically reset if a character is written to UCB0TXBU
	if(sSpiB0.txidx < sSpiB0.length){
		if(sSpiB0.txbuf) UCB0TXBUF = sSpiB0.txbuf[sSpiB0.txidx];		
		else  UCB0TXBUF = sSpiB0.filler;
		sSpiB0.txidx++;	
	}
}
void SpiB0RxIsr(){
	//Reading UCB0RXBUF resets UCRXIFG and the receive error bits.
	// For some unknown reason it loses interrupts if we don't save RXBUF0 to a temporary variable "bogus" first. 
	BYTE ch;
	
	ch= UCB0RXBUF; // clears the interrupt
	if(sSpiB0.rxidx < sSpiB0.length){
		if(sSpiB0.rxbuf) 
			sSpiB0.rxbuf[sSpiB0.rxidx]= ch;
		sSpiB0.rxidx++;	
		if(sSpiB0.rxidx==sSpiB0.length) { // done
			sSpiB0.busy= false;
			if(sSpiB0.deassert && sSpiB0.port){ // sSpi0.port could be null. 
				(*sSpiB0.port)|= sSpiB0.cs;
			}
		} else {
		    while (!(UCB0IFG & UCTXIFG));
			if(sSpiB0.txbuf) UCB0TXBUF = sSpiB0.txbuf[sSpiB0.txidx];		
			else  UCB0TXBUF = sSpiB0.filler;
			sSpiB0.txidx++;
		}
	} 
}
#endif // SPI_B0

#ifdef SPI_A1
Spi	sSpiA1;
void SpiA1TxIsr(){
	//UCTXIFG is automatically reset if a character is written to UCB0TXBU
	if(sSpiA1.txidx < sSpiA1.length){
		if(sSpiA1.txbuf) UCA1TXBUF = sSpiA1.txbuf[sSpiA1.txidx];		
		else  UCA1TXBUF = sSpiA1.filler;
		sSpiA1.txidx++;	
	}
}
void SpiA1RxIsr() {
	//Reading UCA1RXBUF resets UCRXIFG and the receive error bits.
	BYTE ch;
	ch= UCA1RXBUF; // clears the interrupt
	if(sSpiA1.rxidx < sSpiA1.length){
		if(sSpiA1.rxbuf) sSpiA1.rxbuf[sSpiA1.rxidx]= ch;
		sSpiA1.rxidx++;	
		if(sSpiA1.rxidx>=sSpiA1.length) { // done
			sSpiA1.busy= false;
			if(sSpiA1.deassert && (sSpiA1.port!=0)){ // sSpiB1.port could be null. 
				(*sSpiA1.port)|= sSpiA1.cs;
			}			
		} else {
		    while (!(UCA1IFG & UCTXIFG));   // do we really want to block in an ISR???
			if(sSpiA1.txbuf) UCA1TXBUF = sSpiA1.txbuf[sSpiA1.txidx];		
			else  UCA1TXBUF = sSpiA1.filler;
			sSpiA1.txidx++;
		}
	} 
}
#endif // SPI_A1

#ifdef SPI_B1
Spi	sSpiB1;
void SpiB1TxIsr(){
	//UCTXIFG is automatically reset if a character is written to UCB1TXBU
	if(sSpiB1.txidx < sSpiB1.length){
		if(sSpiB1.txbuf) UCB1TXBUF = sSpiB1.txbuf[sSpiB1.txidx];		
		else  UCB1TXBUF = sSpiB1.filler;
		sSpiB1.txidx++;	
	}
	
}
void SpiB1RxIsr() {
	//Reading UCB1RXBUF resets UCRXIFG and the receive error bits.
	BYTE ch;
	ch= UCB1RXBUF; 
	if(sSpiB1.rxidx < sSpiB1.length){
		if(sSpiB1.rxbuf) sSpiB1.rxbuf[sSpiB1.rxidx]= ch;
		sSpiB1.rxidx++;	
		if(sSpiB1.rxidx>=sSpiB1.length) { // done
			sSpiB1.busy= false;
			if(sSpiB1.deassert && (sSpiB1.port!=0)){ // sSpiB1.port could be null. 
				(*sSpiB1.port)|= sSpiB1.cs;
			}			
		} else {
			while (!(UCB1IFG & UCTXIFG));   // do we really want to block in an ISR???
			if(sSpiB1.txbuf) UCB1TXBUF = sSpiB1.txbuf[sSpiB1.txidx];		
			else  UCB1TXBUF = sSpiB1.filler;
			sSpiB1.txidx++;
		}
	} 
}
#endif // SPI_B1

/********************* SPI Functions *************************************/

void SpiInit(BYTE spi_mask) {

#ifdef SPI_A0
	if(SPI_A0 & spi_mask) {
		sSpiA0.port= 0;
		sSpiA0.cs= 0;
		sSpiA0.busy= false;
		sSpiA0.length=0;
		sSpiA0.rxidx= 0;
		sSpiA0.txidx= 0; 
		sSpiA0.txbuf= NULL;
		sSpiA0.rxbuf= NULL;
		
		// set up port pins
		SPIA0_SEL0|= SPIA0_ALL;			// SPI
		SPIA0_SEL1&= ~SPIA0_ALL;        // SPI
		SPIA0_DIR|= (SCLKA0|MOSIA0);
		SPIA0_DIR&= ~(MISOA0);
		
		// configure USCI in SPI mode
        UCA0CTLW0|= UCSWRST;
        UCA0CTLW0|= (UCSSEL__SMCLK|UCCKPH|UCMSB|UCMST|UCMODE_0|UCSYNC); // Master,Capture rising edge, MSB first, 8bit, 3-pin SPI mode, inactive clock high, synchronous
        UCA0BRW = 0x0020; // make sure we are not exceeding the RF chip clock max
        UCA0CTLW0&= ~UCSWRST;
        UCA0IE |= UCRXIE;
	}
#endif // SPI_A0

#ifdef SPI_B0
	if(SPI_B0 & spi_mask) {
		sSpiB0.port= 0;
		sSpiB0.cs= 0;
		sSpiB0.busy= false;
		sSpiB0.length=0;
		sSpiB0.rxidx= 0;
		sSpiB0.txidx= 0; 
		sSpiB0.txbuf= NULL;
		sSpiB0.rxbuf= NULL;
		
		// set up port pins
		SPIB0_SEL0|= SPIB0_ALL;			// SPI
		SPIB0_SEL1&= ~SPIB0_ALL;
		SPIB0_DIR|= (SCLKB0|MOSIB0);
		SPIB0_DIR&= ~(MISOB0);
		
		// configure USCI in SPI mode
		UCB0CTLW0|= UCSWRST;
		UCB0CTLW0= (UCSSEL__SMCLK|UCCKPH|UCMSB|UCMST|UCSYNC);	// Master,Capture rising edge, MSB first, 8bit, 3-pin SPI mode, inactive clock high, synchronous
		UCB0BRW = 0x0020; // make sure we are not exceeding the RF chip clock max
		UCB0CTLW0&= ~UCSWRST;
		UCB0IE |= UCTXIE;
		UCB0IFG= 0; // clear any pending interrupts.
	}
#endif // SPI_B0

#ifdef SPI_A1
	if(SPI_A1 & spi_mask) {
		sSpiA1.port= 0;
		sSpiA1.cs= 0;
		sSpiA1.busy= false;
		sSpiA1.length=0;
		sSpiA1.rxidx= 0;
		sSpiA1.txidx= 0; 
		sSpiA1.txbuf= NULL;
		sSpiA1.rxbuf= NULL;	
		// set up port pins
		SPIA1_SEL0|= SPIA1_ALL;				// SPI
		SPIA1_SEL1&= ~SPIA1_ALL;
		SPIA1_DIR|= (SCLKA1|MOSIA1);
		SPIA1_DIR&= ~(MISOA1);
		
        // configure USCI in SPI mode
        UCA1CTLW0|= UCSWRST;
        UCA1CTLW0|= (UCSSEL__SMCLK|UCCKPH|UCMSB|UCMST|UCMODE_0|UCSYNC); // Master,Capture rising edge, MSB first, 8bit, 3-pin SPI mode, inactive clock high, synchronous
        UCA1BRW = 0x0020; // make sure we are not exceeding the RF chip clock max
        UCA1CTLW0&= ~UCSWRST;
        UCA1IE |= UCRXIE;
	}
#endif // SPI_A1

#ifdef SPI_B1
	if(SPI_B1 & spi_mask) {
		sSpiB1.port= 0;
		sSpiB1.cs= 0;
		sSpiB1.busy= false;
		sSpiB1.length=0;
		sSpiB1.rxidx= 0;
		sSpiB1.txidx= 0; 
		sSpiB1.txbuf= NULL;
		sSpiB1.rxbuf= NULL;	
		// set up port pins
		SPIB1_SEL0|= SPIB1_ALL;				// SPI
		SPIB1_SEL1&= ~SPIB1_ALL;
		SPIB1_DIR|= (SCLKB1|MOSIB1);
		SPIB1_DIR&= ~(MISOB1);
		//*
        // configure USCI in SPI mode
        UCB1CTLW0|= UCSWRST;
        UCB1CTLW0|= (UCSSEL__SMCLK|UCCKPH|UCMSB|UCMST|UCMODE_0|UCSYNC); // Master,Capture rising edge, MSB first, 8bit, 3-pin SPI mode, inactive clock high, synchronous
        UCB1BRW = 0x0020; // make sure we are not exceeding the RF chip clock max
        UCB1CTLW0&= ~UCSWRST;
        UCB1IE |= UCRXIE;
		//*/
	}
#endif // SPI_B1
}
// THIS NEEDS TO BE CALLED BEFORE USING A SPI PORT (AND BEFORE EACH CHANGE OF PORT)
// PORT IS OF THE FORM P4OUT_, WHERE P4 IS THE PORT THAT CS IS CONNECTED TO
// CS IS A BITMASK FOR THE CHIP SELECT PIN
void SpiPortSet(BYTE spi_mask,WORD *port,WORD cs,BYTE filler) {
	while(SpiBusy(spi_mask)) WATCHDOG_CLEAR; // Snick: caution potential endless loop.

#ifdef SPI_A0
	if(SPI_A0 & spi_mask) {
		sSpiA0.port= port;
		sSpiA0.cs= cs;
		sSpiA0.filler= filler;
	}
#endif // SPI_A0

#ifdef SPI_B0
	if(SPI_B0 & spi_mask) { 
		sSpiB0.port= port;
		sSpiB0.cs= cs;
		sSpiB0.filler= filler;
	}
#endif // SPI_B0
	
#ifdef SPI_A1
	if(SPI_A1 & spi_mask) {
		sSpiA1.port= port;
		sSpiA1.cs= cs;
		sSpiA1.filler= filler;
	}
#endif	// SPI_A1	
	
#ifdef SPI_B1
	if(SPI_B1 & spi_mask) {
		sSpiB1.port= port;
		sSpiB1.cs= cs;
		sSpiB1.filler= filler;
	}
#endif	// SPI_B1
}


void SpiPortSetCtrl(BYTE spi_port,WORD rate, WORD ctrl0) {
#ifdef SPI_A0
    if(spi_port & SPI_A0) {
        UCA0CTLW0|= UCSWRST;
        UCA0CTLW0= ctrl0;           //UCCKPL| Master,Capture falling edge, LSB first, 8bit, 3-pin SPI mode, inactive clock low, synchronous
        UCA0BRW = rate;
        UCA0CTLW0&= ~UCSWRST;                               //
        UCA0IE |= UCRXIE;                                   // Enable USART0 RX interrupt
    }
#endif
#ifdef SPI_B0
	if(spi_port&SPI_B0) {
		UCB0CTLW0|= UCSWRST;
		UCB0CTLW0= ctrl0;			//UCCKPL| Master,Capture falling edge, LSB first, 8bit, 3-pin SPI mode, inactive clock low, synchronous
		UCB0BRW = rate;
		UCB0CTLW0&= ~UCSWRST;								//
		UCB0IE |= UCRXIE; 									// Enable USART0 RX interrupt
	}
#endif
#ifdef SPI_A1
    if(spi_port & SPI_A0) {
        UCA1CTLW0|= UCSWRST;
        UCA1CTLW0= ctrl0;           //UCCKPL| Master,Capture falling edge, LSB first, 8bit, 3-pin SPI mode, inactive clock low, synchronous
        UCA1BRW = rate;
        UCA1CTLW0&= ~UCSWRST;                               //
        UCA1IE |= UCRXIE;                                   // Enable USART0 RX interrupt
    }
#endif
#ifdef SPI_B1
    if(spi_port & SPI_B1) {
        UCB1CTLW0|= UCSWRST;
        UCB1CTLW0= ctrl0;           //UCCKPL| Master,Capture falling edge, LSB first, 8bit, 3-pin SPI mode, inactive clock low, synchronous
        UCB1BRW = rate;
        UCB1CTLW0&= ~UCSWRST;                               //
        UCB1IE |= UCRXIE;                                   // Enable USART0 RX interrupt
    }
#endif

}

void SpiXfer(BYTE spi_mask,BYTE* txbuf, BYTE* rxbuf,int len,BOOL deassert) {
	if(len==0) return;
	while(SpiBusy(spi_mask)); // Snick: caution potential endless loop.

#ifdef SPI_A0
	if(SPI_A0 & spi_mask) {
		sSpiA0.deassert= deassert;
		sSpiA0.rxidx= 0;
		sSpiA0.txidx= 0;
		sSpiA0.rxbuf= rxbuf;
		sSpiA0.txbuf= txbuf;
		sSpiA0.busy= true;
		sSpiA0.length= len;
		if(sSpiA0.port)	(*sSpiA0.port) &= ~sSpiA0.cs;
		// kick off xfer. 
		UCA0IE |= UCRXIE;	// disable more tx's until we get an rx!
		if(sSpiA0.txbuf) 
			UCA0TXBUF = *txbuf;		
		else  
			UCA0TXBUF = sSpiA0.filler;
		sSpiA0.txidx++;
	}
#endif	// SPI_A0

#ifdef SPI_B0
	if(SPI_B0 & spi_mask) {
		sSpiB0.deassert= deassert;
		sSpiB0.rxidx= 0;
		sSpiB0.txidx= 0;
		sSpiB0.rxbuf= rxbuf;
		sSpiB0.txbuf= txbuf;
		sSpiB0.busy= true;
		sSpiB0.length= len;
		if(sSpiB0.port) {
			(*(BYTE*)sSpiB0.port) &= ~sSpiB0.cs;
		}
		// kick off xfer. 
		UCB0IE |= UCTXIE;	// disable more tx's until we get an rx!
		if(sSpiB0.txbuf) UCB0TXBUF = *txbuf;		// send the first byte
		else UCB0TXBUF = sSpiB0.filler;
		sSpiB0.txidx++;
		// the rest of the xfer is handled by the RX ISR
	}
#endif	// SPI_B0

#ifdef SPI_A1
	if(SPI_A1 & spi_mask) {
		sSpiA1.deassert= deassert;
		sSpiA1.rxidx= 0;
		sSpiA1.txidx= 0;	
		sSpiA1.rxbuf= rxbuf;
		sSpiA1.txbuf= txbuf;	
		sSpiA1.busy= true;
		sSpiA1.length= len;
		if(sSpiA1.port) 
			(*sSpiA1.port) &= ~sSpiA1.cs;
		
		// kick off xfer. 
		UCA1IE |= UCRXIE;	// disable more tx's until we get an rx!
		
		if(sSpiA1.txbuf) 
			UCA1TXBUF = *txbuf;		
		else  
			UCA1TXBUF = sSpiA1.filler;
		sSpiA1.txidx++;
		
	}
#endif // SPI_A1

#ifdef SPI_B1
	if(SPI_B1 & spi_mask) {
		sSpiB1.deassert= deassert;
		sSpiB1.rxidx= 0;
		sSpiB1.txidx= 0;	
		sSpiB1.rxbuf= rxbuf;
		sSpiB1.txbuf= txbuf;	
		sSpiB1.busy= true;
		sSpiB1.length= len;
		if(sSpiB1.port) 
			(*sSpiB1.port) &= ~sSpiB1.cs;
		UCB1IE |= /*UCRXIE|*/UCTXIE;	
		// kick off xfer. 
		if(sSpiB1.txbuf) 
			UCB1TXBUF = *txbuf;		
		else  
			UCB1TXBUF = sSpiB1.filler;
		sSpiB1.txidx++;
	}
#endif // SPI_B1
}

BOOL SpiBusy(BYTE spi_mask) {
#ifdef SPI_A0 
	if(SPI_A0 & spi_mask) return sSpiA0.busy;
#endif
#ifdef SPI_B0 
	if(SPI_B0 & spi_mask) return sSpiB0.busy;
#endif
#ifdef SPI_A1 
	if(SPI_A1 & spi_mask) return sSpiA1.busy;
#endif
#ifdef SPI_B1 
	if(SPI_B1 & spi_mask) return sSpiB1.busy;
#endif
	return false;
}

