/*
	BEE GAME - Microprocessors Assignment

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

// game logic functions
void startMenu(void);
void initGame(int *score, int *lives, int *correctFlower);
void drawGameInfo(uint32_t yellow, int score, int lives);
void handlePause(uint32_t yellow, uint16_t x, uint16_t y,
				 int hmoved, int hinverted, int vmoved,
				 int score, int lives,
				 const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[]);
void handleInput(uint16_t *x, uint16_t *y, int *hmoved, int *vmoved, int *hinverted, int *vinverted);
void drawBee(uint16_t x, uint16_t y, int hmoved, int hinverted, int vmoved);
void redrawFlowers(const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[]);
void checkCollisions(uint16_t *x, uint16_t *y, uint16_t *oldx, uint16_t *oldy,
					 int *score, int *lives, int *correctFlower, int *flowerCheck,
					 const uint16_t *flowers[8], int flowerX[], int flowerY[]);
void gameOverScreen(uint32_t yellow, int score);

// serial communication helpers
int serialAvailable(void); // check if a character is waiting UART	buffer
int quitRequested(void);   // check if player typed Q or q to quit
void printScore(int);

// GLOBAL VARIABLES
volatile uint32_t milliseconds = 0; // incremented every 1ms by SysTick interrupt

// SCREEN AND SPRITE CONSTANTS
#define SCREEN_W 128 // LCD width in pixels
#define SCREEN_H 160 // LCD height in pixels
#define FLOWER_W 24
#define FLOWER_H 24

// Size of rectangle used to erase the bee
#define BEE_CLEAR_W 16 // size of the width
#define BEE_CLEAR_H 16 // size of the height

// MAIN FUNCTION
int main(void)
{
	uint32_t yellow = RGBToWord(0xff, 0xff, 0); // color for the text

	initClock();
	initSysTick();
	initSerial();
	setupIO(); // configure buttons, LEDs, and start display

	while (1) // Outer loop - allows full game restart after Game Over or Quit
	{
		startMenu();								  // display start screen and wait for player to press UP
		eputs("The game... HAS STARTEEED!!!\r\n"); // display text in the terminal

		// GAME STATER VARIABLES
		int score = 0;		   // player's current score
		int lives = 5;		   // remaining lives
		int correctFlower = 0; // which flower is the "correct" one this round

		uint16_t x = 55, y = 75;	 // bee's current position (x, y)
		uint16_t oldx = x, oldy = y; // previous position - used to erase old bee

		// fixed positions for the 8 flowers
		int flowerX[8] = {10, 52, 95, 10, 52, 95, 0, 104};
		int flowerY[8] = {20, 20, 20, 126, 126, 126, 70, 70};

		int flowerCheck[8] = {1, 1, 1, 1, 1, 1, 1, 1}; // 1 means the flower is still "active"

		// pointers to all flower sprite images
		const uint16_t *flowers[8] = {
			flower1, flower2, flower3, flower4,
			flower5, flower6, flower7, flower8};

		initGame(&score, &lives, &correctFlower);	// reset games variables
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0); // clear the sceen to black

		// Draw 8 flowers with random appearances at the start
		for (int i = 0; i < 8; i++)
		{
			int type = rand() % 8; // pick random flower image
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
			flowerCheck[i] = 1;
		}

		drawGameInfo(yellow, score, lives); // draw SCORE and LIVES at the top

		// MAIN GAME LOOP
		while (1)
		{
			// check for the quit resquest from the terminal
			if (quitRequested())
			{
				uint32_t yellow = RGBToWord(0xff, 0xff, 0);
				fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
				printText("QUITTING...", 25, 75, yellow, 0);
				delay(2000); // give time to see the message
				break;		 // exit game loop -> return to startMenu
			}

			// read button inputs and update bee position
			int hmoved = 0, vmoved = 0;
			int hinverted = 0, vinverted = 0;

			handleInput(&x, &y, &hmoved, &vmoved, &hinverted, &vinverted);

			// check if player pressed the pause button
			handlePause(yellow, x, y, hmoved, hinverted, vmoved,
						score, lives, flowers, flowerX, flowerY, flowerCheck);

			// only redraw if the bee actually moved (reduces screen flicker)
			if (hmoved || vmoved)
			{
				// Clear old bee position
				fillRectangle(oldx, oldy, BEE_CLEAR_W, BEE_CLEAR_H, 0);
				oldx = x;
				oldy = y;

				// draw bee with correct direction
				drawBee(x, y, hmoved, hinverted, vmoved);

				// check if bee touched any flowers (correct or wrong)
				checkCollisions(&x, &y, &oldx, &oldy, &score, &lives, &correctFlower,
								flowerCheck, flowers, flowerX, flowerY);

				// refresh score and lives display on LCD
				drawGameInfo(yellow, score, lives);
			}

			// game over condition
			if (lives <= 0)
			{
				gameOverScreen(yellow, score);
				break; // Exit inner loop -> will restart from startMenu
			}

			delay(40); // controls game speed (+- 25 FPS)
		}
	}

	return 0;
}

// HARDWARE INITIALIZATION FUNCTIONS

// Configure SysTick to generate an interrupt every 1ms.
// Assumes system clock will be configured to 48MHz so LOAD=48000.
void initSysTick(void)
{
	SysTick->LOAD = 48000;
	SysTick->CTRL = 7;
	SysTick->VAL = 10;
	__asm(" cpsie i "); // enable interrupts
}
// SysTick interrupt handler increments a millisecond counter used by delay().
void SysTick_Handler(void)
{
	milliseconds++;
}
// Configure system PLL/clock to run the MCU at 48MHz.
// WARNING: incorrect settings here may make the MCU unresponsive.
// Keep changes minimal and verify on hardware.
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
// Blocking delay measured in milliseconds. Uses WFI to reduce power while waiting.
void delay(volatile uint32_t dly)
{
	uint32_t end_time = dly + milliseconds;
	while (milliseconds != end_time)
	{
		__asm(" wfi "); // sleep until next interrupt
	}
}
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber)
{
	Port->PUPDR = Port->PUPDR & ~(3u << BitNumber * 2); // clear pull-up resistor bits
	Port->PUPDR = Port->PUPDR | (1u << BitNumber * 2);	// set pull-up bit
}
// Set the mode of a GPIO pin. Mode follows the MODER register encoding:
// 0 -> input, 1 -> general purpose output, 2 -> alternate function, 3 -> analog
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode)
{
	uint32_t mode_value = Port->MODER;
	Mode = Mode << (2 * BitNumber);
	mode_value = mode_value & ~(3u << (BitNumber * 2));
	mode_value = mode_value | Mode;
	Port->MODER = mode_value;
}
// Return 1 if point (px,py) lies inside rectangle (x1,y1,w,h), inclusive bounds.
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
// configure all GPIO pins: buttons, LEDs, and enable display
void setupIO()
{
	RCC->AHBENR |= (1 << 18) | (1 << 17); // enable Ports A and B

	// Configure Pause button (PB0) and movement buttons
    pinMode(GPIOB, 0, 0);   // Pause button
    pinMode(GPIOB, 4, 0);   // Right
    pinMode(GPIOB, 5, 0);   // Left
    pinMode(GPIOA, 8, 0);   // Up
    pinMode(GPIOA, 11, 0);  // Down

    // Enable internal pull-up resistors on all buttons
    enablePullUp(GPIOB, 0);
    enablePullUp(GPIOB, 4);
    enablePullUp(GPIOB, 5);
    enablePullUp(GPIOA, 8);
    enablePullUp(GPIOA, 11);

    // Configure LEDs as outputs
    pinMode(GPIOA, 9, 1);   // Green LED (PA9)
    pinMode(GPIOA, 10, 1);  // Red LED (PA10)

    redOff();
    greenOff();

    display_begin();        // Initialize the LCD display
}

// SERIAL COMMUNICATION HELPERS

// check if a character has been received on USART1 (non-blocking)
int serialAvailable(void)
{
    if (USART1->ISR & (1 << 5))     // RXNE flag = data is ready to read
        return 1;
    return 0;
}
// Check if player wants to quit by typing Q or q
int quitRequested(void)
{
    if (serialAvailable())
    {
        char c = egetchar();
        eputchar(c); // Echo character back to terminal

        if (c == 'Q' || c == 'q')
        {
            eputs("\r\nPlayer quit the game.\r\n");
            return 1;
        }
    }
    return 0;
}

// LED CONTROL FUNCTIONS
void greenOn(void)
{
	GPIOA->ODR |= (1 << 9);
}
void greenOff(void)
{
	GPIOA->ODR &= ~(1 << 9);
}
void redOn(void)
{
	GPIOA->ODR |= (1 << 10);
}
void redOff(void)
{
	GPIOA->ODR &= ~(1 << 10);
}

// GAME LOGIC FUNCTIONS
/*	displays the start menu and waits for the player to 
	begin && continues only when the UP button is pressed */
