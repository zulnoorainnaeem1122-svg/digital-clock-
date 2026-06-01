#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// ================= LCD =================
// Will be set to correct address in setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

RTC_DS1307 rtc;

const int btnNext  = 9;
const int btnInc   = 10;
const int btnDec   = 11;
const int btnAlarm = 12;
const int buzzer   = 8;

DateTime now;

int  alarmHour    = 0;
int  alarmMinute  = 0;
int  alarmSecond  = 0;
bool alarmSet     = false;
bool alarmRinging = false;
unsigned long alarmRingStart = 0;
const unsigned long ALARM_RING_MS = 10000UL;

bool buzzerState = false;
unsigned long lastBeep = 0;
const unsigned long BEEP_INTERVAL = 300UL;

int menuState = 0;
int setHour, setMinute, setSecond;
int setDay, setDate, setMonth, setYear;
int alSetHour = 0, alSetMinute = 0, alSetSecond = 0;

unsigned long saveScreenEnteredAt = 0;
const unsigned long SAVE_TIMEOUT_MS = 10000UL;
unsigned long alarmSecondEnteredAt = 0;
const unsigned long ALARM_EDIT_TIMEOUT_MS = 10000UL;

bool showConfirm = false;
unsigned long confirmStart = 0;
const unsigned long CONFIRM_MS = 1500UL;
char confirmLine1[17] = "";
char confirmLine2[17] = "";

unsigned long lastNextDebounce  = 0;
unsigned long lastIncDebounce   = 0;
unsigned long lastDecDebounce   = 0;
unsigned long lastAlarmDebounce = 0;
const unsigned long DEBOUNCE_MS = 30UL;

bool lastNextRaw  = HIGH, lastIncRaw   = HIGH;
bool lastDecRaw   = HIGH, lastAlarmRaw = HIGH;
bool nextPressed  = false, incPressed  = false;
bool decPressed   = false, alarmPressed = false;

bool blinkState = true;
unsigned long lastBlink = 0;

char oldLine1[17] = "";
char oldLine2[17] = "";

bool secondUserEdited = false;

// ======================================================
// SCAN I2C — returns found address or 0x27 as default
// ======================================================
byte scanI2C() {
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      // Return first device found (LCD will be 0x27 or 0x3F)
      if (addr == 0x27 || addr == 0x3F) return addr;
    }
  }
  return 0x27; // default fallback
}

// ======================================================
void lcdUpdateLine(int row, char* newText, char* oldText) {
  if (strcmp(newText, oldText) != 0) {
    lcd.setCursor(0, row);
    lcd.print(newText);
    int len = strlen(newText);
    for (int i = len; i < 16; i++) lcd.print(' ');
    strcpy(oldText, newText);
  }
}

