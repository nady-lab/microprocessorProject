/*
	BEE GAME - Microprocessors Project

	A game for STM32F031
	Features: Movement with buttons, collision detection, scoring,
		lives system, pause, quit via terminal (Q/q), and restart.

	by Nadyla Barbosa, Emily Bolton & Juleanne Cruz
*/

#include <stm32f031x6.h>
#include "display.h" // LCD functions (fillRectangle, putImage, printText, etc.)
#include "sprites.h" // file with all sprite data (beeRight, beeLeft, flower1, flower2, etc.)
#include <stdlib.h>	 // for rand() function
#include <stdio.h>	 // for snprintf()
#include "serial.h"	 // UART functions: eputchar, egetchar, eputs, printDecimal, initSerial

#include "sound.h"
#include "musical_notes.h"

// function prototypes -> tells the compiler what functions exist before they are defined
void initClock(void);			   // configure system clock to 48MHz
void initSysTick(void);			   // setup millisecond timer interrupt
void SysTick_Handler(void);		   // interrupt handler called every 1ms
void delay(volatile uint32_t dly); // blocking delay using milliseconds
void setupIO();					   // configure all pins (buttons, LEDs, display)

int isInside(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint16_t px, uint16_t py);
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber);
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode);

// functions that controls the LEDs
void redOn(void);
void redOff(void);
void greenOn(void);
void greenOff(void);

void startMenu(void);									   // setup menu for the start of the game
void initGame(int *score, int *lives, int *correctFlower); // reset game state (score, lives, correct flower)
void drawGameInfo(uint32_t pink, int score, int lives);  // draws the score and the remaining lives at the top of the screen
void handlePause(uint32_t pink, uint16_t x, uint16_t y,  // function for when the pause button is pressed
				 int hmoved, int hinverted, int vmoved,
				 int score, int lives,
				 const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[]);
void handleInput(uint16_t *x, uint16_t *y, int *hmoved, int *vmoved, int *hinverted, int *vinverted); // function for when the movement buttons are pressed
void drawBee(uint16_t x, uint16_t y, int hmoved, int hinverted, int vmoved);						  // draws the bee at the right position
void redrawFlowers(const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[]);	  // draws and chooses the "right" flower when used
void checkCollisions(uint16_t *x, uint16_t *y, uint16_t *oldx, uint16_t *oldy,
					 int *score, int *lives, int *correctFlower, int *flowerCheck,
					 const uint16_t *flowers[8], int flowerX[], int flowerY[]);
void gameOverScreen(uint32_t pink, int score); // draws the game over screen and goes back to the startMenu after UP is pressed

// serial communication helpers
int serialAvailable(void); // check if a character is available in UART buffer
int quitRequested(void);   // check if player typed Q or q to quit
void printScore(int);	   // function to print the decimal numbers without the leading zeros

void initSound(void); // initialize sound system (PWM/timer for audio output)
void playNoteFor(uint32_t freq, uint32_t duration_ms); // will play note at a certain frequency for a certain amount of milliseconds
void stopSound(void); // will stop any sound

// GLOBAL VARIABLES
volatile uint32_t milliseconds = 0; // incremented every 1ms by SysTick interrupt

// SCREEN AND SPRITE CONSTANTS
#define SCREEN_W 128 // LCD width in pixels
#define SCREEN_H 160 // LCD height in pixels

// size of the square for each flower sprite
#define FLOWER_W 24 // width for each flower sprite
#define FLOWER_H 24 // height for each flower sprite

// size of rectangle used to erase the bee
#define BEE_CLEAR_W 16 // size of the width
#define BEE_CLEAR_H 16 // size of the height

