// ChickenDoor.c

/************ Includes ********************/
#include "SysDep.h"
#include "SpiMsp.h"
#include "LCD_JHD12864.h"
#include "Hc595.h"
#include "IconCommon.h"
#include "Font.h"			
#include "TimeDate.h"
#include "FlashFR2X.h"
#include "math.h"
#include <string.h>
#include <stdio.h>


/*************************************************
Copyright 2026, Vegetronix, Inc.
Project: Chicken Door
*************************************************/

/************************************************
Hardware and Compiler:

Processor: MSP430FR2476
ACLK Speed: 32.678KHz
MCLK Speed: Internal Oscillator. 

Compiler:  Code Composer Studio V4
Compiler Settings:
Project -> Properties -> C/C++ Build -> Tools Settings -> Library Function Assumptions:
In the LEVEL OF PRINTF SUPPORT REQUIRED change the option MINIMAL to FULL.
Project -> Properties -> C/C++ Build -> Build Steps:
Apply Predefined Step:  Create Flash Image:  TI-TXT


Version 	0.1  1/1/2028  -  Initial 

*********************************************/

/**********************************************************


 ************************************************************/

extern const FONT_INFO vCROSDMono_16ptFontInfo;
extern const FONT_INFO vCROSDMono_26ptFontInfo;
extern const ICON_INFO IconCommon_IconInfo;

/************ Defines *********************/

#define VERSION "0.1"

#define NUM_ADC_VALS		7
#define TAR_MAX             (32768-1)

#define TIMERA0_CLK			(ACLK_HZ/8)	// 4096Hz, Keep larger than KEY_DEBOUNCE_TIME
#define TA0R_MAX			(TIMERA0_CLK/TICS_PER_SEC)
#define TA0_CCR0			(TA0R_MAX-1)

#define TIMERA1_CLK			(ACLK_HZ/2)	// 16096Hz, Keep larger than KEY_DEBOUNCE_TIME
#define TA1R_MAX			(TIMERA1_CLK/SAMPLES_PER_SEC)
#define TA1_CCR0			(TA1R_MAX-1)


#define KEY_DEBOUNCE_TIME	(TA0_CCR0/2)	// 500ms/4= 250ms

#define KEYPAD_LED_ON_TICS	1		// Number of seconds keypad led remains on, after push. 

#define ADC_RANGE			4096
#define ADC_MAX				(ADC_RANGE-1)

#define VCC					(3.3)
#define VREF				(VCC) 


// The ALS-PT19-315C/L177/TR8 phototransistor has a Lux/voltage relationship of lux= 5*Ic(uA).
// The collector resistor is 100 ohms.  Ic(uA)= Vc/Rc;   Vc= Rc*Ic;
// Vc= VREF*Vadc/ADC_MAX;
// Rc*Ic= VREF*Vadc/ADC_MAX;
// Ic= VREF*Vadc/ADC_MAX/Rc;
// LUX= 5*1M*Ic;
// LUX= 5*1M*VREF*Vadc/ADC_MAX/Rc;
// LUX= Vadc*(lux_factor); where lux_factor= (5*1M*VREF/ADC_MAX/Rc)=  40.283203125.
// When compared to my the chineese lux meter, it's off by 1.25.


#define LIGHT_FACTOR		(40.283203125*1.25)   // 1.25 is fudge factor.
#define	BAT_FACTOR			(ADC_RANGE/VREF)	// 	
#define LOW_BATTERY_VOLTAGE	(2.2*(BAT_FACTOR))		// 2.2V*ADC_RANGE/VREF= 2731(ADC) 
#define DIODE_DROP			(.34)


#define IDX_LIGHT    	0
#define IDX_CUR			1
#define IDX_VBAT		6



#define MA_SIZE_TEMP    16.0
#define MA_SIZE_LUX     4.0

#define MAX_SLEEP_TIME	99		// in minutes
#define MIN_SLEEP_TIME	0		// 0= always on. 

#define MAX_DISPLAY_LINES		4	// we can display up to 4 lines on the LCD 

#define ORDER_IDX_TOP  			0
#define ORDER_IDX_LIGHT			1
#define ORDER_IDX_TEMP			2
#define ORDER_IDX_TIME			3

// A row is defined as the height of a small font text line, which is 2 pages.
#define LCD_PAGES_PER_ROW		2									//Each row is 2 pages tall.
#define LCD_NUM_ROWS			(LCD_NUM_PAGES/LCD_PAGES_PER_ROW)	//The row count is used when displaying strings like in the menu
#define LCD_FIRST_ROW			0
#define LCD_LAST_ROW			(LCD_NUM_ROWS - 1)

#define LCD_SPACE_WIDTH			5		// width in pixels of the space character ' '.

#define LCD_CHAR_WIDTH			11

// The MSP430F22XX has BSL V2.0.  By default if you send the wrong password. It will mass erase the whole memory
// including the info memory.  If you have clock calibration coefficients that you are using, then this can wipe
// them out, and cause erratic clock behavior.  If the DCO clock has a bad register value, it's possible for the DCO
// to no start. If the DCO doesn't start, then the JTAG wont be accessible.  Under these circumstances the only option
// is to program through the BSL, a program that fixes the DCO registers.  Once this is sent via BSL, the JTAG should
// then be accessible.
// The following pragma stores 0x0000 to 0xFFDE.  This memory location controls the BSL failed password behavior.
// We set it to 0x0000, so that a bad password doesn't erase the entire memory, which is the default.

// There are two binary interfaces that can be used:  COFF and EABI.  The math functions for EABI are not very optimized and
// so they will waste code space.  So if you are tight on space, as in this program, it is recommended to use COFF, otherwise,
// flash memory will be exceeded.  To retain the bsl password. There are two different approaches depending upon which interface
// is used.  EABI has the following method:
//
// ------ EABI METHOD:
// #pragma RETAIN(bsl_password_behavior)
// #pragma location=0xFFDE   // Only use this is EABI is use.  EABI is not efficient.
// const WORD bsl_password_behavior = 0x0000;
//
// Whereas COFF has a more complex method. You have to mess with the linker command file to create a special section
// Under MEMORY section create this page:
//	MYBSL					: origin = 0xFFDE, length = 0x0002
//  If there is a memory overlap when it compiles then reduce the length of the FLASH section by 2.
// Under SECTIONS create this page:
//     .mybsl		: {} > MYBSL
// Then in the c file place this:
// #pragma DATA_SECTION(bsl_password_behavior,".mybsl")
// const WORD bsl_password_behavior = 0x0000;

//#pragma DATA_SECTION(bsl_password_behavior,".mybsl")
//const WORD bsl_password_behavior = 0x0000;

/************ TypeDefs *********************/


WORD gTAR_capture;

volatile typedef struct capture_t {
	WORD	tic;
	DWORD	sec;
} Capture;

Capture gCapture;

int gDistState;

typedef struct adc_t {
	int ready;
	int chan;
	WORD vals[NUM_ADC_VALS];		//
} Adc;

volatile Adc gAdc;

WORD gKeypadLedTic;

BOOL gSleeping= false;

typedef struct keypad_t {
	WORD capt_tar;	// capture timer for debounce. 
	WORD temp_key;
	WORD key;		// 0 if no key.  
}	Keypad;

Keypad gKeypad;

volatile BOOL gUpdateDisplay= false;

int gTimeShow;

typedef struct sleep_t {
	WORD last_press_tics;
	WORD last_press_secs;
} SleepTime;

SleepTime gSleep;

volatile FlashVars gFlashVars;

/********************* Main State **********************/
#define ST_SLEEPING						0x00
#define ST_BANNER1						0x01
#define ST_BANNER2						0x02
#define ST_BANNER3						0x03
#define ST_BANNER4						0x04
#define ST_SCREEN						0x05
#define ST_SCREEN_NEXUS					0x06
#define ST_SCREEN_ALL					0x07
#define ST_SCREEN_LIGHT					0x08

#define ST_MENU_DRAW					0x10
#define ST_MENU_NEXUS					0x11
#define ST_CHECKBOX_DRAW				0x12

#define ST_RADIO_BUTTON_DRAW			0x15
#define ST_RADIO_BUTTON_NEXUS			0x16

#define ST_MENU_SPIN_DRAW				0x17
#define ST_MENU_SPIN_NEXUS				0x18

#define ST_MENU_REAL_FIELD_DRAW			0x19
#define ST_MENU_REAL_FIELD_NEXUS		0x1A

