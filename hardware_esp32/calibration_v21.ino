/*
 * ============================================================
 *  LINE FOLLOW v21 — Fixes for branch end + home stop
 *
 *  FIXES vs v20:
 *   FIX 1 — Branch end false trigger:
 *            Increased LINE_END_CONFIRM_MS from 300 → 600ms
 *            Robot now stops completely (not coasting) while
 *            confirming end, so no drift off line
 *            Also requires ALL 5 sensors dark (not just sum==0
 *            which could be a momentary bump)
 *
 *   FIX 2 — Doesn't stop at home:
 *            Removed unreliable "coast 300ms past junction"
 *            Instead: stop immediately when home junction seen,
 *            set lineFollowing=false FIRST, then stopMotorsOnly
 *            Loop() checks lineFollowing before calling doLineFollow
 *            so it can never re-enter after mission complete
 *
 *  TRACK LAYOUT:
 *   Robot starts at TOP, moves DOWN vertical line
 *   Junction A = 1st horizontal branch (turns LEFT)
 *   Junction B = 2nd horizontal branch (turns LEFT)
 *   C          = end of vertical line
 *
 *  FULL JOURNEY (A or B):
 *   1. Follow vertical line downward (PD)
 *   2. Detect junction → stop → wait 2s
 *   3. Nudge past centre → spin LEFT onto branch
 *   4. Follow branch (PD) until ALL 5 sensors dark for 600ms
 *   5. Stop → wait 8s
 *   6. Spin LEFT 180° (U-turn)
 *   7. Follow branch back (PD) to main line junction
 *   8. Spin RIGHT to face upward on main line
 *   9. Follow main line upward (PD), count junctions
 *  10. After N junctions seen → HARD STOP, mission complete
 * ============================================================
 *  COMMANDS:
 *   A/B/C → select room, then 'g' to go, 's' to stop
 *   + / - → speed    ] / [ → Kp    > / < → Kd    ) / ( → turn
 *   n / m → nudge time +/- 50ms
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
int   rightOffset          = 120;
float Kp                   = 20.0;
float Kd                   = 20.0;
int   turnSpeed            = 140;
int   searchSpeed          = 100;
int   forwardAfterJunction = 250;  // ms nudge past junction before turning

// Timing constants
#define JUNCTION_PAUSE_MS      2000  // pause at junction before turning
#define ROOM_WAIT_MS           8000  // wait at room end
#define UTURN_MIN_MS            800  // min spin ms for U-turn
#define REJOIN_SPIN_MS          500  // min spin ms for right-turn rejoin
#define BRANCH_TURN_MIN_MS      400  // min spin ms for initial left turn onto branch

// FIX 1: longer confirm time + robot STOPS while confirming
#define LINE_END_CONFIRM_MS     600  // all-5-dark for this long = room confirmed

#define JUNCTION_FRAMES_NEEDED    3
#define JUNCTION_COOLDOWN_MS    900

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
  ARRIVED_HOME,    // FIX 2: terminal state, never exits
  SEARCHING
};
RobotState state            = LINE_FOLLOWING;
RobotState stateAfterSearch = LINE_FOLLOWING;

// ── Journey vars ──────────────────────────────────────────────
int  targetJunction         = 1;
int  junctionsSeen          = 0;
int  returnJunctionsNeeded  = 0;
int  returnJunctionsSeen    = 0;
int  junctionFrames         = 0;

unsigned long lastJunctionTime   = 0;
unsigned long junctionPauseStart = 0;
unsigned long roomArrivalTime    = 0;
unsigned long lineEndStart       = 0;
bool          lineEndActive      = false;

// Spin helper state (persistent across loop calls)
unsigned long spinStart   = 0;
bool          spinStarted = false;

// ── Misc ──────────────────────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

struct IRSensor { bool s1,s2,s3,s4,s5; };

// ── Forward declarations ──────────────────────────────────────
IRSensor readIR();
int  irSum(IRSensor &ir);
bool allDark(IRSensor &ir);
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopMotorsOnly();
void stopAll();
void missionComplete();
void printSettings();
void printIR();
void doPD();
bool checkJunction();
bool spinLeft(unsigned long minMs);
bool spinRight(unsigned long minMs);
void doLineFollow();
void processCommand(char cmd);

// ─────────────────────────────────────────────────────────────
//  MOTORS
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
  lineFollowing         = false;
  watchingIR            = false;
  state                 = LINE_FOLLOWING;
  stateAfterSearch      = LINE_FOLLOWING;
  junctionsSeen         = 0;
  junctionFrames        = 0;
  returnJunctionsSeen   = 0;
  returnJunctionsNeeded = 0;
  lastJunctionTime      = 0;
  lineEndActive         = false;
  spinStarted           = false;
}

// FIX 2: hard stop that can never be undone by loop()
void missionComplete() {
  lineFollowing = false;          // checked FIRST in loop() — blocks re-entry
  stopMotorsOnly();
  state = ARRIVED_HOME;
  Serial.println("================================");
  Serial.println("   MISSION COMPLETE — HOME      ");
  Serial.println("================================");
}

// ─────────────────────────────────────────────────────────────
//  IR HELPERS
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

// FIX 1: require all 5 individually dark, not just sum==0
bool allDark(IRSensor &ir) {
  return (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5);
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("[%d%d%d%d%d] sum=%d\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,irSum(ir));
}

void printSettings() {
  Serial.println("======== SETTINGS v21 ========");
  Serial.printf("  baseSpeed            : %d\n",  baseSpeed);
  Serial.printf("  rightOffset          : %d\n",  rightOffset);
  Serial.printf("  Kp / Kd              : %.1f / %.1f\n", Kp, Kd);
  Serial.printf("  turnSpeed            : %d\n",  turnSpeed);
  Serial.printf("  forwardAfterJunction : %dms\n", forwardAfterJunction);
  Serial.printf("  junctionPause        : %dms\n", JUNCTION_PAUSE_MS);
  Serial.printf("  roomWait             : %dms\n", ROOM_WAIT_MS);
  Serial.printf("  uturnMinMs           : %dms\n", UTURN_MIN_MS);
  Serial.printf("  lineEndConfirm       : %dms\n", LINE_END_CONFIRM_MS);
  Serial.printf("  junctionCooldown     : %dms\n", JUNCTION_COOLDOWN_MS);
  Serial.printf("  targetJunction       : %d (%s)\n", targetJunction,
    targetJunction==0?"C-straight":targetJunction==1?"A-Room1":"B-Room2");
  Serial.println("==============================");
}

// ─────────────────────────────────────────────────────────────
//  PD FOLLOW
// ─────────────────────────────────────────────────────────────
void doPD() {
  IRSensor ir = readIR();

  if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed+rightOffset,0,255));
    return;
  }
  if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
    setMotorLeft(turnSpeed);
    setMotorRight(-(turnSpeed-rightOffset));
    return;
  }

  int error      = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(1*(int)ir.s4)+(2*(int)ir.s5);
  int derivative = error - lastError;
  lastError      = error;
  float corr     = Kp*error + Kd*derivative;

  int L = constrain((int)(baseSpeed - corr),               0, 255);
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
//  JUNCTION CHECK
// ─────────────────────────────────────────────────────────────
bool checkJunction() {
  IRSensor ir  = readIR();
  int      sum = irSum(ir);

  if (sum >= 3) {
    if (millis() - lastJunctionTime < JUNCTION_COOLDOWN_MS) {
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
//  SPIN HELPERS  (stateful, use global spinStart/spinStarted)
// ─────────────────────────────────────────────────────────────
bool spinLeft(unsigned long minMs) {
  if (!spinStarted) {
    stopMotorsOnly();
    delay(150);
    spinStart   = millis();
    spinStarted = true;
  }
  setMotorLeft(-turnSpeed);
  setMotorRight(constrain(turnSpeed+rightOffset,0,255));

  if (millis() - spinStart < minMs) return false;

  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly();
    delay(100);
    spinStarted = false;
    return true;
  }
  if (millis() - spinStart > 4000) {
    Serial.println(">> spinLeft timeout");
    stopMotorsOnly();
    delay(100);
    spinStarted = false;
    return true;
  }
  return false;
}

bool spinRight(unsigned long minMs) {
  if (!spinStarted) {
    stopMotorsOnly();
    delay(150);
    spinStart   = millis();
    spinStarted = true;
  }
  setMotorLeft(constrain(turnSpeed+rightOffset,0,255));
  setMotorRight(-turnSpeed);

  if (millis() - spinStart < minMs) return false;

  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly();
    delay(100);
    spinStarted = false;
    return true;
  }
  if (millis() - spinStart > 4000) {
    Serial.println(">> spinRight timeout");
    stopMotorsOnly();
    delay(100);
    spinStarted = false;
    return true;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
//  MAIN DRIVE FUNCTION
// ─────────────────────────────────────────────────────────────
void doLineFollow() {

  IRSensor ir  = readIR();
  int      sum = irSum(ir);

  // ── ARRIVED HOME — hard block, never moves again ──────────
  if (state == ARRIVED_HOME) {
    stopMotorsOnly();
    return;
  }

  // ── SEARCHING ─────────────────────────────────────────────
  if (state == SEARCHING) {
    IRSensor si = readIR();
    if (irSum(si) > 0) {
      Serial.println(">> Line found — resuming");
      state = stateAfterSearch;
    } else {
      if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
      else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    }
    return;
  }

  // ── JUNCTION PAUSE ────────────────────────────────────────
  if (state == JUNCTION_PAUSE) {
    unsigned long elapsed = millis() - junctionPauseStart;
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 500) {
      Serial.printf("  junction pause %lums/%dms\n", elapsed, JUNCTION_PAUSE_MS);
      lastMsg = millis();
    }
    if (elapsed >= JUNCTION_PAUSE_MS) {
      Serial.println(">> Pause done — nudging past junction...");
      unsigned long nudge = millis();
      while (millis() - nudge < (unsigned long)forwardAfterJunction) {
        setMotorLeft(baseSpeed);
        setMotorRight(constrain(baseSpeed+rightOffset,0,255));
        delay(10);
      }
      stopMotorsOnly();
      delay(100);
      spinStarted = false;
      state       = TURNING_TO_ROOM;
      Serial.println(">> Turning LEFT to branch...");
    }
    return;
  }

  // ── TURNING TO ROOM ───────────────────────────────────────
  if (state == TURNING_TO_ROOM) {
    if (spinLeft(BRANCH_TURN_MIN_MS)) {
      lastError        = 0;
      lineEndActive    = false;
      junctionFrames   = 0;
      lastJunctionTime = millis();  // cooldown
      state            = ON_BRANCH;
      Serial.println(">> ON_BRANCH — following to room wall");
    }
    return;
  }

  // ── ON BRANCH ─────────────────────────────────────────────
  if (state == ON_BRANCH) {

    // FIX 1: use allDark() and STOP while confirming (no coast drift)
    if (allDark(ir)) {
      if (!lineEndActive) {
        lineEndActive = true;
        lineEndStart  = millis();
        stopMotorsOnly();                        // ← STOP immediately
        Serial.println("  branch end? confirming (stopped)...");
      }

      // Stay stopped while confirming
      unsigned long waited = millis() - lineEndStart;
      static unsigned long lastEndMsg = 0;
      if (millis() - lastEndMsg > 200) {
        Serial.printf("  dark for %lums / %dms\n", waited, LINE_END_CONFIRM_MS);
        lastEndMsg = millis();
      }

      // Re-read mid-confirm — if line reappears, it was a false alarm
      IRSensor recheck = readIR();
      if (!allDark(recheck)) {
        lineEndActive = false;
        Serial.println("  false end — line back, continuing");
        return;
      }

      if (waited >= LINE_END_CONFIRM_MS) {
        // Confirmed: room wall reached
        stopMotorsOnly();
        lineEndActive   = false;
        state           = AT_ROOM;
        roomArrivalTime = millis();
        Serial.printf(">> ROOM REACHED — waiting %ds\n", ROOM_WAIT_MS/1000);
      }
    } else {
      lineEndActive = false;
      doPD();
    }
    return;
  }

  // ── AT ROOM ───────────────────────────────────────────────
  if (state == AT_ROOM) {
    unsigned long elapsed = millis() - roomArrivalTime;
    static unsigned long lastRoomMsg = 0;
    if (millis() - lastRoomMsg > 1000) {
      Serial.printf("  room wait %lus/%ds\n", elapsed/1000, ROOM_WAIT_MS/1000);
      lastRoomMsg = millis();
    }
    if (elapsed >= ROOM_WAIT_MS) {
      spinStarted = false;
      state       = UTURNING;
      Serial.println(">> Room wait done — U-turning");
    }
    return;
  }

  // ── U-TURN ────────────────────────────────────────────────
  if (state == UTURNING) {
    if (spinLeft(UTURN_MIN_MS)) {
      lastError        = 0;
      junctionFrames   = 0;
      lineEndActive    = false;
      lastJunctionTime = millis();
      state            = BRANCH_RETURN;
      Serial.println(">> BRANCH_RETURN — heading back to main");
    }
    return;
  }

  // ── BRANCH RETURN ─────────────────────────────────────────
  if (state == BRANCH_RETURN) {
    if (sum == 0) {
      stateAfterSearch = BRANCH_RETURN;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }
    if (checkJunction()) {
      Serial.println(">> Main line reached — rejoining");
      spinStarted = false;
      state       = REJOINING_MAIN;
      return;
    }
    doPD();
    return;
  }

  // ── REJOIN MAIN ───────────────────────────────────────────
  if (state == REJOINING_MAIN) {
    if (spinRight(REJOIN_SPIN_MS)) {
      lastError             = 0;
      junctionFrames        = 0;
      lastJunctionTime      = millis();
      returnJunctionsSeen   = 0;
      returnJunctionsNeeded = targetJunction;
      state                 = RETURNING;
      Serial.printf(">> RETURNING — need %d junction(s) for home\n",
        returnJunctionsNeeded);
    }
    return;
  }

  // ── RETURNING ─────────────────────────────────────────────
  if (state == RETURNING) {
    if (sum == 0) {
      stateAfterSearch = RETURNING;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }
    if (checkJunction()) {
      returnJunctionsSeen++;
      Serial.printf("*** RETURN JUNCTION %d / %d ***\n",
        returnJunctionsSeen, returnJunctionsNeeded);

      // FIX 2: hard stop immediately — no coast, lineFollowing cleared first
      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        missionComplete();
        return;
      }
    }
    doPD();
    return;
  }

  // ── OUTBOUND LINE FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {

    // Mode C: straight, stop when line ends
    if (targetJunction == 0) {
      if (allDark(ir)) {
        missionComplete();
        return;
      }
      doPD();
      return;
    }

    if (sum == 0) {
      stateAfterSearch = LINE_FOLLOWING;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }

    if (checkJunction()) {
      junctionsSeen++;
      Serial.printf("*** OUTBOUND JUNCTION %d ***\n", junctionsSeen);
      if (junctionsSeen == targetJunction) {
        Serial.printf(">> Target! Pausing %dms...\n", JUNCTION_PAUSE_MS);
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
//  COMMANDS
// ─────────────────────────────────────────────────────────────
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
      Serial.println(">>> Mode C: straight to end — type 'g'");
      break;
    case 'g':
      lineFollowing         = true;
      state                 = LINE_FOLLOWING;
      stateAfterSearch      = LINE_FOLLOWING;
      lastError             = 0;
      junctionsSeen         = 0;
      junctionFrames        = 0;
      returnJunctionsSeen   = 0;
      returnJunctionsNeeded = 0;
      lastJunctionTime      = 0;
      lineEndActive         = false;
      spinStarted           = false;
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
  Serial.println("  LINE FOLLOW v21 — Hospital Rover           ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=end  then 'g'          ");
  Serial.println("  detect jn → pause 2s → left → branch end  ");
  Serial.println("  wait 8s → U-turn → retrace → HARD STOP     ");
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
  // NOTE: once missionComplete() sets lineFollowing=false,
  // doLineFollow() is NEVER called again until 'g' is pressed
}
