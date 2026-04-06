/*
 * ============================================================
 *  LINE FOLLOW v11 — Full Telemetry + No Junction Stop
 *  Logs every state change with timestamp
 *  Paste the Serial output to Claude for analysis!
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
 *   p → Print settings
 *   t → Toggle full telemetry on/off (default: on)
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
int   turnSpeed   = 150;
int   searchSpeed = 100;

// ── Flags ─────────────────────────────────────────────────
bool watchingIR    = false;
bool lineFollowing = false;
bool telemetry     = true;   // full logging on by default
int  lastError     = 0;
int  searchDir     = 1;

// ── Telemetry counters ────────────────────────────────────
int  pdCount       = 0;
int  sharpLCount   = 0;
int  sharpRCount   = 0;
int  searchCount   = 0;
int  lostCount     = 0;
unsigned long startTime = 0;

enum FollowState {
  FOLLOWING,
  SEARCHING,
  SHARP_TURN_LEFT,
  SHARP_TURN_RIGHT
};
FollowState followState = FOLLOWING;
FollowState prevState   = FOLLOWING;

struct IRSensor { bool s1,s2,s3,s4,s5; };

// ── Prototypes ────────────────────────────────────────────
IRSensor readIR();
void printIR();
void doLineFollow();
void stopAll();
void stopMotorsOnly();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void processCommand(char cmd);
void printSettings();
void printSummary();
void doSearch();
void logState(const char* label, IRSensor ir, int L, int R, float corr);

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
  if (startTime > 0) printSummary();
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
  Serial.println("======== SETTINGS ========");
  Serial.printf("  baseSpeed   : %d\n",  baseSpeed);
  Serial.printf("  rightOffset : %d\n",  rightOffset);
  Serial.printf("  Kp          : %.1f\n", Kp);
  Serial.printf("  Kd          : %.1f\n", Kd);
  Serial.printf("  turnSpeed   : %d\n",  turnSpeed);
  Serial.printf("  searchSpeed : %d\n",  searchSpeed);
  Serial.printf("  telemetry   : %s\n",  telemetry?"ON":"OFF");
  Serial.println("==========================");
}

void printSummary() {
  unsigned long elapsed = millis() - startTime;
  Serial.println("\n========== RUN SUMMARY ==========");
  Serial.printf("  Duration     : %lu ms\n", elapsed);
  Serial.printf("  PD cycles    : %d\n",  pdCount);
  Serial.printf("  Sharp LEFT   : %d\n",  sharpLCount);
  Serial.printf("  Sharp RIGHT  : %d\n",  sharpRCount);
  Serial.printf("  Search events: %d\n",  searchCount);
  Serial.printf("  Lost events  : %d\n",  lostCount);
  Serial.println("=================================");
  Serial.println("Paste this full output to Claude!");
  // reset counters
  pdCount=sharpLCount=sharpRCount=searchCount=lostCount=0;
  startTime=0;
}

// ── Log only on state change or significant error change ──
void logState(const char* label, IRSensor ir, int L, int R, float corr) {
  if (!telemetry) return;
  unsigned long t = millis() - startTime;
  Serial.printf("[%5lums] %s  [%d%d%d%d%d]  L:%4d R:%4d  corr:%.1f\n",
    t, label,
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
    L, R, corr);
}

void doSearch() {
  IRSensor ir = readIR();
  int sensorSum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
  if (sensorSum > 0) {
    followState = FOLLOWING;
    lastError = 0;
    logState("FOUND  ", ir, 0, 0, 0);
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

  // ── Junction: 4+ sensors → treat as sharp turn, don't stop ──
  // Instead, keep turning in the direction of last error
  if (sensorSum >= 4) {
    // Junction = intersection of main line and branch
    // Keep moving forward through it — don't stop
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
    if (followState != FOLLOWING) {
      logState("JUNCT  ", ir,
        baseSpeed, baseSpeed+rightOffset, 0);
    }
    followState = FOLLOWING;
    return;
  }

  // ── Line lost → search ────────────────────────────────
  if (sensorSum == 0) {
    lostCount++;
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      searchCount++;
      logState("LOST   ", ir, 0, 0, lastError);
      Serial.printf("         Searching %s (lastErr:%d)\n",
        searchDir>0?"RIGHT":"LEFT", lastError);
    }
    doSearch();
    return;
  }

  // ── Sharp LEFT: only S1 active ────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    sharpLCount++;
    if (followState != SHARP_TURN_LEFT) {
      followState = SHARP_TURN_LEFT;
      logState("SHARP_L", ir, -turnSpeed, turnSpeed+rightOffset, 0);
    }
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed + rightOffset);
    return;
  }

  // ── Sharp RIGHT: only S5 active ───────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    sharpRCount++;
    if (followState != SHARP_TURN_RIGHT) {
      followState = SHARP_TURN_RIGHT;
      logState("SHARP_R", ir, turnSpeed, -(turnSpeed-rightOffset), 0);
    }
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed - rightOffset));
    return;
  }

  // ── Normal PD ─────────────────────────────────────────
  pdCount++;
  bool stateChanged = (followState != FOLLOWING);
  followState = FOLLOWING;

  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError = error;

  float correction = (Kp * error) + (Kd * derivative);
  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  // Log on state change or significant error
  static int prevLogErr = 99;
  if (stateChanged || abs(error - prevLogErr) >= 1) {
    logState("PD     ", ir, leftSpeed, rightSpeed, correction);
    prevLogErr = error;
  }
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      Serial.printf(">>> FORWARD  L:%d R:%d\n",baseSpeed,baseSpeed+rightOffset);
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case 'g':
      watchingIR=false; lineFollowing=true;
      followState=FOLLOWING; lastError=0;
      pdCount=sharpLCount=sharpRCount=searchCount=lostCount=0;
      startTime=millis();
      Serial.println(">>> Line following v11 ON");
      Serial.println("    Full telemetry active — paste output to Claude!");
      printSettings();
      break;

    case 't':
      telemetry=!telemetry;
      Serial.printf(">>> Telemetry: %s\n", telemetry?"ON":"OFF");
      break;

    case '+': baseSpeed=min(baseSpeed+5,200); Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case '-': baseSpeed=max(baseSpeed-5,80);  Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case ']': Kp=min(Kp+0.5f,80.f); Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '[': Kp=max(Kp-0.5f,1.f);  Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '>': Kd=min(Kd+0.5f,80.f); Serial.printf(">>> Kd:%.1f\n",Kd); break;
    case '<': Kd=max(Kd-0.5f,0.f);  Serial.printf(">>> Kd:%.1f\n",Kd); break;
    case ')': turnSpeed=min(turnSpeed+5,220); Serial.printf(">>> TurnSpd:%d\n",turnSpeed); break;
    case '(': turnSpeed=max(turnSpeed-5,80);  Serial.printf(">>> TurnSpd:%d\n",turnSpeed); break;

    case 'i': printIR(); break;
    case 'p': printSettings(); break;
    case '\n': case '\r': case ' ': break;
    default: Serial.printf("? '%c' | g s f t + - ] [ > < ) ( i p\n",cmd);
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
  Serial.println("  LINE FOLLOW v11 — Telemetry Edition");
  Serial.println("==========================================");
  Serial.println("Type 'g' → run → type 's' to stop");
  Serial.println("Then PASTE the full output here to Claude!");
  Serial.println("Claude will analyze and tell you exactly");
  Serial.println("what to tune and by how much.");
  Serial.println("==========================================");
  printSettings();
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (lineFollowing) { doLineFollow(); delay(15); }
}
