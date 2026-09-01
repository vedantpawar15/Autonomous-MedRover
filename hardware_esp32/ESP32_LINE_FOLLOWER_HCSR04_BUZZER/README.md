# ESP32 Line Follower + HC-SR04 + Active Buzzer
----------------------------------------------------------------------------------------------------------------------------------------
## Overview
Tested and working baseline firmware for the ESP32 autonomous line-following robot.

Features:
- 5-sensor IR line following
- L298N dual-motor control
- HC-SR04 front obstacle detection
- Automatic motor stop when obstacle is detected
- Active buzzer warning
- Automatic resume after obstacle removal
- Serial monitoring/debug commands
- Line-loss recovery using last detected direction
- Motor PWM disabled
----------------------------------------------------------------------------------------------------------------------------------------
## Hardware / Pin Mapping

### L298N
- Left Motor IN1 → GPIO 27
- Left Motor IN2 → GPIO 26
- Right Motor IN3 → GPIO 25
- Right Motor IN4 → GPIO 33
- ENA → 5V
- ENB → 5V
- Motor PWM → DISABLED

### IR Sensors
- S1 (Far Left) → GPIO 34
- S2 (Left) → GPIO 35
- S3 (Center) → GPIO 32
- S4 (Right) → GPIO 18
- S5 (Far Right) → GPIO 19

IR logic:
- LOW = Line detected
- HIGH = No line

### HC-SR04
- TRIG → GPIO 5
- ECHO → GPIO 23
- Obstacle threshold → ≤ 30 cm
- Ultrasonic update interval → 60 ms

### Active Buzzer
- Buzzer + → GPIO 4
- Buzzer - → GND
- HIGH = ON
- LOW = OFF
- ON time = 200 ms
- OFF time = 300 ms
----------------------------------------------------------------------------------------------------------------------------------------
## Line Following

The 5-sensor controller handles:
- Center
- Center-left
- Left
- Far-left
- Extreme-left
- Center-right
- Right
- Far-right
- Extreme-right
- Wide line
- Line lost
- Fallback

`lastDirection`:
- `-1` = LEFT
- `0` = CENTER
- `+1` = RIGHT

When the line is lost, the robot searches using the last detected direction.

### Tested Line-Following Parameters
GENTLE_TURN_TIME      = 12 ms
MEDIUM_TURN_TIME      = 22 ms
STRONG_TURN_TIME      = 38 ms
LOST_LINE_TURN_TIME   = 35 ms
----------------------------------------------------------------------------------------------------------------------------------------
##Obstacle Behavior
Normal line following
        ↓
Obstacle ≤ 30 cm
        ↓
STOP MOTORS
        ↓
BUZZER BEEPS
        ↓
Wait while obstacle remains
        ↓
Obstacle removed
        ↓
Confirm clear reading
        ↓
BUZZER OFF
        ↓
Resume line following

NOTE: The robot does NOT bypass obstacles. It remains stopped until the obstacle is removed.
      A 100 ms confirmation delay is used before resuming.
      
----------------------------------------------------------------------------------------------------------------------------------------
### Serial Commands
Serial Monitor: 115200 baud

g → Start line following
s → Stop robot
i → Print IR sensor readings
d → Print ultrasonic distance
w → Continuous IR sensor monitor
h → Show help
      
