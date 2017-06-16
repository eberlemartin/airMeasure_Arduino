/***************************************************************************
   Pins:
   Display(Yellow/Blue):
    SCL -> D15
    SDA -> D14
    VCC -> 3.3V
    GND -> GND

   BME280:
    SCL -> D3
    SDA -> D4
    VIN -> 3.3V
    GND -> GND

   DHT11:
    Dat -> D6
    VIN -> 3.3V
    GND -> GND
    
   MQ135:
   VIN -> 3.3V
   GND -> GND
   AO  -> A0
   
   Achtung! 5V/3.3V Problem!
 ****************************************************************************/

 /****************
  * TODO List:
  * -Add Presence Sensor to turn off Display when no one is around
  * -Add Time from Internet to dim screen at night
  * -Add subscreens with a chart for last 5 Hours
  * -Add Button to switch through subscreens
  * **************/

#include <Wire.h>
//#include <SPI.h>
//#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <SparkFunBME280.h>
#include <dht11.h>
#include <MQ135.h>


#define OLED_RESET -1        //TODO: erase this maybe? If then fix library for reset pin in i2c
#define DISPLAY_ADDRESS 0x3C
#define ANALOGPIN A0         //Analog Pin for MQ135 GasSensor
#define RZERO 206.85         //Rzero for beginning


Adafruit_SSD1306 display(OLED_RESET);
BME280 envSensor;
dht11 dhtSensor;
MQ135 gasSensor = MQ135(ANALOGPIN);

//##### WIFI Stuff #####
//Pin for Internal LED is D5(onboard) in case ever needed
//int ledPin = d5;
const char* ssid = "miau";
const char* password = "**PASSWORD**";
unsigned long lastSendTime = 0;

void setup() {
  // put your setup code here, to run once:
  //Wire.begin(4, 5); //i2c sda:4, scl:5 (zusammen mit 14,15) ... useless, as library does this internally without params
  
  Serial.begin(115200);
  initDisplay();
  initEnvSensor();
  initWifi();
  initdhtSensor();
  initmq135Sensor();
}

void loop() {
  // put your main code here, to run repeatedly:
  //read temperature data from BMP, DHT and MQ Sensor
  String temperature = String(envSensor.readTempC(),2);
  String pressure = String(envSensor.readFloatPressure(),2);
  String altitude = String(envSensor.readFloatAltitudeMeters(),2);
  String humidity = String(envSensor.readFloatHumidity(),2);
  String gas = String(gasSensor.getPPM());
  Serial.print("Gassensor RZERO: ");
  Serial.println(gasSensor.getRZero());
  if(dhtSensor.read(D6) != 0){
    Serial.print("DHT Readerror: ");
    Serial.println(dhtSensor.read(D6));
  }

  //Delete Screen and refresh Data
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print("SSID: ");
  display.print(ssid);
  display.print(" sent: ");
  display.print(String(((millis() -lastSendTime)/1000)));
  display.println("s");
  display.println("View: Environment");
  display.print("TempC(BM/DH):");
  display.print(temperature);
  display.print("/");
  display.println(dhtSensor.temperature);
  display.print("Pres: ");
  display.print(pressure);
  display.println(" pa");
  display.print("Alt: ");
  display.print(altitude);
  display.println(" m");
  display.print("Hum(BME/DHT):");
  display.print(humidity);
  display.print("/");
  display.println(dhtSensor.humidity);
  display.print("CO2: ");
  display.print(gas);
  display.println("ppm");
  display.display();
  if((millis() - lastSendTime) > (5 * 60 * 1000)){
    display.println("Sending to thingspeak");
    display.display();
    Serial.print("Temperatur: ");
    Serial.println(temperature);
    updateThingSpeak(temperature, pressure, humidity, gas, String(dhtSensor.temperature), String(dhtSensor.humidity));
    lastSendTime = millis();
  }
  delay(2000);

}

void initDisplay() {
  display.begin(DISPLAY_ADDRESS);
  display.display();
  delay(2000);                //Wait 2 sec for splash screen
  display.clearDisplay();     //Clear the buffer.
  display.setTextSize(1);     //Set Textsize to 1
  display.setTextColor(WHITE);//Standard Color bright Text on dark Background
  display.setCursor(0, 0);    //Put Cursor to top right corner
  display.dim(true);
  display.display();          //Show empty screen (remove splash)
}

void initEnvSensor() {
  //https://learn.sparkfun.com/tutorials/sparkfun-bme280-breakout-hookup-guide
  //actual config
  envSensor.settings.commInterface = I2C_MODE;
  envSensor.settings.I2CAddress = 0x76;
  envSensor.settings.runMode = 3;
  envSensor.settings.tStandby = 0;
  envSensor.settings.filter = 0;
  envSensor.settings.tempOverSample = 1;
  envSensor.settings.pressOverSample = 1;
  envSensor.settings.humidOverSample = 1;
  delay(10);
  //debug stuff for console
  Serial.print("Stating BME280...resultat von .begin(): 0x");
  Serial.println(envSensor.begin(), HEX);
  Serial.print("Displaying ID, reset and ctrl regs\n");
  Serial.print("ID(0xD0): 0x");
  Serial.println(envSensor.readRegister(BME280_CHIP_ID_REG), HEX);
  Serial.print("Reset register(0xE0): 0x");
  Serial.println(envSensor.readRegister(BME280_RST_REG), HEX);
  Serial.print("ctrl_meas(0xF4): 0x");
  Serial.println(envSensor.readRegister(BME280_CTRL_MEAS_REG), HEX);
  Serial.print("ctrl_hum(0xF2): 0x");
  Serial.println(envSensor.readRegister(BME280_CTRL_HUMIDITY_REG), HEX);
}

void initWifi() {
  //https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiClient/WiFiClient.ino
  //also check https!
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);      //WiFi client only
  if(WiFi.status() != WL_CONNECTED){ //workaround for reconnect bug (with quick reset WLAN stays connected)
    WiFi.begin(ssid, password);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initdhtSensor(){
  //nothing to do...
  
}

void initmq135Sensor(){

}

void updateThingSpeak(String temp, String pressure, String humidity, String gas, String dhtTemp, String dhtHum){
//create TCP Connection
WiFiClient client;
const int httpPort = 80;
const char* host = "api.thingspeak.com";
if(!client.connect(host, httpPort)){
  Serial.println("connection failed");
  return;
}

//create uri
String uri = "/update?api_key=**KEY**&field1=";
uri += temp;
uri += "&field2=";
uri += pressure;
uri += "&field3=";
uri += humidity;
uri += "&field4=";
uri += dhtTemp;
uri += "&field5=";
uri += dhtHum;
uri += "&field6=";
uri += gas;
Serial.print("created URI: ");
Serial.print(host);
Serial.println(uri);

//send request = update a field
//do this with POST, bitch!
client.print(String("GET ") + uri + " HTTP/1.1\r\n" +
                    "Host: " + host + "\r\n" +
                    "Connection: close\r\n\r\n");
                    
unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
}
//read server reply
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  Serial.println();
  Serial.println("closing connection");
}