// MAIN FUNCTION -> controls overall game flow
int main(void)
{
	// define color used for the on-screen text
	uint32_t pink = RGBToWord(0xff, 0xaf, 0xaf);

	// initialize all hardware components
	initClock();   // set system clock
	initSysTick(); // enable 1ms timer interrupt
	initSerial();  // enable UART communication (for quit input)
	setupIO();	   // configure buttons, LEDs, and start display

	// Outer loop - allows full game restart after Game Over or Quit
	while (1)
	{
		startMenu(); // display start screen and wait for player input
		eputs("Game started!!!\r\n");

		// GAME STATE VARIABLES (reset at start of each new game)
		int score = 0;		   // player's current score
		int lives = 5;		   // remaining lives
		int correctFlower = 0; // index of the "right" flower

		// bee positions
		uint16_t x = 55, y = 75;	 // bee's current position (x, y)
		uint16_t oldx = x, oldy = y; // previous position - used to erase old bee

		// fixed positions for the 8 flowers on screen
		int flowerX[8] = {10, 52, 95, 10, 52, 95, 0, 104};
		int flowerY[8] = {20, 20, 20, 126, 126, 126, 70, 70};

		// tracks whether each flower is still active (1 = visible)
		int flowerCheck[8] = {1, 1, 1, 1, 1, 1, 1, 1};

		// array of pointers of flower sprite images
		const uint16_t *flowers[8] = {
			flower1, flower2, flower3, flower4,
			flower5, flower6, flower7, flower8};

		// initialize game variables (score, lives, correct flower)
		initGame(&score, &lives, &correctFlower);

		// clear screen before starting game
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

		// Draw all flowers at random appearances
		for (int i = 0; i < 8; i++)
		{
			int type = rand() % 8; // pick random flower sprite
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
			flowerCheck[i] = 1;
		}

		// display initial score and lives
		drawGameInfo(pink, score, lives);

		// INNER LOOP -> main gameplay loop (runs until quit or game over)
		while (1)
		{
			// check for the quit request from the terminal (Q/q)
			if (quitRequested())
			{
				uint32_t pink = RGBToWord(0xff, 0xaf, 0xaf);
				fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
				printText("QUITTING...", 25, 75, pink, 0);
				delay(2000); // allow user to see message
				break;		 // exit game loop -> return to menu
			}

			// movement flags (used for updating and drawing)
			int hmoved = 0, vmoved = 0;
			int hinverted = 0, vinverted = 0;

			// read button inputs and update bee position
			handleInput(&x, &y, &hmoved, &vmoved, &hinverted, &vinverted);

			// handles pause button (freezes game until resumed)
			handlePause(pink, x, y, hmoved, hinverted, vmoved,
						score, lives, flowers, flowerX, flowerY, flowerCheck);

			// only redraw if the bee actually moved (reduces screen flicker)
			if (hmoved || vmoved)
			{
				// clear previous bee position
				fillRectangle(oldx, oldy, BEE_CLEAR_W, BEE_CLEAR_H, 0);

				// update stored previous position
				oldx = x;
				oldy = y;

				// draw bee at new position
				drawBee(x, y, hmoved, hinverted, vmoved);

				// check for collisions with flowers
				checkCollisions(&x, &y, &oldx, &oldy, &score, &lives, &correctFlower,
								flowerCheck, flowers, flowerX, flowerY);

				// update score and lives display
				drawGameInfo(pink, score, lives);
			}

			// check for game over condition
			if (lives <= 0)
			{
				gameOverScreen(pink, score);
				break; // exit inner loop -> will restart from startMenu
			}

			delay(40); // controls game speed (+- 25 FPS)
		}
	}

	return 0; // not expected to be reached due to infinite game loop
}

/* HARDWARE INITIALIZATION FUNCTIONS */
// initialize SysTick timer to generate an interrupt every 1ms
// used for timing operations such as delay() and game timing
void initSysTick(void)
{
	SysTick->LOAD = 48000;
	SysTick->CTRL = 7;
	SysTick->VAL = 10;
	__asm(" cpsie i "); // enable interrupts
}

// interrupt service routine called every 1ms
// increments global millisecond counter for timing control
void SysTick_Handler(void)
{
	milliseconds++;
}

// configure system PLL/clock to run the MCU at 48MHz
// WARNING: incorrect settings here may make the MCU unresponsive
// keep changes minimal and verify on hardware
void initClock(void)
{
	// First ensure PLL is disabled
	RCC->CR &= ~(1u << 24);
	while ((RCC->CR & (1 << 25)) != 0)
	{
		/* wait for PLL ready to be cleared */
	}

	// Warning here: if system clock is greater than 24MHz then wait-state(s) need to be
	// inserted into Flash memory interface

	FLASH->ACR |= (1 << 0);
	FLASH->ACR &= ~((1u << 2) | (1u << 1));
	// Turn on FLASH prefetch buffer
	FLASH->ACR |= (1 << 4);
	// set PLL multiplier to 12 (yielding 48MHz)
	RCC->CFGR &= ~((1u << 21) | (1u << 20) | (1u << 19) | (1u << 18));
	RCC->CFGR |= ((1 << 21) | (1 << 19));

	// Need to limit ADC clock to below 14MHz so will change ADC prescaler to 4
	RCC->CFGR |= (1 << 14);

	// and turn the PLL back on again
	RCC->CR |= (1 << 24);
	// set PLL as system clock source
	RCC->CFGR |= (1 << 1);
}

