#include <SPI.h>             // SD interface/communication? 
#include <Wire.h>            // I2C library
#include <RTClib.h>          // https://github.com/MrAlvin/RTClib
#include <MemoryFree.h>      // monitor memory while programming
#include "LowPower.h"        // https://github.com/rocketscream/Low-Power
#include "SdFat.h"           // SD Fat32 library

//------------------------------------------------------------------------------
// File system object.
//------------------------------------------------------------------------------
#define FILE_BASE_NAME "123456"
#define error(msg) sd.errorHalt(F(msg))

//------------------------------------------------------------------------------
// Objects
//------------------------------------------------------------------------------
SdFat sd;                           //SDFat object
SdFile file;                        //Log file
RTC_DS3231 RTC;                     //RTC object

//------------------------------------------------------------------------------
// User Defined constants
//------------------------------------------------------------------------------
#define serial "987654321"          // SN of logger. These will probaby be shorter
#define feature "Atomizer"          // Feature names sometimes include spaces. We need to make spaces underscores
#define SampleIntervalMinutes 1  // Options: 1,2,3,4,5,6,10,12,15,20,30,60 ONLY (must be a divisor of 60)
// this is the number of minutes the loggers sleeps between each sensor reading
#define ECHO_TO_SERIAL // this define enables debugging output to the serial monitor when your logger is powered via USB/UART
// comment out this define when you are deploying the logger and running on batteries

//------------------------------------------------------------------------------
// Additional Constants
//------------------------------------------------------------------------------
const uint8_t chipSelect = SS;
const uint8_t cardDetect = 8;       //SD card detect
const uint32_t SAMPLE_INTERVAL_MS = 1000;
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
const uint8_t ANALOG_COUNT = 4;
const int BAUD = 9600;              //default baud rate
const int temp_time = 600;         //time to measure temperature. Look into how long it actually takes, but I think 1 sec is close to min
const byte POWER = 3;               // Power pin to turn on sensor board

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------
uint32_t logTime;                   // Time in micros for next data record.
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
byte sensor_bytes_received = 0;     // We need to know how many characters bytes have been received
byte bcdToDec(byte val){return ( (val/16*10) + (val%16) );}
byte code = 0;                      // used to hold the I2C response code.
byte in_char = 0;                   // used as a 1 byte buffer to store in bound bytes from the I2C Circuit.
int clockAddress = 0x68;            // I2C address of clock
char fileName[13] = FILE_BASE_NAME "00.csv";  //MAKE THIS TAKE LOGGERFILENAME.csv!!!!!!! Will need to increase number of characters
char getTime[30];                   // number of characters in getTime call from RTC
char dateStr[8];                    // number of characters in dateStr (yyyymmdd)
char timeStr[8];                    // number of characters in timeStr (hh:mm:ss)
char tempData[30];                  // number of characters in tempData. Maybe bring this down if string space becomes an issue
char loggerFileName[100];           // number of characters in logger file name (yyyymmdd_feature_serial)
bool alreadyBegan = false;          // SD.begin() misbehaves if not first call

// This tracks what cycle the wake up period is on. E.G. if we want to wait 10 
// seconds between readings but wake up every 2 seconds we would track the 
// number of 2 second increments to get to 10 seconds, 5 in this case.
uint8_t wakeUpPeriod = 0; 


// variables for reading the RTC time & handling the INT(0) interrupt it generates
#define DS3231_I2C_ADDRESS 0x68
#define DS3231_CONTROL_REG 0x0E
#define RTC_INTERRUPT_PIN 2
uint8_t alarmHour;
uint8_t alarmMinute;
uint8_t alarmDay;
char CycleTimeStamp[ ] = "0000/00/00,00:00"; //16 ascii characters (without seconds because they are always zeros on wakeup)
volatile boolean clockInterrupt = false;  //this flag is set to true when the RTC interrupt handler is executed
//variables for reading the DS3231 RTC temperature register
float rtc_TEMP_degC;
uint8_t tMSB = 0;
uint8_t tLSB = 0;

uint8_t bytebuffer1 = 0;

