// 102:r<CR>
// 99:i<CR>
// 100:c<CR>
// 99:r<CR>
// 110:cal,mid,7.00<CR>
//
#include <Wire.h>                   // enable I2C.
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
unsigned long serial_host  = 9600;  // set baud rate for host serial monitor(pc/mac/other)

char sensordata[30];                // A 30 byte character array to hold incoming data from the sensors
char tempdata[30];
char ecdata[30];
byte computer_bytes_received = 0;   // We need to know how many characters bytes have been received
byte sensor_bytes_received = 0;     // We need to know how many characters bytes have been received
int channel;                        // INT pointer for channel switching - 0-7 serial, 8-127 I2C addresses
char *cmd;                          // Char pointer used in string parsing

char computerdata[48];              // we make a 20 byte character array to hold incoming data from a pc/mac/other.
byte code = 0;                      // used to hold the I2C response code.
byte in_char = 0;                   // used as a 1 byte buffer to store in bound bytes from the I2C Circuit.
int time;                         // used to change the dynamic polling delay needed for I2C read operations.


void setup() {                      // startup function
  Serial.begin(9600);              // Set the hardware serial port.
  Wire.begin();         // enable I2C port.
  powerOnPeripherals ();
  intro();          // display startup message
}

void powerOnPeripherals (){
  // power MOSFET - initially off
  digitalWrite (POWER, LOW);
  pinMode      (POWER, OUTPUT);
  }

void powerOffPeripherals (){

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

void serialEvent() {                          // This interrupt will trigger when the data coming from the serial monitor(pc/mac/other) is received
  computer_bytes_received = Serial.readBytesUntil(13, computerdata, 20);  // We read the data sent from the serial monitor(pc/mac/other) until we see a <CR>. We also count how many characters have been received
  computerdata[computer_bytes_received] = 0;                // We add a 0 to the spot in the array just after the last character we received.. This will stop us from transmitting incorrect data that may have been left in the buffer
}

void loop() {                                   // main loop
  powerOnPeripherals();
  delay(2000);
  if (computer_bytes_received != 0) {           // If computer_bytes_received does not equal zero
    
    channel = atoi(strtok(computerdata, ":"));  // Let's parse the string at each colon
    cmd = strtok(NULL, ":");                    // Let's parse the string at each colon

    I2C_call();                           // send to I2C

    computer_bytes_received = 0;                // Reset the var computer_bytes_received to equal 0
  }
  Get_Temp();
  powerOffPeripherals();
  delay(15000);
}

void intro() {                                  // print intro
  Serial.flush();
  Serial.println(" ");
  Serial.println("Welcome to the YNP Sensor Prototype");
}

void I2C_call() {               // function to parse and call I2C commands
  sensor_bytes_received = 0;                    // reset data counter
  memset(sensordata, 0, sizeof(sensordata));    // clear sensordata array;

  if (cmd[0] == 'c' || cmd[0] == 'r')time = 1400;
  else time = 300;                              //if a command has been sent to calibrate or take a reading we
  //wait 1400ms so that the circuit has time to take the reading.
  //if any other command has been sent we wait only 300ms.
  
  Wire.beginTransmission(channel);  // call the circuit by its ID number.
  Wire.write(cmd);            // transmit the command that was sent through the serial port.
  Wire.endTransmission();           // end the I2C data transmission.

  delay(time);

  code = 254;       // init code value

  while (code == 254) {                 // in case the cammand takes longer to process, we keep looping here until we get a success or an error

    Wire.requestFrom(channel, 48, 1);   // call the circuit and request 48 bytes (this is more then we need).
    code = Wire.read();

    while (Wire.available()) {          // are there bytes to receive.
      in_char = Wire.read();            // receive a byte.

      if (in_char == 0) {               // if we see that we have been sent a null command.
        Wire.endTransmission();         // end the I2C data transmission.
        break;                          // exit the while loop.
      }
      else {
        sensordata[sensor_bytes_received] = in_char;  // load this byte into our array.
        sensor_bytes_received++;
      }
    }


    switch (code) {                   // switch case based on what the response code is.
      case 1:                         // decimal 1.
        Serial.println("Success");    // means the command was successful.
        break;                          // exits the switch case.

      case 2:                         // decimal 2.
        Serial.println("< command failed");     // means the command has failed.
        break;                          // exits the switch case.

      case 254:                       // decimal 254.
        Serial.println("< command pending");    // means the command has not yet been finished calculating.
        delay(200);                     // we wait for 200ms and give the circuit some time to complete the command
        break;                          // exits the switch case.

      case 255:                       // decimal 255.
        Serial.println("No Data");    // means there is no further data to send.
        break;                          // exits the switch case.
    }

  }

  Serial.println(sensordata);         // print the data.
}

void Get_Temp() {               // function to parse and call I2C commands
  sensor_bytes_received = 0;                    // reset data counter
  memset(tempdata, 0, sizeof(tempdata));    // clear sensordata array; 
  time = 500;
  Wire.beginTransmission(102);  // call the circuit by its ID number.
  Wire.write("r");            // transmit the command that was sent through the serial port.
  Wire.endTransmission();           // end the I2C data transmission.

  delay(time);

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
        tempdata[sensor_bytes_received] = in_char;  // load this byte into our array.
        sensor_bytes_received++;
      }
    }
  }

  Serial.println(tempdata);         // print the data.
}

void Get_EC() {               // function to parse and call I2C commands
  sensor_bytes_received = 0;                    // reset data counter
  memset(ecdata, 0, sizeof(ecdata));    // clear sensordata array; 
  Wire.beginTransmission(100);  // call the circuit by its ID number.
  Wire.write("r");            // transmit the command that was sent through the serial port.
  Wire.endTransmission();           // end the I2C data transmission.
  delay(3000);

  code = 254;       // init code value

  while (code == 254) {                 // in case the command takes longer to process, we keep looping here until we get a success or an error

    Wire.requestFrom(100, 48, 1);   // call the circuit and request 48 bytes (this is more then we need).
    code = Wire.read();

    while (Wire.available()) {          // are there bytes to receive.
      in_char = Wire.read();            // receive a byte.

      if (in_char == 0) {               // if we see that we have been sent a null command.
        Wire.endTransmission();         // end the I2C data transmission.
        break;                          // exit the while loop.
      }
      else {
        tempdata[sensor_bytes_received] = in_char;  // load this byte into our array.
        sensor_bytes_received++;
      }
    }
  }

  Serial.println(ecdata);         // print the data.
}
