//        LCD_JHD12864.c


/*******************************************************
DRIVER IC CHIP: Sitronix ST7565R, Serial Interface.



You must define the following in SysDep.h:

void DelayMs();


#define LCD_SDA				0x01
#define LCD_SCL				0x02
#define LCD_A0				0x04
#define LCD_RST				0x08
#define LCD_CS				0x10

** The LCD control pins may be on a different port or the same port as the SPI lines, so we must define both separately
** LCD CONTROL LINES:
#define LCD_ALL			(LCD_CS|LCD_RST|LCD_A0)
#define LCD_OUT			P3OUT
#define LCD_DIR			P3DIR
#define LCD_SEL			P3SEL
** LCD SPI LINES:
** #define LCD_SPI_ALL		(MOSI|SCLK)
#define LCD_SPI_OUT		P3OUT
#define LCD_SPI_DIR		P3DIR
#define LCD_SPI_SEL		P3SEL



#define LCD_SPI_OUT	// The SCLK and MOSI(DATA) pins may be on a different port.

If using the spi port then place the following in SysDep.h:
#define LCD_SPI
otherwise the LCD interface is bit-banged.

 ******************************************************/

#include "LCD_JHD12864.h"
#ifdef LCD_SPI
#include "SpiMsp.h"
#endif

FONT_INFO* spFont;
ICON_INFO *spIcon;

#define LCD_ROWS_PER_PAGE					8
#define LCD_CTRLR_COLS						132		// total number of addressable columns, including unused.
#define LCD_UNUSED_COLS						(LCD_CTRLR_COLS-LCD_NUM_COLS)		// controller is 132 pixels, display is 128, in reverse mode the first 4 bits are not used.


typedef struct lcd_spi_t {
	BYTE spi_port;
	BYTE rate;
} LcdSpi;
LcdSpi	sLcdSpi;

BOOL sLcdCursorOn;


// LCD Commands
#define LCDCMD_NOP							0xE3	// 1 byte
#define LCDCMD_STATIC_INDICATOR				0xAC	// 2 bytes
#define LCDCMD_WAKE							0xAC	// 2 bytes
#define LCDCMD_SLEEP						0xAD	// 2 bytes
#define LCDCMD_REG_RESISTOR					0x20	// 1 byte:  (1_Rb/Ra) ratio, 5 default
#define LCDCMD_POW_CTRL						0x28	// 1 byte: power supply control register
#define LCDCMD_SHL_NORMAL					0xC0	// 1 byte: COM output direction COM0 to COM63, or reverse
#define LCDCMD_SHL_REVERSE					0xC8	// 1 byte: COM output direction COM0 to COM63, or reverse
#define LCDCMD_RESET						0xE2	// 1 byte: reset the column and page addresses.
#define LCDCMD_RESET_MOD_READ				0xEE	// 1 byte:
#define LCDCMD_SET_MOD_READ					0xE0	// 1 byte:
#define LCDCMD_BIAS0						0xA2	// 1 byte:
#define LCDCMD_BIAS1						0xA3	// 1 byte:
#define LCDCMD_ALL_ON_NORMAL				0xA4	// 1 byte: Normal display oposite of ALL On
#define LCDCMD_ALL_ON						0xA5	// 1 byte: Force all of the display on
#define LCDCMD_REVERSE_NORMAL				0xA6	// 1 byte: Don't reverse pixel colors
#define LCDCMD_REVERSE						0xA7	// 1 byte: Reverse pixel colors
#define LCDCMD_SEG_DIR_NORMAL				0xA0	// 1 byte: SEG0 to SEG131
#define LCDCMD_SEG_DIR_REVERSE				0xA1	// 1 byte: SEG131 to SEG0
#define LCDCMD_COL_MSB						0x10	// 1 byte: sets upper nibble
#define LCDCMD_COL_LSB						0x00	// 1 byte: sets lower nibble
#define LCDCMD_PAGE							0xB0	// 1 byte: lower nibble is page: 0 to 8.
#define LCDCMD_REF_VOLT						0x81	// 2 bytes: lower 6 bits set contrast. Big means more contrast.
#define LCDCMD_FIRST_LINE					0x40	// 1 byte: lower 6 bits sets the COM line (0 to 63).
#define LCDCMD_DISPLAY_ON					0xAF	// 1 byte: turn display on
#define LCDCMD_DISPLAY_OFF					0xAE	// 1 byte: turn display off

