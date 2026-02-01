#define TRIG_PIN 2
#define ECHO_PIN 3
#include <Servo.h>

Servo arm;
const int SERVO_PIN = 4;


const int ARM_DOWN = 0;
const int ARM_MID  = 40;
const int ARM_UP   = 170;



const int S0 = A0;
const int S1 = A1;
const int S2 = A2;


const int S3 = A3;
const int OUT = 12;

const int ENA = 5;    // PWM
const int ENB = 10;   // PWM
const int IN1 = 6;
const int IN2 = 7;
const int IN3 = 8;
const int IN4 = 9;

// Tune these
const int MAX_SPD = 250;     // 0-255
const int MIN_SPD = 120;     // raise if motors stall
const int STEP    = 10;      // ramp step
const int STEP_MS = 80;      // ramp delay
const int delayNum = 50;
const int turnAmount = 400;

int turns = 0;
int turn_dir = -1; //Lets do -1 is left, 1 is right
bool leftpath = true;
bool rightpath = false;
int adjustment_factor = 0;
String boxStatus = "search";

String main_color = "green";

enum Color { COLOR_UNKNOWN=0, COLOR_RED=1, COLOR_GREEN=2, COLOR_BLUE=3, COLOR_DARK=4 };


void setup() {
  // put your setup code here, to run once:

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);
  

  // 20% scaling: S0=LOW, S1=HIGH
  digitalWrite(S0, LOW);
  digitalWrite(S1, HIGH);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("This is gonna be generational. Smaller number => stronger colour.");

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  arm.attach(SERVO_PIN);
  arm.write(ARM_MID);

  stopMotors();
  delay(1000);

}

void loop() {
  pathScan();
  if (leftpath && !rightpath){
    forward(250);
  }
  else if(!leftpath && !rightpath){
    turnLeft(turnAmount);
  }
  else if(leftpath && rightpath){
    turnRight(turnAmount);
  } 
  else{
    forward(100);
  } 
  

}


void pathScan(){
  if (boxStatus == "search"){
    if (colorCheck() == "blue"){
      pickBox();
      boxStatus = "picked";
    }
  }
  else if (boxStatus == "picked"){
    if (colorCheck() == "blue"){
      dropBox();
      boxStatus = "dropped";
    }
  }
  
  leftpath = (colorCheck() == "green" || colorCheck() == "black");
  turnRight(turnAmount);
  rightpath = (colorCheck() == "green" || colorCheck() == "black");  
  if (!rightpath) turnLeft(turnAmount); 
}

void placeBox(){
  backward(300);
  arm.write(ARM_DOWN);
  delay(1000);
  arcRight(3000);
  moveArm(ARM_MID);
  delay(800);
  turnLeft(turn_90 + 50);
  arm.write(ARM_DOWN);
  delay(300);
  turnLeft(100);
  arm.write(ARM_UP);
  delay(300);
  turnRight(100 + turn_90);
  forward(200);
}

void moveArmSmooth(int target) {
  int current = arm.read();
  if (current < target) {
    for (int a = current; a <= target; a++) {
      arm.write(a);
      delay(10);
    }
  } else {
    for (int a = current; a >= target; a--) {
      arm.write(a);
      delay(10);
    }
  }
}

// --- COLOR HELPERS ---- 

unsigned long readPulse(bool s2, bool s3) {
  digitalWrite(S2, s2 ? HIGH : LOW);
  digitalWrite(S3, s3 ? HIGH : LOW);
  delay(3);
  unsigned long t = pulseIn(OUT, LOW, 50000); // 50ms timeout
  if (t == 0) t = 50000;
  return t;
}

unsigned long avgPulse(bool s2, bool s3, int n = 5) {
  unsigned long sum = 0;
  for (int i = 0; i < n; i++) sum += readPulse(s2, s3);
  return sum / n;
}