#define ST_MENU_TIME_FIELD_NEXUS		0x1C
#define ST_MENU_TIME_FIELD_DRAW			0x1D



#define ST_MENU_SETTINGS				0x30
#define ST_MENU_SLEEP_DELAY				0x31
#define ST_MENU_SLEEP_DELAY_SAVE		0x32
#define ST_MENU_SLEEP_BLOCK				0x33
#define ST_MENU_SLEEP_BLOCK_SAVE		0x34
#define ST_MENU_BATTERY					0x35
#define ST_MENU_BATTERY_NEXUS			0x36

#define ST_MENU_LIGHT					0x50
#define ST_MENU_LIGHT_OFFSET			0x51
#define ST_MENU_LIGHT_OFFSET_SAVE		0x52
#define ST_MENU_LIGHT_SCALE				0x53
#define ST_MENU_LIGHT_SCALE_SAVE		0x54

#define ST_MENU_TIME					0x70
#define ST_MENU_TIME_SHOW				0x71
#define ST_MENU_TIME_SHOW_SAVE			0x72
#define ST_MENU_TIME_MILITARY			0x73
#define ST_MENU_TIME_MILITARY_SAVE		0x74
#define ST_MENU_TIME_SET				0x75
#define ST_MENU_TIME_SET_DRAW			0x76
#define ST_MENU_TIME_SET_NEXUS			0x77
#define ST_MENU_SAMPLE_PERIOD_SAVE		0x78	// snick not used.


#define NUM_ORDER_ITEMS  		4
BYTE gOrderItems[NUM_ORDER_ITEMS];


/************ Function Prototypes *********/
void OscillatorInit();
void Software_Trim();

void TimerA0Init(void);
void TimerA0Capture(void);
BOOL TimerExpired(WORD interval);

void BackLightOn(BOOL on);
void Adc12Init();

void EyePowInit();
void EyePowOn(BOOL on);
void Wake();
void Sleep();

void SleepTimerClear();
BOOL SleepTimerExpired();
inline void SleepTimerProc();
BOOL ProbeInactive();


/**************** Interrupt ISRs ****************************/

#pragma vector = PORT2_VECTOR
__interrupt void P2Isr ()	{
	gKeypad.capt_tar= TA0R;
	gKeypad.temp_key= (KEYPAD_IFG & KEYPAD_ALL); 
	KEYPAD_IFG&= ~KEYPAD_ALL;
	LPM3_EXIT; // wake up
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0A0_ISR ()	{  // 2Hz.
	static int tics=0;
	TimeDateProc();
	tics++;
	if(tics >= TICS_PER_SEC) {
		tics= 0;
	}
	if (!gSleeping) {
		SleepTimerProc();
		gKeypadLedTic++;
	}	
}


/*#pragma vector = TIMER1_A1_VECTOR
__interrupt void Timer1_A1_ISR(void) {
    switch (__even_in_range(TA1IV, TA1IV_TAIFG)) {
        case TA1IV_NONE:
            break;
        case TA1IV_TACCR1:
            break;
        case TA1IV_TACCR2: // CCR2 capture/compare event.
            break;
        case TA1IV_TAIFG:  // Timer overflow event.
            break;
        default:
            break;
    }
}*/


#pragma vector= unused_interrupts
__interrupt void ISR_trap(void)
{
  // the following will cause an access violation which results in a PUC reset
  WDTCTL = 0;
}



/*************** System Functions **************************/

// Note that for 16MHz - you need 3.3V.  If it's less you can't run at this
// speed. Or need to up the voltage first.
#define MCLK_FREQ_MHZ 16 // MCLK = 16MHz

#define MCLK_FREQ_MHZ 16

void OscillatorInit(void) {
	__delay_cycles(200000);
    // External 32.768 kHz crystal:
    P2REN  &= ~(XIN | XOUT);
    P2SEL0 |=  (XIN | XOUT);
    P2SEL1 &= ~(XIN | XOUT);

    // Restore normal low-frequency crystal configuration:
    // maximum drive, AGC enabled, bypass disabled,
    // automatic shutdown when XT1 is unused.
    CSCTL6 = XT1DRIVE_3 | XT1AUTOOFF;

    // Select XT1 for ACLK so the oscillator is requested.
    // Keep MCLK/SMCLK on DCOCLKDIV.
    CSCTL4 = SELMS__DCOCLKDIV | SELA__XT1CLK;

    // TI sample's oscillator fault-clearing sequence.
    do {
        CSCTL7 &= ~(XT1OFFG | DCOFFG);
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);

    // One FRAM wait state for MCLK above 8 MHz.
    FRCTL0 = FRCTLPW | NWAITS_1;

    __bis_SR_register(SCG0);       // Disable FLL

    // Retain REFO as the DCO reference during diagnosis.
    CSCTL3 = SELREF__REFOCLK;
    CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1
           | DCORSEL_5;
    CSCTL2 = FLLD_0 | 487;

    __delay_cycles(3);
    __bic_SR_register(SCG0);       // Enable FLL

    Software_Trim();

    // MCLK/SMCLK approximately 16 MHz, without division.
    CSCTL5 &= ~(DIVM_7 | DIVS_3);

    // ACLK remains sourced from the external crystal.
    CSCTL4 = SELMS__DCOCLKDIV | SELA__XT1CLK;
	if (1) {
		P1DIR |= BIT7;  //
		P1SEL1 |= BIT7; // P1.7 to SMCLK

	    P2SEL0 &= ~BIT2;
		P2SEL1 |= BIT2; // P2.2 to ACLK
		P2DIR |= BIT2;  //
		P2REN &= ~BIT2;
	}
}


/*void OscillatorInit() {
	__delay_cycles(200000); // delay 0.2 seconds for VH to come on. 

	// Configure one FRAM waitstate as required by the device datasheet for MCLK
	// operation beyond 8MHz _before_ configuring the clock system.
	FRCTL0 = FRCTLPW | NWAITS_1;
	__bis_SR_register(SCG0);   // disable FLL
	CSCTL3 |= SELREF__REFOCLK; // Set REFO as FLL reference source
	CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1 |
			DCORSEL_5;    // DCOFTRIM=5, DCO Range = 16MHz
	CSCTL2 = FLLD_0 + 487; // DCOCLKDIV = 16MHz
	__delay_cycles(3);
	__bic_SR_register(SCG0); // enable FLL
	Software_Trim();         // Software Trim to get the best DCOFTRIM value
	CSCTL4 = SELMS__DCOCLKDIV |
			SELA__REFOCLK;  // set default REFO(~32768Hz) as ACLK source, ACLK =
							// 32768Hz, default DCOCLKDIV as MCLK and SMCLK source
	if (0) {
		P1DIR |= BIT7;  //
		P1SEL1 |= BIT7; // P1.7 to SMCLK
		P2DIR |= BIT2;  //
		P2SEL1 |= BIT2; // P2.2 to ACLK
	}
}*/


void Software_Trim() {
  unsigned int oldDcoTap = 0xffff;
  unsigned int newDcoTap = 0xffff;
  unsigned int newDcoDelta = 0xffff;
  unsigned int bestDcoDelta = 0xffff;
  unsigned int csCtl0Copy = 0;
  unsigned int csCtl1Copy = 0;
  unsigned int csCtl0Read = 0;
  unsigned int csCtl1Read = 0;
  unsigned int dcoFreqTrim = 3;
  unsigned char endLoop = 0;

  do {
    CSCTL0 = 0x100; // DCO Tap = 256
    do {
      CSCTL7 &= ~DCOFFG; // Clear DCO fault flag
    } while (CSCTL7 & DCOFFG); // Test DCO fault flag

    __delay_cycles((unsigned int)3000 *
                   MCLK_FREQ_MHZ); // Wait FLL lock status (FLLUNLOCK) to be
                                   // stable Suggest to wait 24 cycles of
                                   // divided FLL reference clock
    while ((CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)) && ((CSCTL7 & DCOFFG) == 0))
      ;

    csCtl0Read = CSCTL0; // Read CSCTL0
    csCtl1Read = CSCTL1; // Read CSCTL1

    oldDcoTap = newDcoTap;           // Record DCOTAP value of last time
    newDcoTap = csCtl0Read & 0x01ff; // Get DCOTAP value of this time
    dcoFreqTrim = (csCtl1Read & 0x0070) >> 4; // Get DCOFTRIM value

    if (newDcoTap < 256) // DCOTAP < 256
    {
      newDcoDelta = 256 - newDcoTap; // Delta value between DCPTAP and 256
      if ((oldDcoTap != 0xffff) && (oldDcoTap >= 256)) // DCOTAP cross 256
        endLoop = 1;                                   // Stop while loop
      else {
        dcoFreqTrim--;
        CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim << 4);
      }
    } else // DCOTAP >= 256
    {
      newDcoDelta = newDcoTap - 256; // Delta value between DCPTAP and 256
      if (oldDcoTap < 256)           // DCOTAP cross 256
        endLoop = 1;                 // Stop while loop
      else {
        dcoFreqTrim++;
        CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim << 4);
      }
    }

    if (newDcoDelta < bestDcoDelta) // Record DCOTAP closest to 256
    {
      csCtl0Copy = csCtl0Read;
      csCtl1Copy = csCtl1Read;
      bestDcoDelta = newDcoDelta;
    }

  } while (endLoop == 0); // Poll until endLoop == 1

  CSCTL0 = csCtl0Copy; // Reload locked DCOTAP
  CSCTL1 = csCtl1Copy; // Reload locked DCOFTRIM
  while (CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1))
    ; // Poll until FLL is locked
}

