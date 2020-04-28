//************************************************************
//* Respduino
//*
//* c 2020 Bruce McDonald
//************************************************************
#include <Arduino.h>

// declare functions
long getDistance(int ping, int echo);

// Height and safety margins
const int vesselHeight = 20;
const int tidal = 12;
const int safety = 4;
const int highWaterMark = vesselHeight-safety;
const int lowWaterMark = vesselHeight-safety-tidal;
const int centerWaterMark = vesselHeight / 2;

// PINS
const int motor_a2f = 2;
const int motor_a2e = 3;
const int motor_b2f = 4;
const int motor_b2e = 5;
const int common_ping = 6;
const int echo_a = 7; 
const int echo_b = 8;
const int solenoid_a1 = 9;
const int solenoid_b1 = 10;
const int water_hi_a = 11;
const int water_hi_b = 12;
const int led = 13;
const int button_1 = 14;
const int button_2 = 15;

// Motor curve
const byte motor_curve[] = {1, 3, 8, 18, 38, 78, 128, 177, 217, 237, 247, 252, 254, 255};
const int motor_curve_len = 13;

// State management
int running = 0;            // Start not running
int raisingA = 0;            // Three directions: 1 = raising, -1 = emptying, 0 - unknown
int raisingB = 0;            // Three directions: 1 = raising, -1 = emptying, 0 - unknown
int motor_a2f_index = 0;      // index into the 
int motor_a2e_index = 0;      // index of drive for motor a2e
int motor_b2f_index = 0;      // index of drive for motor b2f
int motor_b2e_index = 0;      // index of drive for motor b2e

int motor_inc=10;             // Drive increment 

int ledState = LOW;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 150;    // the debounce time; increase if the output flickers

/**
 * Run pump A, smoothly ramp the motor until either top or bottom limits are met
 * 
 * direction >0, 0, <0
 */
void runPumpA(int direction) {
  if(direction > 0) {
    motor_a2f_index = motor_a2f_index < motor_curve_len ? motor_a2f_index + 1 : motor_curve_len;
    analogWrite(motor_a2f, motor_curve[motor_a2f_index]);

    motor_a2e_index = motor_a2e_index < motor_curve_len ? motor_a2e_index + 1 : motor_curve_len;
    analogWrite(motor_a2e, motor_curve[motor_a2e_index]);

  } else if(direction < 0) {
    motor_a2f_index = motor_a2f_index > 0 ? motor_a2f_index - 1 : 0;
    analogWrite(motor_a2f, motor_curve[motor_a2f_index]);

    motor_a2e_index = motor_a2e_index > 0 ? motor_a2e_index - 1 : 0;
    analogWrite(motor_a2e, motor_curve[motor_a2e_index]);
  } else {
    analogWrite(motor_a2f, 0);
    analogWrite(motor_a2e, 0);
  }
}

/**
 * Run pump B
 * 
 * direction >0, 0, <0
 */
void runPumpB(int direction) {
  if(direction > 0) {
    motor_b2f_index = motor_b2f_index < motor_curve_len ? motor_b2f_index + 1 : motor_curve_len;
    analogWrite(motor_b2f, motor_curve[motor_b2f_index]);

    motor_b2e_index = motor_b2e_index < motor_curve_len ? motor_b2e_index + 1 : motor_curve_len;
    analogWrite(motor_b2e, motor_curve[motor_b2e_index]);

  } else if(direction < 0) {
    motor_b2f_index = motor_b2f_index > 0 ? motor_b2f_index - 1 : 0;
    analogWrite(motor_b2f, motor_curve[motor_b2f_index]);

    motor_b2e_index = motor_b2e_index > 0 ? motor_b2e_index - 1 : 0;
    analogWrite(motor_b2e, motor_curve[motor_b2e_index]);

  } else {
    analogWrite(motor_b2f, 0);
    analogWrite(motor_b2e, 0);
  }
}

/**
 * Get the distance from the top of the vessel A to the water level
 */
long getWaterLevelA() {
  long height = vesselHeight - getDistance(common_ping, echo_a);

  return max(height, 0);
}

