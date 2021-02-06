/**
 * Adapted from: Shawn Hymel (SparkFun Electronics)
 * Date: October 30, 2016
 *
 * Log c02, temperature, and humidity data to a channel on
 * thingspeak.com once every 20 seconds.
 * 
 * Pinout for pimoroni blinkt : https://pinout.xyz/pinout/blinkt#
 */

#include "WiFi.h"
#include "Wire.h"
#include "SPI.h"
#include "SCD30.h"
#include "ThingSpeak.h"
#include "Adafruit_DotStar.h"
#include "keys.h"

// WiFi and Channel parameters
const char WIFI_SSID[] = SECRET_SSID;
const char WIFI_PSK[] = SECRET_PASS;
unsigned long CHANNEL_ID = SECRET_CH_ID;
const char * WRITE_API_KEY = SECRET_WRITE_APIKEY;

// Pin definitions
const int PIN_LED = 13;
const int PIN_LED_CLK = 5;
const int PIN_LED_DATA = 18;
const int PIN_SCD30_SDA = 23;
const int PIN_SCD30_SCL = 22;

// Global variables
int nb_led = 8; // number of LEDs in strip
int val_brightness = 10; // brightness of led strip
int threshold[] = {600, 900, 1500, 1800}; // co2 thresholds that defines color range
int step = (threshold[3] - threshold[0]) / nb_led; // value of ppm per pixel
unsigned long update_time = 0; // time since the last update (ms)
const unsigned long update_interval = 120L * 1000L; // time between cloud updates (ms)

uint32_t green = 0x00FF00;
uint32_t orange = 0xFF9900;
uint32_t red = 0xFF0000;
uint32_t purple = 0x800080;

// objects
WiFiClient client;

Adafruit_DotStar strip(nb_led, PIN_LED_DATA, PIN_LED_CLK, DOTSTAR_BGR);

void setup() {

  // Set up LED for debugging
  pinMode(PIN_LED, OUTPUT);

  // Connect to WiFi
  connectWiFi();

  // Initialize connection to ThingSpeak
  ThingSpeak.begin(client);

  // Initialize the SCD30 driver
  Wire.begin(PIN_SCD30_SDA, PIN_SCD30_SCL);
  scd30.initialize();
  scd30.setAutoSelfCalibration(1);
  scd30.setTemperatureOffset(3); // set temperature offset (2°C when sensor is exposed, 7°C else)
  
  // Initialize the LED indicator 
  strip.begin(); 
  strip.setBrightness(val_brightness);
  strip.show();
}

void loop() {
  
  float result[3] = {-99};
  
  if (scd30.isAvailable()) {
    
    // get measured [co2] from scd30 sensor
    scd30.getCarbonDioxideConcentration(result);
    
    float co2 = result[0];
    float temp_c = result[1];
    float humidity = result[2];
    
    // update led strip as a function of [co2] 
    updateLED(co2);
    
    // write the values to the ThingSpeak channel
    if (millis() - update_time >=  update_interval) {
      updateCloud(co2, temp_c, humidity);
      update_time = millis();
    }
  }

  delay(1000);
}


// update LED strip as a function of [co2]
void updateLED(float co2) {

  int c_co2 = (int)co2;

  int index_co2 = map(c_co2, threshold[0], threshold[3] - step, 1, nb_led); // led number = f([co2])

  if (c_co2 < threshold[0]){
    strip.clear();
    strip.setBrightness(2);
    strip.setPixelColor(0, green);
    strip.show();
    delay(1000);
    strip.setBrightness(val_brightness);
    strip.setPixelColor(0, green);
    strip.show();
  }
  else if (c_co2 >= threshold[0] && c_co2 < threshold[1]){
    strip.clear();
    strip.fill(green, 0, index_co2);
    strip.show();
  }
  else if (c_co2 >= threshold[1] && c_co2 < threshold[2]){
    strip.clear();
    strip.fill(green, 0, 2);
    strip.fill(orange, 2, index_co2 - 2);
    strip.show();
  }
  else if (c_co2 >= threshold[2] && c_co2 < threshold[3]){
    strip.clear();
    strip.fill(green, 0, 2);
    strip.fill(orange, 2, 4);
    strip.fill(red, 6, index_co2 - 6);
    strip.show();
  }
  else {
    strip.clear();
    strip.fill(green, 0, 2);
    strip.fill(orange, 2, 4);
    strip.fill(red, 6, 1);
    strip.setPixelColor(7, purple);
    strip.show();
  }

} 

// update chosen cloud service with collected data
void updateCloud(float co2, float temp_c, float humidity) {
  ThingSpeak.setField(1, co2);
  ThingSpeak.setField(2, temp_c);
  ThingSpeak.setField(3, humidity);
  ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
}


// Attempt to connect to WiFi
void connectWiFi() {

  byte led_status = 0;

  // Set WiFi mode to station (client)
  WiFi.mode(WIFI_STA);

  // Initiate connection with SSID and PSK
  WiFi.begin(WIFI_SSID, WIFI_PSK);

  // Blink LED while we wait for WiFi connection
  while ( WiFi.status() != WL_CONNECTED ) {
    digitalWrite(PIN_LED, led_status);
    led_status ^= 0x01;
    delay(100);
  }

  // Turn LED off when we are connected
  digitalWrite(PIN_LED, HIGH);
}
