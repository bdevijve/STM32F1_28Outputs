#define SERIALDEBUG1    // Comment to remove serial debugging info level 1
//#define SERIALDEBUG2    // Comment to remove serial debugging info level 2
//#define SERIALDEBUG9    // Comment to remove serial debugging info level 9
#define SELF_TEST       // teste toutes les sorties au démarrage
#define KEEP_BUG        // ne corrige pas le bug des channels 29 et +

/*
 * Reste à faire :  - Faire flasher Output[0] chaque seconde pendant la boucle loop si tout est OK      DONE
 *                  - HTTP Publish Output                                                               DONE 
 *                  - Watchdog                                                                          DONE
 *                  - Vérifier memory leak                                                              DONE
 *                  - HTTP Publish Uptime                                                               DONE
 *                  - HTTP Publish FreeMemory                                                           DONE
 *                  - Mettre tous les GPIO en INPUT_PULLUP                                              DONE
 *                  - Nettoyage profond de la fonction d'accès à l'EEPROM et de la structure Settings   DONE
 *                  - MQTT Publish Uptime                                                               En Cours
 *                  - MQTT Publish FreeMemory                                                           En Cours
 *                  - Faire flasher Output[0] rapidement tant que l'init n'est pas terminé
 *                  - HTTP set Output                                                                   En Cours
 *                  - DHCP Hostname & Freebox issue             
 *                  - Activer/désactiver la configuration HTTP depuis une commande MQTT
 *                  - Vérifier persistance des sorties après reboot (MQTT persistance)
 */

#include <Ethernet.h>

#include <EEPROM.h>
#include <PubSubClient.h>     // Library from https://github.com/knolleary/pubsubclient (Standard on the Arduino IDE Library Manager)
#include <ArduinoUniqueID.h>  // Library from https://github.com/ricaun/ArduinoUniqueID (Standard on the Arduino IDE Library Manager)
#include <IWatchdog.h>        // Library included in Arduino_Core_STM32 version > 1.3.0

#define STRING_LEN 30
#define OUTPUT_COUNT 28       // Nb de sortie externe réel, sachant que la LED interne d'état sera comptabilisé en +

bool IWatchdog_isReset=false;
long BlinkStatus = 0;                            // Use to make Blink Status LED once each second

uint16_t GPIOINIT_TABLE[] = {
  PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10, PA11, PA12, PA13, PA14, PA15,
  PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, PB8, PB9, PB10, PB11, PB12, PB13, PB14, PB15,
  PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13, PC14, PC15,
  PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7, PD8, PD9, PD10, PD11, PD12, PD13, PD14, PD15,
  PE0, PE1, PE2, PE3, PE4, PE5, PE6, PE7, PE8, PE9, PE10, PE11, PE12, PE13, PE14, PE15,
  0xffff
};

uint8_t GPIO_OUTPUT[] = {
  PC13, PE12, PE13, PE14, PB8, PA0, PA1, PA2, PA3, PB9, PB0, PB1, PE8, PE9, PE10, PE11, PB4, PB3, PA15, PB10, PB11, PD12, PD13, PD14, PD15, PC6, PC7, PC8, PC9
};

byte mac[] = {0x00, 0x80, 0xE1, 0x03, 0x04, 0x05} ; 

#define ETH_CS_PIN PA4
#define STATUS_PIN PC13

EthernetServer HTTPEthernetServer(80);
EthernetClient MqttEthernetClient;

bool needReset = false;
uint8_t isON [] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} ;

#include "MQTT.h"
#include "HTTP.h"

