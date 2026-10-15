/*
 * ============================================================
 *   ESP32 HOSPITAL ROVER — NO PWM BUILD
 *   Line follow + junctions + rooms + obstacle + Supabase
 * ============================================================
 *
 *  BASE: your working HC-SR04 line follower.
 *  Motor functions, correction timings and the followLine()
 *  ladder are UNCHANGED — full speed via digitalWrite, exactly
 *  as they were when it followed the line well.
 *
 *  ENA -> 5V   ENB -> 5V   (keep the jumpers ON)
 *  NO PWM ANYWHERE. No baseSpeed, no rightOffset, no Kp/Kd.
 *
 * ------------------------------------------------------------
 *  TRACK
 * ------------------------------------------------------------
 *        C  (end of line)
 *        |
 *    B---+  J2        branch on the LEFT going up
 *        |
 *    A---+  J1        branch on the LEFT going up
 *        |
 *      __|__ START    black on BOTH sides
 *
 *  Your measured sensor patterns:
 *      on the line      00100
 *      1cm left of it   00010
 *      on J1            11100
 *      on the Start bar 11111
 *
 *  So junctions are told apart by SHAPE, never counted:
 *      s1 s2 s3 dark, s5 clear -> LEFT tee  (J1/J2 going up)
 *      s3 s4 s5 dark, s1 clear -> RIGHT tee (J1/J2 coming back)
 *      s1 AND s5 dark          -> START bar = HOME
 *
 * ------------------------------------------------------------
 *  HOW MISSED JUNCTIONS ARE FIXED
 * ------------------------------------------------------------
 *  followLine()'s corrections block for 12-38ms at a time, so a
 *  junction could slip past between reads. Every delay() inside
 *  a correction is now watchDelay(), which polls the sensors
 *  every 2ms while it waits and latches anything it sees. Same
 *  timings, same behaviour — it just cannot miss a junction now.
 *
 * ------------------------------------------------------------
 *  HOW TURNS ARE FIXED
 * ------------------------------------------------------------
 *  Turns used to exit the moment any sensor saw black — while
 *  sitting on a junction, that is instantly true, so it stopped
 *  at a random angle and drove off into white. Now every turn is
 *  two phase: pivot until the centre sensor LEAVES the line,
 *  then pivot until it FINDS the line again. That is a real
 *  alignment, not a guess.
 *
 * ------------------------------------------------------------
 *  SERIAL
 * ------------------------------------------------------------
 *   A / B / C = pick room      g = GO        s = STOP
 *   i = IR snapshot            w = IR monitor
 *   d = distance               h = help
 *   t = turn test (pivot left 90 on the spot)
 * ============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/* ============================================================
 * WIFI + SUPABASE
 * ============================================================
 */

const char* WIFI_SSID         = "Ram";
const char* WIFI_PASSWORD     = "latur@24";
const char* SUPABASE_URL      = "https://tdkccbbqktzeojgeuaoh.supabase.co";
const char* SUPABASE_ANON_KEY =
  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRka2NjYmJxa3R6ZW9qZ2V1YW9oIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMzMzUyODEsImV4cCI6MjA4ODkxMTI4MX0."
  "eBjZ0U9FEIg3vwHqXr8KR8VygMQDORBcaRwVY1LsdyY";

#define POLL_INTERVAL_MS  5000


/* ============================================================
 * MOTOR PINS
 * ============================================================
 */

#define LEFT_IN1   27
#define LEFT_IN2   26

#define RIGHT_IN1  25
#define RIGHT_IN2  33


/* ============================================================
 * IR SENSOR PINS
 * ============================================================
 */

#define SENSOR_1   34
#define SENSOR_2   35
#define SENSOR_3   32
#define SENSOR_4   18
#define SENSOR_5   19


/* ============================================================
 * HC-SR04 + BUZZER
 * ============================================================
 */

#define TRIG_PIN   5
#define ECHO_PIN   23
#define BUZZER_PIN 4

#define OBSTACLE_DISTANCE_CM 30.0
#define ULTRASONIC_INTERVAL_MS 60

#define BUZZER_ON_TIME   200
#define BUZZER_OFF_TIME  300


/* ============================================================
 * LINE FOLLOW PARAMETERS  (unchanged)
 * ============================================================
 */

#define GENTLE_TURN_TIME   12
#define MEDIUM_TURN_TIME   22
#define STRONG_TURN_TIME   38
#define LOST_LINE_TURN_TIME 35


