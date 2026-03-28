#include <stm32f031x6.h>
#include "display.h"

// file with all the sprites
#include "sprites.h"

#include <stdlib.h>
#include <stdio.h>
#include "serial.h"

void initClock(void);
void initSysTick(void);
void SysTick_Handler(void);
void delay(volatile uint32_t dly);
void setupIO();
int isInside(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint16_t px, uint16_t py);
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber);
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode);

// functions that controls the LEDs
void redOn(void);
void redOff(void);
void greenOn(void);
void greenOff(void);

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

int serialAvailable(void);

int quitRequested(void);

void printScore(int);

volatile uint32_t milliseconds;

/* Layout constants - adjust if sprite sizes change */
#define SCREEN_W 128
#define SCREEN_H 160
#define FLOWER_W 24
#define FLOWER_H 24
#define BEE_CLEAR_W 16 /* rectangle used to erase bee (covers both orientations) */
#define BEE_CLEAR_H 16

int main(void)
{
	uint32_t yellow = RGBToWord(0xff, 0xff, 0);

	initClock();
	initSysTick();
	initSerial();
	setupIO();

	while (1) // Outer loop allows restarting the game
	{
		startMenu();

		eputs("The game... HAS STARTEEED!!!!!!\r\n");

		// Game variables
		int score = 0;
		int lives = 5;
		int correctFlower = 0;

		uint16_t x = 55, y = 75;
		uint16_t oldx = x, oldy = y;

		int flowerX[8] = {10, 52, 95, 10, 52, 95, 0, 104};
		int flowerY[8] = {20, 20, 20, 126, 126, 126, 70, 70};
		int flowerCheck[8] = {1, 1, 1, 1, 1, 1, 1, 1};

		const uint16_t *flowers[8] = {
			flower1, flower2, flower3, flower4,
			flower5, flower6, flower7, flower8};

		initGame(&score, &lives, &correctFlower);
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

		// Draw initial flowers with random types
		for (int i = 0; i < 8; i++)
		{
			int type = rand() % 8;
			putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[type], 0, 0);
			flowerCheck[i] = 1;
		}

		drawGameInfo(yellow, score, lives);

		// Main game loop
		while (1)
		{
			// if quit is requested
			if (quitRequested())
			{
				uint32_t yellow = RGBToWord(0xff, 0xff, 0);
				fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
				printTextX2("QUITTING...", 10, 60, yellow, 0);
				delay(2000); // give time to see the message
				break;		 // exit game loop → return to startMenu
			}

			int hmoved = 0, vmoved = 0;
			int hinverted = 0, vinverted = 0;

			handleInput(&x, &y, &hmoved, &vmoved, &hinverted, &vinverted);

			handlePause(yellow, x, y, hmoved, hinverted, vmoved,
						score, lives, flowers, flowerX, flowerY, flowerCheck);

			if (hmoved || vmoved)
			{
				// Clear old bee position
				fillRectangle(oldx, oldy, BEE_CLEAR_W, BEE_CLEAR_H, 0);
				oldx = x;
				oldy = y;

				drawBee(x, y, hmoved, hinverted, vmoved);

				// Check collisions
				checkCollisions(&x, &y, &oldx, &oldy, &score, &lives, &correctFlower,
								flowerCheck, flowers, flowerX, flowerY);

				drawGameInfo(yellow, score, lives); // Update HUD if changed
			}

			if (lives <= 0)
			{
				gameOverScreen(yellow, score);
				break; // Exit inner loop → will restart from startMenu
			}

			delay(40);
		}
	}

	return 0;
}

void initSysTick(void)
{
	SysTick->LOAD = 48000;
	SysTick->CTRL = 7;
	SysTick->VAL = 10;
	__asm(" cpsie i "); // enable interrupts
}
void SysTick_Handler(void)
{
	milliseconds++;
}
void initClock(void)
{
	// This is potentially a dangerous function as it could
	// result in a system with an invalid clock signal - result: a stuck system
	// Set the PLL up
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
void delay(volatile uint32_t dly)
{
	uint32_t end_time = dly + milliseconds;
	while (milliseconds != end_time)
	{
		__asm(" wfi "); // sleep
	}
}

void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber)
{
	Port->PUPDR = Port->PUPDR & ~(3u << BitNumber * 2); // clear pull-up resistor bits
	Port->PUPDR = Port->PUPDR | (1u << BitNumber * 2);	// set pull-up bit
}
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode)
{
	/*
	 */
	uint32_t mode_value = Port->MODER;
	Mode = Mode << (2 * BitNumber);
	mode_value = mode_value & ~(3u << (BitNumber * 2));
	mode_value = mode_value | Mode;
	Port->MODER = mode_value;
}
int isInside(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint16_t px, uint16_t py)
{
	// checks to seet px,py is within the rectange defined by x,y,w,h
	uint16_t x2, y2;
	x2 = x1 + w;
	y2 = y1 + h;
	int rvalue = 0;
	if ((px >= x1) && (px <= x2))
	{
		// ok, x constraint met
		if ((py >= y1) && (py <= y2))
			rvalue = 1;
	}
	return rvalue;
}

