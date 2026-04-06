/*
 * ============================================================
 *  LINE FOLLOW v12 — Edge Riding Fix
 *  Key changes from v11:
 *  1. Loop delay 15ms → 5ms (3x faster corrections)
 *  2. Kp default 30 (stronger pull to center)
 *  3. Edge riding fix: when only S2 or S4 active for too long,
 *     apply extra correction to push toward S3
 *  4. Sharp turn: waits for S2/S3/S4 before resuming PD
 *     (prevents resuming on edge sensors)
 * ============================================================
 *  COMMANDS:
 *   g → Start line following
 *   s → Stop
 *   + → Base speed +5
 *   - → Base speed -5
 *   ] → Kp +1
 *   [ → Kp -1
 *   > → Kd +1
 *   < → Kd -1
 *   ) → Turn speed +5
 *   ( → Turn speed -5
 *   p → Settings
 *   i → IR once
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
int   baseSpeed   = 120;
int   rightOffset = 120;
float Kp          = 30.0;  // increased from 20
float Kd          = 20.0;
int   turnSpeed   = 140;
int   searchSpeed = 90;

// ── State ─────────────────────────────────────────────────
bool lineFollowing = false;
int  lastError     = 0;
int  searchDir     = 1;

// Edge riding detection
int  edgeCount     = 0;
#define EDGE_THRESHOLD 8  // cycles on edge before extra correction

enum FollowState { FOLLOWING, SEARCHING, SHARP_LEFT, SHARP_RIGHT };
FollowState followState = FOLLOWING;

// Telemetry
unsigned long startTime = 0;
int pdCount=0, sharpLCount=0, sharpRCount=0, lostCount=0;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopAll();
void stopMotorsOnly();
void printSettings();
void printSummary();
void doLineFollow();
void doSearch(IRSensor ir);
void processCommand(char cmd);

void motorSetup() {
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
}

void setMotorLeft(int spd) {
  if (spd >= 0) { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); }
  else          { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); spd=-spd; }
  ledcWrite(ENA, constrain(spd,0,255));
}

void setMotorRight(int spd) {
  if (spd >= 0) { digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); }
  else          { digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); spd=-spd; }
  ledcWrite(ENB, constrain(spd,0,255));
}

void stopMotorsOnly() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA,0); ledcWrite(ENB,0);
}

void stopAll() {
  stopMotorsOnly();
  lineFollowing = false;
  followState = FOLLOWING;
  if (startTime > 0) printSummary();
}

IRSensor readIR() {
  IRSensor d;
  d.s1=!digitalRead(IR_S1); d.s2=!digitalRead(IR_S2);
  d.s3=!digitalRead(IR_S3); d.s4=!digitalRead(IR_S4);
  d.s5=!digitalRead(IR_S5);
  return d;
}

void printSettings() {
  Serial.println("======== SETTINGS ========");
  Serial.printf("  baseSpeed   : %d\n",  baseSpeed);
  Serial.printf("  rightOffset : %d\n",  rightOffset);
  Serial.printf("  Kp          : %.1f\n", Kp);
  Serial.printf("  Kd          : %.1f\n", Kd);
  Serial.printf("  turnSpeed   : %d\n",  turnSpeed);
  Serial.printf("  searchSpeed : %d\n",  searchSpeed);
  Serial.println("==========================");
}

void printSummary() {
  Serial.println("\n===== RUN SUMMARY =====");
  Serial.printf("  Time    : %lums\n", millis()-startTime);
  Serial.printf("  PD      : %d\n", pdCount);
  Serial.printf("  SharpL  : %d\n", sharpLCount);
  Serial.printf("  SharpR  : %d\n", sharpRCount);
  Serial.printf("  Lost    : %d\n", lostCount);
  Serial.println("=======================");
  pdCount=sharpLCount=sharpRCount=lostCount=0;
  startTime=0;
}

void doSearch(IRSensor ir) {
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
  if (sum > 0) {
    // Only resume if center sensors found — not edge sensors
    if (ir.s2 || ir.s3 || ir.s4) {
      followState = FOLLOWING;
      lastError = 0;
      edgeCount = 0;
      Serial.println(">> Line found!");
    }
    // If only S1 or S5 found, keep searching
    return;
  }
  if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
  else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── Junction: drive straight through ─────────────────
  if (sum >= 4) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    followState = FOLLOWING;
    return;
  }

  // ── Line lost ─────────────────────────────────────────
  if (sum == 0) {
    lostCount++;
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      edgeCount = 0;
      Serial.printf("[%lums] LOST → search %s\n",
        millis()-startTime, searchDir>0?"RIGHT":"LEFT");
    }
    doSearch(ir);
    return;
  }

  // ── Sharp LEFT: only S1 ───────────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    if (followState != SHARP_LEFT) {
      sharpLCount++;
      followState = SHARP_LEFT;
      Serial.printf("[%lums] SHARP LEFT\n", millis()-startTime);
    }
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed + rightOffset);
    return;
  }

  // ── Sharp RIGHT: only S5 ──────────────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    if (followState != SHARP_RIGHT) {
      sharpRCount++;
      followState = SHARP_RIGHT;
      Serial.printf("[%lums] SHARP RIGHT\n", millis()-startTime);
    }
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed - rightOffset));
    return;
  }

  // ── Normal PD ─────────────────────────────────────────
  pdCount++;
  followState = FOLLOWING;

  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);

  // ── Edge riding detection & fix ───────────────────────
  // If only S2 active (error=-1) or only S4 active (error=+1)
  // for many consecutive cycles → apply extra push toward center
  bool onEdge = (sum == 1) && (ir.s2 || ir.s4);
  if (onEdge) {
    edgeCount++;
  } else {
    edgeCount = 0;
  }

  // After EDGE_THRESHOLD cycles on edge, boost correction
  float edgeBoost = (edgeCount > EDGE_THRESHOLD) ? 1.5f : 1.0f;

  int derivative = error - lastError;
  lastError = error;

  float correction = ((Kp * edgeBoost) * error) + (Kd * derivative);

  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  // Log significant changes
  static int prevErr = 99;
  if (abs(error - prevErr) >= 1 || edgeCount == EDGE_THRESHOLD) {
    Serial.printf("[%lums] PD err:%2d deriv:%2d corr:%.0f L:%d R:%d [%d%d%d%d%d]%s\n",
      millis()-startTime, error, derivative, correction,
      leftSpeed, rightSpeed,
      ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
      edgeCount>EDGE_THRESHOLD?" EDGE_BOOST":"");
    prevErr = error;
  }
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'g':
      lineFollowing=true; followState=FOLLOWING;
      lastError=0; edgeCount=0;
      pdCount=sharpLCount=sharpRCount=lostCount=0;
      startTime=millis();
      Serial.println(">>> v12 ON"); printSettings();
      break;
    case 's': stopAll(); Serial.println(">>> STOPPED"); break;
    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      Serial.printf(">>> FWD L:%d R:%d\n",baseSpeed,baseSpeed+rightOffset);
      break;
    case '+': baseSpeed=min(baseSpeed+5,200); Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case '-': baseSpeed=max(baseSpeed-5,80);  Serial.printf(">>> Speed:%d\n",baseSpeed); break;
    case ']': Kp=min(Kp+1.0f,80.f); Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '[': Kp=max(Kp-1.0f,1.f);  Serial.printf(">>> Kp:%.1f\n",Kp); break;
    case '>': Kd=min(Kd+1.0f,80.f); Serial.printf(">>> Kd:%.1f\n",Kd); break;
    case '<': Kd=max(Kd-1.0f,0.f);  Serial.printf(">>> Kd:%.1f\n",Kd); break;
    case ')': turnSpeed=min(turnSpeed+5,220); Serial.printf(">>> Turn:%d\n",turnSpeed); break;
    case '(': turnSpeed=max(turnSpeed-5,80);  Serial.printf(">>> Turn:%d\n",turnSpeed); break;
    case 'i': {
      IRSensor ir=readIR();
      Serial.printf("S1:%d S2:%d S3:%d S4:%d S5:%d\n",
        ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
      break;
    }
    case 'p': printSettings(); break;
    case '\n': case '\r': case ' ': break;
    default: Serial.printf("? '%c'\n",cmd);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup(); stopAll();
  pinMode(IR_S1,INPUT); pinMode(IR_S2,INPUT);
  pinMode(IR_S3,INPUT); pinMode(IR_S4,INPUT);
  pinMode(IR_S5,INPUT);
  Serial.println("=============================");
  Serial.println("  LINE FOLLOW v12 — Edge Fix");
  Serial.println("=============================");
  Serial.println("Key fixes:");
  Serial.println("  - 3x faster loop (5ms)");
  Serial.println("  - Edge riding detection");
  Serial.println("  - Smarter line recovery");
  Serial.println("=============================");
  printSettings();
}

void loop() {
  while (Serial.available()>0) {
    char cmd=(char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (lineFollowing) { doLineFollow(); delay(5); }  // 5ms not 15ms
}