/* ============================================================
 * MISSION PARAMETERS
 * ============================================================
 */

/* A shape must be seen this many consecutive 2ms samples.
 * Cheap noise rejection without slowing detection down. */
#define SHAPE_FRAMES         3

/* After handling a junction, ignore shapes for this long so the
 * same bar is never handled twice. */
#define JUNCTION_COOLDOWN_MS 1200

/* Sensor bar sits ahead of the wheels, so after the sensors see
 * a junction the wheels are still short of it. Drive forward this
 * long before pivoting, to put the axle over the junction. */
#define NUDGE_FORWARD_MS      250

/* Driving straight across a junction we are not stopping at. */
#define CROSS_STRAIGHT_MS     260

/* Turns */
#define TURN_MIN_MS           200
#define TURN_TIMEOUT_MS      2500
#define UTURN_MIN_MS          600
#define UTURN_TIMEOUT_MS     4000

/* Room end of line: creep and confirm. Short, so it stops dead
 * instead of overshooting. */
#define LINE_END_CONFIRM_MS   300
#define CREEP_ON_MS            25
#define CREEP_OFF_MS           45

/* Ignore end-of-line for this long after entering a branch, so a
 * tape gap right after the turn is not mistaken for the room. */
#define BRANCH_ARM_MS        2000

/* Ignore HOME for this long after starting the return, so the bar
 * we just rejoined at is not read as the Start bar. */
#define RETURN_ARM_MS        1500

/* How long the rover waits at a room. */
#define ROOM_WAIT_MS         7000


/* ============================================================
 * MISSION STATE
 * ============================================================
 */

enum MissionState {
  IDLE,
  OUTBOUND,       /* up the main line, looking for the target tee */
  ON_BRANCH,      /* driving down a room branch                   */
  AT_ROOM,        /* parked, waiting                              */
  BRANCH_BACK,    /* driving back along the branch to the main    */
  RETURNING,      /* down the main line, looking for the Start bar*/
  DONE
};

MissionState mission = IDLE;

bool robotRunning = false;

/* 1 = Room A (J1)   2 = Room B (J2)   0 = Room C (end of line) */
int  targetRoom    = 1;
int  leftTeesSeen  = 0;

int  lastDirection = 0;

unsigned long stateEnteredAt   = 0;
unsigned long lastJunctionAt   = 0;
bool          endDetectArmed   = false;


/* ── Shape latches, set by the sampler ────────────────────── */
bool sawLeftTee  = false;
bool sawRightTee = false;
bool sawFullBar  = false;

int  leftTeeRun  = 0;
int  rightTeeRun = 0;
int  fullBarRun  = 0;


/* ============================================================
 * ULTRASONIC + BUZZER STATE
 * ============================================================
 */

float currentDistance = -1.0;
unsigned long lastUltrasonicRead = 0;
bool obstacleDetected = false;

bool buzzerState = false;
unsigned long lastBuzzerChange = 0;


/* ============================================================
 * SUPABASE ORDER STATE
 * ============================================================
 */

struct Order {
  long   id;
  String roomCode;
  String roomLabel;
  bool   valid;
};

long          activeOrderId = 0;
bool          orderActive   = false;
unsigned long lastPoll      = 0;


/* ── forward declarations ─────────────────────────────────── */
void sampleShapes();
void watchDelay(unsigned long ms);
bool updateOrderStatus(long orderId, const char* newStatus);


/* ============================================================
 * MOTOR FUNCTIONS   (verbatim from the working sketch)
 * ============================================================
 */

void leftMotorForward()
{
  digitalWrite(LEFT_IN1, HIGH);
  digitalWrite(LEFT_IN2, LOW);
}

void leftMotorBackward()
{
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, HIGH);
}

void rightMotorForward()
{
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
}

void rightMotorBackward()
{
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, HIGH);
}

void leftMotorStop()
{
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
}

void rightMotorStop()
{
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
}

void moveForward()
{
  leftMotorForward();
  rightMotorForward();
}

void moveBackward()
{
  leftMotorBackward();
  rightMotorBackward();
}

void stopMotors()
{
  leftMotorStop();
  rightMotorStop();
}


/* ============================================================
 * IR SENSOR FUNCTIONS   (active LOW)
 * ============================================================
 */

int readSensor1() { return !digitalRead(SENSOR_1); }
int readSensor2() { return !digitalRead(SENSOR_2); }
int readSensor3() { return !digitalRead(SENSOR_3); }
int readSensor4() { return !digitalRead(SENSOR_4); }
int readSensor5() { return !digitalRead(SENSOR_5); }

