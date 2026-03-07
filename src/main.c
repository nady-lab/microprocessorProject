#include <stm32f031x6.h>
#include "display.h"
#include "sprites.h"
#include <stdlib.h>
#include <stdio.h>

void initClock(void);
void initSysTick(void);
void SysTick_Handler(void);
void delay(volatile uint32_t dly);
void setupIO();
int isInside(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint16_t px, uint16_t py);
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber);
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode);
//void drawBee(uint16_t bx, uint16_t by, int facing_right);
//void uart2_init(void);
//int __io_putchar(int ch);
//int quit_requested(void);

volatile uint32_t milliseconds;

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
	
	initClock();
	initSysTick();
	setupIO();
	//uart2_init();

	/*
	// Simple entropy for srand() — time since boot + some GPIO noise
    uint32_t seed = milliseconds ^ (GPIOB->IDR << 16) ^ GPIOA->IDR ^ 0xA5A5A5A5UL;
    srand(seed);
    printf("Game seeded with: 0x%08lX\r\n", seed);*/

	// START MENU
	fillRectangle(0,0,128,160,0); // clear screen

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
		/*if (quit_requested())
		{
			printf("Quit requested before game start.\r\n");
			goto quit_game;
		}
		delay(40);*/
	}

	// game starts
	fillRectangle(0,0,128,160,0);
	printf("Bee game started - UART OK\r\n");

	int score = 0;
	int correctFlower = rand() % 8;

	printTextX2("SCORE:", 10, 2, yellow, 0);
	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printTextX2(scoreText, 80, 2, yellow, 0);

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
	// putImage(x, y, width, height, image, flipX, flipY);

	int flowerX[8] = {10, 52, 95, 10, 52, 95, 0, 104};
	int flowerY[8] = {20, 20, 20, 126, 126, 126, 70, 70};

	// draw initial flowers
	for (int i = 0; i < 8; i++)
	{
		int flowerType = rand() % 8;
		putImage(flowerX[i], flowerY[i], 24, 24, flowers[flowerType], 0, 0);
	}

	// draw initial bee (facing right)
	//drawBee(x, y, 1);

	// main game loop
	while (1)
	{
		/*if (quit_requested())
		{
			printf("\r\nGame quit by user. Final score: %d\r\n", score);
			goto quit_game;
		}*/
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
			fillRectangle(oldx, oldy, 16, 16, 0);
			oldx = x;
			oldy = y;

			// draw new bee
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
			// Now check for an overlap by checking to see if ANY of the 4 corners of deco are within the target area
			if (isInside(flowerX[correctFlower], flowerY[correctFlower], 24, 24, x, y) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], 24, 24, x + 12, y) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], 24, 24, x, y + 16) ||
				isInside(flowerX[correctFlower], flowerY[correctFlower], 24, 24, x + 12, y + 16))
			{
				score++;
				snprintf(scoreText, sizeof(scoreText), "%d", score);
                printTextX2(scoreText, 80, 2, yellow, 0);

				printf("Collected! Score now: %d\r\n", score);

                // Erase bee at collection position
                fillRectangle(x, y, 16, 16, 0);

                // Clear old flowers
                for (int i = 0; i < 8; i++) {
                    fillRectangle(flowerX[i], flowerY[i], 24, 24, 0);
                }

                // Pick new correct flower & redraw all
                correctFlower = rand() % 8;
                for (int i = 0; i < 8; i++) {
                    int flowerType = rand() % 8;
                    putImage(flowerX[i], flowerY[i], 24, 24, flowers[flowerType], 0, 0);
                }

                // Reset & draw bee at center
                x = 55;
                y = 75;
                oldx = x;
                oldy = y;
                //drawBee(x, y, 1);  // facing right
            }
        }
        delay(40);
    }

/*quit_game:
	fillRectangle(0,0,128,160,0);
	printTextX2("QUIT", 40, 50, red, 0);
	printTextX2("Score: ",25,85,yellow,0);
	snprintf(scoreText, sizeof(scoreText), "%d", score);
	printTextX2(scoreText, 75, 85, yellow, 0);

	printf("Game over screen displayed. Reset to play again.\r\n");

    while (1) {
        __asm("wfi");
    }*/

    return 0;
}
/*
// Add this helper function before main() in main.c
// (Helper to draw bee consistently; facing_right=1 for right, 0 for up/left-ish)
void drawBee(uint16_t bx, uint16_t by, int facing_right)
{
    if (facing_right) {
        putImage(bx, by, 16, 12, beeRight, 0, 0);
    } else {
        putImage(bx, by, 12, 16, beeUp, 0, 0);  // fallback to up for simplicity
    }
}

void uart2_init(void)   // keep name or rename to uart_init() if you prefer
{
    // Enable clocks
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN;     // GPIOA clock
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;   // USART1 clock (F0 uses APB2 for USART1)

    // PA2 = AF1 = USART1_TX (remapped)
    // PA15 = AF1 = USART1_RX (remapped)
    GPIOA->MODER   &= ~((3u << (2*2)) | (3u << (2*15)));   // clear mode
    GPIOA->MODER   |=  ((2u << (2*2)) | (2u << (2*15)));   // AF mode
    GPIOA->AFR[0]  |=  (1u << (4*2));                      // AF1 for PA2
    GPIOA->AFR[1]  |=  (1u << (4*(15-8)));                 // AF1 for PA15

    // Baud rate: 48 MHz / 115200 ≈ 416.666 → BRR = 0x1A0 (416 + 10/16 fraction)
    USART1->BRR = 0x1A0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;  // TX+RX+enable
}

// Redirect printf to USART1
int __io_putchar(int ch)
{
    while (!(USART1->ISR & USART_ISR_TXE));   // wait TX empty
    USART1->TDR = (uint8_t)ch;
    return ch;
}

// Non-blocking quit check (echo printable chars)
int quit_requested(void)
{
    if (USART1->ISR & USART_ISR_RXNE)
    {
        uint8_t c = (uint8_t)USART1->RDR;

        // Echo only printable (cleaner terminal)
        if (c >= 32 && c <= 126) {
            while (!(USART1->ISR & USART_ISR_TXE));
            USART1->TDR = c;
        }

        if (c == 'Q' || c == 'q') {
            return 1;
        }
    }
    return 0;
}*/

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
	// checks to see if point px,py is within the rectange defined by x,y,w,h
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

