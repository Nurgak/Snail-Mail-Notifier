/***************************************************************************************
 *
 * Title:       Snail Mail Notifier - Receiver
 * File:        SMN_Receiver.ino
 * Version:     v0.1
 * Date:        2016-01-24
 * Author:      Karl Kangur <karl.kangur@gmail.com>
 * Licence:     CC-BY
 * Website:     https://github.com/Nurgak/Snail-Mail-Notifier
 * Development: https://hackaday.io/project/1954-snail-mail-notifier
 *
 ***************************************************************************************/
#define F_CPU 16500000UL

#include <avr/wdt.h>
#include <VirtualWire.h> // This version of VirtualWire comes with Digispark libraries and has been modified to work with Digispark
#include "DigiKeyboard.h"
#include "pitch.h"
#include "sound.h"

// ATMEL ATTINY85 / ARDUINO / DIGISPARK pinout
//
//                       +-\/-+
//  RST, Ain0 (D5) PB5  1|    |8  Vcc
// USB-, Ain3 (D3) PB3  2|    |7  PB2 (D2) Ain1, PCINT2, INT0, RX
// USB+, Ain2 (D4) PB4  3|    |6  PB1 (D1) PWM1, PCINT1, BUZZER
//                 GND  4|    |5  PB0 (D0) PWM0, PCINT0, BUTTON
//                       +----+

#define KEY_STROKE        KEY_L // Key stroke sent to the computer when a message is receieved
#define MOD_KEYS          MOD_CONTROL_LEFT | MOD_ALT_LEFT | MOD_GUI_LEFT  // Modifier keys such as Ctrl, Alt, Option...
#define SLEEP_MULTIPLIER  450  // Watchdog resets the system when it has been triggered after 450*8s = 3600s = 1h, maximum is 0xfffe
#define BITRATE           200  // Bitrate at which the emitter sends data to the receiver, must be the same on emitter and receiver
#define PIN_BUTTON        0    // Pin definition for button
#define PIN_BUZZER        1    // Pin definition for buzzer
#define PIN_RX            2    // Pin definition for receiver ping
// Pins 3 and 4 are reserved for USB data lines, defined in Digispark libraries

static volatile boolean mail = false; // Volatile as called from within an interrupt vector (resetSystem())
static volatile boolean buttonBusy = false;
static volatile melodyPlay song = {0, STOP, 0, 0, 0, 0}; // Struct to track sound playing

void setup()
{
  // Switch ADC off as it is not needed
  ADCSRA &= ~_BV(ADEN);
    
  // Shut down unused peripherals (USI and ADC)
  PRR |= _BV(PRUSI) | _BV(PRADC);
  
  // Enable button pull-up
  PORTB |= _BV(PIN_BUTTON);
  
  // Receiver initialisation
  vw_set_rx_pin(PIN_RX);
  vw_setup(BITRATE);
  vw_rx_start();
  
  // Watchdog interrupt to reset the system after an hour
  WDTCR = _BV(WDP3) | _BV(WDP0); // Set watchdog timeout to 8s, do not enable yet
}

// Watchdog timeout interrupt vector called every 8s, non blocking since low priority
ISR(WDT_vect, ISR_NOBLOCK)
{
  static uint16_t sleepTimer;

  // Trick to make the countdown internal to the interrupt vector
  if(sleepTimer == 0xffff)
  {
    sleepTimer = SLEEP_MULTIPLIER;
  }
  else
  {
    sleepTimer--;
  }
  
  if(!sleepTimer)
  {
    sleepTimer = 0xffff;
    
    // Disable watchdog, reset system
    WDTCR &= ~_BV(WDIE);
    resetSystem();
  }
}

