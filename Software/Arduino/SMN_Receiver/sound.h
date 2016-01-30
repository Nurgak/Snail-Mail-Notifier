/***************************************************************************************
 *
 * Title:       Snail Mail Notifier - Receiver
 * File:        pitch.cpp
 * Version:     v0.1
 * Date:        2016-01-24
 * Author:      Karl Kangur <karl.kangur@gmail.com>
 * Licence:     CC-BY
 * Website:     https://github.com/Nurgak/Snail-Mail-Notifier
 * Development: https://hackaday.io/project/1954-snail-mail-notifier
 *
 ***************************************************************************************/
#ifndef SOUND_H
#define SOUND_H

// Needed for varaible type definitions
#include <Arduino.h>

// Define a note with a pitch and a duration
struct note
{
  uint16_t pitch;
  uint16_t duration;
};

// Define a melody with notes and a length (total number of notes)
struct melody
{
  const note * notes;
  uint8_t length;
};

typedef enum {STOP, PLAY} operation;
struct melodyPlay
{
  melody * reference;
  operation action;
  uint16_t calls;
  uint16_t pause;
  uint16_t nextCall;
  int8_t noteCounter;
};

// Available melodies to use in the main program, declared in the sound.cpp
extern const melody melodyNewMail;
extern const melody melodyButtonPush;

#endif

