#define VERSION "1.4.24"

#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display


#include <Arduino.h>
#include "DHT.h"
#include "EEPROM.h"
#include "DS3231.h"
#include <Sodaq_BMP085.h>
#include <Encoder.h>

const int CLK = 5;
const int DT  = 4;
const int SW  = 3;

Encoder menueSelector(DT, CLK);

long menueValue = 0;
long oldMenueValue = 0;
#define LIGHT_MAX 1            // current LCD type only supports on and off
#define LIGHT_MIN 0
int  lightValue = LIGHT_MAX;

#define MODE_WETTER_STATION 0
#define MODE_MAIN_MENUE          1
#define MODE_SETTINGS       2
#define MODE_RESET_MINMAX   3
#define MODE_

int mode = MODE_WETTER_STATION;

int32_t altitude = 1200;      // like Balderschwang
int32_t pressure_in_pascal;

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
// tself to work on faster procs.

DHT dht(DHTPIN, DHTTYPE);

DS3231 RTC;
boolean Dummy;

Sodaq_BMP085 bmp;

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
      EEPROM format
      Offset : Type  : Length : Remark
      0 :    : Byte  : 1      : Version
      1 :    : Byte  : 1      : Reset Info (byte) 255-Reset all values
      2 :    : Float : 4      : tempMax
      6 :    : Float : 4      : tempMin
*/

//
//
#define ADDR_VERSION  0
#define ADDR_RESET    1
#define ADDR_TEMP_MAX 2
#define ADDR_TEMP_MIN 6

bool  useMax = false;
bool  useMin = false;
float tempMax = 0.0;
float tempMin = 0.0;

int klicked = 0;

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

float eprom_get_float(int addr)
{
  int count;
  float result;
  byte * emap2float;
  byte help;
  emap2float = (byte *) &result;
  for ( count = 0; count < sizeof(float); count++ ) {
    help = EEPROM.read(addr + count);
    emap2float[count] = help;
    /*
     *     
    Serial.print(F("EE["));
    Serial.print(addr + count);
    Serial.print(F("]="));
    Serial.println(help);
    */
  }
  return result;
}

void eprom_set_float(int addr, float value)
{
  int count;
  byte * emap2float;
  emap2float = (byte *) &value;
  for ( count = 0; count < sizeof(float); count++ ) {
    /*
    Serial.print(F("EE["));
    Serial.print(addr + count);
    Serial.print(F("]="));
    Serial.println(emap2float[count]);
    */
    EEPROM.write(addr + count, emap2float[count]);
  }
}

void eprom_reset_float(int addr)
{
  int count;
  for ( count = 0; count < sizeof(float); count++ ) {
    EEPROM.write(addr + count, 0);
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
  Serial.begin(9600);
  Serial.print(F("DHT_LCD loop ")); Serial.println(F(VERSION));
  // EEPROM.write(ADDR_VERSION,1); EEPROM.write(ADDR_RESET,255);
  dht.begin();
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
    RTC.setDate(10);
    RTC.setMonth(9);
    RTC.setYear(20);
    RTC.setHour(20);
    RTC.setMinute(22);
    RTC.setSecond(0);
  }

  eprom_dump_flat(0, 16);
  if (true || eprom_check_float(ADDR_TEMP_MAX)) {
    useMax = true;
    Serial.println(F("useMax=true"));
    tempMax = eprom_get_float(ADDR_TEMP_MAX);  // 1st field in epropm is tempMax
    Serial.print(F("tempMax from EEPROM: ")); Serial.println(tempMax);
  }
  if (true || eprom_check_float(ADDR_TEMP_MIN)) {
    useMin = true;
    Serial.println(F("useMin=true"));
    tempMin = eprom_get_float(ADDR_TEMP_MIN);  // 2nd field in epprom is tempMin
    Serial.print(F("tempMax from EEPROM: ")); Serial.println(tempMin);
  }

  pinMode(SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SW), Interrupt, FALLING);
}

void show_menue(int entry)
{
  int row, col;
  if ( entry < 4 ) {
    col=0; row=entry;
  } else {
    col=10; row=entry-4;
  }
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(F("Set"));
  lcd.setCursor(1, 1);
  lcd.print(F("Reset"));
  lcd.setCursor(1, 2);
  lcd.print(F("Drei"));
  lcd.setCursor(1, 3);
  lcd.print(F("Vier"));
  lcd.setCursor(11, 0);
  lcd.print(F("Fuenf"));  
  lcd.setCursor(col, row);
  lcd.print(F("*"));
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
  show_menue(0);
  while (! klicked) {
    /*
    #if 0
    menueValue = menueSelector.read() / 4;
    if ( menueValue != oldMenueValue ) {
      if ( menueValue > oldMenueValue ) {
        selected_entry++;
      } else {
        selected_entry--;
      }
      if ( selected_entry > 7 ) {
        selected_entry = 7;
      } else if ( selected_entry < 0 ) {
        selected_entry = 0;
      }
      show_menue(selected_entry);
      Serial.print(menueValue); Serial.print(F(" ")); Serial.println(selected_entry);
      oldMenueValue = menueValue;
    }
    #endif
    */
    
    if ( check_selector(&oldMenueValue, &menueValue, &selected_entry, 0, 7)) {
      show_menue(selected_entry);
      Serial.print(F("Menue: ")); Serial.println(selected_entry);  
    }
  }
  lcd.clear();
  klicked = 0;
  return selected_entry;
}

