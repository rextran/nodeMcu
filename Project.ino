#include "FirebaseArduino.h"
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "DHTesp.h"
#include <Wire.h>
#include <BH1750.h>
Ticker blinker;
BH1750 lightMeter;
DHTesp dht;
//===================== define PIN ===============================
#define DHTpin    15  // 
#define LIGHT     2   // Pin 4
#define waterPump 13  // PIN D8
#define R         4   // PIN D2
#define G         5   // PIN D1
#define B         0   // PIN D3
#define SCL       14  // PIN D5
#define SDA       12  // PIN D6
// ============== Config Wifi and Firebase =======================
#define FIREBASE_HOST "greenhouse-ac4e5.firebaseio.com"
#define FIREBASE_AUTH ""
#define WIFI_SSID     "Testwf"
#define WIFI_PASSWORD "12345678"
// ===================== declare variable ========================
float         humidity, temperature, lux, soilMoisture;
String        modeControl,red, green, blue, lightControl, waterPumpControl, r,g,b;
String        header, ssid, password;
unsigned long previousMillis = 0, startScheduleLight = 0, startScheduleWaterPump = 0;
const long    interval = 30000;
int           timeSchedule;
bool          ScheduleStartLight = 0, ScheduleStartWaterPump = 0;
// ======================== get localtime ========================
const char*   ntpServer = "0.asia.pool.ntp.org";
const long    gmtOffset_sec = 0;
const int     daylightOffset_sec = 3600;
char          buffer[80];
char          localTime;
int           Time;
// ======================= Open Port 80 ==========================
WiFiServer server(80);
// ====================== login wifi =============================
void login_wf() {
  WiFi.softAP("SMART FARM","12345678",1,false);
  delay(2000);
  Serial.println("Mode: login");
  while (WiFi.status() != WL_CONNECTED){
    WiFiClient client = server.available();
    if (client) // Neu co ki tu den
    {
      while (client.connected()) // loop khi client duoc ket noi
      {
        if (client.available()) // 
        {
          char c = client.read();
          Serial.write(c);
          header += c;
          if (header.indexOf("/*END*/") >= 0)
          {
            ssid = header.substring(header.lastIndexOf("/*SSID*/"), header.indexOf("/*password*/"));
            ssid = ssid.substring(8);
            password = header.substring(header.lastIndexOf("/*password*/"), header.indexOf("/*END*/"));
            password = password.substring(12);

            Serial.println("");
            Serial.println(ssid);
            Serial.println(password);
            Serial.println("");
            header = "";

            WiFi.begin(ssid, password);
            Serial.print("Connecting to ");
            Serial.println(ssid);
            delay(10000);
            if (WiFi.status() == WL_CONNECTED)
            {
              client.println("HTTP/1.1 200 CONNECTED");
              client.println("Connection: close");
              client.println();
              Serial.println("Connected successful.");
              Serial.println(WiFi.localIP());
              
              
            }
            else
            {
              client.println("HTTP/1.1 403 FAILED");
              client.println("Connection: close");
              client.println();
              Serial.println("Connected failed.");
            }
            break;
          }
        }
      } 
    }
  }
}
// ===================== read sensor =============================
void readSensors() {
  humidity = 0, temperature = 0, lux = 0, soilMoisture = 0;
  for (int i = 0; i < 6; i++) {
    humidity += dht.getHumidity();
    temperature += dht.getTemperature();
    lux += lightMeter.readLightLevel();
    soilMoisture += analogRead(A0);
    delay (1000);
  }
  soilMoisture = soilMoisture / 6 / 1023 * 100; 
  humidity = humidity / 6;
  temperature = temperature / 6;
  lux = lux / 6;
  if (lux < 100) {
    light_ON();
    Firebase.setString("GreenHouse/Device/Light/Control", "\"on\"");
  }
  else {
    light_OFF();
    Firebase.setString("GreenHouse/Device/Light/Control", "\"off\"");
  }
  if(temperature <= 33 && soilMoisture < 30){
    waterPump_ON();
    Firebase.setString("GreenHouse/Device/WaterPump/Control", "\"on\"");
  }
  else{
    waterPump_OFF();
    Firebase.setString("GreenHouse/Device/WaterPump/Control", "\"off\"");
  }
}

