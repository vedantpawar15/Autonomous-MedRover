/*
 * ============================================================
 *  LINE FOLLOW v20 — Exact Hospital Rover Behaviour
 *
 *  TRACK LAYOUT:
 *   Robot starts at TOP, moves DOWN vertical line
 *   Junction A = 1st horizontal branch (turns LEFT)
 *   Junction B = 2nd horizontal branch (turns LEFT)
 *   C          = end of vertical line
 *
 *  FULL JOURNEY (A or B):
 *   1. Follow vertical line downward
 *   2. Detect junction → stop → wait 2s (settle)
 *   3. Turn LEFT onto branch
 *   4. Follow branch until line ends (room wall)
 *   5. Stop → wait 8s (medicine handoff)
 *   6. U-turn 180°
 *   7. Follow branch back to main vertical line
 *   8. Detect main-line junction → turn RIGHT back onto main
 *   9. Follow vertical line UPWARD
 *  10. Detect start junction (same count) → stop completely
 *
 *  STATES:
 *   LINE_FOLLOWING   outbound PD follow on main line
 *   JUNCTION_PAUSE   stopped at junction, waiting 2s
 *   TURNING_TO_ROOM  spinning left onto branch
 *   ON_BRANCH        PD follow on branch toward room
 *   AT_ROOM          stopped, waiting 8s
 *   UTURNING         spinning 180°
 *   BRANCH_RETURN    PD follow back on branch toward main line
 *   REJOINING_MAIN   spinning right to rejoin vertical line
 *   RETURNING        PD follow upward on main line
 *   ARRIVED_HOME     stopped, mission complete
 *   SEARCHING        lost line, scanning
 * ============================================================
 *  COMMANDS:
 *   A/B/C → select room, then 'g' to start, 's' to stop
 *   + / - → base speed      ] / [ → Kp      > / < → Kd
 *   ) / ( → turn speed
 *   n / m → junction-pass time +/- 50ms
 *   p → settings    i → IR snap    w → watch IR
 * ============================================================
 */

// ── Pins ─────────────────────────────────────────────────────
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

// ── Tunable params ────────────────────────────────────────────
int   baseSpeed            = 110;
int   rightOffset          = 120;   // right motor trim (hardware offset)
float Kp                   = 20.0;
float Kd                   = 20.0;
int   turnSpeed            = 140;
int   searchSpeed          = 100;
int   forwardAfterJunction = 250;   // ms to nudge past junction centre before turning

// Fixed timing
#define JUNCTION_PAUSE_MS     2000  // wait at junction before turning
#define ROOM_WAIT_MS          8000  // wait at room end
#define UTURN_MIN_MS           700  // min spin time before accepting line as U-turn done
#define LINE_END_CONFIRM_MS    300  // all-dark for this long = room wall confirmed
#define JUNCTION_FRAMES_NEEDED   3  // consecutive frames to confirm junction
#define JUNCTION_COOLDOWN_MS   900  // prevent re-counting same junction

// ── State machine ─────────────────────────────────────────────
enum RobotState {
  LINE_FOLLOWING,
  JUNCTION_PAUSE,
  TURNING_TO_ROOM,
  ON_BRANCH,
  AT_ROOM,
  UTURNING,
  BRANCH_RETURN,
  REJOINING_MAIN,
  RETURNING,
  ARRIVED_HOME,
  SEARCHING
};
RobotState state             = LINE_FOLLOWING;
RobotState stateAfterSearch  = LINE_FOLLOWING;

// ── Journey variables ─────────────────────────────────────────
int  targetJunction          = 1;
int  junctionsSeen           = 0;   // outbound count
int  returnJunctionsNeeded   = 0;   // = targetJunction
int  returnJunctionsSeen     = 0;
int  junctionFrames          = 0;

unsigned long lastJunctionTime   = 0;
unsigned long junctionPauseStart = 0;
unsigned long roomArrivalTime    = 0;
unsigned long uturnStart         = 0;
unsigned long lineEndStart       = 0;
bool          lineEndActive      = false;

// ── Misc globals ──────────────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

// ── IR sensor struct ──────────────────────────────────────────
struct IRSensor { bool s1,s2,s3,s4,s5; };

// ── Forward declarations ──────────────────────────────────────
IRSensor readIR();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopMotorsOnly();
void stopAll();
void printSettings();
void printIR();
void doLineFollow();
void processCommand(char cmd);

// ─────────────────────────────────────────────────────────────
//  MOTOR CONTROL
// ─────────────────────────────────────────────────────────────
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
  lineFollowing        = false;
  watchingIR           = false;
  state                = LINE_FOLLOWING;
  stateAfterSearch     = LINE_FOLLOWING;
  junctionsSeen        = 0;
  junctionFrames       = 0;
  returnJunctionsSeen  = 0;
  returnJunctionsNeeded= 0;
  lastJunctionTime     = 0;
  lineEndActive        = false;
}