// LCDCMD_STATIC_INDICATOR flags first byte
#define FLG_SI_WAKE							0x01	//
// LCDCMD_STATIC_INDICATOR flags second byte
#define FLG_SI_OFF							0x00	// off
#define FLG_SI_BLINK_ONESEC					0x01	// Blink every second
#define FLG_SI_BLINK_HALFSEC				0x02	// Blink every half second
#define FLG_SI_ON_ALWAYS					0x03	// always on
// LCDCMD_REG_RESISTOR flags
#define FLG_RRES_30							0x00	// 3.0
#define FLG_RRES_35							0x01	// 3.5
#define FLG_RRES_40							0x02	// 4.0
#define FLG_RRES_45							0x03	// 4.5
#define FLG_RRES_50							0x04	// 5.0		// default
#define FLG_RRES_55							0x05	// 5.5
#define FLG_RRES_60							0x06	// 6.0
#define FLG_RRES_64							0x07	// 6.4
// LCDCMD_POW_CTRL flags
#define FLG_POWCTRL_VF						0x01	// Voltage follower is on
#define FLG_POWCTRL_VR						0x02	// Voltage regulator is on
#define FLG_POWCTRL_VC						0x04	// Voltage converter is on
// LCDCMD_REF_VOLT flags
#define FLG_MAX_CONTRAST					0x3F
// LCD STATUS register flags
#define FLG_BUSY							0x80
#define FLG_ADC_NORM						0x40
#define FLG_DISP_OFF						0x20
#define FLG_RESET							0x10

#ifndef PROGRESS_BAR_WEIGHT
#define PROGRESS_BAR_WEIGHT					1
#endif


// Static (local) Functions
void LcdDataWrite(BYTE data);
void LcdCmdWrite(BYTE command);

void LcdInit() {
	sLcdCursorOn= false;
#ifdef LCD_SPI
	LCD_DIR|= LCD_ALL;
	LCD_OUT&= ~LCD_ALL;			// all low
#endif

	LCD_DIR|=  LCD_ALL;			// all outputs
	LCD_OUT&= ~LCD_ALL;			// all low
	LCD_OUT &= ~LCD_RST;		// Reset Display
	DelayMs(10);
	LCD_OUT |= LCD_RST;
	DelayMs(10);
#ifdef LCD_SPI
	LcdSpiTake();
#endif
	LcdCmdWrite(LCDCMD_BIAS0);	//Set Bias Ratio: (1010001X), bit0=0= normal, bit0=1 reverse.
	LcdCmdWrite(LCDCMD_SEG_DIR_NORMAL);	//Segment Direction Select: (1010 000X), bit0=0 is normal, bit0=1 is reverse.
	LcdCmdWrite(LCDCMD_SHL_REVERSE);	//Common Direction Select: (1100 1X00),bit3=0 is normal direction, else reverse direction.
	LcdCmdWrite(LCDCMD_FIRST_LINE|0x00);
	LcdCmdWrite(LCDCMD_STATIC_INDICATOR);	//Static Indicator On/Off:  (1010 110X), bit0=0 is off, bit0=1 is on. Second byte is flashing mode (0000 00XX) bit0= 0 is no flash, but0=1 is flash;
	LcdCmdWrite(LCDCMD_REVERSE_NORMAL);	//Pixels Normal/Reverse: bit0=0 is black pixels on white background, bit0=1 is white pixels on black background. (1010 011X)
	LcdCmdWrite(LCDCMD_ALL_ON_NORMAL);	//All points on/off:  bit0=0 is on, bit0=1 is off. (1010 010X)
	LcdCmdWrite(LCDCMD_POW_CTRL|FLG_POWCTRL_VC);	//Power Control Set, last 3 bits (0010 1XXX)
	DelayMs(50);
	LcdCmdWrite(LCDCMD_POW_CTRL|FLG_POWCTRL_VC|FLG_POWCTRL_VR);	//Power Control Set, last 3 bits (0010 1XXX)
	DelayMs(50);
	LcdCmdWrite(LCDCMD_POW_CTRL|FLG_POWCTRL_VC|FLG_POWCTRL_VR|FLG_POWCTRL_VF);	//Power Control Set, last 3 bits (0010 1XXX)
	DelayMs(50);
	LcdCmdWrite(LCDCMD_REG_RESISTOR|FLG_RRES_50);	//set internal resistor ratio:  ra/rb 0x25 last 3 bits (0010 0XXX). default ratio is 5



	LcdCmdWrite(LCDCMD_REF_VOLT);	// Set Contrast, the second value contains the 6 bit value (00XX XXXX).
	LcdCmdWrite(0x27);	// 27 is maximum contrast value.
	LcdOriginSet(0,0);
	LcdScreenFill(LCD_WHITE);
	LcdCmdWrite(LCDCMD_DISPLAY_ON); 		//Display ON
}