void ClockSetFast() {
	// Call after your existing 16 MHz oscillator initialization.
	// Keep the existing FRAM wait-state setting.
    CSCTL5 = (CSCTL5 & ~DIVM_7) | DIVM__1;
}

void ClockSetSlow() {
    CSCTL5 = (CSCTL5 & ~DIVM_7) | DIVM__16; //DIVM__128
}

void DelayMs(WORD num_ms) { // This functions delays about num_ms milliseconds.
	WORD i;

	for(i=0;i<num_ms;i++) {
		__delay_cycles(MCLK_HZ/1000);
		WATCHDOG_CLEAR;
	}
}

inline float CtoF(float deg_c) {
    return (1.8)*deg_c+32.0;
}

inline  float FtoC(float deg_v) {
    return (deg_v-32.0)*0.5555555555555556;
}


//******************* TimerA0 *****************************/

void TimerA0Init(){
	TimeDate td;
	TA0CTL = TASSEL__ACLK | ID__8 | MC__STOP | TACLR;		// set the clock source to ACLK, set clock divider to clk, enable the interrupt generator
	TA0EX0 = TAIDEX_0;        								// No additional division
	TA0CCR0 = TA0_CCR0;										// reset clock and generate interrupt twice a second
	TA0CCTL0= CCIE;
	TA0CTL = TASSEL__ACLK | ID__8 | MC__UP | TACLR;
	TimerA0Capture();
	td.tic= 0;
	td.sec= 0;
	td.min= 0;
	td.hour= 14;
	td.day= 1;
	td.month= 0;
	td.year= 2011;	 
	TimeDateSet(&td);
}

void TimerA0Capture() {
	TimeDate td;
	TimeDateGet(&td);
	gCapture.sec= td.sec;
}

BOOL TimerA0Expired(WORD sec) {
	TimeDate td;
	TimeDateGet(&td);
	if((td.sec - gCapture.sec) < sec) return false;
	else return true;
}

inline void TimerA0Pause(BOOL pause) {
	if(pause) TA0CCTL0&= ~CCIE;
	else TA0CCTL0= CCIE;
}


void TimerA1Init() {
	// This timer controlls the adc sample rate
    TA1CTL = TASSEL__ACLK | MC__STOP | TACLR;

    TA1EX0 = TAIDEX_0;          // Expansion divider: /1
    TA1CCTL0 = 0;               // No CCR0 interrupt
    TA1CCTL1 = 0;               // Initialize output low

    TA1CCR0 = TA1_CCR0;        	// Period: 16384 / 16384 = 1 s
    TA1CCR1 = TA1CCR0/2;       	// 50% Duty Cycle
    TA1CCTL1 = OUTMOD_3;   		// Set at CCR1; reset at CCR0

    TA1CTL = TASSEL__ACLK      // ACLK source
           | ID__2             // Input divider: /2
           | MC__UP            // Count from 0 through CCR0
           | TACLR;            // Clear counter and start
}

WORD gTA1CCR0_save;
void TimerA1Faster() {                 
       gTA1CCR0_save= TA1CCR0;
       TA1CCR0 = TIMERA1_CLK/32;      // Period: 16384 / 32 = 
    TA1CCR1 = TA1CCR0/2;               // 50% Duty Cycle
}

void TimerA1Restore() {
       TA1CCR0 = gTA1CCR0_save;
    TA1CCR1 = TA1CCR0/2;               // 50% Duty Cycle
}


// ************************* ADC12 Functions ************************************

// ************************* ADC12 Functions ************************************

#pragma vector = ADC_VECTOR
	__interrupt void adc_isr(void) {
	// This interrupt triggers when the adc is finished sampling
	switch (__even_in_range(ADCIV, ADCIV_ADCIFG)) {
		case ADCIV_NONE:
		break;
		case ADCIV_ADCOVIFG:
		break;
		case ADCIV_ADCTOVIFG:
		break;
		case ADCIV_ADCHIIFG:
		break;
		case ADCIV_ADCLOIFG:
		break;
		case ADCIV_ADCINIFG:
		break;
		case ADCIV_ADCIFG:
			// Reads lowest first, so reverse order.
			gAdc.vals[gAdc.chan] = ADCMEM0;
			if (gAdc.chan == 0) {
				__no_operation(); // set break point here  Only for debugger
				gAdc.chan = NUM_ADC_VALS - 1;
				gUpdateDisplay= true;
			} else
				gAdc.chan--;
		break;
		default:
		break;
	}
}

void Adc12Init() {
	gAdc.chan = NUM_ADC_VALS - 1;
	ADC_DIR  &= ~ANALOGS_IN;            // Inputs
    ADC_REN  &= ~ANALOGS_IN;            // Disable pull-up/down resistors
    ADC_SEL0 |= ANALOGS_IN;             // SEL1:SEL0 = 11 selects analog
    ADC_SEL1 |= ANALOGS_IN;


	ADCCTL0 |= ADCSHT_2 | ADCON; // 16ADCclks, ADC ON
	ADCCTL1 |= ADCSHP | ADCSHS_2 | ADCCONSEQ_3; // ADC clock MODCLK, sampling timer, TA1.1B trig.,repeat sequence
	ADCCTL2 &= ~ADCRES;                  // clear ADCRES in ADCCTL
	ADCCTL2 |= ADCRES_2;                 // 12-bit conversion results
	ADCMCTL0 |= ADCINCH_3 | ADCSREF_0;   // A0~3(EoS); Vref=VCC
	ADCIE |= ADCIE0;                     // Enable ADC conv complete interrupt
	ADCCTL0 |= ADCENC;                   // enable ADC
}

void Adc12Sleep() {
    TA1CTL &= ~MC_3;                 // Stop trigger timer
    TA1CCTL1 &= ~(CCIE | CCIFG);

    ADCIE = 0;                      // Disable ADC interrupts
    ADCCTL0 &= ~ADCENC;
    ADCCTL1 &= ~ADCCONSEQ_3;         // Abort sequence: single mode
    ADCCTL0 &= ~(ADCON | ADCSC);
    ADCIFG = 0;                     // Discard pending flags
}

void Adc12Wake(){
    // Keep the trigger timer stopped during setup.
    TA1CTL &= ~MC_3;

    ADCCTL0 &= ~ADCENC;
    ADCCTL1 = (ADCCTL1 & ~ADCCONSEQ_3) | ADCCONSEQ_3;
    ADCCTL0 |= ADCON;

    gAdc.chan = NUM_ADC_VALS - 1;   // First conversion is A3
    gUpdateDisplay = false;

    ADCIFG = 0;
    ADCIE = ADCIE0;
    ADCCTL0 |= ADCENC;

    // Reset trigger output low and restore its configuration.
    TA1CCTL1 = 0;
    TA1CCTL1 = OUTMOD_3;

    TA1CTL = TASSEL__ACLK | ID__2 | MC__UP | TACLR;
}

/********************* Push Button Functions **********************/

void KeypadInit() {
	gKeypad.key= 0;
	gKeypad.temp_key= 0;
	
	KEYPAD_SEL0&= ~KEYPAD_ALL;
	KEYPAD_SEL1&= ~KEYPAD_ALL;
	KEYPAD_DIR&= ~KEYPAD_ALL;
	KEYPAD_OUT|= KEYPAD_ALL;	// PUP
	KEYPAD_REN|=  KEYPAD_ALL;
	KEYPAD_IES|=  KEYPAD_ALL;	// Falling edge
	KEYPAD_IFG&= ~KEYPAD_ALL;	// clear any pending interrupts.
	KEYPAD_IE|=   KEYPAD_ALL;
}

