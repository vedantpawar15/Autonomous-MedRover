/*
 * ============================================================
 *  LINE FOLLOW v14 — Junction Control via Serial
 *  Based on v13 (working PD + recovery + sharp turns)
 *
 *  JUNCTION COMMANDS (type before starting):
 *   A → Take FIRST junction turn (Room 1)
 *   B → Skip first, take SECOND junction turn (Room 2)
 *   C → Skip all junctions, go straight to end (Room 3)
 *
 *  OTHER COMMANDS:
 *   g → Start line following
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

// ── Tuning ────────────────────────────────────────────────
int   baseSpeed   = 125;
int   rightOffset = 120;
float Kp          = 20.0;
float Kd          = 20.0;
int   turnSpeed   = 150;
int   searchSpeed = 100;

// ── Junction control ──────────────────────────────────────
int targetJunction = 1;   // which junction to turn at (1, 2, or 0=none)
int junctionsSeen  = 0;   // how many junctions passed so far
bool missionDone   = false;

// ── State ─────────────────────────────────────────────────
bool lineFollowing = false;
int  lastError     = 0;
int  searchDir     = 1;

enum FollowState { FOLLOWING, SEARCHING, SHARP_LEFT, SHARP_RIGHT, TURNING_AT_JUNCTION };
FollowState followState = FOLLOWING;

// Junction debounce
bool junctionActive = false;
unsigned long junctionTime = 0;
#define JUNCTION_DEBOUNCE 600  // ms to stay in junction before counting it

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
  junctionsSeen = 0;
  junctionActive = false;
  missionDone = false;
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
  Serial.printf("  baseSpeed    : %d\n",  baseSpeed);
  Serial.printf("  rightOffset  : %d\n",  rightOffset);
  Serial.printf("  Kp           : %.1f\n", Kp);
  Serial.printf("  Kd           : %.1f\n", Kd);
  Serial.printf("  turnSpeed    : %d\n",  turnSpeed);
  Serial.printf("  searchSpeed  : %d\n",  searchSpeed);
  Serial.printf("  targetJunction: %d %s\n", targetJunction,
    targetJunction==0?"(C - no turn, go straight)":
    targetJunction==1?"(A - turn at junction 1)":
                      "(B - turn at junction 2)");
  Serial.println("==========================");
}

void handleJunction(IRSensor ir) {
  // ── Debounce: only count junction after staying on it ──
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  if (sum >= 3) {
    if (!junctionActive) {
      junctionActive = true;
      junctionTime = millis();
    }
    // Not enough time on junction yet — drive straight through
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    return;
  }

  // Just left junction area
  if (junctionActive) {
    junctionActive = false;
    unsigned long timeOnJunction = millis() - junctionTime;

    if (timeOnJunction > JUNCTION_DEBOUNCE) {
      // Valid junction detected
      junctionsSeen++;
      Serial.printf("Junction %d detected!\n", junctionsSeen);

      if (targetJunction == 0) {
        // Mode C: ignore all junctions, go straight
        Serial.println("Mode C: ignoring junction, going straight");

      } else if (junctionsSeen == targetJunction) {
        // This is our target junction — TURN RIGHT
        Serial.printf("Target junction %d reached! Turning right...\n", junctionsSeen);
        followState = TURNING_AT_JUNCTION;

        // Execute turn: spin right until line found on center sensors
        unsigned long turnStart = millis();
        while (millis() - turnStart < 600) {  // spin for 600ms
          setMotorLeft(turnSpeed);
          setMotorRight(-(turnSpeed - rightOffset));
          delay(10);
        }
        // Now follow line again
        followState = FOLLOWING;
        lastError = 0;
        missionDone = true;  // after turning, go straight to end
        Serial.println("Turn complete! Following to destination...");
      } else {
        Serial.printf("Junction %d: not target (%d), going straight\n",
          junctionsSeen, targetJunction);
      }
    }
  }
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── Junction handling ─────────────────────────────────
  if (sum >= 3 || junctionActive) {
    handleJunction(ir);
    return;
  }

  // ── Line lost → search ────────────────────────────────
  if (sum == 0) {
    if (followState != SEARCHING) {
      followState = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST → search %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
    else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    return;
  }

  // ── Sharp LEFT: only S1 ───────────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    if (followState != SHARP_LEFT) {
      followState = SHARP_LEFT;
      Serial.println("SHARP LEFT");
    }
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed + rightOffset);
    return;
  }

  // ── Sharp RIGHT: only S5 ─────────────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    if (followState != SHARP_RIGHT) {
      followState = SHARP_RIGHT;
      Serial.println("SHARP RIGHT");
    }
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed - rightOffset));
    return;
  }

  // ── Normal PD ─────────────────────────────────────────
  followState = FOLLOWING;
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError = error;

  float correction = (Kp * error) + (Kd * derivative);
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

    // ── Junction mode selection ───────────────────────
    case 'A': case 'a':
      targetJunction = 1;
      Serial.println(">>> Mode A: will turn at FIRST junction (Room 1)");
      Serial.println("    Type 'g' to start");
      break;

    case 'B': case 'b':
      targetJunction = 2;
      Serial.println(">>> Mode B: will skip first, turn at SECOND junction (Room 2)");
      Serial.println("    Type 'g' to start");
      break;

    case 'C': case 'c':
      targetJunction = 0;
      Serial.println(">>> Mode C: ignore all junctions, go straight to end (Room 3)");
      Serial.println("    Type 'g' to start");
      break;

    // ── Start/stop ────────────────────────────────────
    case 'g':
      lineFollowing=true; followState=FOLLOWING;
      lastError=0; junctionsSeen=0;
      junctionActive=false; missionDone=false;
      Serial.println(">>> Line following ON");
      printSettings();
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      Serial.printf(">>> FWD L:%d R:%d\n",baseSpeed,baseSpeed+rightOffset);
      break;

    // ── Tuning ────────────────────────────────────────
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

  Serial.println("==========================================");
  Serial.println("  LINE FOLLOW v14 — Junction Control");
  Serial.println("==========================================");
  Serial.println("BEFORE starting, select room:");
  Serial.println("  A → Room 1 (turn at junction 1)");
  Serial.println("  B → Room 2 (turn at junction 2)");
  Serial.println("  C → Room 3 (straight, no turns)");
  Serial.println("Then type 'g' to start!");
  Serial.println("==========================================");
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
