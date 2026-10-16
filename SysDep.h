// SysDep.h

#ifndef SYSDEP_H
#define SYSDEP_H
#include  <msp430.h>

#define WATCHDOG_CLEAR	//WDTCTL = WDTPW+WDTCNTCL
#define WATCHDOG_HOLD 	WDTCTL = WDTPW + WDTHOLD;

/***************** Defines ****************/

#define	BYTE				unsigned char
#define	WORD				unsigned short
#define	BOOL				unsigned char
#define	DWORD   			unsigned long
#ifndef false
#define	false   			0x00
#endif
#ifndef true
#define	true				0xFF
#endif
#ifndef NULL
#define	NULL				0x00
#endif

#define MIN(a,b) (((a) < (b)) ? (a) : (b))


#define ACLK_HZ				(32768)
#define DCO_HZ				16012800			// 16 MHz
#define MCLK_HZ				DCO_HZ
#define SMCLK_HZ			DCO_HZ
#define TICS_PER_SEC		(2)
#define SAMPLES_PER_SEC 	(4)

#define LCD_SPI
#define SPI_B1				0x08


// PORT 1
#define EYE				BIT0	 // light sensor (Analog input)
#define CUR				BIT1     // Current sensor (Analog input)
#define SDA				BIT2	 // RESERVED FOR I2C Slave Port
#define SCL 			BIT3	 // RESERVED FOR I2C Slave Port 
#define RSVD1			BIT4	 // Reserved for potential comm port UCA0TXD
#define RSVD2			BIT5	 // Reserved for potential comm port UCA0RXD
#define BATS			BIT6	 // Battery Voltage sense (Analog input)
#define HALL			BIT7	 // Hall effect sensor (Digital Input)

#define HALL_SEL0		P1DIR
#define HALL_SEL1		P1DIR
#define HALL_DIR		P1DIR
#define HALL_OUT		P1OUT
#define HALL_REN		P1REN
#define HALL_IE			P1IE
#define HALL_IN			P1IN
#define HALL_IFG		P1IFG

#define USB_SNS_DIR 	P1DIR
#define USB_SNS_REN		P1REN
#define USB_SNS_OUT		P1OUT
#define USB_SNS_IN		P1IN

#define TX_A0			RX_USB
#define RX_A0			TX_USB
#define CTS_A0			CTS


#define ANALOGS_IN		(EYE | CUR |BATS)

#define ADC_SEL0		P1SEL0
#define ADC_SEL1		P1SEL1
#define ADC_REN			P1REN
#define ADC_DIR			P1DIR

#define P1UNUSED		(0)

// PORT 2
#define XIN				BIT0 		// 32,768Hz Crystal input
#define XOUT			BIT1		// 32,768Hz Crystal output
#define EYE_POW			BIT2		// powers the light sensor, high powers on, low off.  Turn off when sleeping
#define KEY_L			BIT3		// Keypad LEFT  - when a key is pressed it goes low.  when released the internal pull up takes it high. 
#define KEY_D			BIT4		// Keypad DOWN
#define KEY_C			BIT5		// Keypad CENTER
#define KEY_U			BIT6		// Keypad UP
#define KEY_R			BIT7		// Keypad RIGHT

#define KEYPAD_ALL		(KEY_U|KEY_D|KEY_C|KEY_L|KEY_R)
#define KEYPAD_SEL0		P2SEL0
#define KEYPAD_SEL1		P2SEL1
#define KEYPAD_DIR		P2DIR
#define KEYPAD_IN		P2IN
#define KEYPAD_OUT		P2OUT
#define KEYPAD_REN		P2REN
#define KEYPAD_IE		P2IE
#define KEYPAD_IFG		P2IFG
#define KEYPAD_IES		P2IES

#define P2UNUSED		(0)

#define EYE_POW_SEL0	P2DIR
#define EYE_POW_SEL1	P2DIR
#define EYE_POW_DIR		P2DIR
#define EYE_POW_OUT		P2OUT
#define EYE_POW_REN		P2REN

// PORT 3