void KeyProc() {
	WORD time;
	WORD cap_tar;
	
	cap_tar= TA0R;
	if(gKeypad.temp_key) { // a key has been pressed.
		// check debounce time. 
		if(gKeypad.capt_tar <= cap_tar) { 
			time= cap_tar- gKeypad.capt_tar;
		} else { // wrapped around 0
			time= cap_tar + (TA0R_MAX-gKeypad.capt_tar);
		}

		if(time >= KEY_DEBOUNCE_TIME) {
			switch(gKeypad.temp_key) { // a key is debounced and ready;
				case 0x08: // left
					gKeypad.key= 'L';
				break;
				case 0x10: // down
					gKeypad.key= 'D';
				break;
				case 0x20: // center
					gKeypad.key= 'C';
				break;
				case 0x40: // up
					gKeypad.key= 'U';
				break;
				case 0x80: // right
					gKeypad.key= 'R';
				break;
			}
			SleepTimerClear();
			gKeypad.temp_key=0;
			gKeypadLedTic= 0;
		}
	}
}

BOOL KeyGet(char* key) { // return key;
	*key= gKeypad.key;
	gKeypad.key= 0;
	return (*key > 0);
}

void MenuProc(void) {  // TEST ONLY NOT USED
	if(gKeypad.key) {
		switch(gKeypad.key) {
			case 'U':
				LcdStringWrite("UP    ",1,1,false);
			break;
			case 'D':
				LcdStringWrite("Down  ",1,1,false);
			break;
			case 'C':
				LcdStringWrite("Center",1,1,false);
			break;
			case 'L':
				LcdStringWrite("Left  ",1,1,false);
			break;
			case 'R':
				LcdStringWrite("Right ",1,1,false);
			break;							
		}
		gKeypad.key= 0;
	}
}

float LightGet() {
	float lux;
	static float lux_ma= 0.0;
	static float lux_prior= 0.0; // prior value

	lux= ((float)gAdc.vals[IDX_LIGHT]-1.0)*LIGHT_FACTOR; // the -1 is to compensate for display light.
	lux_ma= lux_ma + lux - lux_prior;
	lux_prior= lux_ma/MA_SIZE_LUX;
	lux= gFlashVars.light_scale*lux_prior + gFlashVars.light_offset;
    if(lux<0.0) lux= 0.0;
	return lux;
}

void TimeDateToString(char* str) {
	TimeDate td;
	int hour;
	char* suf;

	TimeDateGet(&td);
	if(gFlashVars.time_military) {
		hour= td.hour;
		suf="";
	} else {
		if(td.hour<12) {
			suf="A";
		} else {
			suf="P";
		}
		hour= td.hour%12;
		if(hour==0) hour=12;
	}
	sprintf(str,"%02u:%02u:%02u %s",hour,td.min,td.sec,suf);
}

#define UNIT_LABEL_PAGE				4 // the verticle offset in pages of the unit label.

void ScreenDrawValue(int icon,char* heading_str,char *val_str,char *label_str) {
	int x;
	x= LcdIconDraw(icon,0,0);
	x=(LCD_WIDTH-(x + LcdStringWidth(heading_str)))/2+x; // center in space following icon.
	LcdStringWrite(heading_str,x,0,false);
	LcdLineHorizontalDraw(0, LCD_WIDTH,16,1);

	// write units label
	x= LCD_WIDTH- (LcdStringWidth(label_str)); // right align units label
	LcdStringWrite(label_str,x,UNIT_LABEL_PAGE,false);
	// write value
	LcdFontSet((FONT_INFO *)&vCROSDMono_26ptFontInfo); // Big Font
	x= x-(LcdStringWidth(val_str)); // right align value against units label
	LcdSquareFill(0,3,x,3 + vCROSDMono_26ptFontInfo.char_height - 1,LCD_WHITE);
	x= LcdStringWrite(val_str,x,3,false);
	LcdFontSet((FONT_INFO *)&vCROSDMono_16ptFontInfo);
}

void ScreenLineWrite(int icon,char *str,int page) {
	int x;
	x= (LCD_WIDTH- (LcdStringWidth(str) + LcdIconWidth(ICON_WATER)))/2; // Center Justify with Icon.
	LcdSquareFill(0,page,x,page + vCROSDMono_16ptFontInfo.char_height - 1,LCD_WHITE);
	x= LcdStringWrite(str,x,page,false);
	x= LcdIconDraw(icon,x,page);
	LcdSquareFill(x,page,LCD_WIDTH,page+vCROSDMono_16ptFontInfo.char_height-1,LCD_WHITE);
}


void MenuCheckBox(char* str,BOOL flg,WORD *state) {
	LcdScreenFill(LCD_WHITE);
	LcdStringWrite(str,9,2,false);
	if(flg) {
		LcdIconDraw(ICON_CHECK_YES,50,4);
	} else {
		LcdIconDraw(ICON_CHECK_NO,50,4);
	}
	(*state)++; //go forward 1 stateg
}
BOOL MenuCheckBoxNexus(BOOL *flg,WORD *state) {
	// returns true if enter pressed. 
	BOOL ret= false;
	char key= 0;

	KeyGet(&key);
	switch(key) {
		case 'L':
			(*state)= ST_MENU_DRAW;
		break;
		case 'C':
			(*state)= ST_MENU_DRAW;
			ret= true;
		break;
		case 'D':
		case 'U':
			if(*flg) *flg= false;
			else *flg= true;
			(*state)--; // go back 1 state.
		break;
	}
	return ret;
}

#define COEF_LEN		5

BYTE OrderLineGet(BYTE item) { // find the page, for the given item index
	int i;
	int page;
	for(i=0;i<NUM_ORDER_ITEMS;i++) {
		if(gOrderItems[i]==item) {
			page= i;
			return (BYTE)(page<<1); // <<1 = *2.
		}
	}
	return 0;
}

int minimum(int x, int y) {
	if(x<y) return x;
	else return y;
}

//******************************* MENUS **********************************************


#define MENU_LEAVE				0xFF		// Indicates the top most root of the menu tree.
#define MENU_IDX_MAIN			0			// These are indexes into the menus.
#define MENU_IDX_SETTINGS		1			// These are indexes into the menus.
#define MENU_IDX_LIGHT			2			// These are indexes into the menus.
#define MENU_IDX_TIME			3			// These are indexes into the menus.

typedef struct menu_t {
	BYTE parent;			// parent index
	BYTE num_items;			// Number of menu linesST_RADIO_BUTTON_DRAW
	char cur_item;
	const char * const *strs;
	const char *states;
	//BOOL chk_boxes;
} Menu;

//** MainMenu
const char* const MENU_MAIN_STRS[] = {
	"SETTINGS",
	"LIGHT",
	"TIME"
};
const char MENU_MAIN_STATES[] = {
	ST_MENU_SETTINGS,
	ST_MENU_LIGHT,
	ST_MENU_TIME,
};
//** Settings Menu
const char* const MENU_SETTINGS_STRS[] = {
	"SLEEP DELAY",
	"SLEEP BLOCK",
	"BATTERY",
};
const char MENU_SETTINGS_STATES[] = {
	ST_MENU_SLEEP_DELAY,
	ST_MENU_SLEEP_BLOCK,
	ST_MENU_BATTERY,
};
//** Light Menu
const char* const MENU_LIGHT_STRS[] = {
	"OFFSET",
	"SCALE",
};
const char MENU_LIGHT_STATES[] = {
	ST_MENU_LIGHT_OFFSET,
	ST_MENU_LIGHT_SCALE,
};

//** Time Menu
const char* const MENU_TIME_STRS[] = {
	"SHOW TIME",
	"TIME UNITS",
	"SET TIME",
};
const char MENU_TIME_STATES[] = {
	ST_MENU_TIME_SHOW,
	ST_MENU_TIME_MILITARY,
	ST_MENU_TIME_SET,
};

