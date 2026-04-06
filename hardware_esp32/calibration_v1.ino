/*
 * ============================================================
 *  MOTOR CALIBRATION SKETCH v2
 *  Fixed for "Newline" line ending in Serial Monitor
 * ============================================================
 *  COMMANDS (type in Serial Monitor and press Send/Enter):
 *   f → Both motors FORWARD
 *   b → Both motors BACKWARD
 *   l → Left motor only
 *   r → Right motor only
 *   s → STOP
 *   + → Speed up by 10
 *   - → Speed down by 10
 * ============================================================
 */

#define IN1  27
#define IN2  26
#define IN3  25
#define IN4  33
#define ENA  14
#define ENB  12

#define PWM_FREQ 1000
#define PWM_RES  8

int spd = 150;

void motorSetup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
}

void leftMotor(int s) {
  if (s >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else        { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); s = -s; }
  ledcWrite(ENA, constrain(s, 0, 255));
}

void rightMotor(int s) {
  if (s >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else        { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); s = -s; }
  ledcWrite(ENB, constrain(s, 0, 255));
}

void stopAll() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA, 0); ledcWrite(ENB, 0);
}

void processCommand(char cmd) {
  switch (cmd) {
    case 'f':
      leftMotor(spd); rightMotor(spd);
      Serial.println(">>> FORWARD");
      Serial.println("    Both wheels should spin FORWARD (robot moves away from you)");
      Serial.println("    If robot moves TOWARD you: swap IN1<->IN2 AND IN3<->IN4");
      Serial.println("    If only LEFT goes backward: swap IN1<->IN2");
      Serial.println("    If only RIGHT goes backward: swap IN3<->IN4");
      break;

    case 'b':
      leftMotor(-spd); rightMotor(-spd);
      Serial.println(">>> BACKWARD");
      break;

    case 'l':
      leftMotor(spd); rightMotor(0);
      Serial.println(">>> LEFT motor only — watch left wheel direction");
      break;

    case 'r':
      leftMotor(0); rightMotor(spd);
      Serial.println(">>> RIGHT motor only — watch right wheel direction");
      break;

    case 's':
      stopAll();
      Serial.println(">>> STOPPED");
      break;

    case '+':
      spd = min(spd + 10, 255);
      Serial.print(">>> Speed: "); Serial.println(spd);
      break;

    case '-':
      spd = max(spd - 10, 50);
      Serial.print(">>> Speed: "); Serial.println(spd);
      break;

    // ignore newline, carriage return, spaces
    case '\n': case '\r': case ' ': break;

    default:
      Serial.print("Unknown command: ");
      Serial.println(cmd);
      Serial.println("Valid: f b l r s + -");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // give Serial Monitor time to open
  motorSetup();
  stopAll();
  Serial.println("==============================");
  Serial.println("  MOTOR CALIBRATION READY");
  Serial.println("==============================");
  Serial.println("f=forward  b=backward");
  Serial.println("l=left only  r=right only");
  Serial.println("s=stop  +=faster  -=slower");
  Serial.print("Current speed: "); Serial.println(spd);
  Serial.println("------------------------------");
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    processCommand(cmd);
    delay(10);  // small delay to flush buffer cleanly
  }
}