// ─────────────────────────────────────────────────────────────
//  IR
// ─────────────────────────────────────────────────────────────
IRSensor readIR() {
  IRSensor d;
  d.s1=!digitalRead(IR_S1); d.s2=!digitalRead(IR_S2);
  d.s3=!digitalRead(IR_S3); d.s4=!digitalRead(IR_S4);
  d.s5=!digitalRead(IR_S5);
  return d;
}

int irSum(IRSensor &ir) {
  return (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("[%d%d%d%d%d] sum=%d\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,irSum(ir));
}

void printSettings() {
  Serial.println("======== SETTINGS v20 ========");
  Serial.printf("  baseSpeed            : %d\n",  baseSpeed);
  Serial.printf("  rightOffset          : %d\n",  rightOffset);
  Serial.printf("  Kp / Kd              : %.1f / %.1f\n", Kp, Kd);
  Serial.printf("  turnSpeed            : %d\n",  turnSpeed);
  Serial.printf("  forwardAfterJunction : %dms\n", forwardAfterJunction);
  Serial.printf("  junctionPause        : %dms\n", JUNCTION_PAUSE_MS);
  Serial.printf("  roomWait             : %dms\n", ROOM_WAIT_MS);
  Serial.printf("  uturnMinMs           : %dms\n", UTURN_MIN_MS);
  Serial.printf("  targetJunction       : %d (%s)\n", targetJunction,
    targetJunction==0?"C-straight":targetJunction==1?"A-Room1":"B-Room2");
  Serial.println("==============================");
}

// ─────────────────────────────────────────────────────────────
//  PD FOLLOW  (call only when sum > 0)
// ─────────────────────────────────────────────────────────────
void doPD() {
  IRSensor ir = readIR();

  // Sharp left
  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed+rightOffset,0,255));
    return;
  }
  // Sharp right
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed-rightOffset));
    return;
  }

  int error      = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError      = error;
  float corr     = Kp*error + Kd*derivative;

  int L = constrain((int)(baseSpeed - corr),          0, 255);
  int R = constrain((int)(baseSpeed + rightOffset + corr), 0, 255);
  setMotorLeft(L);
  setMotorRight(R);

  static int pe = 99;
  if (error != pe) {
    Serial.printf("err:%2d L:%3d R:%3d [%d%d%d%d%d]\n",
      error,L,R,ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
    pe = error;
  }
}

// ─────────────────────────────────────────────────────────────
//  JUNCTION CHECK  (call in LINE_FOLLOWING / RETURNING)
//  Returns true the moment a new junction is confirmed
// ─────────────────────────────────────────────────────────────
bool checkJunction() {
  IRSensor ir = readIR();
  int sum = irSum(ir);

  if (sum >= 3) {
    if (millis() - lastJunctionTime < JUNCTION_COOLDOWN_MS) {
      // In cooldown — just drive straight
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      return false;
    }
    junctionFrames++;
    if (junctionFrames >= JUNCTION_FRAMES_NEEDED) {
      junctionFrames   = 0;
      lastJunctionTime = millis();
      return true;
    }
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    return false;
  }
  junctionFrames = 0;
  return false;
}