// Interrupt vector for buzzer sounds, non blocking since low priority
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
  if(song.action == STOP)
  {
    return;
  }
  
  if(song.calls > 0)
  {
    // Decrement call count
    song.calls--;
    // Toggle pin value to make some sound
    PORTB ^= _BV(PIN_BUZZER);
  }
  else if(song.pause > 0)
  {
    // Decrement pause count while not toggling the buzzer pin
    song.pause--;
  }
  else
  {
    // The melody is finished, stop
    if(song.noteCounter == -1)
    {
      song.noteCounter = 0;
      notification(STOP);
      
      // Allow user to push the button again
      buttonBusy = false;
      
      return;
    }
    
    // Evaluate timeout value for the given sound freqency, one has to use the timeout interrupt vector as the overflow won't work
    // The timeout vector is always called at TOP = 0xff in CTC mode so simply set an offset to the timer 0 counter register: TCNT0
    // TCNT0 = 0xff - (16.5MHz / 1024 / 2 / frequency)
    song.nextCall = 0xff - ((F_CPU >> 11) / song.reference->notes[song.noteCounter].pitch);
    
    // If the frequency of the current sound is the same as the next one insert a 5ms pause at the end of the note duration to define the notes better
    if(song.noteCounter < song.reference->length - 2 && song.reference->notes[song.noteCounter].pitch == song.reference->notes[song.noteCounter + 1].pitch)
    {
      // The casting is necessary or it won't behave as expected
      // The division is by 500 instead of 1000 because the pin is toggeled instead of making a full period per interrupt call, so there are twice the number of cycles
      song.calls = ((uint32_t) song.reference->notes[song.noteCounter].pitch * (song.reference->notes[song.noteCounter].duration - 5)) / 500;
      // Insert a 5ms pause at the end of the sound
      song.pause = (song.reference->notes[song.noteCounter].pitch * 5) / 500;
    }
    else
    {
      // Evaluate call count for given frequency
      song.calls = ((uint32_t) song.reference->notes[song.noteCounter].pitch * song.reference->notes[song.noteCounter].duration) / 500;
    }
    
    if(song.noteCounter < song.reference->length - 1)
    {
      // Evaluate calls for the next frequency
      song.noteCounter++;
    }
    else
    {
      // End of the song, stop
      song.noteCounter = -1;
    }
  }
  
  // Set the counter register to a set value so it would trigger this interrupt next time
  TCNT0 = song.nextCall;
}

void loop()
{
  // Buffer for the received message
  static uint8_t buf[5] = {0};
  static uint8_t buflen = sizeof(buf) / sizeof(buf[0]);

  // Received data variables
  static uint8_t address = 0;
  static uint16_t voltage = 0;
  static uint8_t temperature = 0;
  static uint8_t data = 0; // Dummy data, should be 0x55 by default

  // The receiver can only receieve a message when not playing a notification as they use the same timer
  if(song.action == STOP && !mail && vw_get_message(buf, &buflen))
  {
    // Fill the values
    address = buf[0];
    voltage = (buf[1] << 8) | buf[2];
    temperature = buf[3];
    data = buf[4];

    // Send the key stroke to the computer
    DigiKeyboard.sendKeyStroke(KEY_STROKE, MOD_KEYS);
    
    // Toggle mail flag
    mail = true;

    // Enable watchdog timeout (WDT_vect) interrupt
    wdt_reset();
    WDTCR |= _BV(WDIE);
    
    selectMelody(&melodyNewMail);
  }
  
  // Button was pressed, cannot use the interrupt vector as the DigiKeyboard library needs it, so poll instead
  if(!(PINB & _BV(PIN_BUTTON)) && !buttonBusy)
  {
    button();
  }
  
  // Wait 1ms and fill the time with USB polling to indicate to the computer that the device has not disappeared
  DigiKeyboard.delay(1);
}

void button()
{
  // The user can reset the system by pushing on the button
  resetSystem();
  
  // The user could be holding the button down, prevent calling this multiple times
  buttonBusy = true;
  selectMelody(&melodyButtonPush);
}

void selectMelody(const melody * _melody)
{
  // Select the melody to play by setting the references to the sound and its length
  song.reference = (melody *)_melody;
    
  // Enable notification so the buzzer would play
  notification(PLAY);
}

// Setup the buzzer to notify user of arrived mail
void notification(operation action)
{
  // Save timer 0 configuration, since the receiver uses the same timer as the buzzer
  static uint8_t temp_TCCR0A = TCCR0A;
  static uint8_t temp_TCCR0B = TCCR0B;
  static uint8_t temp_OCR0A = OCR0A;
  static uint8_t temp_TIMSK = TIMSK;
  
  if(action == PLAY)
  {
    // Set buzzer pin to output
    DDRB |= _BV(PIN_BUZZER);
    
    // Reset sound generation variables
    song.calls = 0;
    song.pause = 0;
    song.nextCall = 0;
    song.noteCounter = 0;
    
    TCCR0A = _BV(WGM01); // CTC mode
    OCR0A = 0xff; // Count up to 0xff to trigger the overflow interrupt vector
    TIMSK |= _BV(TOIE0); // Enable the overflow (TIMER0_OVF_vect) interrupt vector to change frequency
    TCCR0B = _BV(CS02) | _BV(CS00); // Prescaler = 1024
  }
  else
  {
    // Restore timer 0 configuration
    TCCR0A = temp_TCCR0A;
    TCCR0B = temp_TCCR0B;
    OCR0A = temp_OCR0A;
    TIMSK = temp_TIMSK;
    
    // Set buzzer pin off and to input mode
    PORTB &= ~_BV(PIN_BUZZER);
    DDRB &= ~_BV(PIN_BUZZER);
  }
  
  song.action = action;
}

void resetSystem()
{
  // Disable watchdog interrupt for automatic reset (not actual MCU reset, only program reset)
  WDTCR |= _BV(WDIE);
  
  // Turn off the notification
  notification(STOP);
  
  // Remove mail flag
  mail = false;
}

