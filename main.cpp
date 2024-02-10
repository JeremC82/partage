#include <Arduino.h>

#include "WiFi.h"

#include <driver/adc.h>
#include "config/config.h"
#include "config/enums.h"
#include "../include/traduction.h"


//#include <NTPClient.h>


#include <AsyncElegantOTA.h>

// File System
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson

#include "tasks/updateDisplay.h"
#include "tasks/switchDisplay.h"
#include "tasks/fetch-time-from-ntp.h"
#include "tasks/wifi-connection.h"
#include "tasks/measure-electricity.h"
#include "tasks/Dimmer.h"
#include "tasks/autoforce.h"

#include "functions/otaFunctions.h"
#include "functions/spiffsFunctions.h"
#include "functions/Mqtt_http_Functions.h"
#include "functions/webFunctions.h"

//#include "functions/froniusFunction.h"
#include "functions/enphaseFunction.h"
#include "functions/WifiFunctions.h"


// Dallas 18b20
#include <OneWire.h>
#include <DallasTemperature.h>
#include "tasks/dallas.h"
#include "functions/dallasfunction.h"

// Dimmer Library
#include <RBDdimmer.h>   /// the corrected librairy  in RBDDimmer-master-corrected.rar , the original has a bug
#include "functions/dimmerFunction.h"



//***********************************
//************* Afficheur Oled
//***********************************
// Oled
#include "Adafruit_SSD1306.h" /// Oled ( https://github.com/ThingPulse/esp8266-oled-ssd1306 ) 
const int I2C_DISPLAY_ADDRESS = 0x3c;
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // pin 21 SDA - 22 SCL

DisplayValues gDisplayValues;
Config        config; 
Configwifi    configwifi; 
Configmodule  configmodule; 

//WiFiUDP ntpUDP;


// Place to store local measurements before sending them off to AWS
unsigned short measurements[LOCAL_MEASUREMENTS];
unsigned char measureIndex = 0;

//***********************************
//************* Dallas
//***********************************
Dallas dallas[4]; 

/***************************
 *  Dimmer init
 **************************/
dimmerLamp dimmer_hard1(OUTPUTPIN1, ZEROCROSS); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards
dimmerLamp dimmer_hard2(OUTPUTPIN2, ZEROCROSS); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards

void setup()
{
  time_t  t = 0;

  // Always init Serial connection, without DEBUG, it will be only used for startup
  Serial.begin(115200);

  //démarrage file system
  Serial.println("start SPIFFS");
  SPIFFS.begin();


  ///define if AP mode or load configuration
  if (loadwifi(wifi_conf, configwifi)) 
  {
    AP = false; 
  }
  ///  WIFI INIT
  ///// AP WIFI INIT 
  if (AP) 
  {
    APConnect(); 
    gDisplayValues.currentState = UP;
    gDisplayValues.IP = String(WiFi.softAPIP().toString());
    btStop();
  }
  else 
  {
    if (strcmp(WIFI_PASSWORD,"xxx") == 0) 
    { 
      Serial.print("connect to Wifi: ");
      Serial.println(configwifi.SSID);
      Serial.print("Passwd: ");
      Serial.println(configwifi.passwd);
      
      WiFi.begin(configwifi.SSID, configwifi.passwd); 
    }
    else 
    { 
      WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD); 
    }
    
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      Serial.print(".");
    }
    
    serial_println("WiFi connected");
    serial_println("IP address: ");
    serial_println(WiFi.localIP());

    gDisplayValues.currentState = UP;
    gDisplayValues.IP = String(WiFi.localIP().toString());

    btStop();
  }

  // Init OLED
  Serial.println(OLEDSTART);
  if(!display.begin(SSD1306_SWITCHCAPVCC, I2C_DISPLAY_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  //display.init();
  //display.flipScreenVertically();

  display.clearDisplay();

  // Init Dimmer
  Dimmer_setup();

   // vérification de la présence d'index.html
  if (!SPIFFS.exists("/index.html"))
  {
    Serial.println(SPIFFSNO);  
  }

  if (!SPIFFS.exists(filename_conf))
  {
    Serial.println(CONFNO);  
  }

     //***********************************
    //************* Setup -  récupération du fichier de configuration
    //***********************************
  
  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename_conf, config);

  // Init Dallas sensor - Shall be done after config read to affect multiple sensor to the right location
  Serial.println("start 18b20");
  sensors.begin();
  dallaspresent();

  // Initialize Dimmer State 
  gDisplayValues.dimmer = 0;

  //***********************************
	//************* Setup -  demarrage du webserver et affichage de l'oled
	//***********************************
   Serial.println("start Web server");
   call_pages();

  //***********************************
	//************* Setup -  Init NTP service
	//***********************************
  // Configure the Timezone
  Serial.println("start NTP config");
  configTime(0, 0, NTP_SERVER); // 0, 0 because we will use TZ in the next line
  setenv("TZ", TZ, 1); // Set environment variable with your time zone
  tzset();

  // Init the system time according NTP time
  while (t < 1000000)
  {
    t = time(NULL);

  }
  setTime(t);
  Serial.println("Time set");


  // CORE affectation: Loop is by default on Core 1
  // All critical task shall be executed on Core 0
  // All other can be executed on Core 1 (because MQTT is in the Loop)
  // ----------------------------------------------------------------
  // TASK: Connect to WiFi & keep the connection alive.
  // ----------------------------------------------------------------
  if (!AP)
  {
    xTaskCreatePinnedToCore(
      keepWiFiAlive,
      "keepWiFiAlive",  // Task name
      6000,             // Stack size (bytes)
      NULL,             // Parameter
      5,                // Task priority
      NULL,             // Task handle
      LOW_PRIORITY_CORE // Task Core
    );
  }

  // ----------------------------------------------------------------
  // TASK: Update the display every second
  //       This is pinned to the same core as Arduino
  //       because it would otherwise corrupt the OLED
  // ----------------------------------------------------------------
  xTaskCreatePinnedToCore(
    updateDisplay,
    "UpdateDisplay",  // Task name
    5000,            // Stack size (bytes)
    NULL,             // Parameter
    6,                // Task priority
    NULL,             // Task handle
    LOW_PRIORITY_CORE // Task Core
  );

  // ----------------------------------------------------------------
  // Task: Read Dallas Temp
  // ----------------------------------------------------------------
  xTaskCreatePinnedToCore(
    dallasread,
    "Dallas temp",          // Task name
    5000,                   // Stack size (bytes)
    NULL,                   // Parameter
    2,                      // Task priority
    NULL,                   // Task handle
    LOW_PRIORITY_CORE      // Task Core
  );


  // ----------------------------------------------------------------
  // Task: measure electricity consumption ;)
  // ----------------------------------------------------------------
  xTaskCreatePinnedToCore(
    measureElectricity,
    "Measure electricity",  // Task name
    6000,                   // Stack size (bytes)
    NULL,                   // Parameter
    3,                     // Task priority
    NULL,                   // Task handle
    HIGH_PRIORITY_CORE      // Task Core
  );

  // ----------------------------------------------------------------
  // Task: Update Dimmer power
  // ----------------------------------------------------------------
  xTaskCreatePinnedToCore(
    updateDimmer,
    "Update Dimmer",        // Task name
    5000,                   // Stack size (bytes)
    NULL,                   // Parameter
    4,                      // Task priority
    NULL,                   // Task handle
    HIGH_PRIORITY_CORE      // Task Core
  );

  // ----------------------------------------------------------------
  // Task: Auto Force maangement
  // ----------------------------------------------------------------
  xTaskCreatePinnedToCore(
    updateAutoForce,
    "Update Auto Force",    // Task name
    5000,                   // Stack size (bytes)
    NULL,                   // Parameter
    4,                      // Task priority
    NULL,                   // Task handle
    LOW_PRIORITY_CORE       // Task Core
  );

  // ----------------------------------------------------------------
  // TASK: update time from NTP server.
  // ----------------------------------------------------------------
  if (!AP) 
  {
      xTaskCreatePinnedToCore(
        fetchTimeFromNTP,
        "Update NTP time",// Task name  
        5000,             // Stack size (bytes)
        NULL,             // Parameter
        2,                // Task priority
        NULL,             // Task handle
        LOW_PRIORITY_CORE // Task Core
      );
  }

  AsyncElegantOTA.begin(&server);
  server.begin(); 



}

void loop()
{

  vTaskDelay(40000 / portTICK_PERIOD_MS);

}