//==============================================================================
// Functions
//==============================================================================
void intro() {                                  // print intro
  Serial.flush();                   // flush any serial info that may exist
  Serial.println(" ");
  Serial.println("Welcome to the YNP Sensor Prototype");
}
//------------------------------------------------------------------------------

void loggerName() {
  sprintf(loggerFileName, "20%d%d%d_%s_%s.csv", year, month, dayOfMonth, feature, serial);  //yyyymmdd_feature_serial.csv
}
//------------------------------------------------------------------------------

void writeHeader() {                // we need to figure out how to enable additional sensors with a GUI
  file.print(F("date"));            // date header
  file.print(F(",time"));           // time of day header
  file.print(F(",temperature"));    // temperature measurement
  file.println();                   // brings cursor down for immediate data entry
}
//------------------------------------------------------------------------------

void logData() {                    // data logging loop
  file.open((fileName), FILE_WRITE);    // open existing filename cached and begin writting
  file.print(dateStr);              // write date
  file.write(',');                 
  file.print(timeStr);              // write time
  file.print(',');
  file.println(tempData);           // write temp
  delay (200);                      // supposed to give a 200ms delay before closing
  file.close();                     // close file after writing. do we need to flush? 
}
//------------------------------------------------------------------------------

void initializeCard() {
  if (!digitalRead(cardDetect))
  {
    Serial.println(F("No card detected. Waiting for card."));
    while (!digitalRead(cardDetect));
    delay(250); // 'Debounce insertion'
  }
  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(10, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // Find an unused file name.
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }
  // Read any Serial data.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);
  writeHeader();
  Serial.print(F("Logging to: "));
  Serial.println(fileName);
  Serial.println(F("Type any character to stop"));
 }
//------------------------------------------------------------------------------

 void inUseCardRemoval()
{
  // Is there even a card?
  if (!digitalRead(cardDetect))
  {
    initializeCard();
  }
  else
  {
    alreadyBegan = true;
    
  }
}
//------------------------------------------------------------------------------

void forceData() {
  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }
}
//------------------------------------------------------------------------------

void waitLog() {
int32_t diff;
  do {
    diff = micros() - logTime;
  } while (diff < 0);

  // Check for data rate too high.
  if (diff > 10) {
    error("Missed data record");
  }
}
//------------------------------------------------------------------------------

void powerOnPeripherals (){
  // power MOSFET - initially off
  digitalWrite (POWER, LOW);
  pinMode      (POWER, OUTPUT);
  }
//------------------------------------------------------------------------------

void powerOffPeripherals (){

  // put all digital pins into low stats
  for (byte pin = 0; pin < 14; pin++)
    {
    digitalWrite (POWER, HIGH);  //SET TO HIGH FOR CODING!!! Change!!!
    pinMode (POWER, OUTPUT);
    }
}
//------------------------------------------------------------------------------

void setLogTime () {
  // Start on a multiple of the sample interval.
  logTime = micros()/(1000UL*SAMPLE_INTERVAL_MS) + 1;
  logTime *= 1000UL*SAMPLE_INTERVAL_MS;
}
//------------------------------------------------------------------------------

void Get_Time() {                               // function to parse and call I2C commands      
  sensor_bytes_received = 0;                    // reset data counter
  memset(getTime, 0, sizeof(getTime));          // clear sensordata array; 
  Wire.beginTransmission(clockAddress);         // call the circuit by its ID number.
  Wire.write(byte(0x00));
  Wire.endTransmission();

  Wire.requestFrom(clockAddress, 7);
  // A few of these need masks because certain bits are control bits
  second     = bcdToDec(Wire.read() & 0x7f);
  minute     = bcdToDec(Wire.read());
  
  // Need to change this if 12 hour am/pm
  hour       = bcdToDec(Wire.read() & 0x3f);  
  dayOfWeek  = bcdToDec(Wire.read());
  dayOfMonth = bcdToDec(Wire.read());
  month      = bcdToDec(Wire.read());
  year       = bcdToDec(Wire.read());
  Wire.endTransmission();
}
//------------------------------------------------------------------------------

void dateString () {
  sprintf (dateStr, "20%02d%02d%02d",
    year, month, dayOfMonth);
    //Serial.println(dateStr);
}
//------------------------------------------------------------------------------

