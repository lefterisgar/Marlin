/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * Low level pin manipulation routines - used by all the drivers.
 *
 * These are based on the LPC1768 pinMode, digitalRead & digitalWrite routines.
 *
 * Couldn't just call exact copies because the overhead killed the LCD update speed
 * With an intermediate level the softspi was running in the 10-20KHz range which
 * resulted in using about about 25% of the CPU's time.
 */

#ifdef TARGET_LPC1768

#include <LPC17xx.h>
#include <lpc17xx_pinsel.h>
#include "../../../core/macros.h"
//#include <pinmapping.h>

#define LPC_PORT_OFFSET         (0x0020)
#define LPC_PIN(pin)            (1UL << pin)
#define LPC_GPIO(port)          ((volatile LPC_GPIO_TypeDef *)(LPC_GPIO0_BASE + LPC_PORT_OFFSET * port))

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

uint8_t LPC1768_PIN_PORT(const uint8_t pin);
uint8_t LPC1768_PIN_PIN(const uint8_t pin);

#ifdef __cplusplus
  extern "C" {
#endif

// I/O functions
// As defined by Arduino INPUT(0x0), OUTPUT(0x1), INPUT_PULLUP(0x2)
void pinMode_LCD(uint8_t pin, uint8_t mode) {
  #define LPC1768_PIN_PORT(pin) ((uint8_t)((pin >> 5) & 0b111))
  #define LPC1768_PIN_PIN(pin) ((uint8_t)(pin & 0b11111))
  PINSEL_CFG_Type config = { LPC1768_PIN_PORT(pin),
                             LPC1768_PIN_PIN(pin),
                             PINSEL_FUNC_0,
                             PINSEL_PINMODE_TRISTATE,
                             PINSEL_PINMODE_NORMAL };
  switch (mode) {
    case INPUT:
      LPC_GPIO(LPC1768_PIN_PORT(pin))->FIODIR &= ~LPC_PIN(LPC1768_PIN_PIN(pin));
      PINSEL_ConfigPin(&config);
      break;
    case OUTPUT:
      LPC_GPIO(LPC1768_PIN_PORT(pin))->FIODIR |=  LPC_PIN(LPC1768_PIN_PIN(pin));
      PINSEL_ConfigPin(&config);
      break;
    case INPUT_PULLUP:
      LPC_GPIO(LPC1768_PIN_PORT(pin))->FIODIR &= ~LPC_PIN(LPC1768_PIN_PIN(pin));
      config.Pinmode = PINSEL_PINMODE_PULLUP;
      PINSEL_ConfigPin(&config);
      break;
    default: break;
  }
}

void u8g_SetPinOutput(uint8_t internal_pin_number) {
   pinMode_LCD(internal_pin_number, 1);  // OUTPUT
}

void u8g_SetPinInput(uint8_t internal_pin_number) {
   pinMode_LCD(internal_pin_number, 0);  // INPUT
}

void u8g_SetPinLevel(uint8_t  pin, uint8_t  pin_status) {
  #define LPC1768_PIN_PORT(pin) ((uint8_t)((pin >> 5) & 0b111))
  #define LPC1768_PIN_PIN(pin) ((uint8_t)(pin & 0b11111))
  if (pin_status)
    LPC_GPIO(LPC1768_PIN_PORT(pin))->FIOSET = LPC_PIN(LPC1768_PIN_PIN(pin));
  else
    LPC_GPIO(LPC1768_PIN_PORT(pin))->FIOCLR = LPC_PIN(LPC1768_PIN_PIN(pin));
}

uint8_t u8g_GetPinLevel(uint8_t pin) {
  #define LPC1768_PIN_PORT(pin) ((uint8_t)((pin >> 5) & 0b111))
  #define LPC1768_PIN_PIN(pin) ((uint8_t)(pin & 0b11111))
  return (uint32_t)LPC_GPIO(LPC1768_PIN_PORT(pin))->FIOPIN & LPC_PIN(LPC1768_PIN_PIN(pin)) ? 1 : 0;
}

#ifdef __cplusplus
  }
#endif

#endif // TARGET_LPC1768
