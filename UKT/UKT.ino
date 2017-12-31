/*##############################################################################

	Name	: Ultimativer Küchen Timer
	Version	: 
	autor	: Hans-Joachim Beck, der_ukt@web.de
	License	: GNU General Public License 
  
//############################################################################*/#include <LiquidCrystal.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <Adafruit_NeoPixel.h>
#include "RTClib.h"
#include <avr/wdt.h>

Adafruit_AlphaNum4 Alpha4 = Adafruit_AlphaNum4();
char alpha_buffer[4];

#define PIN 4

Adafruit_7segment Clock = Adafruit_7segment();
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);
#define SecPtr_Color 100, 0, 0
#define CountDownSecPtr_Color 140, 50, 0
#define CountDown_Color 64, 64, 0
#define Mark_5_Color 0, 20, 0
#define Mark_15_Color 0, 10, 10
#define Mark_60_Color 0, 50, 50


int encoder, light;
byte keys;
#define EncoderKey 0x01
#define StartKey 0x02
#define StopKey 0x04
byte mysec, mymin, myhour;
int Brightness;

byte mode, alarmmode;

int count_down_time;
long count_up_time;
#define max_countdowntime_sec  28740  // in Sekunden, das sind dann 7h 59Min

//RTC_DS1307 RTC;
DateTime mynow;
RTC_DS1307 RTC;
#define _1307_pos_second 0
#define _1307_pos_minute 1
#define _1307_pos_hour 2
#define _1307_pos_dayofweek 3
#define _1307_pos_day 4
#define _1307_pos_month 5
#define _1307_pos_year 6

//#define DS1307_ADDRESS 0x1f

int DS1307_pos;
byte old_sec, old_date;
unsigned int mytimer;
unsigned int modechange;
#define modefallback 30

static uint8_t bcd2bin (uint8_t val) {
  return val - 6 * (val >> 4);
}
static uint8_t bin2bcd (uint8_t val) {
  return val + 6 * (val / 10);
}


//DCF data
int DCF_pin = 2;
volatile unsigned int DCF_oldtime, DCF_result, DCF_diff, DCF_ontime;
byte pinchanged;
int DCF_SEC_COUNT;
byte dcf_data, DCF_MUSTER;
int paritycount;

typedef struct
{
  byte _day;
  byte _month;
  byte _year;
  byte _dayofweek;
  byte _min;
  byte _hour;
  boolean _error;
}  DCF_DATA;

boolean dcf_data_validate(DCF_DATA *_old, DCF_DATA *_new);
void copy_dcf_data(DCF_DATA *in, DCF_DATA *out);

DCF_DATA mycurtime, myprevtime;
boolean dcf_not_syncronized;



void DS1307_setTime(int what, int value)
{
  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)what);
  Wire.write(value);
  Wire.endTransmission();
}

void changed()
{
  if (digitalRead(DCF_pin) == false)
  {
    DCF_ontime = millis() - DCF_oldtime;
    DCF_oldtime = millis();
  }
  else
  {
    DCF_result = millis() - DCF_oldtime;
    if (DCF_result > 50)
    {
      pinchanged = 1;
      DCF_oldtime = millis();
    }
  }
}


void display_date()
{
  if (dcf_not_syncronized)
  {

    alpha_buffer[0] = 'D';
    alpha_buffer[1] = 'C';
    alpha_buffer[2] = 'F';
    alpha_buffer[3] = ' ';
    Alpha4.writeDigitAscii(0, alpha_buffer[0]);
    Alpha4.writeDigitAscii(1, alpha_buffer[1]);
    Alpha4.writeDigitAscii(2, alpha_buffer[2]);
    Alpha4.writeDigitAscii(3, alpha_buffer[3]);
    Alpha4.setBrightness(Brightness-2);
    Alpha4.writeDisplay();
  }
  else
  {
    Alpha4.writeDigitAscii(0, (mynow.day()) / 10 + 0x30);
    Alpha4.writeDigitAscii(1, (mynow.day()) % 10 + 0x30, true);
    Alpha4.writeDigitAscii(2, mynow.month() / 10 + 0x30);
    Alpha4.writeDigitAscii(3, mynow.month() % 10 + 0x30);
    Alpha4.setBrightness(Brightness-2);
    Alpha4.writeDisplay();
  }
}

void copy_dcf_data(DCF_DATA *in, DCF_DATA *out)
{
  out->_day = in->_day;
  out->_month = in->_month;
  out->_year = in->_year;
  out->_dayofweek = in->_dayofweek;
  out->_min = in->_min;
  out->_hour = in->_hour;
  out->_error = in->_error;
}


