#define VERSION "1.7.1"

#include <LiquidCrystal_I2C.h> // LiquidCrystal_I2C - Frank de Brabander (https://github.com/johnrickman/LiquidCrystal_I2C)

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display


#include <Arduino.h>
#include "DHT.h"           // DHT sensor library - Adafruit (1.4.4)
#include "EEPROM.h"        // pre-installed library
#include "DS3231.h"        // DS3231 - Andrew Wickert
#include <Sodaq_BMP085.h>  // Sodaq_BMP085  -> or better going with alternative BMP280??
#include <Encoder.h>       // Encoder - Paul Stoffregen

const int CLK = 5;         // Pin A5 is Encoder CLK (rotating detection)
const int DT  = 4;         // Pin A4 is Encoder DT  (rotating detection)
const int SW  = 3;         // Pin A3 is Encoder SW (klick / switch)

Encoder menueSelector(DT, CLK);

long menueValue = 0;
long oldMenueValue = 0;
int last_klick = 0;
bool release_klick = false;
long wErrorCode = 0;
DateTime startMoment;

#define LCD_ERROR  1
#define DHT_ERROR  2
#define BMP_ERROR  4
#define ENC_ERROR  8
#define RTC_ERROR 16

#define LIGHT_MAX 1            // current LCD type only supports on and off
#define LIGHT_MIN 0
int  lightValue = LIGHT_MAX;

#define MODE_NOT_IMPLEMENTED -1
#define MODE_WETTER_STATION   0
#define MODE_MAIN_MENU        1
#define MODE_SETTINGS         2
#define MODE_RESET_MINMAX     3
#define MODE_SET_ALTITUDE     4

int mode = MODE_WETTER_STATION;

int32_t altitude = 1044;      // like Balderschwang
int32_t pressure_in_pascal;

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

DS3231 RTC;
boolean Dummy;

Sodaq_BMP085 bmp;

/*
 * MENU STRUCTURES
 */

typedef struct menu {
  char entry_title[20];
  int  action;
  int next_action;                                                                                                                                                                                                                                       
};

const menu main_menu[] = {
  { "Set", MODE_SETTINGS, MODE_WETTER_STATION },    
  { "Reset", MODE_RESET_MINMAX, MODE_WETTER_STATION },
 // { "Test 3", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 4", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 5", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 6", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 7", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
  { "Wetter", MODE_WETTER_STATION, MODE_WETTER_STATION },
};

const menu set_menu[] = {
  { "Altitude", MODE_SET_ALTITUDE, MODE_WETTER_STATION },
 // { "Test 2", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 3", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 4", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 5", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 6", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
 // { "Test 7", MODE_NOT_IMPLEMENTED, MODE_WETTER_STATION },
  { "Back",     MODE_MAIN_MENU,     MODE_WETTER_STATION },
};

byte deg[8] = { B00100,
                B01010,
                B01010,
                B00100,
                B00000,
                B00000,
                B00000,
                B00000
              };

byte max[8] = { B00100,
                B01110,
                B11111,
                B01110,
                B01110,
                B01110,
                B01110,
                B00000
              };

byte min[8] = { B00000,
                B01110,
                B01110,
                B01110,
                B01110,
                B11111,
                B01110,
                B00100
              };

/*
      EEPROM format version 1
      Offset : Type  : Length : Remark
      0  :    : Byte  : 1      : Version
      1  :    : Byte  : 1      : Reset Info (byte) 255-Reset all values
      2  :    : Float : 4      : tempMax
      6  :    : Float : 4      : tempMin
      10 :    : int   : 4/2?   : Altitude
*/

typedef struct efv1 { 
  byte  version;
  byte  reset;
  float tempMax;
  float tempMin;
  int   altitude;
};

struct efv1 eeprom_values;
bool   eeprom_update = false;

#define ADDR_VERSION  0
#define ADDR_RESET    1
#define ADDR_TEMP_MAX 2
#define ADDR_TEMP_MIN 6

bool  useMax = false;
bool  useMin = false;
float tempMax = 0.0;
float tempMin = 0.0;

int klicked = 0;

