//TTN decoder function:

/*
function Decoder(bytes, port) {
  var decoded = {};
  
  if(port == 1)
  {
    // Decode bytes to int
    var pHInt = (bytes[0] << 8) | bytes[1];
    var OrpInt = (bytes[2] << 8) | bytes[3];
    var batteryInt = (bytes[4] << 8) | bytes[5];
    var TempInt = (bytes[6] << 8) | bytes[7];
    
    // Decode int to float
    decoded.pH = pHInt / 100;
    decoded.Orp = OrpInt;
    decoded.batt = batteryInt / 100;
    decoded.temp = TempInt / 100;
   
    return decoded;
  }
}
*/
/************************** Configuration ***********************************/
#include <TinyLoRa.h>
#include "OneWire.h"
#include <DallasTemperature.h>
#include <RunningMedian.h>
#include <SoftTimer.h>
#include <Streaming.h>
#include <yasm.h>

//serial printing stuff
String _endl = "\n";

// Visit your thethingsnetwork.org device console
// to create an account, and obtain the session keys below.

// Network Session Key (MSB)
uint8_t NwkSkey[16] = { XXXXXXXXXXXXXXXXXXXXXX };

// Application Session Key (MSB)
uint8_t AppSkey[16] = { XXXXXXXXXXXXXXXXXXXXXX };

// Device Address (MSB)
uint8_t DevAddr[4] = { XXXXXXXXXXXXXXXXXXXXXX };

/************************** Example Begins Here ***********************************/
// Data Packet to Send to TTN
// Bytes 0-1: pH reading
// Bytes 2-3: Orp reading
// Bytes 4-5: battery reading
// Bytes 6-7: temperature reading
unsigned char loraData[8];

float measuredTemp = 0.0;
float measuredpH = 0.0;
float measuredOrp = 0.0;
float measuredvbat = 0.0;

int16_t batteryInt = 0;
int16_t pHInt = 0;
int16_t OrpInt = 0;
int16_t TempInt = 0;

// Pinout for Adafruit Feather 32u4 LoRa
TinyLoRa lora = TinyLoRa(7, 8);

//Analog input pin to read the pH level
#define pHPIN A0

//Analog input pin to read the Orp level
#define OrpPIN A1

//Analog input pin to read the Battery level
#define VBATPIN A3

//Done pin to switch TPL1150 off (not implemented yet)
#define donePin 5

//One wire bus for the water temperature measurement
//Data wire is connected to input digital pin 6 on the Arduino
#define ONE_WIRE_BUS 6

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature library instance 
DallasTemperature sensor(&oneWire);

//12bits (0,06°C) temperature sensor resolution
#define TEMPERATURE_RESOLUTION 12

//MAC Address of DS18b20 water temperature sensor
DeviceAddress DS18b20 = { 0x28, 0x92, 0x25, 0x41, 0x0A, 0x00, 0x00, 0xEE };

//Signal filtering library. Only used in this case to compute the average
//over multiple measurements but offers other filtering functions such as median, etc. 
RunningMedian samples_Temp = RunningMedian(10);
RunningMedian samples_Ph = RunningMedian(10);
RunningMedian samples_Orp = RunningMedian(10);

//Callbacks
//Here we use the SoftTimer library which handles multiple timers (Tasks)
//It is more elegant and readable than a single loop() functtion, especially
//when tasks with various frequencies are to be used
void MeasureCallback(Task* me);
void PublishDataCallback(Task* me);

Task t1(100, MeasureCallback);                //Take measurements
Task t2(30000, PublishDataCallback);          //Publish data to MQTT broker every 30 secs

//State Machine
//Getting a 12 bits temperature reading on a DS18b20 sensor takes >750ms
//Here we use the sensor in asynchronous mode, request a temp reading and use
//the nice "YASM" state-machine library to do other things while it is being obtained
YASM gettemp;

void setup()
{
  delay(200);
  Serial.begin(9600);
  
  //uncomment this in debug mode
 // while (! Serial);
 
  // Initialize pin LED_BUILTIN as an output
  pinMode(donePin, OUTPUT);
  digitalWrite(donePin, LOW);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(11, OUTPUT);
  digitalWrite(11, LOW);
  
  // Initialize LoRa
  // Make sure Region #define is correct in TinyLora.h file
  Serial.print("Starting LoRa...");
  // define multi-channel sending
  lora.setChannel(MULTI);
  // set datarate
  lora.setDatarate(SF7BW125);
  if(!lora.begin())
  {
    Serial.println("Failed");
    Serial.println("Check your radio");
    while(true);
  }
  Serial.println("OK");

  //MeasureCallback
  SoftTimer.add(&t1);

  //PublishDataCallback
  SoftTimer.add(&t2);

  //Start measurements state machine
  gettemp.next(gettemp_start);
}