int irSum()
{
  return readSensor1() + readSensor2() + readSensor3()
       + readSensor4() + readSensor5();
}

bool allDark()
{
  return (!readSensor1() && !readSensor2() && !readSensor3()
       && !readSensor4() && !readSensor5());
}

void printSensors()
{
  int s1 = readSensor1();
  int s2 = readSensor2();
  int s3 = readSensor3();
  int s4 = readSensor4();
  int s5 = readSensor5();

  Serial.print("[");
  Serial.print(s1); Serial.print(s2); Serial.print(s3);
  Serial.print(s4); Serial.print(s5);
  Serial.print("]  sum=");
  Serial.print(s1+s2+s3+s4+s5);

  if (s1 && s2 && s3 && !s5) Serial.print("   LEFT-TEE");
  if (s3 && s4 && s5 && !s1) Serial.print("   RIGHT-TEE");
  if (s1 && s5 && s3)        Serial.print("   START-BAR");

  Serial.println();
}


/* ============================================================
 * SHAPE SAMPLER
 *
 * Called constantly — from the main loop AND from inside every
 * correction delay. A shape must persist SHAPE_FRAMES samples
 * before it latches.
 * ============================================================
 */

void sampleShapes()
{
  int s1 = readSensor1();
  int s2 = readSensor2();
  int s3 = readSensor3();
  int s4 = readSensor4();
  int s5 = readSensor5();

  /* LEFT tee: black spreads left of the line only. */
  if (s1 && s2 && s3 && !s5) {
    leftTeeRun++;
    if (leftTeeRun >= SHAPE_FRAMES) sawLeftTee = true;
  } else {
    leftTeeRun = 0;
  }

  /* RIGHT tee: black spreads right of the line only. */
  if (s3 && s4 && s5 && !s1) {
    rightTeeRun++;
    if (rightTeeRun >= SHAPE_FRAMES) sawRightTee = true;
  } else {
    rightTeeRun = 0;
  }

  /* START bar: black on BOTH outer sensors at once. */
  if (s1 && s5 && s3) {
    fullBarRun++;
    if (fullBarRun >= SHAPE_FRAMES) sawFullBar = true;
  } else {
    fullBarRun = 0;
  }
}

void clearShapes()
{
  sawLeftTee = sawRightTee = sawFullBar = false;
  leftTeeRun = rightTeeRun = fullBarRun = 0;
}

/*
 * A delay that keeps its eyes open. Drop-in replacement for
 * delay() inside the motion corrections.
 */
void watchDelay(unsigned long ms)
{
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    sampleShapes();
    delay(2);
  }
}


/* ============================================================
 * LINE CORRECTION   (same timings, watching delays)
 * ============================================================
 */

void correctLeft(int correctionTime)
{
  leftMotorStop();
  rightMotorForward();
  watchDelay(correctionTime);
  moveForward();
}

void correctRight(int correctionTime)
{
  leftMotorForward();
  rightMotorStop();
  watchDelay(correctionTime);
  moveForward();
}

void strongLeft()
{
  leftMotorBackward();
  rightMotorForward();
  watchDelay(STRONG_TURN_TIME);
  moveForward();
}

void strongRight()
{
  leftMotorForward();
  rightMotorBackward();
  watchDelay(STRONG_TURN_TIME);
  moveForward();
}


/* ============================================================
 * SLOW MOTION WITHOUT PWM
 *
 * ENA/ENB are tied to 5V so speed cannot be lowered. Pulsing
 * the motors on and off gives a slow crawl instead, used when
 * confirming the end of a line so the rover does not overshoot
 * into the wall.
 * ============================================================
 */

void creepForward(unsigned long ms)
{
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    moveForward();
    watchDelay(CREEP_ON_MS);
    stopMotors();
    watchDelay(CREEP_OFF_MS);
  }
  stopMotors();
}


/* ============================================================
 * TURNS — two phase, so they end ALIGNED not guessed
 * ============================================================
 */

/*
 * phase 1: pivot until the centre sensor leaves the line
 * phase 2: keep pivoting until it finds the line again
 */
