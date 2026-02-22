#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>

// === User Names ===
String userNames[] = {
  "", "Jeffrin", "Durai", "Bala", "Jessy", "Jaanu"
};

// === Attendance Status ===
bool userPresent[6] = {false, false, false, false, false, false}; // Track check-in status

// === OLED Setup ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// === Fingerprint Setup ===
HardwareSerial mySerial(1); // UART1
Adafruit_Fingerprint finger(&mySerial);

// === Pin Setup ===
#define BUTTON_PIN 14
#define BUZZER_PIN 26

// === States ===
int count = 0;
bool mode = true; // true = Check In, false = Check Out
bool buttonPressed = false;
unsigned long buttonPressStart = 0;
bool enrollMode = false;
uint8_t nextEnrollID = 1;

// Track rapid button presses for delete mode
int buttonPressCount = 0;
unsigned long lastButtonPressTime = 0;
#define MULTI_PRESS_TIMEOUT 1000 // 1 second timeout between presses

// === Functions ===
void beep(int duration = 100) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void showStatus() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 15, "Select Mode:");
  u8g2.drawStr(0, 35, mode ? "> Check In" : "  Check In");
  u8g2.drawStr(0, 50, !mode ? "> Check Out" : "  Check Out");

  char buf[20];
  sprintf(buf, "Count: %d", count);
  u8g2.drawStr(0, 64, buf);

  u8g2.sendBuffer();
}

// Check if a fingerprint ID is already enrolled
bool isFingerEnrolled(uint8_t id) {
  uint8_t p = finger.loadModel(id);
  return (p == FINGERPRINT_OK);
}

// Find next available ID for enrollment
uint8_t findNextAvailableID() {
  uint8_t id = 1;
  while (id < sizeof(userNames)/sizeof(userNames[0])) {
    if (!isFingerEnrolled(id)) {
      return id;
    }
    id++;
  }
  return 0; // No available IDs
}

// Check if fingerprint already exists in database
int checkFingerExists() {
  uint8_t p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID; // Return the ID if fingerprint was found
  }
  return -1; // Fingerprint not found
}

// Delete all fingerprints from the sensor
void deleteAllFingerprints() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 20, "Deleting all");
  u8g2.drawStr(0, 40, "fingerprints...");
  u8g2.sendBuffer();
  
  Serial.println("⚠️ Deleting all fingerprints!");
  
  uint8_t p = finger.emptyDatabase();
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ Database emptied successfully");
    
    // Reset all attendance data
    for (int i = 0; i < sizeof(userPresent)/sizeof(userPresent[0]); i++) {
      userPresent[i] = false;
    }
    count = 0;
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "All fingerprints");
    u8g2.drawStr(0, 50, "deleted!");
    u8g2.sendBuffer();
    
    // Triple beep to confirm deletion
    beep(100);
    delay(100);
    beep(100);
    delay(100);
    beep(300);
    
    delay(2000);
  } else {
    Serial.print("❌ Error emptying database: ");
    Serial.println(p);
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "Delete failed!");
    u8g2.sendBuffer();
    delay(2000);
  }
}

// Display enrolled users in Serial Monitor
void printEnrolledUsers() {
  Serial.println("\n=== Enrolled Users ===");
  bool anyEnrolled = false;
  for (int i = 1; i < sizeof(userNames)/sizeof(userNames[0]); i++) {
    if (isFingerEnrolled(i)) {
      anyEnrolled = true;
      Serial.print("ID #");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(userNames[i]);
      Serial.println(userPresent[i] ? " (Present)" : " (Absent)");
    }
  }
  if (!anyEnrolled) {
    Serial.println("No users enrolled yet");
  }
  Serial.println("====================\n");
}

void enrollFingerprint(uint8_t id) {
  // Check if ID is already enrolled
  if (isFingerEnrolled(id)) {
    Serial.print("ID #");
    Serial.print(id);
    Serial.print(" (");
    Serial.print(userNames[id]);
    Serial.println(") is already enrolled!");
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 20, "Already Enrolled!");
    u8g2.sendBuffer();
    beep(100);
    delay(2000);
    return;
  }

  int p = -1;
  Serial.print("Place finger to enroll ID #");
  Serial.print(id);
  Serial.print(" (");
  Serial.print(userNames[id]);
  Serial.println(")");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 20, "Place Finger");
  u8g2.sendBuffer();

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) delay(100);
  }

  // First check if this fingerprint already exists
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Error converting image");
    return;
  }
  
  // Search for this fingerprint in the database
  int existingID = checkFingerExists();
  if (existingID >= 0) {
    Serial.print("❌ This fingerprint is already enrolled as ID #");
    Serial.print(existingID);
    Serial.print(" (");
    Serial.print(userNames[existingID]);
    Serial.println(")");
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 20, "Already Enrolled!");
    char idBuf[30];
    sprintf(idBuf, "as ID #%d: %s", existingID, userNames[existingID].c_str());
    u8g2.drawStr(0, 40, idBuf);
    u8g2.sendBuffer();
    beep(100);
    delay(2500);
    return;
  }

  Serial.println("Remove finger");
  u8g2.clearBuffer();
  u8g2.drawStr(0, 20, "Remove Finger");
  u8g2.sendBuffer();
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  Serial.println("Place same finger again");
  u8g2.clearBuffer();
  u8g2.drawStr(0, 20, "Place Again");
  u8g2.sendBuffer();

  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) delay(100);
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Error converting second image");
    return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Error creating model");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.print("✅ Fingerprint enrolled successfully for ");
    Serial.println(userNames[id]);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 20, "Enrolled OK!");
    u8g2.sendBuffer();
    beep(300);
    delay(2000);
    
    // Print updated list of enrolled users
    printEnrolledUsers();
  } else {
    Serial.println("❌ Error saving fingerprint");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 20, "Enroll Failed");
    u8g2.sendBuffer();
    delay(2000);
  }
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  u8g2.begin();
  showStatus();

  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX, TX
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor found");
    beep();
  } else {
    Serial.println("❌ Fingerprint sensor NOT found");
    while (1);
  }
  
  // Find next available ID for enrollment
  nextEnrollID = findNextAvailableID();
  if (nextEnrollID == 0) {
    Serial.println("⚠️ All IDs are enrolled!");
    nextEnrollID = 1; // Reset to 1 as fallback
  }
  
  // Print currently enrolled users at startup
  printEnrolledUsers();
}