void setup() {

  
  { // initialize ALL GPIO pins to INPUT_PULLUP in order to limit interrupt on floating PINs
    int pinNumber = 0;
    while ( GPIOINIT_TABLE[pinNumber] != 0xffff ) pinMode(GPIOINIT_TABLE[pinNumber++], INPUT_PULLUP);
  }
//    for (int ; pinNumber < 80; pinNumber++) pinMode(GPIOINIT_TABLE[pinNumber], INPUT_PULLUP);
  
  Serial.begin(115200);
  IWatchdog_isReset=IWatchdog.isReset(true);

  if ( IWatchdog_isReset ) Serial.println("Card was restarted by Watchdog !!");

  #ifdef SERIALDEBUG1
    { long now = millis();Serial.print(now); }
    Serial.println(" - Begin of SETUP !!!");
    Serial.print("UniqueID  - ");UniqueIDdump(Serial);
    Serial.print("UniqueID8 - ");UniqueID8dump(Serial);
  #endif


  // initialize output pins
  for (int pinNumber = 0; pinNumber < OUTPUT_COUNT+1; pinNumber++) {
    pinMode(GPIO_OUTPUT[pinNumber], OUTPUT);
    digitalWrite(GPIO_OUTPUT[pinNumber], LOW);
	}


  #ifdef SELF_TEST
    for (int i=0; i<OUTPUT_COUNT+1 ; i++){ digitalWrite(GPIO_OUTPUT[i] , 1);delay(10);digitalWrite(GPIO_OUTPUT[i], 0); }
    for (int i=OUTPUT_COUNT; i>=0; i--)  { digitalWrite(GPIO_OUTPUT[i] , 1);delay(10);digitalWrite(GPIO_OUTPUT[i], 0); }
  #endif


//// MAKE BLINK FAST HERE


  
  mac[0] = 0x00; // UniqueID8[2]; 
  mac[1] = 0x80; // UniqueID8[3]; 
  mac[2] = 0xE1; // UniqueID8[4]; 
  mac[3] = UniqueID8[5]; 
  mac[4] = UniqueID8[6]; 
  mac[5] = UniqueID8[7]; 

  #ifdef SERIALDEBUG1
    Serial.print("MAC :");
    for (byte octet = 0; octet < 6; octet++) {
      Serial.print(mac[octet], HEX);
      if (octet < 5) Serial.print('-');
      }
    Serial.println();
  #endif

  for(size_t i = 0; i < 8; i++)
	  MQTT_CLIENT_ID[i] = UniqueID8[i]; 
  MQTT_CLIENT_ID[8] = 0;
  
	#ifdef SERIALDEBUG1
    Serial.print("MQTT CLIENT ID:");
    Serial.println(MQTT_CLIENT_ID);
  #endif
  
  for (int i= 0 ; i<8 ; i++){
	  if ( ((uint8_t)MQTT_CLIENT_ID[i] < 48) || ((uint8_t)MQTT_CLIENT_ID[i] > 122) || ( (uint8_t)MQTT_CLIENT_ID[i]>90 && (uint8_t)MQTT_CLIENT_ID[i]<97) || ( (uint8_t)MQTT_CLIENT_ID[i]>57 && (uint8_t)MQTT_CLIENT_ID[i]<65)){
		  #ifdef SERIALDEBUG1
  		  Serial.print("Caractère interdit, remplacé par un A: ");
  		  Serial.println(MQTT_CLIENT_ID[i]);
		  #endif
		  MQTT_CLIENT_ID[i]= 'A';
	    }	  
    }
  #ifdef SERIALDEBUG1
    Serial.print("MQTT CLIENT ID:");
    Serial.println(MQTT_CLIENT_ID);
  #endif
  
  readStoredVariables();
  
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }
    Serial.println (" - Lecture EEPROM:");
    Serial.println(settings.deviceName);
    Serial.print(settings.mqttServerIP[0]);Serial.print(".");
    Serial.print(settings.mqttServerIP[1]);Serial.print(".");
    Serial.print(settings.mqttServerIP[2]);Serial.print(".");
    Serial.println(settings.mqttServerIP[3]);
    Serial.println(settings.mqttPrefix);    
    Serial.println(settings.subTopicStatus);
    Serial.println(settings.subTopicSet);
    Serial.println(settings.subTopicUptime);
  #endif
  
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }
    Serial.println(" - Ethernet Initialisation...") ;
	#endif
	
  // start the Ethernet connection and the server:  
