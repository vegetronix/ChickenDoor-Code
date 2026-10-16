// USCIFR2X.c

#include "Comm2X.h"
#include "SpiMsp.h"


#ifdef EUSCI_A0_VECTOR
#pragma vector = EUSCI_A0_VECTOR
__interrupt void EUSCI_A0_ISR (void)	{
    switch(__even_in_range(UCA0IV,UCIV__UCTXCPTIFG)) {
        case UCIV__UCRXIFG: 	//UCRXIFG- 0x02 Receive buffer full
#ifdef SPI_A0
                SpiA0RxIsr();
#endif
#ifdef COMM_A0
                UartA0RxIsr();
#endif
        break;
        case UCIV__UCTXIFG: 	//UCTXIFG- 0x04 Transmit buffer empty
#ifdef SPI_A0
                SpiA0TxIsr();
#endif
#ifdef COMM_A0
                UartA0TxIsr();
#endif
        break;
        case UCIV__UCTXCPTIFG: //UCTXCPTIFG - 0x08 Transmit complete
        break;
    }
	LPM3_EXIT;
}

WORD gTemp;

#pragma vector = EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_ISR (void)	{
    switch(__even_in_range(UCB0IV,UCIV__UCTXCPTIFG)) {
        case UCIV__UCRXIFG: //UCRXIFG- 0x02 Receive buffer full
#ifdef SPI_B0
                SpiB0RxIsr();
#endif
        break;
        case UCIV__UCTXIFG: //UCTXIFG- 0x04 Transmit buffer empty
#ifdef SPI_B0
                SpiB0TxIsr();
#endif
        break;
        case UCIV__UCTXCPTIFG: //UCTXCPTIFG - 0x08 Transmit complete
        break;
    }
    LPM3_EXIT;
}
#endif // #ifdef USCIAB0TX_VECTOR



#ifdef EUSCI_A1_VECTOR
#pragma vector = EUSCI_A1_VECTOR
__interrupt void EUSCI_A1_ISR (void)	{
    switch(__even_in_range(UCA1IV,UCIV__UCTXCPTIFG)) {
        case UCIV__UCRXIFG: //UCRXIFG- 0x02 Receive buffer full
#ifdef SPI_A1
                SpiA1RxIsr();
#endif
#ifdef COMM_A1
                UartA1RxIsr();
#endif
        break;
        case UCIV__UCTXIFG: //UCTXIFG- 0x04 Transmit buffer empty
#ifdef SPI_A1
                SpiA1TxIsr(); 
#endif
#ifdef COMM_A1
                UartA1TxIsr();
#endif
        break;
        case UCIV__UCTXCPTIFG: //UCTXCPTIFG - 0x08 Transmit complete
        break;
    }
    LPM3_EXIT;
}


#pragma vector = EUSCI_B1_VECTOR
__interrupt void EUSCI_B1_ISR (void) {
    switch(__even_in_range(UCB1IV,UCIV__UCTXCPTIFG)) {
        case UCIV__UCRXIFG: //UCRXIFG- 0x02 Receive buffer full
#ifdef SPI_B1
                SpiB1RxIsr();
#endif
        break;
        case UCIV__UCTXIFG: //UCTXIFG- 0x04 Transmit buffer empty
#ifdef SPI_B1
                SpiB1TxIsr();
#endif
        break;
        case UCIV__UCTXCPTIFG: //UCTXCPTIFG - 0x08 Transmit complete
        break;
    }
    LPM3_EXIT;
}
#endif // #ifdef USCIAB1TX_VECTOR