// ─────────────────────────────────────────────────────────────
//  SPIN LEFT until centre sensor sees line
//  used for: initial branch turn AND U-turn
// ─────────────────────────────────────────────────────────────
bool spinLeft(unsigned long minMs, unsigned long &startTime, bool &started) {
  if (!started) {
    stopMotorsOnly();
    delay(150);
    startTime = millis();
    started   = true;
  }
  setMotorLeft(-turnSpeed);
  setMotorRight(constrain(turnSpeed+rightOffset,0,255));

  if (millis() - startTime < minMs) return false; // still spinning minimum

  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly();
    delay(100);
    started = false;
    return true; // done
  }
  if (millis() - startTime > 3000) {
    // safety timeout
    stopMotorsOnly();
    delay(100);
    started = false;
    return true;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
//  SPIN RIGHT until centre sensor sees line
//  used for: rejoining main vertical line after branch return
// ─────────────────────────────────────────────────────────────
bool spinRight(unsigned long minMs, unsigned long &startTime, bool &started) {
  if (!started) {
    stopMotorsOnly();
    delay(150);
    startTime = millis();
    started   = true;
  }
  setMotorLeft(constrain(turnSpeed+rightOffset,0,255));
  setMotorRight(-turnSpeed);

  if (millis() - startTime < minMs) return false;

  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly();
    delay(100);
    started = false;
    return true;
  }
  if (millis() - startTime > 3000) {
    stopMotorsOnly();
    delay(100);
    started = false;
    return true;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
//  MAIN LOOP FUNCTION
// ─────────────────────────────────────────────────────────────
void doLineFollow() {

  // ── static spin state vars (persistent across calls) ──────
  static unsigned long spinStart   = 0;
  static bool          spinStarted = false;

  IRSensor ir  = readIR();
  int      sum = irSum(ir);

  // ── ARRIVED HOME ──────────────────────────────────────────
  if (state == ARRIVED_HOME) {
    stopMotorsOnly();
    return;
  }

  // ── SEARCHING ─────────────────────────────────────────────
  if (state == SEARCHING) {
    if (sum > 0) {
      Serial.println(">> Line found — resuming");
      state = stateAfterSearch;
    } else {
      if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
      else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    }
    return;
  }

  // ── JUNCTION PAUSE: stopped 2s before turning ─────────────
  if (state == JUNCTION_PAUSE) {
    unsigned long elapsed = millis() - junctionPauseStart;
    if (elapsed < JUNCTION_PAUSE_MS) {
      // Show countdown every 500ms
      static unsigned long lastPauseMsg = 0;
      if (millis() - lastPauseMsg > 500) {
        Serial.printf("  junction pause... %lums / %dms\n",
          elapsed, JUNCTION_PAUSE_MS);
        lastPauseMsg = millis();
      }
      return; // stay stopped
    }
    // Pause done → nudge forward past junction centre then turn
    Serial.println(">> Pause done — nudging past junction...");
    unsigned long nudgeStart = millis();
    while (millis() - nudgeStart < (unsigned long)forwardAfterJunction) {
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed+rightOffset,0,255));
      delay(10);
    }
    stopMotorsOnly();
    delay(100);
    spinStarted = false;
    state       = TURNING_TO_ROOM;
    Serial.println(">> Turning LEFT to room...");
    return;
  }

  // ── TURNING TO ROOM (spin left until branch found) ────────
  if (state == TURNING_TO_ROOM) {
    if (spinLeft((unsigned long)turnSpeed > 0 ? 400 : 400, spinStart, spinStarted)) {
      lastError     = 0;
      lineEndActive = false;
      junctionFrames = 0;
      lastJunctionTime = millis(); // cooldown so branch junction isn't counted
      state         = ON_BRANCH;
      Serial.println(">> ON_BRANCH — following to room end");
    }
    return;
  }

  // ── ON BRANCH: PD follow until line ends ──────────────────
  if (state == ON_BRANCH) {
    if (sum == 0) {
      if (!lineEndActive) {
        lineEndActive = true;
        lineEndStart  = millis();
        Serial.println("  branch end? confirming...");
      }
      // Coast slowly while confirming
      setMotorLeft(baseSpeed - 40);
      setMotorRight(constrain((baseSpeed-40)+rightOffset,0,255));

      if (millis() - lineEndStart >= LINE_END_CONFIRM_MS) {
        stopMotorsOnly();
        state           = AT_ROOM;
        roomArrivalTime = millis();
        lineEndActive   = false;
        Serial.printf(">> ROOM REACHED — waiting %ds\n", ROOM_WAIT_MS/1000);
      }
    } else {
      if (lineEndActive) { lineEndActive = false; Serial.println("  false end — line back"); }
      doPD();
    }
    return;
  }

  // ── AT ROOM: wait 8s ──────────────────────────────────────
  if (state == AT_ROOM) {
    unsigned long elapsed = millis() - roomArrivalTime;
    static unsigned long lastRoomMsg = 0;
    if (millis() - lastRoomMsg > 1000) {
      Serial.printf("  room wait... %lus / %ds\n",
        elapsed/1000, ROOM_WAIT_MS/1000);
      lastRoomMsg = millis();
    }
    if (elapsed >= ROOM_WAIT_MS) {
      Serial.println(">> Room wait done — U-turning");
      spinStarted = false;
      state       = UTURNING;
    }
    return;
  }

  // ── U-TURN: spin left 180° ────────────────────────────────
  if (state == UTURNING) {
    if (spinLeft(UTURN_MIN_MS, spinStart, spinStarted)) {
      lastError        = 0;
      junctionFrames   = 0;
      lineEndActive    = false;
      lastJunctionTime = millis(); // cooldown
      state            = BRANCH_RETURN;
      Serial.println(">> BRANCH_RETURN — heading back to main line");
    }
    return;
  }

  // ── BRANCH RETURN: follow branch back toward main line ────
  if (state == BRANCH_RETURN) {
    if (sum == 0) {
      // Lost on branch return → search
      stateAfterSearch = BRANCH_RETURN;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }

    // Junction detected = we've reached the main vertical line
    if (checkJunction()) {
      Serial.println(">> Main line junction found — rejoining vertical line");
      spinStarted = false;
      state       = REJOINING_MAIN;
      return;
    }

    doPD();
    return;
  }

  // ── REJOIN MAIN: turn right to face upward ────────────────
  if (state == REJOINING_MAIN) {
    if (spinRight(400, spinStart, spinStarted)) {
      lastError             = 0;
      junctionFrames        = 0;
      lastJunctionTime      = millis(); // cooldown
      returnJunctionsSeen   = 0;
      returnJunctionsNeeded = targetJunction; // how many junctions to pass going up
      state                 = RETURNING;
      Serial.printf(">> RETURNING upward — need %d junction(s) to reach home\n",
        returnJunctionsNeeded);
    }
    return;
  }

  // ── RETURNING: PD follow up main line, count junctions ───
  if (state == RETURNING) {
    if (sum == 0) {
      stateAfterSearch = RETURNING;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      Serial.println("LOST on return — searching");
      return;
    }

    if (checkJunction()) {
      returnJunctionsSeen++;
      Serial.printf("*** RETURN JUNCTION %d / %d ***\n",
        returnJunctionsSeen, returnJunctionsNeeded);

      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        // Coast a little past start marker then halt
        unsigned long coastStart = millis();
        while (millis() - coastStart < 300) {
          setMotorLeft(baseSpeed);
          setMotorRight(constrain(baseSpeed+rightOffset,0,255));
          delay(10);
        }
        stopMotorsOnly();
        state         = ARRIVED_HOME;
        lineFollowing = false;
        Serial.println("================================");
        Serial.println("   MISSION COMPLETE — HOME      ");
        Serial.println("================================");
        return;
      }
    }

    doPD();
    return;
  }

  // ── OUTBOUND LINE FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {

    // Mode C: straight to end of line
    if (targetJunction == 0) {
      if (sum == 0) {
        stopMotorsOnly();
        state = ARRIVED_HOME;
        lineFollowing = false;
        Serial.println(">> END OF LINE — arrived at C");
        return;
      }
      doPD();
      return;
    }

    // Line lost on main
    if (sum == 0) {
      stateAfterSearch = LINE_FOLLOWING;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      Serial.println("LOST on main — searching");
      return;
    }

    if (checkJunction()) {
      junctionsSeen++;
      Serial.printf("*** OUTBOUND JUNCTION %d ***\n", junctionsSeen);

      if (junctionsSeen == targetJunction) {
        Serial.printf(">> TARGET junction! Pausing %dms...\n", JUNCTION_PAUSE_MS);
        stopMotorsOnly();
        state              = JUNCTION_PAUSE;
        junctionPauseStart = millis();
      } else {
        Serial.printf(">> Junction %d: not target — straight\n", junctionsSeen);
      }
      return;
    }

    doPD();
  }
}

