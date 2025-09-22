#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>

// ---------- WiFi ----------
const char* ssid = "KARTHI";
const char* password = "karthi police";

// ---------- InfluxDB ----------
const char* influxURL = "https://us-east-1-1.aws.cloud2.influxdata.com/api/v2/write?org=Data_base&bucket=esp32&precision=s";
const char* token = "7geVbMvR9y7Xhu6EChuOs68-yzQF4KGUoZFA2z2KlTzlZlygBIVy3mhQpS4zuPRCdXKCjGrFn4gZW3Ai4Ws-8A==";

// ---------- CircuitDigest SMS ----------
const char* apiKey = "XXXXXXXXX";         // replace with your API key
const char* templateID = "101";              // replace with your template ID
const char* mobileNumber = "910000000000";   // your phone number
const char* var1 = "field is  ";                      // optional template variables
const char* var2 = "risk occupied by elephant ";                      // optional template variables

// ---------- Pins ----------
const int trigPin = 5;
const int echoPin = 18;
const int greenLED = 2;
const int redLED = 4;
const int buzzerPin = 23;
const int servoPin = 22;

// ---------- Objects ----------
Servo myservo;
WiFiClientSecure client;

// ---------- Threshold ----------
const int thresholdDistance = 20; // cm

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  myservo.attach(servoPin);

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  client.setInsecure(); // skip certificate check
}

// ---------- Read Distance ----------
long readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// ---------- Send to InfluxDB ----------
void sendToInflux(long distance, bool detected) {
  HTTPClient http;
  http.begin(client, influxURL);
  http.addHeader("Authorization", String("Token ") + token);
  http.addHeader("Content-Type", "text/plain");

  String status = detected ? "Detected" : "Clear";
  String data = "animal_intrusion,device=esp32 distance=" + String(distance) + ",status=\"" + status + "\"";

  int code = http.POST(data);
  Serial.print("InfluxDB Response Code: ");
  Serial.println(code);
  http.end();
}

// ---------- Send SMS via CircuitDigest ----------
void sendSMS() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "http://www.circuitdigest.cloud/send_sms?ID=" + String(templateID);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", apiKey);

    String payload = "{\"mobiles\":\"" + String(mobileNumber) +
                     "\",\"var1\":\"" + String(var1) +
                     "\",\"var2\":\"" + String(var2) + "\"}";

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
      Serial.println("SMS Sent Successfully!");
    } else {
      Serial.print("Error Sending SMS. Code: ");
      Serial.println(httpResponseCode);
    }

    String response = http.getString();
    Serial.println(response);

    http.end();
  } else {
    Serial.println("WiFi not connected - cannot send SMS");
  }
}

void loop() {
  // Sweep servo to cover area
  for (int pos = 30; pos <= 150; pos += 30) {
    myservo.write(pos);
    delay(500);

    long distance = readDistance();
    Serial.print("Distance: ");
    Serial.println(distance);

    bool detected = (distance > 0 && distance < thresholdDistance);

    if (detected) {
      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);
      digitalWrite(buzzerPin, HIGH);

      sendToInflux(distance, true);
      sendSMS(); // send SMS

      // Prevent SMS spamming: wait until distance increases
      while (readDistance() < thresholdDistance) {
        delay(500);
      }
    } else {
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, HIGH);
      digitalWrite(buzzerPin, LOW);

      sendToInflux(distance, false);
    }
  }
}
