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

// int quit_requested(int);


volatile uint32_t milliseconds;

/* Layout constants - adjust if sprite sizes change */
#define SCREEN_W 128
#define SCREEN_H 160
#define FLOWER_W 24
#define FLOWER_H 24
#define BEE_CLEAR_W 16 /* rectangle used to erase bee (covers both orientations) */
#define BEE_CLEAR_H 16

int main()
{
	int hinverted = 0;
	int vinverted = 0;
	int hmoved = 0;
	int vmoved = 0;

	uint16_t x = 50;
	uint16_t y = 50;
	uint16_t oldx = x;
	uint16_t oldy = y;
	char scoreText[16]; // Declare scoreText at the top of main
	char livesText[20]; // Declare livesText at the top of main

	// int quit = 0;

	initClock();
	initSysTick();
	initSerial();
	setupIO();

	// START MENU
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0); // clear screen

	// print centered menu text
	uint32_t yellow = RGBToWord(0xff, 0xff, 0);
	printTextX2("PRESS UP", 15, 55, yellow, 0);
	printTextX2("TO START", 15, 75, yellow, 0);
	printText("press Q or q", 22, 95, yellow, 0);
	printText("to quit", 40, 105, yellow, 0);
	printText("at anytime", 30, 115, yellow, 0);
	//uint32_t red = RGBToWord(255,60,60);
	int started = 0;
	while (started == 0)
	{
		if((GPIOA->IDR & (1 << 8)) == 0) // up pressed
		{
			delay(120); // simple debounce
			started = 1;
		}
		delay(50);
	}

	// game starts
	fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);
	
	// // send signal to terminal (show that the board and terminal is connected)
	// printf("Bee game started - UART OK\r\n");

	int score = 0;
	int lives = 5;
	int correctFlower = rand() % 8;

	printText("SCORE:", 10, 2, yellow, 0);
	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printText(scoreText, 55, 2, yellow, 0);

	printText("LIVES:", 70, 2, yellow, 0);
	snprintf(livesText, sizeof(livesText), "%d", lives);
	printText(livesText, 115, 2, yellow, 0);

	// Store all flower sprites
	const uint16_t *flowers[8] = {
		flower1,
		flower2,
		flower3,
		flower4,
		flower5,
		flower6,
		flower7,
		flower8
	};

	int flowerX[8] = {10, 52, 95, 10, 52, 95, 0, 104};
	int flowerY[8] = {20, 20, 20, 126, 126, 126, 70, 70};
	int flowerCheck[8] = {1,1,1,1,1,1,1,1};

	// draw initial flowers
	for (int i = 0; i < 8; i++)
	{
		int flowerType = rand() % 8;
		putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[flowerType], 0, 0);
	}

	// main game loop
	int last_score = -1;

	while (/*!quit*/ 1)
	{

		//quit = quit_requested(score);

		/* only print score when it changes to avoid flooding UART */
		if (score != last_score)
		{
			printDecimal(score);
			eputs("\r\n");
			last_score = score;
		}
		
		hmoved = vmoved = 0;
		hinverted = vinverted = 0;

		if ((GPIOB->IDR & (1 << 4)) == 0) // right pressed
		{
			if (x < 110)
			{
				x = x + 1;
				hmoved = 1;
				hinverted = 0;
			}
		}
		if ((GPIOB->IDR & (1 << 5)) == 0) // left pressed
		{

			if (x > 10)
			{
				x = x - 1;
				hmoved = 1;
				hinverted = 1;
			}
		}
		if ((GPIOA->IDR & (1 << 11)) == 0) // down pressed
		{
			if (y < 140)
			{
				y = y + 1;
				vmoved = 1;
				vinverted = 0;
			}
		}
		if ((GPIOA->IDR & (1 << 8)) == 0) // up pressed
		{
			// set the limitation of where the sprite can go to in the screen
			if (y > 16)
			{
				y = y - 1;
				vmoved = 1;
				vinverted = 1;
			}
		}
		if ((vmoved) || (hmoved))
		{
			// only redraw if there has been some movement (reduces flicker)
			fillRectangle(oldx, oldy, BEE_CLEAR_W, BEE_CLEAR_H, 0);
			oldx = x;
			oldy = y;

			// draw new bee
			if (hmoved)
			{
				if (hinverted)
				{
					// putImage(x, y, width, height, image, flipX, flipY);
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
			// Now check for an overlap by checking to see if ANY of the 4 corners of bee are within the flower area
			if (isInside(flowerX[correctFlower], flowerY[correctFlower], FLOWER_W, FLOWER_H, x, y) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], FLOWER_W, FLOWER_H, x + 12, y) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], FLOWER_W, FLOWER_H, x, y + 16) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], FLOWER_W, FLOWER_H, x + 12, y + 16))
			{
				greenOn();
				delay(1000);
				greenOff();

				score++;
				snprintf(scoreText, sizeof(scoreText), "%d", score);
                printText(scoreText, 55, 2, yellow, 0);

				// printf("Collected! Score now: %d\r\n", score);

				// Erase bee at collection position (clear rectangle that covers bee in any orientation)
				fillRectangle(x, y, BEE_CLEAR_W, BEE_CLEAR_H, 0);

				// Clear old flowers
				for (int i = 0; i < 8; i++) {
					fillRectangle(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, 0);
				}

				// Pick new correct flower & redraw all (reset flowerCheck)
				correctFlower = rand() % 8;
				for (int i = 0; i < 8; i++) {
					int flowerType = rand() % 8;
					putImage(flowerX[i], flowerY[i], FLOWER_W, FLOWER_H, flowers[flowerType], 0, 0);
					flowerCheck[i] = 1;
				}

                // Reset & draw bee at center
                x = 55;
                y = 75;
                oldx = x;
                oldy = y;
                //drawBee(x, y, 1);  // facing right
            }
			else // Check if I am hitting one of the incorrect flowers
			{
				for(int incorrect_flower = 0; incorrect_flower <= 7; incorrect_flower++)
				{

					if(incorrect_flower == correctFlower)
					{
						// Do nothing
					}
					else
					{
						// Do your collsion check as above for the correct flower
						if ((isInside(flowerX[incorrect_flower], flowerY[incorrect_flower], FLOWER_W, FLOWER_H, x, y) ||
							isInside(flowerX[incorrect_flower], flowerY[incorrect_flower], FLOWER_W, FLOWER_H, x + 12, y) ||
							isInside(flowerX[incorrect_flower], flowerY[incorrect_flower], FLOWER_W, FLOWER_H, x, y + 16) ||
							isInside(flowerX[incorrect_flower], flowerY[incorrect_flower], FLOWER_W, FLOWER_H, x + 12, y + 16)) &&
										flowerCheck[incorrect_flower] == 1)
						{
							redOn();

							lives--;

							snprintf(livesText, sizeof(livesText), "%d", lives);
							printText(livesText, 115, 2, yellow, 0);

							flowerCheck[incorrect_flower] = 0;
							// Erase flower at collection position
							fillRectangle(flowerX[incorrect_flower], flowerY[incorrect_flower], FLOWER_W, FLOWER_H, 0);

							redOff();
						}		
					}

				}

			}

			if(lives == 0)
			{
				fillRectangle(x, y, BEE_CLEAR_W, BEE_CLEAR_H, 0);
				fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

				// print centered menu text
				uint32_t yellow = RGBToWord(0xff, 0xff, 0);
				printTextX2("GAME OVER", 10, 35, yellow, 0);
				printTextX2("SCORE: ", 15, 65, yellow, 0);
				snprintf(scoreText, sizeof(scoreText), "%d", score);
				printTextX2(scoreText, 90, 65, yellow, 0);
				printText("press the button", 10, 95, yellow, 0);
				printText("to play again!!", 15, 110, yellow, 0);
			}
        }
        delay(40);
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
	display_begin();
	pinMode(GPIOB, 4, 0);
	pinMode(GPIOB, 5, 0);
	pinMode(GPIOA, 8, 0);
	pinMode(GPIOA, 11, 0);
	enablePullUp(GPIOB, 4);
	enablePullUp(GPIOB, 5);
	enablePullUp(GPIOA, 11);
	enablePullUp(GPIOA, 8);
}

// function that turns the red LED on
void redOn(void)
{
	GPIOA->ODR |= (1 << 10);
}
void redOff(void)
{
	GPIOA->ODR &= ~(1 << 10);
}

void greenOn(void)
{
	GPIOA->ODR |= (1 << 9);
}
void greenOff(void)
{
	GPIOA->ODR &= ~(1 << 9);
}

/* int quit_requested(int score)
{
	char ch;

	ch = egetchar();
	eputchar(ch);

	ch = ch | 32;

	if (ch == 'q')
	{
		fillRectangle(0, 0, SCREEN_W, SCREEN_H, 0);

		char scoreText[20];

		// print centered menu text
		uint32_t yellow = RGBToWord(0xff, 0xff, 0);
		printTextX2("GAME QUIT", 10, 35, yellow, 0);
		printTextX2("SCORE: ", 15, 65, yellow, 0);
		snprintf(scoreText, sizeof(scoreText), "%d", score);
		printTextX2(scoreText, 90, 65, yellow, 0);
		printText("press the button", 10, 95, yellow, 0);
		printText("to play again!!", 15, 110, yellow, 0);

		return 1;
	}

	return 0;
} */