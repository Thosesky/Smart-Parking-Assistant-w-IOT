#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiS3.h>

const char* ssid = "121014_KMITL_2.4G"; 
const char* password = "57304289";

String apiKey = "IUW2T0X8XD2GQAC1"; 
const char* server = "api.thingspeak.com";

WiFiClient client;

LiquidCrystal_I2C lcd(0x27, 16, 2); 

const int trigPin = 10;
const int echoPin = 11;
int warningDistance = 30; 

const int mcp9700Pin = A0; 
const int maxTempLimit = 70; 
float currentTemp = 0.0;
unsigned long lastTempRead = 0;

const int buzzerPin = 12; 

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 

String inputBuffer = ""; 

unsigned long lastThingSpeakUpdate = 0;
const unsigned long thingspeakInterval = 15000; 

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }

  lcd.clear();
  lcd.print("WiFi Connected!");
  delay(1000);
  lcd.clear();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  for (int i = 0; i < ROWS; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for (int i = 0; i < COLS; i++) pinMode(colPins[i], INPUT);
}

void loop() {
  currentTemp = readTemperature();

  int currentDistance = getDistance();

  char key = getCustomKey();

  if (key) {
    if (key >= '0' && key <= '9') {
      if (inputBuffer.length() < 3) {
        inputBuffer += key;
      }
    } 
    else if (key == 'A') {
      if (inputBuffer.length() > 0) {
        warningDistance = inputBuffer.toInt();
        inputBuffer = "";
        lcd.clear();
        lcd.print("Set Limit: Saved!");
        delay(1000);
        lcd.clear();
      }
    }
    else if (key == 'C') {
      inputBuffer = "";
    }
  }

  bool isTempOverheat = (currentTemp >= maxTempLimit);
  bool isTooClose = (currentDistance <= warningDistance);

  if (isTempOverheat || isTooClose) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  lcd.setCursor(0, 0);
  lcd.print("D:");
  lcd.print(currentDistance);
  lcd.print("cm ");

  lcd.setCursor(9, 0);
  lcd.print("T:");
  lcd.print((int)currentTemp);
  if (isTempOverheat) {
    lcd.print("C! ");
  } else {
    lcd.print((char)223);
    lcd.print("C  ");
  }

  lcd.setCursor(0, 1);
  if (inputBuffer.length() > 0) {
    lcd.print("Set Limit: ");
    lcd.print(inputBuffer);
    lcd.print("  ");
  } 
  else if (isTempOverheat) {
    lcd.print("!! OVERHEAT !!  ");
  } 
  else if (isTooClose) {
    lcd.print("!!! STOP !!!    ");
  } 
  else {
    lcd.print("Safe (Lim:");
    lcd.print(warningDistance);
    lcd.print(") ");
  }

  if (millis() - lastThingSpeakUpdate >= thingspeakInterval) {
    lastThingSpeakUpdate = millis();

    int liveDist = getDistance();
    float liveTemp = readTemperature();

    sendDataToThingSpeak(liveDist, liveTemp);
  }

  delay(100);
}

float readTemperature() {
  long sumRaw = 0;
  for (int i = 0; i < 10; i++) {
    sumRaw += analogRead(mcp9700Pin);
    delay(2);
  }
  float avgRaw = sumRaw / 10.0;
  float voltage = avgRaw * (5.0 / 1023.0);
  return (voltage - 0.5) * 100.0;
}

void sendDataToThingSpeak(int distance, float temp) {
  if (client.connect(server, 80)) {
    String url = "/update?api_key=" + apiKey + 
                 "&field1=" + String(distance) + 
                 "&field2=" + String(temp) + 
                 "&field3=" + String(warningDistance);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.thingspeak.com\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.print("-> Sent to ThingSpeak | Dist: ");
    Serial.print(distance);
    Serial.print(" cm, Temp: ");
    Serial.print(temp);
    Serial.print(" C, Limit: ");
    Serial.println(warningDistance);
  } else {
    Serial.println("-> Connection to ThingSpeak Failed!");
  }
  client.stop();
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

char getCustomKey() {
  char pressedKey = '\0';
  for (int c = 0; c < COLS; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], LOW);

    for (int r = 0; r < ROWS; r++) {
      if (digitalRead(rowPins[r]) == LOW) {
        pressedKey = keys[r][c];
        delay(250); 
      }
    }
    pinMode(colPins[c], INPUT);
  }
  return pressedKey;
}