void timeString () {
  sprintf (timeStr, "%02d:%02d:%02d",
    hour, minute, second);
    //Serial.println(timeStr);
}
//------------------------------------------------------------------------------

void Get_Temp() {               // function to parse and call I2C commands
  sensor_bytes_received = 0;                    // reset data counter
  memset(tempData, 0, sizeof(tempData));    // clear sensordata array; 
  Wire.beginTransmission(102);  // call the circuit by its ID number.
  Wire.write("r");            // transmit the command that was sent through the serial port.
  Wire.endTransmission();           // end the I2C data transmission.

  delay(temp_time);

  code = 254;       // init code value

  while (code == 254) {                 // in case the command takes longer to process, we keep looping here until we get a success or an error

    Wire.requestFrom(102, 48, 1);   // call the circuit and request 48 bytes (this is more then we need).
    code = Wire.read();

    while (Wire.available()) {          // are there bytes to receive.
      in_char = Wire.read();            // receive a byte.

      if (in_char == 0) {               // if we see that we have been sent a null command.
        Wire.endTransmission();         // end the I2C data transmission.
        break;                          // exit the while loop.
      }
      else {
        tempData[sensor_bytes_received] = in_char;  // load this byte into our array.
        sensor_bytes_received++;
      }
    }
  }

  Serial.print(tempData);         // print the data.
  Serial.println(" C");
}
//------------------------------------------------------------------------------

void sysCallHalt() {
    if (Serial.available()) {
    // Close file and stop.
    file.close();
    Serial.println(F("Done"));
    SysCall::halt();
  }
}
//------------------------------------------------------------------------------

void viewSysMemory () {
  Serial.print("freeMemory()=");
  Serial.println(freeMemory());
}


// This is the Interrupt subroutine that only executes when the RTC alarm goes off:
void rtcISR() {
  clockInterrupt = true;
}

// Enable Battery-Backed Square-Wave Enable on the RTC module: 
// Bit 6 (Battery-Backed Square-Wave Enable) of DS3231_CONTROL_REG 0x0E, can be set to 1 
// When set to 1, it forces the wake-up alarms to occur when running the RTC from the back up battery alone. 
// [note: This bit is usually disabled (logic 0) when power is FIRST applied]
//
void enableRTCAlarmsonBackupBattery(){
  Wire.beginTransmission(DS3231_I2C_ADDRESS);// Attention RTC 
  Wire.write(DS3231_CONTROL_REG);            // move the memory pointer to CONTROL_REG
  Wire.endTransmission();                    // complete the ‘move memory pointer’ transaction
  Wire.requestFrom(DS3231_I2C_ADDRESS,1);    // request data from register
  uint8_t resisterData = Wire.read();           // byte from registerAddress
  bitSet(resisterData, 6);                   // Change bit 6 to a 1 to enable
  Wire.beginTransmission(DS3231_I2C_ADDRESS);// Attention RTC
  Wire.write(DS3231_CONTROL_REG);            // target the register
  Wire.write(resisterData);                  // put changed byte back into CONTROL_REG
  Wire.endTransmission();
}

void clearClockTrigger()   // from http://forum.arduino.cc/index.php?topic=109062.0
{
  Wire.beginTransmission(0x68);   //Tell devices on the bus we are talking to the DS3231
  Wire.write(0x0F);               //Tell the device which address we want to read or write
  Wire.endTransmission();         //Before you can write to and clear the alarm flag you have to read the flag first!
  Wire.requestFrom(0x68,1);       //Read one byte
  bytebuffer1=Wire.read();        //In this example we are not interest in actually using the byte
  Wire.beginTransmission(0x68);   //Tell devices on the bus we are talking to the DS3231 
  Wire.write(0x0F);               //Status Register: Bit 3: zero disables 32kHz, Bit 7: zero enables the main oscilator
  Wire.write(0b00000000);         //Bit1: zero clears Alarm 2 Flag (A2F), Bit 0: zero clears Alarm 1 Flag (A1F)
  Wire.endTransmission();
  clockInterrupt=false;           //Finally clear the flag we used to indicate the trigger occurred
}


