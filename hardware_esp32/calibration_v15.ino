/*
 * ============================================================
 *  LINE FOLLOW v15 — Fixed Junction Detection
 *  Key fixes from v14:
 *  1. Junction debounce reduced 600ms → 150ms
 *  2. Junction detection has PRIORITY over lost/search logic
 *  3. Turn direction fixed to LEFT (your junction is [11100])
 *  4. Loop delay reduced to 10ms for faster junction catch
 * ============================================================
 *  COMMANDS:
 *   A → Room 1 (turn at junction 1)
 *   B → Room 2 (turn at junction 2)
 *   C → Room 3 (no turn, straight to end)
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
 *   w → Watch IR
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

// ── Junction control ──────────────────────────────────────
int targetJunction  = 1;
int junctionsSeen   = 0;
#define JUNCTION_DEBOUNCE 150  // ms — reduced from 600ms

// ── Junction debounce state ───────────────────────────────
bool junctionActive    = false;
unsigned long jEnterTime = 0;
bool junctionCounted   = false;  // prevent double counting

// ── Line follow state ─────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
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
void printIR();
void doLineFollow();
void processCommand(char cmd);
void executeTurn();

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
  lineFollowing  = false;
  watchingIR     = false;
  followState    = FOLLOWING;
  junctionsSeen  = 0;
  junctionActive = false;
  junctionCounted= false;
}

IRSensor readIR() {
  IRSensor d;
  d.s1=!digitalRead(IR_S1); d.s2=!digitalRead(IR_S2);
  d.s3=!digitalRead(IR_S3); d.s4=!digitalRead(IR_S4);
  d.s5=!digitalRead(IR_S5);
  return d;
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("[%d%d%d%d%d]\n",ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
}

void printSettings() {
  Serial.println("======== SETTINGS ========");
  Serial.printf("  baseSpeed     : %d\n",  baseSpeed);
  Serial.printf("  rightOffset   : %d\n",  rightOffset);
  Serial.printf("  Kp            : %.1f\n", Kp);
  Serial.printf("  Kd            : %.1f\n", Kd);
  Serial.printf("  turnSpeed     : %d\n",  turnSpeed);
  Serial.printf("  searchSpeed   : %d\n",  searchSpeed);
  Serial.printf("  targetJunction: %d %s\n", targetJunction,
    targetJunction==0 ? "(C - straight)" :
    targetJunction==1 ? "(A - Room 1)"   : "(B - Room 2)");
  Serial.println("==========================");
}

// ── Execute 90 degree left turn ───────────────────────────
// Your junction is [11100] = branch goes LEFT
void executeTurn() {
  Serial.println(">> Executing LEFT turn...");

  // First drive forward a bit to clear the junction
  setMotorLeft(baseSpeed);
  setMotorRight(constrain(baseSpeed+rightOffset,0,255));
  delay(200);

  // Spin left until center sensor finds the branch line
  unsigned long spinStart = millis();
  unsigned long maxSpin   = 1500;  // max 1.5 seconds spinning

  while (millis() - spinStart < maxSpin) {
    // Spin left: right motor forward, left motor backward
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed+rightOffset, 0, 255));

    IRSensor ir = readIR();
    // Stop spinning when center sensors find the line
    if (ir.s2 || ir.s3 || ir.s4) {
      Serial.println(">> Line found after turn!");
      break;
    }
    delay(10);
  }

  stopMotorsOnly();
  delay(100);

  // Reset PD state
  lastError   = 0;
  followState = FOLLOWING;
  Serial.println(">> Turn complete, resuming line follow");
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── JUNCTION DETECTION (highest priority) ─────────────
  if (sum >= 3) {
    // Enter junction zone
    if (!junctionActive) {
      junctionActive  = true;
      junctionCounted = false;
      jEnterTime      = millis();
      Serial.printf("Junction zone entered [%d%d%d%d%d]\n",
        ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    }

    // Count junction after debounce time
    if (!junctionCounted && (millis() - jEnterTime >= JUNCTION_DEBOUNCE)) {
      junctionCounted = true;
      junctionsSeen++;
      Serial.printf("*** Junction %d CONFIRMED ***\n", junctionsSeen);

      if (targetJunction > 0 && junctionsSeen == targetJunction) {
        // This is our target — execute turn
        Serial.printf("Target junction %d! Turning...\n", targetJunction);
        executeTurn();
        return;
      } else if (targetJunction == 0) {
        Serial.println("Mode C: driving straight through");
      } else {
        Serial.printf("Junction %d: not target (%d), going straight\n",
          junctionsSeen, targetJunction);
      }
    }

    // While in junction zone — drive straight
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    return;
  }

  // ── Left junction zone ────────────────────────────────
  if (junctionActive) {
    junctionActive = false;
    Serial.println("Junction zone exited");
  }

  // ── Line lost → search ────────────────────────────────
  if (sum == 0) {
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir   = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST → %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
    else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    return;
  }

  // ── Sharp LEFT: only S1 ───────────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    if (followState != SHARP_LEFT) {
      followState = SHARP_LEFT;
      Serial.println("SHARP L");
    }
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed+rightOffset);
    return;
  }

  // ── Sharp RIGHT: only S5 ─────────────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    if (followState != SHARP_RIGHT) {
      followState = SHARP_RIGHT;
      Serial.println("SHARP R");
    }
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed-rightOffset));
    return;
  }

  // ── Normal PD ─────────────────────────────────────────
  followState = FOLLOWING;
  int error      = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError      = error;

  float correction = (Kp*error) + (Kd*derivative);
  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

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
    case 'A': case 'a':
      targetJunction=1;
      Serial.println(">>> Mode A: Room 1 (turn at junction 1)");
      Serial.println("    Type 'g' to start");
      break;
    case 'B': case 'b':
      targetJunction=2;
      Serial.println(">>> Mode B: Room 2 (turn at junction 2)");
      Serial.println("    Type 'g' to start");
      break;
    case 'C': case 'c':
      targetJunction=0;
      Serial.println(">>> Mode C: Room 3 (straight, no turns)");
      Serial.println("    Type 'g' to start");
      break;
    case 'g':
      lineFollowing=true; followState=FOLLOWING;
      lastError=0; junctionsSeen=0;
      junctionActive=false; junctionCounted=false;
      Serial.println(">>> START"); printSettings();
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
    case 'i': printIR(); break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR (s=stop)"); break;
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

  Serial.println("==========================================");
  Serial.println("  LINE FOLLOW v15 — Fixed Junction");
  Serial.println("==========================================");
  Serial.println("Select room FIRST, then type 'g':");
  Serial.println("  A → Room 1 (turn at junction 1)");
  Serial.println("  B → Room 2 (turn at junction 2)");
  Serial.println("  C → Room 3 (straight, no turns)");
  Serial.println("==========================================");
  printSettings();
}

void loop() {
  while (Serial.available()>0) {
    char cmd=(char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(100); }
  if (lineFollowing) { doLineFollow(); delay(10);  }
}
