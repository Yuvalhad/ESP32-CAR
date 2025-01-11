#include <WiFi.h>
#include <WebServer.h>

#define CAMERA_MODEL_AI_THINKER

#include "esp_camera.h"
#include "camera_pins.h"

#define motor_rb 13 // in4
#define motor_rf 15 // in3
#define motor_lb 14 // in2
#define motor_lf 2 //in1

// Wi-Fi credentials
const char* ssid = "Pixel 6";
const char* password = "Yuval111";
//const char* ssid = "Hadad";
//const char* password = "0505660119";

// Web server
WebServer server(80);

//void startCameraServer();

void Backward(){
  digitalWrite(motor_rb,HIGH);
  digitalWrite(motor_rf,LOW);
  digitalWrite(motor_lb,HIGH);
  digitalWrite(motor_lf,LOW);
}

void Forward(){
  digitalWrite(motor_rb,LOW);
  digitalWrite(motor_rf,HIGH);
  digitalWrite(motor_lb,LOW);
  digitalWrite(motor_lf,HIGH);
}

void Left() {
  // Right motor forward, Left motor stop
  digitalWrite(motor_rb, LOW);
  digitalWrite(motor_rf, HIGH);  // Right side moves
  digitalWrite(motor_lb, LOW);
  digitalWrite(motor_lf, LOW);   // Left side stops
}

void Right() {
  // Left motor forward, Right motor stop
  digitalWrite(motor_rb, LOW);
  digitalWrite(motor_rf, LOW);   // Right side stops
  digitalWrite(motor_lb, LOW);
  digitalWrite(motor_lf, HIGH);  // Left side moves
}

void initialMotor(){
  digitalWrite(motor_rb,LOW);
  digitalWrite(motor_rf,LOW);
  digitalWrite(motor_lb,LOW);
  digitalWrite(motor_lf,LOW);
}

// Camera setup function
void setupCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (!psramFound()) {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    } else {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    }
  //  startCameraServer();
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  }
}


void handle_stream() {
    Serial.println("Starting video stream..."); // Debugging

    // הגדרת כותרות לשידור וידאו
    server.sendHeader("Content-Type", "multipart/x-mixed-replace; boundary=frame");
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");

    while (true) {
        // לכידת תמונה
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            break;
        }
        else{
         Serial.printf("Camera capture Works");
         break;
        }

        // שליחת פריים
        server.sendContent("--frame\r\n");
        server.sendContent("Content-Type: image/jpeg\r\n\r\n");
        server.sendContent((const char *)fb->buf, fb->len);
        server.sendContent("\r\n");

        // שחרור המסגרת
        esp_camera_fb_return(fb);

        delay(100); // קצב פריימים (לדוגמה, 10 פריימים לשנייה)
    }
}

const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Car Control with Camera</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        .button {
            padding: 20px 40px;
            font-size: 20px;
            margin: 5px;
            border: 1px solid #000; /* הוסף גבול */
        }
        .controls {
            text-align: center;
            margin-top: 20px;
        }
        .row {
            margin: 10px;
        }
        .camera-container {
            text-align: center;
            margin-bottom: 20px;
        }
        #camera-stream {
            max-width: 100%;
            height: auto;
            margin: 0 auto;
            border: 2px solid #333;
        }
    </style>
</head>
<body>
    <div class="camera-container">
        <img id="camera-stream" src="/stream" alt="Camera Stream" onerror="console.log('Stream error!'); reloadImage()" 
     onload="console.log('Stream loaded!')"/>
    </div>
    <div class="controls">
        <div class="row">
            <button class="button" onmousedown="sendCommand('W')" onmouseup="sendCommand('Q')">Forward</button>
        </div>
        <div class="row">
            <button class="button" onmousedown="sendCommand('A')" onmouseup="sendCommand('Q')">Left</button>
            <button class="button" onmousedown="sendCommand('S')" onmouseup="sendCommand('Q')">Backward</button>
            <button class="button" onmousedown="sendCommand('D')" onmouseup="sendCommand('Q')">Right</button>
        </div>
    </div>

    <script>
        function sendCommand(command) {
            console.log("Sending command: " + command); // Debugging
            fetch('/control?cmd=' + command, { timeout: 5000 }) // הוסף timeout
                .then(response => {
                    if (!response.ok) {
                        throw new Error("Network response was not ok");
                    }
                    return response.text();
                })
                .then(data => console.log(data))
                .catch(error => {
                    console.error("Error sending command:", error);
                });
        }
        
        function reloadImage() {
            console.log("Reloading image..."); // Debugging
            fetch('/stream')
                .then(response => {
                    if (response.ok) {
                        document.getElementById('camera-stream').src = '/stream?' + new Date().getTime();
                    } else {
                        console.error("Server not responding");
                    }
                })
                .catch(error => {
                    console.error("Error fetching stream:", error);
                });
        }
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
   
  setupCamera();

  pinMode(motor_rf,OUTPUT);
  pinMode(motor_rb,OUTPUT);
  pinMode(motor_lf,OUTPUT);
  pinMode(motor_lb,OUTPUT);

     // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/stream", HTTP_GET, handle_stream);

    // Define web server routes
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", html);       
    });

    
 server.on("/control", HTTP_GET, []() {
    String cmd = server.arg("cmd");
    Serial.printf("Received command: %s\n", cmd.c_str()); // Debugging
    if (cmd == "W" || cmd == "w") {
        Forward();
    } else if (cmd == "S" || cmd == "s") {
        Backward();
    } else if (cmd == "A" || cmd == "a") {
        Left();
    } else if (cmd == "D" || cmd == "d") {
        Right();
    } else if (cmd == "Q" || cmd == "q") {
        initialMotor();
    }
    server.send(200, "text/plain", "OK");
});

    server.begin();
}

void loop() {
 server.handleClient();  
}