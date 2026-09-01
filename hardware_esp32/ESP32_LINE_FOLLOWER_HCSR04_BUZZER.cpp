/*
 * ============================================================
 *      ESP32 LINE FOLLOWER + HC-SR04 OBSTACLE DETECTION + Buzzer
 * ============================================================
 *
 * CURRENT STAGE:
 *
 *   1. Follow line normally
 *   2. Detect obstacle <= 30 cm
 *   3. Stop immediately
 *   4. Keep checking distance
 *   5. Resume line following when obstacle is removed
 *
 * NO OBSTACLE BYPASS YET.
 *
 *
 * ============================================================
 * HARDWARE
 * ============================================================
 *
 * L298N:
 *
 *   ENA -> 5V
 *   ENB -> 5V
 *
 * NO PWM.
 *
 *
 * LEFT MOTOR:
 *
 *   IN1 -> GPIO 27
 *   IN2 -> GPIO 26
 *
 *
 * RIGHT MOTOR:
 *
 *   IN3 -> GPIO 25
 *   IN4 -> GPIO 33
 *
 *
 * IR SENSORS:
 *
 *   S1 -> GPIO 34
 *   S2 -> GPIO 35
 *   S3 -> GPIO 32
 *   S4 -> GPIO 18
 *   S5 -> GPIO 19
 *
 *
 * HC-SR04:
 *
 *   TRIG -> GPIO 5
 *   ECHO -> GPIO 23
 *
 *
 * HC-SR04 ECHO MUST BE LEVEL-SHIFTED / VOLTAGE-DIVIDED
 * BEFORE CONNECTING TO ESP32 GPIO.
 *
 *
 * ============================================================
 * SENSOR LOGIC
 * ============================================================
 *
 * IR:
 *
 *   LOW  = LINE
 *   HIGH = NO LINE
 *
 *
 * HC-SR04:
 *
 *   Distance <= 30 cm
 *   = OBSTACLE
 *
 *
 * ============================================================
 * SERIAL
 * ============================================================
 *
 *   g = START
 *   s = STOP
 *   i = IR snapshot
 *   d = distance reading
 *   w = IR monitor
 *   h = help
 *
 * ============================================================
 */


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
 * HC-SR04
 * ============================================================
 */

#define TRIG_PIN   5
#define ECHO_PIN   23

// ============================================================
// ACTIVE BUZZER
// ============================================================
#define BUZZER_PIN  4


/* ============================================================
 * OBSTACLE SETTINGS
 * ============================================================
 */

#define OBSTACLE_DISTANCE_CM 30.0


/*
 * Distance readings are taken every 60 ms.
 *
 * This prevents the ultrasonic sensor from being hammered
 * continuously while still giving fast obstacle detection.
 */

#define ULTRASONIC_INTERVAL_MS 60


/* ============================================================
 * LINE FOLLOW PARAMETERS
 * ============================================================
 */

#define GENTLE_TURN_TIME   12

#define MEDIUM_TURN_TIME   22

#define STRONG_TURN_TIME   38

#define LOST_LINE_TURN_TIME 35


/* ============================================================
 * ROBOT STATE
 * ============================================================
 */

bool robotRunning = false;


/*
 * True when obstacle has been detected.
 */

bool obstacleDetected = false;


/*
 * Last direction in which the line was seen.
 *
 * -1 = LEFT
 *  0 = CENTER
 * +1 = RIGHT
 */

int lastDirection = 0;


/* ============================================================
 * ULTRASONIC VARIABLES
 * ============================================================
 */

float currentDistance = -1.0;

unsigned long lastUltrasonicRead = 0;

// ============================================================
// BUZZER STATE
// ============================================================
// Active buzzer: HIGH = ON, LOW = OFF
bool buzzerState = false;
unsigned long lastBuzzerChange = 0;
#define BUZZER_ON_TIME   200
#define BUZZER_OFF_TIME  300


/* ============================================================
 * MOTOR FUNCTIONS
 * ============================================================
 */


/*
 * ------------------------------------------------------------
 * LEFT MOTOR FORWARD
 * ------------------------------------------------------------
 */

void leftMotorForward()
{
  digitalWrite(LEFT_IN1, HIGH);
  digitalWrite(LEFT_IN2, LOW);
}


/*
 * ------------------------------------------------------------
 * LEFT MOTOR BACKWARD
 * ------------------------------------------------------------
 */

void leftMotorBackward()
{
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, HIGH);
}


/*
 * ------------------------------------------------------------
 * RIGHT MOTOR FORWARD
 * ------------------------------------------------------------
 */

