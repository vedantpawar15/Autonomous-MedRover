/*
 * ============================================================
 *  LINE FOLLOW v24 — Fix return junction frame reset
 *
 *  ROOT CAUSE FIXED vs v23:
 *
 *  BUG 3 — Robot never stops at start junction on return:
 *    Cause: Strict junction needs 4 consecutive frames with
 *           4+ sensors lit. But the vertical line is narrow
 *           enough that robot briefly loses it (sum=0) between
 *           frames → enters SEARCHING → junctionFrames reset
 *           to 0 → count starts over. Never reaches 4 frames.
 *           Serial showed: frames:1/6, 2/6, 3/6 then reset.
 *
 *    Fix A: Replaced SEARCHING on return leg with inline
 *           "wobble recovery" — instead of entering SEARCHING
 *           state (which resets everything), robot just wiggles
 *           left/right for up to 300ms to reacquire line,
 *           keeping junctionFrames intact.
 *
 *    Fix B: RETURN_JUNCTION_FRAMES reduced 6 → 4.
 *           We already require 4+ sensors (wider than line),
 *           so 4 frames is strict enough without needing 6.
 *           Fewer frames = less chance of interrupt mid-count.
 *
 *    Fix C: checkJunctionStrict() no longer resets
 *           junctionFrames to 0 on sum<4 — it only decrements
 *           by 1 (sticky counter). This makes it tolerant of
 *           a single missed frame without losing all progress.
 *
 *  ALL v23 FIXES KEPT:
 *   - BRANCH_MIN_TRAVEL_MS=2000ms (no fake room early)
 *   - LINE_END_CONFIRM_MS=1500ms + creep (no grout false end)
 *   - RETURN_MIN_TRAVEL_MS=1500ms (skip rejoin junction)
 *   - RETURN_JUNCTION_COOLDOWN=2000ms
 *   - Adaptive U-turn (left first, right on timeout)
 *
 *  TRACK LAYOUT:
 *   Robot starts at TOP, moves DOWN vertical line
 *   Junction A = 1st horizontal branch (turns LEFT)
 *   Junction B = 2nd horizontal branch (turns LEFT)
 *   C          = end of vertical line
 *
 *  FULL JOURNEY (A or B):
 *   1. Follow vertical line downward (PD)
 *   2. Detect target junction → stop → wait 1s
 *   3. Nudge past centre → spin LEFT onto branch
 *   4. Wait 2s before arming end detection
 *   5. Follow branch (PD) until ALL 5 dark for 1500ms + creep
 *   6. Stop → wait 7s (medicine handoff)
 *   7. U-turn: try LEFT first, then RIGHT
 *   8. Follow branch back via PD to main line junction
 *   9. Spin RIGHT to face upward on main line
 *  10. Wait 1.5s then count junctions with sticky strict check
 *  11. After N junctions → HARD STOP, mission complete
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
int   forwardAfterJunction = 250;
int   creepSpeed           = 30;

// Timing constants
#define JUNCTION_PAUSE_MS        1000
#define ROOM_WAIT_MS             7000
#define UTURN_MIN_MS              800
#define UTURN_TIMEOUT_MS         4000
#define REJOIN_SPIN_MS            500
#define BRANCH_TURN_MIN_MS        400

// Branch end detection
#define BRANCH_MIN_TRAVEL_MS     2000   // arm end-detect only after this
#define LINE_END_CONFIRM_MS      1500   // all-dark for this long = real wall

// Outbound junction
#define JUNCTION_FRAMES_NEEDED      3
#define JUNCTION_COOLDOWN_MS      900

// Return junction — strict
#define RETURN_JUNCTION_FRAMES      4   // FIX B: reduced from 6→4
#define RETURN_JUNCTION_COOLDOWN  2000
#define RETURN_MIN_TRAVEL_MS     1500   // don't count until this far from rejoin

// FIX A: wobble recovery on return (instead of SEARCHING state)
#define RETURN_WOBBLE_MS          300   // max time to search for line on return

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
  SEARCHING         // only used on outbound + branch_return
};
RobotState state            = LINE_FOLLOWING;
RobotState stateAfterSearch = LINE_FOLLOWING;

// ── Journey vars ──────────────────────────────────────────────
int  targetJunction         = 1;
int  junctionsSeen          = 0;
int  returnJunctionsNeeded  = 0;
int  returnJunctionsSeen    = 0;
int  junctionFrames         = 0;      // shared frame counter

unsigned long lastJunctionTime   = 0;
unsigned long junctionPauseStart = 0;
unsigned long roomArrivalTime    = 0;
unsigned long lineEndStart       = 0;
unsigned long branchStartTime    = 0;
unsigned long returnStartTime    = 0;
bool          lineEndActive      = false;
bool          endDetectArmed     = false;

// FIX A: wobble recovery state
unsigned long wobbleStart    = 0;
bool          wobbling       = false;
int           wobbleDir      = 1;

// Spin helper state
unsigned long spinStart       = 0;
bool          spinStarted     = false;
bool          uturnPhase2     = false;
unsigned long uturnPhaseStart = 0;

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
}

void missionComplete() {
  lineFollowing = false;
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
  Serial.println("======== SETTINGS v24 ========");
  Serial.printf("  baseSpeed              : %d\n",  baseSpeed);
  Serial.printf("  rightOffset            : %d\n",  rightOffset);
  Serial.printf("  Kp / Kd                : %.1f / %.1f\n", Kp, Kd);
  Serial.printf("  turnSpeed              : %d\n",  turnSpeed);
  Serial.printf("  forwardAfterJunction   : %dms\n", forwardAfterJunction);
  Serial.printf("  junctionPause          : %dms\n", JUNCTION_PAUSE_MS);
  Serial.printf("  roomWait               : %dms\n", ROOM_WAIT_MS);
  Serial.printf("  branchMinTravel        : %dms\n", BRANCH_MIN_TRAVEL_MS);
  Serial.printf("  lineEndConfirm         : %dms\n", LINE_END_CONFIRM_MS);
  Serial.printf("  uturnMinMs             : %dms\n", UTURN_MIN_MS);
  Serial.printf("  uturnTimeout(switch)   : %dms\n", UTURN_TIMEOUT_MS);
  Serial.printf("  returnJunctionFrames   : %d\n",   RETURN_JUNCTION_FRAMES);
  Serial.printf("  returnJunctionCooldown : %dms\n", RETURN_JUNCTION_COOLDOWN);
  Serial.printf("  returnMinTravel        : %dms\n", RETURN_MIN_TRAVEL_MS);
  Serial.printf("  returnWobbleMs         : %dms\n", RETURN_WOBBLE_MS);
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
//  JUNCTION CHECK — normal (outbound + branch return)
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
//  JUNCTION CHECK — strict (return leg only)
//  FIX B: 4 frames required (down from 6)
//  FIX C: sticky counter — brief drop decrements by 1, not reset
// ─────────────────────────────────────────────────────────────
bool checkJunctionStrict() {
  IRSensor ir  = readIR();
  int      sum = irSum(ir);

  if (millis() - lastJunctionTime < RETURN_JUNCTION_COOLDOWN) {
    // In cooldown — just drive forward
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    junctionFrames = 0;
    return false;
  }

  if (sum >= 4) {
    // Strong junction signal — increment frame counter
    junctionFrames++;
    Serial.printf("  [strict jn] frames:%d/%d sum:%d\n",
      junctionFrames, RETURN_JUNCTION_FRAMES, sum);
    if (junctionFrames >= RETURN_JUNCTION_FRAMES) {
      junctionFrames   = 0;
      lastJunctionTime = millis();
      return true;
    }
    // Keep driving forward while accumulating frames
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    return false;
  }

  // FIX C: sum < 4 — sticky decay instead of hard reset
  // Only decrement (never below 0) so 1 bad frame doesn't lose all progress
  if (junctionFrames > 0) {
    junctionFrames--;
    Serial.printf("  [strict jn] decay→%d sum:%d\n", junctionFrames, sum);
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
//  SPIN HELPERS
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

// ─────────────────────────────────────────────────────────────
//  ADAPTIVE U-TURN — LEFT first, switch to RIGHT on timeout
// ─────────────────────────────────────────────────────────────
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

  // ── ARRIVED HOME — terminal state ─────────────────────────
  if (state == ARRIVED_HOME) {
    stopMotorsOnly();
    return;
  }

  // ── SEARCHING (outbound + branch return only) ──────────────
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

  // ── JUNCTION PAUSE ────────────────────────────────────────
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

  // ── TURNING TO ROOM ───────────────────────────────────────
  if (state == TURNING_TO_ROOM) {
    if (spinLeft(BRANCH_TURN_MIN_MS)) {
      lastError        = 0;
      lineEndActive    = false;
      endDetectArmed   = false;
      junctionFrames   = 0;
      lastJunctionTime = millis();
      branchStartTime  = millis();
      state            = ON_BRANCH;
      Serial.printf(">> ON_BRANCH — end detect arms in %dms\n",
        BRANCH_MIN_TRAVEL_MS);
    }
    return;
  }

  // ── ON BRANCH ─────────────────────────────────────────────
  if (state == ON_BRANCH) {
    // Wait minimum travel time before arming end detection
    if (!endDetectArmed) {
      unsigned long travelled = millis() - branchStartTime;
      if (travelled < BRANCH_MIN_TRAVEL_MS) {
        doPD();
        static unsigned long lastArmMsg = 0;
        if (millis() - lastArmMsg > 500) {
          Serial.printf("  [branch] arming in %lums\n",
            BRANCH_MIN_TRAVEL_MS - travelled);
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
        Serial.println("  branch end? confirming (creeping slowly)...");
      }
      // Creep forward — grout gap reappears, real wall stays dark
      setMotorLeft(creepSpeed);
      setMotorRight(constrain(creepSpeed + rightOffset/4, 0, 255));

      unsigned long waited = millis() - lineEndStart;
      static unsigned long lastEndMsg = 0;
      if (millis() - lastEndMsg > 300) {
        Serial.printf("  dark for %lums / %dms\n", waited, LINE_END_CONFIRM_MS);
        lastEndMsg = millis();
      }
      IRSensor recheck = readIR();
      if (!allDark(recheck)) {
        lineEndActive = false;
        Serial.println("  false end (floor gap) — continuing");
        return;
      }
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
      uturnPhase2 = false;
      state       = UTURNING;
      Serial.println(">> Room wait done — U-turning");
    }
    return;
  }

  // ── U-TURN ────────────────────────────────────────────────
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

  // ── BRANCH RETURN ─────────────────────────────────────────
  if (state == BRANCH_RETURN) {
    if (sum == 0) {
      stateAfterSearch = BRANCH_RETURN;
      state            = SEARCHING;
      searchDir        = (lastError >= 0) ? 1 : -1;
      return;
    }
    if (checkJunction()) {
      Serial.println(">> Main line junction detected — rejoining");
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
      returnStartTime       = millis();
      wobbling              = false;
      state                 = RETURNING;
      Serial.printf(">> RETURNING — need %d junction(s) — counting starts in %dms\n",
        returnJunctionsNeeded, RETURN_MIN_TRAVEL_MS);
    }
    return;
  }

  // ── RETURNING ─────────────────────────────────────────────
  if (state == RETURNING) {

    // FIX C: minimum travel before counting junctions
    unsigned long returnTravelled = millis() - returnStartTime;
    if (returnTravelled < RETURN_MIN_TRAVEL_MS) {
      static unsigned long lastRetMsg = 0;
      if (millis() - lastRetMsg > 400) {
        Serial.printf("  [return] counting arms in %lums\n",
          RETURN_MIN_TRAVEL_MS - returnTravelled);
        lastRetMsg = millis();
      }
      // FIX A: inline wobble recovery — no SEARCHING state on return
      if (sum == 0) {
        if (!wobbling) {
          wobbling   = true;
          wobbleStart = millis();
          wobbleDir   = (lastError >= 0) ? 1 : -1;
          Serial.println("  [return] lost line — wobbling");
        }
        if (millis() - wobbleStart < RETURN_WOBBLE_MS) {
          if (wobbleDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
          else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
        } else {
          // Flip direction
          wobbleDir  = -wobbleDir;
          wobbleStart = millis();
        }
      } else {
        wobbling = false;
        doPD();
      }
      return;
    }

    // Counting phase
    // FIX A: wobble recovery instead of SEARCHING on return
    if (sum == 0) {
      if (!wobbling) {
        wobbling    = true;
        wobbleStart = millis();
        wobbleDir   = (lastError >= 0) ? 1 : -1;
        Serial.println("  [return] lost line — wobbling");
      }
      if (millis() - wobbleStart < RETURN_WOBBLE_MS) {
        if (wobbleDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
        else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
      } else {
        wobbleDir   = -wobbleDir;
        wobbleStart = millis();
      }
      return;
    }

    wobbling = false;

    // FIX B+C: strict junction check with sticky frame counter
    if (checkJunctionStrict()) {
      returnJunctionsSeen++;
      Serial.printf("*** RETURN JUNCTION %d / %d ***\n",
        returnJunctionsSeen, returnJunctionsNeeded);
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
    if (targetJunction == 0) {
      if (allDark(ir)) { missionComplete(); return; }
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
        Serial.printf(">> Junction %d: not target — straight through\n",
          junctionsSeen);
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
      endDetectArmed        = false;
      spinStarted           = false;
      uturnPhase2           = false;
      wobbling              = false;
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
  Serial.println("  LINE FOLLOW v24 — Hospital Rover           ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=end  then 'g'          ");
  Serial.println("  FIX: sticky frame counter on return        ");
  Serial.println("  FIX: wobble recovery (no SEARCHING reset)  ");
  Serial.println("  FIX: 4-frame strict junction (was 6)       ");
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
