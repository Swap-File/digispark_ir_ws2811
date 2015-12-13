//Adafruit's library saves ~1.5K+ of program space versus FastLED 3.1, which on a Attiny85 (digispark) is needed.
//Bringing Adafruit's assembly into this program and stripping out more extra stuff saves another 1K (awesome!)

//We can't shut off the IR interrupt while writing out the strip, so just shut off timer interrupts for millis()

#include <EEPROM.h>
#include "IRLremote.h"
#include "PinChangeInterrupt.h" //must use this because its fast enough to fit it's interrupt between pixels

//Pins
#define IR_PIN 2
#define DATA_PIN 3

//This is designed for no more than 64 LEDs per device.
//If you go over 64 some variables will need to be made 16 bit...
//If you have 64 physical LEDs, you will be using a integer index range of -64 to 127
#define NUM_LEDS 30

//Remote Button Codes
#define REMOTE_ADDRESS 61184
#define REPEAT_ADDRESS 0
#define REPEAT_COMMAND 65535

#define FLASH_BUTTON 62475
#define STROBE_BUTTON 61455
#define FADE_BUTTON 60435
#define SMOOTH_BUTTON 59415
#define R1_BUTTON 64260
#define R2_BUTTON 63240
#define R3_BUTTON 62220
#define R4_BUTTON 61200
#define R5_BUTTON 60180
#define G1_BUTTON 64005
#define G2_BUTTON 62985
#define G3_BUTTON 61965
#define G4_BUTTON 60945
#define G5_BUTTON 59925
#define B1_BUTTON 63750
#define B2_BUTTON 62730
#define B3_BUTTON 61710
#define B4_BUTTON 60690
#define B5_BUTTON 59670
#define W_BUTTON 63495
#define UP_BUTTON 65280
#define DOWN_BUTTON 65025
#define ON_BUTTON 64515
#define OFF_BUTTON 64770

const volatile uint8_t *port;         // Output PORT register
uint8_t pinMask;       // Output PORT bitmask

// temporary variables to save latest IR input
volatile uint8_t IRProtocol = 0;
volatile uint16_t IRAddress = 0;
volatile uint32_t IRCommand = 0;

uint16_t IRAddress_previous = 0;
uint32_t IRCommand_previous = 0;

//stuff to save to NVRAM
uint16_t effect_speed = 1000;
uint8_t effect_mode = 1;
int16_t width_lit_up = 2;

uint8_t blend_factor = 255;  //1 is max smoothing  255 is no smoothing
int8_t blend_mode = 7;

uint32_t strip_refresh = 0;
uint32_t effect_time = 0;
uint32_t saved_time = 0;
uint32_t last_color_load_time = 0;

