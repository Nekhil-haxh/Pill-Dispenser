/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "d:/Pill_dispenser_file/Code/PD/src/PD.ino"
#include <ArduinoJson.h>
#include <Stepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

time_t parseTime(const String& timeString);
void triggerWebhook(String dataValue, String containerID);
void myHandler(const char *event, const char *data);
void setup();
void loop();
#line 7 "d:/Pill_dispenser_file/Code/PD/src/PD.ino"
#define TFT_CS        10
#define TFT_RST        9 
#define TFT_DC         8

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Define hardware pins
const int pillDispenserButtonPin = D18;
const int stepsPerRevolution = 2048;

// Motor initialization
Stepper motors[] = {
    Stepper(stepsPerRevolution, D0, D2, D1, D3),
    Stepper(stepsPerRevolution, D4, D6, D5, D7),
    Stepper(stepsPerRevolution, A0, A2, A1, A5)
};

const int numMotors = sizeof(motors) / sizeof(motors[0]);

// Constants
const int retryWindow = 3600;  // Retry window in seconds (e.g., 1 hour)
const int hourInSeconds = 3600;
const int dayInSeconds = 86400; // 24 hours in seconds
const int weekInSeconds = 604800; // 7 days in seconds

// Pill dispenser state
struct Container {
    int amount;
    String pillsName;
    String lastDispensed;
    String scheduleType;
    String notifications[5];
    int notificationCount;
    bool pillTaken;
    time_t lastNotificationTime;
    time_t lastDispensedTime;
};

Container containers[3];

String receivedDataBuffer = ""; // Buffer to store complete data

// Function prototypes
void driveMotor(Stepper &motor);
void dispensePills(int containerIndex);
void managePillReminders();
void checkPillSchedule(int containerIndex);
void handleButtonPress();
void parseJson(const String &jsonData);

void driveMotor(Stepper &motor) {
    motor.step(stepsPerRevolution);
    delay(1000);
}

// Function to parse the JSON data and update containers
void parseJson(const String &jsonData) {
    StaticJsonDocument<1024> doc;

    char jsonCharArray[jsonData.length() + 1];
    jsonData.toCharArray(jsonCharArray, sizeof(jsonCharArray));

    DeserializationError error = deserializeJson(doc, jsonCharArray);
    if (error) {
        Serial.print("JSON Parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    for (int i = 0; i < 3; i++) {
        String containerKey = "container-" + String(i + 1);
        JsonObject container = doc[containerKey.c_str()];
        containers[i].amount = container["amount"];
        containers[i].pillsName = container["pillsName"].as<const char*>();
        containers[i].scheduleType = container["scheduleType"].as<const char*>();
        containers[i].notificationCount = container["notifications"].size();
        int j = 0;
        JsonArray notifications = container["notifications"];
        for (JsonVariant notification : notifications) {
            containers[i].notifications[j++] = notification.as<const char*>();
        }
        containers[i].pillTaken = false; // Initially, the pill is not taken
        containers[i].lastNotificationTime = 0; // No notification has been sent yet
        containers[i].lastDispensedTime = container["lastDispensed"]; // No pill has been dispensed yet
    }
}

// Function to control the pill dispenser (motor or servo)
void dispensePills(int containerIndex) {
    if (containers[containerIndex].amount > 0) {
        tft.fillScreen(ST77XX_BLACK); // Clear the screen
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(5);
        tft.setCursor(20, 70);
        tft.print("Dispensing");
        tft.setTextSize(4);
        tft.setCursor(40, 140);
        tft.print(containers[containerIndex].pillsName.c_str());
        Serial.printf("Dispensing Pill: %s From Container %d\n", containers[containerIndex].pillsName.c_str(), containerIndex + 1);
        driveMotor(motors[containerIndex]);
        containers[containerIndex].lastDispensed = Time.format(Time.now(), "%H:%M"); // Update last dispensed time
        containers[containerIndex].lastDispensedTime = Time.now(); // Record timestamp of dispensation
        triggerWebhook(String(Time.now()),String(containerIndex+1));
        //triggerWebhook("1812123123","1");
    }
}

// Function to send notifications and handle pill reminders
void managePillReminders() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < containers[i].notificationCount; j++) {
            String notificationTime = containers[i].notifications[j];
            if (Time.format(Time.now(), "%H:%M") == notificationTime) {
                if (!containers[i].pillTaken) {
                    Serial.printf("Reminder for container %d at %s: %s\n", i + 1, notificationTime.c_str(), containers[i].pillsName.c_str());
                    containers[i].lastNotificationTime = Time.now(); // Record the time when the reminder was sent
                } else {
                    Serial.printf("Pill taken for container %d at %s\n", i + 1, notificationTime.c_str());
                }
            }
        }
    }
}

time_t parseTime(const String& timeString) {
    struct tm tm = {}; // Initialize to zero
    time_t epoch;
    float timezoneOffset = 5.5;

    // Get the current date
    tm.tm_year = Time.year() - 1900; // tm_year is years since 1900
    tm.tm_mon = Time.month() - 1;    // tm_mon is 0-based
    tm.tm_mday = Time.day();

    // Parse the time (HH:MM)
    if (strptime(timeString.c_str(), "%H:%M", &tm) != NULL) {
        // Use mktime to convert the `tm` structure to `time_t`
        epoch = mktime(&tm);

        if (epoch != -1) {
            int offsetInSeconds = timezoneOffset * 3600; // Convert hours to seconds
            return epoch - offsetInSeconds;
        } else {
            Serial.println("Failed to convert tm to time_t.");
            return 0; // Error converting time
        }
    } else {
        Serial.println("Failed to parse time string.");
        return 0; // Error parsing time
    }
}

