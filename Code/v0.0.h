#include <Wire.h>                     // enable I2C.
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <SPI.h>
#include <RTClib.h>
#include <SdFat.h>
#include <FormatDouble.h>

char sensordata[30];                  // A 30 byte character array to hold incoming data from the sensors
byte sensor_bytes_received = 0;       // We need to know how many characters bytes have been received

byte code = 0;                        // used to hold the I2C response code.
byte in_char = 0;                     // used as a 1 byte buffer to store in bound bytes from the I2C Circuit.

#define TOTAL_CIRCUITS 2              // <-- CHANGE THIS | set how many I2C circuits are attached to the Tentacle shield(s): 1-8

int channel_ids[] = {100, 102};// <-- CHANGE THIS.  100 EC, 102 TEMP
// A list of I2C ids that you set your circuits to.
// This array should have 1-8 elements (1-8 circuits connected)

char *channel_names[] = {"EC", "TEMP"}; // <-- CHANGE THIS.
// A list of channel names (must be the same order as in channel_ids[]) 
// it's used to give a name to each sensor ID. This array should have 1-8 elements (1-8 circuits connected).
// {"PH Tank 1", "PH Tank 2", "EC Tank 1", "EC Tank2"}, or {"PH"}

int val = 0;           // variable to store the value read
int analogPin = 3;
const byte POWER = 3;
const byte LED = 4;
const byte SD_CHIP_SELECT = 10;
const char * fileName = "TEST_READINGS.TXT";
const int TIME_TO_SHOW_READINGS = 3000;
const int TAKE_READINGS_EVERY = 1;    // minutes

unsigned long errorCount;
unsigned long writeCount;

void setup() {                      // startup function
  Serial.begin(9600);              // Set the hardware serial port.
  Wire.begin();         // enable I2C port.
  powerOnPeripherals ();
  delay (1000);
  powerOffPeripherals ();
}

void powerOnPeripherals ()
  {
  // power MOSFET - initially off
  digitalWrite (POWER, LOW);
  pinMode      (POWER, OUTPUT);
  }

void powerOffPeripherals ()
  {

  // put all digital pins into low stats
  for (byte pin = 0; pin < 14; pin++)
    {
    digitalWrite (pin, LOW);
    pinMode (pin, OUTPUT);
    }

  // except the power pin which has to be high
  digitalWrite (POWER, HIGH);  // turn power OFF
  // and activation switch
  }  // end of powerOffPeripherals


void loop() {
  powerOnPeripherals ();
  delay(2000);
  
  for (int channel = 0; channel < TOTAL_CIRCUITS; channel++) {       // loop through all the sensors
  
    Wire.beginTransmission(channel_ids[channel]);     // call the circuit by its ID number.
    Wire.write('r');                          // request a reading by sending 'r'
    Wire.endTransmission();                         // end the I2C data transmission.
    
    delay(1000);  // AS circuits need a 1 second before the reading is ready

    sensor_bytes_received = 0;                        // reset data counter
    memset(sensordata, 0, sizeof(sensordata));        // clear sensordata array;

    Wire.requestFrom(channel_ids[channel], 48, 1);    // call the circuit and request 48 bytes (this is more then we need).
    code = Wire.read();

    while (Wire.available()) {          // are there bytes to receive?
      in_char = Wire.read();            // receive a byte.

      if (in_char == 0) {               // null character indicates end of command
        Wire.endTransmission();         // end the I2C data transmission.
        break;                          // exit the while loop, we're done here
      }
      else {
        sensordata[sensor_bytes_received] = in_char;      // append this byte to the sensor data array.
        sensor_bytes_received++;
      }
    }
    
    Serial.print(channel_names[channel]);   // print channel name
    Serial.print(':');

    switch (code) {                       // switch case based on what the response code is.
      case 1:                             // decimal 1  means the command was successful.
        Serial.println(sensordata);       // print the actual reading
        break;                              // exits the switch case.

      case 2:                             // decimal 2 means the command has failed.
        Serial.println("command failed");   // print the error
        break;                              // exits the switch case.

      case 254:                           // decimal 254  means the command has not yet been finished calculating.
        Serial.println("circuit not ready"); // print the error
        break;                              // exits the switch case.

      case 255:                           // decimal 255 means there is no further data to send.
        Serial.println("no data");          // print the error
        break;                              // exits the switch case.
    }

  } // for loop 
  //POWER OFF
  powerOffPeripherals ();
  delay(15000);
}
