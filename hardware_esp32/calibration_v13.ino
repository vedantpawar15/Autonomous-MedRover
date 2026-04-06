/*
 * ============================================================
 *  LINE FOLLOW v13 — Back to v10 (what worked) + no junction stop
 *  v10 successfully did S-shape. This keeps that exact logic.
 *  Only change: junction (4+ sensors) drives through instead of stop
 * ============================================================
 *  COMMANDS:
 *   g → Start
 *   s → Stop
 *   + → Speed +5
 *   - → Speed -5
 *   ] → Kp +0.5
 *   [ → Kp -0.5
 *   > → Kd +0.5
 *   < → Kd -0.5
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

// ── Tuning — same as v10 that worked ─────────────────────
int   baseSpeed   = 125;
int   rightOffset = 120;
float Kp          = 20.0;
float Kd          = 20.0;
int   turnSpeed   = 150;
int   searchSpeed = 100;

bool lineFollowing = false;
int  lastError     = 0;
int  searchDir     = 1;

enum FollowState { FOLLOWING, SEARCHING, SHARP_LEFT, SHARP_RIGHT };
FollowState followState = FOLLOWING;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopAll();
void stopMotorsOnly();
void printSettings();
void doLineFollow();
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

void doLineFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── Junction: 4+ sensors → drive straight through ─────
  if (sum >= 4) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    followState = FOLLOWING;
    return;
  }

  // ── Lost line → search based on last error ─────────── 
  if (sum == 0) {
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST → search %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    // Spin toward last known line direction
    if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
    else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    return;
  }

  // ── Sharp LEFT: only S1 active ────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    if (followState != SHARP_LEFT) {
      followState = SHARP_LEFT;
      Serial.println("SHARP LEFT");
    }
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed + rightOffset);
    return;
  }

  // ── Sharp RIGHT: only S5 active ───────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    if (followState != SHARP_RIGHT) {
      followState = SHARP_RIGHT;
      Serial.println("SHARP RIGHT");
    }
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed - rightOffset));
    return;
  }

  // ── Normal PD following ───────────────────────────────
  followState = FOLLOWING;
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError = error;

  float correction = (Kp * error) + (Kd * derivative);
  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  // Print only on change
  static int prevErr = 99;
  if (error != prevErr) {
    Serial.printf("err:%2d L:%4d R:%4d [%d%d%d%d%d]\n",
      error, leftSpeed, rightSpeed,
      ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    prevErr = error;
  }
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'g':
      lineFollowing=true; followState=FOLLOWING; lastError=0;
      Serial.println(">>> v13 ON"); printSettings();
      break;
    case 's': stopAll(); Serial.println(">>> STOPPED"); break;
    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      Serial.printf(">>> FWD L:%d R:%d\n",baseSpeed,baseSpeed+rightOffset);
      break;
    case '+': baseSpeed=min(baseSpeed+5,200); Serial.printf("Speed:%d\n",baseSpeed); break;
    case '-': baseSpeed=max(baseSpeed-5,80);  Serial.printf("Speed:%d\n",baseSpeed); break;
    case ']': Kp=min(Kp+0.5f,80.f); Serial.printf("Kp:%.1f\n",Kp); break;
    case '[': Kp=max(Kp-0.5f,1.f);  Serial.printf("Kp:%.1f\n",Kp); break;
    case '>': Kd=min(Kd+0.5f,80.f); Serial.printf("Kd:%.1f\n",Kd); break;
    case '<': Kd=max(Kd-0.5f,0.f);  Serial.printf("Kd:%.1f\n",Kd); break;
    case ')': turnSpeed=min(turnSpeed+5,220); Serial.printf("Turn:%d\n",turnSpeed); break;
    case '(': turnSpeed=max(turnSpeed-5,80);  Serial.printf("Turn:%d\n",turnSpeed); break;
    case 'i': {
      IRSensor ir=readIR();
      Serial.printf("[%d%d%d%d%d]\n",ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
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
  Serial.println("================================");
  Serial.println("  LINE FOLLOW v13");
  Serial.println("  Same as v10 that worked!");
  Serial.println("  Junction = drives through now");
  Serial.println("================================");
  printSettings();
}

void loop() {
  while (Serial.available()>0) {
    char cmd=(char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (lineFollowing) { doLineFollow(); delay(15); }
}