void read_eprom_values(efv1 *eprom_data) {
  int count;
  byte *eByte = (byte *) eprom_data;
  for ( count = 0; count < sizeof(efv1); count++ ) {
    *eByte = EEPROM.read(count);
    eByte++;
  }
}

void write_eprom_values(efv1 *eprom_data) {
  int count;
  byte *eByte = (byte *) eprom_data;
  for ( count = 0; count < sizeof(efv1); count++ ) {
    EEPROM.write(count,*eByte);
    eByte++;
  }
}

/*
   byte floatTest[] = { 205, 204, 192, 65 };
*/

bool eprom_check_float(int addr)
{
  // currently only checks, if the field in the eprom is unequal 0 0 0 0
  int count;
  bool result = false;
  byte check;
  for ( count = 0; count < sizeof(float); count++ ) {
    check = EEPROM.read(addr + count);
    if ( check != 0 ) {
      result = true;
    }
  }
}

void eprom_dump_flat(int addr, int len)
{
  int count;
  byte eeByte;
  for ( count = 0; count < len; count++ ) {
    eeByte = EEPROM.read((addr + count));
    Serial.print(eeByte); Serial.print(F(" "));
  }
  Serial.println();
}

void dump_float(float dumpMe)
{
  int count;
  byte * emap2float = (byte *) &dumpMe;
  byte eeByte;
  Serial.print(dumpMe); Serial.print(F(" = "));
  for ( count = 0; count < sizeof(float); count++ ) {
    eeByte = emap2float[count];
    Serial.print(eeByte); Serial.print(F(" "));
  }
  Serial.println();
}

void setup()
{
  // do not accept klicks during setup
  
  release_klick = false;
  int device_rc = 0;
  wErrorCode = 0;     // reset global variable to capture error-bits
  Serial.begin(9600); 
  Serial.print(F("weatherstation ")); Serial.println(F(VERSION));
  // EEPROM.write(ADDR_VERSION,1); EEPROM.write(ADDR_RESET,255);
  // TODO: Allow DHT to be optional, if BMP280 already covers the temperature, huminity and pressure
  dht.begin();  // dht.begin is of type void, so we could not check the return code here
  Serial.print(F("TEST-rc: ")); Serial.println(device_rc);
  bmp.begin();

  lcd.init();                      // initialize the lcd
  // Print a message to the LCD.
  lcd.createChar(0, deg);
  lcd.createChar(1, max);
  lcd.createChar(2, min);
  lcd.backlight();
  lcd.noBlink();

  lcd.setCursor(0, 0);
  lcd.print(F("fh weatherstation"));
  lcd.setCursor(0, 1);
  lcd.print(F(VERSION));

  delay(5000);

  lcd.clear();
  /*
      initialize RTC, if year is equal 0 otherwise keep already stored time
  */
  if ( RTC.getYear() == 0 ) {
    RTC.setDate(15);
    RTC.setMonth(8);
    RTC.setYear(22);
    RTC.setHour(9);
    RTC.setMinute(0);
    RTC.setSecond(0);
  }
  startMoment = RTClib::now();
  eprom_dump_flat(0, 16);
  read_eprom_values(&eeprom_values);
  Serial.print("version: "); Serial.println(eeprom_values.version);
  Serial.print("reset:   "); Serial.println(eeprom_values.reset);
  Serial.print("tempMax:   "); Serial.println(eeprom_values.tempMax);
  Serial.print("tempMin:   "); Serial.println(eeprom_values.tempMin);
  Serial.print("altitude:   "); Serial.println(eeprom_values.altitude);
  if ( eeprom_values.version == 1 ) {
    useMax = true;
    useMin = true;
    tempMax = eeprom_values.tempMax;
    tempMin = eeprom_values.tempMin;
    Serial.println("Migrate version 1 to version 2.");
    eeprom_values.version = 2;          // migrate version 1 to 2
    eeprom_values.altitude = altitude;  // version 2 also needs altitude
    write_eprom_values(&eeprom_values);
  } else if ( eeprom_values.version == 2 ) {
    useMax = true;
    useMin = true;
    tempMax = eeprom_values.tempMax;
    tempMin = eeprom_values.tempMin;
    altitude = eeprom_values.altitude;
  }
  pinMode(SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SW), Interrupt, FALLING);
  last_klick = millis();
  Serial.print("last_klick:   "); Serial.println(last_klick);
  // begin to accept klicks
  release_klick = true;
}

