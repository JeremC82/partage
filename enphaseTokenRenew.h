#ifndef ENPHASE_TOKEN_RENEW
#define ENPHASE_TOKEN_RENEW

#include <Arduino.h>
#include <FS.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"
#include "SPIFFS.h"
#include "config/enums.h"

extern Config config; 






#define user "test@gmail.com"
#define passwordEnphase "password"
const char* envoySerial = "100145076996";

const char* url = "https://enlighten.enphaseenergy.com/login/login.json?";
const char* data = "user[email]="  user  "&user[password]=" passwordEnphase;

void authenticateEnphase() {



  //SessionId.clear();
  Serial.print("voici l'url=");
  Serial.println(url);

  Serial.print("voici les data=");
  Serial.println(data);


  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure(); // pour le moment on peut faire comme cela


  http.begin(client, url); 
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpResponseCode = http.POST(data);

    Serial.print("voici le httpResponseCode =");
    Serial.println(httpResponseCode);


  if (httpResponseCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    const char* sessionId = doc["session_id"];      // Récupération de la session_id

    // Construction des données pour la deuxième requête POST
    String urlTokens = "https://entrez.enphaseenergy.com/tokens";
    String dataTokens = "{\"session_id\":\"" + String(sessionId) + "\",\"serial_num\":\"" + String(envoySerial) + "\",\"username\":\"" + String(user) + "\"}";

    // Envoi de la deuxième requête POST
    http.begin(client, urlTokens); // Utilisation de WiFiClientSecure avec l'URL HTTPS
    http.addHeader("Content-Type", "application/json");
    httpResponseCode = http.POST(dataTokens);

    if (httpResponseCode > 0) {
      String tokenRaw = http.getString();


      const char *text2 = tokenRaw.c_str();





      Serial.print("Token : ");
      Serial.println(tokenRaw);

      Serial.print("Token char array text2 : ");
      Serial.println((char*)text2);


      
      
      
    } else {
      Serial.println("Échec de la requête pour obtenir le token");
      config.countbadtoken = 0;
      config.renewtoken = false;
      
    }

    http.end();
  } else {
    Serial.println("Échec de la requête d'authentification Enphase");
    config.countbadtoken = 0;
    config.renewtoken = false;
  }
  http.end();
}




      //SessionId.clear();


#endif
