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
#ifdef __AVR__

#include "../../inc/MarlinConfigPre.h"
#include "HAL.h"

#if NEEDS_HARDWARE_PWM // Specific meta-flag for features that mandate PWM

struct Timer {
  volatile uint8_t* TCCRnQ[3];  // max 3 TCCR registers per timer
  volatile uint16_t* OCRnQ[3];  // max 3 OCR registers per timer
  volatile uint16_t* ICRn;      // max 1 ICR register per timer
  uint8_t n;                    // the timer number [0->5]
  uint8_t q;                    // the timer output [0->2] (A->C)
};

// Macros for the Timer structure
#define _SET_WGMnQ(TCCRnQ, V) do{ \
    *(TCCRnQ)[0] = (*(TCCRnQ)[0] & ~(0x3 << 0)) | (( int(V)       & 0x3) << 0); \
    *(TCCRnQ)[1] = (*(TCCRnQ)[1] & ~(0x3 << 3)) | (((int(V) >> 2) & 0x3) << 3); \
  }while(0)

// Set TCCR CS bits
#define _SET_CSn(TCCRnQ, V) (*(TCCRnQ)[1] = (*(TCCRnQ[1]) & ~(0x7 << 0)) | ((int(V) & 0x7) << 0))

// Set TCCR COM bits
#define _SET_COMnQ(TCCRnQ, Q, V) (*(TCCRnQ)[0] = (*(TCCRnQ)[0] & ~(0x3 << (6-2*(Q)))) | (int(V) << (6-2*(Q))))

// Set OCRnQ register
#define _SET_OCRnQ(OCRnQ, Q, V) (*(OCRnQ)[Q] = int(V) & 0xFFFF)

// Set ICRn register (one per timer)
#define _SET_ICRn(ICRn, V) (*(ICRn) = int(V) & 0xFFFF)

/**
 * get_pwm_timer
 *  Get the timer information and register of the provided pin.
 *  Return a Timer struct containing this information.
 *  Used by set_pwm_frequency, set_pwm_duty
 */
Timer get_pwm_timer(const pin_t pin) {
  uint8_t q = 0;

  switch (digitalPinToTimer(pin)) {
    // Protect reserved timers (TIMER0 & TIMER1)
    #ifdef TCCR0A
      IF_DISABLED(AVR_AT90USB1286_FAMILY, case TIMER0A:)
      case TIMER0B:
    #endif
    #ifdef TCCR1A
      case TIMER1A: case TIMER1B:
    #endif

    break;

    #if HAS_TCCR2
      case TIMER2: {
        Timer timer = {
          { &TCCR2, nullptr, nullptr },
          { (uint16_t*)&OCR2, nullptr, nullptr },
            nullptr,
            2, 0
        };
        return timer;
      }
    #elif defined(TCCR2A)
      #if ENABLED(USE_OCR2A_AS_TOP)
        case TIMER2A:   break; // protect TIMER2A
        case TIMER2B: {
          Timer timer = {
            { &TCCR2A,  &TCCR2B,  nullptr },
            { (uint16_t*)&OCR2A, (uint16_t*)&OCR2B, nullptr },
              nullptr,
              2, 1
          };
          return timer;
        }
      #else
        case TIMER2B: ++q;
        case TIMER2A: {
          Timer timer = {
            { &TCCR2A,  &TCCR2B,  nullptr },
            { (uint16_t*)&OCR2A, (uint16_t*)&OCR2B, nullptr },
              nullptr,
              2, q
          };
          return timer;
        }
      #endif
    #endif

    #ifdef OCR3C
      case TIMER3C: ++q;
      case TIMER3B: ++q;
      case TIMER3A: {
        Timer timer = {
          { &TCCR3A,  &TCCR3B,  &TCCR3C },
          { &OCR3A,   &OCR3B,   &OCR3C },
            &ICR3,
            3, q
        };
        return timer;
      }
    #elif defined(OCR3B)
      case TIMER3B: ++q;
      case TIMER3A: {
        Timer timer = {
          { &TCCR3A,  &TCCR3B,  nullptr },
          { &OCR3A,   &OCR3B,  nullptr },
            &ICR3,
            3, q
        };
        return timer;
      }
    #endif

    #ifdef TCCR4A
      case TIMER4C: ++q;
      case TIMER4B: ++q;
      case TIMER4A: {
        Timer timer = {
          { &TCCR4A,  &TCCR4B,  &TCCR4C },
          { &OCR4A,   &OCR4B,   &OCR4C },
            &ICR4,
            4, q
        };
        return timer;
      }
    #endif

    #ifdef TCCR5A
      case TIMER5C: ++q;
      case TIMER5B: ++q;
      case TIMER5A: {
        Timer timer = {
          { &TCCR5A,  &TCCR5B,  &TCCR5C },
          { &OCR5A,   &OCR5B,   &OCR5C },
            &ICR5,
            5, q
        };
        return timer;
      }
    #endif
  }

  Timer timer = {
    { nullptr, nullptr, nullptr },
    { nullptr, nullptr, nullptr },
      nullptr,
      0, 0
  };
  return timer;
}

