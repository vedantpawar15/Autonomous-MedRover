/*
 * ============================================================
 *  LINE FOLLOW v16 — Simplified Junction Logic
 *  Approach: detect junction → drive forward a bit → turn left
 *  No complex debounce — simple and reliable
 * ============================================================
 *  COMMANDS:
 *   A → Room 1 (turn at junction 1)
 *   B → Room 2 (turn at junction 2)
 *   C → Room 3 (straight, no turns)
 *   g → Start
 *   s → Stop
 *   + / - → Speed
 *   ] / [ → Kp
 *   > / < → Kd
 *   ) / ( → Turn speed
 *   n → forward-past time +50ms (turn later)
 *   m → forward-past time -50ms (turn earlier)
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

int   baseSpeed   = 110;
int   rightOffset = 120;
float Kp          = 20.0;
float Kd          = 20.0;
int   turnSpeed   = 140;
int   searchSpeed = 100;
int   forwardAfterJunction = 250; // ms to drive past junction before turning

int  targetJunction = 1;
int  junctionsSeen  = 0;
int  junctionFrames = 0;
#define JUNCTION_FRAMES_NEEDED 3

unsigned long drivePastStart = 0;

bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

enum RobotState {
  LINE_FOLLOWING,
  DRIVING_PAST,
  SEARCHING
};
RobotState state = LINE_FOLLOWING;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopAll();
void stopMotorsOnly();
void printSettings();
void printIR();
void doLineFollow();
void executeTurnLeft();
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
  lineFollowing  = false;
  watchingIR     = false;
  state          = LINE_FOLLOWING;
  junctionsSeen  = 0;
  junctionFrames = 0;
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
  int sum=(int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
  Serial.printf("[%d%d%d%d%d] sum=%d\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,sum);
}

void printSettings() {
  Serial.println("======== SETTINGS ========");
  Serial.printf("  baseSpeed            : %d\n",  baseSpeed);
  Serial.printf("  rightOffset          : %d\n",  rightOffset);
  Serial.printf("  Kp                   : %.1f\n", Kp);
  Serial.printf("  Kd                   : %.1f\n", Kd);
  Serial.printf("  turnSpeed            : %d\n",  turnSpeed);
  Serial.printf("  forwardAfterJunction : %dms\n", forwardAfterJunction);
  Serial.printf("  targetJunction       : %d %s\n", targetJunction,
    targetJunction==0?"(C-straight)":
    targetJunction==1?"(A-Room1)":"(B-Room2)");
  Serial.println("==========================");
}

void executeTurnLeft() {
  Serial.println(">> TURNING LEFT...");
  stopMotorsOnly();
  delay(100);

  unsigned long spinStart = millis();
  while (millis() - spinStart < 2000) {
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed+rightOffset,0,255));
    delay(10);
    IRSensor ir = readIR();
    if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
      Serial.println(">> Branch line found!");
      break;
    }
  }

  stopMotorsOnly();
  delay(100);
  lastError = 0;
  state     = LINE_FOLLOWING;
  Serial.println(">> Resuming PD follow");
}

void doLineFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // ── DRIVING PAST junction ─────────────────────────────
  if (state == DRIVING_PAST) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    if (millis() - drivePastStart >= (unsigned long)forwardAfterJunction) {
      Serial.println(">> Past junction — turning now");
      executeTurnLeft();
    }
    return;
  }

  // ── Junction detection ────────────────────────────────
  if (state == LINE_FOLLOWING) {
    if (sum >= 3) {
      junctionFrames++;
      Serial.printf("  jFrame:%d [%d%d%d%d%d]\n",
        junctionFrames,ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);

      if (junctionFrames >= JUNCTION_FRAMES_NEEDED) {
        junctionsSeen++;
        junctionFrames = 0;
        Serial.printf("*** JUNCTION %d CONFIRMED ***\n", junctionsSeen);

        if (targetJunction > 0 && junctionsSeen == targetJunction) {
          Serial.printf("Target! Driving past %dms then turning\n",
            forwardAfterJunction);
          state          = DRIVING_PAST;
          drivePastStart = millis();
        } else {
          Serial.printf("Junction %d: not target, straight\n", junctionsSeen);
        }
      }
      // Drive straight while in junction
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      return;
    } else {
      if (junctionFrames > 0) {
        Serial.printf("  junction frames reset (was %d)\n", junctionFrames);
      }
      junctionFrames = 0;
    }
  }

  // ── Line lost ─────────────────────────────────────────
  if (sum == 0) {
    if (state != SEARCHING) {
      state     = SEARCHING;
      searchDir = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST → %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
    else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    return;
  }

  if (state == SEARCHING) {
    state = LINE_FOLLOWING;
    Serial.println("Found!");
  }

  // ── Sharp LEFT ────────────────────────────────────────
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed+rightOffset);
    return;
  }

  // ── Sharp RIGHT ───────────────────────────────────────
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed-rightOffset));
    return;
  }

  // ── Normal PD ─────────────────────────────────────────
  state = LINE_FOLLOWING;
  int error      = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError      = error;

  float correction = (Kp*error)+(Kd*derivative);
  int leftSpeed  = baseSpeed              - (int)correction;
  int rightSpeed = baseSpeed + rightOffset + (int)correction;

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  static int prevErr = 99;
  if (error != prevErr) {
    Serial.printf("err:%2d L:%4d R:%4d [%d%d%d%d%d]\n",
      error,leftSpeed,rightSpeed,
      ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    prevErr = error;
  }
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'A': case 'a':
      targetJunction=1;
      Serial.println(">>> Mode A: Room 1 — type 'g'");
      break;
    case 'B': case 'b':
      targetJunction=2;
      Serial.println(">>> Mode B: Room 2 — type 'g'");
      break;
    case 'C': case 'c':
      targetJunction=0;
      Serial.println(">>> Mode C: Room 3 — type 'g'");
      break;
    case 'g':
      lineFollowing=true; state=LINE_FOLLOWING;
      lastError=0; junctionsSeen=0; junctionFrames=0;
      Serial.println(">>> START"); printSettings();
      break;
    case 's': stopAll(); Serial.println(">>> STOPPED"); break;
    case 'f':
      stopAll();
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      break;
    case '+': baseSpeed=min(baseSpeed+5,200); Serial.printf("Speed:%d\n",baseSpeed); break;
    case '-': baseSpeed=max(baseSpeed-5,80);  Serial.printf("Speed:%d\n",baseSpeed); break;
    case ']': Kp=min(Kp+0.5f,80.f); Serial.printf("Kp:%.1f\n",Kp); break;
    case '[': Kp=max(Kp-0.5f,1.f);  Serial.printf("Kp:%.1f\n",Kp); break;
    case '>': Kd=min(Kd+0.5f,80.f); Serial.printf("Kd:%.1f\n",Kd); break;
    case '<': Kd=max(Kd-0.5f,0.f);  Serial.printf("Kd:%.1f\n",Kd); break;
    case ')': turnSpeed=min(turnSpeed+5,220); Serial.printf("Turn:%d\n",turnSpeed); break;
    case '(': turnSpeed=max(turnSpeed-5,80);  Serial.printf("Turn:%d\n",turnSpeed); break;
    case 'n':
      forwardAfterJunction=min(forwardAfterJunction+50,1000);
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction);
      break;
    case 'm':
      forwardAfterJunction=max(forwardAfterJunction-50,0);
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction);
      break;
    case 'i': printIR(); break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR"); break;
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
  Serial.println("  LINE FOLLOW v16 — Clean Junction Logic");
  Serial.println("==========================================");
  Serial.println("  A=Room1  B=Room2  C=Room3  then 'g'");
  Serial.println("  n=turn later  m=turn earlier");
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