#ifdef LCD_SPI
void LcdSpiInit(BYTE spi_port,WORD rate) {
	sLcdSpi.spi_port= spi_port;
	sLcdSpi.rate= rate;
}

void LcdSpiTake() {
	// DATA is latched in on rising edge of clock.
	while(SpiBusy(sLcdSpi.spi_port)) WATCHDOG_CLEAR;				// CHECK BUSY before switching over!!!!!!!!!!!!!!!!
#ifdef UCSSEL__SMCLK  // used in MSP430FR2xxx series processors. 
	SpiPortSetCtrl(sLcdSpi.spi_port,sLcdSpi.rate,(UCSSEL__SMCLK|UCCKPL|UCMSB|UCMST|UCMODE_0|UCSYNC)); // clock inactive high,
#else	// used in MSP430G2xxx series processors. 
	SpiPortSetCtrl(sLcdSpi.spi_port,sLcdSpi.rate,(UCCKPL|UCMSB|UCMST|UCMODE_0|UCSYNC)); // clock inactive high,
#endif
	SpiPortSet(sLcdSpi.spi_port,(WORD*)&LCD_OUT,LCD_CS,0x00);		// Make sure we null out the port and CS.
}
#endif

void LcdDataWrite(BYTE data) {
#ifdef LCD_SPI
	LCD_OUT |= LCD_A0;
	SpiXfer(sLcdSpi.spi_port,&data,NULL,1,true);
	while(SpiBusy(sLcdSpi.spi_port)) WATCHDOG_CLEAR;		// must complete before exiting since buffer is on stack
#else
	int i,j;
	j=0x80;
	LCD_OUT |= LCD_A0;
	LCD_OUT &= ~LCD_CS;
	for(i=0;i<8;i++) {
		LCD_OUT &= ~LCD_SCL;
		if(data & j)LCD_OUT |= LCD_SDA;
		else LCD_OUT &= ~LCD_SDA;
		LCD_OUT |= LCD_SCL; // clock inactive high.
		j>>=1;
	}
	LCD_OUT|= LCD_CS;
#endif
}


void LcdCmdWrite(BYTE command) {
#ifdef LCD_SPI
	LCD_OUT &= ~LCD_A0;
	SpiXfer(sLcdSpi.spi_port,&command,NULL,1,true);
	while(SpiBusy(sLcdSpi.spi_port)) WATCHDOG_CLEAR;		// must complete before exiting since buffer is on stack
#else
	int i,j;
	j=0x80;
	LCD_OUT &= ~LCD_A0;
	LCD_OUT &= ~LCD_CS;
	for(i=0;i<8;i++) {
		LCD_OUT &= ~LCD_SCL;
		if(command&j)LCD_OUT |= LCD_SDA;
		else LCD_OUT &= ~LCD_SDA;
		LCD_OUT |= LCD_SCL;
		j>>=1;
	}
	LCD_OUT |= LCD_CS;
#endif
}


void LcdSleep() { // Consumes about 4uA in this mode.
	LcdCmdWrite(LCDCMD_SLEEP);
	LcdCmdWrite(0x00);
	LcdCmdWrite(LCDCMD_DISPLAY_OFF);
	LcdCmdWrite(LCDCMD_ALL_ON);

}