void set_pwm_frequency(const pin_t pin, const uint16_t f_desired) {
  Timer timer = get_pwm_timer(pin);
  if (timer.n == 0) return; // Don't proceed if protected timer or not recognized
  uint16_t size;
  if (timer.n == 2) size = 255; else size = 65535;

  uint16_t res = 255;   // resolution (TOP value)
  uint8_t j = 0;        // prescaler index
  uint8_t wgm = 1;      // waveform generation mode

  // Calculating the prescaler and resolution to use to achieve closest frequency
  if (f_desired != 0) {
    int f = (F_CPU) / (2 * 1024 * size) + 1; // Initialize frequency as lowest (non-zero) achievable
    uint16_t prescaler[] = { 0, 1, 8, /*TIMER2 ONLY*/32, 64, /*TIMER2 ONLY*/128, 256, 1024 };

    // loop over prescaler values
    LOOP_S_L_N(i, 1, 8) {
      uint16_t res_temp_fast = 255, res_temp_phase_correct = 255;
      if (timer.n == 2) {
        // No resolution calculation for TIMER2 unless enabled USE_OCR2A_AS_TOP
        #if ENABLED(USE_OCR2A_AS_TOP)
          const uint16_t rtf = (F_CPU) / (prescaler[i] * f_desired);
          res_temp_fast = rtf - 1;
          res_temp_phase_correct = rtf / 2;
        #endif
      }
      else {
        // Skip TIMER2 specific prescalers when not TIMER2
        if (i == 3 || i == 5) continue;
        const uint16_t rtf = (F_CPU) / (prescaler[i] * f_desired);
        res_temp_fast = rtf - 1;
        res_temp_phase_correct = rtf / 2;
      }

      LIMIT(res_temp_fast, 1U, size);
      LIMIT(res_temp_phase_correct, 1U, size);
      // Calculate frequencies of test prescaler and resolution values
      const int f_temp_fast = (F_CPU) / (prescaler[i] * (1 + res_temp_fast)),
                f_temp_phase_correct = (F_CPU) / (2 * prescaler[i] * res_temp_phase_correct),
                f_diff = ABS(f - f_desired),
                f_fast_diff = ABS(f_temp_fast - f_desired),
                f_phase_diff = ABS(f_temp_phase_correct - f_desired);

      // If FAST values are closest to desired f
      if (f_fast_diff < f_diff && f_fast_diff <= f_phase_diff) {
        // Remember this combination
        f = f_temp_fast;
        res = res_temp_fast;
        j = i;
        // Set the Wave Generation Mode to FAST PWM
        if (timer.n == 2)
          wgm = TERN(USE_OCR2A_AS_TOP, WGM2_FAST_PWM_OCR2A, WGM2_FAST_PWM);
        else
          wgm = WGM_FAST_PWM_ICRn;
      }
      // If PHASE CORRECT values are closes to desired f
      else if (f_phase_diff < f_diff) {
        f = f_temp_phase_correct;
        res = res_temp_phase_correct;
        j = i;
        // Set the Wave Generation Mode to PWM PHASE CORRECT
        if (timer.n == 2)
          wgm = TERN(USE_OCR2A_AS_TOP, WGM2_PWM_PC_OCR2A, WGM2_FAST_PWM);
        else
          wgm = WGM_PWM_PC_ICRn;
      }
    }
  }
  _SET_WGMnQ(timer.TCCRnQ, wgm);
  _SET_CSn(timer.TCCRnQ, j);

  if (timer.n == 2) {
    TERN_(USE_OCR2A_AS_TOP, _SET_OCRnQ(timer.OCRnQ, 0, res));  // Set OCR2A value (TOP) = res
  }
  else
    _SET_ICRn(timer.ICRn, res);         // Set ICRn value (TOP) = res
}

#endif // NEEDS_HARDWARE_PWM

void set_pwm_duty(const pin_t pin, const uint16_t v, const uint16_t v_size/*=255*/, const bool invert/*=false*/) {
  #if NEEDS_HARDWARE_PWM

    // If v is 0 or v_size (max), digitalWrite to LOW or HIGH.
    // Note that digitalWrite also disables pwm output for us (sets COM bit to 0)
    if (v == 0)
      digitalWrite(pin, invert);
    else if (v == v_size)
      digitalWrite(pin, !invert);
    else {
      Timer timer = get_pwm_timer(pin);
      if (timer.n == 0) return; // Don't proceed if protected timer or not recognized
      // Set compare output mode to CLEAR -> SET or SET -> CLEAR (if inverted)
      _SET_COMnQ(timer.TCCRnQ, timer.q TERN_(HAS_TCCR2, + (timer.q == 2)), COM_CLEAR_SET + invert); // COM20 is on bit 4 of TCCR2, so +1 for q==2
      const uint16_t top = timer.n == 2 ? TERN(USE_OCR2A_AS_TOP, *timer.OCRnQ[0], 255) : *timer.ICRn;
      _SET_OCRnQ(timer.OCRnQ, timer.q, uint16_t(uint32_t(v) * top / v_size)); // Scale 8/16-bit v to top value
    }

  #else

    analogWrite(pin, v);
    UNUSED(v_size);
    UNUSED(invert);

  #endif
}

#endif // __AVR__
