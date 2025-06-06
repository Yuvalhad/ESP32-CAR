#include <Wire.h>
#include <MPU6500_WE.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define MPU_ADDR 0x68
MPU6500_WE MPU6500(MPU_ADDR);

// Wi‑Fi credentials
const char* ssid     = "Pixel 6";
const char* password = "Yuval111";

// Server IP and URL
const char* serverIP = "192.168.1.100"; // need to change

// Offset storage
xyzFloat accOffset;

// Function to send command to server
void sendGloveCommand(char cmd, float x, float y) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String("http://") + serverIP + "/glove?source=glove&cmd=" + cmd + "&x=" + String(x, 2) + "&y=" + String(y, 2);
        http.begin(url);
        http.setTimeout(500); // Set timeout to 500ms
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.println("HTTP Response: " + payload);
        } else {
            Serial.println("HTTP Request failed: " + String(httpCode));
            // Retry once after a short delay
            delay(100);
            http.begin(url);
            httpCode = http.GET();
            if (httpCode == 200) {
                Serial.println("Retry successful");
            } else {
                Serial.println("Retry failed: " + String(httpCode));
            }
        }
        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(13, 14); // Custom I2C pins

  Serial.println("Checking MPU6500...");

  // Wake up the MPU6500
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  // Read WHO_AM_I
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1);

  if (Wire.available()) {
    uint8_t whoami = Wire.read();
    Serial.print("WHO_AM_I Register: 0x");
    Serial.println(whoami, HEX);

    if (whoami == 0x70 || whoami == 0x68) {
      Serial.println("✅ MPU6500 detected successfully!");
    } else {
      Serial.println("❌ MPU6500 not detected! Check wiring.");
    }
  } else {
    Serial.println("❌ No response from MPU6500!");
  }

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to Wi-Fi");
  Serial.print("Client IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize MPU6500
  if (!MPU6500.init()) {
    Serial.println("MPU6500 init failed!");
    while (1);
  }

  // Read initial offset when flat
  delay(1000); // Allow time for settling
  accOffset = MPU6500.getGValues(); // Store straight-position gravity values
  Serial.println("Accelerometer offset stored.");
}

void loop() {
  xyzFloat acc = MPU6500.getGValues(); // Get acceleration in G

  // Subtract offset
  float accX = acc.x - accOffset.x;
  float accY = acc.y - accOffset.y;

  // Normalize assuming max tilt gives ~1G
  float normX = constrain(accX / 1.0, -1.0, 1.0);
  float normY = constrain(accY / 1.0, -1.0, 1.0);

  // Determine command based on normX and normY
    char cmd = 'Q'; // Default: stop
    if (normX < -0.2) {
        cmd = 'A'; // Left
    } else if (normX > 0.2) {
        cmd = 'D'; // Right
    } else if (normY > 0.1) {
        cmd = 'W'; // Forward
    } else if (normY < -0.1) {
        cmd = 'S'; // Backward
    }  
  
   sendGloveCommand(cmd,normX,normY);
  
  Serial.print("Norm X: ");
  Serial.print(normX, 2);
  Serial.print(" | Norm Y: ");
  Serial.println(normY, 2);

  delay(300);
}
