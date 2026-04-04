/*
 * ============================================================
 *  LINE FOLLOW v18 — Stop at Room + Return to Start
 *
 *  WHAT'S NEW vs v17:
 *   - Robot stops 10s at destination (medicine handoff)
 *   - Then does a U-turn and follows line back
 *   - Counts junctions on return (same count as going)
 *   - Stops itself when it reaches home
 *
 *  STATES (in order):
 *   LINE_FOLLOWING  → PD follow toward destination
 *   DRIVING_PAST    → drive forward a bit before turning
 *   AT_ROOM         → stopped for 10 seconds
 *   UTURNING        → spinning 180°
 *   RETURNING       → PD follow back home
 *   ARRIVED_HOME    → stopped, mission complete
 *   SEARCHING       → lost line, scanning
 * ============================================================
 *  COMMANDS:
 *   A → Room 1  |  B → Room 2  |  C → Room 3 (straight)
 *   g → Start   |  s → Stop
 *   + / - → Speed        ] / [ → Kp       > / < → Kd
 *   ) / ( → Turn speed
 *   n / m → forward-past time +/-50ms
 *   p → Settings   i → IR once   w → Watch IR
 * ============================================================
 */

// ── Pin definitions ───────────────────────────────────────────
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

// ── Tunable parameters ────────────────────────────────────────
int   baseSpeed            = 110;
int   rightOffset          = 120;
float Kp                   = 20.0;
float Kd                   = 20.0;
int   turnSpeed            = 140;
int   searchSpeed          = 100;
int   forwardAfterJunction = 250;   // ms to drive past junction before turning
int   roomWaitMs           = 10000; // ms to wait at room (10 seconds)
int   uturnMs              = 700;   // ms to spin for U-turn — tune this!

#define JUNCTION_FRAMES_NEEDED  3
#define JUNCTION_COOLDOWN_MS  800   // prevents re-counting same junction

// ── State machine ─────────────────────────────────────────────
enum RobotState {
  LINE_FOLLOWING,   // going to room
  DRIVING_PAST,     // driving past junction before turning
  AT_ROOM,          // waiting at room
  UTURNING,         // spinning 180
  RETURNING,        // going back home
  ARRIVED_HOME,     // mission complete
  SEARCHING         // lost line
};
RobotState state = LINE_FOLLOWING;

// ── Journey variables ─────────────────────────────────────────
int  targetJunction       = 1;   // which junction to turn at (outbound)
int  junctionsSeen        = 0;   // outbound junction counter
int  returnJunctionsNeeded = 0;  // = targetJunction (how many to count back)
int  returnJunctionsSeen  = 0;   // return junction counter
int  junctionFrames       = 0;

unsigned long lastJunctionTime = 0;
unsigned long drivePastStart   = 0;
unsigned long roomArrivalTime  = 0;
unsigned long uturnStart       = 0;

// ── Other globals ─────────────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

// ── Forward declarations ──────────────────────────────────────
struct IRSensor { bool s1,s2,s3,s4,s5; };
IRSensor readIR();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopMotorsOnly();
void stopAll();
void printSettings();
void printIR();
void doLineFollow();
void executeTurnLeft();
void startUturn();
void processCommand(char cmd);

// ── Motor setup ───────────────────────────────────────────────
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
  lineFollowing         = false;
  watchingIR            = false;
  state                 = LINE_FOLLOWING;
  junctionsSeen         = 0;
  junctionFrames        = 0;
  returnJunctionsSeen   = 0;
  returnJunctionsNeeded = 0;
  lastJunctionTime      = 0;
}

// ── IR ────────────────────────────────────────────────────────
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
  Serial.printf("  roomWaitMs           : %dms\n", roomWaitMs);
  Serial.printf("  uturnMs              : %dms\n", uturnMs);
  Serial.printf("  junctionCooldown     : %dms\n", JUNCTION_COOLDOWN_MS);
  Serial.printf("  targetJunction       : %d %s\n", targetJunction,
    targetJunction==0?"(C-straight)":
    targetJunction==1?"(A-Room1)":"(B-Room2)");
  Serial.println("==========================");
}

// ── Turn left onto branch ─────────────────────────────────────
void executeTurnLeft() {
  Serial.println(">> TURNING LEFT into room...");
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

  // Follow branch line a little to fully enter the room
  unsigned long enterStart = millis();
  while (millis() - enterStart < 400) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    delay(10);
  }

  stopMotorsOnly();

  // Transition → wait at room
  state           = AT_ROOM;
  roomArrivalTime = millis();
  Serial.printf(">> ARRIVED at Room %d — waiting %d seconds...\n",
    junctionsSeen, roomWaitMs/1000);
}