void startMenu(void)
{
	uint32_t yellow = RGBToWord(0xff, 0xff, 0);

	// clear the screen to prepare for menu display
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

	// display start instructions to the player
	printTextX2("WELCOME", 23, 20, yellow, 0);
	// printText("PRESS UP", 35, 55, yellow, 0);
	// printText("TO START", 35, 65, yellow, 0);
	printTextX2("PRESS UP", 17, 55, yellow, 0);
	printTextX2("TO START", 17, 75, yellow, 0);
	printText("ENTER Q OR q", 22, 115, yellow, 0);
	printText("TO QUIT", 40, 125, yellow, 0);
	printText("AT ANYTIME", 30, 135, yellow, 0);

	// wait until the player presses the UP button to start the game
	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // UP button pressed
		{
			delay(120); // debounce to avoid accidental triggers
			return;     // exit menu and start game
		}
		delay(50); // small delay to reduce CPU usage
	}
}

/*	initialises game state variables at the start of a new game && 
	resets score and lives, and selects a random correct flower */
void initGame(int *score, int *lives, int *correctFlower)
{
	*score = 0;
	*lives = 5;
	*correctFlower = rand() % 8;
}

void drawGameInfo(uint32_t yellow, int score, int lives)
{
	char scoreText[16];
	char livesText[16];

	printText("SCORE:", 10, 2, yellow, 0);
	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printText(scoreText, 55, 2, yellow, 0);

	printText("LIVES:", 70, 2, yellow, 0);
	snprintf(livesText, sizeof(livesText), "%d", lives);
	printText(livesText, 115, 2, yellow, 0);
}

