/*
 * ============================================================
 *  LINE FOLLOW CALIBRATION v10 — PD + Recovery + Sharp Turns
 *  Special case: when only outermost sensor active,
 *  robot spins in place instead of using PD correction
 * ============================================================
 *  COMMANDS:
 *   g → Start line following
 *   s → Stop
 *   + → Base speed +5
 *   - → Base speed -5
 *   ] → Kp +0.5
 *   [ → Kp -0.5
 *   > → Kd +0.5
 *   < → Kd -0.5
 *   ) → Turn speed +5
 *   ( → Turn speed -5
 *   f → Forward test
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

// ── Tuning ────────────────────────────────────────────────
int   baseSpeed   = 125;
int   rightOffset = 120;
float Kp          = 20.0;
float Kd          = 20.0;
int   turnSpeed   = 150;  // spin speed for sharp 90° turns
int   searchSpeed = 100;

// ── State ─────────────────────────────────────────────────
enum FollowState { FOLLOWING, SEARCHING, SHARP_TURN_LEFT, SHARP_TURN_RIGHT };
FollowState followState = FOLLOWING;

bool watchingIR    = false;
bool lineFollowing = false;
int  lastError     = 0;
int  searchDir     = 1;

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
void doSearch();

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
  followState = FOLLOWING;
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
  Serial.printf("  Base speed   : %d\n",  baseSpeed);
  Serial.printf("  Right offset : %d\n",  rightOffset);
  Serial.printf("  Kp           : %.1f\n", Kp);
  Serial.printf("  Kd           : %.1f\n", Kd);
  Serial.printf("  Turn speed   : %d\n",  turnSpeed);
  Serial.printf("  Search speed : %d\n",  searchSpeed);
  Serial.println("==================================");
}

void doSearch() {
  IRSensor ir = readIR();
  int sensorSum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
  if (sensorSum > 0) {
    followState = FOLLOWING;
    lastError = 0;
    Serial.println(">> Line found! Resuming PD...");
    return;
  }
  if (searchDir > 0) {
    setMotorLeft(searchSpeed);
    setMotorRight(-searchSpeed);
  } else {
    setMotorLeft(-searchSpeed);
    setMotorRight(searchSpeed);
  }
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sensorSum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── Junction / end marker ─────────────────────────────
  if (sensorSum >= 4) {
    stopMotorsOnly();
    lineFollowing = false;
    Serial.println("!! JUNCTION or END — stopped");
    return;
  }

  // ── Line lost → search ────────────────────────────────
  if (sensorSum == 0) {
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      Serial.printf("!! Lost — searching %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    doSearch();
    return;
  }

  // ── SHARP TURN: only S1 active → hard left spin ───────
  // S1 only means line is at far left — need to turn left sharply
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    followState = SHARP_TURN_LEFT;
    // Spin left in place until S3 or center sensors catch line
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed + rightOffset);
    Serial.println(">> SHARP LEFT spin");
    return;
  }

  // ── SHARP TURN: only S5 active → hard right spin ──────
  // S5 only means line is at far right — need to turn right sharply
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    followState = SHARP_TURN_RIGHT;
    // Spin right in place until S3 or center sensors catch line
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed - rightOffset));
    Serial.println(">> SHARP RIGHT spin");
    return;
  }

  // ── Normal PD following ────────────────────────────────
  followState = FOLLOWING;
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError = error;

  float correction = (Kp * error) + (Kd * derivative);
  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  static int prevError = 99;
  if (error != prevError) {
    Serial.printf("err:%3d  L:%4d  R:%4d  [%d%d%d%d%d]\n",
      error, leftSpeed, rightSpeed,
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

    case 's': stopAll(); Serial.println(">>> STOPPED"); break;

    case 'g':
      watchingIR=false; lineFollowing=true;
      followState=FOLLOWING; lastError=0;
      Serial.println(">>> Line following v10 ON (s to stop)");
      printSettings();
      break;

    case '+': baseSpeed=min(baseSpeed+5,200); Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case '-': baseSpeed=max(baseSpeed-5,80);  Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case ']': Kp=min(Kp+0.5f,80.f); Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '[': Kp=max(Kp-0.5f,1.f); Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '>': Kd=min(Kd+0.5f,80.f); Serial.printf(">>> Kd:%.1f\n",Kd); break;
    case '<': Kd=max(Kd-0.5f,0.f); Serial.printf(">>> Kd:%.1f\n",Kd); break;

    case ')':
      turnSpeed=min(turnSpeed+5,220);
      Serial.printf(">>> Turn speed:%d\n",turnSpeed);
      break;
    case '(':
      turnSpeed=max(turnSpeed-5,80);
      Serial.printf(">>> Turn speed:%d\n",turnSpeed);
      break;

    case 'i': printIR(); break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watching IR..."); break;
    case 'p': printSettings(); break;
    case '\n': case '\r': case ' ': break;
    default: Serial.printf("? '%c' | g s f + - ] [ > < ) ( i w p\n",cmd);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();
  pinMode(IR_S1,INPUT); pinMode(IR_S2,INPUT);
  pinMode(IR_S3,INPUT); pinMode(IR_S4,INPUT);
  pinMode(IR_S5,INPUT);

  Serial.println("==========================================");
  Serial.println("  LINE FOLLOW v10 — PD + Recovery + Sharp Turns");
  Serial.println("==========================================");
  Serial.println("NEW: Sharp turn detection!");
  Serial.println("  Only S1 active → hard left spin");
  Serial.println("  Only S5 active → hard right spin");
  Serial.println("  ')' = turn faster   '(' = turn slower");
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
