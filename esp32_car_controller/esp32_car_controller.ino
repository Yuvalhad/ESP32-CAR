#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <MPU9250_WE.h>  // Library for MPU9250/MPU6500

// WiFi credentials - same as the car
const char* ssid = "Pixel 6";
const char* password = "Yuval111";

// Car ESP32 IP address - you'll need to replace this with your car's actual IP
const char* carIP = "192.168.x.x";  // Replace with your car's IP address

// MPU9250/MPU6500
MPU9250_WE myMPU = MPU9250_WE(0x68);  // Default I2C address is 0x68

// Gesture thresholds
const float TILT_THRESHOLD = 0.5;  // Adjust based on sensitivity needed (in g, typical range ±1g)
const int GESTURE_DELAY = 100;     // Minimum time between commands (ms)

// Command state tracking
char lastCommand = 'Q';
unsigned long lastCommandTime = 0;
bool mpuConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Glove Controller Starting...");
  
  // Initialize I2C
  Wire.begin();
  delay(1000); // Give some time for the serial monitor to open
  
  // Test MPU connection
  Serial.println("Initializing MPU6500/MPU9250...");
  
  // Try to initialize MPU
  if (!myMPU.init()) {
    Serial.println("MPU connection failed. Check your wiring!");
    mpuConnected = false;
  } else {
    Serial.println("MPU connected successfully!");
    mpuConnected = true;
    
    // Configure MPU settings
    myMPU.autoOffsets();
    myMPU.enableAccDLPF(true);
    myMPU.setAccDLPF(MPU9250_DLPF_6);  // Low pass filter setting
    myMPU.setAccRange(MPU9250_ACC_RANGE_2G);  // Set range to 2G for better sensitivity
    
    Serial.println("MPU initialized and configured!");
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Glove controller ready!");
  Serial.println("Starting to print accelerometer values...");
}

void loop() {
  // Check if MPU is connected
  if (!mpuConnected) {
    Serial.println("MPU not connected. Retrying...");
    mpuConnected = myMPU.init();
    if (mpuConnected) {
      // Configure MPU settings on successful connection
      myMPU.autoOffsets();
      myMPU.enableAccDLPF(true);
      myMPU.setAccDLPF(MPU9250_DLPF_6);
      myMPU.setAccRange(MPU9250_ACC_RANGE_2G);
      Serial.println("MPU connected successfully!");
    }
    delay(1000);
    return;
  }
  
  // Read accelerometer data
  xyzFloat accRaw = myMPU.getAccRawValues();
  xyzFloat accG = myMPU.getCorrectedAccRawValues();
  
  // Print accelerometer values for calibration and testing
  Serial.print("Accel Raw X: ");
  Serial.print(accRaw.x);
  Serial.print(" | Y: ");
  Serial.print(accRaw.y);
  Serial.print(" | Z: ");
  Serial.println(accRaw.z);
  
  Serial.print("Accel (g) X: ");
  Serial.print(accG.x);
  Serial.print(" | Y: ");
  Serial.print(accG.y);
  Serial.print(" | Z: ");
  Serial.println(accG.z);
  
  // Determine command based on glove orientation
  char command = 'Q';  // Default to stop
  
  // Forward: Tilting forward (negative X)
  if (accG.x < -TILT_THRESHOLD) {
    command = 'W';
    Serial.println("GESTURE: FORWARD");
  } 
  // Backward: Tilting backward (positive X)
  else if (accG.x > TILT_THRESHOLD) {
    command = 'S';
    Serial.println("GESTURE: BACKWARD");
  }
  // Left: Tilting left (negative Y)
  else if (accG.y < -TILT_THRESHOLD) {
    command = 'A';
    Serial.println("GESTURE: LEFT");
  }
  // Right: Tilting right (positive Y)
  else if (accG.y > TILT_THRESHOLD) {
    command = 'D';
    Serial.println("GESTURE: RIGHT");
  }
  
  // Only send command if it's different from the last one or enough time has passed
  unsigned long currentTime = millis();
  if ((command != lastCommand || currentTime - lastCommandTime > 500) && 
      command != 'Q' && currentTime - lastCommandTime > GESTURE_DELAY) {
    // In test mode, just print the command rather than sending it
    Serial.print("Would send command: ");
    Serial.println(command);
    
    // Uncomment this line when you're ready to actually send commands
    // sendCommand(command);
    
    lastCommand = command;
    lastCommandTime = currentTime;
  }
  // Send stop command if returning to neutral position
  else if (command == 'Q' && lastCommand != 'Q') {
    Serial.println("GESTURE: NEUTRAL (STOP)");
    
    // Uncomment this line when you're ready to actually send commands
    // sendCommand('Q');
    
    lastCommand = 'Q';
  }
  
  delay(200);  // Slightly longer delay for readable serial output
}

void sendCommand(char cmd) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Construct the URL with command and source parameters
    String url = "http://" + String(carIP) + "/control?source=glove&cmd=" + cmd;
    
    Serial.print("Sending request to: ");
    Serial.println(url);
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("Command sent: ");
      Serial.print(cmd);
      Serial.print(" - Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending command. Error code: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