Menu gMenus[] = {
	{MENU_LEAVE,sizeof(MENU_MAIN_STATES),0,MENU_MAIN_STRS, MENU_MAIN_STATES},
	{MENU_IDX_MAIN,sizeof(MENU_SETTINGS_STATES),0,MENU_SETTINGS_STRS, MENU_SETTINGS_STATES},
	{MENU_IDX_MAIN,sizeof(MENU_LIGHT_STATES),0,MENU_LIGHT_STRS, MENU_LIGHT_STATES},
	{MENU_IDX_MAIN,sizeof(MENU_TIME_STATES),0,MENU_TIME_STRS, MENU_TIME_STATES},
};

#define NUM_MENUS			(sizeof(gMenus)/sizeof(Menu))

//******************************* RADIO BUTTONS **********************************************

typedef struct radio_button_t {
	BYTE parent_menu;		// parent menu
	BYTE return_state;		//
	BYTE num_items;			// number of menu lines
	char cur_item;			// item pointed to by scroll arrow.
	BYTE sel_item; 			// selected index
	const char **strs;
} RadioButton;


const char* SLEEP_BLOCK_OPTIONS[]= {
	"BLOCK",
	"ALLOW",
};

const char* TIME_SHOW_OPTIONS[]= {
	"HIDE",
	"SHOW"
};

const char* TIME_MILITARY[]= {
	"12 HOUR",
	"24 HOUR",
};

#define RADIO_IDX_SLEEP_BLOCK			0
#define RADIO_IDX_TIME_SHOW				1
#define RADIO_IDX_TIME_MILITARY			2



RadioButton gRadioButtons[]= {
	{MENU_IDX_SETTINGS,ST_MENU_SLEEP_BLOCK_SAVE,sizeof(SLEEP_BLOCK_OPTIONS)/2,0,0,SLEEP_BLOCK_OPTIONS},
	{MENU_IDX_TIME,ST_MENU_TIME_SHOW_SAVE,sizeof(TIME_SHOW_OPTIONS)/2,0,0,TIME_SHOW_OPTIONS},
	{MENU_IDX_TIME,ST_MENU_TIME_MILITARY_SAVE,sizeof(TIME_MILITARY)/2,0,0,TIME_MILITARY},
};

#define NUM_RADIO_BUTTONS			(sizeof(gRadioButtons)/sizeof(RadioButton))

//******************************* SPIN FIELDS **********************************************

typedef struct spin_field_t {
	BYTE parent_menu;		// parent menu
	BYTE return_state;		// state where we return.
	short min;
	short max;
	WORD inc;
	short value;
	const char *label;
	const char *units;
} SpinField;


#define SPIN_IDX_SLEEP_DELAY					0

SpinField gSpins[]= {
	{MENU_IDX_SETTINGS,ST_MENU_SLEEP_DELAY_SAVE,10,1000,10,100,"SLEEP DELAY","sec"},
};

#define NUM_SPIN_FIELDS			(sizeof(gSpins)/sizeof(SpinField))

//******************************* REAL FIELDS **********************************************
// floating point input field.

typedef struct real_field_t {
	BYTE parent_menu;		// parent menu
	BYTE return_state;		// state where we return.
	float min;
	float max;
	float value;
	const char *label;
	const char *units;
} RealField;


#define REAL_IDX_TEMP_OFFSET					0
#define REAL_IDX_TEMP_SCALE						1
#define REAL_IDX_LIGHT_OFFSET					2
#define REAL_IDX_LIGHT_SCALE					3

RealField gRealFields[]= {
		{MENU_IDX_LIGHT,ST_MENU_LIGHT_OFFSET_SAVE,-10000,10000,0,"Offset",""},
		{MENU_IDX_LIGHT,ST_MENU_LIGHT_SCALE_SAVE,-10000,10000,1,"Scale",""},
};

#define NUM_REAL_FIELDS			(sizeof(gRealFields)/sizeof(RealField))

//******************************* Time Date FIELDS **********************************************

typedef struct time_field_t {
	BYTE parent_menu;		// parent menu
	BYTE return_state;		// state where we return.
	DWORD min;				// in seconds.
	DWORD max;				// in seconds.
	DWORD value;			// in seconds.
	const char *label;
} TimeField;

#define TIME_IDX_SAMPLE_PERIOD			0

TimeField gTimeFields[]= {
		{MENU_IDX_MAIN,ST_MENU_SAMPLE_PERIOD_SAVE,1,100000,1,"Sample Time"},
};

#define NUM_TIME_FIELDS			(sizeof(gTimeFields)/sizeof(TimeField))


float power10(int exp) { //returns 10^exp
	int i;
	float pow= 1;

	if(exp>0) {
		for(i=0;i<exp;i++) pow*= 10;
	} else if(exp<0){ // negative exponent
		for(i=0;i<-exp;i++) pow*= 0.1;
	} // else return 10^0=1
	return pow;
}

#define NUM_SCREENS				4
#define SCREEN_ALL				0