// ── U-turn: spin until center sensor finds line ───────────────
void startUturn() {
  Serial.println(">> U-TURN starting...");
  stopMotorsOnly();
  delay(200);

  state       = UTURNING;
  uturnStart  = millis();
}

void doUturn() {
  // Spin left (same direction as branch turn, now faces back)
  setMotorLeft(-turnSpeed);
  setMotorRight(constrain(turnSpeed+rightOffset,0,255));

  IRSensor ir = readIR();

  // Wait at least uturnMs before accepting line detection
  if (millis() - uturnStart < (unsigned long)uturnMs) return;

  // Line found after minimum spin time
  if (ir.s3 || (ir.s2 && ir.s3) || (ir.s3 && ir.s4)) {
    Serial.println(">> U-turn complete — line found, heading home");
    stopMotorsOnly();
    delay(100);
    lastError             = 0;
    junctionFrames        = 0;
    lastJunctionTime      = millis(); // cooldown so we don't count the branch junction immediately
    returnJunctionsSeen   = 0;
    returnJunctionsNeeded = targetJunction; // e.g. A→1, B→2
    state                 = RETURNING;
    Serial.printf(">> RETURNING — need to pass %d junction(s) to reach home\n",
      returnJunctionsNeeded);
  }

  // Safety: if spin times out (3s), just switch to RETURNING anyway
  if (millis() - uturnStart > 3000) {
    Serial.println(">> U-turn timeout — forcing RETURNING");
    stopMotorsOnly();
    delay(100);
    lastError             = 0;
    lastJunctionTime      = millis();
    returnJunctionsSeen   = 0;
    returnJunctionsNeeded = targetJunction;
    state                 = RETURNING;
  }
}

// ── PD follow helper (used for both outbound and return) ──────
void doPDFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  // Line lost
  if (sum == 0) {
    if (state != SEARCHING) {
      searchDir = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST → searching %s\n", searchDir>0?"RIGHT":"LEFT");
    }
    RobotState prevState = state;
    state = SEARCHING;
    if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
    else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    // remember what state to return to (hack: store in a flag below)
    return;
  }

  // Sharp LEFT
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    setMotorLeft(-turnSpeed);
    setMotorRight(turnSpeed+rightOffset);
    return;
  }

  // Sharp RIGHT
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed-rightOffset));
    return;
  }

  // Normal PD
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
    Serial.printf("err:%2d L:%3d R:%3d [%d%d%d%d%d]\n",
      error,leftSpeed,rightSpeed,
      ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    prevErr = error;
  }
}

// ── Junction detector (shared, with cooldown) ─────────────────
// Returns true if a new junction was just confirmed
bool checkJunction() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  if (sum >= 3) {
    if (millis() - lastJunctionTime < JUNCTION_COOLDOWN_MS) {
      // In cooldown — drive straight
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      return false;
    }

    junctionFrames++;
    if (junctionFrames >= JUNCTION_FRAMES_NEEDED) {
      junctionFrames   = 0;
      lastJunctionTime = millis();
      return true;  // new junction confirmed
    }

    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    return false;
  } else {
    junctionFrames = 0;
    return false;
  }
}

