// ESP32 Xiaomi Body Scale to MQTT v0.0.1 (MQTT)

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Your Mi Scale BLE mac adress (in lowercase)
#define scale_mac_addr "ff:ff:ff:ff:ff:ff"

// pin connected to piezo buzzer
const int BUZZZER_PIN = 32; // GIOP32

// pin connected to led
const int LED_PIN = 2;

// network details
const char* ssid = "<your wifi ssid here>";
const char* password = "<your wifi password>";

// This ESP32's IP. Use a static IP so we shave off a bunch of time connecting to wifi
IPAddress ip(192, 168, 0, 100);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// MQTT Details
const char* mqtt_server = "192.168.0.2";
const int mqtt_port = 1883;
const char* mqtt_userName = "<your mqtt login>";
const char* mqtt_userPass = "<your mqtt password>";
const char* clientId = "esp32_";

// If you change these, you should also update the corresponding topics in the appdaemon app
String base_topic = "scale";
const char* mqtt_command = "cmnd/";
const char* mqtt_stat = "stat/";
const char* mqtt_attributes = "/attributes";
const char* mqtt_telemetry = "tele/";
const char* mqtt_tele_status = "/status";

String mqtt_clientId = String( clientId + base_topic );  // esp32_scale
String mqtt_topic_subscribe = String( mqtt_command + base_topic); // cmnd/scale
String mqtt_topic_telemetry = String( mqtt_telemetry + base_topic + mqtt_tele_status ); // tele/scale/status
String mqtt_topic_attributes = String( mqtt_stat + base_topic + mqtt_attributes ); // stat/scale/attributes

uint32_t unMillis = 1000;

uint8_t unNoImpedanceCount = 0;

String publish_data;
String last_time;

BLEAdvertisedDevice foundDevice;

void beepWithLed()
{
  digitalWrite (BUZZZER_PIN, HIGH); //turn buzzer on
  digitalWrite (LED_PIN, HIGH);
  
  delay(0.1 * unMillis);
  
  digitalWrite (BUZZZER_PIN, LOW);  //turn buzzer off
  digitalWrite (LED_PIN, LOW);

  delay(0.1 * unMillis);
}

void setup()
{
  // Initializing serial port for debugging purposes
  Serial.begin(115200);

  pinMode(BUZZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  beepWithLed();
}

int16_t stoi( String input, uint16_t index1 )
{
    return (int16_t)( strtol( input.substring( index1, index1+2 ).c_str(), NULL, 16) );
}

int16_t stoi2( String input, uint16_t index1 )
{
    Serial.print( "Substring: " );
    Serial.println( (input.substring( index1+2, index1+4 ) + input.substring( index1, index1+2 )).c_str() );
    return (int16_t)( strtol( (input.substring( index1+2, index1+4 ) + input.substring( index1, index1+2 )).c_str(), NULL, 16) );
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.printf("Near mac address: %s \n", advertisedDevice.getAddress().toString().c_str());
      
    if ( advertisedDevice.getAddress().toString() == scale_mac_addr ) {
      Serial.println("Found!");
      
      foundDevice = advertisedDevice;
      
      Serial.printf("Found device: %s \n", foundDevice.getAddress().toString().c_str());

      //delay( 2 * unMillis );
      
      BLEDevice::getScan()->stop();
    }
  }
};

void sConnect()
{
  Serial.println("Connect");
  int nFailCount = 0;
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.config(ip, gateway, subnet); 
    WiFi.begin(ssid, password);
    WiFi.config(ip, gateway, subnet); 
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      Serial.print(".");
      nFailCount++;
      if (nFailCount > 1500) {
        // Why can't we connect?  Let's not get stuck forever trying...
        // Just reboot and start fresh.
        esp_restart();
      }
    }
    
    Serial.println("");
    Serial.println("WiFi connected");    
  }
  
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");

    mqtt_client.setServer(mqtt_server, mqtt_port);

    // Attempt to connect
    if (mqtt_client.connect(mqtt_clientId.c_str(), mqtt_userName, mqtt_userPass)) {
      Serial.println("MQTT connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println("try again in 200 milliseconds");
      
      delay(200);
      
      nFailCount++;
      if (nFailCount > 500) {
        esp_restart();      
      }
    }
  }
}