bool pivotUntilLine(bool left, unsigned long minMs, unsigned long timeoutMs)
{
  unsigned long t0 = millis();

  if (left) { leftMotorBackward(); rightMotorForward(); }
  else      { leftMotorForward();  rightMotorBackward(); }

  /* minimum swing — gets us off the junction blob */
  while (millis() - t0 < minMs) delay(2);

  /* phase 1 — wait for the centre sensor to go clear */
  while (readSensor3()) {
    if (millis() - t0 > timeoutMs) {
      stopMotors();
      Serial.println("   turn: timeout in phase 1");
      return false;
    }
    delay(2);
  }

  /* phase 2 — wait for it to pick the new line up */
  while (!readSensor3()) {
    if (millis() - t0 > timeoutMs) {
      stopMotors();
      Serial.println("   turn: timeout in phase 2");
      return false;
    }
    delay(2);
  }

  stopMotors();
  delay(120);
  Serial.printf("   turn done in %lums\n", millis() - t0);
  return true;
}

void turnLeft90()
{
  Serial.println(">> Turning LEFT onto branch");
  pivotUntilLine(true, TURN_MIN_MS, TURN_TIMEOUT_MS);
}

void turnRight90()
{
  Serial.println(">> Turning RIGHT onto main line");
  pivotUntilLine(false, TURN_MIN_MS, TURN_TIMEOUT_MS);
}

void turnAround180()
{
  Serial.println(">> U-turn");
  pivotUntilLine(true, UTURN_MIN_MS, UTURN_TIMEOUT_MS);
}


/* ============================================================
 * HC-SR04
 * ============================================================
 */

float measureDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 12000);

  if (duration == 0) return -1.0;
  return duration / 58.0;
}

void updateUltrasonic()
{
  unsigned long now = millis();
  if (now - lastUltrasonicRead < ULTRASONIC_INTERVAL_MS) return;
  lastUltrasonicRead = now;
  currentDistance = measureDistanceCM();
}

bool obstacleIsPresent()
{
  return (currentDistance > 0 && currentDistance <= OBSTACLE_DISTANCE_CM);
}

void printDistance()
{
  lastUltrasonicRead = 0;
  updateUltrasonic();
  if (currentDistance < 0) Serial.println("Distance: NO ECHO");
  else {
    Serial.print("Distance: ");
    Serial.print(currentDistance, 1);
    Serial.println(" cm");
  }
}


/* ============================================================
 * BUZZER
 * ============================================================
 */

void updateBuzzer()
{
  unsigned long now = millis();

  if (buzzerState) {
    if (now - lastBuzzerChange >= BUZZER_ON_TIME) {
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      lastBuzzerChange = now;
    }
  } else {
    if (now - lastBuzzerChange >= BUZZER_OFF_TIME) {
      buzzerState = true;
      digitalWrite(BUZZER_PIN, HIGH);
      lastBuzzerChange = now;
    }
  }
}

void buzzerOff()
{
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
  lastBuzzerChange = millis();
}


/* ============================================================
 * OBSTACLE HANDLING
 *
 * Only while travelling. Not while turning, parked at a room,
 * or U-turning — the sensor sweeps across walls during a pivot
 * and would stop the rover for no reason.
 * ============================================================
 */

bool obstacleWatchActive()
{
  return (mission == OUTBOUND ||
          mission == BRANCH_BACK ||
          mission == RETURNING);
}

/* Returns true if the rover is being held by an obstacle. */
bool handleObstacle()
{
  if (!obstacleWatchActive()) return false;

  updateUltrasonic();

  if (obstacleIsPresent()) {
    stopMotors();
    updateBuzzer();

    if (!obstacleDetected) {
      obstacleDetected = true;
      Serial.println();
      Serial.println("================================");
      Serial.println("       OBSTACLE DETECTED");
      Serial.print("Distance: ");
      Serial.print(currentDistance, 1);
      Serial.println(" cm");
      Serial.println("ROBOT PAUSED — mission held");
      Serial.println("================================");
    }
    return true;
  }

  if (obstacleDetected) {
    /* confirm it really is gone before moving off */
    delay(100);
    updateUltrasonic();
    if (obstacleIsPresent()) return true;

    obstacleDetected = false;
    buzzerOff();
    lastDirection = 0;
    Serial.println(">>> OBSTACLE REMOVED — resuming");
  }

  return false;
}


/* ============================================================
 * LINE FOLLOWER   (verbatim ladder from the working sketch)
 * ============================================================
 */