/**
 * Get the distance from the top of the vessel b to the water level
 */
long getWaterLevelB() {
  long height = vesselHeight - getDistance(common_ping, echo_b);

  return max(height, 0);
}

/**
 * Drive the A solenoid 
 */
void solenoidA(int state) {
  digitalWrite(solenoid_a1, state);
}

/**
 * Drive the B solenoid 
 */
void solenoidB(int state) {
  digitalWrite(solenoid_b1, state);
}


/**
 * inhale
 * we force air from vessel B into the lungs
 * both vessels are filling
 * we close Solenoid A, forcing old exhaled air out through check valve
 * we open solenoid B, allowing fresh air into the lungs
 */
void inhale() {
  raisingA = 1;
  raisingB = 1;

  // Close a1
  solenoidA(LOW);

  // Open b1
  solenoidB(HIGH);
}

/**
 * exhale
 * we force air from the lungs into vessel A
 * both vessels are emptying
 * we open Solenoid A, allowing air from the lungs into vessel A
 * we close solenoid B, stopping air from the lungs into vessel B, which is filling via check valve.
 */
void exhale() {
  // Signal emptying
  raisingA = -1;
  raisingB = -1;

  // Open a1
  solenoidA(HIGH);

  // Close b1
  solenoidB(LOW);
}


void setup() {
  Serial.begin(9600); // Starting Serial Terminal

  pinMode(motor_a2f, OUTPUT);
  pinMode(motor_a2e, OUTPUT);
  pinMode(motor_b2f, OUTPUT);
  pinMode(motor_b2e, OUTPUT);
  pinMode(common_ping, OUTPUT);
  pinMode(echo_a, INPUT);
  pinMode(echo_b, INPUT);
  pinMode(solenoid_a1, OUTPUT);
  pinMode(solenoid_b1, OUTPUT);
  pinMode(water_hi_a, INPUT);
  pinMode(water_hi_b, INPUT);
  pinMode(led, OUTPUT);  
  pinMode(button_1, INPUT_PULLUP);
  pinMode(button_2, INPUT_PULLUP);

  // set initial LED state
  digitalWrite(led, ledState);

  // Turn off the pumps
  runPumpA(0);
  runPumpB(0);
}

void loop() {
  // Get the water levels in both
  long heightA = getWaterLevelA();
  long heightB = getWaterLevelB();

  Serial.print("Running: ");
  Serial.print(running);
  Serial.print(" Raising A: ");
  Serial.print(raisingA);
  Serial.print(" Raising B: ");
  Serial.print(raisingB);
  Serial.print(" Height A: ");
  Serial.print(heightA);
  Serial.print("  Height B: ");
  Serial.println(heightB);

  // read the state of the switch into a local variable:
  int reading = digitalRead(button_1);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  // Handle the debounce
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        ledState = !ledState;

        // set the LED:
        digitalWrite(led, ledState);

        // Change the running state
        running = running ? 0 : 1;
      }
    }
  }

  // Are we arerunning
  if(running) {
    // If A is filling
    if(raisingA == 1) {
      // Check the upper water mark on A
      if(heightA >= highWaterMark) {
        // stop filling
        raisingA = 0;
      }
    } else if(raisingA == -1) { // Emptying
      if(heightA <= lowWaterMark) {
        //stop emptying
        raisingA = 0;
      }
    }

    // Check height on B
    if(raisingB == 1) {
      // Check the upper water mark on either 
      if(heightB >= highWaterMark) {
        // stop filling
        raisingB = 0;
      }
    } else if(raisingB == -1) {
      if(heightB <= lowWaterMark) {
        //stop emptying
        raisingB = 0;
      }
    } 

    // Now check for both stopped pumps
    if(raisingA == 0 && raisingB == 0) {

      // If we are above the center point, we empty
      if(heightA > centerWaterMark) {
        exhale();
      } else {  // else we inhale
        inhale();
      }
    }
  } else {
    raisingA = 0;
    raisingA = 0;
  }

  // Run the pumps
  runPumpA(raisingA);
  runPumpB(raisingB);

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;

  // avoid quick switching
  delay(10);
}
