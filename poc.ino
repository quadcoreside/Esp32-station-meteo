#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <LiquidCrystal_I2C.h>

#include <ESP32Servo.h>
#include <analogWrite.h>
#include <ESP32PWM.h>

#define IS_DEBUG 1
#define SEUIL_SUP 3000
#define SEUIL_INF 2500

#define uS_TO_mS_FACTOR 1000ULL
#define TIME_TO_SLEEP 5000 // 5 secondes

#define BACK_LIGHT_TIME 5000 // 5 secondes

#define BUTTON_PIN 33
#define LED_PIN 2
#define LDR_PIN 36
#define SERVO_PIN 4

#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5
Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

Servo myservo;     // object for servo motor

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, 16, 2);  

const char* ssid = "Bombardier";
const char* password = "moussa94";

const char* weather_url_api = "http://api.weatherstack.com/current?access_key=af882f474169cc3e4859335b9e4b9d4c&query=Paris";

int lastButtonState = 0;

void setup(){
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_mS_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33 ,1);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  myservo.attach(SERVO_PIN, 500, 2400);// attaches the servo on pin 18 to the servo object
                                         // using SG90 servo min/max of 500us and 2400us
                                         // for MG995 large servo, use 1000us and 2000us,
                                         // which are the defaults, so this line could be
                                         // "myservo.attach(servoPin);"

  Serial.begin(115200);
  while (!Serial); //Wait serial init

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  #if IS_DEBUG
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
  #endif

  //LCD initialization
  #if IS_DEBUG
  	Serial.print("LCD Init...");
  #endif
  lcd.init();
  lcd.noBacklight();
  lcd.setCursor(0, 0);
  lcd.print("Working...");
  
  if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    // We start by connecting to a WiFi network
    lcd.backlight();
    Serial.print("Connecting to ");
    lcd.setCursor(0, 0);
    lcd.print("Connecting to ");
    lcd.print(ssid);
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    #if IS_DEBUG
	    Serial.println("");
	    Serial.println("WiFi connected");
	    Serial.println("IP address: ");
	    Serial.println(WiFi.localIP());

	    Serial.println(F("BME680 test"));
    #endif
    if (!bme.begin()) { //Init BME680 sensor for temp
      Serial.println("Could not find a valid BME680 sensor, check wiring!");
      while (1);
    }
    // Set up oversampling and filter initialization for temperature only
    bme.setTemperatureOversampling(BME680_OS_8X);
  }

  lumiosite(); //Check LDR only (amélioration wakeup by adc low power component)

  if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    lcd.backlight();
    getMeteo();
    getTemperature();
    delay(BACK_LIGHT_TIME);
    lcd.noBacklight();
  }

  Serial.println("Going to sleep !");
  esp_deep_sleep_start();
}
void loop(){
}

void lumiosite() {
  int value = analogRead(LDR_PIN);
  #if IS_DEBUG
  	Serial.println(value);
  #endif
  if(value > SEUIL_SUP) {
    myservo.write(180);
    digitalWrite(LED_PIN, HIGH);
  } else if(value < SEUIL_INF) {
    myservo.write(0);
    digitalWrite(LED_PIN,LOW);
  } // else do nothing
  delay(500);
}

void getMeteo() {
  // wait for WiFi connection
  if((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    
    #if IS_DEBUG
    	Serial.print("[HTTP] begin...\n");
    #endif
    http.begin(weather_url_api);
    
    #if IS_DEBUG
    	Serial.print("[HTTP] GET...\n");
    #endif
    // start connection and send HTTP header
    int httpCode = http.GET();
    
    // httpCode will be negative on error
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      #if IS_DEBUG
      	Serial.println(payload);
      #endif

      // Convert to JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      // Read and display values
      
      String desc = doc["current"]["weather_descriptions"][0];
      #if IS_DEBUG
        String temp = doc["current"]["temperature"];
      	Serial.println("Temperature: "+temp+"*C, description: "+desc);
      #endif

      lcd.setCursor(0, 0);
      lcd.print("                ");// erease line
      lcd.setCursor(0, 0);
      lcd.print(desc);
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}
void getTemperature() {
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }

  #if IS_DEBUG
	  Serial.print("Temperature = ");
	  Serial.print(bme.temperature);
	  Serial.println("*C");
  #endif
  lcd.setCursor(0, 1);
  lcd.print(String( (int) round(bme.temperature)) + "*C");
}
