/*
 * ============================================================
 *  MOTOR + LINE FOLLOW CALIBRATION v5
 *  Right offset starts at 120 (your found sweet spot)
 * ============================================================
 *  COMMANDS:
 *   f → Both forward
 *   b → Both backward
 *   l → Left motor only
 *   r → Right motor only
 *   s → Stop
 *   + → Base speed +10
 *   - → Base speed -10
 *   ] → Right offset +5
 *   [ → Right offset -5
 *   i → IR values once
 *   w → Watch IR continuously
 *   g → Start line following
 * ============================================================
 */

#define IN1  27
#define IN2  26
#define IN3  25
#define IN4  33
#define ENA  14
#define ENB  12

#define IR_S1 34
#define IR_S2 35
#define IR_S3 32
#define IR_S4 18
#define IR_S5 19

#define PWM_FREQ 1000
#define PWM_RES  8

int baseSpeed   = 150;
int rightOffset = 120;   // Your sweet spot — right motor gets +120

bool watchingIR    = false;
bool lineFollowing = false;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void printIR();
void doLineFollow();
void stopAll();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void processCommand(char cmd);

void motorSetup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
}

void setMotorLeft(int spd) {
  if (spd >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else          { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); spd = -spd; }
  ledcWrite(ENA, constrain(spd, 0, 255));
}

void setMotorRight(int spd) {
  if (spd >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else          { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); spd = -spd; }
  ledcWrite(ENB, constrain(spd, 0, 255));
}

void stopAll() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA, 0); ledcWrite(ENB, 0);
  watchingIR = false; lineFollowing = false;
}

IRSensor readIR() {
  IRSensor d;
  d.s1 = !digitalRead(IR_S1);
  d.s2 = !digitalRead(IR_S2);
  d.s3 = !digitalRead(IR_S3);
  d.s4 = !digitalRead(IR_S4);
  d.s5 = !digitalRead(IR_S5);
  return d;
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("S1:%d S2:%d S3:%d S4:%d S5:%d   [%s%s%s%s%s]\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
    ir.s1?"█":" ",ir.s2?"█":" ",ir.s3?"█":" ",
    ir.s4?"█":" ",ir.s5?"█":" ");
}

void doLineFollow() {
  IRSensor ir = readIR();
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);

  int leftSpeed  = baseSpeed - (error * 35);
  int rightSpeed = baseSpeed + rightOffset + (error * 35);

  setMotorLeft(leftSpeed);
  setMotorRight(constrain(rightSpeed, -255, 255));

  Serial.printf("err:%3d  L:%4d  R:%4d  [%d%d%d%d%d]\n",
    error, leftSpeed, rightSpeed,
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'f':
      stopAll(); lineFollowing=false; watchingIR=false;
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
      Serial.printf(">>> FORWARD  L:%d  R:%d\n", baseSpeed, baseSpeed+rightOffset);
      break;

    case 'b':
      stopAll(); lineFollowing=false; watchingIR=false;
      setMotorLeft(-baseSpeed);
      setMotorRight(-constrain(baseSpeed + rightOffset, 0, 255));
      Serial.println(">>> BACKWARD");
      break;

    case 'l':
      stopAll(); lineFollowing=false; watchingIR=false;
      setMotorLeft(baseSpeed);
      Serial.println(">>> Left motor only");
      break;

    case 'r':
      stopAll(); lineFollowing=false; watchingIR=false;
      setMotorRight(baseSpeed);
      Serial.println(">>> Right motor only");
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case '+':
      baseSpeed = min(baseSpeed+10, 220);
      Serial.printf(">>> Base speed: %d\n", baseSpeed);
      break;

    case '-':
      baseSpeed = max(baseSpeed-10, 60);
      Serial.printf(">>> Base speed: %d\n", baseSpeed);
      break;

    case ']':
      rightOffset = min(rightOffset+5, 200);
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case '[':
      rightOffset = max(rightOffset-5, 0);
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case 'i':
      printIR();
      Serial.println("    1=black  0=white   S1=leftmost  S5=rightmost");
      break;

    case 'w':
      stopAll(); watchingIR=true;
      Serial.println(">>> Watching IR... (s to stop)");
      break;

    case 'g':
      watchingIR=false; lineFollowing=true;
      Serial.println(">>> Line following ON (s to stop)");
      Serial.printf("    base=%d  rightOffset=%d\n", baseSpeed, rightOffset);
      Serial.println("    Watch 'err' values — should stay near 0 on line");
      break;

    case '\n': case '\r': case ' ': break;

    default:
      Serial.printf("Unknown: '%c'  |  f b l r s + - ] [ i w g\n", cmd);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();
  pinMode(IR_S1, INPUT); pinMode(IR_S2, INPUT);
  pinMode(IR_S3, INPUT); pinMode(IR_S4, INPUT);
  pinMode(IR_S5, INPUT);

  Serial.println("==========================================");
  Serial.println("   MOTOR + LINE FOLLOW CALIBRATION v5");
  Serial.println("==========================================");
  Serial.printf("Base speed: %d   Right offset: %d\n", baseSpeed, rightOffset);
  Serial.println("------------------------------------------");
  Serial.println("STEP 1: type 'f' — check straightness");
  Serial.println("        ']' more right power  '[' less");
  Serial.println("STEP 2: type 'i' — check IR on white/black");
  Serial.println("STEP 3: type 'w' — slide over line");
  Serial.println("STEP 4: type 'g' — test line following");
  Serial.println("        Watch 'err' value in output");
  Serial.println("==========================================");
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(150); }
  if (lineFollowing) { doLineFollow(); delay(20);  }
}