// blocking delay function using SysTick timing
// CPU enters low-power state (WFI) while waiting
void delay(volatile uint32_t dly)
{
	uint32_t end_time = dly + milliseconds;
	while (milliseconds != end_time)
	{
		__asm(" wfi "); // sleep until next interrupt
	}
}

// enable internal pull-up resistor on specified GPIO pin
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber)
{
	Port->PUPDR = Port->PUPDR & ~(3u << BitNumber * 2); // clear pull-up resistor bits
	Port->PUPDR = Port->PUPDR | (1u << BitNumber * 2);	// set pull-up bit
}

// set the mode of a GPIO pin. Mode follows the MODER register encoding:
// 0 -> input, 1 -> general purpose output, 2 -> alternate function, 3 -> analog
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode)
{
	uint32_t mode_value = Port->MODER;
	Mode = Mode << (2 * BitNumber);
	mode_value = mode_value & ~(3u << (BitNumber * 2));
	mode_value = mode_value | Mode;
	Port->MODER = mode_value;
}

// return 1 if point (px,py) lies inside rectangle (x1,y1,w,h), inclusive bounds
int isInside(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint16_t px, uint16_t py)
{
	uint16_t x2, y2;
	x2 = x1 + w;
	y2 = y1 + h;
	int rvalue = 0;
	if ((px >= x1) && (px <= x2))
	{
		if ((py >= y1) && (py <= y2))
			rvalue = 1;
	}

	return rvalue;
}

// initialize all GPIO: buttons (inputs with pull-ups), LEDs (outputs),
// and start the LCD display
void setupIO()
{
	RCC->AHBENR |= (1 << 18) | (1 << 17); // enable Ports A and B

	// Configure Pause button (PB0) and movement buttons
	pinMode(GPIOB, 0, 0);  // Pause button
	pinMode(GPIOB, 4, 0);  // Right
	pinMode(GPIOB, 5, 0);  // Left
	pinMode(GPIOA, 8, 0);  // Up
	pinMode(GPIOA, 11, 0); // Down

	// Enable internal pull-up resistors on all buttons
	enablePullUp(GPIOB, 0);
	enablePullUp(GPIOB, 4);
	enablePullUp(GPIOB, 5);
	enablePullUp(GPIOA, 8);
	enablePullUp(GPIOA, 11);

	// Configure LEDs as outputs
	pinMode(GPIOA, 9, 1);  // Green LED (PA9)
	pinMode(GPIOA, 10, 1); // Red LED (PA10)

	// configure speaker
	pinMode(GPIOB, 1, 1);

	// before display begin
	redOff();	// turn red LED off
	greenOff(); // turn green LED off

	display_begin(); // Initialize the LCD display

	initSound(); // initialize sound
}

/* SERIAL COMMUNICATION HELPERS */
// check if data is available to read from UART (non-blocking)
int serialAvailable(void)
{
	if (USART1->ISR & (1 << 5)) // RXNE flag = data is ready to read
		return 1;
	return 0;
}

// check if player entered 'Q' or 'q' via terminal to quit the game
int quitRequested(void)
{
	if (serialAvailable())
	{
		char c = egetchar();
		eputchar(c); // Echo character back to terminal

		if (c == 'Q' || c == 'q')
		{
			eputs("\tYou quit the game :( Hope you come back soon!\r\n\n");
			return 1;
		}
	}
	return 0;
}

/* LED CONTROL FUNCTIONS */
// turn green LED on
void greenOn(void)
{
	GPIOA->ODR |= (1 << 9);
}

// turn green LED off
void greenOff(void)
{
	GPIOA->ODR &= ~(1 << 9);
}

// turn red LED on
void redOn(void)
{
	GPIOA->ODR |= (1 << 10);
}

// turn red LED off
void redOff(void)
{
	GPIOA->ODR &= ~(1 << 10);
}

