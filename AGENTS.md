# Agent Guidance

## Project

- This is a TI Code Composer Studio project for the MSP430FR2476.
- The project is configured for the TI MSP430 compiler 21.6.1.LTS. Keep device and compiler-specific code compatible with that toolchain unless the project configuration is intentionally changed.
- Hardware pin assignments, peripheral mappings, and timing constants are defined in `SysDep.h`. Check those definitions and the board configuration before changing hardware-facing code.
- The project references a shared `MSP_COMMON` directory through a machine-specific path. If headers or sources from that dependency are unavailable, report the missing dependency rather than replacing or guessing its contents.

- This project is for an automatic chicken door.  The door opens in the morning based on input from a light sensor after a specified time delay, or closes in the same way. The end user can set the settings through a 5 key keypad and view status and a settings menu on the display. The device may be operated either by line power, solar power or battery power, so if battery mode is selected, it will be important to optimize power. 
- Upon power up the unit will flash all LEDS, beep, and display a sequence of screens on the display, the Vegetronix icon for a second, the firmware version and date on the second screen for a second, and then it will display the top status screen. The status screen will show the current time, the state of the door, low battery icon, if the battery is low. In battery mode - the screen will timeout after 5 minutes.  When the screen has text on it, the display illumination LED should be on. 
- If on the status screen and the center button is pushed the top menu should be displayed on the LCD. If on the status screen and the UP button is pressed the door should lift if it is not already all the way up.  If on the status screen and the DOWN button is pressed the door should lift if it is not already all the way down.
- The top level menu has these options: OPEN, CLOSE, METHOD, POWER MODE, SET TIME.

- The METHOD has 2 options selected as radio button: LIGHT, and TIME.  LIGHT means that the door is controlled by the light level, and TIME means an absolute time.  
- The OPEN and CLOSE menu has these options: LIGHT LEVEL, DELAY, TIME. When options are selected, it brings up another screen that allows you enter a value, such as a number input filed, checkbox, or radio selection. 
- If light level is selected, a percentage may be entered.  This defaults to a 50%. This represents a threshold for the door to take action by raising or lowering after a delay. This option and the delay option are used if the METHOD option is set for LIGHT. 
 If Delay is entered, the number of minutes may be selected. The default is 10 minutes.  This is the number of minutes to delay until a door action occurs. 
 If Time is selected, then an absolute time is selected for the opening and closing of the door, and the light sensor and delay is ignored if the METHOD option is set for TIME.
- POWER MODE is a radio with the options: BATTERY, SOLAR, LINE.  BATTERY mode should optimize all functions to preserve battery power, and SOLAR and LINE mode are essentially the same and it assume that the device has unlimited power.  
- SET TIME allows you to set the current time of the real time clock. 

-The hall effect sensor detects a magnet embedded on the door, when the door raises to the fully open position the hall effect sensor detects the magnet and stops the door. The bottom position of the door 
is detected by the current sensor. When first powered the door will calibrate itself, by detecting the lower and upper positions. It will remind the user to set the time if in TIME mode. If it's in light mode it will move into the open or closed position depending on the level of light immediately ignoring the delay. 
- The time to open and the time to close will be tracked and stored in to the flash memory, and serve as a diagnostic.  For example if it takes too short  for it to open it will indicated an error.  
- The buzzer should be tied to the Timer whose output generates a frequency corresponding to musical notes and should sound a melody when opening, and when closing, to alert the chickens to go in, or come out. 
- The leds should show an appropriate action takes place.   They should also flash periodically (every 10 minutes) at night to show the state of the door from a distance, flashing red for open, and green for closed. They can also be used to express errors, and key board presses.  The buzzer should also emit a click upon a key press. 





- This project targets hardware that includes: 
	an LCD Display on a SPI port,
	the LCD display has an white LED to provide background illumination to the display, 
	a hall effect sensor input,
	a battery voltage level input, 
	a h-bridge motor driver, 
	5 push buttons (up, down, center, left, right),
	current sensor input that detects if the motor is blocked or straining to lift or close the door,
	4 green and 4 red leds, the leds are controlled by an 74HC595 shift register tied to the spi port, the leds alternate Red, Green with red at bit 0. The red LEDs share a common current limiting resistor, and the green LEDs share a separate common current limiting resistor, and thus no more than 1 led per color should be lit at any given time.  The LEDS are physically in a line.  
	a PWM status output, the PWM signal is low pass filtered to create a settable voltage that another processor could read to get status, such as the state of the door being closed open, etc.
	a buzzer whose frequency is controlled by one of the on-board timers
	a 32678Hz crystal that keeps real time clock synchronized.  







## Build And Verification

- Import the existing project into Code Composer Studio and build the project configuration there.
- `Debug/` contains generated build files and outputs. Do not edit generated files there to make a source change; change the project or source inputs instead.
- `targetConfigs/` is managed by CCS according to the project settings. Treat its configuration as generated unless the task explicitly calls for a manually managed target configuration.
- No automated test suite is apparent in the project. Report what was actually built or tested, and do not claim hardware behavior is verified without testing on the target board.

## Change Constraints

- Keep changes focused and consistent with the existing C and CCS project conventions.
- Preserve MSP430 register behavior, interrupt handling, clock assumptions, and timing-sensitive behavior unless the requested change requires modifying them.
- For hardware-facing changes, explain any assumptions about board wiring or peripheral configuration and identify the relevant on-target verification needed.
- Do not change project paths, compiler settings, linker settings, or target configuration as a workaround for a local environment issue without an explicit requirement.
- Do not change lnk_msp430fr2476.cmd
- Only modify the ChickenDoor.c file as the other files are shared common files. 