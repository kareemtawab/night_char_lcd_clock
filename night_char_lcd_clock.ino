#include <NTPClient.h>     // https://github.com/arduino-libraries/NTPClient
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <OneWire.h>
#include <DallasTemperature.h>
/*
  You need to adjust the UTC offset for your timezone in milliseconds. Refer the list of UTC time offsets.  Here are some examples for different timezones:
  For UTC -5.00 : -5 * 60 * 60 : -18000
  For UTC +1.00 : 1 * 60 * 60 : 3600
  For UTC +0.00 : 0 * 60 * 60 : 0
  const long utcOffsetInSeconds = 3600;
*/
const long utcOffsetInSeconds = 7200;  // set offset
//char daysOfTheWeek[7][12] = {"Nedelja", "Ponedeljak", "Utorak", "Sreda", "Cetvrtak", "Petak", "Subota"};  // Serbian
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};  // English
// Define NTP Client to get time
/*
  Area_____________________________________________________HostName
  Worldwide_______________________________________________ pool.ntp.org
  Asia____________________________________________________ asia.pool.ntp.org
  Europe__________________________________________________ europe.pool.ntp.org
  North America___________________________________________ north-america.pool.ntp.org
  Oceania_________________________________________________ oceania.pool.ntp.org
  South America___________________________________________ south-america.pool.ntp.org

*/

// Default I2C pins on NodeMcu is    SDA - D2 , SCL - D1

RTC_DS3231 ds3231;
const int driftComp = -1;    //seconds to adjust clock by per two hours to compensate for RTC drift

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
LiquidCrystal_I2C lcd(0x3F, 16, 2);  // I2c address, columns lcd module, rows lcd module
WiFiManager wifiManager;
#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallastemp(&oneWire);

byte counter;

struct LCDNumber {
  byte top;
  byte bottom;
};

class LCDBigNumbers {
  private:
    LiquidCrystal_I2C * _lcd;
    int _row;
    int _col;
    long _value; /*0..100*/

    void _clear() {
      int cont = 1;
      long x = 9;
      while (_value > x) {
        cont++;
        x = x * 10 + 9;
      }
      for (int i = 0; i < cont; i++) {
        _lcd->setCursor(_col + i, _row);
        //_lcd->print( " " );
        _lcd->setCursor(_col + i, _row + 1);
        //_lcd->print( " " );
      }
    }
  public:
    static byte c0[8];
    static byte c1[8];
    static byte c2[8];
    static byte c3[8];
    static byte c4[8];
    static byte c5[8];
    static byte c6[8];
    static byte c7[8];
    static LCDNumber _lcd_numbers[];

    void createChars() {
      _lcd->createChar(0, c0);
      _lcd->createChar(1, c1);
      _lcd->createChar(2, c2);
      _lcd->createChar(3, c3);
      _lcd->createChar(4, c4);
      _lcd->createChar(5, c5);
      _lcd->createChar(6, c6);
      _lcd->createChar(7, c7);
    }

    LCDBigNumbers(LiquidCrystal_I2C * lcd, int row, int col) {
      _lcd = lcd;      _row = row;      _col = col;
    }

    void setRow(int row) {
      _clear();
      _row = row;
      setValue(_value);
    }
    void setCol(int col) {
      _clear();
      _col = col;
      setValue(_value);
    }

    void setValue(long value) {
      _clear();
      _value = value;

      int cont = 1;
      long x = 9;
      while (abs(_value) > x) {
        cont++;
        x = x * 10 + 9;
      }

      for (int i = 0; i < cont; i++) {
        int n = value / pow(10, cont - 1 - i);
        value = value - pow(10, cont - 1 - i) * n;

        _lcd->setCursor(_col + i, _row);
        _lcd->write( _lcd_numbers[n].top );
        _lcd->setCursor(_col + i, _row + 1);
        _lcd->write( _lcd_numbers[n].bottom );
      }

    }
};

byte LCDBigNumbers::c0[8] = {B11111, B10001, B10001, B10001, B10001, B10001, B10001, B10001};
byte LCDBigNumbers::c1[8] = {B10001, B10001, B10001, B10001, B10001, B10001, B10001, B11111};
byte LCDBigNumbers::c2[8] = {B00001, B00001, B00001, B00001, B00001, B00001, B00001, B00001};
byte LCDBigNumbers::c3[8] = {B11111, B00001, B00001, B00001, B00001, B00001, B00001, B11111};
byte LCDBigNumbers::c4[8] = {B11111, B10000, B10000, B10000, B10000, B10000, B10000, B11111};
byte LCDBigNumbers::c5[8] = {B11111, B00001, B00001, B00001, B00001, B00001, B00001, B00001};
byte LCDBigNumbers::c6[8] = {B11111, B10001, B10001, B10001, B10001, B10001, B10001, B11111};
byte LCDBigNumbers::c7[8] = {B00001, B00001, B00001, B00001, B00001, B00001, B00001, B11111};


LCDNumber LCDBigNumbers::_lcd_numbers[] = {
  {0, 1}, //0
  {2, 2}, //1
  {5, 4}, //2
  {3, 7}, //3
  {1, 2}, //4
  {4, 7}, //5
  {4, 1}, //6
  {5, 2}, //7
  {6, 1}, //8
  {6, 7} // 9
};