//==============================================================================
// Setup
//==============================================================================
void setup() {
  powerOnPeripherals ();    // powers board through MOSFET
  Serial.begin(BAUD);       // set BAUD rate
  delay(1000);              // give sensors time to boot.
  initializeCard();         // start sd card. upon reinsertion of sd card it will create a new file even if one exists
  setLogTime ();            // I'm not 100% sure how this works or what it ties into. Look into this
  Wire.begin();             // Start I2C communication (RTC and sensors)
  RTC.begin();              // Start Real-Time Clock
  Get_Time();               // Grab a time from the RTC. Will likely be wrong the first pull, so we pull it twice
  delay(200);               // Give the RTC a sec to send a new signal
  Get_Time();               // This time is almost never correct on the first call, so by calling it here the first reading will be correct in the loop
  loggerName();             // Needs to be run after correct time has been called since the name is based on the format yyyymmdd_feature_serial
  intro();                  // Run intro script (Welcome to YNP Sensor Prototype)
  
  
  pinMode(RTC_INTERRUPT_PIN,INPUT_PULLUP);// RTC alarms low, so need pullup on the D2 line 
  //Note using the internal pullup is not needed if you have hardware pullups on SQW line, and most RTC modules do.
  RTC.begin();  // RTC initialization:
  clearClockTrigger(); //stops RTC from holding the interrupt low after power reset occured
  RTC.turnOffAlarm(1);
  DateTime now = RTC.now();
  sprintf(CycleTimeStamp, "%04d/%02d/%02d %02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute());
  enableRTCAlarmsonBackupBattery(); // this is only needed if you cut the VCC pin supply on the DS3231
}

//==============================================================================
// Loop
//==============================================================================
void loop() {
  
  // Time for next record.
  DateTime now = RTC.now(); //this reads the time from the RTC
  logTime += 1000UL*SAMPLE_INTERVAL_MS;   // again, LOOK INTO THIS
  //waitLog();              // If this is run while I2C is running it pops an error. Not sure why. 
  powerOnPeripherals ();    // Power on sensor board (and RTC/SD) SD not currently being powered down with board. Where should we put it? 
  delay(800);              // This delay should cover the read time for the temp probe
  Get_Temp();               // Get temperature from sensor
  Get_Time();               // Get time string from RTC 
  dateString ();            // Pull date from the get_time string. Can we move this into logData?
  timeString ();            // Pull time from the get_time string. Can we move this into logData? 
  inUseCardRemoval();       // Check to see if there is a card present. If not hold operations
  logData();                // Write the data to the SD
  forceData();              // Not sure? 
  //viewSysMemory ();       //check system memory while programming 
  powerOffPeripherals ();   // Power off sensor board
  
  // Set the next alarm time
  alarmHour = now.hour();
  alarmMinute = now.minute() + SampleIntervalMinutes;
  alarmDay = now.day();
  
  // Check for rollovers
  if (alarmMinute > 59) {
    alarmMinute = 0;
    alarmHour++;
    if (alarmHour > 23) {
      alarmHour = 0; 
      // Seems like this isn't quite right. Should investigate more - Kevin
    }
  }
  
  // Set the alarm
  RTC.setAlarm1Simple(alarmHour, alarmMinute);
  RTC.turnOnAlarm(1);
  if (RTC.checkAlarmEnabled(1)) {
    // For testing only
    #ifdef ECHO_TO_SERIAL
    Serial.print(F("RTC Alarm Enabled!"));
    Serial.print(F(" Going to sleep for : "));
    Serial.print(SampleIntervalMinutes);
    Serial.println(F(" minute(s)"));
    Serial.println();
    Serial.flush();//adds a carriage return & waits for buffer to empty
    #endif
  }

  //——– sleep and wait for next RTC alarm ————–
  // Enable interrupt on pin2 & attach it to rtcISR function:
  attachInterrupt(0, rtcISR, LOW);
  // Enter power down state with ADC module disabled to save power:
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_ON);
  //processor starts HERE AFTER THE RTC ALARM WAKES IT UP
  detachInterrupt(0); // immediately disable the interrupt on waking
  //Interupt woke processor, now go back to the start of the main loop
}