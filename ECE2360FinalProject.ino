#include <BleKeyboard.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "esp_log.h"

const int pin26 = 26;
const int pin27 = 27;
const int pin25 = 25;
const int pin32 = 32;
const int pin13 = 13;
const int pin12 = 12;

const int potPin = 35; // analog pin for potentiometer
const int pirPin = 33; // pin for motion sensor

const int dataPin = 2;   // SER
const int latchPin = 5;  // RCLK
const int clockPin = 4;  // SRCLK

// Create LCD object: address 0x27, 16 columns, 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wi-Fi credentials for the phone hotspot
const char* ssid = "Samsung Galaxy S8 8534";  // replace with your hotspot's SSID
const char* password = "roydinh1234";  // replace with your hotspot's password

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -4 * 3600, 60000);  // Update every 60 seconds

// potMode = true to change volume
// potMode = false to change brightness
bool potMode = true;

unsigned long lastMotionTime = 0;
const unsigned long displayTimeout = 60000; // 5 seconds in milliseconds
int state = 0;

BleKeyboard bleKeyboard("SpotifyRemote", "Roy Inc.", 100);
bool wasPressed = false;


int lastPotPercent = -1;

bool inSpotify = false;
bool isPlaying = false;


const int ledSpotifyOpen = 0; // Q0
const int ledSongPlaying = 1; // Q1
const int ledSongPaused = 2;  // Q2

void setup() {
  pinMode(pin26, INPUT_PULLUP);
  pinMode(pin27, INPUT_PULLUP);
  pinMode(pin25, INPUT_PULLUP);
  pinMode(pin13, INPUT_PULLUP);
  pinMode(pin12, INPUT_PULLUP);
 
  pinMode(pirPin, INPUT);
  pinMode(pin32, OUTPUT);

  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  digitalWrite(pin32, LOW);
  Serial.begin(9600);

  Wire.begin(21, 22); // Set I2C pins for ESP32
  lcd.begin(16, 2);
  lcd.backlight();

  // Connect to Wi-Fi
  Serial.println("Connecting to WiFi...");
  esp_log_level_set("wifi", ESP_LOG_WARN);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Start the NTP client
  timeClient.begin();
  bleKeyboard.begin();

  updateShiftRegister(0, 0, 0);
  inSpotify = false;
  isPlaying = false;
}

void loop() {


  state = digitalRead(pirPin);         // Read the state of the PIR sensor
  if (state == HIGH) {
    //Serial.println("Somebody here!");
    lastMotionTime = millis(); // reset timer
    lcd.backlight();
    lcd.display();
  } else {
    //Serial.println("Monitoring...");
    
    // check if 5 seconds have passed since last motion
    if (millis() - lastMotionTime >= displayTimeout) {
      lcd.noDisplay();
      lcd.noBacklight();
    }
  }

  if (bleKeyboard.isConnected()) {
    
    if (digitalRead(pin27) == LOW && !wasPressed) {
      Serial.println("pin27 pressed");
      wasPressed = true;
      if (!inSpotify) {
        inSpotify = true;
        digitalWrite(pin32, HIGH);
        Serial.println("Opening Spotify...");

        // Step 1: Press Windows + R
        bleKeyboard.press(KEY_LEFT_GUI); // Windows key
        bleKeyboard.press('r');
        delay(800);
        bleKeyboard.releaseAll();

        // Step 2: Wait for Run dialog
        delay(800);

        // Step 3: Type "spotify" and press Enter
        const char* command = "C:\\Scripts\\ToggleSpotify.bat";
        for (int i = 0; i < strlen(command); i++) {
          bleKeyboard.write(command[i]);
          delay(50);  // Delay between characters
        }

        bleKeyboard.write(KEY_RETURN);

        // Update the shift register to show that Spotify is open
        updateShiftRegister(1, 0, 0); // Set Q0 to HIGH (Spotify open)

      } else {
        inSpotify = false;
         // Step 1: Press Windows + R
        bleKeyboard.press(KEY_LEFT_GUI); // Windows key
        bleKeyboard.press('r');
        delay(400);
        bleKeyboard.releaseAll();

        // Step 2: Wait for Run dialog
        delay(400);

        bleKeyboard.print("C:\\Scripts\\ToggleSpotify.bat");

        bleKeyboard.write(KEY_RETURN);
        delay(100);
        bleKeyboard.releaseAll();
        

        // Update the shift register to show that Spotify is open
        updateShiftRegister(0, 0, 0); // Set Q0 to LOW (Spotify closed)
        isPlaying = false;
      }
      
    } else if (digitalRead(pin27) == HIGH) {
      wasPressed = false;
      digitalWrite(pin32, LOW);
    }
  }

  delay(50);





  

  // Handle button on pin 26
  if (digitalRead(pin26) == LOW) {
    Serial.println("Button on pin 26 is pressed");
    if (!isPlaying) {
      isPlaying = true;
      updateShiftRegister(1, 1, 0);
    } else {
      isPlaying = false;
      updateShiftRegister(1, 0, 1);
    }
    bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
    delay(200); // debounce
  }

  
  if (digitalRead(pin13) == LOW) {
    Serial.println("Button on pin 13 is pressed");
    bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
  }

  if (digitalRead(pin12) == LOW) {
    Serial.println("Button on pin 12 is pressed");
    bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
  }

  // Read potentiometer value
  int potValue = analogRead(potPin);
  int potPercent = map(potValue, 0, 4095, 0, 100);

  // Only act if the value changed significantly (say 2% or more)
  if (abs(potPercent - lastPotPercent) >= 0) {
    int difference = potPercent - lastPotPercent;
    
    int steps = abs(difference) / 2;

    if (difference > 0) {
      // Pot value increased — raise volume
      for (int i = 0; i < steps; i++) {
        bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
        delay(30);  // prevent overloading the system
      }
    } else {
      // Pot value decreased — lower volume
      for (int i = 0; i < steps; i++) {
        bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
        delay(30);
      }
    }

    lastPotPercent = potPercent; // Update stored value
  }

  
  // Get the current time from NTP client
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();

  // Show time on the first line of the LCD
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(formattedTime);

  // Show potentiometer value on the second line
  lcd.setCursor(0, 1);
  
  lcd.print("Volume: ");
  delay(500); // adjust refresh rate
  
  lcd.print(potValue / 4095.00 * 100.0);
  lcd.print("%");
  lcd.print("     "); // clear leftover chars
  delay(100); // adjust refresh rate
  
}

// Function to update the shift register
void updateShiftRegister(int spotifyOpen, int songPlaying, int songPaused) {
  int shiftData = 0;
  
  // Set bits for each LED based on the states
  shiftData = (spotifyOpen << ledSpotifyOpen) | (songPlaying << ledSongPlaying) | (songPaused << ledSongPaused);
  
  // Send the data to the shift register
  digitalWrite(latchPin, LOW);  // Prepare to send data
  shiftOut(dataPin, clockPin, MSBFIRST, shiftData);  // Send data bit by bit
  digitalWrite(latchPin, HIGH);  // Latch the data into the outputs
}