LCDBigNumbers lcdNum(&lcd, 0, 0); //inclui uma barra no lcd, primeira linha, coluna 8. tamanho 8

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(2000);
  Serial.println("keep holding button on power-up to set time/date via Wifi");
  lcd.backlight();
  lcd.init();
  lcdNum.createChars();
  lcd.clear();
  lcd.setCursor(0, 0); //   column, row
  lcd.print("Night Char Clock");
  lcd.setCursor(0, 1); //   column, row
  lcd.print("Compiled 09/2020");
  delay(3000);
  dallastemp.begin();
  pinMode(14, INPUT_PULLUP);  //pin D5
  pinMode(12, INPUT);  //pin D6
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  ds3231.begin();
  lcd.clear();
  if (digitalRead(14) == 0) {
    digitalWrite(LED_BUILTIN, LOW);
    lcd.setCursor(0, 0); //   column, row
    lcd.print("Connect to AP,");
    lcd.setCursor(0, 1); //   column, row
    lcd.print("IP: 192.168.4.1");
    wifiManager.autoConnect("HK89 CharLCD Clock");
    lcd.clear();
    lcd.setCursor(0, 0); //   column, row
    lcd.print("Wifi connected,");
    lcd.setCursor(0, 1); //   column, row
    lcd.print("getting epoch...");
    delay (3000);
    timeClient.begin();
    timeClient.update();
    ds3231.adjust(timeClient.getEpochTime());
    lcd.clear();
    lcd.setCursor(0, 0); //   column, row
    lcd.print("Got epoch,");
    lcd.setCursor(0, 1); //   column, row
    lcd.print("RTC is set.");
    delay (3000);
    lcd.clear();
    digitalWrite(LED_BUILTIN, HIGH);
    WiFi.mode(WIFI_OFF);
  }
  if (ds3231.lostPower()) {
    Serial.println("ERROR! RTC is not running!");
    lcd.setCursor(0, 0); //   column, row
    lcd.print("ERROR!");
    lcd.setCursor(0, 1); //   column, row
    lcd.print("RTC not running!");
    while (1) {
      delay(0);
    }
  }
  WiFi.mode(WIFI_OFF);
}

void loop() {
  if (digitalRead(14) == 0) {
    lcd.init();
    lcdNum.createChars();
  }
  if (digitalRead(12) == 1) {
    lcd.backlight();
    lcd.setCursor(9, 1);
    lcd.print(" ");
  }
  else {
    lcd.noBacklight();
    lcd.setCursor(9, 1);
    lcd.print(".");
  }
  DateTime now = ds3231.now();
  int hourin12;
  if (now.hour() > 12) {
    hourin12 = now.hour() - 12;
  }
  else {
    hourin12 = now.hour();
  }
  if (now.hour() == 0) {
    hourin12 = 12;
  }
  lcdNum.setCol(0);
  lcdNum.setValue((hourin12 / 10) % 10);
  lcdNum.setCol(1);
  lcdNum.setValue((hourin12) % 10);
  lcdNum.setCol(3);
  lcdNum.setValue(((int)now.minute() / 10) % 10);
  lcdNum.setCol(4);
  lcdNum.setValue(((int)now.minute()) % 10);
  lcdNum.setCol(6);
  lcdNum.setValue(((int)now.second() / 10) % 10);
  lcdNum.setCol(7);
  lcdNum.setValue(((int)now.second()) % 10);
  lcd.setCursor(2, 0);
  lcd.print(".");
  lcd.setCursor(2, 1);
  lcd.print(".");
  lcd.setCursor(5, 0);
  lcd.print(".");
  lcd.setCursor(5, 1);
  lcd.print(".");
  lcd.setCursor(8, 0);
  if (now.hour() <= 12) {
    lcd.print("A  ");
  }
  else {
    lcd.print("P  ");
  }
  lcd.setCursor(8, 1);
  lcd.print("M");
  lcd.setCursor(10, 1);
  lcd.print(" ");
  if (counter % 3 == 0) {
    lcdNum.setCol(13);
    dallastemp.requestTemperatures();
    float t = dallastemp.getTempCByIndex(0);
    Serial.println(t);
    lcdNum.setValue(((int)t / 10) % 10);
    lcdNum.setCol(14);
    lcdNum.setValue(((int)t % 10));
    lcd.setCursor(15, 0);
    //lcd.write(char(B11011111));
    lcd.write(char(B10100001));
    lcd.setCursor(15, 1);
    //lcd.write(byte(4));
    lcd.print("C");
    lcd.setCursor(11, 0);
    lcd.print("  ");
    lcd.setCursor(11, 1);
    lcd.print("  ");
  }
  else {
    lcdNum.setCol(11);
    lcdNum.setValue(((int)now.day() / 10) % 10);
    lcdNum.setCol(12);
    lcdNum.setValue(((int)now.day()) % 10);
    lcdNum.setCol(14);
    lcdNum.setValue(((int)now.month() / 10) % 10);
    lcdNum.setCol(15);
    lcdNum.setValue(((int)now.month()) % 10);
    lcd.setCursor(13, 0);
    lcd.print(" ");
    lcd.setCursor(13, 1);
    lcd.print(".");
  }
  if (counter > 254) {
    counter = 0;
  }
  else {
    counter++;
  }
  //ESP.deepSleep(5e6, WAKE_RF_DISABLED);
  yield();
  delay(500);
}
