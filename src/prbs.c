#include <stdint.h>
#include "stm32f031x6.h"

static uint32_t shift_register = 0;

// Blocking single ADC conversion – returns 0–4095
// Using PA5 (ADC_IN5, Arduino A5 header) – leave floating for max noise
uint32_t readADC(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    ADC1->CFGR1 = 0;                        // 12-bit, right aligned
    ADC1->CFGR2 = 0;
    ADC1->SMPR  = 0b100;                    // ~41.5 cycles sampling time

    ADC1->CHSELR = ADC_CHSELR_CHSEL5;       // PA5 / ADC_IN5

    ADC1->CR |= ADC_CR_ADEN;
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) {}

    ADC1->CR |= ADC_CR_ADSTART;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0) {}

    uint32_t value = ADC1->DR;
    ADC1->ISR |= ADC_ISR_EOC;

    return value;
}

void randomize(void)
{
    shift_register = 0;

    for (int i = 0; i < 16; i++)
    {
        uint32_t adc = readADC();
        shift_register ^= (adc << (i & 31));
        shift_register ^= (adc >> (i % 12));
    }

    shift_register ^= SysTick->VAL;
    shift_register ^= (GPIOB->IDR << 16) | (GPIOA->IDR);
    shift_register ^= 0xDEADBEEFUL;

    if (shift_register == 0) {
        shift_register = 0x80000001UL;
    }
}

uint32_t prbs(void)
{
    uint32_t new_bit = ((shift_register >> 27) & 1) ^ ((shift_register >> 30) & 1);
    shift_register = (shift_register << 1) | new_bit;
    return shift_register & 0x7FFFFFFFUL;
}