//Loop where various tasks are updated/handled
void MeasureCallback(Task* me)
{
  //request and get temp, pH and Orp readings
  gettemp.run();
}

//Publish measurements every 30 secs
void PublishDataCallback(Task* me)
{
    // encode float as int
    TempInt = round(measuredTemp * 100);
    batteryInt = round(measuredvbat * 100);
    pHInt = round(measuredpH * 100);
    OrpInt = round(measuredOrp);
     
    Serial.print("VBat: " ); 
    Serial.print(measuredvbat);
    Serial.print("V\t");
    Serial.print("pH: ");
    Serial.print(measuredpH);
    Serial.print("\t");
    Serial.print("Orp: ");
    Serial.print(OrpInt);
    Serial.print("mV\t");
    Serial.print("Temp: ");
    Serial.print(measuredTemp);
    Serial.print("°C\n");
  
    // encode int as bytes
    //byte payload[2];
    loraData[0] = highByte(pHInt);
    loraData[1] = lowByte(pHInt);
    
    loraData[2] = highByte(OrpInt);
    loraData[3] = lowByte(OrpInt);
    
    loraData[4] = highByte(batteryInt);
    loraData[5] = lowByte(batteryInt);
  
    loraData[6] = highByte(TempInt);
    loraData[7] = lowByte(TempInt);

    Serial.println("Sending LoRa Data...");
    lora.sendData(loraData, sizeof(loraData), 0x01, lora.frameCounter);
    Serial.print("Frame Counter: ");Serial.println(lora.frameCounter);
    lora.frameCounter++;
  
    // blink LED to indicate packet sent
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  
/*
    //Go to sleep. Not yet implemented
    digitalWrite(donePin, HIGH);
    delay(10);
    digitalWrite(donePin, LOW);
    delay(10);
*/
}

//Update temperature, Ph and Orp values
void getMeasures(DeviceAddress deviceAddress)
{
    //Get water temperature
    samples_Temp.add(sensor.getTempC(deviceAddress));
    measuredTemp = samples_Temp.getAverage(10);
    if (measuredTemp == -127.00) {
      Serial<<F("Error getting temperature from DS18b20")<<_endl;
    } else {
      Serial<<F("DS18b20: ")<<measuredTemp<<F("°C")<<F(" - ");
    }
  
    //Read pH level
    measuredpH = analogRead(pHPIN)* 1.0846 * 5.0 / 1023.0;// we divided by 1.666 with the resistors bridge and our ref is 3.3V, so multiply by 1.11 in theory
    measuredpH = (0.0178 * measuredpH * 200.0) - 1.889; 
    samples_Ph.add(measuredpH);                                                                      // compute average of pH from last 5 measurements
    measuredpH = samples_Ph.getAverage(10);
    Serial<<F("Ph: ")<<measuredpH<<F(" - ");

    //Read Orp level
    measuredOrp = analogRead(OrpPIN)* 1.0846 * 5.0 / 1023.0;// we divided by 1.666 with the resistors bridge and our ref is 3.3V, so multiply by 1.11 in theory
    measuredOrp = ((2.5 - measuredOrp) / 1.037) * 1000.0;
    samples_Orp.add(measuredOrp);                                                                    // compute average of ORP from last 5 measurements
    measuredOrp = samples_Orp.getAverage(10);
    Serial<<F("Orp: ")<<measuredOrp<<F("mV")<<_endl;
    
    //Read battery level
    measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2 with the resistors bridge, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
}

////////////////////////gettemp state machine///////////////////////////////////
//Init DS18B20 one-wire library
void gettemp_start()
{
    // Start up the library
    sensor.begin();
       
    // set the resolution
    sensor.setResolution(DS18b20, TEMPERATURE_RESOLUTION);

    //don't wait ! Asynchronous mode
    sensor.setWaitForConversion(false); 
       
    gettemp.next(gettemp_request);
} 

//Request temperature asynchronously
void gettemp_request()
{
  sensor.requestTemperatures();
  gettemp.next(gettemp_wait);
}

//Wait asynchronously for requested temperature measurement
void gettemp_wait()
{ //we need to wait that time for conversion to finish
  if (gettemp.elapsed(1000/(1<<(12-TEMPERATURE_RESOLUTION))))
    gettemp.next(gettemp_read);
}

//read and print temperature measurement
void gettemp_read()
{
    getMeasures(DS18b20); 
    gettemp.next(gettemp_request);
}
