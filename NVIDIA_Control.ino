/*Change the port to a random (free) port for security.*/
const int webSocketPort = 2339;

//Websocket Initialization
#include <Arduino.h>
#include <WiFiManager.h>
//WebSocket Library - https://github.com/Links2004/arduinoWebSockets
#include <WebSocketsServer.h>

WiFiManager wm;
WebSocketsServer webSocket = WebSocketsServer(webSocketPort);

//JSON Parsing and Key Conversion
//Arduino_JSON library by Arduino
#include <Arduino_JSON.h>
//int playerpause = 2;
//int oldplayerpause
/* Assume media is playing to begin with */
bool playing = true;
uint16_t counter = 1;

#include "USB.h"
#include "USBHIDKeyboard.h"
USBHIDKeyboard Keyboard;
#include "USBHIDConsumerControl.h"
USBHIDConsumerControl MediaControl;
#define MEDIA_KEY 0
#define KEYBOARD_KEY 1

/*  Struct to Hold Each Element of the JsonToUSB_HID_Map */
struct JSONMethodToCecType {
  char JSONMethod[24];
  char JSONAction[14];
  bool KeyboardAction;
  uint8_t USBHID;
};

struct IPAddressFail {
  int FailedAttempts;
  time_t lastFailure;
};

/* Try to block users who are trying to brute force a connection */
IPAddressFail IPAddressFailures[254];

/*  JsonToUSB_HID_Map */
const JSONMethodToCecType JSONMethodToCec[] = {
  {"Input.Select", "", KEYBOARD_KEY, KEY_RETURN},
  {"Input.Left", "", KEYBOARD_KEY, KEY_LEFT_ARROW},
  {"Input.Right", "", KEYBOARD_KEY, KEY_RIGHT_ARROW},
  {"Input.Up", "", KEYBOARD_KEY, KEY_UP_ARROW},
  {"Input.Down", "", KEYBOARD_KEY, KEY_DOWN_ARROW},
  {"Input.ShowOSD", "", KEYBOARD_KEY, 0x98},
  {"Input.Info", "", MEDIA_KEY, 0},
  {"Input.Back", "", KEYBOARD_KEY, KEY_ESC},
  {"Input.Home", "", KEYBOARD_KEY, KEY_ESC},
  {"Input.SendText", "", KEYBOARD_KEY, 0},
  {"VideoLibrary.Scan", "", MEDIA_KEY, 0},
  {"Input.ContextMenu", "", MEDIA_KEY, 0},
  {"Player.GetActivePlayers", "", MEDIA_KEY, 0},
  /* Pause - the shield does not require this, but a normal PC or perhaps other Android TV do*/
  {"Player.PlayPause", "1", MEDIA_KEY, 0x00B1},
  /* Play */
  {"Player.PlayPause", "2", MEDIA_KEY, CONSUMER_CONTROL_PLAY_PAUSE},
  {"Player.Stop", "", MEDIA_KEY, CONSUMER_CONTROL_STOP},
  {"Input.ExecuteAction", "play", MEDIA_KEY, CONSUMER_CONTROL_PLAY_PAUSE},
  {"Input.ExecuteAction", "mute", MEDIA_KEY, CONSUMER_CONTROL_MUTE},
  {"Input.ExecuteAction", "stepback", MEDIA_KEY, 0x00B4},
  {"Input.ExecuteAction", "stepforward", MEDIA_KEY, 0x00B3},
  {"Input.ExecuteAction", "skipprevious", MEDIA_KEY, CONSUMER_CONTROL_STOP},
  {"Input.ExecuteAction", "skipnext", MEDIA_KEY, CONSUMER_CONTROL_SCAN_NEXT},
  {"Input.ExecuteAction", "volumeup", MEDIA_KEY, CONSUMER_CONTROL_VOLUME_INCREMENT},
  {"Input.ExecuteAction", "volumedown", MEDIA_KEY, CONSUMER_CONTROL_VOLUME_DECREMENT}
};

void keyboard_control(uint8_t Key, const char* JSONMethod) {
  Serial.print("Sending key code :");
  Serial.println(Key);
  if (strcmp(JSONMethod, "Input.Home") == 0) {
    Keyboard.press(KEY_LEFT_CTRL);
  }
  Keyboard.press(Key);
  delay(100);  
  Keyboard.releaseAll();
}

void keyboard_write_control(const uint8_t* KeysToEnter, const char* JSONMethod) {
  Serial.print("Sending key string :");
  Serial.println((const char *)KeysToEnter);
  Keyboard.write(KeysToEnter, sizeof(KeysToEnter));
  delay(100);  
  Keyboard.write(KEY_RETURN);
}

void media_control(uint8_t Key) {
  Serial.print("Sending media code :");
  Serial.println(Key);
  MediaControl.press(Key);
  delay(10);
  MediaControl.release();
}

