#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "HX711.h"

const char *ssid = "NAMA_WIFI";
const char *password = "PASSWORD_WIFI";

#define MOTOR_IN1 23
#define MOTOR_IN2 24
#define MOTOR_EN 25

#define SERVO_PIN 17

#define LOADCELL_DT_PIN 5
#define LOADCELL_SCK_PIN 7

WebServer server(80);
Servo sorterServo;
HX711 scale;

const int SERVO_IDLE = 0;
const int SERVO_REJECT = 90;
const int SERVO_DELAY_MS = 500;

float calibration_factor = 1.0;

const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;

void setupMotor()
{
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  ledcAttach(MOTOR_EN, PWM_FREQ, PWM_RESOLUTION);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_EN, 0);
}

void motorForward(int speedMotor)
{
  speedMotor = constrain(speedMotor, 0, 255);
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_EN, speedMotor);
}

void motorStop()
{
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_EN, 0);
}

void rejectServo()
{
  sorterServo.write(SERVO_REJECT);
  delay(SERVO_DELAY_MS);
  sorterServo.write(SERVO_IDLE);
}

float getWeight()
{
  if (scale.is_ready())
  {
    return scale.get_units(5);
  }
  return -1;
}

void handleControl()
{
  String startHeader = server.header("X-Start");
  String predictionHeader = server.header("X-Prediction");
  String speedHeader = server.header("X-Speed");
  String durationHeader = server.header("X-Duration");

  bool start = startHeader == "1";
  int prediction = predictionHeader.toInt();
  int motorSpeed = speedHeader.length() > 0 ? speedHeader.toInt() : 180;
  int motorDuration = durationHeader.length() > 0 ? durationHeader.toInt() : 2000;

  motorSpeed = constrain(motorSpeed, 0, 255);

  if (!start)
  {
    server.send(200, "application/json", "{\"status\":\"idle\"}");
    return;
  }

  motorForward(motorSpeed);
  delay(motorDuration);
  motorStop();

  float weight = 0;

  if (prediction == 1)
  {
    rejectServo();
    weight = 0;
  }
  else
  {
    delay(500);
    weight = getWeight();
  }

  String response = "{";
  response += "\"status\":\"ok\",";
  response += "\"prediction\":" + String(prediction) + ",";
  response += "\"motor_speed\":" + String(motorSpeed) + ",";
  response += "\"motor_duration\":" + String(motorDuration) + ",";
  response += "\"weight\":" + String(weight);
  response += "}";

  server.send(200, "application/json", response);
}

void handleTest()
{
  motorForward(180);
  delay(2000);
  motorStop();
  rejectServo();
  float weight = getWeight();

  String response = "{";
  response += "\"status\":\"test_done\",";
  response += "\"weight\":" + String(weight);
  response += "}";

  server.send(200, "application/json", response);
}

void setup()
{
  Serial.begin(115200);

  setupMotor();

  sorterServo.attach(SERVO_PIN);
  sorterServo.write(SERVO_IDLE);

  scale.begin(LOADCELL_DT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  Serial.println(WiFi.localIP());

  const char *headers[] = {
      "X-Start",
      "X-Prediction",
      "X-Speed",
      "X-Duration"};

  server.collectHeaders(headers, 4);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/test", HTTP_GET, handleTest);
  server.begin();
}

void loop()
{
  server.handleClient();
}