void setupIO()
{
	RCC->AHBENR |= (1 << 18) | (1 << 17); // enable Ports A and B

	pinMode(GPIOB, 0, 0);
	enablePullUp(GPIOB, 0);

	// configure led pins as outputs before display_begin()
	pinMode(GPIOA, 9, 1);  // green LED -> PA9
	pinMode(GPIOA, 10, 1); // red LED -> PA10

	// ensure that all LEDs start off
	GPIOA->ODR &= ~(1 << 9);
	GPIOA->ODR &= ~(1 << 10);

	display_begin();

	// configure the 4 buttons for movement

	// configure the extra button for the pause
	pinMode(GPIOB, 0, 0); // pause button -> PB0

	pinMode(GPIOB, 4, 0);
	pinMode(GPIOB, 5, 0);
	pinMode(GPIOA, 8, 0);
	pinMode(GPIOA, 11, 0);

	enablePullUp(GPIOB, 0);

	enablePullUp(GPIOB, 4);
	enablePullUp(GPIOB, 5);
	enablePullUp(GPIOA, 11);
	enablePullUp(GPIOA, 8);
}

// function that turns the green LED on
void greenOn(void)
{
	GPIOA->ODR |= (1 << 9);
}
// function that turns the green LED off
void greenOff(void)
{
	GPIOA->ODR &= ~(1 << 9);
}

// function that turns the red LED on
void redOn(void)
{
	GPIOA->ODR |= (1 << 10);
}
// function that turns the red LED off
void redOff(void)
{
	GPIOA->ODR &= ~(1 << 10);
}

// function for the start menu
void startMenu(void)
{
	uint32_t yellow = RGBToWord(0xff, 0xff, 0);

	// clear screen (fill screen with black pixels)
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

	// display message to the user
	printTextX2("PRESS UP", 15, 55, yellow, 0);
	printTextX2("TO START", 15, 75, yellow, 0);
	printText("Enter Q or q", 22, 95, yellow, 0);
	printText("to quit", 40, 105, yellow, 0);
	printText("at anytime", 30, 115, yellow, 0);

	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // if UP button is pressed
		{
			delay(120);
			return; // exit function
		}
		delay(50);
	}
} // end startMenu()

// function to intitiate game
void initGame(int *score, int *lives, int *correctFlower)
{
	*score = 0;
	*lives = 5;
	*correctFlower = rand() % 8;
} // end initGame()

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
} // end drawGameInfo()

// function for pause action and menu
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
		printTextX2("GAME PAUSED", 15, 55, yellow, 0);
		printTextX2("PRESS BUTTON", 10, 75, yellow, 0);
		printText("TO RESUME", 25, 95, yellow, 0);

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
} // end handlePause()

// function that manages input from user (buttons/movement)
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

void checkCollisions(uint16_t *x, uint16_t *y, uint16_t *oldx, uint16_t *oldy,
					 int *score, int *lives, int *correctFlower, int *flowerCheck,
					 const uint16_t *flowers[8], int flowerX[], int flowerY[])
{
	// Check correct flower
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

	printTextX2("GAME OVER", 10, 35, yellow, 0);
	printTextX2("SCORE:", 15, 65, yellow, 0);

	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printTextX2(scoreText, 90, 65, yellow, 0);

	printText("PRESS UP", 20, 100, yellow, 0);
	printText("TO RESTART", 15, 115, yellow, 0);

	// wait for player to restart
	while (1)
	{
		if ((GPIOA->IDR & (1 << 8)) == 0) // UP button
		{
			delay(200);
			return; // go back to gameLoop
		}
	}
} // end gameOver()

int serialAvailable(void)
{
	if (USART1->ISR & (1 << 5))
	{
		return 1;
	}

	return 0;
}

int quitRequested(void)
{
	if (serialAvailable())
	{
		char c = egetchar();

		eputchar(c);

		if (c == 'Q' || c == 'q')
		{
			eputs("\tPlayer quit the game. Hope you come soon...QQ\r\n");
			return 1;
		}
	}
	return 0;
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