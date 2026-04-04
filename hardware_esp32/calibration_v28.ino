/*
 * ============================================================
 *  LINE FOLLOW v28 — v27 + Rich Debug for Room B return fault
 *
 *  CHANGES FROM v27:
 *   - RETURNING state: print irSum + sensor pattern every
 *     loop tick (throttled to 50ms) so Serial Monitor shows
 *     exactly what sum value the robot sees at every junction
 *   - checkJunctionStrict(): print sum on EVERY call ≥3,
 *     not just ≥4 — reveals if start-T is reading sum=3
 *     instead of sum=4 (misalignment / approach angle)
 *   - Added "COOLDOWN ACTIVE" print so you can see if 800ms
 *     cooldown is eating the start-T
 *   - Added timestamp to every important print
 *   - returnSpeed dropped to 40 (more dwell on start-T)
 *   - RETURN_JUNCTION_COOLDOWN dropped to 600ms (safer gap)
 *   - sum>=3 fallback added to checkJunctionStrict() but
 *     only AFTER seeing sum>=4 first in the run — disabled
 *     by default, enable with '#define STRICT_FALLBACK 1'
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
int   wobbleSpeed          = 60;
int   returnSpeed          = 40;   // v28: slower = more dwell on start-T
int   forwardAfterJunction = 250;
int   creepSpeed           = 30;

// Timing constants
#define JUNCTION_PAUSE_MS        1000
#define ROOM_WAIT_MS             7000
#define UTURN_MIN_MS              800
#define UTURN_TIMEOUT_MS         4000
#define REJOIN_SPIN_MS            500
#define BRANCH_TURN_MIN_MS        400

#define BRANCH_MIN_TRAVEL_MS     2000
#define LINE_END_CONFIRM_MS      1500

#define JUNCTION_FRAMES_NEEDED      3
#define JUNCTION_COOLDOWN_MS      900

// v28: cooldown reduced 800→600ms, gives more margin to catch start-T
#define RETURN_JUNCTION_FRAMES      1
#define RETURN_JUNCTION_COOLDOWN   600
#define RETURN_MIN_TRAVEL_MS      1500

#define RETURN_WOBBLE_MS          600
#define REJOIN_CREEP_MS           400
#define FINAL_UTURN_MIN_MS        800
#define FINAL_UTURN_TIMEOUT_MS   5000

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
unsigned long branchStartTime    = 0;
unsigned long returnStartTime    = 0;
bool          lineEndActive      = false;
bool          endDetectArmed     = false;

unsigned long wobbleStart  = 0;
bool          wobbling     = false;
int           wobbleDir    = 1;

bool          rejoinCreeping   = false;
unsigned long rejoinCreepStart = 0;

unsigned long spinStart       = 0;
bool          spinStarted     = false;
bool          uturnPhase2     = false;
unsigned long uturnPhaseStart = 0;

// ── v28 debug ─────────────────────────────────────────────────
unsigned long lastDebugPrint = 0;   // throttle per-tick prints

// ── Misc ──────────────────────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

struct IRSensor { bool s1,s2,s3,s4,s5; };

// ── Forward declarations ──────────────────────────────────────
IRSensor readIR();
int  irSum(const IRSensor &ir);
bool allDark(const IRSensor &ir);
void setMotorLeft(int spd);
void setMotorRight(int spd);
void stopMotorsOnly();
void stopAll();
void missionComplete();
void printSettings();
void printIR();
void doPD();
bool checkJunction();
bool checkJunctionStrict();
bool spinLeft(unsigned long minMs);
bool spinRight(unsigned long minMs);
bool spinUTurn();
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
  endDetectArmed        = false;
  spinStarted           = false;
  uturnPhase2           = false;
  wobbling              = false;
  rejoinCreeping        = false;
}

// ─────────────────────────────────────────────────────────────
//  MISSION COMPLETE — BLOCKING (v23 style, unchanged)
// ─────────────────────────────────────────────────────────────
void missionComplete() {
  stopMotorsOnly();
  delay(300);
  Serial.println(">> START REACHED — spinning 180deg...");

  unsigned long spinBegin = millis();
  while (true) {
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed + rightOffset, 0, 255));
    unsigned long elapsed = millis() - spinBegin;
    if (elapsed >= FINAL_UTURN_MIN_MS) {
      IRSensor ir = readIR();
      if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) break;
    }
    if (millis() - spinBegin >= FINAL_UTURN_TIMEOUT_MS) {
      Serial.println(">> U-turn timeout — stopping anyway");
      break;
    }
  }

  stopAll();
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

int irSum(const IRSensor &ir) {
  return (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
}

bool allDark(const IRSensor &ir) {
  return (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5);
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("[%d%d%d%d%d] sum=%d\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,irSum(ir));
}

void printSettings() {
  Serial.println("======== SETTINGS v28 ========");
  Serial.printf("  baseSpeed              : %d\n",  baseSpeed);
  Serial.printf("  rightOffset            : %d\n",  rightOffset);
  Serial.printf("  Kp / Kd                : %.1f / %.1f\n", Kp, Kd);
  Serial.printf("  turnSpeed              : %d\n",  turnSpeed);
  Serial.printf("  wobbleSpeed            : %d\n",  wobbleSpeed);
  Serial.printf("  returnSpeed            : %d\n",  returnSpeed);
  Serial.printf("  forwardAfterJunction   : %dms\n", forwardAfterJunction);
  Serial.printf("  junctionPause          : %dms\n", JUNCTION_PAUSE_MS);
  Serial.printf("  roomWait               : %dms\n", ROOM_WAIT_MS);
  Serial.printf("  branchMinTravel        : %dms\n", BRANCH_MIN_TRAVEL_MS);
  Serial.printf("  lineEndConfirm         : %dms\n", LINE_END_CONFIRM_MS);
  Serial.printf("  uturnMinMs             : %dms\n", UTURN_MIN_MS);
  Serial.printf("  returnJunctionFrames   : %d\n",   RETURN_JUNCTION_FRAMES);
  Serial.printf("  returnJunctionCooldown : %dms\n", RETURN_JUNCTION_COOLDOWN);
  Serial.printf("  returnMinTravel        : %dms\n", RETURN_MIN_TRAVEL_MS);
  Serial.printf("  rejoinCreepMs          : %dms\n", REJOIN_CREEP_MS);
  Serial.printf("  finalUturnMinMs        : %dms\n", FINAL_UTURN_MIN_MS);
  Serial.printf("  finalUturnTimeoutMs    : %dms\n", FINAL_UTURN_TIMEOUT_MS);
  Serial.printf("  targetJunction         : %d (%s)\n", targetJunction,
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
//  JUNCTION CHECK — normal outbound
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
//  JUNCTION CHECK STRICT — return leg
//
//  v28 DEBUG CHANGES:
//   - Print sum on every call where sum>=3 (not just >=4)
//     → reveals if start-T is only reading sum=3
//   - Print "COOLDOWN ACTIVE" with ms remaining
//     → reveals if cooldown is masking the start-T
//   - sum>=3 accepted ONLY for 2nd+ junction (returnJunctionsSeen>=1)
//     → B-junction still needs sum>=4 (clean T), but start-T
//        which robot approaches at slight angle may read sum=3
// ─────────────────────────────────────────────────────────────
bool checkJunctionStrict() {
  IRSensor ir  = readIR();
  int      sum = irSum(ir);
  unsigned long now = millis();
  unsigned long sinceLastJn = now - lastJunctionTime;

  // Always print if we're seeing anything interesting
  if (sum >= 3) {
    if (sinceLastJn < RETURN_JUNCTION_COOLDOWN) {
      // v28: print cooldown status so we can see if start-T is eaten
      static unsigned long lastCooldownPrint = 0;
      if (now - lastCooldownPrint > 100) {
        Serial.printf("  [strict] sum=%d [%d%d%d%d%d] COOLDOWN %lums remaining (seen=%d)\n",
          sum, ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
          RETURN_JUNCTION_COOLDOWN - sinceLastJn,
          returnJunctionsSeen);
        lastCooldownPrint = now;
      }
      setMotorLeft(returnSpeed);
      setMotorRight(constrain(returnSpeed+rightOffset,0,255));
      return false;
    }

    // Outside cooldown — log every reading >=3
    Serial.printf("  [strict] sum=%d [%d%d%d%d%d] t+%lums seen=%d/%d\n",
      sum, ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
      now - returnStartTime,
      returnJunctionsSeen, returnJunctionsNeeded);

    // Accept sum>=4 always; accept sum>=3 only for final junction
    bool trigger = false;
    if (sum >= 4) {
      trigger = true;
    } else if (sum == 3 && returnJunctionsSeen >= 1) {
      // Start-T approached at angle may read only 3 sensors
      Serial.println("  [strict] sum=3 accepted for FINAL junction (approach angle)");
      trigger = true;
    }

    if (trigger) {
      Serial.printf("  [strict] JUNCTION TRIGGERED sum=%d\n", sum);
      junctionFrames   = 0;
      lastJunctionTime = now;
      return true;
    }
  }

  return false;
}

// ─────────────────────────────────────────────────────────────
//  SPIN HELPERS (unchanged from v27)
// ─────────────────────────────────────────────────────────────
bool spinLeft(unsigned long minMs) {
  if (!spinStarted) {
    stopMotorsOnly(); delay(150);
    spinStart   = millis();
    spinStarted = true;
  }
  setMotorLeft(-turnSpeed);
  setMotorRight(constrain(turnSpeed+rightOffset,0,255));
  if (millis() - spinStart < minMs) return false;
  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    return true;
  }
  if (millis() - spinStart > 4000) {
    Serial.println(">> spinLeft timeout");
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    return true;
  }
  return false;
}

bool spinRight(unsigned long minMs) {
  if (!spinStarted) {
    stopMotorsOnly(); delay(150);
    spinStart   = millis();
    spinStarted = true;
  }
  setMotorLeft(constrain(turnSpeed+rightOffset,0,255));
  setMotorRight(-turnSpeed);
  if (millis() - spinStart < minMs) return false;
  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    return true;
  }
  if (millis() - spinStart > 4000) {
    Serial.println(">> spinRight timeout");
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    return true;
  }
  return false;
}

bool spinUTurn() {
  if (!spinStarted) {
    stopMotorsOnly(); delay(150);
    spinStart       = millis();
    uturnPhaseStart = millis();
    spinStarted     = true;
    uturnPhase2     = false;
    Serial.println(">> U-turn: trying LEFT first");
  }
  if (!uturnPhase2 && (millis() - uturnPhaseStart >= UTURN_TIMEOUT_MS)) {
    uturnPhase2     = true;
    uturnPhaseStart = millis();
    stopMotorsOnly(); delay(150);
    Serial.println(">> U-turn: switching to RIGHT");
  }
  if (!uturnPhase2) {
    setMotorLeft(-turnSpeed);
    setMotorRight(constrain(turnSpeed+rightOffset,0,255));
  } else {
    setMotorLeft(constrain(turnSpeed+rightOffset,0,255));
    setMotorRight(-turnSpeed);
  }
  if (millis() - spinStart < UTURN_MIN_MS) return false;
  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && !ir.s1) || (ir.s4 && !ir.s5)) {
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    uturnPhase2 = false;
    Serial.println(">> U-turn complete — line found");
    return true;
  }
  if (millis() - spinStart > 8000) {
    Serial.println(">> U-turn hard timeout");
    stopMotorsOnly(); delay(100);
    spinStarted = false;
    uturnPhase2 = false;
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

  if (state == ARRIVED_HOME) { stopMotorsOnly(); return; }

  if (state == SEARCHING) {
    if (irSum(readIR()) > 0) {
      Serial.println(">> Line found — resuming");
      state = stateAfterSearch;
    } else {
      if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
      else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    }
    return;
  }

  if (state == JUNCTION_PAUSE) {
    unsigned long elapsed = millis() - junctionPauseStart;
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 300) {
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
      stopMotorsOnly(); delay(100);
      spinStarted = false;
      state       = TURNING_TO_ROOM;
      Serial.println(">> Turning LEFT to branch...");
    }
    return;
  }

  if (state == TURNING_TO_ROOM) {
    if (spinLeft(BRANCH_TURN_MIN_MS)) {
      lastError        = 0;
      lineEndActive    = false;
      endDetectArmed   = false;
      junctionFrames   = 0;
      lastJunctionTime = millis();
      branchStartTime  = millis();
      state            = ON_BRANCH;
      Serial.printf(">> ON_BRANCH — end detect arms in %dms\n", BRANCH_MIN_TRAVEL_MS);
    }
    return;
  }

  if (state == ON_BRANCH) {
    if (!endDetectArmed) {
      unsigned long travelled = millis() - branchStartTime;
      if (travelled < BRANCH_MIN_TRAVEL_MS) {
        doPD();
        static unsigned long lastArmMsg = 0;
        if (millis() - lastArmMsg > 500) {
          Serial.printf("  [branch] arming in %lums\n", BRANCH_MIN_TRAVEL_MS - travelled);
          lastArmMsg = millis();
        }
        return;
      }
      endDetectArmed = true;
      Serial.println("  [branch] end detection ARMED");
    }
    if (allDark(ir)) {
      if (!lineEndActive) {
        lineEndActive = true;
        lineEndStart  = millis();
        Serial.println("  branch end? confirming...");
      }
      setMotorLeft(creepSpeed);
      setMotorRight(constrain(creepSpeed + rightOffset/4, 0, 255));
      unsigned long waited = millis() - lineEndStart;
      static unsigned long lastEndMsg = 0;
      if (millis() - lastEndMsg > 300) {
        Serial.printf("  dark for %lums / %dms\n", waited, LINE_END_CONFIRM_MS);
        lastEndMsg = millis();
      }
      IRSensor recheck = readIR();
      if (!allDark(recheck)) { lineEndActive = false; return; }
      if (waited >= LINE_END_CONFIRM_MS) {
        stopMotorsOnly();
        lineEndActive   = false;
        endDetectArmed  = false;
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

  if (state == AT_ROOM) {
    unsigned long elapsed = millis() - roomArrivalTime;
    static unsigned long lastRoomMsg = 0;
    if (millis() - lastRoomMsg > 1000) {
      Serial.printf("  room wait %lus/%ds\n", elapsed/1000, ROOM_WAIT_MS/1000);
      lastRoomMsg = millis();
    }
    if (elapsed >= ROOM_WAIT_MS) {
      spinStarted = false; uturnPhase2 = false;
      state = UTURNING;
      Serial.println(">> Room wait done — U-turning");
    }
    return;
  }

  if (state == UTURNING) {
    if (spinUTurn()) {
      lastError        = 0;
      junctionFrames   = 0;
      lineEndActive    = false;
      lastJunctionTime = millis();
      state            = BRANCH_RETURN;
      Serial.println(">> BRANCH_RETURN — heading back to main line");
    }
    return;
  }

  if (state == BRANCH_RETURN) {
    if (sum == 0) {
      stateAfterSearch = BRANCH_RETURN;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }
    if (checkJunction()) {
      Serial.println(">> Main line junction detected — rejoining");
      spinStarted    = false;
      rejoinCreeping = false;
      state          = REJOINING_MAIN;
      return;
    }
    doPD();
    return;
  }

  if (state == REJOINING_MAIN) {
    if (!rejoinCreeping) {
      if (spinRight(REJOIN_SPIN_MS)) {
        rejoinCreeping   = true;
        rejoinCreepStart = millis();
        lastError        = 0;
        Serial.printf(">> Rejoin spin done — creeping %dms\n", REJOIN_CREEP_MS);
      }
      return;
    }
    unsigned long crept = millis() - rejoinCreepStart;
    if (crept < REJOIN_CREEP_MS) {
      setMotorLeft(creepSpeed);
      setMotorRight(constrain(creepSpeed + rightOffset/4, 0, 255));
      return;
    }
    stopMotorsOnly(); delay(100);
    junctionFrames        = 0;
    lastJunctionTime      = millis();
    returnJunctionsSeen   = 0;
    returnJunctionsNeeded = targetJunction;
    returnStartTime       = millis();
    wobbling              = false;
    rejoinCreeping        = false;
    state                 = RETURNING;
    Serial.println("================================================");
    Serial.printf(">> RETURNING — need %d junction(s)\n", returnJunctionsNeeded);
    Serial.printf("   counting arms after %dms travel\n", RETURN_MIN_TRAVEL_MS);
    Serial.printf("   returnSpeed=%d  cooldown=%dms\n",
      returnSpeed, RETURN_JUNCTION_COOLDOWN);
    Serial.println("================================================");
    return;
  }

  // ── RETURNING — v28: rich per-tick debug ─────────────────────
  if (state == RETURNING) {
    unsigned long returnTravelled = millis() - returnStartTime;

    // ── v28: print IR state every 50ms so we see full picture ──
    unsigned long now = millis();
    if (now - lastDebugPrint >= 50) {
      IRSensor dbg = readIR();
      int dbgSum   = irSum(dbg);
      unsigned long sinceJn = now - lastJunctionTime;
      Serial.printf("  [RET t=%lu] [%d%d%d%d%d] sum=%d travel=%lums jnSince=%lums seen=%d/%d%s\n",
        now,
        dbg.s1,dbg.s2,dbg.s3,dbg.s4,dbg.s5,
        dbgSum,
        returnTravelled,
        sinceJn,
        returnJunctionsSeen,
        returnJunctionsNeeded,
        (returnTravelled < RETURN_MIN_TRAVEL_MS) ? " [PRE-ARM]" : ""
      );
      lastDebugPrint = now;
    }

    if (returnTravelled < RETURN_MIN_TRAVEL_MS) {
      if (sum == 0) {
        if (!wobbling) {
          wobbling    = true; wobbleStart = millis();
          wobbleDir   = (lastError >= 0) ? 1 : -1;
          Serial.println("  [return] lost line — wobbling");
        }
        if (millis() - wobbleStart < RETURN_WOBBLE_MS) {
          if (wobbleDir > 0) { setMotorLeft(wobbleSpeed); setMotorRight(-wobbleSpeed); }
          else               { setMotorLeft(-wobbleSpeed); setMotorRight(wobbleSpeed); }
        } else { wobbleDir = -wobbleDir; wobbleStart = millis(); }
      } else {
        wobbling = false;
        int savedBase = baseSpeed;
        baseSpeed = returnSpeed;
        doPD();
        baseSpeed = savedBase;
      }
      return;
    }

    if (sum == 0) {
      if (!wobbling) {
        wobbling    = true; wobbleStart = millis();
        wobbleDir   = (lastError >= 0) ? 1 : -1;
        Serial.println("  [return] lost line — wobbling");
      }
      if (millis() - wobbleStart < RETURN_WOBBLE_MS) {
        if (wobbleDir > 0) { setMotorLeft(wobbleSpeed); setMotorRight(-wobbleSpeed); }
        else               { setMotorLeft(-wobbleSpeed); setMotorRight(wobbleSpeed); }
      } else { wobbleDir = -wobbleDir; wobbleStart = millis(); }
      return;
    }

    wobbling = false;

    if (checkJunctionStrict()) {
      returnJunctionsSeen++;
      Serial.println("================================================");
      Serial.printf("*** RETURN JUNCTION %d / %d at t=%lums ***\n",
        returnJunctionsSeen, returnJunctionsNeeded, millis()-returnStartTime);
      Serial.println("================================================");
      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        missionComplete();
        return;
      }
    }

    int savedBase = baseSpeed;
    baseSpeed = returnSpeed;
    doPD();
    baseSpeed = savedBase;
    return;
  }

  // ── OUTBOUND LINE FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {
    if (targetJunction == 0) {
      if (allDark(ir)) { missionComplete(); return; }
      doPD(); return;
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
        Serial.printf(">> Junction %d: not target — straight through\n", junctionsSeen);
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
    case 'A': case 'a': targetJunction=1; Serial.println(">>> Mode A: Room 1"); break;
    case 'B': case 'b': targetJunction=2; Serial.println(">>> Mode B: Room 2"); break;
    case 'C': case 'c': targetJunction=0; Serial.println(">>> Mode C: straight"); break;
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
      endDetectArmed        = false;
      spinStarted           = false;
      uturnPhase2           = false;
      wobbling              = false;
      rejoinCreeping        = false;
      lastDebugPrint        = 0;
      Serial.println(">>> START");
      printSettings();
      break;
    case 's': stopAll(); Serial.println(">>> STOPPED"); break;
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
    case 'i': printIR();                                                break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR"); break;
    case 'p': printSettings();                                          break;
    case '\n': case '\r': case ' ':                                     break;
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
  Serial.println("  LINE FOLLOW v28 — Hospital Rover           ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=end  then 'g'          ");
  Serial.println("  v28: Rich debug for Room B return fault     ");
  Serial.println("  returnSpeed=40  cooldown=600ms  sum3fallback");
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