#define BLYNK_TEMPLATE_ID "TMPL3qzf9aJ8f"
#define BLYNK_TEMPLATE_NAME "Major Projectt"
#define BLYNK_AUTH_TOKEN ""

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "YOUR_WIFI_SSID"; 
char pass[] = "YOUR_WIFI_PASSWORD"; 

const int I2C_SDA = D2;
const int I2C_SCL = D1;
const int ONE_WIRE_BUS = D4;
const int PH_CHAN = 0;
const int TDS_CHAN = 1;

// ML & Calibration Constants
float V_PH7 = 2500.0; 
float V_PH4 = 3000.0; 
float PH_SLOPE = (7.0 - 4.0) / (V_PH7 - V_PH4);
const float DO_INTERCEPT = 14.65; 
const float C_TEMP = -0.315;   
const float C_PH = -0.12;      
const float C_TDS = -0.0004;

Adafruit_ADS1115 ads; 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
BlynkTimer timer;

void monitorSystem() {
  // 1. Temperature Read
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // 2. pH Read
  int16_t adc0 = ads.readADC_SingleEnded(PH_CHAN);
  float mvPh = ads.computeVolts(adc0) * 1000.0;
  float phVal = 7.0 + ((V_PH7 - mvPh) * PH_SLOPE);
  if (phVal < 0) phVal = 0; if (phVal > 14) phVal = 14;

  // 3. TDS Read
  int16_t adc1 = ads.readADC_SingleEnded(TDS_CHAN);
  float vTds = ads.computeVolts(adc1);
  float compCoeff = 1.0 + 0.02 * (tempC - 25.0);
  float compV = vTds / compCoeff;
  float tdsVal = (133.42 * pow(compV, 3) - 255.86 * pow(compV, 2) + 857.39 * compV) * 0.5;

  // 4. TinyML Prediction
  float predDO = DO_INTERCEPT + (C_TEMP * tempC) + (C_PH * (phVal - 7.0)) + (C_TDS * tdsVal);
  if (predDO < 0) predDO = 0;

  String viability = (predDO >= 5.0) ? "HEALTHY" : (predDO >= 3.0 ? "MODERATE" : "CRITICAL");

  // --- NEW: DETAILED SERIAL MONITOR PRINTING ---
  Serial.println("--- NEW DATA PACKET ---");
  Serial.print("RAW DATA -> Temp: "); Serial.print(tempC); Serial.println(" C");
  Serial.print("RAW DATA -> pH mV: "); Serial.print(mvPh); Serial.print(" | pH Val: "); Serial.println(phVal);
  Serial.print("RAW DATA -> TDS Volts: "); Serial.print(vTds); Serial.print(" | TDS ppm: "); Serial.println(tdsVal);
  Serial.print("INFERENCE -> Predicted DO: "); Serial.print(predDO); Serial.println(" mg/L");
  Serial.print("DECISION  -> Status: "); Serial.println(viability);
  Serial.println("-----------------------");

  // Update Blynk
  Blynk.virtualWrite(V1, tempC);
  Blynk.virtualWrite(V2, phVal);
  Blynk.virtualWrite(V3, tdsVal);
  Blynk.virtualWrite(V4, predDO);
  Blynk.virtualWrite(V5, viability);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) {
    Serial.println("ADS1115 not found!");
    while (1); 
  }
  ads.setGain(GAIN_TWOTHIRDS);
  sensors.begin();
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(5000L, monitorSystem);
}

void loop() {
  Blynk.run();
  timer.run();
}