// ===================== get local time ==========================
void getTime() {
  struct tm * timeinfo;
  time_t rawtime;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  timeinfo->tm_hour += 6;
  strftime (buffer, 80, " %d %B %Y %H:%M:%S ", timeinfo);
  Serial.println(buffer);
  timeinfo->tm_mon += 1;
  timeinfo->tm_year += 1900;
  Time = timeinfo->tm_hour * pow(10, 2) + timeinfo->tm_min;
  Firebase.setString("GreenHouse/Time", String(Time));
}

// ===================== up data to firebase =====================
void upData() {
  getTime();
  Serial.print("lux: ");
  Serial.println(lux);
  Serial.print("humidity: ");
  Serial.println(humidity);
  Serial.print("temperature: ");
  Serial.println(temperature);
  Serial.print("soil moisture: ");
  Serial.println(soilMoisture);
  Firebase.setFloat(String("GreenHouse/Data/") + buffer + String("/Temperature"), temperature);
  delay(200);
  Firebase.setFloat(String("GreenHouse/Data/") + buffer + String("/Lux"), lux);
  delay(200);
  Firebase.setFloat(String("GreenHouse/Data/") + buffer + String("/Humidity"), humidity);
  delay(200);
  Firebase.setFloat(String("GreenHouse/Data/") + buffer + String("/soilMoisture"), soilMoisture);
}

// ===================== get data from firebase ==================
void getData() {
  modeControl = Firebase.getString("GreenHouse/Mode");
  red = Firebase.getString("GreenHouse/Device/Light/Color/R");
  green = Firebase.getString("GreenHouse/Device/Light/Color/G");
  blue = Firebase.getString("GreenHouse/Device/Light/Color/B");
  lightControl = Firebase.getString("GreenHouse/Device/Light/Control");
  waterPumpControl = Firebase.getString("GreenHouse/Device/WaterPump/Control");

}

// ===================== schedule Light ==========================
void scheduleLight() {
  unsigned long currentLight = millis();
  String light_schedule = Firebase.getString("GreenHouse/Schedule/Light/Check");
  delay(200);
  if (light_schedule == "true") {
    String timeLight = Firebase.getString("GreenHouse/Schedule/Light/TimeStartGet");
    delay(200);
    String durationLight = Firebase.getString("GreenHouse/Schedule/Light/Duration");
    durationLight.replace("\"", "");
    Serial.println("time before schedule" +  String(startScheduleLight));
    if ((0 <= Time - timeLight.toInt()) && (Time - timeLight.toInt() <= 1) && (!ScheduleStartLight)) {
      light_ON();
      Firebase.setString("GreenHouse/Device/Light/Control", "\"on\"");
      startScheduleLight = millis();
      Serial.println("light on schedule!");
      ScheduleStartLight = 1;
    }
    if ((ScheduleStartLight) && (millis() - startScheduleLight >= durationLight.toInt() * 60 * 1000)) {
      light_OFF();
      Firebase.setString("GreenHouse/Device/Light/Control", "\"off\"");
      Firebase.setString("GreenHouse/Schedule/Light/Check", "false");
      ScheduleStartLight = 0;
      Serial.println("Light off. Schedule end!");
    }
  }
  else {
    Serial.println("Light is not scheduled");
    String WaterPumpScheduleCheck = Firebase.getString("GreenHouse/Schedule/WaterPump/Check");
    delay(200);
    Serial.println(WaterPumpScheduleCheck);
    if(WaterPumpScheduleCheck == "false"){
      modeControl = "\"auto\"";
      Firebase.setString("GreenHouse/Mode", "\"auto\"");
      Serial.println(modeControl);
    }
  }

}