void wetterStation()
{
  int runtime;
  int seconds, minutes, hours, date, month, year;
  bool blinking = true;
  char bufferOut[20];
  char str_temp[20];

  // TODO the DHT11 has some
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  // float f = dht.readTemperature(true);

  if (useMax) {
    if ( t > tempMax ) {
      tempMax = t;
      Serial.print(F("Set new Max ")); Serial.print(tempMax); Serial.print(F(": "));
      eprom_set_float(ADDR_TEMP_MAX, tempMax);
      // eprom_dump_flat(0,4);
    }
  } else {
    tempMax = t;
    eprom_set_float(ADDR_TEMP_MAX, tempMax);
    useMax = true;
  }
  if (useMin) {
    if ( t < tempMin ) {
      tempMin = t;
      Serial.print(F("Set new Min ")); Serial.print(tempMin); Serial.print(F(": "));
      eprom_set_float(ADDR_TEMP_MIN, tempMin);
      // eprom_dump_flat(5,4);
    }
  } else {
    tempMin = t;
    eprom_set_float(ADDR_TEMP_MIN, tempMin);
    useMin = true;
  }

  // Check if any reads failed and exit early (to try again).
  if (isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  dtostrf(t, 3, 1, str_temp);
  sprintf(bufferOut, " %s", str_temp);

  lcd.setCursor(0, 0);
  lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  lcd.setCursor(10, 0);
  lcd.print((int)h); lcd.print(F("%"));

  //dtostrf(hic, 3, 1, str_temp);
  // sprintf(bufferOut, "(%s", str_temp);

  lcd.setCursor(0, 1);
  pressure_in_pascal = bmp.readPressure(altitude, 9.81);
  dtostrf(pressure_in_pascal / 100.0, 4, 0, str_temp);
  sprintf(bufferOut, "%s hPa", str_temp);
  lcd.print((bufferOut));

  lcd.setCursor(0, 2);
  lcd.print(lightValue);

  dtostrf(tempMax, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10, 1);
  lcd.print((char)1); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  dtostrf(tempMin, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10, 2);
  lcd.print((char)2); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  runtime = millis() / 1000;

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
}

int lastRuntime = 0;
int currRuntime;

void loop()
{
  if ( klicked && mode == MODE_WETTER_STATION ) {
    mode = MODE_MAIN_MENUE;
    klicked = 0;
  }
  
  menueValue = menueSelector.read() / 4; // in the serial dump I see alway 4 hits per tick
  if ( mode == MODE_WETTER_STATION ) {
    /*
     * Handling for mode "wetter station"
     */
    if ( menueValue != oldMenueValue ) {
      if ( menueValue > oldMenueValue ) {
        lightValue++;
      } else {
        lightValue--;
      }
      if ( lightValue > LIGHT_MAX ) {
        lightValue = LIGHT_MAX;
      } else if ( lightValue < LIGHT_MIN ) {
        lightValue = LIGHT_MIN;
      }
      Serial.println(lightValue);
      oldMenueValue = menueValue;
      Serial.println(menueValue);
    }
    /*
      if ( menueValue % 2 == 0 ) {
      lcd.backlight();
      } else {
      lcd.noBacklight();
      }
    */
    lcd.setBacklight(lightValue);
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
    }
  } else if (mode == MODE_MAIN_MENUE ) {
     int follow_up = menue();    // get entry later
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print(F("Entry ")); lcd.print(follow_up);
     if (follow_up == 1) {
        mode = MODE_RESET_MINMAX;
     } else {
       mode = MODE_WETTER_STATION; // change later (need to jump to submenue or subfunction)
       delay(500);
     }
     klicked = 0;
  } else if (mode == MODE_RESET_MINMAX ) {
    useMax = false;
    useMin = false;
    mode = MODE_WETTER_STATION;
  }
}

int last_klick = 0;

void Interrupt()
{
  int now = millis();
  if ( now < last_klick ) { // millis overflow
    last_klick = now;
  }
  if ( last_klick + 500 <= now ) { // klicks "entprellen"
    Serial.print("Klick!! ");
    Serial.print(now - last_klick);
    Serial.println();
    klicked = 1;
    last_klick = now;
  }
}
