/*
 * ============================================================
 *  LINE FOLLOW v19 — Arrival = Line End Detection
 *
 *  WHAT'S NEW vs v18:
 *   - After turning into branch, robot follows branch line
 *   - "Arrived at room" = ALL sensors go dark (line ends)
 *   - Stops, waits 10s, U-turns, follows back to main line
 *   - On main line, counts junctions to reach home and stops
 *
 *  YOUR TRACK LAYOUT (from image):
 *   Robot starts top, moves DOWN the vertical line
 *   Junction A = 1st horizontal branch (left turn)
 *   Junction B = 2nd horizontal branch (left turn)
 *   C          = end of vertical line
 *
 *  STATES:
 *   LINE_FOLLOWING  → PD follow down main line
 *   DRIVING_PAST    → drive past junction center before turning
 *   ON_BRANCH       → following branch line toward room
 *   AT_ROOM         → line ended, waiting 10s
 *   UTURNING        → spinning 180° to face back
 *   RETURNING       → PD follow back to home
 *   ARRIVED_HOME    → mission complete, stopped
 *   SEARCHING       → lost line, scanning
 * ============================================================
 *  COMMANDS:
 *   A → Room 1  |  B → Room 2  |  C → Room 3 (end of main)
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
int   uturnMs              = 700;   // min ms to spin for U-turn before accepting line

// How many ms of all-dark = "line truly ended" (not just a gap)
#define LINE_END_CONFIRM_MS  300

#define JUNCTION_FRAMES_NEEDED  3
#define JUNCTION_COOLDOWN_MS  800

// ── State machine ─────────────────────────────────────────────
enum RobotState {
  LINE_FOLLOWING,
  DRIVING_PAST,
  ON_BRANCH,
  AT_ROOM,
  UTURNING,
  RETURNING,
  ARRIVED_HOME,
  SEARCHING
};
RobotState state        = LINE_FOLLOWING;
RobotState stateBeforeSearch = LINE_FOLLOWING; // to restore after search

// ── Journey variables ─────────────────────────────────────────
int  targetJunction        = 1;
int  junctionsSeen         = 0;
int  returnJunctionsNeeded = 0;
int  returnJunctionsSeen   = 0;
int  junctionFrames        = 0;

unsigned long lastJunctionTime  = 0;
unsigned long drivePastStart    = 0;
unsigned long roomArrivalTime   = 0;
unsigned long uturnStart        = 0;
unsigned long lineEndStart      = 0;   // when all sensors first went dark on branch
bool          lineEndTimerActive = false;

// ── Other globals ─────────────────────────────────────────────
bool lineFollowing = false;
bool watchingIR    = false;
int  lastError     = 0;
int  searchDir     = 1;

// ── Structs / forward declarations ───────────────────────────
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
void doUturn();
bool checkJunction();
void doPDFollow();
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
  stateBeforeSearch     = LINE_FOLLOWING;
  junctionsSeen         = 0;
  junctionFrames        = 0;
  returnJunctionsSeen   = 0;
  returnJunctionsNeeded = 0;
  lastJunctionTime      = 0;
  lineEndTimerActive    = false;
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
  Serial.printf("  lineEndConfirm       : %dms\n", LINE_END_CONFIRM_MS);
  Serial.printf("  targetJunction       : %d %s\n", targetJunction,
    targetJunction==0?"(C-straight)":
    targetJunction==1?"(A-Room1)":"(B-Room2)");
  Serial.println("==========================");
}

// ── Turn left onto branch then switch to ON_BRANCH ───────────
void executeTurnLeft() {
  Serial.println(">> TURNING LEFT onto branch...");
  stopMotorsOnly();
  delay(100);

  // Spin until branch line found (max 2s)
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
  delay(150);

  // Now follow branch until line ends
  lastError          = 0;
  lineEndTimerActive = false;
  state              = ON_BRANCH;
  Serial.println(">> ON_BRANCH — following until line ends");
}

// ── U-turn ────────────────────────────────────────────────────
void startUturn() {
  Serial.println(">> U-TURN starting...");
  stopMotorsOnly();
  delay(200);
  state      = UTURNING;
  uturnStart = millis();
}

void doUturn() {
  setMotorLeft(-turnSpeed);
  setMotorRight(constrain(turnSpeed+rightOffset,0,255));

  if (millis() - uturnStart < (unsigned long)uturnMs) return;

  IRSensor ir = readIR();
  if (ir.s3 || (ir.s2 && ir.s3) || (ir.s3 && ir.s4)) {
    Serial.println(">> U-turn complete — line found");
    stopMotorsOnly();
    delay(100);
    lastError             = 0;
    junctionFrames        = 0;
    lineEndTimerActive    = false;
    // Cooldown so we don't immediately re-count the branch junction
    lastJunctionTime      = millis();
    returnJunctionsSeen   = 0;
    returnJunctionsNeeded = targetJunction;
    state                 = RETURNING;
    Serial.printf(">> RETURNING — need %d junction(s) to reach home\n",
      returnJunctionsNeeded);
    return;
  }

  // Safety timeout
  if (millis() - uturnStart > 3000) {
    Serial.println(">> U-turn timeout — forcing RETURNING");
    stopMotorsOnly();
    delay(100);
    lastError             = 0;
    lastJunctionTime      = millis();
    lineEndTimerActive    = false;
    returnJunctionsSeen   = 0;
    returnJunctionsNeeded = targetJunction;
    state                 = RETURNING;
  }
}

// ── PD follow (shared) ────────────────────────────────────────
void doPDFollow() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

  if (sum == 0) return; // caller handles line-lost

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

// ── Junction detector with cooldown ──────────────────────────
// Returns true only when a NEW junction is freshly confirmed
bool checkJunction() {
  IRSensor ir = readIR();
  int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

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
  } else {
    junctionFrames = 0;
    return false;
  }
}

// ── Main drive function ───────────────────────────────────────
void doLineFollow() {

  // ── AT ROOM: waiting 10s ──────────────────────────────────
  if (state == AT_ROOM) {
    unsigned long elapsed = millis() - roomArrivalTime;
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      Serial.printf("  at room... %lus / %ds\n", elapsed/1000, roomWaitMs/1000);
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

  // ── DRIVING PAST junction before branch turn ───────────────
  if (state == DRIVING_PAST) {
    setMotorLeft(baseSpeed);
    setMotorRight(constrain(baseSpeed+rightOffset,0,255));
    if (millis() - drivePastStart >= (unsigned long)forwardAfterJunction) {
      Serial.println(">> Past junction centre — turning now");
      executeTurnLeft();
    }
    return;
  }

  // ── ON BRANCH: follow until line ends ─────────────────────
  if (state == ON_BRANCH) {
    IRSensor ir = readIR();
    int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

    if (sum == 0) {
      // All sensors dark — start/continue line-end confirm timer
      if (!lineEndTimerActive) {
        lineEndTimerActive = true;
        lineEndStart       = millis();
        Serial.println("  branch end? confirming...");
      }
      // Keep coasting slowly forward while confirming
      setMotorLeft(baseSpeed - 30);
      setMotorRight(constrain((baseSpeed - 30) + rightOffset, 0, 255));

      if (millis() - lineEndStart >= LINE_END_CONFIRM_MS) {
        // Confirmed: line has ended = room wall reached
        stopMotorsOnly();
        state              = AT_ROOM;
        roomArrivalTime    = millis();
        lineEndTimerActive = false;
        Serial.printf(">> ROOM REACHED (line ended) — waiting %ds\n",
          roomWaitMs/1000);
      }
    } else {
      // Line still visible — reset end timer and keep following
      if (lineEndTimerActive) {
        Serial.println("  false alarm — line back");
        lineEndTimerActive = false;
      }
      doPDFollow();
    }
    return;
  }

  // ── SEARCHING ─────────────────────────────────────────────
  if (state == SEARCHING) {
    IRSensor ir = readIR();
    int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
    if (sum > 0) {
      Serial.println("Line found — resuming");
      state = stateBeforeSearch;
    } else {
      if (searchDir > 0) { setMotorLeft(searchSpeed); setMotorRight(-searchSpeed); }
      else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed); }
    }
    return;
  }

  // ── OUTBOUND: LINE_FOLLOWING ───────────────────────────────
  if (state == LINE_FOLLOWING) {
    IRSensor ir = readIR();
    int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

    // Line lost on main line
    if (sum == 0) {
      stateBeforeSearch = LINE_FOLLOWING;
      state             = SEARCHING;
      searchDir         = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST on main → searching %s\n", searchDir>0?"RIGHT":"LEFT");
      return;
    }

    if (targetJunction == 0) {
      // Mode C: straight to end
      doPDFollow();
      return;
    }

    if (checkJunction()) {
      junctionsSeen++;
      Serial.printf("*** OUTBOUND JUNCTION %d ***\n", junctionsSeen);
      if (junctionsSeen == targetJunction) {
        Serial.printf(">> Target! Driving past %dms then turning\n",
          forwardAfterJunction);
        state          = DRIVING_PAST;
        drivePastStart = millis();
      } else {
        Serial.printf(">> Junction %d: not target, straight\n", junctionsSeen);
      }
      return;
    }

    doPDFollow();
    return;
  }

  // ── RETURNING: follow back, count junctions to home ───────
  if (state == RETURNING) {
    IRSensor ir = readIR();
    int sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;

    if (sum == 0) {
      stateBeforeSearch = RETURNING;
      state             = SEARCHING;
      searchDir         = (lastError >= 0) ? 1 : -1;
      Serial.printf("LOST on return → searching %s\n", searchDir>0?"RIGHT":"LEFT");
      return;
    }

    if (checkJunction()) {
      returnJunctionsSeen++;
      Serial.printf("*** RETURN JUNCTION %d / %d ***\n",
        returnJunctionsSeen, returnJunctionsNeeded);

      if (returnJunctionsSeen >= returnJunctionsNeeded) {
        // Coast past the home junction marker then stop
        Serial.println(">> HOME junction — stopping");
        unsigned long coast = millis();
        while (millis() - coast < 300) {
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
        return;
      }
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
      Serial.println(">>> Mode A: Room 1 — type 'g'");
      break;
    case 'B': case 'b':
      targetJunction=2;
      Serial.println(">>> Mode B: Room 2 — type 'g'");
      break;
    case 'C': case 'c':
      targetJunction=0;
      Serial.println(">>> Mode C: Room 3 (straight) — type 'g'");
      break;
    case 'g':
      lineFollowing         = true;
      state                 = LINE_FOLLOWING;
      stateBeforeSearch     = LINE_FOLLOWING;
      lastError             = 0;
      junctionsSeen         = 0;
      junctionFrames        = 0;
      returnJunctionsSeen   = 0;
      returnJunctionsNeeded = 0;
      lastJunctionTime      = 0;
      lineEndTimerActive    = false;
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
  Serial.println("  LINE FOLLOW v19 — Arrival = Line End       ");
  Serial.println("==============================================");
  Serial.println("  A=Room1  B=Room2  C=Room3  then 'g'");
  Serial.println("  Stops when branch line ends (room wall)");
  Serial.println("  Waits 10s, U-turns, returns home");
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