void show_menue(int arraySize,const menu theMenu[], int entry)
{
  lcd.clear();
  int row, col;
  if ( entry < 4 ) {
    col=0; row=entry;
  } else {
    col=10; row=entry-4;
  }

  lcd.setCursor(col, row);
  lcd.print(F("*"));

  Serial.println(arraySize);
  for (int i=0; i<arraySize; i++) { // TODO make 8 flexible!!
    if ( i < 4 ) {
      col=1; row=i;
    } else {
      col=11; row=i-4;
    } 
    lcd.setCursor(col, row);
    lcd.print(theMenu[i].entry_title);  
  }
}

/*
 * check_selector 
 * - uses referenced variable oldValue (rw) -> maybe to be transferred into a field of an object to avoid side effects 
 * - params: referece to oldValue, lastSelectedValue, MIN value, MAX value
 */
boolean check_selector(long *oldValue, long *newValue, int *selected_value, int min_value, int max_value)
{
  boolean event=false;
  *newValue = menueSelector.read() / 4;
  if ( *newValue != *oldValue ) {
    event=true;
    Serial.print(F("o: ")); Serial.print(*selected_value);
    if ( *newValue > *oldValue ) {
      (*selected_value)++;
    } else {
      (*selected_value)--;
    }
    if ( *selected_value > max_value ) {
      *selected_value = max_value;
    } else if ( *selected_value < min_value ) {
      *selected_value = min_value;
    }
    Serial.print(F(" n: ")); Serial.println(*selected_value);
    *oldValue = *newValue;
  }
  return event;
}

int menue()
{
  int selected_entry = 0;
  klicked = 0; // reset "klicked" to react on a new event
  int arraySize = int(sizeof(main_menu) / sizeof(main_menu[0]));
  show_menue(arraySize,main_menu,0);
  while (! klicked) {    
    if ( check_selector(&oldMenueValue, &menueValue, &selected_entry, 0, arraySize-1)) {
      show_menue(arraySize,main_menu, selected_entry);
      Serial.print(F("Menue: ")); Serial.print(selected_entry); Serial.print(F(" ")); Serial.print(main_menu[selected_entry].entry_title); Serial.print(F(" ")); Serial.print(main_menu[selected_entry].action);
      Serial.println();  
    }
  }
  lcd.clear();
  klicked = 0;
  return selected_entry;
}

// MODE_SETTINGS
int set_menue()
{
  int selected_entry = 0;
  Serial.println("set_menu");
  klicked = 0; // reset "klicked" to react on a new event
  int arraySize = int(sizeof(set_menu) / sizeof(set_menu[0]));
  show_menue(arraySize, set_menu, 0);
  while (! klicked) {    
    if ( check_selector(&oldMenueValue, &menueValue, &selected_entry, 0, 7)) {
      show_menue(arraySize, set_menu, selected_entry);
      Serial.print(F("Menue: ")); Serial.print(selected_entry); Serial.print(F(" ")); Serial.print(set_menu[selected_entry].entry_title); Serial.print(F(" ")); Serial.print(set_menu[selected_entry].action);
      Serial.println();  
    }
  }
  lcd.clear();
  klicked = 0;
  return selected_entry;
}

int32_t set_altitude(int32_t oldAltitude) {
  // TODO allow fast-setting mode (like jumping 10m or 100m), if events are quite fast
  int newAltitude=oldAltitude;
  lcd.setCursor(0,1);
  lcd.print(newAltitude);
  while (! klicked) {
    if ( check_selector(&oldMenueValue, &menueValue, &newAltitude, 0, 4000) ) {
      lcd.setCursor(0,1);
      lcd.print(newAltitude);
    }
  }
  lcd.clear();
  klicked = 0;
  return (int32_t) newAltitude;
}