bool debounce(int pin, bool &lastRaw, unsigned long &lastTime, bool &pressedFlag) {
  bool reading = digitalRead(pin);
  if (reading != lastRaw) {
    lastRaw  = reading;
    lastTime = millis();
  }
  if ((millis() - lastTime) > DEBOUNCE_MS) {
    if (reading == LOW && !pressedFlag) { pressedFlag = true;  return true; }
    if (reading == HIGH)                 { pressedFlag = false; }
  }
  return false;
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Wire.begin();
  delay(100); // let I2C bus settle

  // --- Find LCD address ---
  byte addr = scanI2C();

  // --- Point lcd object at correct address ---
  lcd = LiquidCrystal_I2C(addr, 16, 2);

  // --- Init LCD (only init(), never begin() for I2C LCD) ---
  lcd.init();
  delay(50);
  lcd.backlight();
  delay(50);
  lcd.clear();

  // --- Boot message ---
  lcd.setCursor(0, 0); lcd.print("System Booting");
  lcd.setCursor(0, 1); lcd.print("Please Wait...");
  delay(2000);
  lcd.clear();

  // --- Pins ---
  pinMode(btnNext,  INPUT_PULLUP);
  pinMode(btnInc,   INPUT_PULLUP);
  pinMode(btnDec,   INPUT_PULLUP);
  pinMode(btnAlarm, INPUT_PULLUP);
  pinMode(buzzer,   OUTPUT);
  digitalWrite(buzzer, LOW);

  // --- RTC ---
  byte rtcTry = 0;
  while (!rtc.begin()) {
    rtcTry++;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("RTC ERROR!");
    lcd.setCursor(0, 1); lcd.print("Retry " + String(rtcTry));
    delay(1000);
    if (rtcTry >= 5) break; // give up waiting, continue anyway
  }

  if (!rtc.isrunning()) {
    // Set to compile time if RTC never ran
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.clear();
  memset(oldLine1, 0, sizeof(oldLine1));
  memset(oldLine2, 0, sizeof(oldLine2));

  // Uncomment FIRST UPLOAD ONLY, then comment out again:
  // rtc.adjust(DateTime(2026, 5, 21, 12, 0, 0));
}

// ======================================================
// LOOP
// ======================================================
void loop() {

  now = rtc.now();

  if (menuState >= 1 && menuState <= 7 && !secondUserEdited)
    setSecond = now.second();

  if (millis() - lastBlink >= 500) {
    lastBlink  = millis();
    blinkState = !blinkState;
  }

  // --- Alarm ring handler ---
  if (alarmRinging) {
    if (millis() - alarmRingStart >= ALARM_RING_MS) {
      alarmRinging = false;
      alarmSet     = false;
      alarmHour    = 0;
      alarmMinute  = 0;
      alarmSecond  = 0;
      digitalWrite(buzzer, LOW);
      buzzerState  = false;
    } else {
      if (millis() - lastBeep >= BEEP_INTERVAL) {
        lastBeep    = millis();
        buzzerState = !buzzerState;
        digitalWrite(buzzer, buzzerState ? HIGH : LOW);
      }
    }
  }

  // --- Alarm trigger ---
  if (alarmSet && !alarmRinging) {
    if (now.hour()   == alarmHour   &&
        now.minute() == alarmMinute &&
        now.second() >= alarmSecond &&
        now.second() <= alarmSecond + 2) {
      alarmRinging   = true;
      alarmRingStart = millis();
      lastBeep       = millis();
    }
  }

  // --- Confirmation timeout ---
  if (showConfirm && millis() - confirmStart >= CONFIRM_MS) {
    showConfirm = false;
    lcd.clear();
    memset(oldLine1, 0, sizeof(oldLine1));
    memset(oldLine2, 0, sizeof(oldLine2));
  }

  // --- Save screen timeout ---
  if (menuState == 8 && millis() - saveScreenEnteredAt >= SAVE_TIMEOUT_MS) {
    lcd.clear();
    memset(oldLine1, 0, sizeof(oldLine1));
    memset(oldLine2, 0, sizeof(oldLine2));
    menuState = 0;
  }

  // --- Alarm second screen timeout ---
  if (menuState == 13 && millis() - alarmSecondEnteredAt >= ALARM_EDIT_TIMEOUT_MS) {
    lcd.clear();
    memset(oldLine1, 0, sizeof(oldLine1));
    memset(oldLine2, 0, sizeof(oldLine2));
    menuState = 0;
  }

  // --- Buttons ---
  bool gotNext  = debounce(btnNext,  lastNextRaw,  lastNextDebounce,  nextPressed);
  bool gotInc   = debounce(btnInc,   lastIncRaw,   lastIncDebounce,   incPressed);
  bool gotDec   = debounce(btnDec,   lastDecRaw,   lastDecDebounce,   decPressed);
  bool gotAlarm = debounce(btnAlarm, lastAlarmRaw, lastAlarmDebounce, alarmPressed);

  // --- Alarm button ---
  if (gotAlarm) {
    if (alarmRinging) {
      alarmRinging = false;
      alarmSet     = false;
      alarmHour    = 0;
      alarmMinute  = 0;
      alarmSecond  = 0;
      digitalWrite(buzzer, LOW);
      buzzerState  = false;
      lcd.clear();
      memset(oldLine1, 0, sizeof(oldLine1));
      memset(oldLine2, 0, sizeof(oldLine2));
      menuState = 0;
    }
    else if (menuState == 0) {
      alSetHour   = alarmHour;
      alSetMinute = alarmMinute;
      alSetSecond = alarmSecond;
      menuState   = 10;
    }
    else if (menuState >= 10 && menuState <= 13) {
      alarmHour   = alSetHour;
      alarmMinute = alSetMinute;
      alarmSecond = alSetSecond;
      alarmSet    = true;
      lcd.clear();
      memset(oldLine1, 0, sizeof(oldLine1));
      memset(oldLine2, 0, sizeof(oldLine2));
      sprintf(confirmLine1, "ALARM SET!");
      sprintf(confirmLine2, "%02d:%02d:%02d", alSetHour, alSetMinute, alSetSecond);
      showConfirm  = true;
      confirmStart = millis();
      menuState    = 0;
    }
  }

  // --- Next button ---
  if (gotNext) {
    if (menuState == 0) {
      now = rtc.now();
      setHour   = now.hour();
      setMinute = now.minute();
      setSecond = now.second();
      setDay    = now.dayOfTheWeek();
      setDate   = now.day();
      setMonth  = now.month();
      setYear   = now.year();
      secondUserEdited = false;
      menuState = 1;
    }
    else if (menuState == 8) {
      rtc.adjust(DateTime(setYear, setMonth, setDate, setHour, setMinute, setSecond));
      lcd.clear();
      memset(oldLine1, 0, sizeof(oldLine1));
      memset(oldLine2, 0, sizeof(oldLine2));
      const char* daysN[] = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
      sprintf(confirmLine1, "SAVED %02d:%02d:%02d", setHour, setMinute, setSecond);
      sprintf(confirmLine2, "%s %02d/%02d/%04d", daysN[setDay], setDate, setMonth, setYear);
      showConfirm  = true;
      confirmStart = millis();
      menuState    = 0;
    }
    else if (menuState >= 1 && menuState <= 7) {
      menuState++;
      if (menuState > 7) { menuState = 8; saveScreenEnteredAt = millis(); }
    }
    else if (menuState == 10) { menuState = 11; }
    else if (menuState == 11) { menuState = 12; }
    else if (menuState == 12) { menuState = 13; alarmSecondEnteredAt = millis(); }
  }

  // --- Inc button ---
  if (gotInc) {
    switch (menuState) {
      case 1:  setHour   = (setHour   + 1) % 24; break;
      case 2:  setMinute = (setMinute + 1) % 60; break;
      case 3:  secondUserEdited = true; setSecond = (setSecond + 1) % 60; break;
      case 4:  setDay    = (setDay    + 1) % 7;  break;
      case 5:  setDate++;  if (setDate  > 31) setDate  = 1;    break;
      case 6:  setMonth++; if (setMonth > 12) setMonth = 1;    break;
      case 7:  setYear++;  if (setYear > 2099)  setYear  = 2024; break;
      case 11: alSetHour++;   if (alSetHour   > 23) alSetHour   = 0; break;
      case 12: alSetMinute++; if (alSetMinute > 59) alSetMinute = 0; break;
      case 13: alSetSecond++; if (alSetSecond > 59) alSetSecond = 0;
               alarmSecondEnteredAt = millis(); break;
    }
  }

  // --- Dec button ---
  if (gotDec) {
    switch (menuState) {
      case 1:  setHour   = (setHour   - 1 + 24) % 24; break;
      case 2:  setMinute = (setMinute - 1 + 60) % 60; break;
      case 3:  secondUserEdited = true; setSecond = (setSecond - 1 + 60) % 60; break;
      case 4:  setDay    = (setDay    - 1 +  7) %  7; break;
      case 5:  setDate--;  if (setDate  < 1) setDate  = 31;   break;
      case 6:  setMonth--; if (setMonth < 1) setMonth = 12;   break;
      case 7:  setYear--;  if (setYear < 2024) setYear = 2099; break;
      case 11: alSetHour--;   if (alSetHour   < 0) alSetHour   = 23; break;
      case 12: alSetMinute--; if (alSetMinute < 0) alSetMinute = 59; break;
      case 13: alSetSecond--; if (alSetSecond < 0) alSetSecond = 59;
               alarmSecondEnteredAt = millis(); break;
    }
  }

  // --- Build display ---
  const char* days[] = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
  char line1[17], line2[17];

  if (showConfirm) {
    snprintf(line1, 17, "%-16s", confirmLine1);
    snprintf(line2, 17, "%-16s", confirmLine2);
  }
  else if (menuState == 8) {
    unsigned long el = millis() - saveScreenEnteredAt;
    int sl = (int)((SAVE_TIMEOUT_MS - el) / 1000UL) + 1;
    if (sl < 1) sl = 1;
    snprintf(line1, 17, "SAVE SETTINGS?%2ds", sl);
    snprintf(line2, 17, "NEXT=SAVE       ");
  }
  else if (alarmRinging) {
    snprintf(line1, 17, "** ALARM!! **   ");
    snprintf(line2, 17, "ALARM BTN=STOP  ");
  }
  else if (menuState == 10) {
    snprintf(line1, 17, "SET ALARM:      ");
    if (alarmSet) snprintf(line2, 17, "%02d:%02d:%02d ARMED  ", alarmHour, alarmMinute, alarmSecond);
    else          snprintf(line2, 17, "NOT SET         ");
  }
  else if (menuState == 11) {
    snprintf(line1, 17, "AL HOUR         ");
    char hh[3], mm[3], ss[3];
    if (blinkState) strcpy(hh, "  "); else sprintf(hh, "%02d", alSetHour);
    sprintf(mm, "%02d", alSetMinute);
    sprintf(ss, "%02d", alSetSecond);
    snprintf(line2, 17, "%s:%s:%s        ", hh, mm, ss);
  }
  else if (menuState == 12) {
    snprintf(line1, 17, "AL MIN          ");
    char hh[3], mm[3], ss[3];
    sprintf(hh, "%02d", alSetHour);
    if (blinkState) strcpy(mm, "  "); else sprintf(mm, "%02d", alSetMinute);
    sprintf(ss, "%02d", alSetSecond);
    snprintf(line2, 17, "%s:%s:%s        ", hh, mm, ss);
  }
  else if (menuState == 13) {
    unsigned long el = millis() - alarmSecondEnteredAt;
    int sl = (int)((ALARM_EDIT_TIMEOUT_MS - el) / 1000UL) + 1;
    if (sl < 1) sl = 1;
    snprintf(line1, 17, "AL SEC       %2ds", sl);
    char hh[3], mm[3], ss[3];
    sprintf(hh, "%02d", alSetHour);
    sprintf(mm, "%02d", alSetMinute);
    if (blinkState) strcpy(ss, "  "); else sprintf(ss, "%02d", alSetSecond);
    snprintf(line2, 17, "%s:%s:%s        ", hh, mm, ss);
  }
  else if (menuState == 0) {
    sprintf(line1, "%02d:%02d:%02d AL:%s",
            now.hour(), now.minute(), now.second(),
            alarmSet ? "ON" : "OF");
    sprintf(line2, "%s %02d/%02d/%04d",
            days[now.dayOfTheWeek()],
            now.day(), now.month(), now.year());
  }
  else {
    char hh[3], mm[3], ss[3];
    if (menuState == 1 && blinkState) strcpy(hh, "  "); else sprintf(hh, "%02d", setHour);
    if (menuState == 2 && blinkState) strcpy(mm, "  "); else sprintf(mm, "%02d", setMinute);
    if (menuState == 3 && blinkState) strcpy(ss, "  "); else sprintf(ss, "%02d", setSecond);
    snprintf(line1, 17, "%s:%s:%s SET     ", hh, mm, ss);

    char dayText[4], dd[3], mo[3], yy[5];
    if (menuState == 4 && blinkState) strcpy(dayText, "   "); else strcpy(dayText, days[setDay]);
    if (menuState == 5 && blinkState) strcpy(dd, "  ");       else sprintf(dd, "%02d", setDate);
    if (menuState == 6 && blinkState) strcpy(mo, "  ");       else sprintf(mo, "%02d", setMonth);
    if (menuState == 7 && blinkState) strcpy(yy, "    ");     else sprintf(yy, "%04d", setYear);
    snprintf(line2, 17, "%s %s/%s/%s   ", dayText, dd, mo, yy);
  }

  lcdUpdateLine(0, line1, oldLine1);
  lcdUpdateLine(1, line2, oldLine2);
}