// ─────────────────────────────────────────────────────────────
//  COMMAND HANDLER
// ─────────────────────────────────────────────────────────────
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
      Serial.println(">>> Mode C: straight to end — type 'g' to start");
      break;
    case 'g':
      lineFollowing        = true;
      state                = LINE_FOLLOWING;
      stateAfterSearch     = LINE_FOLLOWING;
      lastError            = 0;
      junctionsSeen        = 0;
      junctionFrames       = 0;
      returnJunctionsSeen  = 0;
      returnJunctionsNeeded= 0;
      lastJunctionTime     = 0;
      lineEndActive        = false;
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
    case 'i': printIR();                                               break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR"); break;
    case 'p': printSettings();                                         break;
    case '\n': case '\r': case ' ':                                    break;
    default: Serial.printf("? '%c'\n",cmd);
  }
}

// ─────────────────────────────────────────────────────────────
//  SETUP & LOOP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();
  pinMode(IR_S1,INPUT); pinMode(IR_S2,INPUT);
  pinMode(IR_S3,INPUT); pinMode(IR_S4,INPUT);
  pinMode(IR_S5,INPUT);

  Serial.println("==============================================");
  Serial.println("  LINE FOLLOW v20 — Hospital Rover Final     ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=Room3(end)  then 'g'  ");
  Serial.println("  Flow: detect jn → pause 2s → left turn    ");
  Serial.println("        → follow to end → wait 8s → U-turn  ");
  Serial.println("        → retrace → rejoin → return home     ");
  Serial.println("==============================================");
  printSettings();
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(100); }
  if (lineFollowing) { doLineFollow(); delay(10);  }
}
