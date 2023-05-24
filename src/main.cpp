#include <Arduino.h>
#include <Bounce2.h>

#define MIDI_NAME {'P', 'i', 'n', 'g', 'b', 'o', 'a', 'r', 'd'}
#define MIDI_NAME_LEN 9

#define CHECK_PINS 9
#define KEY_PINS 6
#define BOUNCE_TIME 5
#define POWER_SUPPLY_CHECK_PIN 13

#define NUMBER_OF_KEYS 49
#define SELF_DRIVE_INTERVAL 100

Bounce check_pins[CHECK_PINS] {
  {14, BOUNCE_TIME}, // not KEYS1..6
  {15, BOUNCE_TIME}, // not KEYS7..12
  {16, BOUNCE_TIME}, // not KEYS13..18
  {17, BOUNCE_TIME}, // not KEYS19..24
  {18, BOUNCE_TIME}, // not KEYS25..30
  {19, BOUNCE_TIME}, // not KEYS31..36
  {20, BOUNCE_TIME}, // not KEYS37..42
  {21, BOUNCE_TIME}, // not KEYS43..48
  {22, BOUNCE_TIME}  // not KEYS49
};

// not KEY0%6..not KEY5%6
const unsigned char key_pins[KEY_PINS] {2, 3, 4, 5, 6, 7};
// Array of pressed keys (initially all 0)
bool keys_pressed[NUMBER_OF_KEYS];
// K1, K2, K3, K4, K5, K12, K13, K14, K15
const unsigned char self_drive_pins[CHECK_PINS] {8, 9, 10, 11, 12, 26, 23, 24, 25};
volatile unsigned char curr_self_drive_pin = 0;
IntervalTimer self_drive_timer = IntervalTimer();

// Map the current array and pressed key to a MIDI note
unsigned char mapToMidi(char curr_arr, char key) {
  unsigned char offset = (curr_arr >> 1) * 12;
  // TODO: maybe we have to switch the notes and array offsets
  // Uneven offset are the upper octave, even the lower
  if (curr_arr & 1) {
    switch (key) {
      case 0: return offset + 42; // F#2 + offset
      case 1: return offset + 43; // G2 + offset
      case 2: return offset + 44; // G#2 + offset
      case 3: return offset + 45; // A2 + offset
      case 4: return offset + 46; // A#2 + offset
      case 5: return offset + 47; // B2 + offset
    }
  } else {
    switch (key) {
      case 0: return offset + 36; // C2 + offset
      case 1: return offset + 37; // C#2 + offset
      case 2: return offset + 38; // D2 + offset
      case 3: return offset + 39; // D#2 + offset
      case 4: return offset + 40; // E2 + offset
      case 5: return offset + 41; // F2 + offset
    }
  }

  // We hopefully never get here
  return 0;
}

// Check if any of the array pins fell since last time
char findCurrentArrPin() {
  for (unsigned char i = 0; i < CHECK_PINS; i++) {
    // Update status
    check_pins[i].update();
    // Check if the pin fell or is low
    // ! inverted
    if (check_pins[i].fell()) return i;
  }

  // If none fell we should have enough time to see which one is low
  for (unsigned char i = 0; i < CHECK_PINS; i++) {
    if (check_pins[i].read() == LOW) return i;
  }

  // Default return
  return -1;
}

// Set the next self drive pin
FASTRUN void nextSelfDrivePin() {
  // Set the current pin to high
  digitalWriteFast(self_drive_pins[curr_self_drive_pin], HIGH);
  // Set the next pin
  curr_self_drive_pin = (curr_self_drive_pin + 1) % CHECK_PINS;
}

// Interrupt for power supply check
FASTRUN void powerStateChanged() {
  unsigned char state = digitalReadFast(POWER_SUPPLY_CHECK_PIN);
  // ! inverted
  if (state == LOW) {
    curr_self_drive_pin = 0;
    self_drive_timer.begin(nextSelfDrivePin, SELF_DRIVE_INTERVAL);
  } else {
    self_drive_timer.end();
  }
}

// Initial start function
void setup() {
  // Set all in- and outputs
  for (unsigned char i = 0; i < CHECK_PINS; i++) {
    pinMode(check_pins[i].getPin(), INPUT);
    pinMode(self_drive_pins[i], INPUT);
  }
  for (unsigned char i = 0; i < KEY_PINS; i++) {
    pinMode(key_pins[i], INPUT);
  }
  pinMode(POWER_SUPPLY_CHECK_PIN, INPUT);
  // Manual call, so we can set the initial state
  powerStateChanged();
  // Setup interrupt for power supply
  attachInterrupt(POWER_SUPPLY_CHECK_PIN, []() { while (true); }, CHANGE);
}

// Main loop
void loop() {
  // Find active arr pin
  char curr_arr = findCurrentArrPin();  

  // If none is active, we do nothing, else we check the keys
  if (curr_arr >= 0) {
    // Get all the key values ans send the MIDI message if needed
    unsigned char value;
    
    for (unsigned char i = 0; i < KEY_PINS; i++) {
      value = digitalReadFast(key_pins[i]);
      // If the key is pressed, we send a MIDI message and set the entry in the array
      // ! inverted
      if (value == LOW) { 
        // Check if the key is not already pressed
        if (keys_pressed[curr_arr * 6 + i] == 0) {
          // Send MIDI message
          usbMIDI.sendNoteOn(mapToMidi(curr_arr, i), 127, 1);
          // Set the entry in the array
          keys_pressed[curr_arr * 6 + i] = 1;
        }
      } else {
        // Check if the key is not already released
        if (keys_pressed[curr_arr * 6 + i] == 1) {
          // Send MIDI message
          usbMIDI.sendNoteOff(mapToMidi(curr_arr, i), 0, 1);
          // Set the entry in the array
          keys_pressed[curr_arr * 6 + i] = 0;
        }
      }
    }
  }  

  // MIDI Controllers should discard incoming MIDI messages.
  while (usbMIDI.read()) {
  }
}