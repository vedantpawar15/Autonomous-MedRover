/*
 * ============================================================
 *  LINE FOLLOW v14 — Integrated WiFi + Supabase Order System
 * ============================================================
 *  Combines:
 *   - v13 line-following PD controller (S-shape + junction handling)
 *   - WiFi + Supabase polling for room delivery orders
 *
 *  ROOM ROUTING (customize to your track layout):
 *   Room A → turn left at junction 1 (first junction, branch on left)
 *   Room B → straight past junction 1, turn left at junction 2
 *   Room C → straight past junctions 1 & 2, turn right at junction 3 (last)
 *   At turn junctions: stop, pause JUNCTION_PAUSE_MS, then turn; after turn, follow
 *   until the next junction (no fixed time — replaces old followForTime legs).
 *
 *  SERIAL COMMANDS:
 *   g → Start manual line follow
 *   s → Stop
 *   + / -  → Speed ±5
 *   ] / [  → Kp ±0.5
 *   > / <  → Kd ±0.5
 *   ) / (  → Turn speed ±5
 *   p → Print settings
 *   i → Read IR sensors once
 * ============================================================
 */

 #include <WiFi.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 
 // ── WiFi & Supabase Config ────────────────────────────────
 const char* WIFI_SSID         = "Ram";
 const char* WIFI_PASSWORD     = "latur@24";
 const char* SUPABASE_URL      = "https://tdkccbbqktzeojgeuaoh.supabase.co";
 const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                                  "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRka2NjYmJxa3R6ZW9qZ2V1YW9oIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMzMzUyODEsImV4cCI6MjA4ODkxMTI4MX0."
                                  "eBjZ0U9FEIg3vwHqXr8KR8VygMQDORBcaRwVY1LsdyY";
 
 const unsigned long POLL_INTERVAL_MS = 5000;
 
 // ── Motor Pins ────────────────────────────────────────────
 #define IN1  27
 #define IN2  26
 #define IN3  25
 #define IN4  33
 #define ENA  14
 #define ENB  12
 
 // ── IR Sensor Pins ────────────────────────────────────────
 #define IR_S1 34
 #define IR_S2 35
 #define IR_S3 32
 #define IR_S4 18
 #define IR_S5 19
 
 #define PWM_FREQ 1000
 #define PWM_RES  8
 // Hold still at a crossing before turning, and at room arrival (ms)
 #define JUNCTION_PAUSE_MS 1200
 
 // ── Tuning Parameters ─────────────────────────────────────
 int   baseSpeed   = 125;
 int   rightOffset = 120;
 float Kp          = 20.0;
 float Kd          = 20.0;
 int   turnSpeed   = 150;
 int   searchSpeed = 100;
 
 // ── State ─────────────────────────────────────────────────
 bool lineFollowing = false;
 bool autoNavActive = false;
 int  lastError     = 0;
 int  searchDir     = 1;
 unsigned long lastPoll = 0;
 
 enum FollowState { FOLLOWING, SEARCHING, SHARP_LEFT, SHARP_RIGHT };
 FollowState followState = FOLLOWING;
 
 struct IRSensor { bool s1, s2, s3, s4, s5; };
 
 struct Order {
   long   id;
   String roomCode;
   String roomLabel;
   bool   valid;
 };
 
 // ═══════════════════════════════════════════════════════════
 //  MOTOR CONTROL
 // ═══════════════════════════════════════════════════════════
 
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
 
 void stopMotorsOnly() {
   digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
   digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
   ledcWrite(ENA, 0); ledcWrite(ENB, 0);
 }
 
 void stopAll() {
   stopMotorsOnly();
   lineFollowing = false;
   autoNavActive = false;
   followState   = FOLLOWING;
 }
 
 // ═══════════════════════════════════════════════════════════
 //  IR SENSOR
 // ═══════════════════════════════════════════════════════════
 
 IRSensor readIR() {
   IRSensor d;
   d.s1 = !digitalRead(IR_S1); d.s2 = !digitalRead(IR_S2);
   d.s3 = !digitalRead(IR_S3); d.s4 = !digitalRead(IR_S4);
   d.s5 = !digitalRead(IR_S5);
   return d;
 }
 
 // ═══════════════════════════════════════════════════════════
 //  CORE LINE-FOLLOW TICK — returns true if junction detected
 // ═══════════════════════════════════════════════════════════
 
 bool doLineFollowTick() {
   IRSensor ir  = readIR();
   int      sum = (int)ir.s1+(int)ir.s2+(int)ir.s3+(int)ir.s4+(int)ir.s5;
 
   // Junction: 4+ sensors active
   if (sum >= 4) {
     setMotorLeft(baseSpeed);
     setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
     followState = FOLLOWING;
     return true;
   }
 
   // Lost line → search
   if (sum == 0) {
     if (followState != SEARCHING) {
       followState = SEARCHING;
       searchDir   = (lastError >= 0) ? 1 : -1;
       Serial.printf("LOST → search %s\n", searchDir > 0 ? "RIGHT" : "LEFT");
     }
     if (searchDir > 0) { setMotorLeft(searchSpeed);  setMotorRight(-searchSpeed); }
     else               { setMotorLeft(-searchSpeed); setMotorRight(searchSpeed);  }
     return false;
   }
 
   // Sharp LEFT: only S1
   if (ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && !ir.s5) {
     if (followState != SHARP_LEFT) { followState = SHARP_LEFT; Serial.println("SHARP LEFT"); }
     setMotorLeft(-turnSpeed);
     setMotorRight(turnSpeed + rightOffset);
     return false;
   }
 
   // Sharp RIGHT: only S5
   if (!ir.s1 && !ir.s2 && !ir.s3 && !ir.s4 && ir.s5) {
     if (followState != SHARP_RIGHT) { followState = SHARP_RIGHT; Serial.println("SHARP RIGHT"); }
     setMotorLeft(turnSpeed);
     setMotorRight(-(turnSpeed - rightOffset));
     return false;
   }
 
   // Normal PD following
   followState = FOLLOWING;
   int error      = (-2*(int)ir.s1)+(-1*(int)ir.s2)+(0)+(1*(int)ir.s4)+(2*(int)ir.s5);
   int derivative = error - lastError;
   lastError      = error;
 
   float correction = (Kp * error) + (Kd * derivative);
   int leftSpeed    = baseSpeed               - (int)correction;
   int rightSpeed   = baseSpeed + rightOffset + (int)correction;
 
   setMotorLeft(constrain(leftSpeed,  0, 255));
   setMotorRight(constrain(rightSpeed, 0, 255));
 
   static int prevErr = 99;
   if (error != prevErr) {
     Serial.printf("err:%2d L:%4d R:%4d [%d%d%d%d%d]\n",
       error, leftSpeed, rightSpeed, ir.s1, ir.s2, ir.s3, ir.s4, ir.s5);
     prevErr = error;
   }
   return false;
 }
 
 // ═══════════════════════════════════════════════════════════
 //  NAVIGATION HELPERS
 // ═══════════════════════════════════════════════════════════
 
 // Follow line until N junctions seen.
 // haltAtFinal: false = creep past last junction (use when driving straight through).
 // haltAtFinal: true = stop on last junction, wait JUNCTION_PAUSE_MS, then return (caller turns or ends leg).
 void followUntilJunction(int count = 1, bool haltAtFinal = false) {
   int found = 0;
   unsigned long junctionCooldown = 0;
   Serial.printf("Following until %d junction(s)%s...\n", count,
                 haltAtFinal ? " (halt at last)" : "");
 
   while (found < count) {
     bool junc = doLineFollowTick();
     if (junc && millis() > junctionCooldown) {
       found++;
       junctionCooldown = millis() + 600;  // debounce
       Serial.printf("Junction %d/%d\n", found, count);
     }
     delay(15);
   }
   stopMotorsOnly();
   if (haltAtFinal) {
     Serial.printf("Paused at junction for %lu ms\n", (unsigned long)JUNCTION_PAUSE_MS);
     delay(JUNCTION_PAUSE_MS);
   } else {
     // Creep past centre when continuing along the line
     setMotorLeft(baseSpeed);
     setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
     delay(200);
     stopMotorsOnly();
     delay(300);
   }
 }
 
 void turnLeft90() {
   Serial.println("Turning LEFT 90°");
   setMotorLeft(-turnSpeed);
   setMotorRight(turnSpeed + rightOffset);
   delay(500);   // ← tune for your wheelbase
   stopMotorsOnly();
   delay(300);
 }
 
 void turnRight90() {
   Serial.println("Turning RIGHT 90°");
   setMotorLeft(turnSpeed);
   setMotorRight(-(turnSpeed - rightOffset));
   delay(500);   // ← tune for your wheelbase
   stopMotorsOnly();
   delay(300);
 }
 
 void uTurn() {
   Serial.println("U-Turn");
   setMotorLeft(turnSpeed);
   setMotorRight(-(turnSpeed - rightOffset));
   delay(1000);  // ← tune for 180°
   stopMotorsOnly();
   delay(300);
 }
 
 // ═══════════════════════════════════════════════════════════
 //  ROOM NAVIGATION  (customize routes to your track)
 // ═══════════════════════════════════════════════════════════
 
 void navigateToRoom(const String& roomCode) {
   Serial.println("=== Navigating to: " + roomCode + " ===");
   lastError   = 0;
   followState = FOLLOWING;
 
   if (roomCode == "A") {
     followUntilJunction(1, true);   // stop at J1, pause, then turn
     turnLeft90();
     followUntilJunction(1, true);   // branch line until next junction = room
 
   } else if (roomCode == "B") {
     followUntilJunction(1);         // pass junction 1 (straight)
     followUntilJunction(1, true);    // stop at J2, pause, then turn
     turnLeft90();
     followUntilJunction(1, true);
 
   } else if (roomCode == "C") {
     followUntilJunction(1);
     followUntilJunction(1);
     followUntilJunction(1, true);   // stop at last junction, pause, then turn
     turnRight90();
     followUntilJunction(1, true);
 
   } else {
     Serial.println("Unknown room, follow to next junction");
     followUntilJunction(1, true);
   }
 
   Serial.println("=== Arrived at: " + roomCode + " ===");
   delay(2000);   // simulate delivery pause
 
   // Return to base
   Serial.println("=== Returning to base ===");
   uTurn();
   followUntilJunction(1, true);
   stopMotorsOnly();
   Serial.println("=== Back at base ===");
 }
 
 // ═══════════════════════════════════════════════════════════
 //  WIFI & SUPABASE
 // ═══════════════════════════════════════════════════════════
 
 void connectWiFi() {
   Serial.print("Connecting to WiFi");
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   while (WiFi.status() != WL_CONNECTED) {
     delay(500); Serial.print(".");
   }
   Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
 }
 
 bool httpRequest(const String& method, const String& url,
                  const String& body, String& response) {
   if (WiFi.status() != WL_CONNECTED) { Serial.println("WiFi down"); return false; }
 
   HTTPClient http;
   http.begin(url);
   http.addHeader("apikey",        SUPABASE_ANON_KEY);
   http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
   http.addHeader("Content-Type",  "application/json");
 
   int code;
   if      (method == "GET")   code = http.GET();
   else if (method == "PATCH") code = http.PATCH(body);
   else if (method == "POST")  code = http.POST(body);
   else { http.end(); return false; }
 
   bool ok = false;
   if (code > 0) {
     response = http.getString();
     Serial.printf("[HTTP] %s -> %d\n", method.c_str(), code);
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
   Serial.println("Response: " + resp);
 
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
   Serial.println(ok ? "Status → " + String(newStatus) : "Update FAILED: " + resp);
   return ok;
 }
 
 // ═══════════════════════════════════════════════════════════
 //  SERIAL COMMANDS
 // ═══════════════════════════════════════════════════════════
 
 void printSettings() {
   Serial.println("======== SETTINGS ========");
   Serial.printf("  baseSpeed   : %d\n",   baseSpeed);
   Serial.printf("  rightOffset : %d\n",   rightOffset);
   Serial.printf("  Kp          : %.1f\n", Kp);
   Serial.printf("  Kd          : %.1f\n", Kd);
   Serial.printf("  turnSpeed   : %d\n",   turnSpeed);
   Serial.printf("  searchSpeed : %d\n",   searchSpeed);
   Serial.println("==========================");
 }
 
 void processCommand(char cmd) {
   switch (cmd) {
     case 'g':
       lineFollowing = true; autoNavActive = false;
       followState = FOLLOWING; lastError = 0;
       Serial.println(">>> MANUAL FOLLOW ON"); printSettings(); break;
     case 's': stopAll(); Serial.println(">>> STOPPED"); break;
     case 'f':
       stopAll();
       setMotorLeft(baseSpeed);
       setMotorRight(constrain(baseSpeed + rightOffset, 0, 255));
       Serial.printf(">>> FWD L:%d R:%d\n", baseSpeed, baseSpeed + rightOffset); break;
     case '+': baseSpeed = min(baseSpeed+5, 200);  Serial.printf("Speed:%d\n",  baseSpeed);  break;
     case '-': baseSpeed = max(baseSpeed-5, 80);   Serial.printf("Speed:%d\n",  baseSpeed);  break;
     case ']': Kp = min(Kp+0.5f, 80.f);            Serial.printf("Kp:%.1f\n",   Kp);         break;
     case '[': Kp = max(Kp-0.5f, 1.f);             Serial.printf("Kp:%.1f\n",   Kp);         break;
     case '>': Kd = min(Kd+0.5f, 80.f);            Serial.printf("Kd:%.1f\n",   Kd);         break;
     case '<': Kd = max(Kd-0.5f, 0.f);             Serial.printf("Kd:%.1f\n",   Kd);         break;
     case ')': turnSpeed = min(turnSpeed+5, 220);   Serial.printf("Turn:%d\n",   turnSpeed);  break;
     case '(': turnSpeed = max(turnSpeed-5, 80);    Serial.printf("Turn:%d\n",   turnSpeed);  break;
     case 'i': {
       IRSensor ir = readIR();
       Serial.printf("[%d%d%d%d%d]\n", ir.s1, ir.s2, ir.s3, ir.s4, ir.s5); break;
     }
     case 'p': printSettings(); break;
     case '\n': case '\r': case ' ': break;
     default: Serial.printf("? '%c'\n", cmd);
   }
 }
 
 // ═══════════════════════════════════════════════════════════
 //  SETUP & LOOP
 // ═══════════════════════════════════════════════════════════
 
 void setup() {
   Serial.begin(115200);
   delay(1000);
 
   motorSetup(); stopAll();
   pinMode(IR_S1, INPUT); pinMode(IR_S2, INPUT);
   pinMode(IR_S3, INPUT); pinMode(IR_S4, INPUT); pinMode(IR_S5, INPUT);
 
   Serial.println("================================");
   Serial.println("  LINE FOLLOW v14 — Integrated");
   Serial.println("  WiFi + Supabase + PD Follow");
   Serial.println("================================");
   printSettings();
   connectWiFi();
 }
 
 void loop() {
   // Handle serial commands
   while (Serial.available() > 0) {
     processCommand((char)Serial.read());
     delay(10);
   }
 
   // Manual mode — skip polling
   if (lineFollowing && !autoNavActive) {
     doLineFollowTick();
     delay(15);
     return;
   }
 
   // Supabase polling
   unsigned long now = millis();
   if (now - lastPoll >= POLL_INTERVAL_MS) {
     lastPoll = now;
     Serial.println("Polling Supabase...");
 
     Order o = fetchNextPendingOrder();
     if (!o.valid) { Serial.println("No pending orders."); return; }
 
     Serial.println("Order #" + String(o.id) + " → " + o.roomCode + " (" + o.roomLabel + ")");
 
     if (!updateOrderStatus(o.id, "in_transit")) return;
 
     autoNavActive = true;
     navigateToRoom(o.roomCode);
     autoNavActive = false;
 
     updateOrderStatus(o.id, "delivered");
   }
 }