boolean dcf_data_validate(DCF_DATA *_old, DCF_DATA *_new)
{
  if (_old->_error || _new->_error)
  {
    return false;
  }
  if (_old->_year != _new->_year)
  {
    return false;
  }
  if (_old->_month != _new->_month)
  {
    return false;
  }
  if (_old->_dayofweek != _new->_dayofweek)
  {
    return false;
  }
  if (_old->_hour != _new->_hour)
  {
    return false;
  }
  if ((_old->_min + 1) != _new->_min)
  {
    return false;
  }
  return true;
}

void print_dcf_time(DCF_DATA *in);
void print_dcf_time(DCF_DATA *in)
{
  Serial.print(in->_error); Serial.print("::"); Serial.print(in->_day); Serial.print("."); Serial.print(in->_month); Serial.print("."); Serial.print(in->_year); Serial.print("  "); Serial.print(in->_dayofweek); Serial.print("  "); Serial.print(in->_hour); Serial.print(":"); Serial.println(in->_min);
}


void reset_dcf_data()
{
  dcf_data = 0;
  DCF_MUSTER = 1;
}

void decode_dcf()
{
  if (pinchanged != 0)
  {
    //Serial.print("..");
    Serial.print(DCF_SEC_COUNT); Serial.print(" "); Serial.print(DCF_result); Serial.print(" "); Serial.println(DCF_ontime);
    if (DCF_result > 170)
    {
      //Serial.print("     1  :");
      dcf_data |= DCF_MUSTER;
      paritycount++;
    }
    else
    {
      //Serial.print("     0  :");
    }
    //Serial.println(dcf_data, HEX);

    DCF_MUSTER = DCF_MUSTER << 1;

    pinchanged = 0;


    switch (DCF_SEC_COUNT)
    {
      case 20:
        //Serial.println("MinEiner");
        paritycount = 0;
        reset_dcf_data();
        break;
      case 24:
        mycurtime._min = dcf_data;
        //Serial.println("MinZehner");
        reset_dcf_data();

        break;
      case 27:
        mycurtime._min += 10 * dcf_data;
        //Serial.println("Min Parity");
        reset_dcf_data();
        break;
      case 28:
        if ((paritycount % 2) != 0)
          mycurtime._error = true;
        paritycount = 0;
        //Serial.println("StdEiner");
        reset_dcf_data();
        break;
      case 32:
        mycurtime._hour = dcf_data;
        //Serial.println("StdZehner");
        reset_dcf_data();
        break;
      case 34:
        mycurtime._hour += 10 * dcf_data;
        //Serial.println("Std Parity");
        reset_dcf_data();
        break;
      case 35:
        if ((paritycount % 2) != 0)
          mycurtime._error = true;
        paritycount = 0;
        //Serial.println("TagEiner");
        reset_dcf_data();
        break;
      case 39:
        mycurtime._day = dcf_data;
        //Serial.println("TagZehner");
        reset_dcf_data();
        break;
      case 41:
        mycurtime._day += 10 * dcf_data;
        //Serial.println("WoTag");
        reset_dcf_data();
        break;
      case 44:
        //Serial.println("MonEiner");
        mycurtime._dayofweek = dcf_data;
        reset_dcf_data();
        break;
      case 48:
        mycurtime._month = dcf_data;
        //Serial.println("MonZehner");
        reset_dcf_data();
        break;
      case 49:
        mycurtime._month += 10 * dcf_data;
        //Serial.println("JahrEiner");
        reset_dcf_data();
        break;
      case 53:
        mycurtime._year = dcf_data;
        //Serial.println("JahrZehner");
        reset_dcf_data();
        break;
      case 57:
        mycurtime._year += 10 * dcf_data;
        //Serial.println("Jahr Parity");
        reset_dcf_data();
        break;
      case 58:
        //Serial.println("Jahr Parity");
        if ((paritycount % 2) != 0)
          mycurtime._error = true;
        //Serial.print(mycurtime._error); Serial.print("::"); Serial.print(mycurtime._day); Serial.print("."); Serial.print(mycurtime._month); Serial.print("."); Serial.print(mycurtime._year); Serial.print("  "); Serial.print(mycurtime._dayofweek); Serial.print("  "); Serial.print(mycurtime._hour); Serial.print(":"); Serial.println(mycurtime._min);
        break;

    }

    if (DCF_ontime > 1500)
    {
      DCF_SEC_COUNT = 0;
      //Serial.print(mycurtime._error); Serial.print("::"); Serial.print(mycurtime._day); Serial.print("."); Serial.print(mycurtime._month); Serial.print("."); Serial.print(mycurtime._year); Serial.print("  "); Serial.print(mycurtime._dayofweek); Serial.print("  "); Serial.print(mycurtime._hour); Serial.print(":"); Serial.println(mycurtime._min);
      print_dcf_time(&mycurtime);
      if (dcf_data_validate(&myprevtime, &mycurtime))
      {
        Serial.println("Validate true");
        DS1307_setTime(_1307_pos_second, 0);
        DS1307_setTime(_1307_pos_minute, bin2bcd(mycurtime._min));
        DS1307_setTime(_1307_pos_hour, bin2bcd(mycurtime._hour));
        DS1307_setTime(_1307_pos_dayofweek, bin2bcd(mycurtime._dayofweek));
        DS1307_setTime(_1307_pos_day, bin2bcd(mycurtime._day));
        DS1307_setTime(_1307_pos_month, bin2bcd(mycurtime._month));
        DS1307_setTime(_1307_pos_year, bin2bcd(mycurtime._year));
        dcf_not_syncronized = false;
        display_date();
      }
      else
        Serial.println("Validate false");
      copy_dcf_data(&mycurtime, &myprevtime);
      mycurtime._error = false;
    }
    DCF_SEC_COUNT++;
  }
}


