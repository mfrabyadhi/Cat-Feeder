/*
  Rui Santos
  Complete project details at our blog: https://RandomNerdTutorials.com/esp32-data-logging-firebase-realtime-database/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time.h"
#include <ESP32Servo.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

bool feed = false, sens = false;
unsigned long last_time;
int feedcount = 0;

#define WIFI_SSID "********"
#define WIFI_PASSWORD "********"

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

void reconnectWiFi(){
  if(WiFi.status() != WL_CONNECTED){
    initWiFi();
	
  }
}

#define FIREBASE_HOST "********"
#define FIREBASE_AUTH "********"
#define USER_EMAIL "********"
#define USER_PASSWORD "********"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

String uid; 
String path;

void initFirebase(){
  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_AUTH;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  while(auth.token.uid.length() == 0){
    Serial.print(".");
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.println(uid);
}

void sendData(unsigned long ft, String ft_s, String fd, int fc){
  path = uid + "/" + String(ft);
  json.set("feedtime", ft_s);
  json.set("feeddate:", fd);
  json.set("feedcount", fc);
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json);
}

// thingsboard
#include <PubSubClient.h>
#include <ArduinoJson.h>

constexpr char TOKEN[] = "1achwi6dypuhgnk74l4o";
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";
constexpr uint16_t THINGSBOARD_PORT = 1883U;
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  int requestId;
  feed = true;
}

void initThingsboard(){
  client.setServer(THINGSBOARD_SERVER, THINGSBOARD_PORT);
  client.setCallback(callback);
  while(!client.connected()){
  Serial.println("Connecting to Thingsboard");
  if(client.connect("ESP32Client", TOKEN, NULL)){
    Serial.println("Connected to Thingsboard");
      client.subscribe("v1/devices/me/rpc/request/+");
  } else {
    Serial.print("Failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
  }
}

void reconnectThingsboard(){
  if(!client.connected()){
  initThingsboard();
  }
  client.loop();
}

const char* ntpServer = "pool.ntp.org";

void init_time(){
  configTime(0, 1800, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1702210253) {
    delay(500);
    Serial.print(".");  
    time(&now);
  }
  Serial.println("");
}

unsigned long get_time(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 0;
  }
  return timeinfo.tm_hour*3600 + timeinfo.tm_min*60 + timeinfo.tm_sec;
}

String get_time_str(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
	Serial.println("Failed to obtain time");
	return "";
  }
  return String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
}

String get_date_str(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
	Serial.println("Failed to obtain time");
	return "";
  }
  return String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900);
}

#include <NewPing.h>
const int ULTRASONIC_TRIG_PIN = 12;
const int ULTRASONIC_ECHO_PIN = 13;

NewPing sonar(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN, 200);

bool get_sens(){
  int dist = sonar.ping_cm();
  Serial.println(dist);
  return (dist < 10);
}

#define SERVO_PIN 5
Servo servo;
TaskHandle_t servoTaskHandle = NULL;

void init_servo(){
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write(0);
}

void servoTask(void *pvParameters){
  init_servo();
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    servo.write(180);
    delay(2000);
    servo.write(0);
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initFirebase();
  initThingsboard();
  configTime(7 * 3600, 0, ntpServer, "time.nist.gov");
  xTaskCreatePinnedToCore(servoTask, "servoTask", 10000, NULL, 1, &servoTaskHandle, 1);
}

void loop() {
  reconnectWiFi();
  reconnectThingsboard();
  sens = get_sens();
  Serial.println(millis() - last_time);
  if((millis() - last_time > 6 * 3600000) || feed || (sens && (millis() - last_time > 30000))){
    Serial.println("In");
    sendData(get_time(), get_time_str(), get_date_str(), feedcount);
    xTaskNotifyGive(servoTaskHandle);
    last_time = millis();
	feedcount++;
	feed = false;
	sens = false;
  }
  client.loop();
  delay(100);
}