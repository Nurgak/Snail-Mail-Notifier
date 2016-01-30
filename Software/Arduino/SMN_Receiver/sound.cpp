/***************************************************************************************
 *
 * Title:       Snail Mail Notifier - Receiver
 * File:        sound.cpp
 * Version:     v0.1
 * Date:        2016-01-24
 * Author:      Karl Kangur <karl.kangur@gmail.com>
 * Licence:     CC-BY
 * Website:     https://github.com/Nurgak/Snail-Mail-Notifier
 * Development: https://hackaday.io/project/1954-snail-mail-notifier
 *
 ***************************************************************************************/
#include "sound.h"
#include "pitch.h"

// Melody to play when a new mail (message) arrives
const note newMailNotes[] = {
  {G2, 100},
  {C3, 100},
  {E3, 100},
  {G3, 100},
  {C4, 100},
  {E4, 100},
  {G4, 300},
  {E4, 300},
  {A2b, 100},
  {C3, 100},
  {E3b, 100},
  {A3b, 100},
  {C4, 100},
  {E4b, 100},
  {A4b, 300},
  {E4b, 300},
  {B2b, 100},
  {D3, 100},
  {F3, 100},
  {B3b, 100},
  {D4, 100},
  {F4, 100},
  {B4b, 300},
  {B4b, 100},
  {B4b, 100},
  {B4b, 100},
  {C5, 600},
};

const melody melodyNewMail = {newMailNotes, sizeof(newMailNotes) / sizeof(note)};

// Melody to play when the button is pushed
const note buttonPushNotes[] = {
  {B5, 100},
  {E6, 200},
};

const melody melodyButtonPush = {buttonPushNotes, sizeof(buttonPushNotes) / sizeof(note)};

