/*
 * ============================================================
 *  MOTOR + LINE FOLLOW CALIBRATION v6
 *  Added live correction multiplier tuning
 * ============================================================
 *  COMMANDS:
 *   f → Both forward
 *   s → Stop
 *   + → Base speed +5
 *   - → Base speed -5
 *   ] → Right offset +5
 *   [ → Right offset -5
 *   > → Correction multiplier +5  (more aggressive turns)
 *   < → Correction multiplier -5  (less aggressive turns)
 *   i → IR values once
 *   w → Watch IR continuously
 *   g → Start line following
 *   p → Print current settings
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

int baseSpeed    = 125;  // your safe minimum
int rightOffset  = 120;  // your found sweet spot
int corrMult     = 50;   // start higher than 35, tune with > <

bool watchingIR    = false;
bool lineFollowing = false;

struct IRSensor { bool s1,s2,s3,s4,s5; };

IRSensor readIR();
void printIR();
void doLineFollow();
void stopAll();
void setMotorLeft(int spd);
void setMotorRight(int spd);
void processCommand(char cmd);
void printSettings();

void motorSetup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
}

void setMotorLeft(int spd) {
  if (spd >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else          { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); spd = -spd; }
  ledcWrite(ENA, constrain(spd, 0, 255));
}

void setMotorRight(int spd) {
  if (spd >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else          { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); spd = -spd; }
  ledcWrite(ENB, constrain(spd, 0, 255));
}

void stopAll() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA, 0); ledcWrite(ENB, 0);
  watchingIR = false; lineFollowing = false;
}

IRSensor readIR() {
  IRSensor d;
  d.s1 = !digitalRead(IR_S1);
  d.s2 = !digitalRead(IR_S2);
  d.s3 = !digitalRead(IR_S3);
  d.s4 = !digitalRead(IR_S4);
  d.s5 = !digitalRead(IR_S5);
  return d;
}

void printIR() {
  IRSensor ir = readIR();
  Serial.printf("S1:%d S2:%d S3:%d S4:%d S5:%d   [%s%s%s%s%s]\n",
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5,
    ir.s1?"█":" ",ir.s2?"█":" ",ir.s3?"█":" ",
    ir.s4?"█":" ",ir.s5?"█":" ");
}

void printSettings() {
  Serial.println("========== CURRENT SETTINGS ==========");
  Serial.printf("  Base speed    : %d\n", baseSpeed);
  Serial.printf("  Right offset  : %d\n", rightOffset);
  Serial.printf("  Correction    : %d\n", corrMult);
  Serial.printf("  Effective L   : %d\n", baseSpeed);
  Serial.printf("  Effective R   : %d\n", baseSpeed + rightOffset);
  Serial.println("=======================================");
  Serial.println("  Once happy with 'g' test, note these");
  Serial.println("  values for the main firmware!");
}

void doLineFollow() {
  IRSensor ir = readIR();
  int error = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);

  // When all sensors are off (lost line), keep last direction
  // by using a higher correction — don't just go straight
  int leftSpeed  = baseSpeed           - (error * corrMult);
  int rightSpeed = baseSpeed + rightOffset + (error * corrMult);

  setMotorLeft(constrain(leftSpeed,   0, 255));
  setMotorRight(constrain(rightSpeed, 0, 255));

  Serial.printf("err:%3d  L:%4d  R:%4d  [%d%d%d%d%d]\n",
    error, leftSpeed, rightSpeed,
    ir.s1,ir.s2,ir.s3,ir.s4,ir.s5);
}

void processCommand(char cmd) {
  switch(cmd) {
    case 'f':
      stopAll(); lineFollowing=false; watchingIR=false;
      setMotorLeft(baseSpeed);
      setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
      Serial.printf(">>> FORWARD  L:%d  R:%d\n", baseSpeed, baseSpeed+rightOffset);
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case '+':
      baseSpeed = min(baseSpeed+5, 220);
      Serial.printf(">>> Base speed: %d\n", baseSpeed);
      break;

    case '-':
      baseSpeed = max(baseSpeed-5, 80);
      Serial.printf(">>> Base speed: %d\n", baseSpeed);
      break;

    case ']':
      rightOffset = min(rightOffset+5, 200);
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case '[':
      rightOffset = max(rightOffset-5, 0);
      Serial.printf(">>> Right offset: %d\n", rightOffset);
      break;

    case '>':
      corrMult = min(corrMult+5, 150);
      Serial.printf(">>> Correction multiplier: %d\n", corrMult);
      Serial.println("    Higher = more aggressive steering");
      break;

    case '<':
      corrMult = max(corrMult-5, 10);
      Serial.printf(">>> Correction multiplier: %d\n", corrMult);
      break;

    case 'i':
      printIR();
      Serial.println("    1=black  0=white   S1=leftmost  S5=rightmost");
      break;

    case 'w':
      stopAll(); watchingIR=true;
      Serial.println(">>> Watching IR... (s to stop)");
      break;

    case 'g':
      watchingIR=false; lineFollowing=true;
      Serial.println(">>> Line following ON (s to stop)");
      printSettings();
      Serial.println("  Tune with: > (more aggressive)  < (less)");
      Serial.println("             + (faster)  - (slower)");
      break;

    case 'p':
      printSettings();
      break;

    case '\n': case '\r': case ' ': break;

    default:
      Serial.printf("Unknown: '%c'\n", cmd);
      Serial.println("Commands: f s + - ] [ > < i w g p");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  motorSetup();
  stopAll();
  pinMode(IR_S1, INPUT); pinMode(IR_S2, INPUT);
  pinMode(IR_S3, INPUT); pinMode(IR_S4, INPUT);
  pinMode(IR_S5, INPUT);

  Serial.println("==========================================");
  Serial.println("   MOTOR + LINE FOLLOW CALIBRATION v6");
  Serial.println("==========================================");
  printSettings();
  Serial.println();
  Serial.println("GOAL: type 'g', put on line, tune until");
  Serial.println("      robot follows the line consistently");
  Serial.println("  '>' = more aggressive correction");
  Serial.println("  '<' = less aggressive correction");
  Serial.println("  '+' = faster   '-' = slower");
  Serial.println("==========================================");
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);
  }
  if (watchingIR)    { printIR();      delay(150); }
  if (lineFollowing) { doLineFollow(); delay(20);  }
}
