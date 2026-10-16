// FlashFR2X.c - MSP Information Memory

//#include "SysDep.h"
#include "FlashFR2X.h"

#define MAGIC_NUMBER    	0xA5A5				// Magic number - written at bytes (0,1) of NV rom. 

/******************************** SETUP **********************************************
 The linker command file should have and information segment already defined:
 INFO                : origin = 0x1800, length = 0x200

 The SysDep.h:

 typedef struct flash_vars_t {
	WORD magic;
 } FlashVars;

 FlashVars gFlashVars;


*********************************************************************************************/

// The following allows us to determine the start of the INFO Memory segment defined in the .cmd file.
// This is normally at 0x1800 and is a size of 0x200.
// Add the following 2 lines to the linker cmd file in the appropriate MEMORY and SECTIONS section:
//		MEMORY
//		{
//			INFO : origin = 0x1800, length = 0x0200
//		}
// 		SECTIONS
// 		{
// 			.info   	: {} > INFO   				/* Map the INFO section to a specific memory region */
// 		}

// ****************************************************************************************************
#pragma DATA_SECTION( FRAM_INFO_MEMORY , ".info")
BYTE FRAM_INFO_MEMORY[0x200];

extern FlashVars gFlashVars;

/************************* FRAM INFO MEMORY ************************************************************/

void FramWrite(BYTE* data, int length) {
    int i=0;
    BYTE *ptr= FRAM_INFO_MEMORY;
    SYSCFG0 = FRWPPW | PFWP;
    for(i = 0; i < length; i++) {
        *ptr++ = data[i];
    }
    SYSCFG0 = FRWPPW | DFWP | PFWP;
}

inline BOOL FlashMspIsValid() {
    return( (*(WORD*)(FRAM_INFO_MEMORY)) == MAGIC_NUMBER);
}

void FlashMspVarsSave() {
    gFlashVars.magic= MAGIC_NUMBER;
    FramWrite((BYTE*)&gFlashVars,sizeof(gFlashVars));
}

BOOL FlashMspVarsGet() {
    if(FlashMspIsValid()) {      // use what's saved in memory
        memcpy((BYTE*)&gFlashVars, FRAM_INFO_MEMORY, sizeof(gFlashVars));
        return true; // success
    } else {                                            // use defaults
        gFlashVars.magic=           MAGIC_NUMBER;
        return false; // failure
    }
}
