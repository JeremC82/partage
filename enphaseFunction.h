#ifndef ENPHASE_FUNCTIONS
#define ENPHASE_FUNCTIONS

#include <Arduino.h>
#include <FS.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"
#include "SPIFFS.h"
#include "config/enums.h"

extern Config config; 

String SessionId;
HTTPClient https;
bool initEnphase = true; // Permet de lancer le contrôle du token une fois au démarrage

bool Enphase_get_7_Production(void)
{
  
  int httpCode;
  bool retour = false;
  String adr = String(config.enphaseip);
  String url = "/404.html" ;

  url = String(EnvoyS);
  Serial.print("type S ");
  Serial.println(url);
          
  Serial.println("Enphase Get production : https://" + adr + url);
  if (https.begin("http://" + adr + url)) 
  { 
    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    https.setAuthorizationType("Bearer");
    https.setAuthorization(config.enphasetoken);
    https.addHeader("Accept-Encoding","gzip, deflate, br");
    https.addHeader("User-Agent","PvRouter/1.1.1");
    if (!SessionId.isEmpty()) 
    {
      https.addHeader("Cookie",SessionId);
    }
    httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
    {
      String payload = https.getString();
      //Serial.println(payload);
      DynamicJsonDocument doc(3072);
      deserializeJson(doc, payload);

      gDisplayValues.production = int(doc["production"][1]["wNow"]);
      gDisplayValues.watt = int(doc["consumption"][1]["wNow"]);
      gDisplayValues.whToday = int(doc["production"][1]["whToday"]);

      retour = true;
      // debug
      Serial.println("Enphase Get production > prod: " + String(gDisplayValues.production) + " conso: " + String(gDisplayValues.watt) + " total prod today: " + String(gDisplayValues.whToday));
    } 
    else 
    {
      Serial.println("[Enphase Get production] GET... failed, error: " + String(httpCode));
    }
    //https.end();
  }
  else 
  {
    Serial.println("[Enphase Get production] GET... failed, error: " + String(httpCode));
  }
  https.end();
  return retour;
}

bool Enphase_get_7_JWT(void) 
{
  bool retour = false;
  String url = "/404.html";
  url = String(EnvoyJ);
  String adr = String(config.enphaseip);


  Serial.println("Enphase contrôle token : https://" + adr + url);
  //Initializing an HTTPS communication using the secure client
  if (https.begin("https://" + adr + url)) 
  {
    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    https.setAuthorizationType("Bearer");
    https.setAuthorization(config.enphasetoken);
    https.addHeader("Accept-Encoding","gzip, deflate, br");
    https.addHeader("User-Agent","PvRouter/1.1.1");
    const char * headerkeys[] = {"Set-Cookie"};
    https.collectHeaders(headerkeys, sizeof(headerkeys)/sizeof(char*));
    int httpCode = https.GET();
    
    // httpCode will be negative on error
    //Serial.println("Enphase_Get_7 : httpcode : " + httpCode);
    if (httpCode > 0) 
    {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
      {
        retour = true;
        // Token valide
        Serial.println("Enphase contrôle token : TOKEN VALIDE ");
        config.badtoken = false;
        SessionId.clear();
        SessionId = https.header("Set-Cookie");
      } 
      else 
      {
          Serial.println("Enphase contrôle token : TOKEN INVALIDE !!!");
          config.badtoken = true;
          config.countbadtoken++;
          Serial.print("nombre echec token = ");
          Serial.println(config.countbadtoken);
          https.end();

          if(config.countbadtoken >= 3)
          {
            config.renewtoken = true;


          }

          if(config.countbadtoken <= 0)
          {
            config.renewtoken = false;
          }


      }
    }
  }
  https.end();// Modification

  return retour;
}

void Enphase_get(void) 
{
  if(WiFi.isConnected() ) 
  {
    //create an HTTPClient instance
    if (SessionId.isEmpty() || Enphase_get_7_Production() == false) 
    { // Permet de lancer le contrôle du token une fois au démarrage (Empty SessionId)
      SessionId.clear();
      Enphase_get_7_JWT();
    }
  } 
  else 
  {
    Serial.println("Enphase version 7 : ERROR");
  } 
}

#endif