void LcdWake() {
	LcdCmdWrite(LCDCMD_ALL_ON_NORMAL);
	LcdCmdWrite(LCDCMD_DISPLAY_ON);
	LcdCmdWrite(LCDCMD_WAKE);
	LcdCmdWrite(0x00);
}

void LcdColAddr(BYTE col) {
//	col+= sOffsetX;	// to account for unused display pixels.
	LcdCmdWrite(LCDCMD_COL_MSB|(col>>4));
	LcdCmdWrite(LCDCMD_COL_LSB|(col&0x0F));
}

void LcdPageAddr(BYTE page) {
	LcdCmdWrite(LCDCMD_PAGE|page);
}

void LcdOriginSet(BYTE x,BYTE page) {	// Set the starting column and page address.
	LcdColAddr(x);
	LcdPageAddr(page);
}

/******************************** Drawing Functions *********************************************************/

void LcdPageFill(BYTE page, BYTE fill) {
	int i;
	LcdOriginSet(0,page);
	for(i=0;i<LCD_WIDTH;i++) {
		LcdDataWrite(fill);
	}
}

void LcdScreenFill(BYTE fill) {
	int i;
	for(i=0;i<LCD_NUM_PAGES;i++) {
		LcdPageFill(i,fill);
	}
}

void LcdSquareFill(BYTE x1, BYTE page1, BYTE x2, BYTE page2, BYTE fill) {
	// draw a square, page heights are completely filled in.
	BYTE i,j;
	BYTE len;
	BYTE temp;

	if(x1>x2) { // make sure x2 > s1.
		temp=x1; // swap
		x1=x2;
		x2=temp;
	}
	if(page1>page2) { // make sure page2 > page1.
		temp=page1; // swap
		page1=page2;
		page2=temp;
	}

	len= x2-x1;
	for(i=page1;i<=page2;i++) {
		LcdOriginSet(x1,i);
		for(j=0;j<len;j++) {
			LcdDataWrite(fill);
		}
	}
}


void LcdLineHorizontalDraw(BYTE x1, BYTE x2, BYTE y, BYTE line_thickness) {
	// Draw a horizontal line from x1 to x2, at y with a given line thickness.
	// If the line thickness +calculated shift within the pages is > 8, then it will be trunkated.
	BYTE i;
	BYTE len;
	BYTE row_data=0;
	BYTE page;
	BYTE shift;
	BYTE temp;

	page= y/LCD_NUM_PAGES;
	shift= y%LCD_ROWS_PER_PAGE;

	for(i=0;i<line_thickness;i++){
		row_data<<=1;
		row_data|=0x01;
	}

	row_data= row_data<<shift;

	if(x1>x2) { // make sure x2 > x1.
		temp=x1; // swap
		x1=x2;
		x2=temp;
	}
	len= x2-x1;


	LcdOriginSet(x1,page);
	for(i=0; i<len; i++) {
		LcdDataWrite(row_data);
	}
}

void LcdCursorOn(BOOL on) {
	sLcdCursorOn= on;
}

BYTE LcdCursorDraw(BYTE x, BYTE page) {
	// the cursor is an underscore of size 5 pixels
	int i,j;
	BYTE row_data= 0xC0;
	BYTE len= 10;

	for(j=0;j<spFont->char_height-1;j++) {
		LcdOriginSet(x,page + j);
		for(i=0; i<len; i++) {
			LcdDataWrite(LCD_WHITE);
		}
	}
	LcdOriginSet(x,page + spFont->char_height-1);
	for(i=0; i<len; i++) {
		LcdDataWrite(row_data);
	}
	return x+len;
}

void LcdLineVerticalDraw(BYTE x, BYTE page1, BYTE page2, BYTE line_thickness) {
	// Draws a vertical line from y1 to y2, at x with line thickness.
	// The full height of the calculated start and end pages are draw.
	BYTE i,j;
	BYTE temp;

	if(page1>page2) { // make sure y2 > y1.
		temp=page1; // swap
		page1=page2;
		page2=temp;
	}

	for(i=page1;i<=page2;i++) {
		LcdOriginSet(x,i);
		for(j=0;j<line_thickness;j++) {
			LcdDataWrite(LCD_BLACK);
		}
	}
}