void triggerWebhook(String dataValue, String containerID) {
    // Trigger Particle event that will send data to webhook
    String jsonData = "{ \"data\": \"" + dataValue + "\", \"coreid\": \"" + containerID + "\" }";
    Particle.publish("firebase-put", jsonData, PRIVATE);
}

// Function to check and dispense pills based on the current time and schedule
void checkPillSchedule(int containerIndex) {
    time_t now = Time.now();
    time_t lastDispensedTime = containers[containerIndex].lastDispensedTime;

    // Check if the pill should be dispensed based on the schedule
    if (containers[containerIndex].scheduleType == "daily") {
        for (int j = 0; j < containers[containerIndex].notificationCount; j++) {
            String notificationTime = containers[containerIndex].notifications[j];
            time_t notificationEpoch = parseTime(notificationTime); // Convert notificationTime to epoch
            time_t graceWindowStart = notificationEpoch - (30 * 60); // 30 minutes before
            time_t graceWindowEnd = notificationEpoch + (30 * 60);   // 30 minutes after
            time_t currentEpoch = Time.now(); // Current time as epoch

            if (currentEpoch >= graceWindowStart && currentEpoch <= graceWindowEnd) {
                    if (containers[j].lastDispensedTime < graceWindowStart || containers[j].lastDispensedTime > graceWindowEnd) {
                        dispensePills(containerIndex);
                        containers[j].lastDispensedTime = currentEpoch; // Record the current time as the last notification time
                        
                    } else {
                        Serial.printf("Pill Already Dispensed For Container %d Within Grace Window.\n", j + 1);
                    }
            }
        }
    }
    else if (containers[containerIndex].scheduleType == "alternate") {
        // Dispense every other day
        if ((now - lastDispensedTime) >= (2 * dayInSeconds)) {
            for (int j = 0; j < containers[containerIndex].notificationCount; j++) {
                String notificationTime = containers[containerIndex].notifications[j];
                time_t notificationEpoch = parseTime(notificationTime); // Convert notificationTime to epoch
                time_t graceWindowStart = notificationEpoch - (30 * 60); // 30 minutes before
                time_t graceWindowEnd = notificationEpoch + (30 * 60);   // 30 minutes after
                time_t currentEpoch = Time.now(); // Current time as epoch

                if (currentEpoch >= graceWindowStart && currentEpoch <= graceWindowEnd) {
                    if (containers[j].lastDispensedTime < graceWindowStart || containers[j].lastDispensedTime > graceWindowEnd) {
                        dispensePills(containerIndex);
                        containers[j].lastDispensedTime = currentEpoch; // Record the current time as the last notification time
                        //triggerWebhook("123", "1");
                    } else {
                        Serial.printf("Pill Already Dispensed For Container %d Within Grace Window.\n", j + 1);
                    }
                }
            }
        }
    } 
    else if (containers[containerIndex].scheduleType == "weekly") {
        // Dispense weekly
        if ((now - lastDispensedTime) >= weekInSeconds) {
            for (int j = 0; j < containers[containerIndex].notificationCount; j++) {
                String notificationTime = containers[containerIndex].notifications[j];
                time_t notificationEpoch = parseTime(notificationTime); // Convert notificationTime to epoch
                time_t graceWindowStart = notificationEpoch - (30 * 60); // 30 minutes before
                time_t graceWindowEnd = notificationEpoch + (30 * 60);   // 30 minutes after
                time_t currentEpoch = Time.now(); // Current time as epoch

                if (currentEpoch >= graceWindowStart && currentEpoch <= graceWindowEnd) {
                        if (containers[j].lastDispensedTime < graceWindowStart || containers[j].lastDispensedTime > graceWindowEnd) {
                            dispensePills(containerIndex);
                            containers[j].lastDispensedTime = currentEpoch; // Record the current time as the last notification time
                        } else {
                            Serial.printf("Pill Already Dispensed For Container %d Within Grace Window.\n", j + 1);
                        }
                }
            }
        }
    }
}

// Function to handle pill dispensing based on schedule when button is pressed
void handleButtonPress() {
    if (digitalRead(pillDispenserButtonPin) == LOW) {
        Serial.println("Dispensing Triggered");

        for (int i = 0; i < 3; i++) {
            checkPillSchedule(i);
        }
        delay(1000); // Debounce delay
    }
}

void myHandler(const char *event, const char *data) {
    if (data != nullptr) {
        receivedDataBuffer += String(data); // Append new chunk of data to the buffer
        if (receivedDataBuffer.endsWith("}}")) {
            //Serial.println("Data received:");
            //Serial.println(receivedDataBuffer);
            parseJson(receivedDataBuffer); // Parse the received data
            receivedDataBuffer = ""; // Clear the buffer for the next message
        }
    }
}

void setup() {
    pinMode(pillDispenserButtonPin, INPUT_PULLUP);
    tft.init(240, 320); // Initialize the display
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK); // Clear the screen
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(6);
    tft.setCursor(20, 100);
    for (Stepper &motor : motors) {
        motor.setSpeed(10);
    }
    Serial.begin(9600);
    Particle.subscribe("firebase-get", myHandler, MY_DEVICES);
    Time.zone(+5.5);
}

void loop() {
    handleButtonPress();
    static unsigned long lastPublish = 0;
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 900) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(6);
        tft.setCursor(20, 100);
        tft.print(Time.format(Time.now(), "%H:%M:%S"));
        lastTime = millis();
    }
    
    if (millis() - lastPublish > 10000) {
        Particle.publish("firebase-get", "", PRIVATE);
        managePillReminders();
        lastPublish = millis();
    }
}