/* GAME LOGIC FUNCTIONS */
// display start screen and wait for player to press UP to begin the game
void startMenu(void)
{
	uint32_t pink = RGBToWord(0xff, 0xaf, 0xaf);

	// clear screen
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

	// render start menu instructions
	printTextX2("BUZZ RUSH", 12, 20, pink, 0);
	printTextX2("PRESS UP", 17, 55, pink, 0);
	printTextX2("TO START", 17, 75, pink, 0);
	printText("ENTER Q OR q", 22, 115, pink, 0);
	printText("TO QUIT", 40, 125, pink, 0);
	printText("AT ANYTIME", 30, 135, pink, 0);

	// wait until the player presses the UP button to start the game
	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // UP button pressed
		{
			delay(120); // debounce to avoid accidental triggers
			return;		// exit menu and start game
		}
		delay(50); // small delay to reduce CPU usage
	}
}

// reset game state -> score, lives, and randomly select "right" flower
void initGame(int *score, int *lives, int *correctFlower)
{
	*score = 0;
	*lives = 5;
	*correctFlower = rand() % 8;
}

// display current score and remaining lives on screen
void drawGameInfo(uint32_t pink, int score, int lives)
{
	char scoreText[16];
	char livesText[16];

	printText("SCORE:", 10, 2, pink, 0);
	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printText(scoreText, 55, 2, pink, 0);

	printText("LIVES:", 70, 2, pink, 0);
	snprintf(livesText, sizeof(livesText), "%d", lives);
	printText(livesText, 115, 2, pink, 0);
}

// pause game when button is pressed and display pause screen
// waits for player input to resume and restores game state
void handlePause(uint32_t pink, uint16_t x, uint16_t y,
				 int hmoved, int hinverted, int vmoved,
				 int score, int lives,
				 const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[])
{
	if ((GPIOB->IDR & (1 << 0)) == 0) // Pause button pressed
	{
		delay(200); // debounce

		eputs("Game paused...");
		eputs("\r\n\n");

		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
		printText("GAME PAUSED", 25, 25, pink, 0);
		printText("PRESS", 45, 75, pink, 0);
		printText("BUTTON AGAIN", 20, 95, pink, 0);
		printText("TO RESUME", 30, 115, pink, 0);

		// wait for button release to avoid immediate re-trigger
		while ((GPIOB->IDR & (1 << 0)) == 0);

		// Wait for new press to resume
		while (1)
		{
			if ((GPIOB->IDR & (1 << 0)) == 0)
			{
				delay(200);
				break;
			}
		}

		eputs("Game resumed");
		eputs("\r\n\n");

		// Restore the game screen
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

		// Redraw HUD
		drawGameInfo(pink, score, lives);

		// Redraw active flowers
		for (int i = 0; i < 8; i++)
		{
			if (flowerCheck[i] == 1)
			{
				int type = rand() % 8;
				putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
			}
		}

		// Redraw bee
		drawBee(x, y, hmoved, hinverted, vmoved);
	}
}

// read button inputs and update bee position
// also sets movement direction flags for rendering
void handleInput(uint16_t *x, uint16_t *y, int *hmoved, int *vmoved, int *hinverted, int *vinverted)
{
	*hmoved = *vmoved = 0;
	*hinverted = *vinverted = 0;

	// Right
	if ((GPIOB->IDR & (1 << 4)) == 0 && *x < 110)
	{
		(*x)++;
		*hmoved = 1;
		*hinverted = 0; // facing right
	}

	// Left
	if ((GPIOB->IDR & (1 << 5)) == 0 && *x > 10)
	{
		(*x)--;
		*hmoved = 1;
		*hinverted = 1; // facing left
	}

	// Down
	if ((GPIOA->IDR & (1 << 11)) == 0 && *y < 140)
	{
		(*y)++;
		*vmoved = 1;
		*vinverted = 0;
	}

	// Up
	if ((GPIOA->IDR & (1 << 8)) == 0 && *y > 16)
	{
		(*y)--;
		*vmoved = 1;
		*vinverted = 1;
	}
}

// select and render bee sprite based on movement direction
void drawBee(uint16_t x, uint16_t y, int hmoved, int hinverted, int vmoved)
{
	if (hmoved)
	{
		if (hinverted)
		{
			putImage(x, y, 16, 12, beeLeft, 0, 0);
		}
		else
		{
			putImage(x, y, 16, 12, beeRight, 0, 0);
		}
	}
	else if (vmoved)
	{
		putImage(x, y, 12, 16, beeUp, 0, 0);
	}
	else
	{
		// default to right-facing sprite when no movement is detected
		putImage(x, y, 16, 12, beeRight, 0, 0); 
	}
}

// redraw all active flowers with random appearances
void redrawFlowers(const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[])
{
	for (int i = 0; i < 8; i++)
	{
		if (flowerCheck[i] == 1) // only draw active flowers
		{
			// assign a random flower type each redraw (for visual variation)
			int type = rand() % 8;
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
		}
	}
}

