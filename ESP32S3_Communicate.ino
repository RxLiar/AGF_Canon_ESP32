#include <WiFi.h>
// #include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <esp_wifi.h>

const char* ssid = "JIG-MOLD";
const char* password = "12345678901234567890123456";
// const char* ssid = "AGF_Canon_1";
// const char* password = "MKAC12345";
const char* espHostname = "MKAC-AGF";

// Static IP configuration
// IPAddress local_IP(192,168, 0, 254);
// IPAddress gateway(192, 168, 0, 1);
// IPAddress subnet(255, 255, zz55, 0);

long int cnt_mb = 0;
long int cnt_step = 0;
#define ST1 1000
#define ST2 1200
#define ST3 1400
#define ST4 1600
#define ST5 2000


// Set up WiFi server on port 80
WiFiServer server(80);
WiFiClient client_;
bool clientConnected = false;

SoftwareSerial mySerial1(2, 1);
SoftwareSerial mySerial2(41, 42);


float distance_sensor_1 = 0.0;
float distance_sensor_2 = 0.0;
int cout_error_sensor_1 = 0;
int cout_error_sensor_2 = 0;
char buff[4] = { 0x80, 0x06, 0x03, 0x77 };
unsigned char data1[11] = { 0 };
unsigned char data2[11] = { 0 };

// Sensor and control pins
// const int sensorPin1 = 37;
// const int sensorPin2 = 38;

const int led_stop = 7;           // Đèn đỏ, báo băng tải hoạt động với AGF
const int led_start = 6;          // Đèn xanh, báo băng tải hoạt động chế độ thường
const int request_agf_mode = 15;  // yêu cầu chuyển băng tải sang chế độ hoạt động với AGF


// update in out canon
const int in1 = 35;   // Băng tải chuyển sang chế độ hoạt động vs AGF
const int in2 = 36;   // Cho phép chuyển sang chế độ hoạt động với AGF
const int out1 = 16;  // yêu cầu chuyển băng tải sang chế độ hoạt động với AGF

bool state_in1;
bool state_in2;
bool enable_AGF_controll;
bool ready_conveyor; 

String responseConveyor = "$1*";

void setup() {
  // Setup Serial Monitor
  Serial.begin(115200);
  delay(100);

  // Setup WiFi
  ///  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  readMacAddress();
  server.begin();

  // Configure IO pins

  // pinMode(sensorPin1, INPUT);
  // pinMode(sensorPin2, INPUT);
  pinMode(request_agf_mode, OUTPUT);
  pinMode(led_start, OUTPUT);
  pinMode(led_stop, OUTPUT);
  digitalWrite(request_agf_mode, LOW);  // Bật băng tải mặc định, kéo xuống 0V

  // Update trạng thái chân in out bổ sung

  pinMode(in1, INPUT);
  pinMode(in2, INPUT);
  pinMode(out1, OUTPUT);

  mySerial1.begin(9600);
  mySerial2.begin(9600);
  mySerial1.write(buff, sizeof(buff));
  mySerial2.write(buff, sizeof(buff));
  digitalWrite(led_start, LOW);  // Kéo xuống 0V, bật đèn xanh báo hệ thống hoạt động
  digitalWrite(led_stop, HIGH);
}

float getDistanceSensor1() {
  float distance = -0.1;
  if (mySerial1.available() > 0) {
    for (int i = 0; i < 11; i++) {
      data1[i] = mySerial1.read();
    }
    unsigned char Check = 0;
    for (int i = 0; i < 10; i++) {
      Check = Check + data1[i];
    }
    Check = ~Check + 1;
    if (data1[10] == Check) {
      if (data1[3] == 'E' && data1[4] == 'R' && data1[5] == 'R') {
        Serial.println("Out of range");
      } else {
        distance = (data1[3] - 0x30) * 100 + (data1[4] - 0x30) * 10 + (data1[5] - 0x30) * 1 + (data1[7] - 0x30) * 0.1 + (data1[8] - 0x30) * 0.01 + (data1[9] - 0x30) * 0.001;
      }
    } else {
      // Serial.println("Invalid Data from Sensor 1!");
    }
  }
  return distance;
}

float getDistanceSensor2() {
  float distance = -0.1;
  if (mySerial2.available() > 0) {
    for (int i = 0; i < 11; i++) {
      data2[i] = mySerial2.read();
    }
    unsigned char Check1 = 0;
    for (int i = 0; i < 10; i++) {
      Check1 = Check1 + data2[i];
    }
    Check1 = ~Check1 + 1;
    if (data2[10] == Check1) {
      if (data2[3] == 'E' && data2[4] == 'R' && data2[5] == 'R') {
        Serial.println("Out of range (Sensor 2)");
      } else {
        distance = (data2[3] - 0x30) * 100 + (data2[4] - 0x30) * 10 + (data2[5] - 0x30) * 1 + (data2[7] - 0x30) * 0.1 + (data2[8] - 0x30) * 0.01 + (data2[9] - 0x30) * 0.001;
      }
    } else {
      // Serial.println("Invalid Data from Sensor 2!");
    }
  }
  return distance;
}

