 /***************************************************************************************
 *
 * Title:       Snail Mail Notifier - Emitter
 * File:        SMN_Emitter.ino
 * Version:     v0.1
 * Date:        2016-01-24
 * Author:      Karl Kangur <karl.kangur@gmail.com>
 * Licence:     CC-BY
 * Website:     https://github.com/Nurgak/Snail-Mail-Notifier
 * Development: https://hackaday.io/project/1954-snail-mail-notifier
 *
 ***************************************************************************************/
#include <avr/sleep.h>
#include <avr/wdt.h>
// VirtualWire downloaded from http://www.airspayce.com/mikem/arduino/VirtualWire/ and renamed to VirtualWire_emitter,
// needed so it would not conflict with the receiver VirtualWire library
#include <VirtualWire_emitter.h>

#define ADDRESS           0xf1  // This is arbitrary, might be useful when multiple emitters transmit to one receiver
#define BITRATE           200   // Lower bitrate allows communication over greater distances
#define SLEEP_MULTIPLIER  1     // Time the microcontroller sleeps after sending a message, multiples of 8 seconds, ex: 8*8=64 seconds
#define TEMP_OFFSET       273   // Temperature offset when reading internal temperature sensor
#define SEND_TIMES        3     // How many times the message is sent every time, higher number is more robust
#define PIN_LED           3     // Definition for the LED pin
#define PIN_TX            4     // Definition for the transmission pin

void setup()
{
  // Disable interrupts for setup, just in case
  cli();
  
  // Set the sleep mode that is going to be used, lowest possible is SLEEP_MODE_PWR_DOWN
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  // Setup the transmission parameters
  vw_setup(BITRATE);
  vw_set_tx_pin(PIN_TX);
  
  // Power Reduction Register (datasheet p. 38)
  // Turn everything off: Timer1, USI and ADC clock signals stopped.
  //PRR = (1 << PRTIM1) | (1 << PRUSI) | (1 << PRADC);
  // DO NOT USE!!!
  // For some reason the current goes up 200uA when disabling these features, so do not disable them.
  // This is probably due to the state of the peripheral being frozen while it still somehow consumes power.
  
  // Switch ADC converter off to save power
  ADCSRA &= ~_BV(ADEN);
  
  // Enable pull-up resistors on pins PB2, PB1 and PB0
  PORTB |= _BV(PORTB2) | _BV(PORTB1) | _BV(PORTB0);
  
  // Enable pin change interrupts on pins PB2, PB1 and PB0, they all call the same interrupt vector: PCINT0_vect
  PCMSK |= _BV(PCINT2) | _BV(PCINT1) | _BV(PCINT0);
  // PB2 has technically another way to be interrupted (INT0), allowing for a different interrupt vector,
  // but it reqires system clock which is not available since the microcontroller is going to sleep
  
  // Enable all interrupts
  sei();
}

void loop()
{
  static uint8_t sleep_counter;

  // Enable the pin change interrupts
  GIMSK |= _BV(PCIE);
  // This needs to be called twice the second time it is called, because the PCIE bit is not set, this is a very strange issue...
  GIMSK |= _BV(PCIE);
  
  // Put the microcontroller to sleep to conserve power
  sleep();
  // The microcontroller has been waked up by a pin change interrupt...
  
  // Send the message to the receiver
  sendMessage();
  
  // Configure the watchdog to timeout after 8 seconds
  WDTCR = _BV(WDP3) | _BV(WDP0);

  // Reset the sleep counter
  sleep_counter = SLEEP_MULTIPLIER;
  
  // Reset the watchdog timer, it is an asychronous task and might be about to timeout
  wdt_reset();

  while(sleep_counter--)
  {
    // Enable the watchdog interrupt, it will wake up the microcontroller in 8 seconds
    WDTCR |= _BV(WDIE);
    // Power down the microcontroller
    sleep();
  }
}