void rightMotorForward()
{
  digitalWrite(RIGHT_IN1, HIGH);
  digitalWrite(RIGHT_IN2, LOW);
}


/*
 * ------------------------------------------------------------
 * RIGHT MOTOR BACKWARD
 * ------------------------------------------------------------
 */

void rightMotorBackward()
{
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, HIGH);
}


/*
 * ------------------------------------------------------------
 * LEFT MOTOR STOP
 * ------------------------------------------------------------
 */

void leftMotorStop()
{
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
}


/*
 * ------------------------------------------------------------
 * RIGHT MOTOR STOP
 * ------------------------------------------------------------
 */

void rightMotorStop()
{
  digitalWrite(RIGHT_IN1, LOW);
  digitalWrite(RIGHT_IN2, LOW);
}


/*
 * ------------------------------------------------------------
 * BOTH MOTORS FORWARD
 * ------------------------------------------------------------
 */

void moveForward()
{
  leftMotorForward();
  rightMotorForward();
}


/*
 * ------------------------------------------------------------
 * BOTH MOTORS BACKWARD
 * ------------------------------------------------------------
 */

void moveBackward()
{
  leftMotorBackward();
  rightMotorBackward();
}


/*
 * ------------------------------------------------------------
 * STOP BOTH MOTORS
 * ------------------------------------------------------------
 */

void stopMotors()
{
  leftMotorStop();
  rightMotorStop();
}


/* ============================================================
 * LINE CORRECTION
 * ============================================================
 */


/*
 * ------------------------------------------------------------
 * GENTLE LEFT
 * ------------------------------------------------------------
 */

void correctLeft(int correctionTime)
{
  leftMotorStop();

  rightMotorForward();

  delay(correctionTime);

  moveForward();
}


/*
 * ------------------------------------------------------------
 * GENTLE RIGHT
 * ------------------------------------------------------------
 */

void correctRight(int correctionTime)
{
  leftMotorForward();

  rightMotorStop();

  delay(correctionTime);

  moveForward();
}


/*
 * ------------------------------------------------------------
 * STRONG LEFT
 * ------------------------------------------------------------
 */

void strongLeft()
{
  leftMotorBackward();

  rightMotorForward();

  delay(STRONG_TURN_TIME);

  moveForward();
}


/*
 * ------------------------------------------------------------
 * STRONG RIGHT
 * ------------------------------------------------------------
 */

void strongRight()
{
  leftMotorForward();

  rightMotorBackward();

  delay(STRONG_TURN_TIME);

  moveForward();
}


/* ============================================================
 * IR SENSOR FUNCTIONS
 * ============================================================
 */


/*
 * Active LOW:
 *
 * LOW = line
 *
 * Therefore !digitalRead()
 */

int readSensor1()
{
  return !digitalRead(SENSOR_1);
}


int readSensor2()
{
  return !digitalRead(SENSOR_2);
}


int readSensor3()
{
  return !digitalRead(SENSOR_3);
}


int readSensor4()
{
  return !digitalRead(SENSOR_4);
}


int readSensor5()
{
  return !digitalRead(SENSOR_5);
}


/* ============================================================
 * PRINT IR
 * ============================================================
 */

void printSensors()
{
  int s1 = readSensor1();

  int s2 = readSensor2();

  int s3 = readSensor3();

  int s4 = readSensor4();

  int s5 = readSensor5();


  Serial.print("S1=");

  Serial.print(s1);

  Serial.print("  S2=");

  Serial.print(s2);

  Serial.print("  S3=");

  Serial.print(s3);

  Serial.print("  S4=");

  Serial.print(s4);

  Serial.print("  S5=");

  Serial.println(s5);
}


/* ============================================================
 * HC-SR04 DISTANCE
 * ============================================================
 */

float measureDistanceCM()
{
  /*
   * Make sure TRIG starts LOW.
   */

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  delayMicroseconds(2);


  /*
   * Send 10 us trigger pulse.
   */

  digitalWrite(
    TRIG_PIN,
    HIGH
  );

  delayMicroseconds(10);

  digitalWrite(
    TRIG_PIN,
    LOW
  );


  /*
   * Measure echo.
   *
   * 30 ms timeout corresponds to roughly 5 m.
   */

  unsigned long duration =
    pulseIn(
      ECHO_PIN,
      HIGH,
      30000
    );


  /*
   * No echo.
   */

  if (
    duration == 0
  )
  {
    return -1.0;
  }


  /*
   * Convert microseconds to cm.
   */

  float distance =
    duration / 58.0;


  return distance;
}


/* ============================================================
 * UPDATE ULTRASONIC
 * ============================================================
 */

