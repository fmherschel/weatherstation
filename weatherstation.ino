#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display


#include <Arduino.h>
#include "DHT.h"
#include "EEPROM.h"
#include "DS3231.h"

#define VERSION "1.0.6"

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
// tself to work on faster procs.

DHT dht(DHTPIN, DHTTYPE);

DS3231 RTC;
boolean Dummy;

byte deg[8] = { B00100,
                B01010,
                B01010,
                B00100,
                B00000,
                B00000,
                B00000,
                B00000};

byte max[8] = { B00100,
                B01110,
                B11111,
                B01110,
                B01110,
                B01110,
                B01110,
                B00000};

byte min[8] = { B00000,
                B01110,
                B01110,
                B01110,
                B01110,
                B11111,
                B01110,
                B00100};

/*
 *    EEPROM format
 *    Offset : Type  : Length : Remark
 *    0 :    : Byte  : 1      : Version
 *    1 :    : Byte  : 1      : Reset Info (byte) 255-Reset all values 
 *    2 :    : Float : 4      : tempMax
 *    6 :    : Float : 4      : tempMin
 */

// 
// 
#define ADDR_VERSION  0
#define ADDR_RESET    1
#define ADDR_TEMP_MAX 2
#define ADDR_TEMP_MIN 6

bool  useMax=false;
bool  useMin=false;
float tempMax=0.0;
float tempMin=0.0;

/*
 * byte floatTest[] = { 205, 204, 192, 65 }; 
 */

bool eprom_check_float(int addr)
{
    // currently only checks, if the field in the eprom is unequal 0 0 0 0
    int count;
    bool result = false;
    byte check;
    for ( count=0; count < sizeof(float); count++ ) {
      check = EEPROM.read(addr+count);
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
    for ( count=0; count < sizeof(float); count++ ) {
       help = EEPROM.read(addr+count);
       emap2float[count] = help;
      Serial.print(F("EE["));
      Serial.print(addr+count);
      Serial.print(F("]="));
      Serial.println(help);
       
    }
    return result;
}

void eprom_set_float(int addr, float value)
{   
    int count;
    byte * emap2float;
    emap2float = (byte *) &value;
    for ( count=0; count < sizeof(float); count++ ) {
      Serial.print(F("EE["));
      Serial.print(addr+count);
      Serial.print(F("]="));
      Serial.println(emap2float[count]);
      EEPROM.write(addr+count,emap2float[count]);
    }  
}

void eprom_reset_float(int addr)
{
    int count;
    for ( count=0; count < sizeof(float); count++ ) {
      EEPROM.write(addr+count,0);
    }  
}

void eprom_dump_flat(int addr, int len)
{
    int count;
    byte eeByte;
    for ( count=0; count < len; count++ ) {
      eeByte = EEPROM.read((addr+count));
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
    for ( count=0; count < sizeof(float); count++ ) {
      eeByte = emap2float[count];
      Serial.print(eeByte); Serial.print(F(" "));
    }
    Serial.println();  
}

void setup()
{
  Serial.begin(9600);
  Serial.print(F("DHT_LCD loop "));Serial.println(F(VERSION));
  // EEPROM.write(ADDR_VERSION,1); EEPROM.write(ADDR_RESET,255);
  dht.begin();

  
  lcd.init();                      // initialize the lcd
  // Print a message to the LCD.
  lcd.createChar(0, deg);
  lcd.createChar(1, max);
  lcd.createChar(2, min);
  lcd.backlight();   
  lcd.noBlink();

  if ( RTC.getYear() == 0 ) {
  RTC.setDate(10);
  RTC.setMonth(9);
  RTC.setYear(20);
  RTC.setHour(20);
  RTC.setMinute(22);
  RTC.setSecond(0);
  }
   
  
  eprom_dump_flat(0,16);
  if (true || eprom_check_float(ADDR_TEMP_MAX)){
    useMax=true;
    Serial.println(F("useMax=true"));
    tempMax=eprom_get_float(ADDR_TEMP_MAX);    // 1st field in epropm is tempMax
    Serial.print(F("tempMax from EEPROM: ")); Serial.println(tempMax);
  }
  if (true || eprom_check_float(ADDR_TEMP_MIN)){
    useMin=true;
    Serial.println(F("useMin=true"));
    tempMin=eprom_get_float(ADDR_TEMP_MIN);    // 2nd field in epprom is tempMin
    Serial.print(F("tempMax from EEPROM: ")); Serial.println(tempMin);
  }
  /*
  dump_float(12.0);
  dump_float(0.0);
  dump_float(22.3);
  dump_float(23.5);
  */
}

void loop()
{
   int runtime; 
   int seconds, minutes, hours, date, month, year;
   bool blinking = true;
   char bufferOut[20];
   char str_temp[20];
   /*
   if ( blinking ) {
       lcd.blink();
       blinking = false;
   } else {
       lcd.noBlink();
       blinking = true;    
   }
   */
   // TODO the DHT11 has some 
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true); 

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
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

/*
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
*/
  dtostrf(t, 3, 1, str_temp);
  sprintf(bufferOut, " %s", str_temp);

  /*
  Serial.println(bufferOut);
  */

  lcd.setCursor(0,0);
  lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  lcd.setCursor(10,0);
  lcd.print((int)h); lcd.print(F("%")); 
  
  dtostrf(hic, 3, 1, str_temp);
  sprintf(bufferOut, "(%s", str_temp);

  lcd.setCursor(0,1);
  lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C)"));

  dtostrf(tempMax, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10,1);
  lcd.print((char)1); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  dtostrf(tempMin, 3, 1, str_temp);
  sprintf(bufferOut, "%s", str_temp);
  lcd.setCursor(10,2);
  lcd.print((char)2); lcd.print((bufferOut)); lcd.print((char) 0); lcd.print(F("C"));

  runtime = millis()/ 1000;
   /*
   lcd.setCursor(0,2);
   lcd.print(runtime);
   */

  /*
   * Old code for millis
  
  hours = runtime / 3600;
  runtime = runtime - hours * 3600;

  minutes = runtime / 60;
  runtime = runtime - minutes * 60;

  seconds = runtime;
   */

  year = RTC.getYear();
  month = RTC.getMonth(Dummy);
  date = RTC.getDate();
  hours = RTC.getHour(Dummy, Dummy);
  minutes =  RTC.getMinute();
  seconds = RTC.getSecond();
   
  lcd.setCursor(0,3);
  sprintf(bufferOut, "%02i", date);   lcd.print(bufferOut); lcd.print(F("."));
  sprintf(bufferOut, "%02i", month);   lcd.print(bufferOut); lcd.print(F("."));
  sprintf(bufferOut, "%02i", year);   lcd.print(bufferOut); lcd.print(F(" "));
  
  sprintf(bufferOut, "%02i", hours);   lcd.print(bufferOut); lcd.print(F(":"));
  sprintf(bufferOut, "%02i", minutes); lcd.print(bufferOut); lcd.print(F(":"));
  sprintf(bufferOut, "%02i", seconds); lcd.print(bufferOut);
   
  lcd.backlight();
  delay(1000);
  //lcd.noBacklight();
  //delay(5000);
}