void wetterStation()
{
  // TODO Optionally (also) output values to Serial
  int runtime;
  int seconds, minutes, hours, date, month, year;
  bool blinking = true;
  char bufferOut[20];
  char str_temp[20];

  /*
   * DHT sensor
   */
  // TODO the DHT11 has some
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  if (useMax) {
    if ( t > tempMax ) {
      tempMax = t;
      Serial.print(F("Set new Max ")); Serial.print(tempMax); Serial.print(F(": "));
      eeprom_values.tempMax = tempMax;
      eeprom_update = true;      
    }
  } else {
    tempMax = t;
    eeprom_values.tempMax = tempMax;
    eeprom_update = true;
    useMax = true;
  }
  if (useMin) {
    if ( t < tempMin ) {
      tempMin = t;
      Serial.print(F("Set new Min ")); Serial.print(tempMin); Serial.print(F(": "));
      eeprom_values.tempMin = tempMin;
      eeprom_update = true;
    }
  } else {
    tempMin = t;
    eeprom_values.tempMin = tempMin;
    eeprom_update = true;
    useMin = true;
  }

  // Check if any reads failed and exit early (to try again).
  // TODO - Do not exit fast but try to continue with other devices
  // Check is done by isnan() (is not a number) - if temperature is not a number, something went wrong
  if (isnan(t)) {
    // TODO add error-handling for RTC (if device is defect or not available)
    // In my tess I always got 1482191185 as unixtime, if the RTC is not available    
    //Serial.print(F("Failed to read from DHT sensor!"));
    // mark DHT-error in the error-vector wErrorCode
    wErrorCode = wErrorCode | DHT_ERROR;
        sprintf(bufferOut, " ---");
  } else {  
    dtostrf(t, 3, 1, str_temp);
    sprintf(bufferOut, " %s", str_temp);
  }
  /*
   * LCD-Display current temperature and huminity
   */
  lcd.setCursor(0, 0);
  lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  lcd.setCursor(10, 0);
  lcd.print((int)h); lcd.print(F("%"));

  //dtostrf(hic, 3, 1, str_temp);
  // sprintf(bufferOut, "(%s", str_temp);

  lcd.setCursor(0, 1);
  /*
   * BMP sensor
   */
  // TODO how to check BMP communication error
  // TODO set wErrCode || BMP_ERROR, if communication is broken
  pressure_in_pascal = bmp.readPressure(altitude, 9.81);
  dtostrf(pressure_in_pascal / 100.0, 4, 0, str_temp);
  sprintf(bufferOut, "%s hPa", str_temp);
  lcd.print((bufferOut));

  dtostrf(altitude, 4, 0, str_temp);
  sprintf(bufferOut, "%s m", str_temp);
  lcd.setCursor(0, 2);
  lcd.print(bufferOut);

  /*
   * LCD-Display for Temp Min/Max Values
   */
  dtostrf(tempMax, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10, 1);
  lcd.print((char)1); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  dtostrf(tempMin, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10, 2);
  lcd.print((char)2); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  runtime = millis() / 1000;

  /*
   * LCD-Display Date and Time
   */
  year = RTC.getYear();
  month = RTC.getMonth(Dummy);
  date = RTC.getDate();
  hours = RTC.getHour(Dummy, Dummy);
  minutes =  RTC.getMinute();
  seconds = RTC.getSecond();

  lcd.setCursor(0, 3);
  sprintf(bufferOut, "%02i", date);    lcd.print(bufferOut); lcd.print(F("."));
  sprintf(bufferOut, "%02i", month);   lcd.print(bufferOut); lcd.print(F("."));
  sprintf(bufferOut, "%02i", year);    lcd.print(bufferOut); lcd.print(F(" "));

  sprintf(bufferOut, "%02i", hours);   lcd.print(bufferOut); lcd.print(F(":"));
  sprintf(bufferOut, "%02i", minutes); lcd.print(bufferOut); lcd.print(F(":"));
  sprintf(bufferOut, "%02i", seconds); lcd.print(bufferOut);
  /*
   * Serial-Out error-code vector
   */
  DateTime currentMoment = RTClib::now();
  long timeCode = currentMoment.unixtime();
  if (timeCode - startMoment.unixtime() == 0 ) {
    // RTC not working
    wErrorCode = wErrorCode | RTC_ERROR;
    timeCode = millis()/1000;
  }
  Serial.print(F("Errorcode: "));
  Serial.print(wErrorCode);
  if ((wErrorCode & RTC_ERROR)) {
    Serial.print(F(" RTC"));
  }
  if ((wErrorCode & DHT_ERROR)) {
    Serial.print(F(" DHT"));
  }
  if ((wErrorCode & LCD_ERROR)) {
    Serial.print(F(" LCD"));
  }
  if ((wErrorCode & BMP_ERROR)) {
    Serial.print(F(" BMP"));
  }
  if ((wErrorCode & ENC_ERROR)) {
    Serial.print(F(" ENC"));
  }
  Serial.print(F(" Timecode: "));
  Serial.print(timeCode);
  if ((wErrorCode & RTC_ERROR)) {
    Serial.println(F("s"));
  } else {
    Serial.println(F("ux"));
  }
}

