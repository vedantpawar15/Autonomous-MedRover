/*
 * ============================================================
 *  LINE FOLLOW CALIBRATION v8 — PD Controller
 *  P = corrects based on current error
 *  D = corrects based on how fast error is changing
 *  Together they self-adjust without manual tuning
 * ============================================================
 *  COMMANDS:
 *   g → Start PD line following
 *   s → Stop
 *   + → Base speed +5
 *   - → Base speed -5
 *   ] → Kp +0.5  (more proportional correction)
 *   [ → Kp -0.5  (less proportional correction)
 *   > → Kd +0.5  (more derivative / dampening)
 *   < → Kd -0.5  (less derivative)
 *   f → Forward (open loop test)
 *   i → IR once
 *   w → Watch IR
 *   p → Print settings
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

// ── Tuning parameters ─────────────────────────────────────
int   baseSpeed   = 125;
int   rightOffset = 120;
float Kp          = 25.0;  // Proportional gain — start here
float Kd          = 15.0;  // Derivative gain   — start here

// ── State ─────────────────────────────────────────────────
bool watchingIR    = false;
bool lineFollowing = false;
int  lastError     = 0;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void printIR();
void doLineFollow();
void stopAll();
void stopMotorsOnly();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void processCommand(char cmd);
void printSettings();

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

void stopMotorsOnly() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA, 0); ledcWrite(ENB, 0);
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

void printSettings() {
  Serial.println("======== CURRENT SETTINGS ========");
  Serial.printf("  Base speed  : %d\n",  baseSpeed);
  Serial.printf("  RightOffset : %d\n",  rightOffset);
  Serial.printf("  Kp          : %.1f\n", Kp);
  Serial.printf("  Kd          : %.1f\n", Kd);
  Serial.println("==================================");
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sensorSum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── Lost line: stop and wait ───────────────────────────
  if (sensorSum == 0) {
    stopMotorsOnly();
    static unsigned long lastLostMsg = 0;
    if (millis() - lastLostMsg > 500) {
      Serial.println("!! LINE LOST — waiting...");
      lastLostMsg = millis();
    }
    lastError = 0;  // reset derivative when lost
    return;
  }

  // ── Junction / end of line marker ─────────────────────
  if (sensorSum >= 4) {
    stopMotorsOnly();
    Serial.println("!! JUNCTION or END detected — stopped");
    lineFollowing = false;
    return;
  }

  // ── PD Control ────────────────────────────────────────
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);

  int derivative = error - lastError;  // how fast error is changing
  lastError = error;

  // PD output
  float correction = (Kp * error) + (Kd * derivative);

  int leftSpeed  = baseSpeed             - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  // Print only on change to reduce spam
  static int prevError = 99;
  if (error != prevError) {
    Serial.printf("err:%3d  deriv:%3d  corr:%5.1f  L:%4d  R:%4d  [%d%d%d%d%d]\n",
      error, derivative, correction, leftSpeed, rightSpeed,
      ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    prevError = error;
  }
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      Serial.printf(">>> FORWARD  L:%d  R:%d\n", baseSpeed, baseSpeed+rightOffset);
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case 'g':
      watchingIR=false; lineFollowing=true; lastError=0;
      Serial.println(">>> PD Line following ON (s to stop)");
      printSettings();
      break;

    case '+':
      baseSpeed = min(baseSpeed+5, 200);
      Serial.printf(">>> Speed: %d\n", baseSpeed);
      break;

    case '-':
      baseSpeed = max(baseSpeed-5, 80);
      Serial.printf(">>> Speed: %d\n", baseSpeed);
      break;

    case ']':
      Kp = min(Kp+0.5f, 80.0f);
      Serial.printf(">>> Kp: %.1f\n", Kp);
      break;

    case '[':
      Kp = max(Kp-0.5f, 1.0f);
      Serial.printf(">>> Kp: %.1f\n", Kp);
      break;

    case '>':
      Kd = min(Kd+0.5f, 80.0f);
      Serial.printf(">>> Kd: %.1f\n", Kd);
      break;

    case '<':
      Kd = max(Kd-0.5f, 0.0f);
      Serial.printf(">>> Kd: %.1f\n", Kd);
      break;

    case 'i':
      printIR(); break;

    case 'w':
      stopAll(); watchingIR=true;
      Serial.println(">>> Watching IR... (s to stop)");
      break;

    case 'p':
      printSettings(); break;

    case '\n': case '\r': case ' ': break;

    default:
      Serial.printf("? '%c' | g s f + - ] [ > < i w p\n", cmd);
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
  Serial.println("   LINE FOLLOW CALIBRATION v8 — PD");
  Serial.println("==========================================");
  Serial.println("PD controller auto-corrects smoothly!");
  Serial.println("------------------------------------------");
  Serial.println("TUNING:");
  Serial.println("  Drifts off slowly  → increase Kp with ']'");
  Serial.println("  Wiggles/oscillates → decrease Kp with '['");
  Serial.println("  Overshoots a lot   → increase Kd with '>'");
  Serial.println("  Sluggish response  → decrease Kd with '<'");
  Serial.println("  Stalls on curves   → increase speed '+'");
  Serial.println("==========================================");
  printSettings();
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(150); }
  if (lineFollowing) { doLineFollow(); delay(15);  }
}
