#include <Arduino.h>

#define MIDI_NAME {'P', 'i', 'n', 'g', 'b', 'o', 'a', 'r', 'd'}
#define MIDI_NAME_LEN 9

#define KEY_GROUP_NUM 9
#define KEY_PINS 6
#define NUMBER_OF_KEYS 49

#define DEBOUNCE_TIMES 2

#define POWER_SUPPLY_CHECK_PIN 13

#define SELF_DRIVE_INTERVAL 100

const uint8_t group_pins[KEY_GROUP_NUM] {14, 15, 16, 17, 18, 19, 20, 21, 22};

// not KEY0%6..not KEY5%6
const uint8_t key_pins[KEY_PINS] {2, 3, 4, 5, 6, 7};
// Array of pressed keys (initially all 0)
uint8_t keys_pressed[NUMBER_OF_KEYS];
// K1, K2, K3, K4, K5, K12, K13, K14, K15
const uint8_t self_drive_pins[KEY_GROUP_NUM] {8, 9, 10, 11, 12, 26, 23, 24, 25};
volatile uint8_t current_self_drive_pin = 0;
IntervalTimer self_drive_timer = IntervalTimer();

// Map the current array and pressed key to a MIDI note
uint8_t mapToMidi(uint8_t active_key_group, uint8_t key) {
  uint8_t offset = (active_key_group >> 1) * 12;
  // case 3 and case 4 are swapped on purpose to reflect circuitry
  // Uneven offset are the upper octave, even the lower
  if (active_key_group & 1) {
    switch (key) {
      case 0: return offset + 42; // F#2 + offset
      case 1: return offset + 43; // G2 + offset
      case 2: return offset + 44; // G#2 + offset
      case 4: return offset + 45; // A2 + offset
      case 3: return offset + 46; // A#2 + offset
      case 5: return offset + 47; // B2 + offset
    }
  } else {
    switch (key) {
      case 0: return offset + 36; // C2 + offset
      case 1: return offset + 37; // C#2 + offset
      case 2: return offset + 38; // D2 + offset
      case 4: return offset + 39; // D#2 + offset
      case 3: return offset + 40; // E2 + offset
      case 5: return offset + 41; // F2 + offset
    }
  }

  // We hopefully never get here
  return 0;
}

// Check if any of the array pins fell since last time
int8_t getActiveKeyGroup() {
  uint8_t active_groups = 0;
  int8_t last_active_key_group = -1;
  for (uint8_t i = 0; i < KEY_GROUP_NUM; i++) {
    // Update status
    if (digitalReadFast(group_pins[i])==LOW){
      last_active_key_group = (int8_t) i;
      active_groups++;
    }
  }
  
  if (active_groups==1){
    return last_active_key_group;
  }

  // Default return
  return -1;
}

// Set the next self drive pin
FASTRUN void nextSelfDrivePin() {
  // Set the current pin to high
  digitalWriteFast(self_drive_pins[current_self_drive_pin], HIGH);
  // Set the next pin
  current_self_drive_pin = (current_self_drive_pin + 1) % KEY_GROUP_NUM;
}

// Interrupt for power supply check
FASTRUN void powerStateChanged() {
  uint8_t state = digitalReadFast(POWER_SUPPLY_CHECK_PIN);
  // ! inverted
  if (state == LOW) {
    current_self_drive_pin = 0;
    self_drive_timer.begin(nextSelfDrivePin, SELF_DRIVE_INTERVAL);
  } else {
    self_drive_timer.end();
  }
}

// Initial start function
void setup() {
  // Set all in- and outputs
  for (uint8_t i = 0; i < KEY_GROUP_NUM; i++) {
    pinMode(group_pins[i], INPUT);
    pinMode(self_drive_pins[i], OUTPUT);
  }
  for (uint8_t i = 0; i < KEY_PINS; i++) {
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
  int8_t active_key_group = getActiveKeyGroup();  

  // If none is active, we do nothing, else we check the keys
  if (active_key_group >= 0) {
    // Get all the key values ans send the MIDI message if needed
    uint8_t value;
    
    for (uint8_t i = 0; i < KEY_PINS; i++) {
      // due to the circuitry, there is one phantom note above
      // the highest key constantly 'playing'
      // but we don't need to scan that far anyway 
      if (active_key_group == KEY_GROUP_NUM-1 && i>0){
        break;
      }
      value = digitalReadFast(key_pins[i]);
      // If the key is pressed, we send a MIDI message and set the entry in the array
      // ! inverted
      if (value == LOW) { 
        // Check if the key is not already pressed
        if (keys_pressed[active_key_group * 6 + i] >= DEBOUNCE_TIMES) {
          // Send MIDI message
          usbMIDI.sendNoteOn(mapToMidi(active_key_group, i), 127, 1);
        }
        // Set the entry in the array
          keys_pressed[active_key_group * 6 + i] += keys_pressed[active_key_group * 6 + i] < 0xFF? 1:0;
      } else {
        // Check if the key is not already released
        if (keys_pressed[active_key_group * 6 + i] < DEBOUNCE_TIMES) {
          // Send MIDI message
          usbMIDI.sendNoteOff(mapToMidi(active_key_group, i), 0, 1);
        }
        // Set the entry in the array
        keys_pressed[active_key_group * 6 + i] = 0;
      }
    }
  }

  // MIDI Controllers should discard incoming MIDI messages.
  while (usbMIDI.read()) {
  }
}