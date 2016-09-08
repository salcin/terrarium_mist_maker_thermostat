#include <Arduino.h>

// rtc
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
// alarm
#include "Time.h"
#include "TimeAlarms.h"
// sensor temperature
#include <OneWire.h>
#include <DallasTemperature.h>

// PIN
const int TEMP_SENSOR_PIN = 2, TIP120_PIN = 3, RELAY_BRUM_PIN = 8, RELAY_HEAT_LAMP_PIN = 9; // , BUTTON_PIN = 0, RELAY_HEAT_MAT_PIN = 0;
float  temp_s1 = 'null', temp_s2 = 'null', temp_max_heat_lamp = 27, temp_max_heat_mat = 26;
int mist_during = 5;  // in min
boolean day_bo = true; 

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress temp_dev_s1 = { 0x28, 0xFF, 0x3A, 0xB4, 0x70, 0x16, 0x04, 0x26 };
DeviceAddress temp_dev_s2 = { 0x28, 0xFF, 0x4D, 0xB6, 0x70, 0x16, 0x04, 0x1D };

tmElements_t tm_now, tm_start_day, tm_end_day;
unsigned long lastTempRequest = millis(), delayInMillis = 750 / (1 << (12 - 9));

void setup() {
  Serial.begin(9600);

  bool parse=false;
  bool config=false;

  // get the date and time the compiler was run
  if (set_date(__DATE__) && set_time(__TIME__)) {
    parse = true;
    // and configure the RTC with this info
    if (RTC.write(tm_now)) {
      config = true;
    }
  }

  while (!Serial) ; // wait for Arduino Serial Monitor
  delay(200);

  if (parse && config) {
    Serial.println("DS1307 configured Time");
  }
  else if (parse) {
    Serial.println("DS1307 Communication Error :-{");
    Serial.println("Please check your circuitry");
  }
  else {
    Serial.print("Could not parse info from the compiler, Time=\"");
  }

  // Temp sensors
  sensors.begin();
  sensors.setResolution(temp_dev_s1, 9);
  sensors.setResolution(temp_dev_s2, 9);
  get_temp();

  setTime(tm_now.Hour,tm_now.Minute,tm_now.Second,tm_now.Day,tm_now.Month,tmYearToCalendar(tm_now.Year));    // syn rtc time with alarm

  // time slot day - night
  tm_start_day.Hour = 8;  tm_end_day.Hour = 22;
  // tm_start_day.Minute = 30; // tm_end_day.Minute = 30;

  pinMode(RELAY_BRUM_PIN, OUTPUT);
  pinMode(RELAY_HEAT_LAMP_PIN, OUTPUT);
  // pinMode(RELAY_HEAT_MAT_PIN, OUTPUT);

  // Relay values by defaults
  set_state_mist(false, false);
  digitalWrite(RELAY_HEAT_LAMP_PIN, 1);

  // !!! EDIT TIMEALARM.H FOR INCREASE THE MAX ALARMS AT 17 !!!
  Alarm.timerRepeat(60, check_temp);      // alarm check temperature every 1 mim
  for ( int h = tm_start_day.Hour; h <= tm_end_day.Hour; h+=2 ) {
    Alarm.alarmRepeat(h, 0, 0, do_mist);  // alarm mist maker every two hours
    // Alarm.alarmRepeat(h, tm_now.Minute, tm_now.Second+10, do_mist);
  }

  Serial.print("day : " );  Serial.print( day_bo );
  Serial.print("\nstart_day: ");  Serial.print(tm_start_day.Hour);  Serial.print(":"); Serial.print(tm_start_day.Minute);
  Serial.print("\nend_day: "); Serial.print(tm_end_day.Hour); Serial.print(":");Serial.print(tm_end_day.Minute);
  Serial.print("\ns1 temp: "); Serial.print(temp_s1); Serial.print(" / "); Serial.print(temp_max_heat_lamp);
} // end setup

void loop() {

  RTC.read(tm_now);
   // get_time(true);

  Alarm.delay(1000);
} // end loop

