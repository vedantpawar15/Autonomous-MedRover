#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --------- CONFIG: EDIT THESE ---------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Supabase project REST URL (no trailing slash)
const char* SUPABASE_URL  = "https://tdkccbbqktzeojgeuaoh.supabase.co";
// Public ANON key (same as Vite frontend, NOT service_role)
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRka2NjYmJxa3R6ZW9qZ2V1YW9oIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMzMzUyODEsImV4cCI6MjA4ODkxMTI4MX0.eBjZ0U9FEIg3vwHqXr8KR8VygMQDORBcaRwVY1LsdyY";

// How often to poll Supabase for new orders (ms)
const unsigned long POLL_INTERVAL_MS = 5000;

// --------------------------------------
unsigned long lastPoll = 0;

// Simple structure to hold order info
struct Order {
  long id;
  String roomCode;
  String roomLabel;
  bool valid;
};

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// Generic helper: perform HTTP request and return body as String
bool httpRequest(const String& method, const String& url, const String& body, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  HTTPClient http;
  http.begin(url);
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");

  int httpCode;
  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "PATCH") {
    httpCode = http.PATCH(body);
  } else if (method == "POST") {
    httpCode = http.POST(body);
  } else {
    http.end();
    return false;
  }

  if (httpCode > 0) {
    response = http.getString();
    Serial.printf("[HTTP] %s %s -> code: %d\n", method.c_str(), url.c_str(), httpCode);
    // For Supabase REST, 200/201/204 are OK
    bool ok = (httpCode >= 200 && httpCode < 300);
    http.end();
    return ok;
  } else {
    Serial.printf("[HTTP] Request failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

// Fetch oldest pending order from Supabase
Order fetchNextPendingOrder() {
  Order o;
  o.valid = false;

  // /rest/v1/orders?status=eq.pending&order=created_at.asc&limit=1
  String url = String(SUPABASE_URL) +
               "/rest/v1/orders?status=eq.pending&order=created_at.asc&limit=1";

  String resp;
  if (!httpRequest("GET", url, "", resp)) {
    Serial.println("Failed to fetch pending order");
    return o;
  }

  Serial.println("Orders response: " + resp);

  // Parse JSON array: [ { "id": ..., "room_code": "...", "room_label": "..." } ]
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return o;
  }

  if (!doc.is<JsonArray>() || doc.size() == 0) {
    // No pending orders
    return o;
  }

  JsonObject obj = doc[0];
  o.id        = obj["id"] | 0;
  o.roomCode  = (const char*)obj["room_code"] | "";
  o.roomLabel = (const char*)obj["room_label"] | "";
  o.valid = (o.id > 0 && o.roomCode.length() > 0);

  return o;
}

// Update order status in Supabase
bool updateOrderStatus(long orderId, const char* newStatus) {
  // /rest/v1/orders?id=eq.ORDER_ID
  String url = String(SUPABASE_URL) + "/rest/v1/orders?id=eq." + String(orderId);

  StaticJsonDocument<128> bodyDoc;
  bodyDoc["status"] = newStatus;
  String body;
  serializeJson(bodyDoc, body);

  String resp;
  bool ok = httpRequest("PATCH", url, body, resp);
  if (!ok) {
    Serial.println("Failed to update order status: " + String(newStatus));
    Serial.println("Response: " + resp);
  } else {
    Serial.println("Order " + String(orderId) + " -> status = " + newStatus);
  }
  return ok;
}

// TODO: implement your line-following + junction logic here
void navigateToRoom(const String& roomCode) {
  Serial.println("Starting navigation to room: " + roomCode);

  // Example stub:
  // if (roomCode == "A") {
  //   followLineUntilJunction();  // your function
  //   turnLeft();
  //   followLineUntilDestination();
  // } else if (roomCode == "B") {
  //   followLineUntilJunction();
  //   goStraight();
  //   followLineUntilDestination();
  // } else if (roomCode == "C") {
  //   followLineUntilJunction();
  //   turnRight();
  //   followLineUntilDestination();
  // }

  // For now, just simulate time taken:
  delay(5000);

  Serial.println("Arrived at room " + roomCode);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;

    Serial.println("Polling Supabase for pending orders...");

    Order o = fetchNextPendingOrder();
    if (!o.valid) {
      Serial.println("No pending orders.");
    } else {
      Serial.println("Found order #" + String(o.id) +
                     " for room " + o.roomCode +
                     " (" + o.roomLabel + ")");

      // 1) Mark as in_transit so no other robot picks it
      if (!updateOrderStatus(o.id, "in_transit")) {
        // If we can't mark in_transit, skip to avoid duplicate handling
        return;
      }

      // 2) Navigate to room
      navigateToRoom(o.roomCode);

      // 3) Mark as delivered
      updateOrderStatus(o.id, "delivered");
    }
  }

  // Your normal line-following loop or idle behavior can also go here
}