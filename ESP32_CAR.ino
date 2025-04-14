#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

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
//bool manualMode = true; // for the obstacle detect and pass the obstacle
// Web server
WebServer server(80);

// FreeRTOS variables
QueueHandle_t commandQueue = NULL;
TaskHandle_t cameraTaskHandle = NULL;
char currentCommand = 'Q';
SemaphoreHandle_t cameraMutex = NULL;

unsigned long lastGloveSignal = 0; // the last time that the glove send command
bool gloveActive = false; // if the glove is in control

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
  digitalWrite(motor_rf, LOW);  // Right side moves
  digitalWrite(motor_lb, LOW);
  digitalWrite(motor_lf, HIGH);   // Left side stops
}

void Right() {
  // Left motor forward, Right motor stop
  digitalWrite(motor_rb, LOW);
  digitalWrite(motor_rf, HIGH);   // Right side stops
  digitalWrite(motor_lb, LOW);
  digitalWrite(motor_lf, LOW);  // Left side moves
}

void initialMotor(){
  digitalWrite(motor_rb,LOW);
  digitalWrite(motor_rf,LOW);
  digitalWrite(motor_lb,LOW);
  digitalWrite(motor_lf,LOW);
}

//thread for the commands
void commandTask(void* arg){
    char cmd;
    while(true){
        if(xQueueReceive(commandQueue, &cmd, portMAX_DELAY)){
            executeCommand(cmd);
            Serial.printf("Executed: %c\n", cmd);
        }
    }
    vTaskDelete(NULL);
}

void handleControl(){
    if(server.method() == HTTP_OPTIONS){
        server.send(204);
        return;
    }

    if(!server.hasArg("cmd")){
        server.send(400, "text/plain", "No command");
        return;
    }

    char cmd = server.arg("cmd")[0];
    String source = server.arg("source");

    if(strchr("QWASD", cmd) == NULL){
        server.send(400, "text/plain", "Invalid command");
        return;
    }

    if (source == "glove") {
    lastGloveSignal = millis(); // update time for the glove
    gloveActive = true;
    if (xQueueSend(commandQueue, &cmd, 0) == pdPASS) {
      server.send(200, "text/plain", "Glove command received");
    } else {
      server.send(500, "text/plain", "Queue full");
    }
  } else if (source == "remote" && !gloveActive) { // the web remote will work just if the glove remote is down
    if (xQueueSend(commandQueue, &cmd, 0) == pdPASS) {
      server.send(200, "text/plain", "Remote command received");
    } else {
      server.send(500, "text/plain", "Queue full");
    }
  } else {
    server.send(200, "text/plain", "Command ignored (glove active)");
  }
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
  else if (err == ESP_ERR_INVALID_STATE) {
        Serial.println("Camera already initialized or in an invalid state.");
    } else if (err == ESP_ERR_NO_MEM) {
        Serial.println("Not enough memory.");
    } else if (err == ESP_ERR_NOT_FOUND) {
        Serial.println("Camera not detected.");
    } else {
        Serial.println("Unknown error.");
    }
}

// Camera task function
void cameraTask(void *pvParameters) {
  Serial.printf("cameraTask running on core %d\n", xPortGetCoreID());
  while(1) {
    if(esp_camera_sensor_get() == NULL) {
      setupCamera(); // Reinitialize if needed
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void handle_stream() {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println();
    xSemaphoreTake(cameraMutex, portMAX_DELAY);

    unsigned long lastFrame = 0;
    while(client.connected()) {
        // Maintain 100ms frame interval (~10 FPS)
        if(millis() - lastFrame >= 100) {
            camera_fb_t *fb = esp_camera_fb_get();
            if(fb) {
                String header = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
                client.write(header.c_str(), header.length());
                client.write(fb->buf, fb->len);
                client.println("\r\n");
                esp_camera_fb_return(fb);
                lastFrame = millis();
            }
        }
        xSemaphoreGive(cameraMutex);
        
        // Allow server to handle other requests
        server.handleClient();
        delay(1);
    }
}

void executeCommand(char cmd){
    switch (cmd){
    case 'W':
        Forward();
        break;
    case 'S':
        Backward();
        break;
    case 'A':
        Left();
        break;
    case 'D':
        Right();
        break;
    case 'Q':
        initialMotor();
        break;
    default:
       break;    
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
            border: 1px solid #000; 
        }

     .button:active {
          background-color: #45a049;
        }    
        .controls {
            text-align: center;
            margin-top: 20px;
        }
        .row {
            margin:10px;
        }
        .camera-container {
            text-align: center;
            margin-bottom: 20px;
        }
        #camera-stream {
            max-width: 50%;
            height: auto;
            margin: 0 auto;
            border: 2px solid #333;
        }
    </style>
</head>
<body>
    <div class="camera-container">
      <img id="camera-stream" src="/stream" alt="Camera Stream" style="max-width: 50%; border: 2px solid #333;">
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
            console.log("Sending command: " + command);
            fetch('/control?source=remote&cmd=' + command, { method: "GET" })
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
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  cameraMutex = xSemaphoreCreateMutex();

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

  // Create command queue
  commandQueue = xQueueCreate(10, sizeof(char));
  if(!commandQueue){
    Serial.println("failed");
  }

  // Create tasks
  xTaskCreatePinnedToCore(
    commandTask,
    "CommandTask",
    4096,
    NULL,
    3,  // Higher priority than camera task
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    cameraTask,
    "CameraTask",
    4096,
    NULL,
    1,  // Lower priority than command task
    NULL,//&cameraTaskHandle,
    1
  );

    // Define web server routes
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", html);       
    });

    server.on("/stream", HTTP_GET, handle_stream);

    server.on("/control", HTTP_GET, handleControl);
    
    server.begin();
    Serial.printf("server begin");
}

void loop() {
  executeCommand('W');
  executeCommand('Q');
 server.handleClient();
 vTaskDelay(5 / portTICK_PERIOD_MS);  
}