void MainProc() {
	static int screen= SCREEN_ALL;			// must be signed.  current screen display - 0: all, 2: light, 
	static WORD state= ST_BANNER1;
	static BYTE display_first = 0;
	static BYTE display_last = LCD_LAST_ROW;
	static BYTE cur_menu = MENU_IDX_MAIN;
	static BYTE idx= 0;			// initialized only to avoid compiler warning.
	static int cursor;
	static float inc;
	static char field;
	static TimeDate td;

	char key;
	int i;
	int x;
	float tempf;
	char heading_str[16];
	char str[16];
	char label_str[6];
	int page;
	int digit_offset;
	int num_digits;
	int decimal_pt;
	int inc_pow;
	int icon;
	WORD sec;
	WORD min;
	WORD hour;
	
	if(gFlashVars.sleep_allow) {
		if(SleepTimerExpired()) {
			Sleep();
			state= ST_SLEEPING;
		}
	}

	switch(state) {
		case ST_SLEEPING:	// exit if center key is pressed. 
			if(gKeypad.temp_key == KEY_C) {
				gKeypad.temp_key= 0;
				Wake();
				state= ST_BANNER1;
			} else {
				LPM3;	
			}
		break;
		case ST_BANNER1:
			TimerA0Capture();
			x= (LCD_WIDTH - LcdIconWidth(ICON_VEGETRONIX_LOGO_BIG))/2;
			LcdIconDraw(ICON_VEGETRONIX_LOGO_BIG,x,0);
			state= ST_BANNER2;
		break;
		case ST_BANNER2:
			if(TimerA0Expired(1)) {
				TimerA0Capture();
                LcdScreenFill(LCD_WHITE);
                LcdStringJustifiedWrite("COPYRIGHT",0,JUSTIFY_CENTER,false);
                LcdStringJustifiedWrite("2026",2,JUSTIFY_CENTER,false);
                LcdStringJustifiedWrite("VEGETRONIX",4,JUSTIFY_CENTER,false);
				state= ST_BANNER3;
			}
		break;
		case ST_BANNER3:
			if(TimerA0Expired(1)) {
				TimerA0Capture();
				LcdScreenFill(LCD_WHITE);
				LcdStringJustifiedWrite("CHICKEN",0,JUSTIFY_CENTER,false);
				LcdStringJustifiedWrite("DOOR",2,JUSTIFY_CENTER,false);

				sprintf(str,"VER: %s",VERSION);
				LcdStringJustifiedWrite(str,4,JUSTIFY_CENTER,false);
				state= ST_BANNER4;
			}
		break;
		case ST_BANNER4:
			if(TimerA0Expired(1)) {
				TimerA0Capture();
				KeyGet(&key); 			// clear buffer
				state= ST_SCREEN;
			}
		break;
		case ST_SCREEN:
			LcdScreenFill(LCD_WHITE);
			state= ST_SCREEN_NEXUS;
		break;
		case ST_SCREEN_NEXUS:
			// display samples
			KeyGet(&key);
			switch(key) {
				case 'L': // go back to parent
					LcdScreenFill(LCD_WHITE);
					screen--;
					if(screen<0) screen= NUM_SCREENS-1;
				break;
				case 'C':
					state= ST_MENU_DRAW;
				break;
				case 'R': // next menu
					LcdScreenFill(LCD_WHITE);
					screen++;
					if(screen>=NUM_SCREENS) screen=0;
				break;
			}
			if(state!=ST_MENU_DRAW) {
				switch(screen) {
					case SCREEN_ALL:
						state=  ST_SCREEN_ALL;
					break;
				}
			}
		break;

		case ST_SCREEN_ALL:
			if(gUpdateDisplay) {
				gUpdateDisplay= false;
				if(gAdc.vals[IDX_VBAT] < LOW_BATTERY_VOLTAGE) {
					page= OrderLineGet(ORDER_IDX_TOP); // print low battery 
					x= LCD_WIDTH - LcdIconWidth(ICON_BAT_0); // right justifys
					LcdIconDraw(ICON_BAT_0,x,page);
				}
				//** Light
				page= OrderLineGet(ORDER_IDX_LIGHT);
				tempf= LightGet();
				if(tempf <0.0 ) tempf= 0.0;
				if(tempf < 1000) {
					sprintf(str,"%.0f Lux",tempf);
				} else {
					sprintf(str,"%.1fK Lx",(tempf/1000));
				}
				ScreenLineWrite(ICON_SUN,str,page);
				//** Time
				page= OrderLineGet(ORDER_IDX_TIME);
				if(gTimeShow) {				

					TimeDateToString(str);
					ScreenLineWrite(ICON_TIME,str,page);
				} else {
					LcdSquareFill(0,page,LCD_WIDTH,page+1,LCD_WHITE);
				}
			}
			state= ST_SCREEN_NEXUS;
		break;
	
		//************* DISPLAY MENU ITEMS ***********************

		case ST_MENU_SETTINGS:
			cur_menu= MENU_IDX_SETTINGS;
			state = ST_MENU_DRAW;
		break;
		case ST_MENU_SLEEP_DELAY:
			idx= SPIN_IDX_SLEEP_DELAY;
			gSpins[idx].value= gFlashVars.sleep_delay;
			state = ST_MENU_SPIN_DRAW;
		break;
		case ST_MENU_SLEEP_DELAY_SAVE:
			gFlashVars.sleep_delay= gSpins[idx].value;
			FlashMspVarsSave();
			cur_menu = gSpins[idx].parent_menu;
			state = ST_MENU_DRAW;
		break;
		
		case ST_MENU_SLEEP_BLOCK:
			if(gFlashVars.sleep_allow) gRadioButtons[idx].sel_item= 1; // convert true (0xFF) to an index.
			idx= RADIO_IDX_SLEEP_BLOCK;
			state = ST_RADIO_BUTTON_DRAW;
		break;
		case ST_MENU_SLEEP_BLOCK_SAVE:
			gFlashVars.sleep_allow=  gRadioButtons[idx].sel_item;
			cur_menu = gRadioButtons[idx].parent_menu;
			FlashMspVarsSave();
			state = ST_MENU_DRAW;
		break;
		
		case ST_MENU_BATTERY:
			LcdScreenFill(LCD_WHITE);
			tempf= gAdc.vals[IDX_VBAT]*BAT_FACTOR+DIODE_DROP;
			if(gAdc.vals[IDX_VBAT] < LOW_BATTERY_VOLTAGE) {
				strcpy(heading_str,"REPLACE");
				icon= ICON_BAT_25;
			} else {
				strcpy(heading_str,"GOOD");
				icon= ICON_BAT_100;
			}
			sprintf(str,"%.2f",tempf);
			strcpy(label_str," V  ");
			ScreenDrawValue(icon,heading_str,str,label_str);
			state= ST_MENU_BATTERY_NEXUS;
		break;
		case ST_MENU_BATTERY_NEXUS:
			KeyGet(&key);
			switch(key) {
				case 'L':
					state= ST_MENU_DRAW;
				break;
			}
		break;

		//************* LIGHT MENU ITEMS ******************
		case ST_MENU_LIGHT:
			cur_menu= MENU_IDX_LIGHT;
			state = ST_MENU_DRAW;
		break;
		case ST_MENU_LIGHT_OFFSET:
			idx= REAL_IDX_LIGHT_OFFSET;
			gRealFields[idx].value= gFlashVars.light_offset;
			cursor= 0;
			state = ST_MENU_REAL_FIELD_DRAW;
		break;
		case ST_MENU_LIGHT_OFFSET_SAVE:
			gFlashVars.light_offset= gRealFields[idx].value;
			FlashMspVarsSave();
			cur_menu = gRealFields[idx].parent_menu;
			state = ST_MENU_DRAW;
		break;
		case ST_MENU_LIGHT_SCALE:
			idx= REAL_IDX_LIGHT_SCALE;
			gRealFields[idx].value= gFlashVars.light_scale;
			cursor= 0;
			state = ST_MENU_REAL_FIELD_DRAW;
		break;
		case ST_MENU_LIGHT_SCALE_SAVE:
			gFlashVars.light_scale= gRealFields[idx].value;
			FlashMspVarsSave();
			cur_menu = gRealFields[idx].parent_menu;
			state = ST_MENU_DRAW;
		break;


		//************* TIME MENU ITEMS ***********************
		case ST_MENU_TIME:
			cur_menu= MENU_IDX_TIME;
			state = ST_MENU_DRAW;
		break;
		case ST_MENU_TIME_SHOW:
			if(gTimeShow) gRadioButtons[idx].sel_item= 1; // convert true (0xFF) to an index.
			idx= RADIO_IDX_TIME_SHOW;
			state = ST_RADIO_BUTTON_DRAW;
		break;
		case ST_MENU_TIME_SHOW_SAVE:
			gTimeShow=  gRadioButtons[idx].sel_item;
			FlashMspVarsSave();
			cur_menu = gRadioButtons[idx].parent_menu;
			state = ST_MENU_DRAW;
		break;

		case ST_MENU_TIME_MILITARY:
			if(gFlashVars.time_military) gRadioButtons[idx].sel_item= 1; // convert true (0xFF) to an index.
			idx= RADIO_IDX_TIME_MILITARY;
			state = ST_RADIO_BUTTON_DRAW;
		break;
		case ST_MENU_TIME_MILITARY_SAVE:
			gFlashVars.time_military=  gRadioButtons[idx].sel_item;
			FlashMspVarsSave();
			cur_menu = gRadioButtons[idx].parent_menu;
			state = ST_MENU_DRAW;
		break;		
		

		case ST_MENU_TIME_SET:
			field=0;
			TimeDateGet(&td);
			state = ST_MENU_TIME_SET_DRAW;
		break;
		case ST_MENU_TIME_SET_DRAW:
			page= 3;
			LcdScreenFill(LCD_WHITE);
			LcdStringJustifiedWrite("SET TIME",0,JUSTIFY_CENTER,false);
			LcdLineHorizontalDraw(0, LCD_WIDTH,16,1);
			sprintf(str,"%02d",td.hour);
			x= LcdStringWrite(str,20,page,field==0);
			x= LcdStringWrite(":",x,page,false);
			sprintf(str,"%02d",td.min);
			x= LcdStringWrite(str,x,page,field==1);
			x= LcdStringWrite(":",x,page,false);
			sprintf(str,"%02d",td.sec);
			x= LcdStringWrite(str,x,page,field==2);
			state++;
		break;	
		case ST_MENU_TIME_SET_NEXUS:
			KeyGet(&key);
			switch(key) {
				case 'L':
					state= ST_MENU_DRAW;
				break;
				case 'C': //update
					TimeDateSet(&td);
					state= ST_MENU_DRAW;
				break;
				case 'U':
					switch(field) {
						case 0:
							TimeHourInc(&td);
						break;
						case 1:
							TimeMinInc(&td);
						break;
						case 2:
							TimeSecInc(&td);
						break;
					}
					state = ST_MENU_TIME_SET_DRAW;
				break;
				case 'D':
					switch(field) {
						case 0:
							TimeHourDec(&td);
						break;
						case 1:
							TimeMinDec(&td);
						break;
						case 2:
							TimeSecDec(&td);
						break;
					}
					state=	ST_MENU_TIME_SET_DRAW;
				break;	
				case 'R':
					field++;
					if(field>2) field=0; 
					state = ST_MENU_TIME_SET_DRAW;
				break;		
			}
		break;


		//***************** Common Menu States *************************************
		case ST_MENU_DRAW:
				LcdScreenFill(LCD_WHITE);
				if(gMenus[cur_menu].cur_item < display_first) {
					display_first = gMenus[cur_menu].cur_item;
					display_last = display_first + LCD_LAST_ROW;
				}

				if(gMenus[cur_menu].cur_item > display_last) {
					display_last = gMenus[cur_menu].cur_item;
					display_first = display_last - LCD_LAST_ROW;
				}
				x= LcdIconDraw(ICON_ARROW_RIGHT,0,(gMenus[cur_menu].cur_item - display_first) * LCD_PAGES_PER_ROW);
				for(i=0;i<gMenus[cur_menu].num_items;i++) {
					LcdStringWrite((char*)gMenus[cur_menu].strs[i+display_first],x,i * LCD_PAGES_PER_ROW,NON_INVERTED);
					if(i>=LCD_LAST_ROW) break;
				}

				state = ST_MENU_NEXUS;
				break;
		case ST_MENU_NEXUS:
			if (KeyGet(&key)) {
				switch(key){
				case 'U':
					if(gMenus[cur_menu].cur_item <= 0) {
						gMenus[cur_menu].cur_item = gMenus[cur_menu].num_items - 1;
					} else {
						gMenus[cur_menu].cur_item--;
					}
					state = ST_MENU_DRAW;
					break;
				case 'D':
					if(gMenus[cur_menu].cur_item >= gMenus[cur_menu].num_items - 1) {
						gMenus[cur_menu].cur_item = 0;
					} else {
						gMenus[cur_menu].cur_item++;
					}
					state = ST_MENU_DRAW;
					break;
				case 'L':
					if(gMenus[cur_menu].parent == MENU_LEAVE) {
						state = ST_SCREEN;
					} else {
						cur_menu = gMenus[cur_menu].parent;
						state = ST_MENU_DRAW;
					}
					break;
				case 'R':
				case 'C':
					display_first = 0;
					display_last = LCD_LAST_ROW;
					state = gMenus[cur_menu].states[gMenus[cur_menu].cur_item];
					break;
				}
			}
			break;

		case ST_RADIO_BUTTON_DRAW:
			LcdScreenFill(LCD_WHITE);
			if(gRadioButtons[idx].cur_item < display_first) {
				display_first = gRadioButtons[idx].cur_item;
				display_last = display_first + LCD_LAST_ROW;
			}
			if(gRadioButtons[idx].cur_item > display_last) {
				display_last = gRadioButtons[idx].cur_item;
				display_first = display_last - LCD_LAST_ROW;
			}
			LcdIconDraw(ICON_ARROW_RIGHT,0,(gRadioButtons[idx].cur_item - display_first) * LCD_PAGES_PER_ROW);
			for(i=0;i<gRadioButtons[idx].num_items;i++) {
				if((i+display_first)==gRadioButtons[idx].sel_item) {
					x= LcdIconDraw(ICON_CHECK_YES,LCD_CHAR_WIDTH,i * LCD_PAGES_PER_ROW);
				} else {
					x= LcdIconDraw(ICON_CHECK_NO,LCD_CHAR_WIDTH,i * LCD_PAGES_PER_ROW);
				}
				x+=2; // space between box and text;
				LcdStringWrite((char*)gRadioButtons[idx].strs[i+display_first],x,i * LCD_PAGES_PER_ROW,NON_INVERTED);
				if(i>=LCD_LAST_ROW) break;
			}
			state = ST_RADIO_BUTTON_NEXUS;
		break;
		case ST_RADIO_BUTTON_NEXUS:
			if (KeyGet(&key)) {
				switch(key) {
				case 'U':
					if(gRadioButtons[idx].cur_item <= 0) {
						gRadioButtons[idx].cur_item = gRadioButtons[idx].num_items - 1;
					} else {
						gRadioButtons[idx].cur_item--;
					}
					state = ST_RADIO_BUTTON_DRAW;
					break;
				case 'D':
					if(gRadioButtons[idx].cur_item >= gRadioButtons[idx].num_items - 1){
						gRadioButtons[idx].cur_item = 0;
					} else {
						gRadioButtons[idx].cur_item++;
					}
					state = ST_RADIO_BUTTON_DRAW;
					break;
				case 'L':
					state = gRadioButtons[idx].return_state;
					break;
				case 'R':
				case 'C': // select and save.
					gRadioButtons[idx].sel_item= gRadioButtons[idx].cur_item;
					state = ST_RADIO_BUTTON_DRAW;
					break;
				}
			}
		break;

		case ST_MENU_SPIN_DRAW:
			LcdScreenFill(LCD_WHITE);
			LcdStringJustifiedWrite((char*)gSpins[idx].label, LCD_FIRST_ROW, JUSTIFY_CENTER, NON_INVERTED);
			LcdLineHorizontalDraw(0,LCD_WIDTH-1,17,1);
			LcdIconDraw(ICON_ARROW_UP,0,4);
			x= LcdIconDraw(ICON_ARROW_DOWN,0,6);
			sprintf(str,"%d",gSpins[idx].value);
			x= LcdStringWrite(str, x+5, 5, NON_INVERTED); // +5 is space beteen the up/down arrow icons and the numeric value
			LcdStringWrite((char*)gSpins[idx].units, x+LCD_SPACE_WIDTH,  5, NON_INVERTED);
			state = ST_MENU_SPIN_NEXUS;
		break;
		case ST_MENU_SPIN_NEXUS:
			if(KeyGet(&key)) {
				switch(key) {
				case 'U':
					gSpins[idx].value+= gSpins[idx].inc;
					if(gSpins[idx].value>gSpins[idx].max)
						gSpins[idx].value= gSpins[idx].max;
					state = ST_MENU_SPIN_DRAW;
				break;
				case 'D':
					gSpins[idx].value-= gSpins[idx].inc;
					if(gSpins[idx].value<gSpins[idx].min)
						gSpins[idx].value= gSpins[idx].min;
					state = ST_MENU_SPIN_DRAW;
				break;
				case 'L': // do not save, go back
					cur_menu = gSpins[idx].parent_menu;
					state = ST_MENU_DRAW;
					break;
				case 'R':
				case 'C': // save and go back.
					state = gSpins[idx].return_state;
				break;
				}
			}
		break;

		case ST_MENU_REAL_FIELD_DRAW:
			LcdScreenFill(LCD_WHITE);
			LcdStringJustifiedWrite((char*)gRealFields[idx].label, LCD_FIRST_ROW, JUSTIFY_CENTER, NON_INVERTED);
			LcdLineHorizontalDraw(0,LCD_WIDTH-1,17,1);
			// determin width of the field.
			if(fabs(gRealFields[idx].value) > 1000) {
				sprintf(str,"%.0f",gRealFields[idx].value);
			} else if (fabs(gRealFields[idx].value) > 100.0) {
				sprintf(str,"%.1f",gRealFields[idx].value);
			} else if (fabs(gRealFields[idx].value) > 10.0) {
				sprintf(str,"%.2f",gRealFields[idx].value);
			} else if (fabs(gRealFields[idx].value) > 1.0) {
				sprintf(str,"%.3f",gRealFields[idx].value);
			}  else {
				sprintf(str,"%.5f",gRealFields[idx].value);
			}
			x= LcdStringWrite(str, 5, 5, NON_INVERTED); 	//
			LcdStringWrite((char*)gRealFields[idx].units, x+5, 5, NON_INVERTED); 	//

			num_digits= strlen(str);
			digit_offset= 0;
			inc_pow= 0;
			if(str[0]=='-') {
				inc_pow--;
				num_digits--;
				digit_offset++;
			} else {

			}
			if(strchr(str,'.')) {
				num_digits--;
				decimal_pt= strchr(str,'.')-str-1;
			} else {
				decimal_pt= strlen(str)-1;
			}
			if(cursor>=num_digits) {
				cursor= 0; // wrap around
			}
			inc_pow+= decimal_pt - cursor;
			inc= power10(inc_pow);

			if((cursor+digit_offset)>decimal_pt) {
				digit_offset++; // shift if period
			}
			x= LcdIconDraw(ICON_ARROW_UP,(cursor+digit_offset)*LCD_CHAR_WIDTH+3,7); // cursor is referenced to the most significant cursor.

			state = ST_MENU_REAL_FIELD_NEXUS;
		break;
		case ST_MENU_REAL_FIELD_NEXUS:
			if(KeyGet(&key)) {
				switch(key) {
				case 'U':
					if(gRealFields[idx].value+inc <= gRealFields[idx].max)
						gRealFields[idx].value+= inc;
					state = ST_MENU_REAL_FIELD_DRAW;
				break;
				case 'D':
					if(gRealFields[idx].value-inc >= gRealFields[idx].min)
						gRealFields[idx].value-= inc;
					state = ST_MENU_REAL_FIELD_DRAW;
				break;
				case 'L': // do not save, go back
					cur_menu = gRealFields[idx].parent_menu;
					state = ST_MENU_DRAW;
					break;
				case 'R':
					cursor++;
					state = ST_MENU_REAL_FIELD_DRAW;
				break;
				case 'C': // save and go back.
					state = gRealFields[idx].return_state;
				break;
				}
			}
		break;

		case ST_MENU_TIME_FIELD_DRAW:
			LcdScreenFill(LCD_WHITE);
			LcdStringJustifiedWrite((char*)gTimeFields[idx].label, LCD_FIRST_ROW, JUSTIFY_CENTER, NON_INVERTED);
			LcdLineHorizontalDraw(0,LCD_WIDTH-1,17,1);
			hour= (WORD)(gTimeFields[idx].value / 3600);
			min= (WORD)( (gTimeFields[idx].value / 60) % 60);
			sec= (WORD)(gTimeFields[idx].value % 60);
			sprintf(str,"%0.2u:%0.2u:%0.2u",hour,min,sec);
			x= LcdStringWrite(str, 5, 5, NON_INVERTED); 	//
			switch(cursor) { // cursor is referenced to LSB
				case 0:
					inc= 1;
					x= LCD_CHAR_WIDTH*7+3;		// compute cursor icon location.
				break;
				case 1:
					inc= 10;
					x= LCD_CHAR_WIDTH*6+3;		// compute cursor icon location.
				break;
				case 2:
					inc= 60;
					x= LCD_CHAR_WIDTH*4+3;		// compute cursor icon location.
				break;
				case 3:
					inc= 600;
					x= LCD_CHAR_WIDTH*3+3;		// compute cursor icon location.
				break;
				case 4:
					inc= 3600;
					x= LCD_CHAR_WIDTH*1+3;		// compute cursor icon location.
				break;
				case 5:
					inc= 36000;
					x= LCD_CHAR_WIDTH*0+3;		// compute cursor icon location.
				break;
			}
			x= LcdIconDraw(ICON_ARROW_UP,x,7); // cursor is referenced to the most significant cursor.
			state=  ST_MENU_TIME_FIELD_NEXUS;
		break;
		case ST_MENU_TIME_FIELD_NEXUS:
			if(KeyGet(&key)) {
				switch(key) {
				case 'U':
					if(gTimeFields[idx].value+inc <= gTimeFields[idx].max) gTimeFields[idx].value+= inc;
					state = ST_MENU_TIME_FIELD_DRAW;
				break;
				case 'D':
					if(gTimeFields[idx].value-inc >= gTimeFields[idx].min) {
						gTimeFields[idx].value-= inc;
					} else {
						if(cursor>0) cursor--;
					}
					state = ST_MENU_TIME_FIELD_DRAW;
				break;
				case 'L': // do not save, go back
					cur_menu = gTimeFields[idx].parent_menu;
					state = ST_MENU_DRAW;
					break;
				case 'R':
					cursor--;
					if(cursor<0) cursor=5;		// wrap arround to highest digit 5.
					state = ST_MENU_TIME_FIELD_DRAW;
				break;
				case 'C': // save and go back.
					state = gTimeFields[idx].return_state;
				break;
				}
			}
		break;
		default: // trap should never come here!!!!!
			state = ST_SCREEN;


	} // switch
}