void followLine()
{
  int s1 = readSensor1();
  int s2 = readSensor2();
  int s3 = readSensor3();
  int s4 = readSensor4();
  int s5 = readSensor5();

  /* ALL FIVE — very wide line */
  if (s1 && s2 && s3 && s4 && s5) {
    lastDirection = 0;
    moveForward();
    return;
  }

  /* CENTER 00100 */
  if (!s1 && !s2 && s3 && !s4 && !s5) {
    lastDirection = 0;
    moveForward();
    return;
  }

  /* CENTER-LEFT 01100 */
  if (!s1 && s2 && s3 && !s4 && !s5) {
    lastDirection = -1;
    correctLeft(GENTLE_TURN_TIME);
    return;
  }

  /* LEFT 01000 */
  if (!s1 && s2 && !s3 && !s4 && !s5) {
    lastDirection = -1;
    correctLeft(MEDIUM_TURN_TIME);
    return;
  }

  /* FAR LEFT 11000 */
  if (s1 && s2 && !s3 && !s4 && !s5) {
    lastDirection = -1;
    strongLeft();
    return;
  }

  /* EXTREME LEFT 10000 */
  if (s1 && !s2 && !s3 && !s4 && !s5) {
    lastDirection = -1;
    strongLeft();
    return;
  }

  /* CENTER-RIGHT 00110 */
  if (!s1 && !s2 && s3 && s4 && !s5) {
    lastDirection = 1;
    correctRight(GENTLE_TURN_TIME);
    return;
  }

  /* RIGHT 00010 */
  if (!s1 && !s2 && !s3 && s4 && !s5) {
    lastDirection = 1;
    correctRight(MEDIUM_TURN_TIME);
    return;
  }

  /* FAR RIGHT 00011 */
  if (!s1 && !s2 && !s3 && s4 && s5) {
    lastDirection = 1;
    strongRight();
    return;
  }

  /* EXTREME RIGHT 00001 */
  if (!s1 && !s2 && !s3 && !s4 && s5) {
    lastDirection = 1;
    strongRight();
    return;
  }

  /* LEFT COMBINATIONS */
  if (s1 || s2) {
    lastDirection = -1;
    correctLeft(MEDIUM_TURN_TIME);
    return;
  }

  /* RIGHT COMBINATIONS */
  if (s4 || s5) {
    lastDirection = 1;
    correctRight(MEDIUM_TURN_TIME);
    return;
  }

  /* LINE LOST 00000 */
  if (!s1 && !s2 && !s3 && !s4 && !s5) {
    if (lastDirection < 0) {
      leftMotorStop();
      rightMotorForward();
      watchDelay(LOST_LINE_TURN_TIME);
    }
    else if (lastDirection > 0) {
      leftMotorForward();
      rightMotorStop();
      watchDelay(LOST_LINE_TURN_TIME);
    }
    else {
      moveForward();
      watchDelay(LOST_LINE_TURN_TIME);
    }
    return;
  }

  /* FALLBACK */
  moveForward();
}


/* ============================================================
 * END OF LINE (room wall) — creep and confirm
 * ============================================================
 */

bool confirmEndOfLine()
{
  if (!allDark()) return false;

  Serial.println("   end of line? confirming...");

  unsigned long t0 = millis();
  while (millis() - t0 < LINE_END_CONFIRM_MS) {
    moveForward();
    watchDelay(CREEP_ON_MS);
    stopMotors();
    watchDelay(CREEP_OFF_MS);

    if (!allDark()) {
      Serial.println("   ...just a gap, carrying on");
      return false;
    }
  }

  stopMotors();
  return true;
}


/* ============================================================
 * MISSION HELPERS
 * ============================================================
 */

void enterState(MissionState s, const char* name)
{
  mission        = s;
  stateEnteredAt = millis();
  clearShapes();
  Serial.print(">>> STATE: ");
  Serial.println(name);
}

bool junctionCoolingDown()
{
  return (millis() - lastJunctionAt < JUNCTION_COOLDOWN_MS);
}

/* Drive dead straight over a junction we are not stopping at.
 * Never let the steering chase the crossbar. */
void crossStraight()
{
  moveForward();
  watchDelay(CROSS_STRAIGHT_MS);
  lastJunctionAt = millis();
  clearShapes();
}

const char* roomName(int r)
{
  if (r == 0) return "C (end of line)";
  if (r == 1) return "A (J1)";
  return "B (J2)";
}


/* ============================================================
 * MISSION CONTROL
 * ============================================================
 */

