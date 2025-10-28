#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

////////////////////////////////////////////////////////////////////////////////////////
//LCD I2C address: 0x27

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display
 
////////////////////////////////////////////////////////////////////////////////////////
//I/O DEFINITIONS

const int STATUS_LED_PIN = 13;

const int PARAMETER_NUM = 5;
const String parameter_names[] = {"TEST #:", "MAX THROTTLE (%):", "INCREMENT (%):", "MARKERS:", "INCR. LENGTH (s):"};
String parameter_values[PARAMETER_NUM];
int parameter_index;

bool tared;
bool sending; 
bool choosing;
const int TARE_NUM = 2;
const String tare_names[] = {"KNOWN TORQUE:", "KNOWN THRUST:"};
String tare_values[TARE_NUM];
int tare_index;

String input;

////////////////////////////////////////////////////////////////////////////////////////
//KEYBOARD INITIALIZATION (4x4 Membrane keypad)

const byte ROWS = 4; // rows
const byte COLS = 4; // columns

//Non-digit keybindings
const char BACK_BUTTON = 'D';
const char ENTER_INPUT = '#';
const char SEND_INPUT = '*';
const char SKIP_TARE = 'A';
 
// Define the keymap
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//Define pins
byte rowPins[ROWS] = {9, 10, 11, 12};
byte colPins[COLS] = {5, 6, 7, 8};

//Initialize the keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

////////////////////////////////////////////////////////////////////////////////////////
//THROTTLE LOGIC DEFINITIONS
//(For a HARGRAVE MICRODRIVE ESC, accepted PWM frequencies range from 50Hz to 499 Hz

Servo esc; 

const int MIN_THROTTLE = 1000;
int MAX_THROTTLE;

const int ESC_MIN = 1000;
const int ESC_MAX = 2000;

const int ESC_PIN = 3;
long INCREMENT_TIME;
const int THROTTLE_UP_DELAY = 10;

int throttleIncrement;
int pwm_increment;
int cycle_length;
int RAMP_UP_DELAY = 1500;

bool start_motor; 
bool read_ramp_up_data;
bool piecewise;
bool paused = false;
volatile bool done_throttling;
bool throttling_up;

unsigned long prev_interval_timestamp;

////////////////////////////////////////////////////////////////////////////////////////
//MANUAL OVERRIDE DEFINITIONS

const int INTERRUPT_PIN = 2;