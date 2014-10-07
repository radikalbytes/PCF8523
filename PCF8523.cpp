/*          PCF8523 Arduino library
    Copyright (C) Alfredo Prado ,2014
    radikalbytes [at] gmail [dot] com


  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include <Wire.h>
#include "pcf8523.h"

#ifdef __AVR__
 #include <avr/pgmspace.h>
 #define WIRE Wire
#else
 #define PROGMEM
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
 #define WIRE Wire1
#endif


#define SECONDS_PER_DAY 86400L

#define SECONDS_FROM_1970_TO_2000 946684800

#if (ARDUINO >= 100)
 #include <Arduino.h> // capital A so it is error prone on case-sensitive filesystems
 // Macro to deal with the difference in I2C write functions from old and new Arduino versions.
 #define _I2C_WRITE write
 #define _I2C_READ  read
#else
 #include <WProgram.h>
 #define _I2C_WRITE send
 #define _I2C_READ  receive
#endif


////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const uint8_t daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += pgm_read_byte(daysInMonth + i - 1);
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
}

DateTime::DateTime (const DateTime& copy):
  yOff(copy.yOff),
  m(copy.m),
  d(copy.d),
  hh(copy.hh),
  mm(copy.mm),
  ss(copy.ss)
{}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using F() would further reduce the RAM footprint, see below.
DateTime::DateTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
    switch (date[0]) {
        case 'J': m = date[1] == 'a' ? 1 : m = date[2] == 'n' ? 6 : 7; break;
        case 'F': m = 2; break;
        case 'A': m = date[2] == 'r' ? 4 : 8; break;
        case 'M': m = date[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
}

// A convenient constructor for using "the compiler's time":
// This version will save RAM by using PROGMEM to store it by using the F macro.
//   DateTime now (F(__DATE__), F(__TIME__));
DateTime::DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    char buff[11];
    memcpy_P(buff, date, 11);
    yOff = conv2d(buff + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (buff[0]) {
        case 'J': m = buff[1] == 'a' ? 1 : m = buff[2] == 'n' ? 6 : 7; break;
        case 'F': m = 2; break;
        case 'A': m = buff[2] == 'r' ? 4 : 8; break;
        case 'M': m = buff[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(buff + 4);
    memcpy_P(buff, time, 8);
    hh = conv2d(buff);
    mm = conv2d(buff + 3);
    ss = conv2d(buff + 6);
}

uint8_t DateTime::dayOfWeek() const {    
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000;  // seconds from 1970 to 2000

  return t;
}

long DateTime::secondstime(void) const {
  long t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  return t;
}

DateTime DateTime::operator+(const TimeSpan& span) {
  return DateTime(unixtime()+span.totalseconds());
}

DateTime DateTime::operator-(const TimeSpan& span) {
  return DateTime(unixtime()-span.totalseconds());
}

TimeSpan DateTime::operator-(const DateTime& right) {
  return TimeSpan(unixtime()-right.unixtime());
}

////////////////////////////////////////////////////////////////////////////////
// TimeSpan implementation

TimeSpan::TimeSpan (int32_t seconds):
  _seconds(seconds)
{}

TimeSpan::TimeSpan (int16_t days, int8_t hours, int8_t minutes, int8_t seconds):
  _seconds(days*86400L + hours*3600 + minutes*60 + seconds)
{}

TimeSpan::TimeSpan (const TimeSpan& copy):
  _seconds(copy._seconds)
{}

TimeSpan TimeSpan::operator+(const TimeSpan& right) {
  return TimeSpan(_seconds+right._seconds);
}

TimeSpan TimeSpan::operator-(const TimeSpan& right) {
  return TimeSpan(_seconds-right._seconds);
}



////////////////////////////////////////////////////////////////////////////////
// PCF8523 implementation

static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

uint8_t PCF8523::begin(void) {
  return 1;
}

// Example: bool a = PCF8523.isrunning();
// Returns 1 if RTC is running and 0 it's not 
uint8_t PCF8523::isrunning(void) {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(0);
  WIRE.endTransmission();

  WIRE.requestFrom(PCF8523_ADDRESS, 1);
  uint8_t ss = WIRE._I2C_READ();
  ss = ss & 32;
  return !(ss>>5);
}

// Example: PCF8523.adjust (DateTime(2014, 8, 14, 1, 49, 0))
// Sets RTC time to 2014/14/8 1:49 a.m.
void PCF8523::adjust(const DateTime& dt) {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(0x03);
  WIRE._I2C_WRITE(bin2bcd(dt.second()));
  WIRE._I2C_WRITE(bin2bcd(dt.minute()));
  WIRE._I2C_WRITE(bin2bcd(dt.hour()));
  WIRE._I2C_WRITE(bin2bcd(dt.day()));
  WIRE._I2C_WRITE(bin2bcd(0));
  WIRE._I2C_WRITE(bin2bcd(dt.month()));
  WIRE._I2C_WRITE(bin2bcd(dt.year() - 2000));
  WIRE._I2C_WRITE(0);
  WIRE.endTransmission();
}

// Example: DateTime now = PCF8523.now();
// Returns Date and time in RTC in:
// year = now.year()
// month = now.month()
// day = now.day()
// hour = now.hour()
// minute = now.minute()
// second = now.second()
DateTime PCF8523::now() {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(3);   
  WIRE.endTransmission();

  WIRE.requestFrom(PCF8523_ADDRESS, 7);
  uint8_t ss = bcd2bin(WIRE._I2C_READ() & 0x7F);
  uint8_t mm = bcd2bin(WIRE._I2C_READ());
  uint8_t hh = bcd2bin(WIRE._I2C_READ());
  uint8_t d = bcd2bin(WIRE._I2C_READ());
  WIRE._I2C_READ();
  uint8_t m = bcd2bin(WIRE._I2C_READ());
  uint16_t y = bcd2bin(WIRE._I2C_READ()) + 2000;
  
  return DateTime (y, m, d, hh, mm, ss);
}


// Example: PCF8523.read_reg(buf,size,address);
// Returns:   buf[0] = &address
//            buf[1] = &address + 1
//      ..... buf[size-1] = &address + size
void PCF8523::read_reg(uint8_t* buf, uint8_t size, uint8_t address) {
  int addrByte = address;
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(addrByte);
  WIRE.endTransmission();
  
  WIRE.requestFrom((uint8_t) PCF8523_ADDRESS, size);
  for (uint8_t pos = 0; pos < size; ++pos) {
    buf[pos] = WIRE._I2C_READ();
  }
}

// Example: PCF8523.write_reg(address,buf,size);
// Write:     buf[0] => &address
//            buf[1] => &address + 1
//      ..... buf[size-1] => &address + size
void PCF8523::write_reg(uint8_t address, uint8_t* buf, uint8_t size) {
  int addrByte = address;
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(addrByte);
  for (uint8_t pos = 0; pos < size; ++pos) {
    WIRE._I2C_WRITE(buf[pos]);
  }
  WIRE.endTransmission();
}

// Example: val = PCF8523.read_reg(0x08);
// Reads the value in register addressed at 0x08
// and returns data
uint8_t PCF8523::read_reg(uint8_t address) {
  uint8_t data;
  read_reg(&data, 1, address);
  return data;
}

// Example: PCF8523.write_reg(0x08, 0x25);
// Writes value 0x25 in register addressed at 0x08
void PCF8523::write_reg(uint8_t address, uint8_t data) {
  write_reg(address, &data, 1);
}

// Example: PCF8523.set_alarm(10,5,45)
// Set alarm at day = 5, 5:45 a.m.
void PCF8523::set_alarm(uint8_t day_alarm, uint8_t hour_alarm,uint8_t minute_alarm ) {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(0x0A);
  WIRE._I2C_WRITE(bin2bcd(minute_alarm));
  WIRE._I2C_WRITE(bin2bcd(hour_alarm));
  WIRE._I2C_WRITE(bin2bcd(day_alarm));
  WIRE._I2C_WRITE(0);
  WIRE.endTransmission();
}

void PCF8523::set_alarm(uint8_t hour_alarm,uint8_t minute_alarm ) {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(0x0A);
  WIRE._I2C_WRITE(bin2bcd(minute_alarm));
  WIRE._I2C_WRITE(bin2bcd(hour_alarm));
  WIRE._I2C_WRITE(bin2bcd(0));
  WIRE._I2C_WRITE(0);
  WIRE.endTransmission();
}


// Example: PCF8523.get_alarm(a);
// Returns a[0] = alarm minutes, a[1] = alarm hour, a[2] = alarm day
void PCF8523::get_alarm(uint8_t* buf) {
  WIRE.beginTransmission(PCF8523_ADDRESS);
  WIRE._I2C_WRITE(0x0A);
  WIRE.endTransmission();
  WIRE.requestFrom((uint8_t) PCF8523_ADDRESS, (uint8_t)3);
  for (uint8_t pos = 0; pos < 3; ++pos) {
    buf[pos] = bcd2bin((WIRE._I2C_READ() & 0x7F));
  }
  
}


void PCF8523::start_counter_1(uint8_t value){
    // Set timer freq at 1Hz
    write_reg(PCF8523_TMR_A_FREQ_CTRL , 2);
    // Load Timer value
    write_reg(PCF8523_TMR_A_REG,value); 
    // Set counter mode TAC[1:0] = 01 
    // Disable Clockout
    uint8_t tmp;
    tmp = read_reg(PCF8523_TMR_CLKOUT_CTRL);
    tmp |= (1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<1);
    tmp &= ~(1<<2);
    write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);
    // Set countdown flag CTAF
    // Enable interrupt CTAIE
    tmp = read_reg(PCF8523_CONTROL_2);
    tmp|=_BV(PCF8523_CONTROL_2_CTAF_BIT)|_BV(PCF8523_CONTROL_2_CTAIE_BIT);
    write_reg(PCF8523_CONTROL_2,tmp);
}


// Example: PCF8523.reset();
// Reset the PCF8523
void PCF8523::reset(){
    write_reg(PCF8523_CONTROL_1, 0x58);
}

uint8_t PCF8523::clear_rtc_interrupt_flags() {
  uint8_t rc2 = read_reg(PCF8523_CONTROL_2) & (PCF8523_CONTROL_2_SF_BIT | PCF8523_CONTROL_2_AF_BIT);
  write_reg(PCF8523_CONTROL_2, 0);  // Just zero the whole thing
  return rc2 != 0;
}

// Stop the default 32.768KHz CLKOUT signal on INT1.
void PCF8523::stop_32768_clkout() {
    uint8_t tmp = (read_reg (PCF8523_TMR_CLKOUT_CTRL))|RTC_CLKOUT_DISABLED;

    write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_Millis implementation

long RTC_Millis::offset = 0;

void RTC_Millis::adjust(const DateTime& dt) {
    offset = dt.unixtime() - millis() / 1000;
}

DateTime RTC_Millis::now() {
  return (uint32_t)(offset + millis() / 1000);
}

////////////////////////////////////////////////////////////////////////////////