// ── Main loop function ────────────────────────────────────────
void doLineFollow() {

  // ── AT ROOM: waiting ──────────────────────────────────────
  if (state == AT_ROOM) {
    unsigned long elapsed = millis() - roomArrivalTime;
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      Serial.printf("  waiting... %lus / %ds\n", elapsed/1000, roomWaitMs/1000);
      lastPrint = millis();
    }
    if (elapsed >= (unsigned long)roomWaitMs) {
      Serial.println(">> Wait done — starting U-turn");
      startUturn();
    }
    return;
  }

  // ── U-TURNING ─────────────────────────────────────────────
  if (state == UTURNING) {
    doUturn();
    return;
  }

  // ── ARRIVED HOME ──────────────────────────────────────────
  if (state == ARRIVED_HOME) {
    stopMotorsOnly();
    return;
  }

  // ── DRIVING PAST junction (outbound) ──────────────────────
  if (state == DRIVING_PAST) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    if (millis() - drivePastStart >= (unsigned long)forwardAfterJunction) {
      Serial.println(">> Past junction — turning now");
      executeTurnLeft();
    }
    return;
  }

  // ── SEARCHING ─────────────────────────────────────────────
  if (state == SEARCHING) {
    IRSensor ir = readIR();
    int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
    if (sum > 0) {
      Serial.println("Line found — resuming");
      // Restore correct state based on journey phase
      state = (returnJunctionsNeeded > 0) ? RETURNING : LINE_FOLLOWING;
    } else {
      if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
      else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    }
    return;
  }

  // ── OUTBOUND: LINE_FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {
    if (targetJunction == 0) {
      // Mode C: just follow, no turns
      doPDFollow();
      return;
    }

    if (checkJunction()) {
      junctionsSeen++;
      Serial.printf("*** OUTBOUND JUNCTION %d CONFIRMED ***\n", junctionsSeen);

      if (junctionsSeen == targetJunction) {
        Serial.printf(">> Target junction! Driving past %dms then turning\n",
          forwardAfterJunction);
        state          = DRIVING_PAST;
        drivePastStart = millis();
      } else {
        Serial.printf(">> Junction %d: not target — going straight\n", junctionsSeen);
      }
      return;
    }

    doPDFollow();
    return;
  }

  // ── RETURNING: follow line back, count junctions ──────────
  if (state == RETURNING) {
    if (checkJunction()) {
      returnJunctionsSeen++;
      Serial.printf("*** RETURN JUNCTION %d / %d ***\n",
        returnJunctionsSeen, returnJunctionsNeeded);

      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        // Drive a little past the home junction then stop
        Serial.println(">> HOME junction reached — stopping soon");
        unsigned long homeCoast = millis();
        while (millis() - homeCoast < 300) {
          setMotorLeft(baseSpeed);
          setMotorRight(constrain(baseSpeed+rightOffset,0,255));
          delay(10);
        }
        stopMotorsOnly();
        state         = ARRIVED_HOME;
        lineFollowing = false;
        Serial.println("=============================");
        Serial.println("  MISSION COMPLETE — HOME    ");
        Serial.println("=============================");
      }
      return;
    }

    doPDFollow();
    return;
  }
}

// ── Command handler ───────────────────────────────────────────
void processCommand(char cmd) {
  switch(cmd) {
    case 'A': case 'a':
      targetJunction=1;
      Serial.println(">>> Mode A: Room 1 — type 'g' to start");
      break;
    case 'B': case 'b':
      targetJunction=2;
      Serial.println(">>> Mode B: Room 2 — type 'g' to start");
      break;
    case 'C': case 'c':
      targetJunction=0;
      Serial.println(">>> Mode C: Room 3 (straight) — type 'g' to start");
      break;
    case 'g':
      lineFollowing         = true;
      state                 = LINE_FOLLOWING;
      lastError             = 0;
      junctionsSeen         = 0;
      junctionFrames        = 0;
      returnJunctionsSeen   = 0;
      returnJunctionsNeeded = 0;
      lastJunctionTime      = 0;
      Serial.println(">>> START");
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
      break;
    case '+': baseSpeed=min(baseSpeed+5,200);  Serial.printf("Speed:%d\n",baseSpeed);  break;
    case '-': baseSpeed=max(baseSpeed-5,80);   Serial.printf("Speed:%d\n",baseSpeed);  break;
    case ']': Kp=min(Kp+0.5f,80.f);           Serial.printf("Kp:%.1f\n",Kp);          break;
    case '[': Kp=max(Kp-0.5f,1.f);            Serial.printf("Kp:%.1f\n",Kp);          break;
    case '>': Kd=min(Kd+0.5f,80.f);           Serial.printf("Kd:%.1f\n",Kd);          break;
    case '<': Kd=max(Kd-0.5f,0.f);            Serial.printf("Kd:%.1f\n",Kd);          break;
    case ')': turnSpeed=min(turnSpeed+5,220);  Serial.printf("Turn:%d\n",turnSpeed);   break;
    case '(': turnSpeed=max(turnSpeed-5,80);   Serial.printf("Turn:%d\n",turnSpeed);   break;
    case 'n':
      forwardAfterJunction=min(forwardAfterJunction+50,1000);
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction);
      break;
    case 'm':
      forwardAfterJunction=max(forwardAfterJunction-50,0);
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction);
      break;
    case 'i': printIR();                                              break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR"); break;
    case 'p': printSettings();                                        break;
    case '\n': case '\r': case ' ':                                   break;
    default: Serial.printf("? '%c'\n",cmd);
  }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();
  pinMode(IR_S1,INPUT); pinMode(IR_S2,INPUT);
  pinMode(IR_S3,INPUT); pinMode(IR_S4,INPUT);
  pinMode(IR_S5,INPUT);

  Serial.println("==============================================");
  Serial.println("  LINE FOLLOW v18 — Room Stop + Return Home  ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=Room3  then 'g'");
  Serial.println("  Robot stops 10s at room then returns home");
  Serial.println("==============================================");
  printSettings();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(100); }
  if (lineFollowing) { doLineFollow(); delay(10);  }
}