void control_conveyor(String request) {

  //  Đọc Trạng thái Cảm biến pallet-in1
  if (digitalRead(in2) == 1) {
    enable_AGF_controll = HIGH;
  } else {
    enable_AGF_controll = LOW;
  }

  //  Băng tải không sẵn sàng hoạt động với AGF
  if (enable_AGF_controll == LOW) {
    digitalWrite(request_agf_mode, LOW);  // Kéo xuống 0V, không cho tắt băng tải
    Serial.println("Không sẵn sàng hoạt động với AGF");
    if (request.indexOf("stop") != -1) {
      responseConveyor = "$NG_1*";
    } else if (request.indexOf("start") != -1) {
      responseConveyor = "$STARTED*";
    }
  }
  // Băng tải sẵn sàng hoạt động với AGF
  else if (enable_AGF_controll == HIGH) {
    if (request.indexOf("stop") != -1) {
      digitalWrite(request_agf_mode, HIGH);  // Tắt băng tải, kéo lên 24v
      digitalWrite(out1, HIGH);              // Tắt băng tải, kéo lên 24v
      digitalWrite(led_start, HIGH);
      digitalWrite(led_stop, LOW);
      responseConveyor = "$STOPPED*";
      Serial.println("convey stopped");
    } else if (request.indexOf("start") != -1) {
      digitalWrite(request_agf_mode, LOW);  // Bật băng tải, kéo về 0v
      digitalWrite(out1, LOW);              // Bật băng tải, kéo về 0v
      digitalWrite(led_stop, HIGH);
      digitalWrite(led_start, LOW);
      responseConveyor = "$STARTED*";
    }
  }
}


void readMacAddress() {
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void loop() {
  if (true) {
    //Đọc trạng thái các đầu vào in1, in2 của PLC băng tải
    //in1 điều khiển đèn, in2 tín hiệu cho phép dừng băng tải
    state_in1 = digitalRead(in1);  // Lưu trạng thái in1, phản hồi tín hiệu của băng tải canon
    state_in2 = digitalRead(in2);  // Lưu trạng thái in2 cảm biến
    if (state_in2 == 1) {
      ready_conveyor = 1;  // Trạng thái băng tải sẵn sàng hoạt động
    } else if (state_in2 == 0) {
      ready_conveyor = 0;  // Trạng thái băng tải không sẵn sàng hoạt động
    }
  }
  float distance1 = getDistanceSensor1();
  // Serial.print("distance1 raw Sensor 1 = ");
  // Serial.print(distance1, 3);
  // Serial.println(" M");
  if (distance1 > 0.0 && distance1 < 30.0) {
    distance_sensor_1 = distance1;
    cout_error_sensor_1 = 0;
  } else {
    cout_error_sensor_1 += 1;
  }
  float distance2 = getDistanceSensor2();
  // Serial.print("distance2 raw Sensor 2 = ");
  // Serial.print(distance2, 3);
  // Serial.println(" M");
  if (distance2 > 0.0 && distance2 < 30.0) {
    distance_sensor_2 = distance2;
    cout_error_sensor_2 = 0;
  } else {
    cout_error_sensor_2 += 1;
  }
  if (cout_error_sensor_1 < 10 && cout_error_sensor_2 < 10) {
    //    Serial.print("distance1 from Sensor 1 = ");
    //    Serial.print(distance_sensor_1, 3);
    //    Serial.println(" M");
    //    Serial.print("distance2 from Sensor 2 = ");
    //    Serial.print(distance_sensor_2, 3);
    //    Serial.println(" M");
  } else {
    distance_sensor_1 = -0.1;
    distance_sensor_2 = -0.1;
  }

  if (!clientConnected) {
    client_ = server.available();
    if (client_) {
      clientConnected = true;
      Serial.println("Client mới đã kết nối");
    }
  }
  if (clientConnected) {
    if (client_.available()) {
      String request = client_.readStringUntil('/n');
      Serial.print("request: ");
      Serial.println(request);
      if (request.indexOf("stop") != -1 || request.indexOf("start") != -1) {
        if (ready_conveyor == 1) {
          control_conveyor(request);
          Serial.println("conveyor controlling..");
          Serial.println(responseConveyor);
        } else if (ready_conveyor == 0) {
          Serial.print("conveyor can't controll..");
        }
      }
    }
    String response = "#" + String(distance_sensor_1, 3) + "#" + String(distance_sensor_2, 3) + "%" + responseConveyor;
    client_.println(response);
  }

  if (!client_.connected()) {
    client_.stop();
    clientConnected = false;
  }
  delay(50);
}