void processUSBHID(JSONVar JSONMethod, const char* JSONAction) {
  int JSONMethodToCecLength = 24;
  for (int i = 0; i < JSONMethodToCecLength; i++) {
    if ((strcmp(JSONMethodToCec[i].JSONMethod, (const char *)JSONMethod) == 0) && ((strcmp(JSONMethodToCec[i].JSONAction, (const char *)JSONAction) == 0) || (strcmp(JSONMethod,"Input.SendText") == 0))) {
      Serial.print("Incoming JSON Request :");
      Serial.println((const char *)JSONMethod);
      if (strcmp(JSONMethod,"Input.SendText") == 0) {
        keyboard_write_control((uint8_t *)JSONAction, JSONMethod);
      } else if (JSONMethodToCec[i].KeyboardAction == KEYBOARD_KEY) {
        keyboard_control(JSONMethodToCec[i].USBHID, (const char *)JSONMethod);
      } else {
        media_control(JSONMethodToCec[i].USBHID);
      }
      return;
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WEBSOCKET: [%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("WEBSOCKET: [%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        /* A simple system to block IP Addresses that fail connecting 5 times for a 5 minute period */
        int IPAddressPosition = ip[3]-1;
        if ((IPAddressPosition>=0) && (IPAddressPosition<253)) {
          int FailedAttempts = IPAddressFailures[IPAddressPosition].FailedAttempts;
          if (FailedAttempts<5) {
            if (strcmp((const char *)payload, "/jsonrpc") != 0) {
              Serial.println("WEBSOCKET: Client has not connected to the correct path, disconnecting...");
              Serial.printf("IP Address %d.%d.%d.%d has failed connecting\n",ip[0], ip[1], ip[2], ip[3]);
  
              IPAddressFailures[IPAddressPosition].lastFailure = time(nullptr);
              IPAddressFailures[IPAddressPosition].FailedAttempts=FailedAttempts+1;
              if ((FailedAttempts+1)==5) {
                Serial.printf("IP Address %d.%d.%d.%d has failed connecting 5 times, it will now be blocked for 300 seconds\n",ip[0], ip[1], ip[2], ip[3]);
              } 
              webSocket.disconnect(num);
            }        
          } else {
            Serial.println("Disconnecting client due to being banned");
            if ((IPAddressFailures[IPAddressPosition].lastFailure+300)<time(nullptr)) {
              Serial.println("It has now been over five minutes, disabling client block...");
              IPAddressFailures[IPAddressPosition].FailedAttempts = 0;
            }
            webSocket.disconnect(num);
          }
        }
      }
      break;
    case WStype_TEXT:
      {
        unsigned long startTime;
        startTime = millis();

        Serial.printf("WEBSOCKET: [%u] get Text: %s\n", num, payload);
        Serial.printf("Counter: %d\n",counter);
        
        counter = counter + 1;
        /*  I want to avoid filling up stack space, and this is only needed to test latency */
        if (counter==10000) {
          counter=1;
        }
        JSONVar myObject = JSON.parse((const char *)payload);

        if (strcmp(myObject["method"], "Player.GetActivePlayers") == 0) {
          webSocket.sendTXT(num, "{\"id\": 1, \"jsonrpc\": \"2.0\", \"result\": [ { \"playerid\": 1, \"type\": \"video\" } ]}");
          return;
        } else {
          webSocket.sendTXT(num, "{\"id\":1,\"jsonrpc\":\"2.0\",\"result\":\"OK\"}");
        }

        if (JSON.typeof(myObject) == "undefined") {
          Serial.println("Parsing input failed!");
          return;
        }
        if (!myObject.hasOwnProperty("method")) {
          Serial.println("JSON parse cannot find method");
          return;
        }
        Serial.println("Correctly parsed JSON");
        if (strcmp(myObject["method"], "Player.PlayPause") == 0) {
          /* Leaving this here, it may be essential for a PC or another Android TV Device 
          if (playing==true) {
            processUSBHID(myObject["method"], "1");
            playing = false;
          } else {
            processUSBHID(myObject["method"], "2");            
            playing = true;
          }          
          */
          processUSBHID(myObject["method"], "2");            
        } else if (strcmp(myObject["method"], "Input.ExecuteAction") == 0) {
          if (!((myObject.hasOwnProperty("params")) && (myObject["params"].hasOwnProperty("action")))) {
            Serial.println("JSON parse cannot find params or action and is required for Input.ExecuteAction");
            return;
          }
          processUSBHID(myObject["method"], myObject["params"]["action"]);
        } else if (strcmp(myObject["method"], "Input.SendText") == 0) {          
          if (!((myObject.hasOwnProperty("params")) && (myObject["params"].hasOwnProperty("text")))) {
            Serial.println("JSON parse cannot find params or text and is required for Input.SendText");
            return;
          }
          processUSBHID(myObject["method"], myObject["params"]["text"]);
        } else {
          processUSBHID(myObject["method"], "");
        }
        Serial.printf("Function time was %d\n",millis() - startTime);
        if (strcmp(myObject["method"], "Player.Stop")==0) {
          /* As the current paying media is stopped, assuming next media state is playing */
          playing = true;
        }
        break;
      }
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }

}

void setup() {
  Serial.begin(115200);
  wm.setTitle("Wifi Configuration");
  /* Populate the list of struct array with IP addresses for a /24 subnet this will need to be modified if you are on something else */
  for (int i=0;i<253;i++) {
    IPAddressFailures[i].FailedAttempts = 0;
  }
  
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("WEBSOCKET: [SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  //wm.resetSettings();
  wm.setConfigPortalTimeout(500);

  if (!wm.autoConnect("Shield-Control-Device","cCV$OSVd1cKU")){
    Serial.println("Unable to be configured or connect to AP, restarting");
    ESP.restart();
    delay(1000);
  }
  Serial.println("Wifi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  //xTaskCreate(keyTaskServer, "server", 20000, NULL, 5, NULL);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  /* Setup the USB HID instances */
  MediaControl.begin();
  Keyboard.begin();  
  USB.productName("TEST");
  USB.manufacturerName("TESTICLES");
  USB.begin();
  Serial.println("Setup Completed");
}

void loop() {
  webSocket.loop();
  wm.process();
}