/*************************************** LED Functions **************************/
inline void BackLightInit() {
	LED_DISP_OUT&= ~LED_DISP;
	LED_DISP_DIR|= LED_DISP;
}


inline void BackLightOn(BOOL on) {
	if(on) {
		LED_DISP_OUT|= LED_DISP;
	} else {
		LED_DISP_OUT&= ~LED_DISP;
	}
}

inline void LedPattern(BYTE pat) {
	Hc595SpiTake();
	Hc595Write(&pat,1);
}


/*************************************** Light Sensor Functions **************************/

inline void EyePowInit() {
	EYE_POW_DIR |= EYE_POW;
	EYE_POW_OUT &= ~EYE_POW;
}

inline void EyePowOn(BOOL on) {
	if(on) {
		EYE_POW_OUT|= EYE_POW;
	} else {
		EYE_POW_OUT&= ~EYE_POW;
	}
}

/*************************************** Power/Sleep Functions **************************/




void SleepTimerClear() {
	gSleep.last_press_secs= 0;
	gSleep.last_press_tics= 0;
}

BOOL SleepTimerExpired() {
	return(gSleep.last_press_secs >= gFlashVars.sleep_delay);
}

inline void SleepTimerProc() { // place in timer ISR.
	gSleep.last_press_tics++;
	if(gSleep.last_press_tics >= TICS_PER_SEC) {
		gSleep.last_press_tics=0;
		gSleep.last_press_secs++;
	}
}