void updateUltrasonic()
{
  unsigned long now =
    millis();


  /*
   * Don't measure too frequently.
   */

  if (
    now - lastUltrasonicRead <
    ULTRASONIC_INTERVAL_MS
  )
  {
    return;
  }


  lastUltrasonicRead =
    now;


  currentDistance =
    measureDistanceCM();
}


/* ============================================================
 * BUZZER
 * ============================================================
 */

void updateBuzzer()
{
  unsigned long now = millis();

  if (buzzerState)
  {
    if (now - lastBuzzerChange >= BUZZER_ON_TIME)
    {
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      lastBuzzerChange = now;
    }
  }
  else
  {
    if (now - lastBuzzerChange >= BUZZER_OFF_TIME)
    {
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
 * OBSTACLE CHECK
 * ============================================================
 */

bool obstacleIsPresent()
{
  /*
   * If a valid distance is received
   * and it is <= 30 cm,
   * obstacle exists.
   */

  if (
    currentDistance > 0 &&
    currentDistance <=
    OBSTACLE_DISTANCE_CM
  )
  {
    return true;
  }


  return false;
}


/* ============================================================
 * OBSTACLE HANDLER
 * ============================================================
 */

void handleObstacle()
{
  /*
   * Stop immediately.
   */

  stopMotors();

  // Beep without blocking the main loop.
  updateBuzzer();


  /*
   * Print only when obstacle is first detected.
   */

  if (
    !obstacleDetected
  )
  {
    obstacleDetected = true;


    Serial.println();

    Serial.println(
      "================================"
    );

    Serial.println(
      "       OBSTACLE DETECTED"
    );

    Serial.print(
      "Distance: "
    );

    Serial.print(
      currentDistance,
      1
    );

    Serial.println(
      " cm"
    );

    Serial.println(
      "ROBOT STOPPED"
    );

    Serial.println(
      "Waiting for obstacle removal..."
    );

    Serial.println(
      "================================"
    );

    Serial.println();
  }


  /*
   * Keep motors stopped.
   *
   * The ultrasonic sensor continues checking
   * the distance in the main loop.
   */
}


/* ============================================================
 * RESUME AFTER OBSTACLE
 * ============================================================
 */

void checkObstacleRemoval()
{
  /*
   * If we previously had an obstacle,
   * check whether it has disappeared.
   */

  if (
    obstacleDetected
  )
  {

    if (
      !obstacleIsPresent()
    )
    {

      /*
       * Small confirmation delay.
       *
       * Prevents immediately resuming because
       * of one noisy ultrasonic reading.
       */

      delay(100);


      updateUltrasonic();


      if (
        !obstacleIsPresent()
      )
      {

        obstacleDetected =
          false;

        // Stop the buzzer before resuming.
        buzzerOff();


        /*
         * Reset line direction.
         *
         * The line follower will immediately
         * read the sensors again.
         */

        lastDirection = 0;


        Serial.println();

        Serial.println(
          ">>> OBSTACLE REMOVED"
        );

        Serial.println(
          ">>> RESUMING LINE FOLLOWING"
        );

        Serial.println();
      }
    }
  }
}


/* ============================================================
 * LINE FOLLOWER
 * ============================================================
 */

void followLine()
{
  int s1 = readSensor1();

  int s2 = readSensor2();

  int s3 = readSensor3();

  int s4 = readSensor4();

  int s5 = readSensor5();


  /* ==========================================================
   * ALL FIVE
   *
   * Very wide line.
   * ==========================================================
   */

  if (
    s1 &&
    s2 &&
    s3 &&
    s4 &&
    s5
  )
  {
    lastDirection = 0;

    moveForward();

    return;
  }


  /* ==========================================================
   * CENTER
   *
   * 00100
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    s3 &&
    !s4 &&
    !s5
  )
  {
    lastDirection = 0;

    moveForward();

    return;
  }


  /* ==========================================================
   * CENTER-LEFT
   *
   * 01100
   * ==========================================================
   */

  if (
    !s1 &&
    s2 &&
    s3 &&
    !s4 &&
    !s5
  )
  {
    lastDirection = -1;

    correctLeft(
      GENTLE_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * LEFT
   *
   * 01000
   * ==========================================================
   */

  if (
    !s1 &&
    s2 &&
    !s3 &&
    !s4 &&
    !s5
  )
  {
    lastDirection = -1;

    correctLeft(
      MEDIUM_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * FAR LEFT
   *
   * 11000
   * ==========================================================
   */

  if (
    s1 &&
    s2 &&
    !s3 &&
    !s4 &&
    !s5
  )
  {
    lastDirection = -1;

    strongLeft();

    return;
  }


  /* ==========================================================
   * EXTREME LEFT
   *
   * 10000
   * ==========================================================
   */

  if (
    s1 &&
    !s2 &&
    !s3 &&
    !s4 &&
    !s5
  )
  {
    lastDirection = -1;

    strongLeft();

    return;
  }


  /* ==========================================================
   * CENTER-RIGHT
   *
   * 00110
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    s3 &&
    s4 &&
    !s5
  )
  {
    lastDirection = 1;

    correctRight(
      GENTLE_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * RIGHT
   *
   * 00010
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    !s3 &&
    s4 &&
    !s5
  )
  {
    lastDirection = 1;

    correctRight(
      MEDIUM_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * FAR RIGHT
   *
   * 00011
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    !s3 &&
    s4 &&
    s5
  )
  {
    lastDirection = 1;

    strongRight();

    return;
  }


  /* ==========================================================
   * EXTREME RIGHT
   *
   * 00001
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    !s3 &&
    !s4 &&
    s5
  )
  {
    lastDirection = 1;

    strongRight();

    return;
  }


  /* ==========================================================
   * LEFT COMBINATIONS
   * ==========================================================
   */

  if (
    s1 ||
    s2
  )
  {
    lastDirection = -1;

    correctLeft(
      MEDIUM_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * RIGHT COMBINATIONS
   * ==========================================================
   */

  if (
    s4 ||
    s5
  )
  {
    lastDirection = 1;

    correctRight(
      MEDIUM_TURN_TIME
    );

    return;
  }


  /* ==========================================================
   * LINE LOST
   *
   * 00000
   * ==========================================================
   */

  if (
    !s1 &&
    !s2 &&
    !s3 &&
    !s4 &&
    !s5
  )
  {

    if (
      lastDirection < 0
    )
    {
      leftMotorStop();

      rightMotorForward();

      delay(
        LOST_LINE_TURN_TIME
      );
    }

    else if (
      lastDirection > 0
    )
    {
      leftMotorForward();

      rightMotorStop();

      delay(
        LOST_LINE_TURN_TIME
      );
    }

    else
    {
      moveForward();

      delay(
        LOST_LINE_TURN_TIME
      );
    }


    return;
  }


  /* ==========================================================
   * FALLBACK
   * ==========================================================
   */

  moveForward();
}


/* ============================================================
 * SENSOR MONITOR
 * ============================================================
 */

void sensorMonitor()
{
  Serial.println();

  Serial.println(
    "========== SENSOR MONITOR =========="
  );

  Serial.println(
    "Press 's' to exit."
  );

  Serial.println();


  while (true)
  {

    printSensors();

    delay(200);


    if (
      Serial.available()
    )
    {

      char c =
        Serial.read();


      if (
        c == 's' ||
        c == 'S'
      )
      {

        stopMotors();

        Serial.println(
          "Sensor monitor stopped."
        );

        return;
      }
    }
  }
}


/* ============================================================
 * PRINT DISTANCE
 * ============================================================
 */

void printDistance()
{
  updateUltrasonic();


  if (
    currentDistance < 0
  )
  {
    Serial.println(
      "Distance: NO ECHO"
    );
  }
  else
  {
    Serial.print(
      "Distance: "
    );

    Serial.print(
      currentDistance,
      1
    );

    Serial.println(
      " cm"
    );
  }
}


/* ============================================================
 * HELP
 * ============================================================
 */

void printHelp()
{
  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "       LINE + OBSTACLE TEST"
  );

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "g = START"
  );

  Serial.println(
    "s = STOP"
  );

  Serial.println(
    "i = IR snapshot"
  );

  Serial.println(
    "d = Distance"
  );

  Serial.println(
    "w = IR monitor"
  );

  Serial.println(
    "h = HELP"
  );

  Serial.println(
    "=========================================="
  );

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


  /* ----------------------------------------------------------
   * MOTOR PINS
   * ----------------------------------------------------------
   */

  pinMode(
    LEFT_IN1,
    OUTPUT
  );

  pinMode(
    LEFT_IN2,
    OUTPUT
  );

  pinMode(
    RIGHT_IN1,
    OUTPUT
  );

  pinMode(
    RIGHT_IN2,
    OUTPUT
  );


  /* ----------------------------------------------------------
   * IR
   * ----------------------------------------------------------
   */

  pinMode(
    SENSOR_1,
    INPUT
  );

  pinMode(
    SENSOR_2,
    INPUT
  );

  pinMode(
    SENSOR_3,
    INPUT
  );

  pinMode(
    SENSOR_4,
    INPUT
  );

  pinMode(
    SENSOR_5,
    INPUT
  );


  /* ----------------------------------------------------------
   * HC-SR04
   * ----------------------------------------------------------
   */

  pinMode(
    TRIG_PIN,
    OUTPUT
  );

  pinMode(
    ECHO_PIN,
    INPUT
  );


  digitalWrite(
    TRIG_PIN,
    LOW
  );


  /* ----------------------------------------------------------
   * ACTIVE BUZZER
   * ----------------------------------------------------------
   */

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  buzzerOff();


  /* ----------------------------------------------------------
   * SAFETY
   * ----------------------------------------------------------
   */

  stopMotors();


  robotRunning = false;

  obstacleDetected = false;

  lastDirection = 0;


  /* ----------------------------------------------------------
   * START MESSAGE
   * ----------------------------------------------------------
   */

  Serial.println();

  Serial.println(
    "=============================================="
  );

  Serial.println(
    "   ESP32 LINE FOLLOWER + HC-SR04"
  );

  Serial.println(
    "=============================================="
  );

  Serial.println(
    "PWM       : DISABLED"
  );

  Serial.println(
    "ENA       : 5V"
  );

  Serial.println(
    "ENB       : 5V"
  );

  Serial.println(
    "Obstacle  : 30 cm"
  );

  Serial.println(
    "Buzzer    : GPIO 4 (Active)"
  );

  Serial.println(
    "Waiting   : Until obstacle removed"
  );

  Serial.println(
    "=============================================="
  );

  Serial.println();

  printHelp();
}


/* ============================================================
 * LOOP
 * ============================================================
 */

void loop()
{

  /* ==========================================================
   * SERIAL COMMANDS
   * ==========================================================
   */

  if (
    Serial.available()
  )
  {

    char command =
      Serial.read();


    switch (command)
    {

      /* ------------------------------------------------------
       * START
       * ------------------------------------------------------
       */

      case 'g':
      case 'G':

        robotRunning = true;

        obstacleDetected = false;

        lastDirection = 0;

        Serial.println();

        Serial.println(
          ">>> LINE FOLLOWING STARTED"
        );

        Serial.println();

        break;


      /* ------------------------------------------------------
       * STOP
       * ------------------------------------------------------
       */

      case 's':
      case 'S':

        robotRunning = false;

        obstacleDetected = false;

        stopMotors();
        buzzerOff();

        Serial.println();

        Serial.println(
          ">>> ROBOT STOPPED"
        );

        Serial.println();

        break;


      /* ------------------------------------------------------
       * IR
       * ------------------------------------------------------
       */

      case 'i':
      case 'I':

        printSensors();

        break;


      /* ------------------------------------------------------
       * DISTANCE
       * ------------------------------------------------------
       */

      case 'd':
      case 'D':

        printDistance();

        break;


      /* ------------------------------------------------------
       * SENSOR MONITOR
       * ------------------------------------------------------
       */

      case 'w':
      case 'W':

        if (
          !robotRunning
        )
        {
          sensorMonitor();
        }
        else
        {
          Serial.println(
            "Stop robot first using 's'."
          );
        }

        break;


      /* ------------------------------------------------------
       * HELP
       * ------------------------------------------------------
       */

      case 'h':
      case 'H':

        printHelp();

        break;


      /* ------------------------------------------------------
       * IGNORE ENTER
       * ------------------------------------------------------
       */

      case '\n':
      case '\r':
      case ' ':

        break;


      default:

        Serial.print(
          "Unknown command: "
        );

        Serial.println(
          command
        );

        break;
    }
  }


  /* ==========================================================
   * ULTRASONIC UPDATE
   *
   * We continuously check the obstacle even while
   * line following.
   * ==========================================================
   */

  updateUltrasonic();


  /* ==========================================================
   * ROBOT NOT RUNNING
   * ==========================================================
   */

  if (
    !robotRunning
  )
  {
    stopMotors();

    return;
  }


  /* ==========================================================
   * OBSTACLE CURRENTLY BLOCKING ROBOT
   * ==========================================================
   */

  if (
    obstacleIsPresent()
  )
  {

    handleObstacle();

    return;
  }


  /* ==========================================================
   * OBSTACLE WAS PREVIOUSLY DETECTED
   *
   * Check whether it has now disappeared.
   * ==========================================================
   */

  if (
    obstacleDetected
  )
  {

    checkObstacleRemoval();

    return;
  }


  /* ==========================================================
   * NORMAL LINE FOLLOWING
   * ==========================================================
   */

  followLine();
}