void LcdProgressBarDraw(int progress,int max, BYTE x,BYTE page,BYTE width, BYTE height) { //Draw the box for the progress bar
	// height is number of pages
	// width is in pixels
	// (x, page) origin
	// max is the 100% value
	// progress is assumed to be 0 based.

	BYTE actual_height = height - 1;

	if((height < 1)||(width < 1)||(max < progress)) return;

	if(height > 1){
		LcdLineHorizontalDraw(x, x + width, page * LCD_PIXELS_PER_PAGE, PROGRESS_BAR_WEIGHT);
		LcdLineHorizontalDraw(x, x + width, ((page + actual_height)*LCD_PIXELS_PER_PAGE) + 7, PROGRESS_BAR_WEIGHT); // +7 is the bottom line
	} else {
		LcdSquareFill(x, page, x + width, page, 0x81);  	//Draw top and bottom lines of the progress bar in the same page 0b10000001
	}
	LcdLineVerticalDraw(x, page, page + actual_height, PROGRESS_BAR_WEIGHT);
	LcdLineVerticalDraw(x + width, page, page + actual_height, PROGRESS_BAR_WEIGHT);

	LcdSquareFill(x,page, x + ((progress  *width) / max), page + actual_height, LCD_BLACK);
}

void LcdPixelsDraw(BYTE val,BYTE x, BYTE page, BYTE pixel_mask) { // draw pixel_mask at a location
	BYTE i;
	WORD pixels= pixel_mask;

	for(i=0;i<val;i++) { // convert value into pixel location
		pixels<<=1;
	}
	LcdOriginSet(x,page);
	LcdDataWrite(pixels);

	pixels= pixels & 0xFF00;
	if(pixels) {// crossed page boundary
		pixels= __swap_bytes(pixels); // intrinsic function
		page++;
		if(page<LCD_NUM_PAGES) {
			LcdOriginSet(x,page);
			LcdDataWrite(pixels);
		}
	}
}

void LcdGraphDraw(float* data, int head, int len, BYTE x, BYTE p1, BYTE p2, float margin) { // scale to specified window and plot
	// head points to the most recent data. wrap(head-1) is the tail.
	WORD i;
	int tail;
	float min;
	float max;
	float height;
	float scale;
	BYTE val;

	height= (float)((p2-p1+1)*LCD_PIXELS_PER_PAGE);

	min= data[0];
	max= data[0];
	for(i=0;i<len;i++) {
		if(data[i]<min) min= data[i];
		if(data[i]>max) max= data[i];
	}
	LcdSquareFill(x,p1,x+len,p2,LCD_WHITE);
	min= min-margin;
	max= max+margin;
	if((max-min)==0) return;
	scale= height/(max-min);
	for(i=0;i<len-1;i++) {
		val= (BYTE)(height-(data[i]-min)*scale);
		LcdPixelsDraw(val%LCD_PIXELS_PER_PAGE,x+i,val/LCD_NUM_PAGES+p1,0x3); // 0x3 is a double line
	}
	// draw cursor at tail.
	tail= head+1; // convert to tail.
	if(tail>=LCD_WIDTH) tail= 0;
	for(i=p1;i<=p2;i++) {
		LcdOriginSet(tail,i);
		LcdDataWrite(0xFF);
	}
}

/******************************** String Functions *********************************************************/

void LcdFontSet(FONT_INFO *pfont) {
	spFont= pfont;
}

FONT_INFO* LcdFontGet() {
	return spFont;
}

BYTE LcdCharWrite(char ch,BYTE x, BYTE page, BOOL inv) { // inv is white pixels, rather than black
	// prints the character wherever the pointer is.
	// x,y are pixel coordinates.
	int i,j;
	int char_offset;
	int char_idx;
	int char_width;
	int offset=0;

	// Find starting index into font array
	char_idx= ch - spFont->start_char;
	char_offset= spFont->descriptors[char_idx].offset;
	char_width= spFont->descriptors[char_idx].width;
	// Currently you have to place the character vertically at a page origin, not offset within a page
	// (this keeps big fonts from spanning more than two pages)

	// Lower Byte (LSB is the top-most pixel of the char)


	for(j=0;j<spFont->char_height;j++) { // over multiple byte rows.
		LcdOriginSet(x,page);
		for(i=0;i<char_width;i++) {
			if (inv){
				LcdDataWrite(~(spFont->bitmaps[char_offset+i+offset]));
			} else {
				LcdDataWrite(spFont->bitmaps[char_offset+i+offset]);
			}
		}
		for(i=0;i<spFont->space_x;i++) { // trailing space (for upper part of char)
			if (inv){
				LcdDataWrite(0xFF);  // would have to shift this if the char was not page-aligned
			} else {
				LcdDataWrite(0x00);
			}
		}
		offset+= char_width;
		page++;
	}
	return x + char_width + spFont->space_x;
}