/* 	pauses the game when the pause button is pressed && 
	displays pause screen and waits for player input to resume */
void handlePause(uint32_t yellow, uint16_t x, uint16_t y,
				 int hmoved, int hinverted, int vmoved,
				 int score, int lives,
				 const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[])
{
	if ((GPIOB->IDR & (1 << 0)) == 0) // Pause button pressed
	{
		delay(200); // debounce

		eputs("Player has paused the game...");
		eputs("\r\n");

		// Show pause screen
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
		printText("GAME PAUSED", 25, 25, yellow, 0);
		printText("PRESS", 45, 75, yellow, 0);
		printText("BUTTON AGAIN", 20, 95, yellow, 0);
		printText("TO RESUME", 30, 115, yellow, 0);

		// Wait for release
		while ((GPIOB->IDR & (1 << 0)) == 0)
			;

		// Wait for new press to resume
		while (1)
		{
			if ((GPIOB->IDR & (1 << 0)) == 0)
			{
				delay(200);
				break;
			}
		}

		eputs("Player has continued the game... yayyy!!!!");
		eputs("\r\n");

		// Restore the game screen
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

		// Redraw HUD
		drawGameInfo(yellow, score, lives);

		// Redraw flowers
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

/* 	reads button inputs and updates bee position
	also sets flags to indicate movement direction 
	(used for drawing the correct sprite) */
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
		// Entry point: initialize peripherals then run the top-level game loop.
		// The outer while loop allows restarting the game after Game Over.
	if ((GPIOA->IDR & (1 << 8)) == 0 && *y > 16)
	{
		(*y)--;
		*vmoved = 1;
		*vinverted = 1;
	}
}

void drawBee(uint16_t x, uint16_t y, int hmoved, int hinverted, int vmoved)
{
	if (hmoved)
	{
		if (hinverted)
			putImage(x, y, 16, 12, beeLeft, 0, 0);
		else
			putImage(x, y, 16, 12, beeRight, 0, 0);
	}
	else if (vmoved)
	{
		putImage(x, y, 12, 16, beeUp, 0, 0);
	}
	else
	{
		putImage(x, y, 16, 12, beeRight, 0, 0); // default
	}
}

void redrawFlowers(const uint16_t *flowers[8], int flowerX[], int flowerY[], int flowerCheck[])
{
	for (int i = 0; i < 8; i++)
	{
		if (flowerCheck[i] == 1)
		{
			int type = rand() % 8;
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
		}
	}
}

/*	checks for collisions between the bee and flowers &&
	updates score/lives and resets game state when needed */
void checkCollisions(uint16_t *x, uint16_t *y, uint16_t *oldx, uint16_t *oldy,
					 int *score, int *lives, int *correctFlower, int *flowerCheck,
					 const uint16_t *flowers[8], int flowerX[], int flowerY[])
{
	// Check all 4 corners of the bee's hitbox for collision accuracy
	if (isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x, *y) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x + 12, *y) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x, *y + 16) ||
		isInside(flowerX[*correctFlower], flowerY[*correctFlower], FLOWER_W, FLOWER_H, *x + 12, *y + 16))
	{
		greenOn();
		redOff();
		(*score)++;

		// After collecting correct flower
		eputs("Correct flower! Score: ");
		printScore(*score);

		fillRectangle(*x, *y, BEE_CLEAR_W, BEE_CLEAR_H, 0);

		// Clear all flowers
		for (int i = 0; i < 8; i++)
		{
			fillRectangle(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, 0);
		}

		// Pick new correct flower
		*correctFlower = rand() % 8;

		// Reset flowerCheck and redraw ALL flowers with new random types
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

		return; // important: exit so we don't check wrong flowers in same frame
	}

	for (int i = 0; i < 8; i++)
	{
		if (i == *correctFlower)
			continue; // skip the correct one

		if (flowerCheck[i] &&
			(isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x, *y) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x + 12, *y) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x, *y + 16) ||
			 isInside(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, *x + 12, *y + 16)))
		{
			redOn();
			greenOff();
			(*lives)--;

			// After hitting wrong flower
			eputs("Wrong! Lives: ");
			printScore(*lives);

			flowerCheck[i] = 0;
			fillRectangle(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, 0);
			break; // only punish once per frame
		}
	}
}

void gameOverScreen(uint32_t yellow, int score)
{
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

	char scoreText[16];

	printTextX2("GAME OVER", 12, 15, yellow, 0);
	printTextX2("SCORE:", 18, 45, yellow, 0);

	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printTextX2(scoreText, 92, 45, yellow, 0);

	printText("PRESS UP", 37, 100, yellow, 0);
	printText("TO RESTART", 30, 115, yellow, 0);

	// wait for player to restart
	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // UP button
		{
			delay(800);
			return; // go back to gameLoop
		}
	}
} 

// Clean version - prints number without leading zeros + newline
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
		value /= 10;
	}

	// Print in correct order
	while (i > 0)
	{
		eputchar(buffer[--i]);
	}
	eputs("\r\n");
}