#define P3_0			BIT0		// unused
#define P3_1  			BIT1		// unused
#define MOSI			BIT2		// MOSI
#define LCD_RST			BIT3		// LCD RST
#define LCD_A0			BIT4		// LCD A0
#define SCLK			BIT5		// SPI SCLK
#define LCD_CS			BIT6		// Chip select for LCD
#define RCLK_1			BIT7		// Chip select for HC595 shift register.  

#define HC595_1_SEL0	P3SEL0
#define HC595_1_SEL1	P3SEL1
#define HC595_1_DIR		P3DIR
#define HC595_1_OUT		P3OUT
// The 8 LEDS are on the output of the 74HC595 shift register.  4 red and 4 green, they alternate in color with red at 0, green at 1, etc. 


#define SPIB1_SEL0      P3SEL0
#define SPIB1_SEL1      P3SEL1
#define SPIB1_DIR       P3DIR
#define SCLKB1          SCLK
#define MOSIB1          MOSI
#define MISOB1          0 // MISO  // not used.
#define SPIB1_ALL       (SCLKB1|MOSIB1)

#define LCD_ALL     	(LCD_CS | LCD_RST | LCD_A0)
#define LCD_OUT     	P3OUT
#define LCD_DIR     	P3DIR
#define LCD_SEL0    	P3SEL0
#define LCD_SEL1    	P3SEL1
#define LCD_REN     	P3REN

#define LCD_SPI_ALL     (MOSI|SCLK)
#define LCD_SPI_OUT     P3OUT
#define LCD_SPI_DIR     P3DIR
#define LCD_SPI_SEL0    P3SEL0
#define LCD_SPI_SEL1    P3SEL1

#define P3UNUSED		(P3_0 | P3_1)

// PORT 4
#define STATE			BIT0		// PWM output runs through a low pass RC circuit to create a DAC. 3.3V: open, 0V: Closed
#define RP				BIT1		// H-BRIDGE Positive output
#define RN	 			BIT2		// H-BRIDGE Megative output
#define P4_3			BIT3		
#define P4_4			BIT4		
#define P4_5			BIT5		
#define LED_DISP		BIT6		// the back light LED to the LCD display - low turns in on, high turns it off. 
#define BUZ				BIT7		// BUZZER output connected to TIMERB0.1 output - 

#define P4UNUSED		(P4_3 | P4_4 | P4_5)

#define MOT_ALL			(RP | RN)	// use these aliases to reference the MOTOR port
#define MOT_SEL0		P4DIR
#define MOT_SEL1		P4DIR
#define MOT_DIR			P4DIR
#define MOT_OUT			P4OUT
#define MOT_REN			P4REN

#define BUZ_SEL0		P4DIR
#define BUZ_SEL1		P4DIR
#define BUZ_DIR			P4DIR
#define BUZ_OUT			P4OUT
#define BUZ_REN			P4REN


#define LED_DISP_SEL0		P4DIR		// use these aliases to reference the LED_DSP port
#define LED_DISP_SEL1		P4DIR
#define LED_DISP_DIR		P4DIR
#define LED_DISP_OUT		P4OUT
#define LED_DISP_REN		P4REN

// PORT 5
#define TEST1			BIT0
#define TEST2			BIT1
#define TEST3 			BIT2


#define TEST_ALL		(TEST1|TEST2|TEST3)
#define TEST_DIR		P5DIR
#define TEST_SEL0		P5SEL0
#define TEST_SEL1		P5SEL1
#define TEST_IN			P5IN
#define TEST_REN		P5REN
#define TEST_OUT		P5OUT

#define P5UNUSED		(0)


void DelayMs(WORD num_ms);

// MUST BE WORD ALIGNED!!!
typedef struct t_flashvar { 				// all variables stored in flash
	WORD		magic;						// the magic number
	float       temp_offset;                //
	float		light_scale;				//
	float		light_offset;				//
	WORD		sleep_delay;				// in seconds.
	BOOL 		is_celsius;
	BOOL		sleep_allow;				// allows unit to go to sleep.
	BOOL 		time_military;
} FlashVars;




#endif // #ifndef SYSDEP_H
