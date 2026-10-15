// ESP32 + L298N
// 2 seconds FORWARD
// 2 seconds BACKWARD
// Repeat

#define LEFT_IN1   27
#define LEFT_IN2   26

#define RIGHT_IN1  25
#define RIGHT_IN2  33

void setup()
{
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);

  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  // Start stopped
  stopMotors();
}

void moveForward()
{
  // Left motor forward
  digitalWrite(LEFT_IN1, HIGH);
  digitalWrite(LEFT_IN2, LOW);

  // Right motor forward
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
}

void moveBackward()
{
  // Left motor backward
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, HIGH);

  // Right motor backward
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, HIGH);
}

void stopMotors()
{
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);

  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
}

void loop()
{
  // FORWARD for 2 seconds
  moveForward();
  delay(2000);

  // STOP
  stopMotors();
  delay(500);

  // BACKWARD for 2 seconds
  moveBackward();
  delay(2000);

  // STOP
  stopMotors();
  delay(500);
}
