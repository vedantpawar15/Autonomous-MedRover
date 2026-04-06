/*
 * ============================================================
 *  MOTOR + LINE FOLLOW CALIBRATION SKETCH v4
 * ============================================================
 *  COMMANDS via Serial Monitor (115200 baud, Newline):
 * 
 *  MOTOR TESTS:
 *   f → Both forward
 *   b → Both backward
 *   l → Motor A only (IN1/IN2)
 *   r → Motor B only (IN3/IN4)
 *   s → Stop
 *   + → Speed +10
 *   - → Speed -10
 *   ] → Right offset +5 (right motor more power)
 *   [ → Right offset -5 (right motor less power)
 * 
 *  LINE FOLLOW TESTS:
 *   i → Print IR sensor values once
 *   w → Watch IR sensors continuously
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
int rightOffset = 0;

bool watchingIR    = false;
bool lineFollowing = false;

// ── Renamed struct to avoid conflict with ESP32 IR keyword ──
struct IRSensor {
  bool s1, s2, s3, s4, s5;
};

// ── Function prototypes ───────────────────────────────────
IRSensor readIR();
void printIR();
void doLineFollow();
void stopAll();
void setMotorA(int spd);
void setMotorB(int spd);
void processCommand(char cmd);

// ── Motor setup ───────────────────────────────────────────
void motorSetup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
}

void setMotorA(int spd) {
  if (spd >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else          { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); spd = -spd; }
  ledcWrite(ENA, constrain(spd, 0, 255));
}

void setMotorB(int spd) {
  if (spd >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else          { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); spd = -spd; }
  ledcWrite(ENB, constrain(spd, 0, 255));
}

void stopAll() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  watchingIR    = false;
  lineFollowing = false;
}

// ── IR reading ────────────────────────────────────────────
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
    ir.s1, ir.s2, ir.s3, ir.s4, ir.s5,
    ir.s1?"█":" ", ir.s2?"█":" ", ir.s3?"█":" ",
    ir.s4?"█":" ", ir.s5?"█":" ");
}

// ── Line following ────────────────────────────────────────
void doLineFollow() {
  IRSensor ir = readIR();
  int error = (-2*(int)ir.s1) + (-1*(int)ir.s2) + (0) + (1*(int)ir.s4) + (2*(int)ir.s5);
  int leftSpeed  = baseSpeed - (error * 35);
  int rightSpeed = baseSpeed + (error * 35);

  // A = left motor, B = right motor (swap if needed after motor test)
  setMotorA(leftSpeed);
  setMotorB(constrain(rightSpeed + rightOffset, -255, 255));

  Serial.printf("err:%3d  L:%4d  R:%4d  [%d%d%d%d%d]\n",
    error, leftSpeed, rightSpeed + rightOffset,
    ir.s1, ir.s2, ir.s3, ir.s4, ir.s5);
}

// ── Command processor ─────────────────────────────────────
void processCommand(char cmd) {
  switch(cmd) {

    case 'f':
      stopAll();
      lineFollowing = false; watchingIR = false;
      setMotorA(baseSpeed);
      setMotorB(baseSpeed + rightOffset);
      Serial.println(">>> FORWARD — watch which way robot moves");
      Serial.println("    Motor A = IN1/IN2/ENA   Motor B = IN3/IN4/ENB");
      break;

    case 'b':
      stopAll();
      lineFollowing = false; watchingIR = false;
      setMotorA(-baseSpeed);
      setMotorB(-baseSpeed - rightOffset);
      Serial.println(">>> BACKWARD");
      break;

    case 'l':
      stopAll();
      lineFollowing = false; watchingIR = false;
      setMotorA(baseSpeed);
      Serial.println(">>> Motor A only (IN1/IN2) — which wheel spins?");
      break;

    case 'r':
      stopAll();
      lineFollowing = false; watchingIR = false;
      setMotorB(baseSpeed);
      Serial.println(">>> Motor B only (IN3/IN4) — which wheel spins?");
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case '+':
      baseSpeed = min(baseSpeed + 10, 220);
      Serial.printf(">>> Speed: %d\n", baseSpeed);
      break;

    case '-':
      baseSpeed = max(baseSpeed - 10, 60);
      Serial.printf(">>> Speed: %d\n", baseSpeed);
      break;

    case ']':
      rightOffset += 5;
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case '[':
      rightOffset -= 5;
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case 'i':
      printIR();
      Serial.println("    1=black tape  0=white/floor  S1=leftmost S5=rightmost");
      break;

    case 'w':
      stopAll();
      watchingIR = true;
      Serial.println(">>> Watching IR... type s to stop");
      break;

    case 'g':
      watchingIR = false;
      lineFollowing = true;
      Serial.println(">>> Line following ON — type s to stop");
      break;

    case '\n': case '\r': case ' ': break;

    default:
      Serial.printf("Unknown: '%c'  |  f b l r s + - [ ] i w g\n", cmd);
  }
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();

  pinMode(IR_S1, INPUT);
  pinMode(IR_S2, INPUT);
  pinMode(IR_S3, INPUT);
  pinMode(IR_S4, INPUT);
  pinMode(IR_S5, INPUT);

  Serial.println("==========================================");
  Serial.println("   MOTOR + LINE FOLLOW CALIBRATION v4");
  Serial.println("==========================================");
  Serial.println("STEP 1: l → which wheel is Motor A?");
  Serial.println("STEP 2: r → which wheel is Motor B?");
  Serial.println("STEP 3: f → check forward, fix drift with ] [");
  Serial.println("STEP 4: i → check IR on white then black");
  Serial.println("STEP 5: w → slide over line, watch sensors");
  Serial.println("STEP 6: g → test full line following");
  Serial.println("------------------------------------------");
  Serial.printf("Speed: %d   Right offset: %d\n", baseSpeed, rightOffset);
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }

  if (watchingIR) {
    printIR();
    delay(150);
  }

  if (lineFollowing) {
    doLineFollow();
    delay(20);
  }
}