void ClearStrip(void)
{
  for (byte i = 0; i < 60; i++)
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  //if (!dcf_not_syncronized)
  strip.show();
}


void ReadKeypad()
{
  Wire.beginTransmission(0x34 / 2);
  Wire.write(byte(0x00));            // sends instruction byte
  Wire.endTransmission();     // stop transmitting

  Wire.requestFrom(0x34 / 2, 4);

  keys = Wire.read();
  encoder = (int)Wire.read();
  if (encoder > 100)
    encoder |= 0xff00;
  light = Wire.read() * 256 + Wire.read();       // print the character
}

void display_time(void)
{
  ClearStrip();
  mark5minstripe();

  if (dcf_not_syncronized)
    strip.setPixelColor(DCF_SEC_COUNT, strip.Color(SecPtr_Color));
  else
    strip.setPixelColor(mynow.second(), strip.Color(SecPtr_Color));

  //if (!dcf_not_syncronized)
  strip.show();

  Clock.writeDigitNum(0, (mynow.hour() / 10), false);
  Clock.writeDigitNum(1, (mynow.hour() % 10), false);
  Clock.drawColon(true);
  Clock.writeDigitNum(3, (mynow.minute() / 10) % 10, false);
  Clock.writeDigitNum(4, (mynow.minute() % 10), false);
  Clock.setBrightness(Brightness);
  Clock.writeDisplay();
}

void display_count_up_time(void)
{
  ClearStrip();
  mark5minstripe();
  strip.setPixelColor(count_up_time % 60, strip.Color(SecPtr_Color));
  //if (!dcf_not_syncronized)
  strip.show();
  long help;
  int help1;
  help = count_up_time;
  help1 = count_up_time / 36000l;
  Clock.writeDigitNum(0, help1, false);
  help -= help1 * 36000l;
  help1 = help / 3600l;
  Clock.writeDigitNum(1, help1, false);
  help -= help1 * 3600l;
  help1 = help / 600;
  Clock.drawColon(true);
  Clock.writeDigitNum(3, help1, false);
  help -= help1 * 600;
  help1 = help / 60;
  Clock.writeDigitNum(4, help1, false);
  Clock.setBrightness(Brightness);
  Clock.writeDisplay();
}