void Sleep() { // power everything down.
	// 10/4/2026 Sleep current is ~40uA on USB populated unit.  
#ifdef TC77	
    TempMode(TEMP_SLEEP);
#endif
	Adc12Sleep();
	BackLightOn(false);
	LcdSleep();
    ClockSetSlow();
	gSleeping= true;
	EyePowOn(false);
	LPM3;
}

void Wake() {
	gSleeping= false;
	EyePowOn(false);
	ClockSetFast();
	BackLightOn(true);
	LcdScreenFill(LCD_WHITE);
	LcdWake();
	Adc12Wake();
#ifdef TC77	
	TempMode(TEMP_WAKE);
#endif
	SleepTimerClear();
}

void UnusedPins() {
	P1DIR&= ~P1UNUSED;
	P1OUT&= ~P1UNUSED;
	P1REN|= P1UNUSED;

	P2DIR&= ~P2UNUSED;
	P2OUT&= ~P2UNUSED;
	P2REN|= P2UNUSED;

	P3DIR&= ~P3UNUSED;
	P3OUT&= ~P3UNUSED;
	P3REN|= P3UNUSED;

	P4DIR&= ~P4UNUSED;
	P4OUT&= ~P4UNUSED;
	P4REN|= P4UNUSED;

	P5DIR&= ~P5UNUSED;
	P5OUT&= ~P5UNUSED;
	P5REN|= P5UNUSED;
}

/*void CurrentSleepTest() {
    // put this at start up to test the sleep current.
	  P1SEL0 = P1SEL1 = 0;
      P1DIR&= ~0xFF;
      P1OUT&= ~0xFF;
      P1REN|= 0xFF;

	  P2SEL0 = P2SEL1 = 0;
      P2DIR&= ~0xFF;
      P2OUT&= ~0xFF;
      P2REN|= 0xFF;

	  P3SEL0 = P3SEL1 = 0;
      P3DIR&= ~0xFF;
      P3OUT&= ~0xFF;
      P3REN|= 0xFF;

	  P4SEL0 = P4SEL1 = 0;
      P4DIR&= ~0xFF;
      P4OUT&= ~0xFF;
      P5REN|= 0xFF;

      P5SEL0 = P5SEL1 = 0;
      P5DIR&= ~0xFF;
      P5OUT&= ~0xFF;
      P5REN|= 0xFF;
      LPM3;
}*/


/********************* main **********************/

void main() {
    WATCHDOG_HOLD;
	gTimeShow= true;
	PM5CTL0 &= ~LOCKLPM5; // unlock the GPIO pins and Crystal Pins.
	UnusedPins();
    OscillatorInit(); // PM5CTL0 &= ~LOCKLPM5; must come first.
	
	SpiInit(SPI_B1);
	LcdSpiInit(SPI_B1,1); // called first before LcdInit();
	LcdFontSet((FONT_INFO *)&vCROSDMono_16ptFontInfo);
	LcdIconSet((ICON_INFO *)&IconCommon_IconInfo);	
	Hc595Init(SPI_B1,1);

	if(!FlashMspVarsGet()) {	// must come before all other functions!
		gFlashVars.temp_offset=    0;
		gFlashVars.light_offset=	0;
		gFlashVars.light_scale=		1.0;
		gFlashVars.sleep_delay=		10;
		gFlashVars.is_celsius=		true;
		gFlashVars.sleep_allow= 	true;
		gFlashVars.time_military=	false;
	}
	EyePowInit();
	BackLightInit();
	BackLightOn(true);
	TimerA0Init();
	

	// Default order of items. 
	gOrderItems[0]= 1;  // light
	gOrderItems[3]= 3;  // time 
			
	Adc12Init();
	_EINT();
	TimerA1Init();
	LcdInit(); // must come after interrupts enabled.
	KeypadInit(); // must come after TestProc(), because TestProc messes with the keypad.

	while(1) {
		KeyProc();
		MainProc();
		WATCHDOG_CLEAR;
  	}
}