struct CRGB {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

CRGB color1;
CRGB color2;
CRGB * displaycolor1 = &color1;
CRGB * displaycolor2 = &color2;

// Adjust Settings
#define ADJUST_EFFECT_MODE 1
#define ADJUST_FILL_MODE 6
#define ADJUST_EFFECT_SPEED 2
#define ADJUST_EFFECT_SMOOTH 7
#define ADJUST_COLOR_R 3
#define ADJUST_COLOR_G 4
#define ADJUST_COLOR_B 5
uint8_t adjust_mode = 0;

//Bounce types
#define BOUNCE_NONE 0
#define BOUNCE_VISIBLE 1
#define BOUNCE_HIDDEN 2
uint8_t bounce = BOUNCE_NONE;
bool forward_direction = true;
bool wrap_rendering = false;
bool loading_first_color = true;
bool map_color = false;
int16_t starting_index = 0;
uint8_t starting_index_speed = 0;
uint8_t pixels[NUM_LEDS * 3];
bool pinout = 1;
bool disable_fading_on_edge;
void setup() {
  //IR Setup
  attachPCINT(2, IRLinterrupt<IR_NEC>, CHANGE);

  //LED Setup
  port = portOutputRegister(digitalPinToPort(DATA_PIN));
  pinMask = digitalPinToBitMask(DATA_PIN);
  pinMode(DATA_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);
  //Serial.begin(115200);
  color1.r = 0;
  color1.g = 0;
  color1.b = 125;
  effect_speed = 500;
  effect_mode = 1;
  width_lit_up = 2;
  color2.r = 0;
  color2.g = 125;
  color2.b = 0;
  pinMode(1, OUTPUT); //LED
}

void loop() {
  if (IRProtocol) {

    //if held button, do previous action again
    if ( IRAddress == REPEAT_ADDRESS && IRCommand == REPEAT_COMMAND) {
      IRAddress = IRAddress_previous;
      IRCommand = IRCommand_previous;
    }
    IRAddress_previous = IRAddress;
    IRCommand_previous = IRCommand;

    // reset variable to not read the same value twice
    IRProtocol = 0;

    if (IRAddress == REMOTE_ADDRESS) {
      CRGB temp;
      switch (IRCommand) {
        case R1_BUTTON: temp.r = 255; temp.g = 0;   temp.b = 0;   break;
        case R2_BUTTON: temp.r = 205; temp.g = 50;  temp.b = 0;  break;
        case R3_BUTTON: temp.r = 171; temp.g = 100;  temp.b = 0;  break;
        case R4_BUTTON: temp.r = 171; temp.g = 151;  temp.b = 25;  break;
        case R5_BUTTON: temp.r = 105; temp.g = 202;  temp.b = 23;  break;
        case G1_BUTTON: temp.r = 0;   temp.g = 255; temp.b = 0;   break;
        case G2_BUTTON: temp.r = 0;  temp.g = 221;  temp.b = 34;  break;
        case G3_BUTTON: temp.r = 0;  temp.g = 186;  temp.b = 69;  break;
        case G4_BUTTON: temp.r = 0;  temp.g = 140;  temp.b = 116;  break;
        case G5_BUTTON: temp.r = 0;  temp.g = 71;  temp.b = 185;  break;
        case B1_BUTTON: temp.r = 0;   temp.g = 0;   temp.b = 255; break;
        case B2_BUTTON: temp.r = 50;  temp.g = 0;   temp.b = 205;   break;
        case B3_BUTTON: temp.r = 100;  temp.g =0;   temp.b = 156;   break;
        case B4_BUTTON: temp.r = 151; temp.g = 0;  temp.b = 105;  break;
        case B5_BUTTON: temp.r = 202; temp.g = 0;   temp.b = 54;   break;
        case W_BUTTON:  temp.r = 128; temp.g = 128;   temp.b = 128;   break;
        case FLASH_BUTTON:  adjust_mode = ADJUST_EFFECT_SPEED;  break;
        case STROBE_BUTTON: adjust_mode = ADJUST_FILL_MODE;     break;
        case FADE_BUTTON:   adjust_mode = ADJUST_EFFECT_MODE;   break;
        case SMOOTH_BUTTON: adjust_mode = ADJUST_EFFECT_SMOOTH; break;

        case UP_BUTTON:
          switch (adjust_mode) {
            case ADJUST_FILL_MODE:     if (width_lit_up < (NUM_LEDS - 1)) width_lit_up++; break;
            case ADJUST_EFFECT_SPEED:  if (effect_speed > 10) effect_speed = effect_speed >> 1;          break;
            case ADJUST_EFFECT_MODE:   if (++effect_mode > 6) effect_mode = 6;            break;
            case ADJUST_EFFECT_SMOOTH:   if (blend_mode < 8) blend_mode++;    break;
          }
          break;
        case DOWN_BUTTON:
          switch (adjust_mode) {
            case ADJUST_FILL_MODE:     if (width_lit_up > 0) width_lit_up--;                   break;
            case ADJUST_EFFECT_SPEED:  if (effect_speed < 2000)  effect_speed = effect_speed << 1;               break;
            case ADJUST_EFFECT_MODE:   if (effect_mode > 0) effect_mode--;                     break;
            case ADJUST_EFFECT_SMOOTH: if (blend_mode > -8) blend_mode--;  break;
          }
          break;
        case OFF_BUTTON: {//OFF
            EEPROM.write(0, color1.r);
            EEPROM.write(1, color1.g);
            EEPROM.write(2, color1.b);
            EEPROM.write(3, (effect_speed >> 8) & 0xFF);
            EEPROM.write(4, (effect_speed) & 0xFF);
            EEPROM.write(5, effect_mode);
            EEPROM.write(6, width_lit_up);
            EEPROM.write(7, color2.r);
            EEPROM.write(8, color2.g);
            EEPROM.write(9, color2.b);
          } break;
        case ON_BUTTON:  break; //ON
      }

      //if a color was set, load it.  alternate between setting color 1 and 2.
      if (temp.r != 0 || temp.g != 0 || temp.b != 0) {
        if (millis() - last_color_load_time > 1000) {
          *displaycolor1 = temp;
          *displaycolor2 = temp;
          last_color_load_time = millis();
        } else {
          * displaycolor2 = temp;
          last_color_load_time = 0;
        }
      }
    }
  }

  if (blend_mode < 0) {
    disable_fading_on_edge = true;
    blend_factor =  abs( blend_mode) + 1 ;
  } else {
    disable_fading_on_edge = false;
    blend_factor = ( blend_mode) + 1;
  }
  blend_factor =min( 2 ^ ((int)blend_factor),255);

  //set mode variables
  switch (effect_mode) {
    case 1: //cylon mode w/ visible bounce
      bounce = BOUNCE_VISIBLE; wrap_rendering = false; break;
    case 2: //forwards w/ hidden wrap
      bounce = BOUNCE_NONE; wrap_rendering = false; forward_direction = true; break;
    case 3: //backwards w/ hidden wrap
      bounce = BOUNCE_NONE; wrap_rendering = false; forward_direction = false; break;
    case 4: //cylon mode w/ hidden bounce
      bounce = BOUNCE_HIDDEN; wrap_rendering = false; break;
    case 5: //forwards w/ wrap
      bounce = BOUNCE_NONE; wrap_rendering = true; forward_direction = true; break;
    case 6: //backwards w/ wrap
      bounce = BOUNCE_NONE; wrap_rendering = true; forward_direction = false; break;
  }


  if (millis() > strip_refresh) { //lock strip refresh & effect speed to 100hz


    //render the strip
    for (int16_t processed = 0; processed <= NUM_LEDS; processed++) {
      CRGB erased;
      CRGB * temp = &erased;
      int16_t i = starting_index + processed;
      int16_t location = i * 3;

      if (i >=  0 && i < NUM_LEDS) {
        if (processed <= width_lit_up)  temp = displaycolor1;
      } else {
        //handle overhanging wrapped pixels
        if (i >= NUM_LEDS ) location -= (NUM_LEDS * 3);
        else if (i < 0 )    location += (NUM_LEDS * 3);
        if (wrap_rendering == true && processed <= width_lit_up) temp = displaycolor2;
      }

      blend_one(location, temp->g, i);
      blend_one(location + 1, temp->r, i);
      blend_one(location + 2, temp->b, i);
    }

    show();
    strip_refresh += 10;
  }

  //This nudges the pixels around the array
  //It does not instantly correct "bad" settings

  //For example, if you switch to BOUNCE_VISIBLE while the LEDs are hidden,
  //it will simply nudge the LEDs to be visible over the next few cycles.
  //It's smaller and simpler code this way.

  if (millis() > saved_time) {


    //stop movement when in effect_mode 0, otherwise move in saved direction
    if (effect_mode != 0) forward_direction ? starting_index++ : starting_index--;

    if (bounce == BOUNCE_VISIBLE) {
      if (starting_index < 0) {
        forward_direction = true;
        swap_displayed_colors();
      }
      else if (starting_index + width_lit_up >= NUM_LEDS - 1) {
        forward_direction = false;
        swap_displayed_colors();
      }
    } else if (bounce == BOUNCE_HIDDEN) {
      if (starting_index < -width_lit_up) {
        forward_direction = true;
        swap_displayed_colors();
      }
      else if (starting_index >= NUM_LEDS - 1) {
        forward_direction = false;
        swap_displayed_colors();
      }
    } else { //(bounce == BOUNCE_NONE)
      if (wrap_rendering == true) {
        if (starting_index >= NUM_LEDS) {
          starting_index = 0;
          swap_displayed_colors();
        }
        else if (starting_index < -width_lit_up) {
          starting_index = NUM_LEDS - width_lit_up - 1;
          swap_displayed_colors();
        }
      } else { //(wrap_rendering == false)
        if (starting_index >= NUM_LEDS) {
          starting_index = -width_lit_up + 1;
          swap_displayed_colors();
        }
        else if (starting_index < -width_lit_up) {
          starting_index = NUM_LEDS - 1;
          swap_displayed_colors();
        }
      }
    }
    saved_time = saved_time + effect_speed;
  }
}

void swap_displayed_colors(void) {
  CRGB * temp = displaycolor1;
  displaycolor1 = displaycolor2;
  displaycolor2 = temp;
}

//linear blend between changes and directly edit the pixel array
void blend_one(uint8_t dest, uint8_t target, uint16_t i) {
  if (disable_fading_on_edge && ((forward_direction && i == starting_index + width_lit_up) ||  (forward_direction == false && i == starting_index)) ) {
    pixels[dest] = target;
  } else {
    if (pixels[dest] < target) pixels[dest] = min(qadd8(pixels[dest], blend_factor), target);
    else if (pixels[dest] > target) pixels[dest] = max(qsub8(pixels[dest], blend_factor), target);
  }
}

void IREvent(uint8_t protocol, uint16_t address, uint32_t command) {
  pinout = !pinout;
  digitalWrite(1, pinout);

  // called when directly received a valid IR signal.
  if (!IRProtocol) {
    // update the values to the newest valid input
    IRProtocol = protocol;
    IRAddress = address;
    IRCommand = command;
  }
}

//optimized saturating add borrowed from fastled
uint8_t qadd8( uint8_t i, uint8_t j)
{
  asm volatile(
    "add %0, %1    \n\t"
    "brcc L_%=     \n\t"
    "ldi %0, 0xFF  \n\t"
    "L_%=: "
    : "+a" (i)
    : "a"  (j) );
  return i;
}

//optimized saturating subtract borrowed from fastled
uint8_t qsub8( uint8_t i, uint8_t j) {
  asm volatile(
    "sub %0, %1    \n\t"
    "brcc L_%=     \n\t"
    "ldi %0, 0x00  \n\t"
    "L_%=: "
    : "+a" (i)
    : "a"  (j) );
  return i;
}

//Assembly borrowed from adafruit for WS2811 writing
void show(void) {

  //we cant shut off interrupts or else we will miss IR signals
  //but we need to shut off the timer1 interupt or else
  //it will cause flickering
  //this will cause millis to lose a little time

  _SFR_BYTE(TIMSK) &= ~( 1 << TOIE1);

  volatile uint16_t
  i   = NUM_LEDS * 3; // Loop counter
  volatile uint8_t
  *ptr = pixels,   // Pointer to next byte
   b   = *ptr++,   // Current byte value
   hi,             // PORT w/output bit set high
   lo;             // PORT w/output bit set low

  volatile uint8_t next, bit;

  hi   = *port |  pinMask;
  lo   = *port & ~pinMask;
  next = lo;
  bit  = 8;

  asm volatile(
    "head20:"                   "\n\t" // Clk  Pseudocode    (T =  0)
    "st   %a[port],  %[hi]"    "\n\t" // 2    PORT = hi     (T =  2)
    "sbrc %[byte],  7"         "\n\t" // 1-2  if(b & 128)
    "mov  %[next], %[hi]"     "\n\t" // 0-1   next = hi    (T =  4)
    "dec  %[bit]"              "\n\t" // 1    bit--         (T =  5)
    "st   %a[port],  %[next]"  "\n\t" // 2    PORT = next   (T =  7)
    "mov  %[next] ,  %[lo]"    "\n\t" // 1    next = lo     (T =  8)
    "breq nextbyte20"          "\n\t" // 1-2  if(bit == 0) (from dec above)
    "rol  %[byte]"             "\n\t" // 1    b <<= 1       (T = 10)
    "rjmp .+0"                 "\n\t" // 2    nop nop       (T = 12)
    "nop"                      "\n\t" // 1    nop           (T = 13)
    "st   %a[port],  %[lo]"    "\n\t" // 2    PORT = lo     (T = 15)
    "nop"                      "\n\t" // 1    nop           (T = 16)
    "rjmp .+0"                 "\n\t" // 2    nop nop       (T = 18)
    "rjmp head20"              "\n\t" // 2    -> head20 (next bit out)
    "nextbyte20:"               "\n\t" //                    (T = 10)
    "ldi  %[bit]  ,  8"        "\n\t" // 1    bit = 8       (T = 11)
    "ld   %[byte] ,  %a[ptr]+" "\n\t" // 2    b = *ptr++    (T = 13)
    "st   %a[port], %[lo]"     "\n\t" // 2    PORT = lo     (T = 15)
    "nop"                      "\n\t" // 1    nop           (T = 16)
    "sbiw %[count], 1"         "\n\t" // 2    i--           (T = 18)
    "brne head20"             "\n"   // 2    if(i != 0) -> (next byte)
    : [port]  "+e" (port),
    [byte]  "+r" (b),
    [bit]   "+r" (bit),
    [next]  "+r" (next),
    [count] "+w" (i)
    : [ptr]    "e" (ptr),
    [hi]     "r" (hi),
    [lo]     "r" (lo));

  //turn timer 1 back on
  _SFR_BYTE(TIMSK) |= ( 1 << TOIE1);

}