void mark5minstripe(void)
{
  strip.setPixelColor(5, strip.Color(Mark_5_Color));
  strip.setPixelColor(10, strip.Color(Mark_5_Color));
  strip.setPixelColor(20, strip.Color(Mark_5_Color));
  strip.setPixelColor(25, strip.Color(Mark_5_Color));
  strip.setPixelColor(35, strip.Color(Mark_5_Color));
  strip.setPixelColor(40, strip.Color(Mark_5_Color));
  strip.setPixelColor(50, strip.Color(Mark_5_Color));
  strip.setPixelColor(55, strip.Color(Mark_5_Color));
  strip.setPixelColor(0, strip.Color(Mark_60_Color));
  strip.setPixelColor(15, strip.Color(Mark_15_Color));
  strip.setPixelColor(30, strip.Color(Mark_15_Color));
  strip.setPixelColor(45, strip.Color(Mark_15_Color));

}
void setup() {

  pinMode(A0, INPUT);
  Serial.begin(115200);

  pinMode(DCF_pin, INPUT);
  digitalWrite(DCF_pin, 1);

  DCF_oldtime = 0;
  pinchanged = 0;
  DCF_SEC_COUNT = 0;
  attachInterrupt(digitalPinToInterrupt(DCF_pin), changed, CHANGE);
  dcf_data = 0;
  mycurtime._error = true;
  dcf_not_syncronized = true;
  Wire.begin();
  RTC.begin();
  dcf_data = 0;
  mycurtime._error = true;
  dcf_not_syncronized = true;
        
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    dcf_not_syncronized = true;
  }

  old_sec = 0;
  old_date = 0;
  Clock.begin(0x71);
  Alpha4.begin(0x70);
  Brightness = 5;
  Clock.setBrightness(Brightness);
  Alpha4.setBrightness(Brightness-2);
  strip.begin();
  strip.setBrightness(50);
  //if (!dcf_not_syncronized) if (!dcf_not_syncronized)
  strip.show(); // Initialize all pixels to 'off'
  display_time();
  alpha_buffer[0] = 'V';
  alpha_buffer[1] = '1';
  alpha_buffer[2] = '.';
  alpha_buffer[3] = '1';
  Alpha4.writeDigitAscii(0, alpha_buffer[0]);
  Alpha4.writeDigitAscii(1, alpha_buffer[1]);
  Alpha4.writeDigitAscii(2, alpha_buffer[2]);
  Alpha4.writeDigitAscii(3, alpha_buffer[3]);
  Alpha4.setBrightness(Brightness-2);
  Alpha4.writeDisplay();
  delay(2000); 
  mode = 0;
  mynow = RTC.now();
  count_down_time = 30;
  wdt_enable(WDTO_2S); // stellt die Watchdog auf 2s
  pinMode(13, OUTPUT);
  TWBR = 120;
}

void end_melody(void)
{

  Wire.beginTransmission(0x36 / 2);
  Wire.write(0x00);            // sends instruction byte
  Wire.write(0x03);            // sends instruction byte
  Wire.endTransmission();
}
void melody_stop(void)
{

  Wire.beginTransmission(0x36 / 2);
  Wire.write(0x00);            // sends instruction byte
  Wire.write(0x00);            // sends instruction byte
  Wire.endTransmission();
}

void nerv_1_melody(void)
{

  Wire.beginTransmission(0x36 / 2);
  Wire.write(0x00);            // sends instruction byte
  Wire.write(0x01);            // sends instruction byte
  Wire.endTransmission();
}
void nerv_2_melody(void)
{

  Wire.beginTransmission(0x36 / 2);
  Wire.write(0x00);            // sends instruction byte
  Wire.write(0x02);            // sends instruction byte
  Wire.endTransmission();
}

void nerv_3_melody(void)
{

  Wire.beginTransmission(0x36 / 2);
  Wire.write(0x00);            // sends instruction byte
  Wire.write(0x04);            // sends instruction byte
  Wire.endTransmission();
}

void display_countdown_time(void)
{
  long help, help1;
  byte shelp;
  ClearStrip();
  if (help == 0)
    strip.setPixelColor(0, strip.Color(CountDown_Color));
  help = count_down_time;
  if (mode == 1)
  {
    help /= 60;

  }
  help %= 60;
  for (byte i = 0; i < help; i++)
  {
    strip.setPixelColor(i, strip.Color(CountDown_Color));
  }
  mark5minstripe();
  strip.setPixelColor(help, strip.Color(CountDownSecPtr_Color));

  //if (!dcf_not_syncronized) if (!dcf_not_syncronized)
  strip.show();

  help = count_down_time;
  help1 = count_down_time / 36000l;
  Clock.writeDigitNum(0, help1, false);
  help -= help1 * 36000l;
  help1 = help / 3600;
  Clock.writeDigitNum(1, help1, false);
  help -= help1 * 3600;
  help1 = help / 600;
  Clock.drawColon(true);
  Clock.writeDigitNum(3, help1, false);
  help -= help1 * 600;
  help1 = help / 60;
  Clock.writeDigitNum(4, help1, false);
  Clock.setBrightness(Brightness);
  Clock.writeDisplay();
}

