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

// Visit your thethingsnetwork.org device console
// to create an account, and obtain the session keys below.

// Network Session Key (MSB)
uint8_t NwkSkey[16] = { xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx };

// Application Session Key (MSB)
uint8_t AppSkey[16] = { xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx };

// Device Address (MSB)
uint8_t DevAddr[4] = { xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx };

/************************** Example Begins Here ***********************************/
// Data Packet to Send to TTN
// Bytes 0-1: pH reading
// Bytes 2-3: Orp reading
// Bytes 4-5: battery reading
// Bytes 6-7: temperature reading
unsigned char loraData[8];

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

//Done pin to switch TPL115 off
#define donePin 5

//One wire bus for the water temperature measurement
//Data wire is connected to input digital pin 20 on the Arduino
#define ONE_WIRE_BUS 6

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature library instance 
DallasTemperature sensor(&oneWire);

//12bits (0,06°C) temperature sensor resolution
#define TEMPERATURE_RESOLUTION 12

//MAC Address of DS18b20 water temperature sensor
DeviceAddress DS18b20 = { 0x28, 0x92, 0x25, 0x41, 0x0A, 0x00, 0x00, 0xEE };


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

  // Start up the Dallas library
  sensor.begin();
       
  // set the resolution
  sensor.setResolution(DS18b20, TEMPERATURE_RESOLUTION);

  //Synchronous mode
  sensor.setWaitForConversion(true);

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
}

void loop()
{
    //Get water temperature
    sensor.requestTemperatures();
    float measuredTemp = sensor.getTempC(DS18b20);
  
    //Read pH level
    float measuredpH = analogRead(pHPIN);
    measuredpH *= 1.0846;    // we divided by 1.666 with the resistors bridge and our ref is 3.3V, so multiply by 1.11 in theory
    measuredpH *= 5.0;  // Multiply by 5.0V, our reference voltage
    measuredpH /= 1024; // convert to voltage
    measuredpH = (0.0178 * measuredpH * 200.0) - 1.889; 

    //Read Orp level
    float measuredOrp = analogRead(OrpPIN);
    measuredOrp *= 1.0846;    // we divided by 1.666 with the resistors bridge and our ref is 3.3V, so multiply by 1.11 in theory
    measuredOrp *= 5.0;  // Multiply by 5.0V, our reference voltage
    measuredOrp /= 1024; // convert to voltage
    measuredOrp = ((2.5 - measuredOrp) / 1.037) * 1000.0;
    
    //Read battery level
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2 with the resistors bridge, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    
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
//    SleepCounter = 0;//reset counter
    delay(30000);
  
/*
    //Go to sleep using a TPL5110 board. Not yet implemented
    digitalWrite(donePin, HIGH);
    delay(10);
    digitalWrite(donePin, LOW);
    delay(10);
*/
}
