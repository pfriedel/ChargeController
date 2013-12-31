#include "TinyWireM.h"
#include <EEPROM.h>
#include "RealTimeClockDS1307.h"


byte __attribute__ ((section (".noinit"))) last_mode;

int MAX_MODE = 5;

uint8_t led_grid[6] = {0,0,0,0,0,0};
const uint8_t led_dir[6] = {
  ( 1<<3 | 1<<4 ), // 1
  ( 1<<1 | 1<<4 ), // 2
  ( 1<<4 | 1<<3 ), // 3
  ( 1<<4 | 1<<1 ), // 4
  ( 1<<1 | 1<<3 ), // 5
  ( 1<<3 | 1<<1 ), //6
};
const uint8_t led_out[6] = {
  ( 1<<3 ),
  ( 1<<1 ),
  ( 1<<4 ),
  ( 1<<4 ),
  ( 1<<1 ),
  ( 1<<3 ),
};

int b1 = 0;
int b2 = 0;
uint16_t b;
uint8_t led, drawcount;
uint32_t end_time = 0;


// invert the logic - update the LEDs during the interrupt, constantly draw them otherwise.
ISR(TIMER0_OVF_vect) { 

  // How many times should the routine loop through the array before returning?
  // It's a scaling factor - one pass makes the display dimmer, 8 passes makes
  // the ISR time out.  Arguably this could also be handled by setting the
  // overflow count to 255>>5 so the ISR gets called 7 times more often than
  // 62500 times/second. (16,000,000 mhz / 256)

  for(drawcount=0; drawcount<2; drawcount++)  {  
    for(led = 0; led<6; led++) {
      for( b=0; b < led_grid[led]; b++ ) {
	DDRB = led_dir[led];
	PORTB = led_out[led];
      }
      
      // and turn the LEDs off for the amount of time in the led_grid array
      // between LED brightness and 255>>DEPTH.
      for( b=led_grid[led]; b < 255; b++ ) {
	DDRB = 0;
	PORTB = 0;
      }
    }
    DDRB=0;
    PORTB=0;
  }
}

//might be useful since all I'm ever lighting is a single LED..
void light_led(uint8_t led_num) {
  DDRB = led_dir[led_num];
  PORTB = led_out[led_num];
}

void leds_off() {
  DDRB = 0;
  PORTB = 0;
}
void EEReadSettings (void) {  // TODO: Detect ANY bad values, not just 255.
  byte detectBad = 0;
  byte value = 255;

  value = EEPROM.read(0);
  if (value > MAX_MODE)
    detectBad = 1;
  else
    last_mode = value;  // MainBright has maximum possible value of 8.

  if (detectBad)
    last_mode = 0; // I prefer the rainbow effect.
}

void EESaveSettings (void){
  //EEPROM.write(Addr, Value);

  // Careful if you use  this function: EEPROM has a limited number of write
  // cycles in its life.  Good for human-operated buttons, bad for automation.

  byte value = EEPROM.read(0);

  if(value != last_mode)
    EEPROM.write(0, last_mode);
}

void setup() {
  if(bit_is_set(MCUSR, PORF)) { // Power was just established!
    MCUSR = 0; // clear MCUSR
    EEReadSettings(); // read the last mode out of eeprom
  }
  else if(bit_is_set(MCUSR, EXTRF)) { // we're coming out of a reset condition.
    MCUSR = 0; // clear MCUSR
    last_mode++; // advance mode
    
    if(last_mode > MAX_MODE) {
      last_mode = 0; // reset mode
    }
  }

  TCCR0B |= (1<<CS00);
  TIMSK |= 1<<TOIE0;

  // may not actually be necessary, but it seems to help get the clock started...
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
  sei();
}

void loop() {
  // indicate which mode we're entering
  for(int x = 0; x <= 255; x++) {
    led_grid[last_mode] = x;
    delay(250/int(255));
  }
  delay(500);
  for(int x = 255; x>=0; x--) {
    led_grid[last_mode] = x;
    delay(250/int(255));
  }

  delay(100);
  
  EESaveSettings();

  bool sqw_out = true;

  while(1) {
    for(int x = 0; x<=5; x++) {
      led_grid[x] = 255;
      delay(250);
      led_grid[x] = 0;

    
      /* 
	 Merely clearing interrupts isn't sufficient since I'm jerking around the
	 state of the port B registers in the interrupt.  So initialize the USI interface
	 again and bob's your uncle.  Only works (so far) at 8mhz, but I only need 8mhz
	 to keep 6 LEDs lit without flickering them.
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


