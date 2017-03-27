// Feather9x_TX
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client (transmitter)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example Feather9x_RX

#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP9808.h>
#include <ArduinoJson.h>

/* for feather m0  LoRa95*/
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

//feather M0 display
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5
#define LED      13

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define VBATPIN A7

//display
Adafruit_SSD1306 display = Adafruit_SSD1306();

#if (SSD1306_LCDHEIGHT != 32)
 #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

class Sensor {
  String stype;
  float data;
  String units;
  public:
  Sensor(String t, float d, String u) {
    stype = t;
    data = d;
    units = u;
  }
  //build string representation
  String asString() {
    String s = "type " + stype + " Data ";
    s += data;
    s+= " Units " + units;
    return s;
  }
  //build a JSON representation of the object.
  String asJsonStr(){
    String sJson = "{\"stype\":\"" + stype + "\",\"data\":";
    sJson += String(data,2);  
    sJson += ",\"units\":\"" + units + "\"}";
    return sJson;
  }
};

void setup() 
{
  pinMode(RFM95_RST, OUTPUT);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  digitalWrite(RFM95_RST, HIGH);

  //while (!Serial);
  Serial.begin(9600);
  delay(100);

  //Serial.println("Setting up the OLED");
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
 
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example
  if (!tempsensor.begin()) {
    display.println("Couldn't find MCP9808!");
    while (1);
  }


  display.println("Feather LoRa TX Test!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    display.println("LoRa radio init failed");
    display.display();
    while (1);
  }
  // Serial.println("LoRa radio init OK!");
  //display.println("LoRa radio init OK!");
  
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    display.println("setFrequency failed");
    display.display();
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  display.print("Set Freq to: "); display.println(RF95_FREQ);

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
  display.display();
  delay(10000);
}
//*************************************************************************
//*
//*************************************************************************
int16_t packetnum = 0;  // packet counter, we increment per xmission

void loop()
{
  Serial.println("wake up MCP9808.... "); // wake up MSP9808 - power consumption ~200 mikro Ampere
  tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp

  // Read and print out the temperature, then convert to *F
  float c = tempsensor.readTempC();
  float f = c * 9.0 / 5.0 + 32;
  Sensor temp("temp", f, "F");
  
  Serial.println(temp.asString());
  Serial.println(temp.asJsonStr());
  // Send a message to rf95_server
  
  display.clearDisplay();
  display.setCursor(0,0);

  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Sensor bat("bat", measuredvbat, "V");
  Serial.println(bat.asString());
  Serial.println(bat.asJsonStr());
  display.println(bat.asString());
  display.println(temp.asString());

  Serial.println("Sending to rf95_server");

  String strSend = "{\"UnitID\": 1,\"sensors\":[" + temp.asJsonStr() + "," + bat.asJsonStr() + "]}";
  rf95.send((uint8_t *)strSend.c_str(), strSend.length());
  Serial.println(strSend);
  //display.println(strSend);
  display.display();

  Serial.println("Waiting for packet to complete..."); delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  Serial.println("Waiting for reply..."); delay(10);
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
   {
      Serial.println((char*)buf);
      display.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
  else
  {
    Serial.println("No reply, is there a listener around?");
    display.println("No reply!");
  }
  display.display();
  delay(5000);
}