void startMission(int room, long orderId)
{
  targetRoom    = room;
  activeOrderId = orderId;
  orderActive   = (orderId > 0);

  robotRunning   = true;
  leftTeesSeen   = 0;
  lastDirection  = 0;
  endDetectArmed = false;
  obstacleDetected = false;
  lastJunctionAt = millis();
  buzzerOff();
  clearShapes();

  Serial.println();
  Serial.println("==============================================");
  Serial.print  (">>> MISSION START — Room ");
  Serial.println(roomName(room));
  Serial.println("==============================================");

  /* The rover starts sitting ON the Start bar, which reads
   * 11111. Drive off it before anything else, otherwise the
   * first shape sample is nonsense. */
  moveForward();
  watchDelay(500);
  clearShapes();
  lastJunctionAt = millis();

  enterState(OUTBOUND, "OUTBOUND");
}

void abortMission(const char* why)
{
  stopMotors();
  buzzerOff();
  robotRunning = false;
  mission = IDLE;
  clearShapes();
  Serial.print(">>> STOPPED: ");
  Serial.println(why);
}

void missionComplete()
{
  stopMotors();
  delay(300);

  Serial.println();
  Serial.println("================================");
  Serial.println("   START BAR REACHED — HOME     ");
  Serial.println("================================");

  /* spin around so the rover faces up the line for the next run */
  turnAround180();
  stopMotors();

  robotRunning = false;
  mission      = DONE;

  if (orderActive && activeOrderId > 0) {
    Serial.printf("Marking order #%ld delivered...\n", activeOrderId);
    updateOrderStatus(activeOrderId, "delivered");
    orderActive   = false;
    activeOrderId = 0;
  }

  Serial.println(">>> Ready for the next order.");
  mission = IDLE;
}


/* ── OUTBOUND — up the main line ──────────────────────────── */
void doOutbound()
{
  /* ---- Room C: no junction to take, just drive to the end ---- */
  if (targetRoom == 0) {

    if (sawLeftTee && !junctionCoolingDown()) {
      Serial.println("   [C] passing a side branch — straight through");
      crossStraight();
      return;
    }
    clearShapes();

    if (!endDetectArmed) {
      if (millis() - stateEnteredAt < BRANCH_ARM_MS) { followLine(); return; }
      endDetectArmed = true;
      Serial.println("   [C] end detection armed");
    }

    if (allDark() && confirmEndOfLine()) {
      Serial.println(">>> ROOM C REACHED");
      enterState(AT_ROOM, "AT_ROOM");
      return;
    }

    followLine();
    return;
  }

  /* ---- Rooms A and B: count LEFT tees only ---- */
  if (sawLeftTee && !junctionCoolingDown()) {
    leftTeesSeen++;
    lastJunctionAt = millis();
    clearShapes();

    Serial.printf("*** LEFT TEE %d  (target J%d) ***\n", leftTeesSeen, targetRoom);

    if (leftTeesSeen == targetRoom) {
      /* put the wheels over the junction, then pivot */
      stopMotors();
      delay(150);
      Serial.println("   target — nudging wheels onto the junction");
      moveForward();
      watchDelay(NUDGE_FORWARD_MS);
      stopMotors();
      delay(150);

      turnLeft90();

      lastDirection  = 0;
      endDetectArmed = false;
      enterState(ON_BRANCH, "ON_BRANCH");
    } else {
      Serial.println("   not the target — crossing straight");
      crossStraight();
    }
    return;
  }

  clearShapes();
  followLine();
}


/* ── ON_BRANCH — down the branch to the room ──────────────── */
void doOnBranch()
{
  if (!endDetectArmed) {
    if (millis() - stateEnteredAt < BRANCH_ARM_MS) { followLine(); return; }
    endDetectArmed = true;
    Serial.println("   [branch] end detection armed");
  }

  if (allDark() && confirmEndOfLine()) {
    Serial.println(">>> ROOM REACHED");
    enterState(AT_ROOM, "AT_ROOM");
    return;
  }

  clearShapes();
  followLine();
}


/* ── AT_ROOM — park and wait ──────────────────────────────── */
void doAtRoom()
{
  stopMotors();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 1000) {
    Serial.printf("   room wait %lus / %ds\n",
      (millis() - stateEnteredAt) / 1000, ROOM_WAIT_MS / 1000);
    lastMsg = millis();
  }

  if (millis() - stateEnteredAt < ROOM_WAIT_MS) return;

  Serial.println(">>> Wait done — turning around");
  turnAround180();
  lastDirection  = 0;
  lastJunctionAt = millis();

  if (targetRoom == 0) {
    /* Room C U-turns on the main line — already heading home */
    enterState(RETURNING, "RETURNING");
  } else {
    enterState(BRANCH_BACK, "BRANCH_BACK");
  }
}