void run_countdown(void)
{
  int help, help1;

  ClearStrip();
  mark5minstripe();
  strip.setPixelColor(abs(count_down_time) % 60, strip.Color(SecPtr_Color));
  //if (!dcf_not_syncronized) if (!dcf_not_syncronized)
  strip.show();
  help = abs(count_down_time);
  help1 = abs(count_down_time) / 36000;
  if (count_down_time < 0)
    Clock.blinkRate(1);
  else
    Clock.blinkRate(0);
  Clock.writeDigitNum(0, help1, false);
  help -= help1 * 36000;
  help1 = help / 3600;
  Clock.writeDigitNum(1, help1, false);
  help -= help1 * 3600;
  help1 = help / 600;
  Clock.drawColon(true);
  Clock.writeDigitNum(3, help1, false);
  help -= help1 * 600;
  help1 = help / 60;
  Clock.writeDigitNum(4, help1, false);
  Clock.setBrightness(Brightness);
  Clock.writeDisplay();
}




unsigned long mytime;

void loop() {

  if ((millis() - mytime) > 100)
  {
    ReadKeypad();
    mytime = millis();
  }



  switch (mode)
  {
    case 0:   // Uhrzeit
      if ((keys & StartKey) != 0)
      {
        mode = 1;
        modechange = mytimer;
        ClearStrip();
        count_down_time = 30 * 60;
        display_countdown_time();

        alpha_buffer[0] = 'D';
        alpha_buffer[1] = 'o';
        alpha_buffer[2] = 'w';
        alpha_buffer[3] = 'n';
        Alpha4.writeDigitAscii(0, alpha_buffer[0]);
        Alpha4.writeDigitAscii(1, alpha_buffer[1]);
        Alpha4.writeDigitAscii(2, alpha_buffer[2]);
        Alpha4.writeDigitAscii(3, alpha_buffer[3]);
        Alpha4.setBrightness(Brightness-2);
        Alpha4.writeDisplay();
      }
      if ((keys & EncoderKey) != 0)
      {
        mode = 4;
        modechange = mytimer;
        ClearStrip();
        count_down_time = 330;
        display_countdown_time();

        alpha_buffer[0] = 'S';
        alpha_buffer[1] = 'h';
        alpha_buffer[2] = 'r';
        alpha_buffer[3] = 't';
        Alpha4.writeDigitAscii(0, alpha_buffer[0]);
        Alpha4.writeDigitAscii(1, alpha_buffer[1]);
        Alpha4.writeDigitAscii(2, alpha_buffer[2]);
        Alpha4.writeDigitAscii(3, alpha_buffer[3]);
        Alpha4.setBrightness(Brightness-2);
        Alpha4.writeDisplay();
      }

      break;
    case 1:   //Anwahl Rückwärtszähler
      if ((keys & StopKey) != 0)
      {
        mode = 0;
        ClearStrip();
        display_date();
      }
      if (encoder != 0)
      {
        count_down_time += (encoder * 60);
        if (count_down_time > max_countdowntime_sec)
          count_down_time = max_countdowntime_sec;
        if (count_down_time < 1)
          count_down_time = 60;
        display_countdown_time();
        modechange = mytimer;
      }
      if ((keys & StartKey) != 0)
      {
        mode = 2;
        alarmmode = 0;
        //count_down_time_sec = count_down_time;
      }

      break;
    case 2:   //Rückwärtszähler läuft
      if ((keys & StopKey) != 0)
      {
        mode = 1;
        display_countdown_time();
        modechange = mytimer;
      }
      else
      {
        if ((count_down_time < 0) && (alarmmode == 0))
        {
          end_melody();
          mode = 3;
          alarmmode = 1;
        }
      }
      break;
    case 3:   // Alarm
      if ((keys & StopKey) != 0)
      {
        mode = 1;
        display_countdown_time();
        Clock.blinkRate(0);
        melody_stop();
      }
      else
      {
        if (count_down_time < -59) // Da muss man was tun, um den Alarm zu wiederholen
        {
          if ((count_down_time % 60) == 0)
            nerv_3_melody();
          else
          {
            if ((count_down_time % 10) == 0)
              nerv_1_melody();
          }
        }
      }
      break;
    case 4:   //Anwahl short down timer
      if ((keys & StopKey) != 0)
      {
        mode = 0;
        ClearStrip();
        display_date();
      }
      else if (encoder != 0)
      {
        count_down_time += encoder;
        if (count_down_time > 3599)
          count_down_time = 3599;
        if (count_down_time < 1)
          count_down_time = 1;
        display_countdown_time();
        modechange = mytimer;
      }
      if ((keys & StartKey) != 0)
      {
        mode = 5;
        alarmmode = 0;
        //count_down_time_sec = count_down_time;
      }
      if ((keys & EncoderKey) != 0)
      {
        mode = 7;
        modechange = mytimer;
        ClearStrip();
        count_up_time = 0;
        display_count_up_time();
        alpha_buffer[0] = 'S';
        alpha_buffer[1] = 't';
        alpha_buffer[2] = 'o';
        alpha_buffer[3] = 'p';
        Alpha4.writeDigitAscii(0, alpha_buffer[0]);
        Alpha4.writeDigitAscii(1, alpha_buffer[1]);
        Alpha4.writeDigitAscii(2, alpha_buffer[2]);
        Alpha4.writeDigitAscii(3, alpha_buffer[3]);
        Alpha4.setBrightness(Brightness-2);
        Alpha4.writeDisplay();
      }

      break;
    case 5:   //Rückwärtszähler short läuft
      if ((keys & StopKey) != 0)
      {
        mode = 4;
        display_countdown_time();
        modechange = mytimer;
      }
      else
      {
        if ((count_down_time < 0) && (alarmmode == 0))
        {
          end_melody();
          mode = 6;
          alarmmode = 1;
        }
      }
      break;
    case 6:   // Alarm short timer
      if ((keys & StopKey) != 0)
      {
        mode = 4;
        display_countdown_time();
        Clock.blinkRate(0);
        melody_stop();
      }
      else
      {
        if (count_down_time == 0) // Da muss man was tun, um den Alarm zu wiederholen
        {
          end_melody();
          mode = 6;
          alarmmode = 0;
        }
        if (count_down_time < -20) // Da muss man was tun, um den Alarm zu wiederholen
        {
          if ((count_down_time % 10) == 0)
            nerv_1_melody();
        }
      }
      break;
    case 7:   // Stoppuhr
      if ((keys & StartKey) != 0)
      {
        mode = 8;
        count_up_time = 0;
      }
      if ((keys & StopKey) != 0)
      {
        mode = 0;
        ClearStrip();
        display_date();
      }
      break;
    case 8:   // Stoppuhr
      if ((keys & StopKey) != 0)
      {
        mode = 7;
        modechange = mytimer;
      }
      if (count_up_time > 356400l)
      {
        mode = 0;
      }
      break;
  }


  //if (dcf_not_syncronized == false)
  mynow = RTC.now();

  if (old_sec != mynow.second())
  {
    old_sec = mynow.second();
    if (!dcf_not_syncronized)
      digitalWrite(13, !digitalRead(13));
    mytimer++;
    wdt_reset();
    if (mode == 0)
      display_time();
    if ((mode == 2) || (mode == 3) || (mode == 5) || (mode == 6))
    {
      count_down_time--;
      run_countdown();
    }
    if ((mode == 7) || (mode == 8))
    {
      if (mode == 8)
        count_up_time++;
      display_count_up_time();
    }
    if ((old_sec % 15) == 0)
    {
      if (light > 700)
      {
        strip.setBrightness(30);
        Brightness = 2;
      }
      if ((light > 400) && (light <= 700))
      {
        strip.setBrightness(40);
        Brightness = 5;
      }
      if ((light > 200) && (light <= 400))
      {
        strip.setBrightness(60);
        Brightness = 9;
      }
      if (light <= 200)
      {
        strip.setBrightness(80);
        Brightness = 15;
      }
      if (mode == 0)
        display_date();
      Serial.print("Brightness: ");Serial.println(Brightness);
    }
  }

  if ((old_date != mynow.day()) and (mode == 0))
  {
    display_date();
    old_date = mynow.day();
  }
  encoder = 0;

  if ((mode == 1) || (mode == 4) || (mode == 7))
  {
    if ((mytimer - modechange) > modefallback)
    {
      mode = 0;
      display_date();
    }
  }
  if (dcf_not_syncronized)
  {
    decode_dcf();
    if ((keys & StopKey) != 0)
    {
      dcf_not_syncronized = false;
      mode = 0;
      display_date();
    }
  }
  else
  {
    if ((mynow.hour() == 3) && (mynow.minute() == 0))
    {
      dcf_not_syncronized = true;
      mycurtime._error = true;
      display_date();
    }
  }

  if (keys != 0)
  {
    Serial.print("keys:");
    Serial.print(keys);
    Serial.print("  Mode:");
    Serial.println(mode);
  }
  keys = 0;

}