Ethernet.init(ETH_CS_PIN);
//  Ethernet.hostName(settings.deviceName);
  Ethernet.begin(mac);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    while (true) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      delay(1000); // do nothing, no point running without Ethernet hardware
      }
    }
  if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");

  // Start the HTTP Server
  HTTPEthernetServer.begin();
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }
    Serial.print(" - My IP Address is ");
    Serial.println(Ethernet.localIP());
  #endif
  
 	#ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }
    Serial.println(" - Initialisation MQTT... ") ;
	#endif

	
   // -- Prepare dynamic topic name with trailing slash
   	
	strcpy (dynamicTopic, settings.mqttPrefix);
	strcat (dynamicTopic, "/");
	strcat (dynamicTopic, settings.deviceName);
	strcat (dynamicTopic, "/");
	
	mqttClient.setServer(settings.mqttServerIP, MQTT_PORT) ;
	mqttClient.setCallback(mqttCallback) ;
	
	MQTT_LastReconnectAttempt = 0;

  IWatchdog.begin(4000000);
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }
    Serial.println(" - Watchdog set to 4s");
  #endif

  
	#ifdef SERIALDEBUG1
	  { long now = millis(); Serial.print(now); }
	  Serial.println(" - End of SETUP");
  #endif
}

void loop() {
	
	IWatchdog.reload(); // Recharge le watchdog !!
 
	if (needReset){
    long now = millis();Serial.print(now);
		Serial.println(" - Rebooting...");
    delay(200);NVIC_SystemReset();
	}
  
	if (!mqttClient.connected()) {
    long now = millis();
    if (now - MQTT_LastReconnectAttempt >= 1000) {
      MQTT_LastReconnectAttempt = now;
      // Attempt to reconnect
      #ifdef SERIALDEBUG1
        Serial.print(now);
	      Serial.println (" - MQTT connecting...");
	    #endif
	  
	  if (MQTT_Reconnect()) {
      #ifdef SERIALDEBUG1
		    Serial.print(now);
        Serial.println (" - MQTT connected !!");
		  #endif
		
		  MQTT_LastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    long now = millis();

    if (now - BlinkStatus >= 50) {
      if (now - BlinkStatus < 100) { digitalWrite(GPIO_OUTPUT[0], isON[0]); BlinkStatus-=51; }
      if (now - BlinkStatus >= 1050) { digitalWrite(GPIO_OUTPUT[0], !isON[0]); BlinkStatus = now; }
      }
    
    if (now - MQTT_LastUptimeSent >= 10000) {
      MQTT_LastUptimeSent = now; MQTT_SendUptime(); MQTT_LoopIterationCount=0;
    } else MQTT_LoopIterationCount++;
    mqttClient.loop();
  }


  // listen for incoming clients
  EthernetClient HTTPEthernetClient;

  HTTPEthernetClient = HTTPEthernetServer.available();
  if (HTTPEthernetClient) {  // got client?
    bool currentLineIsBlank = true;
		String req_str = "";
		int data_length = -1;
		bool skip = true;
		bool dataPresent = false ;
		
        while (HTTPEthernetClient.connected()) {
            if (HTTPEthernetClient.available()) {   // client data available to read
                char c = HTTPEthernetClient.read(); // read 1 byte (character) from client
                //Serial.write(c);
				//store characters to string
				req_str += c; 
				
				// last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank && req_str.startsWith("GET")) {
					writeHTTPResponse(HTTPEthernetClient);
					break;
				}
				
				if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST") && !skip) {
					writeHTTPResponse(HTTPEthernetClient);
					break;
				}   
				
				if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST") && skip) {
					skip = false;
					dataPresent = true ;
					String temp = req_str.substring(req_str.indexOf("Content-Length:") + 15);
					temp.trim();
					data_length = temp.toInt();
					writeHTTPResponse(HTTPEthernetClient);
					while(data_length-- > 0)
						{
						c = HTTPEthernetClient.read();
						req_str += c;
					}
					
					break;
				}
		
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                }
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (HTTPEthernetClient.available())
        } // end while (HTTPEthernetClient.connected())
		
    HTTPEthernetClient.stop(); // close the connection
	
	if (dataPresent){
		dataPresent  = false ;

    #ifdef SERIALDEBUG1
      Serial.println("Chaîne HTTP REQ:"); Serial.println(req_str);
    #endif

		if (req_str.indexOf("deviceName=") > 0) handleHTTPPost_deviceName(req_str);
    else if (req_str.indexOf("switch_output=") > 0 ) handleHTTPPost_switch_output(req_str);
	  }
	}
}