void sleep()
{
  sleep_enable();
  sleep_mode();
  // System sleeps here
  sleep_disable();
}

// Pin change interrupt vector
ISR(PCINT0_vect)
{
  // Turn off the pin change interrupts so it will not be called over and over
  GIMSK &= ~_BV(PCIE);
}

// Watchdog interrupt vector
ISR(WDT_vect)
{
  // Disable the watchdog interrupt
  WDTCR &= ~_BV(WDIE);
}

void sendMessage()
{
  // Message buffer
  uint8_t msg[5] = {0};
  uint8_t retries_counter = SEND_TIMES;

  // Switch analog-to-digital (ADC) on
  ADCSRA |= _BV(ADEN);
  
  // Get the battery voltage and temperature values
  uint16_t voltage = readVcc();
  int8_t temperature = readTemp();
  
  // Switch ADC off to conserve power
  ADCSRA &= ~_BV(ADEN);

  // Build the message packet
  // Protocol: 1 byte for address, 2 bytes for voltage level, 1 byte for temperature, 1 byte for data
  msg[0] = ADDRESS;
  msg[1] = voltage >> 8;
  msg[2] = voltage & 0xff;
  msg[3] = temperature;
  msg[4] = 0x55; // Dummy data, can be used for checksums or whatever

  // Configure the watchdog for 1 second call
  WDTCR = _BV(WDP2) | _BV(WDP1);
  
  // Set LED pin as output
  DDRB |= _BV(PIN_LED);

  // Send the message multiple times to make sure the receiver catches it
  while(retries_counter--)
  {    
    // Turn LED on
    PORTB |= _BV(PIN_LED);

    // Actually send the message to the receiver
    vw_send(msg, sizeof(msg) / sizeof(msg[0]));
    vw_wait_tx();

    // Turn LED off
    PORTB &= ~_BV(PIN_LED);

    // Skip the last wait period
    if(!retries_counter)
    {
      break;
    }
    
    // Reset the watchdog timer right before enabling it
    wdt_reset();
    // Enable the watchdog interrupt (not watchdog timer)
    WDTCR |= _BV(WDIE);
    // Turn off the microcontroller for 1 second
    sleep();
  }
  
  // Set LED port as input
  DDRB &= ~_BV(PIN_LED);
}

// Helper function to read battery voltage, return value in millivolts
uint16_t readVcc()
{  
  // Read bandgap reference voltage (1.1V) with reference at Vcc (?V)
  ADMUX = _BV(MUX3) | _BV(MUX2);
  
  // Wait for voltage reference reference to settle, 3ms is minimum
  _delay_ms(5);
  
  // Convert analog to digital twice (first value is unreliable)
  ADCSRA |= _BV(ADSC);
  while(bit_is_set(ADCSRA,ADSC));
  ADCSRA |= _BV(ADSC);
  while(bit_is_set(ADCSRA,ADSC));

  // Convert the value to millivolts
  return 1126400L / ADC;
}

// Helper function to read the internal temperature sensor
int8_t readTemp()
{
  int8_t i;
  uint16_t average = 0;
  
  // Read internal temperature sensor value using the bandgap reference (1.1V)
  ADMUX = _BV(REFS1) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0);
  
  // Wait for the voltage reference to settle, 3ms is minimum
  _delay_ms(5);
  
  // Convert and dump the first value as it is not guaranteed to be correct
  ADCSRA |= _BV(ADSC);
  while(bit_is_set(ADCSRA, ADSC));
  
  // Make an average over 8 readings
  for(i = 0; i < 8; i++)
  {
    // Start the analog-to-digital (ADC) converstion
    ADCSRA |= _BV(ADSC);
    // Wait for the ADC converstion to finish
    while(bit_is_set(ADCSRA, ADSC));
    average += ADC;
  }
  
  // Divide by 8 and remove offset
  return (average >> 3) - TEMP_OFFSET;
}