// ===================== schedule Water Pump =====================
void scheduleWaterPump() {
  unsigned long current = millis();
  String waterPump_schedule = Firebase.getString("GreenHouse/Schedule/WaterPump/Check");
  delay(200);
  if (waterPump_schedule == "true") {
    String timeWaterPump = Firebase.getString("GreenHouse/Schedule/WaterPump/TimeStartGet");
    delay(200);
    String durationWaterPump  = Firebase.getString("GreenHouse/Schedule/WaterPump/Duration");
    Serial.println(durationWaterPump);
    durationWaterPump.replace("\"", "");
    if ((0 <= Time - timeWaterPump.toInt()) && (Time - timeWaterPump.toInt() <= 1) && (!ScheduleStartWaterPump)) {
      waterPump_ON();
      Firebase.setString("GreenHouse/Device/WaterPump/Control", "\"on\"");
      startScheduleWaterPump = millis();
      ScheduleStartWaterPump = 1;
      Serial.println("waterPump On schedule!");
    }
    Serial.println(millis() - startScheduleWaterPump);
    if (ScheduleStartWaterPump && (millis() - startScheduleWaterPump >= durationWaterPump.toInt() * 60 * 1000)) {
      waterPump_OFF();
      Firebase.setString("GreenHouse/Device/WaterPump/Control", "\"off\"");
      Firebase.setString("GreenHouse/Schedule/WaterPump/Check", "false");
      ScheduleStartWaterPump = 0;
      Serial.println("waterPump Off. Schedule End!");
    }
  }
  else {
    Serial.println("Water pump is not scheduled");
    String LightScheduleCheck = Firebase.getString("GreenHouse/Schedule/Light/Check");
    delay(200);
    Serial.println(LightScheduleCheck);
    if(LightScheduleCheck == "false"){
      Firebase.setString("GreenHouse/Mode", "\"auto\"");
      modeControl = "\"auto\"";
      Serial.println(modeControl);
    }
  }
}

// ===================== turn on the led rgb =====================
void light_ON() {
  analogWrite(R, red.toInt());
  analogWrite(G, green.toInt());
  analogWrite(B, blue.toInt());
}

// ===================== turn off the led rgb ====================
void light_OFF() {
  analogWrite(R, 0);
  analogWrite(G, 0);
  analogWrite(B, 0);
}
// ==================== turn on water pump ======================
void waterPump_ON() {
  digitalWrite(waterPump, HIGH);

}

// =================== turn off water Pump ======================
void waterPump_OFF() {
  digitalWrite(waterPump, LOW);

}

// ================== manual ====================================
void manual() {
  if (lightControl == "\"on\"") {
    light_ON();
    Serial.println("light on");
  }
  else if(lightControl == "\"off\"") {
    light_OFF();
    Serial.println("light off");
  }
  if (waterPumpControl == "\"on\"") {
    waterPump_ON();
    Serial.println("water pump on");
  }
  else if (waterPumpControl == "\"off\""){
    waterPump_OFF();
    Serial.println("water pump off");
  }
}

// ==================== setup ===================================
void setup()
{
  Serial.begin(115200);
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(LIGHT, OUTPUT);
  pinMode(waterPump, OUTPUT);
  digitalWrite(LIGHT, HIGH);
  dht.setup(DHTpin, DHTesp::DHT22);
  Wire.begin(SDA, SCL); //SCL - D3, SDA - D4
  lightMeter.begin();
  server.begin();
  delay(3000);
  login_wf();
  Serial.println("oke");
  delay(2000);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(modeControl);
}

void loop() {

  unsigned long currentMillis = millis();
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(LIGHT, LOW);
    getData();
  }
  else{
    digitalWrite(LIGHT, HIGH);
    modeControl = "\"auto\"";
  }
  if (modeControl == "\"login\"") {
    login_wf();
  }
  else if (modeControl == "\"auto\"") {
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      readSensors();
      upData();
    }
  }
  else if (modeControl == "\"manual\"") {
    manual();
    Serial.println("manual");
  }
  else if (modeControl == "\"schedule\"") {
    getTime(); // get Time variable
    Serial.println("schedule starting!");
    scheduleLight();
    scheduleWaterPump();
  }
  else {
    Firebase.setString("GreenHouse/Mode", "\"auto\"");
    modeControl = "\"auto\"";
  }
}
