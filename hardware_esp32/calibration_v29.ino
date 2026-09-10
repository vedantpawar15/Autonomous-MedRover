/*
 * ============================================================
 *  LINE FOLLOW v33 — Auto-order movement hardening
 * ============================================================
 *
 *  WHAT'S NEW vs v31:
 *   - Robust room_code parsing for web orders:
 *       "A", "a", " A ", "Room A" all map correctly
 *   - Mission start motor kick (220ms) so auto runs don't stay still
 *   - Auto start now forces watch mode off before movement
 *   - Extra serial logs for mapped room code and start path
 *
 *  ALL v29 LOGIC UNCHANGED:
 *   - returnSpeed=70, wobble escalation at 3000ms
 *   - inter-junction reseat after intermediate junctions
 *   - RETURN_MIN_TRAVEL_MS=800ms
 *   - Blocking missionComplete() with 180deg spin
 *   - Full return debug prints
 *
 *  TRACK LAYOUT:
 *   Room A -> turn LEFT at junction 1
 *   Room B -> turn LEFT at junction 2
 *   Room C -> straight to end (allDark detection)
 *
 *  SERIAL COMMANDS:
 *   A/B/C -> select room manually, then 'g' to go
 *   s -> stop    p -> settings    i -> IR snap    w -> watch IR
 *   + / - -> speed    ] / [ -> Kp    > / < -> Kd    ) / ( -> turn
 *   n / m -> nudge +/- 50ms
 * ============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── WiFi & Supabase Config ────────────────────────────────────
const char* WIFI_SSID         = "MIT#wPu@EXam2603";
const char* WIFI_PASSWORD     = "latur@24";
const char* SUPABASE_URL      = "https://tdkccbbqktzeojgeuaoh.supabase.co";
const char* SUPABASE_ANON_KEY =
  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRka2NjYmJxa3R6ZW9qZ2V1YW9oIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMzMzUyODEsImV4cCI6MjA4ODkxMTI4MX0."
  "eBjZ0U9FEIg3vwHqXr8KR8VygMQDORBcaRwVY1LsdyY";

#define POLL_INTERVAL_MS  5000

// ── Motor Pins ────────────────────────────────────────────────
#define IN1  27
#define IN2  26
#define IN3  25
#define IN4  33
#define ENA  14
#define ENB  12

// ── IR Sensor Pins ────────────────────────────────────────────
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
int   wobbleSpeed          = 80;
int   returnSpeed          = 70;
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

#define RETURN_JUNCTION_FRAMES      1
#define RETURN_JUNCTION_COOLDOWN  1000
#define RETURN_MIN_TRAVEL_MS       800

#define RETURN_WOBBLE_MS          1200
#define RETURN_WOBBLE_ESCALATE_MS 3000

#define REJOIN_CREEP_MS            400
#define FINAL_UTURN_MIN_MS         800
#define FINAL_UTURN_TIMEOUT_MS    5000

#define INTER_JUNCTION_STOP_MS     300
#define INTER_JUNCTION_CREEP_MS    200

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

// Wobble recovery
unsigned long wobbleStart      = 0;
unsigned long wobbleTotalStart = 0;
bool          wobbling         = false;
int           wobbleDir        = 1;

// Rejoin / reseat
bool          rejoinCreeping     = false;
unsigned long rejoinCreepStart   = 0;
bool          interJnReseating   = false;
unsigned long interJnReseatStart = 0;

// Spin helpers
unsigned long spinStart       = 0;
bool          spinStarted     = false;
bool          uturnPhase2     = false;
unsigned long uturnPhaseStart = 0;

// Debug
unsigned long lastDebugPrint = 0;

// ── Supabase order tracking ───────────────────────────────────
struct Order {
  long   id;
  String roomCode;
  String roomLabel;
  bool   valid;
};

long          activeOrderId = 0;    // 0 = manual / no order
bool          orderActive   = false;
unsigned long lastPoll      = 0;

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
void connectWiFi();
bool httpRequest(const String&, const String&, const String&, String&);
Order fetchNextPendingOrder();
bool updateOrderStatus(long orderId, const char* newStatus);
void startMission(int junction, long orderId);
String normalizeRoomCode(const String& raw);

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
  interJnReseating      = false;
}

// ─────────────────────────────────────────────────────────────
//  MISSION COMPLETE — blocking 180deg spin then idle
//  Marks order delivered in Supabase if auto-mode.
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

  // Mark delivered if this was a Supabase order
  if (orderActive && activeOrderId > 0) {
    Serial.printf("Marking order #%ld delivered...\n", activeOrderId);
    updateOrderStatus(activeOrderId, "delivered");
    orderActive   = false;
    activeOrderId = 0;
    Serial.println("Done. Resuming poll for next order.");
  }
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
  Serial.println("======== SETTINGS v31 ========");
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
  Serial.printf("  returnJunctionCooldown : %dms\n", RETURN_JUNCTION_COOLDOWN);
  Serial.printf("  returnMinTravel        : %dms\n", RETURN_MIN_TRAVEL_MS);
  Serial.printf("  returnWobbleMs         : %dms\n", RETURN_WOBBLE_MS);
  Serial.printf("  returnWobbleEscalate   : %dms\n", RETURN_WOBBLE_ESCALATE_MS);
  Serial.printf("  rejoinCreepMs          : %dms\n", REJOIN_CREEP_MS);
  Serial.printf("  finalUturnMinMs        : %dms\n", FINAL_UTURN_MIN_MS);
  Serial.printf("  targetJunction         : %d (%s)\n", targetJunction,
    targetJunction==0?"C-straight":targetJunction==1?"A-Room1":"B-Room2");
  Serial.printf("  orderActive            : %s (id=%ld)\n",
    orderActive ? "YES" : "NO", activeOrderId);
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
//  JUNCTION CHECK — outbound
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
// ─────────────────────────────────────────────────────────────
bool checkJunctionStrict() {
  IRSensor ir  = readIR();
  int      sum = irSum(ir);
  unsigned long now         = millis();
  unsigned long sinceLastJn = now - lastJunctionTime;

  if (sum >= 3) {
    if (sinceLastJn < RETURN_JUNCTION_COOLDOWN) {
      static unsigned long lastCooldownPrint = 0;
      if (now - lastCooldownPrint > 100) {
        Serial.printf("  [strict] sum=%d COOLDOWN %lums (seen=%d)\n",
          sum, RETURN_JUNCTION_COOLDOWN - sinceLastJn, returnJunctionsSeen);
        lastCooldownPrint = now;
      }
      setMotorLeft(returnSpeed);
      setMotorRight(constrain(returnSpeed+rightOffset,0,255));
      return false;
    }

    Serial.printf("  [strict] sum=%d [%d%d%d%d%d] t+%lums seen=%d/%d\n",
      sum, ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
      now - returnStartTime, returnJunctionsSeen, returnJunctionsNeeded);

    bool trigger = false;
    if (sum >= 4) {
      trigger = true;
    } else if (sum == 3 && returnJunctionsSeen >= 1) {
      Serial.println("  [strict] sum=3 accepted for FINAL junction");
      trigger = true;
    }

    if (trigger) {
      junctionFrames   = 0;
      lastJunctionTime = now;
      return true;
    }
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
    Serial.println(">> U-turn complete");
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
//  MAIN DRIVE FUNCTION  (v29 logic, unchanged)
// ─────────────────────────────────────────────────────────────
void doLineFollow() {
  IRSensor ir  = readIR();
  int      sum = irSum(ir);

  if (state == ARRIVED_HOME) { stopMotorsOnly(); return; }

  if (state == SEARCHING) {
    if (irSum(readIR()) > 0) {
      Serial.println(">> Line found — resuming");
      wobbling = false;
      state    = stateAfterSearch;
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
      Serial.println(">> Nudging past junction...");
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

      if (targetJunction == 0) {
        // Room C: U-turn done on main vertical line — no branch to rejoin.
        // Go straight to RETURNING, need 1 junction = Start T-bar.
        returnJunctionsSeen   = 0;
        returnJunctionsNeeded = 1;
        returnStartTime       = millis();
        wobbling              = false;
        interJnReseating      = false;
        state                 = RETURNING;
        Serial.println(">> [C] U-turn done — RETURNING direct, need 1 junction (Start T-bar)");
      } else {
        state = BRANCH_RETURN;
        Serial.println(">> BRANCH_RETURN");
      }
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
      Serial.println(">> Main line junction — rejoining");
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
    // Always 1: after rejoining the main vertical line and spinning to face
    // downward toward Start, the ONLY junction to count is the Start T-bar.
    // J1 / J2 are horizontal branches — they read sum=3..5 on the outbound
    // sensor array but when heading DOWN the main line the robot crosses them
    // at full width, so we use RETURN_JUNCTION_COOLDOWN to skip them and only
    // stop at the Start T-bar (which is the widest crossing, sum=5).
    returnJunctionsNeeded = 1;
    returnStartTime       = millis();
    wobbling              = false;
    rejoinCreeping        = false;
    interJnReseating      = false;
    state                 = RETURNING;
    Serial.println("================================================");
    Serial.printf(">> RETURNING — need 1 junction (Start T-bar)\n");
    Serial.printf("   returnSpeed=%d  cooldown=%dms\n", returnSpeed, RETURN_JUNCTION_COOLDOWN);
    Serial.println("================================================");
    return;
  }

  if (state == RETURNING) {

    // Inter-junction reseat
    if (interJnReseating) {
      unsigned long reseatElapsed = millis() - interJnReseatStart;
      if (reseatElapsed < INTER_JUNCTION_STOP_MS) {
        stopMotorsOnly(); return;
      }
      if (reseatElapsed < INTER_JUNCTION_STOP_MS + INTER_JUNCTION_CREEP_MS) {
        setMotorLeft(creepSpeed);
        setMotorRight(constrain(creepSpeed + rightOffset/4, 0, 255));
        return;
      }
      interJnReseating = false;
      lastError        = 0;
      wobbling         = false;
      Serial.println("  [return] reseat done — continuing");
    }

    unsigned long returnTravelled = millis() - returnStartTime;
    unsigned long now             = millis();

    if (now - lastDebugPrint >= 50) {
      IRSensor dbg = readIR();
      Serial.printf("  [RET t=%lu] [%d%d%d%d%d] sum=%d travel=%lums seen=%d/%d%s\n",
        now, dbg.s1,dbg.s2,dbg.s3,dbg.s4,dbg.s5, irSum(dbg),
        returnTravelled, returnJunctionsSeen, returnJunctionsNeeded,
        (returnTravelled < RETURN_MIN_TRAVEL_MS) ? " [PRE-ARM]" : "");
      lastDebugPrint = now;
    }

    auto doWobble = [&]() {
      if (!wobbling) {
        wobbling         = true;
        wobbleStart      = millis();
        wobbleTotalStart = millis();
        wobbleDir        = (lastError >= 0) ? 1 : -1;
        Serial.println("  [return] lost line — wobbling");
      }
      if (millis() - wobbleTotalStart >= RETURN_WOBBLE_ESCALATE_MS) {
        Serial.println("  [return] wobble escalated — SEARCHING");
        wobbling         = false;
        stateAfterSearch = RETURNING;
        state            = SEARCHING;
        searchDir        = wobbleDir;
        return;
      }
      if (millis() - wobbleStart < RETURN_WOBBLE_MS) {
        if (wobbleDir > 0) { setMotorLeft(wobbleSpeed); setMotorRight(-wobbleSpeed); }
        else               { setMotorLeft(-wobbleSpeed); setMotorRight(wobbleSpeed); }
      } else {
        wobbleDir   = -wobbleDir;
        wobbleStart = millis();
      }
    };

    if (returnTravelled < RETURN_MIN_TRAVEL_MS) {
      if (sum == 0) { doWobble(); }
      else {
        wobbling = false;
        int s = baseSpeed; baseSpeed = returnSpeed; doPD(); baseSpeed = s;
      }
      return;
    }

    if (sum == 0) { doWobble(); return; }
    wobbling = false;

    if (checkJunctionStrict()) {
      returnJunctionsSeen++;
      Serial.println("================================================");
      Serial.printf("*** RETURN JUNCTION %d / %d at t=%lums ***\n",
        returnJunctionsSeen, returnJunctionsNeeded, millis() - returnStartTime);
      Serial.println("================================================");

      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        missionComplete();
        return;
      }
      Serial.println("  [return] intermediate junction — reseating...");
      interJnReseating   = true;
      interJnReseatStart = millis();
      return;
    }

    int s = baseSpeed; baseSpeed = returnSpeed; doPD(); baseSpeed = s;
    return;
  }

  // ── OUTBOUND LINE FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {
    // ── Room C: straight to end, bypass ALL junctions ────────
    // targetJunction==0 means C. Robot follows the vertical line
    // past J1 and J2 (both crossed straight, not stopped at).
    // When allDark for LINE_END_CONFIRM_MS it has reached the
    // physical end of C. Then wait 5s, U-turn, return to Start.
    if (targetJunction == 0) {
      // End detection — arm only after BRANCH_MIN_TRAVEL_MS
      if (!endDetectArmed) {
        if (millis() - branchStartTime < BRANCH_MIN_TRAVEL_MS) {
          // Still arming — drive straight, ignore junctions
          doPD();
          return;
        }
        endDetectArmed = true;
        Serial.println("  [C] end detection ARMED");
      }

      if (allDark(ir)) {
        if (!lineEndActive) {
          lineEndActive = true;
          lineEndStart  = millis();
          Serial.println("  [C] end of line? confirming...");
        }
        // Creep to confirm real wall (not grout gap)
        setMotorLeft(creepSpeed);
        setMotorRight(constrain(creepSpeed + rightOffset/4, 0, 255));
        unsigned long waited = millis() - lineEndStart;
        static unsigned long lastCEndMsg = 0;
        if (millis() - lastCEndMsg > 300) {
          Serial.printf("  [C] dark for %lums / %dms\n", waited, LINE_END_CONFIRM_MS);
          lastCEndMsg = millis();
        }
        if (!allDark(readIR())) {
          lineEndActive = false;  // grout gap — carry on
          return;
        }
        if (waited >= LINE_END_CONFIRM_MS) {
          // Confirmed end of C line
          stopMotorsOnly();
          lineEndActive  = false;
          endDetectArmed = false;
          state           = AT_ROOM;   // reuse AT_ROOM for the 5s wait
          roomArrivalTime = millis();
          Serial.printf(">> ROOM C REACHED — waiting %ds\n", ROOM_WAIT_MS/1000);
        }
      } else {
        lineEndActive = false;
        doPD();
      }
      return;
    }

    // ── Rooms A / B: follow to target junction then turn left ─
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
//  MISSION KICKOFF — resets all state vars, starts run
// ─────────────────────────────────────────────────────────────
void startMission(int junction, long orderId) {
  targetJunction        = junction;
  activeOrderId         = orderId;
  orderActive           = (orderId > 0);
  watchingIR            = false;
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
  interJnReseating      = false;
  lastDebugPrint        = 0;
  branchStartTime       = millis();  // Room C uses this to arm end-detection
  Serial.printf(">>> MISSION START — junction=%d  orderId=%ld\n", junction, orderId);

  // Kick motors briefly so auto-start cannot appear "stuck" at trigger time.
  setMotorLeft(baseSpeed);
  setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
  delay(220);

  printSettings();
}

// ─────────────────────────────────────────────────────────────
//  WIFI
// ─────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FAILED — offline (manual mode only)");
  }
}

// ─────────────────────────────────────────────────────────────
//  SUPABASE HTTP
// ─────────────────────────────────────────────────────────────
bool httpRequest(const String& method, const String& url,
                 const String& body, String& response) {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[HTTP] WiFi down"); return false; }
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  http.addHeader("apikey",        SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type",  "application/json");
  // Supabase requires this for PATCH — without it returns 406 Not Acceptable
  if (method == "PATCH") http.addHeader("Prefer", "return=minimal");

  int code;
  if      (method == "GET")   code = http.GET();
  else if (method == "PATCH") code = http.PATCH(body);
  else if (method == "POST")  code = http.POST(body);
  else { http.end(); return false; }

  bool ok = false;
  if (code > 0) {
    // Avoid blocking on empty/chunked PATCH responses (common with return=minimal).
    // For GET we still read body (needed for JSON parsing).
    if (method == "GET" || method == "POST") {
      response = http.getString();
    } else {
      response = "";
    }
    Serial.printf("[HTTP] %s %d\n", method.c_str(), code);
    ok = (code >= 200 && code < 300);
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return ok;
}

Order fetchNextPendingOrder() {
  Order o = { 0, "", "", false };
  String url = String(SUPABASE_URL) +
    "/rest/v1/orders?status=eq.pending&order=created_at.asc&limit=1";
  String resp;
  if (!httpRequest("GET", url, "", resp)) return o;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return o;
  if (!doc.is<JsonArray>() || doc.size() == 0) return o;

  JsonObject obj = doc[0];
  o.id        = obj["id"]         | 0;
  o.roomCode  = obj["room_code"]  | "";
  o.roomLabel = obj["room_label"] | "";
  o.valid     = (o.id > 0 && o.roomCode.length() > 0);
  return o;
}

bool updateOrderStatus(long orderId, const char* newStatus) {
  String url = String(SUPABASE_URL) + "/rest/v1/orders?id=eq." + String(orderId);
  StaticJsonDocument<128> bodyDoc;
  bodyDoc["status"] = newStatus;
  String body, resp;
  serializeJson(bodyDoc, body);
  bool ok = httpRequest("PATCH", url, body, resp);
  Serial.println(ok ? String("Order status -> ") + newStatus
                    : String("Status update FAILED: ") + resp);
  return ok;
}

String normalizeRoomCode(const String& raw) {
  String r = raw;
  r.trim();
  r.toUpperCase();

  if (r == "A" || r == "B" || r == "C") return r;

  // Accept labels like "ROOM A" by taking the last alpha character.
  for (int i = (int)r.length() - 1; i >= 0; --i) {
    char c = r[i];
    if (c >= 'A' && c <= 'Z') {
      if (c == 'A' || c == 'B' || c == 'C') return String(c);
      break;
    }
  }
  return "";
}

// ─────────────────────────────────────────────────────────────
//  COMMANDS
// ─────────────────────────────────────────────────────────────
void processCommand(char cmd) {
  switch(cmd) {
    case 'A': case 'a': targetJunction=1; Serial.println(">>> Mode A: Room 1 — 'g' to go"); break;
    case 'B': case 'b': targetJunction=2; Serial.println(">>> Mode B: Room 2 — 'g' to go"); break;
    case 'C': case 'c': targetJunction=0; Serial.println(">>> Mode C: straight — 'g' to go"); break;
    case 'g':
      startMission(targetJunction, 0);   // orderId=0 = manual (no Supabase update)
      break;
    case 's':
      stopAll();
      orderActive   = false;
      activeOrderId = 0;
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
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction); break;
    case 'm':
      forwardAfterJunction=max(forwardAfterJunction-50,0);
      Serial.printf("forwardAfterJunction:%dms\n",forwardAfterJunction); break;
    case 'i': printIR();                                                  break;
    case 'w': stopAll(); watchingIR=true; Serial.println(">>> Watch IR"); break;
    case 'p': printSettings();                                            break;
    case '\n': case '\r': case ' ':                                       break;
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
  Serial.println("  LINE FOLLOW v33 — Hospital Rover           ");
  Serial.println("  FIX: Room C full path + Room B stop fix    ");
  Serial.println("==============================================");
  Serial.println("  A/B/C + g = manual  |  auto via Supabase   ");
  Serial.println("==============================================");
  printSettings();
  connectWiFi();
}

void loop() {
  // Serial commands — always processed
  while (Serial.available() > 0) {
    processCommand((char)Serial.read());
    delay(10);
  }

  // Active mission — drive the bot
  if (lineFollowing) {
    doLineFollow();
    delay(10);
    return;   // skip polling while running
  }

  // Watch IR mode
  if (watchingIR) { printIR(); delay(100); return; }

  // ── IDLE — poll Supabase every POLL_INTERVAL_MS ───────────
  unsigned long now = millis();
  if (now - lastPoll < POLL_INTERVAL_MS) return;
  lastPoll = now;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[poll] WiFi down — skipping");
    return;
  }

  Serial.println("[poll] Checking Supabase for pending orders...");
  Order o = fetchNextPendingOrder();

  if (!o.valid) {
    Serial.println("[poll] No pending orders.");
    return;
  }

  String roomCode = normalizeRoomCode(o.roomCode);
  Serial.printf("[poll] Order #%ld -> room raw='%s' normalized='%s' (%s)\n",
    o.id, o.roomCode.c_str(), roomCode.c_str(), o.roomLabel.c_str());

  // Map room_code to junction number
  int junction = -1;
  if      (roomCode == "A") junction = 1;
  else if (roomCode == "B") junction = 2;
  else if (roomCode == "C") junction = 0;

  if (junction == -1) {
    Serial.printf("[poll] Unknown room_code '%s' — skipping\n", o.roomCode.c_str());
    return;
  }

  // Mark in_transit — log failure but START THE ROBOT REGARDLESS.
  // A failed status update must never keep the robot stationary.
  Serial.println("[poll] Updating status -> in_transit ...");
  if (!updateOrderStatus(o.id, "in_transit")) {
    Serial.println("[poll] WARNING: in_transit update failed — starting anyway");
  }

  // Go!
  Serial.printf("[poll] Starting mission now (junction=%d, orderId=%ld)\n", junction, o.id);
  startMission(junction, o.id);
}