//
// - [ MIST FCT ] -
//
// set high or low fan and mist maker
void set_state_mist(boolean brum, boolean fan) {

  int brum_v, fan_v;

  ( brum ) ? brum_v=0 : brum_v=1 ;
  ( fan ) ? fan_v=1 : fan_v=0 ;

  get_time(true);
  digitalWrite(RELAY_BRUM_PIN, brum_v);    // ON: LOW or 0  | OFF: HIGH or 1
  digitalWrite(TIP120_PIN, fan_v);      // ON: HIGH or 1 | OFF: LOW or 0
}

void do_mist() {
  int bonus_time = 0;

  Serial.print("\n Do MIST: ");
  get_time(true);

  if ( tm_now.Hour >= 20 )
    int bonus_time = 2;

  set_state_mist(true, true);
  Alarm.timerOnce( (bonus_time + mist_during) * 60, stop_mist);
}

void stop_mist() {
  Serial.print("\n Stop MIST: ");
  set_state_mist(false, false);

  if ( tm_now.Hour == 8 )
    day_bo = true;
  if ( tm_now.Hour >= 20 ) {
    day_bo = false;
    digitalWrite(RELAY_HEAT_LAMP_PIN, 1);
  }

  Serial.print("\nday : ");    Serial.print("day_bo");
}


//
// - [ TEMP FCT ] -
//
void check_temp() {

  if( day_bo ) {
    get_temp();
    Serial.print("\ncheck_temp : ");    Serial.print(temp_s1);        Serial.print(" / ");        Serial.print(temp_max_heat_lamp);

    set_state_relay(RELAY_HEAT_LAMP_PIN, temp_s1, temp_max_heat_lamp);              // lamp
    // set_state_relay( RELAY_HEAT_MAT_PIN, temp_s2, temp_max_heat_mat);            // mat
  }
}

void set_state_relay(int pin, float temp, float temp_max) {
  int state = 'null';

  ( temp < temp_max ) ? state=0 : state=1 ;
  digitalWrite(pin, state);
}

void get_temp() {
    sensors.requestTemperatures();

    temp_s1 = sensors.getTempC(temp_dev_s1);
    temp_s2 = sensors.getTempC(temp_dev_s2);
}


//
// - [ TIME FCT ] -
//
//boolean is_day() {
//
//  if ( tm_now.Hour > tm_end_day.Hour || tm_now.Hour < tm_start_day.Hour )     // 22 > 21 && 7 < 8 -> night
//    day_bo = false;
//
//  else {
//    if ( tm_now.Hour < tm_end_day.Hour && tm_now.Hour > tm_start_day.Hour )   // 20 < 21 && 7 > 8 -> day
//      day_bo = true;
//
//    else {
//      if ( tm_now.Hour == tm_end_day.Hour )         // 21h = 21h
//        ( tm_now.Minute >= tm_end_day.Minute ) ? day_bo = false : day_bo = true;
//
//      else if ( tm_now.Hour == tm_start_day.Hour )  // 8h = 8h
//        ( tm_now.Minute >= tm_end_day.Minute ) ? day_bo = true : day_bo = false;
//    }
//  }
//
//  return day_bo;
//} // end fct

void get_time(boolean h_only) {

  //Serial.println();
  print2digits(tm_now.Hour);
  Serial.print(":");
  print2digits(tm_now.Minute);
  Serial.print(":");
  print2digits(tm_now.Second);

  if ( h_only == false ) {
    Serial.print(" ");
    Serial.print(tm_now.Day);
    Serial.print("/");
    Serial.print(tm_now.Month);
    Serial.print("/");
    Serial.print(tmYearToCalendar(tm_now.Year));
  }
  Serial.println("");
}

// 8 -> 08
void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

bool set_time(const char *str) {
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm_now.Hour = Hour;
  tm_now.Minute = Min;
  tm_now.Second = Sec;
  return true;
}

bool set_date(const char *str) {
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  if (monthIndex >= 12) return false;
  tm_now.Day = Day;
  tm_now.Month = monthIndex + 1;
  tm_now.Year = CalendarYrToTm(Year);
  return true;
}