void sendMQTT()
{
  if (!mqtt_client.connected()) {
    Serial.println("Attemping Connect.");
    sConnect();
  }
  
  mqtt_client.publish(mqtt_topic_attributes.c_str(), publish_data.c_str(), true );
  
  Serial.print("Publishing: ");
  Serial.println(publish_data.c_str());
  Serial.print("to : ");
  Serial.println(mqtt_topic_attributes.c_str());

  beepWithLed();
  
  delay(5 * unMillis);
}

void scanBLE()
{
  Serial.println("Starting BLE Scan");

  BLEDevice::init("");

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);
  
  BLEScanResults foundDevices = pBLEScan->start(10);

  if (foundDevice.getAddress().toString() == scale_mac_addr) {
    Serial.printf("Use device: %s \n", foundDevice.getAddress().toString().c_str());

    String hex;
    
    if (foundDevice.haveServiceData()) {
      std::string md = foundDevice.getServiceData();
      uint8_t* mdp = (uint8_t*)foundDevice.getServiceData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      hex = pHex;
      Serial.println(hex);
      Serial.println(pHex);
      Serial.println(stoi2( hex, 22 ));    
      free(pHex);
    }
    
    float weight = stoi2( hex, 22 ) * 0.01f / 2;
    float impedance = stoi2( hex, 18 ) * 0.01f * 100;

    if (unNoImpedanceCount < 5 && impedance <= 0) {
      // Got a reading, but it's missing the impedance value
      // We may have gotten the scan in between the weight measurement
      // and the impedance reading or there may just not be a reading...
      // We'll try a few more times to get a good read before we publish 
      // what we've got.
      unNoImpedanceCount++;
      Serial.println("Reading incomplete, reattempting");
      return;
    }

    unNoImpedanceCount = 0;

    int user = stoi( hex, 6 );
    int units = stoi( hex, 0 );
    String strUnits;
    if ( units == 1 )
      strUnits = "jin";
    else if ( units == 2 )
      strUnits = "kg";
    else if ( units == 3 )
      strUnits = "lbs";      

    String Year = String(stoi2(hex, 4));
    
    int Month = stoi(hex, 8);
    String MonthStr = String(Month);
    if (Month < 10) { MonthStr = "0" + MonthStr; }
    
    int Day = stoi(hex, 10);
    String DayStr = String(Day);
    if (Day < 10) { DayStr = "0" + DayStr; }

    int Hours = stoi(hex, 12);
    String HoursStr = String(Hours);
    if (Hours < 10) { HoursStr = "0" + HoursStr; }

    int Minutes = stoi(hex, 14);
    String MinutesStr = String(Minutes);
    if (Minutes < 10) { MinutesStr = "0" + MinutesStr; }

    int Seconds = stoi(hex, 16);
    String SecondsStr = String(Seconds);
    if (Seconds < 10) { SecondsStr = "0" + SecondsStr; }
    
    String time = HoursStr + ":" + MinutesStr + ":" + SecondsStr;
    
    String timestamp = String(Year + "-" + MonthStr + "-" + DayStr + "T" + time + ".000Z");

    // Currently we just send the raw values over and let appdaemon figure out the rest...
    if (weight > 0) {
      publish_data  = String("{\"Weight\":\"");
      publish_data += String( weight );
      publish_data += String("\", \"Impedance\":\"");  
      publish_data += String( impedance );       
      publish_data += String("\", \"Units\":\"");  
      publish_data += String( strUnits );
      publish_data += String("\", \"User\":\"");  
      publish_data += String( user );      
      publish_data += String("\", \"Timestamp\":\"");    
      publish_data += timestamp;                    
      publish_data += String("\"}");
    }

    if (last_time != timestamp) {
      Serial.println("New time, need send data");
      Serial.println(publish_data);

      sendMQTT();
      
      last_time = timestamp;
    }
  }

  Serial.println("Finish BLE Scan");
}

void loop()
{
  Serial.println("=== loop ===");
  scanBLE();

  uint64_t time = millis();

  // restart after 12 hours in work
  if (time > (12 * 60 * 60 * unMillis)) {
    Serial.println("Doing Periodic Restart");
    delay(1 * unMillis);
    esp_restart();
  }
  
  delay(5 * unMillis);
}
