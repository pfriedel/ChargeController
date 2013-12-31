#include "TinyWireM.h"
#include <EEPROM.h>

/* 
   the only i2c device I have on hand that I can use as a proof of concept is a
   DS1307.  So I'll toggle the SQW pin. 
*/

#include "RealTimeClockDS1307.h"

byte __attribute__ ((section (".noinit"))) last_mode;

int MAX_MODE = 5; /* how many LEDs do you have? (from zero) */

uint8_t led_grid[6] = {0,0,0,0,0,0};

/* ( AnodePin | CathodePin ) for each LED */
const uint8_t led_dir[6] = {
  ( 1<<3 | 1<<4 ), /* 1 */
  ( 1<<1 | 1<<4 ), /* 2 */
  ( 1<<4 | 1<<3 ), /* 3 */
  ( 1<<4 | 1<<1 ), /* 4 */
  ( 1<<1 | 1<<3 ), /* 5 */
  ( 1<<3 | 1<<1 ), /* 6 */
};

/* ( AnodePin) */
const uint8_t led_out[6] = {
  ( 1<<3 ),
  ( 1<<1 ),
  ( 1<<4 ),
  ( 1<<4 ),
  ( 1<<1 ),
  ( 1<<3 ),
};

uint16_t b;
uint8_t led, drawcount;

/* invert the logic - update the LEDs during the interrupt, constantly draw them otherwise. */
ISR(TIMER0_OVF_vect) { 

  /* this is a nasty hangover from earlier versions, but I haven't the heart to
     take it out in case I actually need more repetition. */
  for(drawcount=0; drawcount<2; drawcount++)  {  
    /* for every LED in the array */
    for(led = 0; led<6; led++) {
      /* light that LED for the percentage of time from 0 to the LED's brightness */
      for( b=0; b < led_grid[led]; b++ ) {
	DDRB = led_dir[led];
	PORTB = led_out[led];
      }
      
      /* and turn the LEDs off for the amount of time in the led_grid array
	 between LED brightness and 255 */
      for( b=led_grid[led]; b < 255; b++ ) {
	DDRB = 0;
	PORTB = 0;
      }
    }

    /* turn everything off at the end in case the last LED was still lit at the end. */
    DDRB=0;
    PORTB=0;
  }
}

/* might be useful since all I'm ever lighting is a single LED..
   void light_led(uint8_t led_num) {
   DDRB = led_dir[led_num];
   PORTB = led_out[led_num];
   }
   
   void leds_off() {
   DDRB = 0;
   PORTB = 0;
   }
*/

void EEReadSettings (void) {
  byte detectBad = 0;
  byte value = 255;

  value = EEPROM.read(0);
  if (value > MAX_MODE)
    detectBad = 1;
  else
    last_mode = value;

  if (detectBad) /* or merely unitialized */
    last_mode = 0;
}

void EESaveSettings (void){
  /* EEPROM.write(Addr, Value);

     Careful if you use this function: EEPROM has a limited number of write
     cycles in its life.  Good for human-operated buttons, bad for automation.
  */

  byte value = EEPROM.read(0);

  if(value != last_mode)
    EEPROM.write(0, last_mode);
}

void setup() {

  /*
    Uses the MCUSR flag to determine if it was a cold or a warm startup. A cold
    startup resumes the prior setting.  A warm startup advances the mode by one
    and goes to the main execution loop.  If the loop times out, the setting is
    saved.  Since the EEPROM has more than enough writes, I'm not terribly
    worried.
  */

  if(bit_is_set(MCUSR, PORF)) { /* Power was just established! */
    MCUSR = 0;                  /* clear MCUSR */
    EEReadSettings();           /* read the last mode out of eeprom */
  }
  else if(bit_is_set(MCUSR, EXTRF)) { /* we're coming out of a reset condition. */
    MCUSR = 0;                        /* clear MCUSR */
    last_mode++;                      /* advance mode */
    
    if(last_mode > MAX_MODE) {
      last_mode = 0;                  /* reset mode */
    }
  }

  /* Set the timer overflow flags */
  TCCR0B |= (1<<CS00);
  TIMSK |= 1<<TOIE0;

  /* may not actually be necessary, but it seems to help get the clock started... */
  TinyWireM.begin();
  RTC.switchTo24h();
  RTC.stop();
  RTC.setSeconds(0);
  RTC.setMinutes(0);
  RTC.setHours(0);
  RTC.setDayOfWeek(1);
  RTC.setDate(1);
  RTC.setMonth(1);
  RTC.setYear(9);
  RTC.setClock();
  RTC.start();
  RTC.readClock();

  /* start servicing interrupts. */
  sei();
}

void loop() {
  /* indicate which mode we're entering */
  for(int x = 0; x <= 255; x++) {
    led_grid[last_mode] = x;
    delay(1);
  }
  delay(500);
  for(int x = 255; x>=0; x--) {
    led_grid[last_mode] = x;
    delay(1);
  }

  delay(100);

  /* if the user hasn't reset by now, assume they want the current mode.  Save it. */
  EESaveSettings();

  bool sqw_out = true;

  while(1) {
    for(int x = 0; x<=5; x++) {
      led_grid[x] = 255;
      delay(500);
      led_grid[x] = 0;

    
      /* 
	 Merely clearing interrupts isn't sufficient since I'm jerking around
	 the state of the port B registers in the interrupt.  So initialize the
	 USI interface again and bob's your uncle.  Only works (so far) at 8mhz,
	 but I only need 8mhz to keep 6 LEDs lit without flickering them.
      */
      cli();
      TinyWireM.begin();
      if(sqw_out == true) {
	RTC.sqwEnable(RTC.SQW_4kHz);
	sqw_out = !sqw_out;
      }
      else if (sqw_out == false ) {
	RTC.sqwDisable(1);
	sqw_out = !sqw_out;
      }
      sei();
    }
  }
}