void Interrupt()
{
  int now = millis();
  if ( now < last_klick ) { // millis overflow
    last_klick = now;
  }
  if ( release_klick && (last_klick + 500 <= now) ) { // klicks "entprellen"
    Serial.print("Klick!! ");
    Serial.print(now); Serial.print(" ");
    Serial.print(last_klick); Serial.print(" ");
    Serial.print(now - last_klick);
    Serial.println();
    klicked = 1;
    last_klick = now;
  } else {
    Serial.print("Klick filtered ");    
  }
}

int lastRuntime = 0;
int currRuntime;

void loop()
{
  if ( klicked && mode == MODE_WETTER_STATION ) {
    mode = MODE_MAIN_MENU;
    klicked = 0;
  }
  
  menueValue = menueSelector.read() / 4; // in the serial dump I see alway 4 hits per tick
  if ( mode == MODE_WETTER_STATION ) {
    /*
     * Handling for mode "wetter station"
     */     
    if ( check_selector(&oldMenueValue, &menueValue, &lightValue, LIGHT_MIN, LIGHT_MAX)) {
      lcd.setBacklight(lightValue);
      // show_menue(selected_entry);
      Serial.print(F("Light: ")); Serial.print(lightValue);
      Serial.println();
    }
   /*
       handle overflow of millis()
    */
    currRuntime = millis();
    if ( currRuntime < lastRuntime ) {
      lastRuntime = currRuntime;
    }

    if ( currRuntime >= lastRuntime + 1000 ) {
      lastRuntime = currRuntime;
      wetterStation();
      if ( eeprom_update ) {
          Serial.println("write_eprom_values()");
          eeprom_update = false;
          write_eprom_values(&eeprom_values);
      }
    }
  } else if (mode == MODE_MAIN_MENU ) {
     int follow_up = menue();    // get entry later
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print(F("Entry ")); lcd.print(main_menu[follow_up].entry_title);
     lcd.setCursor(0, 1);
     lcd.print(F("Action ")); lcd.print(main_menu[follow_up].action);
     mode = main_menu[follow_up].action;
     delay(1000);
     lcd.clear();
     /*
     if (follow_up == 1) {
        mode = MODE_RESET_MINMAX;
     } else {
       mode = MODE_WETTER_STATION; // change later (need to jump to submenue or subfunction)
       delay(500);
     }
     */
     klicked = 0;
  } else if (mode == MODE_SETTINGS ) {
     Serial.println("MODE_SETTINGS");
     int follow_up = set_menue();
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print(F("Set Entry ")); lcd.print(set_menu[follow_up].entry_title);
     lcd.setCursor(0, 1);
     lcd.print(F("Set Action ")); lcd.print(set_menu[follow_up].action);
     mode = set_menu[follow_up].action;
     delay(1000);
     lcd.clear();
  } else if (mode == MODE_RESET_MINMAX ) {
    useMax = false;
    useMin = false;
    mode = MODE_WETTER_STATION;
  } else if (mode == MODE_SET_ALTITUDE ) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Set altitude"));
    altitude = set_altitude(altitude);
    eeprom_values.altitude = altitude;
    delay(1000);
    lcd.clear();
    lcd.print(F("Saveing values..."));
    write_eprom_values(&eeprom_values);
    lcd.clear();
    mode = MODE_WETTER_STATION;
  } else if (mode == MODE_NOT_IMPLEMENTED ) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Not implmented!"));
    delay(1000);
    lcd.clear();
    // mode = main_menu[follow_up].next_action;
    mode = MODE_WETTER_STATION;
  }
}