String colorCheck(){
  unsigned long r = avgPulse(false, false); // red filter
  unsigned long b = avgPulse(false, true);  // blue filter
  unsigned long g = avgPulse(true,  true);  // green filter
  unsigned long c = avgPulse(true,  false); // clear/brightness

  // Print raw values
  Serial.print("R "); Serial.print(r);
  Serial.print("  G "); Serial.print(g);
  Serial.print("  B "); Serial.print(b);
  Serial.print("  C "); Serial.print(c);

  // Timeout / disconnected
  if (r >= 49000 && g >= 49000 && b >= 49000) {
    //Serial.println("  -> unknown(timeout)");
    return "black";
  }

  // --- WHITE detection: very bright AND RGB close-ish ---
  unsigned long maxRGB = max(r, max(g, b));
  unsigned long minRGB = min(r, min(g, b));
  unsigned long spread = maxRGB - minRGB;

  const unsigned long WHITE_C_MAX = 120;      // tune if needed
  const unsigned long WHITE_SPREAD_MAX = 80;  // tune if needed

  if (c <= WHITE_C_MAX && spread <= WHITE_SPREAD_MAX) {
    //Serial.println("  -> white");
    return "white";
  }

  // --- BLACK detection: based on your measured black signature ---
  if (r >= 1500 && r <= 2100 &&
      g >= 1950 && g <= 2400 &&
      b >= 1600 && b <= 2100 &&
      c >= 520  && c <= 820) {
    //Serial.println("  -> black");
    return "black";
  }

  // --- RGB classification with a dominance margin ---
  unsigned long m = min(r, min(g, b));
  unsigned long second = (m == r) ? min(g, b) : (m == g ? min(r, b) : min(r, g));

  const unsigned long MARGIN = 80; // increase to 120 if still false blue

  if (second - m < MARGIN) {
    //Serial.println("  -> unknown(tie)");
    return "unknown";
  }

  if (m == r) {
    //Serial.println("  -> red");
    return "red";
  }
  if (m == g) {
    //Serial.println("  -> green");
    return "green";
  }

  //Serial.println("  -> blue");
  return "blue";
}

// -------- Motor helpers --------

// dirA=true means forward, false means backward
void setDirection(bool dirA, bool dirB) {
  // Motor A
  digitalWrite(IN1, dirA ? HIGH : LOW);
  digitalWrite(IN2, dirA ? LOW  : HIGH);

  // Motor B
  digitalWrite(IN3, dirB ? HIGH : LOW);
  digitalWrite(IN4, dirB ? LOW  : HIGH);
}

void drive(int speedA, int speedB) {
  speedA = constrain(speedA, 0, 255);
  speedB = constrain(speedB, 0, 255);
  analogWrite(ENA, speedA);
  analogWrite(ENB, speedB);
}

void rampUp(int fromSpd, int toSpd) {
  if (fromSpd > toSpd) {
    for (int s = fromSpd; s >= toSpd; s -= STEP) {
      drive(s, s);
      delay(STEP_MS);
    }
  } else {
    for (int s = fromSpd; s <= toSpd; s += STEP) {
      drive(s, s);
      delay(STEP_MS);
    }
  }
}

void stopMotors() {
  // coast
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  // optional: also clear direction pins
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void forward(int len){
  setDirection(true, true);  // A forward, B forward
  rampUp(MAX_SPD, MAX_SPD);
  delay(len);

  // stop
  stopMotors();
  delay(delayNum);
}

void backward(int len){
  setDirection(false, false);  // A forward, B forward
  rampUp(MAX_SPD, MAX_SPD);
  delay(len);

  // stop
  stopMotors();
  delay(delayNum);
}

void turnRight(int len){
  setDirection(true, false);
  drive(MAX_SPD, MAX_SPD);
  delay(len + adjustment_factor);

  stopMotors();
  delay(delayNum);

}

void turnLeft(int len){
  setDirection(false, true);
  drive(MAX_SPD, MAX_SPD);
  delay(len);

  stopMotors();
  delay(delayNum);

}

void moveArm(int movement){
  moveArmSmooth(movement);
  delay(20);
}

void arcRight(int len){
  setDirection(true, true);
  drive(120, 0);
  delay(len);

  stopMotors();
  delay(delayNum);

}

void arcLeft(int len){
  setDirection(true, true);
  drive(0, 120);
  delay(len);

  stopMotors();
  delay(delayNum);

}