int LcdStringWrite(char *str,BYTE x, BYTE page, BYTE inv) {
	while(*str) {
		x= LcdCharWrite(*str,x,page,inv);
		str++;
	}
	return x;
}

int LcdStringWidth(char *str) {
	// computes the number of x pixels for a given string.
	// Find starting index into font array
	int x= 0;
	int char_idx;
	int char_width;

	while(*str) {
		char_idx= *str - spFont->start_char;
		char_width= spFont->descriptors[char_idx].width;
		x+= char_width+ spFont->space_x;
		str++;
	}
	return x;
}

int LcdStringJustifiedLocation(char *str,BYTE justify,BYTE x_anchor) {
	//Finds the draw location of the passed string based on a given anchor point (x). Returns left-most edge of text in desired location.
	BYTE left_x;
	switch(justify){
	case JUSTIFY_LEFT:
		left_x = x_anchor;
		break;
	case JUSTIFY_RIGHT:
		left_x = x_anchor-LcdStringWidth(str);
		break;
	case JUSTIFY_CENTER:
		left_x = x_anchor-(LcdStringWidth(str)>>1); //  >>1 is divide by 2
		break;
	}
	return left_x;
}



int LcdStringJustifiedWrite(char *str,BYTE page,BYTE justification, BYTE inv) { // erases before and after.
	BYTE x;
	BYTE fill;

	if(inv) fill= LCD_BLACK;
	else fill= LCD_WHITE;
	switch(justification) {
		case JUSTIFY_LEFT:
			x= LcdStringWrite(str,0,page,inv);
			if(sLcdCursorOn) {
				x= LcdCursorDraw(x,page);
			}
			LcdSquareFill(x,page,LCD_WIDTH,page+spFont->char_height-1,fill); // errase the rest of the line.
		break;
		case JUSTIFY_RIGHT:
			x= LCD_WIDTH-LcdStringWidth(str);
			LcdSquareFill(0,page,x,page+spFont->char_height-1,fill);
			LcdStringWrite(str,x,page,inv);

		break;
		case JUSTIFY_CENTER:
			x= (LCD_WIDTH-LcdStringWidth(str))>>1;		// >>1 is divide by 2.
			LcdSquareFill(0,page,x,page+spFont->char_height-1,fill);
			x= LcdStringWrite(str,x,page,inv);
			LcdSquareFill(x,page,LCD_WIDTH,page+spFont->char_height-1,fill);
		break;
	}
	return x;
}






/******************************** Icon Functions *********************************************************/

void LcdIconSet(ICON_INFO *picon) {
	spIcon= picon;
}

ICON_INFO * LcdIconGet() {
	return spIcon;
}

BYTE LcdIconWidth(WORD icon_idx){
	return spIcon->descriptors[icon_idx].width;
}

BYTE LcdIconHeight(WORD icon_idx){ // in pages
	return spIcon->descriptors[icon_idx].height;
}

WORD LcdIconDraw(WORD icon_idx,BYTE x, BYTE page) {
	BYTE i,j;
	BYTE icon_width,icon_height;
	WORD icon_offset;
	BYTE* bitmap;

	icon_offset= spIcon->descriptors[icon_idx].offset;
	icon_width= spIcon->descriptors[icon_idx].width;
	icon_height= spIcon->descriptors[icon_idx].height;
	bitmap = spIcon->bitmaps+ icon_offset;

	for(j=0;j<icon_height;j++) {
		LcdOriginSet(x,page+j);
		for(i=0;i<icon_width;i++) {
			LcdDataWrite(*bitmap);
			bitmap++;
		}
	}
	return x + icon_width;
}