/* ── BRANCH_BACK — back along the branch to the main line ─── */
void doBranchBack()
{
  /* The main vertical line crosses the branch, so it shows up as
   * a big bar. Either tee shape or the full bar means we are there. */
  if ((sawFullBar || sawLeftTee || sawRightTee || irSum() >= 4)
      && !junctionCoolingDown()) {

    Serial.println("*** MAIN LINE REACHED ***");
    stopMotors();
    delay(150);

    /* wheels onto the crossing before pivoting */
    moveForward();
    watchDelay(NUDGE_FORWARD_MS);
    stopMotors();
    delay(150);

    /* turn right to face down the line toward Start */
    turnRight90();

    lastDirection  = 0;
    lastJunctionAt = millis();
    enterState(RETURNING, "RETURNING");
    return;
  }

  clearShapes();
  followLine();
}


/* ── RETURNING — down the main line to the Start bar ───────── */
void doReturning()
{
  bool armed = (millis() - stateEnteredAt >= RETURN_ARM_MS);

  if (armed && sawFullBar) {
    /* Black on BOTH outer sensors. On this track only the Start
     * bar does that, so no counting is needed. */
    Serial.println("*** START BAR — black on both sides ***");
    stopMotors();
    delay(100);

    /* roll the axle onto the bar before spinning */
    moveForward();
    watchDelay(NUDGE_FORWARD_MS);
    stopMotors();

    missionComplete();
    return;
  }

  if (armed && sawRightTee && !junctionCoolingDown()) {
    Serial.println("   side branch (J1/J2) on the way home — crossing, NOT home");
    crossStraight();
    return;
  }

  clearShapes();
  followLine();
}


/* ============================================================
 * SENSOR MONITOR
 * ============================================================
 */

void sensorMonitor()
{
  Serial.println();
  Serial.println("========== SENSOR MONITOR ==========");
  Serial.println("Press 's' to exit.");
  Serial.println();

  while (true) {
    printSensors();
    delay(200);

    if (Serial.available()) {
      char c = Serial.read();
      if (c == 's' || c == 'S') {
        stopMotors();
        Serial.println("Sensor monitor stopped.");
        return;
      }
    }
  }
}


/* ============================================================
 * WIFI
 * ============================================================
 */

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  else
    Serial.println("\nWiFi FAILED — manual mode only");
}


/* ============================================================
 * SUPABASE
 * ============================================================
 */

