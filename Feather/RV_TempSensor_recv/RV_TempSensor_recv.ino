#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_MCP9808.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <Adafruit_FeatherOLED.h>
#include <Adafruit_FeatherOLED_WiFi.h>

Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_FeatherOLED_WiFi oled = Adafruit_FeatherOLED_WiFi();

/*******************************************************
   define constances and singleton items
 *******************************************************/
/* for feather m0  LoRa95*/
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

//define the LED on the board
#define LED      13

//define frequency
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

//define the battery reference pin
#define VBATPIN A7

//Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

/*******************************************************
   DEfine the sensor object
*/
class Sensor {
    String stype;
    float data;
    String units;
  public:

    /***********************************************
       Constructor passing the values.
    */
    Sensor(String t, float d, String u) {
      stype = t;
      data = d;
      units = u;
    }

    /**********************************************
       getters and setters
    */
    String getType() {
      return stype;
    }
    float  getData() {
      return data;
    }
    String getUnits() {
      return units;
    }
    void setData(float d) {
      data = d;
    }

    /**********************************************
       return the object as a string
       Type temp Data 78.1 Units F
    */
    String asString() {
      String s = "Type " + stype + " Data ";
      s += data;
      s += " Units " + units;
      return s;
    }

    /**********************************************
       return object as a JSON String
       {"stype":"temp","data":78.1,"units":"F"}
    */
    String asJsonStr() {
      String sJson = "{\"stype\":\"" + stype + "\",\"data\":";
      sJson += String(data, 2);
      sJson += ",\"units\":\"" + units + "\"}";
      return sJson;
    }

    /**********************************************
       constuctor from json object.
    */
        Sensor(JsonArraySubscript json) {
          stype = ((const char*) json["stype"]);
          data =  (float)       json["data"];
          units = (const char*) json["units"];
          //Serial.println(asString());
        }
};


void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  
  //while (!Serial);
  Serial.begin(9600);
  delay(100);

  /******************************************************************************************8
   * Setup the OLED display
   */
  oled.init();
  oled.setBatteryVisible(false);
  oled.setIPAddressVisible(false);
  oled.setRSSIVisible(false);
  oled.setConnected(true);  //must be set true to disply RSSI6
  oled.setConnectedVisible(false);
  oled.clearDisplay();
  oled.refreshIcons();
  oled.clearMsgArea();  
  oled.println("Initialize MCP9808 ...");
  oled.display();
   

  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example
  if (!tempsensor.begin()) {
    oled.clearMsgArea();  
    oled.println("Couldn't find MCP9808!");
    oled.display();
    while (1);
  }

  
  /*********************************
     setup radio
  */

  // manual resetradio
  Serial.println("...reset radio");
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    oled.refreshIcons();
    oled.clearMsgArea();  
    oled.println("LoRa radio init failed");
    oled.display();
    while (1);
  }

  /***************************************
     Defaults after init are 434.0MHz,
     modulation GFSK_Rb250Fd250, +13dbM
  */
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    oled.refreshIcons();
    oled.clearMsgArea();  
    oled.println("setFrequency failed");
    oled.display();

    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  oled.refreshIcons();
  oled.clearMsgArea();  
  oled.print("Set Freq to: "); oled.println(RF95_FREQ);
  oled.display();
  

  /***************************************
    you can set transmitter powers from 5 to 23 dBm:
  */
  rf95.setTxPower(23, false);
  delay(1000);
  oled.setBatteryVisible(true);
  oled.setRSSIVisible(true);  


  matrix.begin(0x70);

  // print a floating point 
  matrix.print(00.01);
  matrix.writeDisplay();
  delay(500);

}

void loop() {
  //***********************************************************************************************
  //TEMP TEST CODE REPLACE WHEN RECIEVER IS WORKING.
//  Serial.println("wake up MCP9808.... "); // wake up MSP9808 - power consumption ~200 mikro Ampere
//  tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp
//  delay(10);
//  // Read and print out the temperature, then convert to *F
//  float c = tempsensor.readTempC();
//  float f = c * 9.0 / 5.0 + 32;
//  Sensor temp("temp", f, "F");
//  Serial.print("F "); Serial.println(f);
//  Serial.print("C "); Serial.println(c);
//  
//  Serial.println(temp.asString());
  //*************************************************************************************************


/****************************************************************************************************
 * Get temp from radio
 */
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  StaticJsonBuffer<200> jsonBuffer;

  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  oled.setBattery(measuredvbat); 
  oled.refreshIcons();
  oled.clearMsgArea();  
  oled.println("Waiting for temp");
  oled.display();


  Serial.println("Waiting for temp"); delay(10);
  if (rf95.waitAvailableTimeout(1000))
  {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len))
    {
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      oled.setRSSI((int) rf95.lastRssi());  
      oled.refreshIcons();

 
      JsonObject& root = jsonBuffer.parseObject((char*)buf);
      if (!root.success()) {
        Serial.println("parseObject() failed");
        oled.refreshIcons();
        oled.clearMsgArea();  
        oled.println("parseObject() failed");
        oled.display();

        return;
      }
      
      const char* unitID = root["UnitID"];
      Sensor sensor1(root["sensors"][0]);
      Sensor sensor2(root["sensors"][1]);
      
      oled.refreshIcons();
      oled.clearMsgArea();  
      oled.print("Temp: ");oled.print(sensor1.getData());oled.println(" F");
      oled.display();
      
      matrix.print(sensor1.getData());
      matrix.writeDisplay();
      delay(1500);

    }
    else
    {
      Serial.println("Receive failed");
      oled.refreshIcons();
      oled.clearMsgArea();  
      oled.println("Receive failed");
      oled.display();
      
    }
  }
  else
  {
    Serial.println("No reply, is there a listener around?");
    oled.refreshIcons();
    oled.clearMsgArea();  
    oled.println("No reply, is there a listener around?");
    oled.display();
  }
  delay(5000);


  
}