// === Main Loop ===
void loop() {
  // === BUTTON ===
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStart = millis();
    } else {
      if (millis() - buttonPressStart > 3000 && !enrollMode) { // Long Press
        enrollMode = true;
        Serial.println("🆕 Enroll Mode Activated");
        beep(500);
        
        // Find the next available ID
        nextEnrollID = findNextAvailableID();
        if (nextEnrollID == 0) {
          Serial.println("⚠️ All IDs are enrolled!");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "All IDs enrolled!");
          u8g2.sendBuffer();
          beep(100);
          delay(2000);
        } else {
          enrollFingerprint(nextEnrollID);
        }
        
        enrollMode = false;
        showStatus();
      }
    }
  } else {
    if (buttonPressed) {
      // Regular short press
      if (!enrollMode && (millis() - buttonPressStart < 1000)) {
        unsigned long currentTime = millis();
        
        // Check for rapid button presses
        if (currentTime - lastButtonPressTime < MULTI_PRESS_TIMEOUT) {
          buttonPressCount++;
          
          // If 5 rapid presses detected, enter delete mode
          if (buttonPressCount >= 5) {
            // Show delete confirmation screen
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(0, 20, "DELETE ALL DATA?");
            u8g2.drawStr(0, 40, "Press & hold button");
            u8g2.drawStr(0, 60, "to confirm");
            u8g2.sendBuffer();
            
            // Warning beep pattern
            beep(100);
            delay(100);
            beep(100);
            delay(100);
            beep(100);
            
            // Wait for confirmation (button press and hold)
            unsigned long startWait = millis();
            bool deleteConfirmed = false;
            
            while (millis() - startWait < 5000) { // 5 second window to confirm
              if (digitalRead(BUTTON_PIN) == LOW) {
                // Button pressed, wait for hold
                unsigned long holdStart = millis();
                while (digitalRead(BUTTON_PIN) == LOW) {
                  // If held for 2 seconds, confirm delete
                  if (millis() - holdStart > 2000) {
                    deleteConfirmed = true;
                    break;
                  }
                  delay(10);
                }
                
                if (deleteConfirmed) {
                  break;
                }
              }
              delay(10);
            }
            
            if (deleteConfirmed) {
              deleteAllFingerprints();
              nextEnrollID = 1;
            } else {
              u8g2.clearBuffer();
              u8g2.drawStr(0, 30, "Delete cancelled");
              u8g2.sendBuffer();
              delay(1000);
            }
            
            // Reset counter
            buttonPressCount = 0;
            showStatus();
          }
          else if (buttonPressCount >= 2) {
            // Signal that we're counting presses
            beep(50);
          }
        } else {
          // First press or time expired, toggle mode
          mode = !mode;
          Serial.println(mode ? "🔁 Mode: Check In" : "🔁 Mode: Check Out");
          beep();
          showStatus();
          buttonPressCount = 1;
        }
        
        lastButtonPressTime = currentTime;
      }
      
      buttonPressed = false;
    }
  }

  // === FINGERPRINT ===
  if (finger.getImage() == FINGERPRINT_OK) {
    Serial.println("🖐 Finger detected");
    beep(200);

    if (finger.image2Tz() != FINGERPRINT_OK) {
      Serial.println("❌ Error converting image");
      return;
    }

    if (finger.fingerSearch() != FINGERPRINT_OK) {
      Serial.println("❌ No match found");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 20, "Unknown User");
      u8g2.sendBuffer();
      delay(2000);
      showStatus();
      return;
    }

    int id = finger.fingerID;
    Serial.print("🎯 Found ID: ");
    Serial.print(id);
    Serial.print(" (");
    Serial.print(userNames[id]);
    Serial.println(")");

    String name = (id >= 0 && id < sizeof(userNames)/sizeof(userNames[0])) ? userNames[id] : "Unknown";
    String message;

    if (mode) { // Check In
      if (userPresent[id]) {
        message = name + " already in";
        Serial.println("⚠️ Already checked in!");
      } else {
        userPresent[id] = true;
        count++;
        message = name + " In";
        Serial.println("✅ Checked In");
      }
    } else { // Check Out
      if (!userPresent[id]) {
        message = name + " already out";
        Serial.println("⚠️ Already checked out!");
      } else {
        userPresent[id] = false;
        if (count > 0) count--;
        message = name + " Out";
        Serial.println("🔄 Checked Out");
      }
    }

    // Show name and action on OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 30, message.c_str());

    char buf[20];
    sprintf(buf, "Count: %d", count);
    u8g2.drawStr(0, 60, buf);

    u8g2.sendBuffer();
    delay(2000);
    showStatus();
    
    // Print current attendance after each check-in/out
    printEnrolledUsers();
  }
}