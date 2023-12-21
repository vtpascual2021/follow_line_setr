#include <Servo.h>
#include "FastLED.h"
#include <Thread.h>
#include <ThreadController.h>

#define TRIG_PIN 13  
#define ECHO_PIN 12

#define PIN_RBGLED 4
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// Define motor control pins
#define PIN_Motor_AIN_1 7
#define PIN_Motor_PWMA 5
#define PIN_Motor_BIN_1 8
#define PIN_Motor_PWMB 6

// Define infrared sensor pins
#define PIN_ITR20001_LEFT A2
#define PIN_ITR20001_MIDDLE A1
#define PIN_ITR20001_RIGHT A0

// Enable/Disable motor control.
// HIGH: motor control enabled
// LOW: motor control disabled
#define PIN_Motor_STBY 3

// PID constants
#define KP 30  // Proportional gain
#define KD 15  // Derivative gain 0.2


// Servo for controlling the robot
Servo servo;

int leftMotorSpeed = 0; 
int rightMotorSpeed = 0;

int prevError = 0;
bool last_turn = true; // true -- right; false -- left

float distance = 100; // save distances measured by ultrasound sensor

ThreadController controller = ThreadController();
Thread distance_thread = Thread();

char previous_char = '\0';
bool linelost = false;

void check_distance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  distance = pulseIn(ECHO_PIN, HIGH) / 59.0;
}

// Calculate color for RGB LED
uint32_t Color(uint8_t r, uint8_t g, uint8_t b)
{
  return (((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
}

void send_char(char x){
  if (x != previous_char){
    Serial.println(x);
    previous_char = x;
  }
}

void setup() {
  Serial.begin(9600);

  FastLED.addLeds<NEOPIXEL, PIN_RBGLED>(leds, NUM_LEDS);
  FastLED.setBrightness(20);

  distance_thread.onRun(check_distance);
  distance_thread.setInterval(100);

  controller.add(&distance_thread);

  // Initialize ultrasound pins
  pinMode(TRIG_PIN, OUTPUT);  
  pinMode(ECHO_PIN, INPUT);

  // Initialize motor control pins
  pinMode(PIN_Motor_AIN_1, OUTPUT);
  pinMode(PIN_Motor_PWMA, OUTPUT);
  pinMode(PIN_Motor_BIN_1, OUTPUT);
  pinMode(PIN_Motor_PWMB, OUTPUT);

  // Initialize infrared sensor pins
  pinMode(PIN_ITR20001_LEFT, INPUT);
  pinMode(PIN_ITR20001_MIDDLE, INPUT);
  pinMode(PIN_ITR20001_RIGHT, INPUT);

  // Initialize servo
  servo.attach(10);

  // Set motor control pins as outputs
  pinMode(PIN_Motor_STBY, OUTPUT);

  // Enable motor control
  digitalWrite(PIN_Motor_STBY, HIGH);
  digitalWrite(PIN_Motor_AIN_1, HIGH);
  digitalWrite(PIN_Motor_BIN_1, HIGH);

  // Until 'c' is not received, it waits for the message
  while(1) {

    if (Serial.available()) {
      char c = Serial.read();
      
      // when received, start 'loop' function
      if (c == 'c')  {

        // send 's' to ESP ('starting circuit')
        send_char('s');
        break;
      } 
    }
  }
}

void loop() {
  controller.run();

  // if an object is detected, stop motors and show blue LED
  if (distance <=8) {
    FastLED.showColor(Color(0,0,255));
    analogWrite(PIN_Motor_PWMA, 0);
    analogWrite(PIN_Motor_PWMB, 0);
    int round_dis = (distance >= 0.0) ? static_cast<int>(distance + 0.5) : static_cast<int>(distance - 0.5);
    Serial.println(round_dis);
    while(1){
    };
  }
  // Read sensor values
  int leftSensor = analogRead(PIN_ITR20001_LEFT);
  int middleSensor = analogRead(PIN_ITR20001_MIDDLE);
  int rightSensor = analogRead(PIN_ITR20001_RIGHT);

  // if there is no line
  if (middleSensor < 500 && leftSensor < 500 && rightSensor < 500) {
    if (!last_turn) {
      analogWrite(PIN_Motor_PWMA, 200);
      analogWrite(PIN_Motor_PWMB, 30);
    } else {
      analogWrite(PIN_Motor_PWMB, 200);
      analogWrite(PIN_Motor_PWMA, 30);
    }
    FastLED.showColor(Color(255,0,0));
    // send '(l)ost circuit' to ESP
    send_char('l');
    linelost = true;

  // if only 'middleSensor' reads line
  } else if (middleSensor > 500 && leftSensor < 500 && rightSensor < 500) {
    if (linelost){
      send_char('f');
    }
    linelost = false;
    // set a constant forward speed
    int constantSpeed = 140;
    
    analogWrite(PIN_Motor_PWMA, constantSpeed);
    analogWrite(PIN_Motor_PWMB, constantSpeed);

    // show a green LED for debugging purposes
    FastLED.showColor(Color(0,255,0));
  } else {
    if (linelost){
      send_char('f');
    }
    linelost = false;
    // calculate actual error and apply a PID
    int error = 10 * (leftSensor * (-1) + rightSensor * 1);
    int derivative = error - prevError; // 'D' controller

    int motorSpeed = KP * error;  // 'P' controller

    // modify turn depending on deviation
    if (error < 0) {
      leftMotorSpeed = - motorSpeed - KD * derivative;
      rightMotorSpeed = 50;
      last_turn = false;
    } else {
      leftMotorSpeed = 50;
      rightMotorSpeed = motorSpeed + KD * derivative;
      last_turn = true;
    }

    analogWrite(PIN_Motor_PWMA, abs(leftMotorSpeed));
    analogWrite(PIN_Motor_PWMB, abs(rightMotorSpeed));

    FastLED.showColor(Color(0,255,0));
    prevError = error;
  }
}