bool httpRequest(const String& method, const String& url,
                 const String& body, String& response)
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi down");
    return false;
  }

  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  http.addHeader("apikey",        SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type",  "application/json");
  if (method == "PATCH") http.addHeader("Prefer", "return=minimal");

  int code;
  if      (method == "GET")   code = http.GET();
  else if (method == "PATCH") code = http.PATCH(body);
  else if (method == "POST")  code = http.POST(body);
  else { http.end(); return false; }

  bool ok = false;
  if (code > 0) {
    if (method == "GET" || method == "POST") response = http.getString();
    else                                     response = "";
    Serial.printf("[HTTP] %s %d\n", method.c_str(), code);
    ok = (code >= 200 && code < 300);
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return ok;
}

Order fetchNextPendingOrder()
{
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

bool updateOrderStatus(long orderId, const char* newStatus)
{
  String url = String(SUPABASE_URL) + "/rest/v1/orders?id=eq." + String(orderId);
  StaticJsonDocument<128> bodyDoc;
  bodyDoc["status"] = newStatus;
  String body, resp;
  serializeJson(bodyDoc, body);
  bool ok = httpRequest("PATCH", url, body, resp);
  Serial.println(ok ? String("Order status -> ") + newStatus
                    : String("Status update FAILED"));
  return ok;
}

String normalizeRoomCode(const String& raw)
{
  String r = raw;
  r.trim();
  r.toUpperCase();
  if (r == "A" || r == "B" || r == "C") return r;

  for (int i = (int)r.length() - 1; i >= 0; --i) {
    char c = r[i];
    if (c >= 'A' && c <= 'Z') {
      if (c == 'A' || c == 'B' || c == 'C') return String(c);
      break;
    }
  }
  return "";
}


/* ============================================================
 * HELP
 * ============================================================
 */

void printHelp()
{
  Serial.println();
  Serial.println("==========================================");
  Serial.println("        HOSPITAL ROVER — NO PWM");
  Serial.println("==========================================");
  Serial.println("A / B / C = choose room");
  Serial.println("g = GO          s = STOP");
  Serial.println("i = IR snapshot w = IR monitor");
  Serial.println("d = distance    t = turn test");
  Serial.println("h = help");
  Serial.print  ("Target now: Room ");
  Serial.println(roomName(targetRoom));
  Serial.println("==========================================");
  Serial.println();
}


/* ============================================================
 * SETUP
 * ============================================================
 */

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(LEFT_IN1,  OUTPUT);
  pinMode(LEFT_IN2,  OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);
  pinMode(SENSOR_3, INPUT);
  pinMode(SENSOR_4, INPUT);
  pinMode(SENSOR_5, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  stopMotors();

  robotRunning     = false;
  mission          = IDLE;
  obstacleDetected = false;
  lastDirection    = 0;

  Serial.println();
  Serial.println("==============================================");
  Serial.println("   ESP32 HOSPITAL ROVER — NO PWM BUILD");
  Serial.println("==============================================");
  Serial.println("PWM       : DISABLED (ENA/ENB on 5V)");
  Serial.println("Junctions : by shape, not by counting");
  Serial.println("            11100 = left tee");
  Serial.println("            00111 = right tee");
  Serial.println("            11111 = START bar = home");
  Serial.println("Obstacle  : 30 cm");
  Serial.println("Buzzer    : GPIO 4");
  Serial.println("==============================================");

  printHelp();
  connectWiFi();
}


/* ============================================================
 * LOOP
 * ============================================================
 */

void loop()
{
  /* ---- serial commands ---- */
  if (Serial.available()) {
    char command = Serial.read();

    switch (command) {

      case 'A': case 'a':
        targetRoom = 1;
        Serial.println(">>> Target: Room A (J1) — press 'g'");
        break;

      case 'B': case 'b':
        targetRoom = 2;
        Serial.println(">>> Target: Room B (J2) — press 'g'");
        break;

      case 'C': case 'c':
        targetRoom = 0;
        Serial.println(">>> Target: Room C (end of line) — press 'g'");
        break;

      case 'g': case 'G':
        startMission(targetRoom, 0);
        break;

      case 's': case 'S':
        abortMission("manual stop");
        orderActive   = false;
        activeOrderId = 0;
        break;

      case 'i': case 'I':
        printSensors();
        break;

      case 'd': case 'D':
        printDistance();
        break;

      case 'w': case 'W':
        if (!robotRunning) sensorMonitor();
        else Serial.println("Stop the robot first with 's'.");
        break;

      case 't': case 'T':
        Serial.println(">>> Turn test — pivot left until realigned");
        turnLeft90();
        break;

      case 'h': case 'H':
        printHelp();
        break;

      case '\n': case '\r': case ' ':
        break;

      default:
        Serial.print("Unknown command: ");
        Serial.println(command);
        break;
    }
  }

  /* ---- idle: poll Supabase for a new order ---- */
  if (!robotRunning) {
    stopMotors();

    unsigned long now = millis();
    if (now - lastPoll < POLL_INTERVAL_MS) return;
    lastPoll = now;

    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[poll] checking for pending orders...");
    Order o = fetchNextPendingOrder();
    if (!o.valid) {
      Serial.println("[poll] none.");
      return;
    }

    String code = normalizeRoomCode(o.roomCode);
    Serial.printf("[poll] order #%ld -> room '%s'\n", o.id, code.c_str());

    int room = -1;
    if      (code == "A") room = 1;
    else if (code == "B") room = 2;
    else if (code == "C") room = 0;

    if (room == -1) {
      Serial.println("[poll] unknown room code — skipping");
      return;
    }

    if (!updateOrderStatus(o.id, "in_transit"))
      Serial.println("[poll] in_transit update failed — starting anyway");

    startMission(room, o.id);
    return;
  }

  /* ---- running ---- */

  /* sample shapes every pass, on top of what watchDelay catches */
  sampleShapes();

  /* obstacle overrides everything while travelling */
  if (handleObstacle()) return;

  switch (mission) {
    case OUTBOUND:    doOutbound();    break;
    case ON_BRANCH:   doOnBranch();    break;
    case AT_ROOM:     doAtRoom();      break;
    case BRANCH_BACK: doBranchBack();  break;
    case RETURNING:   doReturning();   break;
    default:          stopMotors();    break;
  }
}