// detect collisions between bee and flowers
// updates score for correct flower and reduces lives for wrong ones
// reset game state after correct flower is collected
void checkCollisions(uint16_t *x, uint16_t *y, uint16_t *oldx, uint16_t *oldy,
					 int *score, int *lives, int *correctFlower, int *flowerCheck,
					 const uint16_t *flowers[8], int flowerX[], int flowerY[])
{
	// check for collision by testing if any of the bee's corners touch a flower
	if (isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x, *y) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x + 12, *y) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x, *y + 16) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x + 12, *y + 16))
	{
		// handle correct flower collection
		greenOn(); 
		redOff();
		(*score)++;

		// Happy sound effect when collecting correct flower
        playNoteFor(C5, 80);
        delay(30);
        playNoteFor(E5, 80);
        delay(30);
        playNoteFor(G5, 150);

		eputs("Niice! You found the right flower\nScore: "); 
		printScore(*score); 
		eputs("\n");

		// clear bee's old position
		fillRectangle(*x, *y, BEE_CLEAR_W, BEE_CLEAR_H, 0);

		// Clear all flowers from the screen 
		for (int i = 0; i < 8; i++)
		{
			fillRectangle(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, 0);
		}

		// Pick new correct flower
		*correctFlower = rand() % 8;

		// reset flower state and redraw all flowers with new randomized types
		for (int i = 0; i < 8; i++)
		{
			flowerCheck[i] = 1;
			int type = rand() % 8;
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
		}

		// Reset bee to center position
		*x = 55;
		*y = 75;
		*oldx = *x;
		*oldy = *y;

		return; // exit so we don't check wrong flowers in same frame
	}

	for (int i = 0; i < 8; i++)
	{
		if (i == *correctFlower)
			continue; // skip correct flower

		// handle incorrect flower collision (lose life and remove flower)
		if (flowerCheck[i] &&
			(isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x, *y) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x + 12, *y) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x, *y + 16) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x + 12, *y + 16)))
		{
			redOn();
			greenOff();
			(*lives)--;

			// Sad / buzz sound for wrong flower
			playNoteFor(DS4_Eb4, 200);
			delay(50);
			playNoteFor(C4, 300);
		
			eputs("Oops! That's the wrong flower\nLives left: ");
			printScore(*lives); 
			eputs("\n");

			// deactivate the wrong flower
			flowerCheck[i] = 0;
			fillRectangle(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, 0);
			break; 
		}
	}
}

// display game over screen and wait for player to restart
void gameOverScreen(uint32_t pink, int score)
{
	eputs("Game over :(\nFinal Score: ");
	printScore(score);
	eputs("\n");

    // Sad descending sound
    playNoteFor(G4, 150);
    delay(100);
    playNoteFor(E4, 150);
    delay(100);
    playNoteFor(C4, 400);
    
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

	char scoreText[16];

	printTextX2("GAME OVER", 12, 15, pink, 0);
	printTextX2("SCORE:", 18, 45, pink, 0);

	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printTextX2(scoreText, 92, 45, pink, 0);

	printText("PRESS UP", 37, 100, pink, 0);
	printText("TO RESTART", 30, 115, pink, 0);

	// wait until the player presses UP to restart
	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // UP button pressed
		{
			// debounce button press
			delay(800);

			return; // go back to startMenu()
		}
	}
}

// Convert integer to characters and print via UART (no leading zeros)
void printScore(int value)
{
	if (value == 0)
	{
		eputchar('0');
		eputs("\r\n");
		return;
	}

	char buffer[12]; // enough for 32-bit int
	int i = 0;

	// Handle negative numbers (if needed)
	if (value < 0)
	{
		eputchar('-');
		value = -value;
	}

	// Convert to string (digits in reverse)
	while (value > 0)
	{
		buffer[i++] = (value % 10) + '0';
		value = value / 10;
	}

	// Print in correct order
	while (i > 0)
	{
		eputchar(buffer[--i]);
	}
	eputs("\r\n");
}

// Play a note for a specific duration (in milliseconds)
void playNoteFor(uint32_t freq, uint32_t duration_ms)
{
    playNote(freq);
    delay(duration_ms);
    stopSound();
}

// Stop any currently playing sound
void stopSound(void)
{
    TIM14->CR1 &= ~(1 << 0);   // Disable Timer14
}