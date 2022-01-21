#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WifiClient.h>
#include <uptime_formatter.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define LED 5
#define RELAY 4

// PIN Number to control system
      int pin = 0000;

// What the system thinks the door state is
      bool  doorOpen = false;
// What our sensor actually says
      bool  sensorDoorState = false;
// Is the relay activated
      bool relaySwitched = false;
// Checking relay activation duration
      unsigned long relayLastSwitch = 0;
// The last time an operation occured from the web  
      unsigned long lastWebOperation = 0;
      
ESP8266WebServer server(80);

void activateRelay() {
  relayLastSwitch = millis();
  relaySwitched = true;
  digitalWrite(RELAY, HIGH);
}

void checkRelay() {
  if(relaySwitched) {
    if((relayLastSwitch + 2000) < millis()) {
      relaySwitched = false;
      digitalWrite(RELAY, LOW);
    }
  }
}

void doorLED() {
  if(doorOpen) {
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(LED, LOW);
  }
}

void webUptime() {
  server.send(200, "text/html", uptime_formatter::getUptime());
}

void webPageNotFound() {
  server.send(404, "text/html", "404: Not Found");
}

bool rateLimitWeb(int seconds) {
  long milliseconds = (seconds * 1000);
  if((lastWebOperation + milliseconds) < millis()) {
    return true;
  } else {
    return false;
  }
}


String loadSsid() {
  if(LittleFS.exists("/settings.json")) {
  File settings = LittleFS.open("/settings.json", "r");
  DynamicJsonDocument jsonData(256);
  deserializeJson(jsonData, settings.readString());
  return jsonData["ssid"];
  } else {
    return "none";
  }
}

String loadKey() {
  if(LittleFS.exists("/settings.json")) {
  File settings = LittleFS.open("/settings.json", "r");
  DynamicJsonDocument jsonData(256);
  deserializeJson(jsonData, settings.readString());
  return jsonData["key"];
  } else {
    return "none";
  }
}

void loadPin() {
  if(LittleFS.exists("/settings.json")) {
  File settings = LittleFS.open("/settings.json", "r");
  DynamicJsonDocument jsonData(256);
  deserializeJson(jsonData, settings.readString());
  pin = jsonData["pin"].as<int16_t>();
  } else {
    pin = 0000;
  }
}


int verifySentPin() {
  if(server.hasArg("plain")) {
    if(server.arg("plain").length() <= 28) {
      DynamicJsonDocument jsonData(32);
      deserializeJson(jsonData, server.arg("plain"));
      const int sentPin = jsonData["pin"].as<int16_t>();
      return sentPin;
    }
  } else {
    return 0000;
  }
}

void webOpenDoor() {
  Serial.println("Door open requested");
  if(rateLimitWeb(10)) {  
   if(verifySentPin() == pin) {
   if(doorOpen) {
      // Door already open
     server.send(200, "application/json", "{\"query\" : \"open\", \"result\":\"failed\", \"reason\" : \"invalid\"}");
    } else {
     // Door now opening
     server.send(200, "application/json", "{\"query\" : \"open\", \"result\":\"success\", \"reason\" : \"opening\"}");
     doorOpen = true;
     activateRelay();
   }
      } else {
       server.send(200, "application/json", "{\"query\" : \"open\", \"result\":\"failed\", \"reason\" : \"invalid-pin\"}");
      }    
  } else {
     server.send(200, "application/json", "{\"query\" : \"open\", \"result\":\"failed\", \"reason\" : \"ratelimited\"}");
  }
}

void webCloseDoor() {
    Serial.println("Door close requested");
    if(rateLimitWeb(10)) {
      if(verifySentPin() == pin) {  
    lastWebOperation = millis();
      if(doorOpen) {
       // Door now closing
       server.send(200, "application/json", "{\"query\" : \"close\", \"result\":\"success\", \"reason\" : \"closing\"}");
        // TODO: Change to magnetic contact checking
        doorOpen = false;
        activateRelay();
      } else {
       // Door already closed
        server.send(200, "application/json", "{\"query\" : \"close\", \"result\":\"failed\", \"reason\" : \"invalid\"}");
     }
      } else {
     server.send(200, "application/json", "{\"query\" : \"open\", \"result\":\"failed\", \"reason\" : \"invalid-pin\"}");
      }
    } else {
     server.send(200, "application/json", "{\"query\" : \"close\", \"result\":\"failed\", \"reason\" : \"ratelimited\"}");
    }
}

void webStatusDoor() {
  Serial.println("Door status requested");

    if(doorOpen) {
        server.send(200, "application/json", "{\"query\" : \"status\", \"result\":\"success\", \"reason\" : \"open\"}");
    } else {
        server.send(200, "application/json", "{\"query\" : \"status\", \"result\":\"success\", \"reason\" : \"closed\"}");
    }
}



void setup() {
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);

  Serial.begin(115200);
  delay(5);
  Serial.println('\n');

  if(!LittleFS.begin()) {
    Serial.println("Error enabling file system");
    return;
  }

  WiFi.begin(loadSsid(), loadKey());

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  Serial.println("System starting...");
  Serial.println(WiFi.localIP());
  loadPin();
  server.serveStatic("/", LittleFS, "/web/index.html");

  server.on("/open/", webOpenDoor);

  server.on("/close/", webCloseDoor);

  server.on("/status/", webStatusDoor);

  server.on("/uptime/", webUptime);
  
  server.onNotFound(webPageNotFound);


  server.begin();
}

void loop() {
  doorLED();
  checkRelay();
  server.handleClient();
}
