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
 *                  - HTTP set Output
 *                  - DHCP Hostname & Freebox issue             
 *                  - Activer/désactiver la configuration HTTP depuis une commande MQTT
 *                  - Vérifier persistance des sorties après reboot (MQTT persistance)
 */


 /*
  * 
            61290000 - uptime: 61290 - LoopIterationCount: 43554 - freeMemory: 59019
            61300000 - uptime: 61300 - LoopIterationCount: 43553 - freeMemory: 59019
            61310000 - uptime: 61310 - LoopIterationCount: 43556 - freeMemory: 59019
            61320000 - uptime: 61320 - LoopIterationCount: 43554 - freeMemory: 59019
            61330000 - uptime: 61330 - LoopIterationCount: 43553 - freeMemory: 59019
            61340000 - uptime: 61340 - LoopIterationCount: 39019 - freeMemory: 58483
            61350000 - uptime: 61350 - LoopIterationCount: 36421 - freeMemory: 58483
            61360000 - uptime: 61360 - LoopIterationCount: 36040 - freeMemory: 58483
            61370000 - uptime: 61370 - LoopIterationCount: 43555 - freeMemory: 58483
            61380000 - uptime: 61380 - LoopIterationCount: 43554 - freeMemory: 58483
            61390000 - uptime: 61390 - LoopIterationCount: 43554 - freeMemory: 58483
            61400000 - uptime: 61400 - LoopIterationCount: 43555 - freeMemory: 58483
            61410000 - uptime: 61410 - LoopIterationCount: 43554 - freeMemory: 58483
  * 
  */

#include <Ethernet.h>
//#include <SPI.h>
//#include <LwIP.h>
//#include <STM32Ethernet.h>
//#include <Wire.h>

#include <EEPROM.h>
#include <PubSubClient.h>     // Library from https://github.com/knolleary/pubsubclient (Standard on the Arduino IDE Library Manager)
#include <ArduinoUniqueID.h>  // Library from https://github.com/ricaun/ArduinoUniqueID (Standard on the Arduino IDE Library Manager)
#include <IWatchdog.h>        // Library included in Arduino_Core_STM32 version > 1.3.0

#define STRING_LEN 30
#define MQTT_STRING_LEN 128
#define OUTPUT_COUNT 28       // Nb de sortie externe réel, sachant que la LED interne d'état sera comptabilisé en +

bool IWatchdog_isReset=false;

const int MQTT_PORT = 1883 ;
char dynamicTopic [MQTT_STRING_LEN];
long MQTT_LastReconnectAttempt = 0;              // For automatic reconnection, non-blocking
long MQTT_LastUptimeSent = 0;                        // For sending MQTT Uptime every nn seconds
long MQTT_LoopIterationCount = 0;                    // For counting loop runs in the nn seconds timeframe
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



char MQTT_CLIENT_ID[STRING_LEN] ; 

byte mac[] = {0x00, 0x80, 0xE1, 0x03, 0x04, 0x05} ; 

#define ETH_CS_PIN PA4
#define STATUS_PIN PC13


// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer HTTPEthernetServer(80);
EthernetClient MqttEthernetClient;
PubSubClient mqttClient (MqttEthernetClient);


#define MaxHeaderLength 350    //maximum length of http header required
String httpPostRequest = String(MaxHeaderLength);

struct STRUCT_Settings {
	char deviceName     [STRING_LEN];
	IPAddress mqttServerIP          ;
	char mqttPrefix     [STRING_LEN];
	char subTopicStatus [STRING_LEN];
  char subTopicSet    [STRING_LEN];
  char subTopicUptime [STRING_LEN];
};

struct STRUCT_Settings settings = (STRUCT_Settings) {
	"carte1",
	IPAddress(192,168,1,22),
	"carterelais",
	"status", 
  "set",
  "uptime"
};

bool needReset = false;
uint8_t isON [] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} ;

bool MQTT_Reconnect() {              // Code to test the MQTT connection and reconnect if necessary, does not block the processor
  if (mqttClient.connect(MQTT_CLIENT_ID)) {   // Unique clientID
    char topicOut[MQTT_STRING_LEN] ;
  
    strcpy (topicOut, dynamicTopic);
    strcat (topicOut, settings.subTopicStatus);
    if ( IWatchdog_isReset ) mqttClient.publish(topicOut, "Client MQTT STM32 connecté. Card was restarted by Watchdog !!"); else mqttClient.publish(topicOut, "Client MQTT STM32 connecté. Bonjour, tout va bien par ici !!");
    strcpy (topicOut, dynamicTopic);
    strcat (topicOut, "#");
    
    #ifdef SERIALDEBUG1
      { long now = millis(); Serial.print(now); }
      Serial.print (" - Subscribe to topic: ") ;
      Serial.println (topicOut);
    #endif
    
    mqttClient.subscribe(topicOut);
  }
  return mqttClient.connected();
}

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__
 
int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

void MQTT_SendUptime() {
    char topicOut[MQTT_STRING_LEN] ;  
    strcpy (topicOut, dynamicTopic);
    strcat (topicOut, settings.subTopicUptime);
//    mqttClient.publish(topicOut, "xxx");
    
    #ifdef SERIALDEBUG1
      { long now = millis(); Serial.print(now); }
      Serial.print (" - uptime: "); Serial.print (millis() / 1000);
      Serial.print (" - LoopIterationCount: "); Serial.print (MQTT_LoopIterationCount);
      Serial.print (" - freeMemory: "); Serial.println(freeMemory());

    
    #endif
    }

void mqttCallback (char* topic , byte* payload , unsigned int length) {			// We will place here all that happens when a message is received on the subscribed MQTT topic(s) 

	#ifdef SERIALDEBUG2
		Serial.print ("Nouveau message, topic: ") ;
		Serial.println(topic);
	

	for (unsigned int i = 0; i < length; i++) {								// Output the payload as text on serial
			Serial.print((char)payload[i]);
	}
			Serial.println(); 
	
	#endif
	
  // Allocate the correct amount of memory for the payload copy. We don't know if a new message will arrive while we work on it, so we work on a copy of it
	byte* msg = (byte*)malloc(length+1);
	// Copy the payload to the new buffer
	memcpy(msg,payload,length);
	msg[length]= '\0';
	
	#ifdef SERIALDEBUG9
		Serial.print("Message reçu: ");
		for (unsigned int i = 0; i < length; i++) Serial.print((char)msg[i]);
	  Serial.println(); 
	  Serial.print("Detection nombre:");
	#endif
		
	String s = String((char*)msg);			// If the payload is an integer, it goes inside this value
	uint16_t valeur = s.toInt();
	
	#ifdef SERIALDEBUG9
	  Serial.println(valeur);
	#endif
	
	// Detection of the channel and saving the value:
	
	char myArray [MQTT_STRING_LEN];
	char slash[5] ;
 	
	sprintf(slash,"/");

	char *result[10];		
	uint8_t channel=0;
	
	// Remove from the TOPIC, the first part which we don't need:
	#ifdef SERIALDEBUG9
	  Serial.print("Remove first part... Length of dynamicTopic: ");
	  Serial.println(strlen(dynamicTopic));
	#endif

	uint8_t Alength = strlen(dynamicTopic)-1;
	uint8_t Blength = strlen(topic)- Alength;
	
	strncpy(myArray, &topic[Alength], Blength);
	
	myArray[Blength]= '\0';
	
	#ifdef SERIALDEBUG9
 	  Serial.print("Chaine résultante: "); 
	  Serial.println(myArray); 
	  Serial.print ("Longeur restante: ");
	  Serial.println (strlen(myArray));
	  Serial.print ("Devrait être: ");
	  Serial.println (strlen(topic)- Alength);	 
	#endif
	
	char * token = strtok(myArray,"/");
	uint8_t j = 0;
	uint8_t numberArguments = 0 ;
	while (token != NULL) {
	  numberArguments ++ ;	
	  result[j] = (char*) malloc (strlen(token));
	  sprintf(result[j], "%s", token);

	  #ifdef SERIALDEBUG9
	    Serial.print(j); Serial.print (" :"); Serial.println (result[j]);
	  #endif
	 	 
    token = strtok(NULL,"/");
	  j++;
	  }
	
	#ifdef SERIALDEBUG9
	  Serial.printf("all Done!\n");
	#endif
	
	if (numberArguments < 3) {
		result[2] = (char*) malloc (3);
		sprintf(result[2],"0");
	  }
	
	#ifdef SERIALDEBUG9
	  for (int i=0 ; i<3 ; i++) Serial.println(result[i]);
	#endif
	
	
	// get the channel number  
	#ifdef SERIALDEBUG9
	  Serial.println("Get channel number...");
	#endif
	
	channel = atoi(result[0]);

	//int erreur;
	//if (sscanf( result[0], "%u", channel) != 1)  erreur = 1 ;
	//if (erreur)  Serial.println("Erreur numéro de canal");
		
	#ifdef SERIALDEBUG9
	  Serial.print("Canal:");
	  Serial.println(channel);
	  Serial.println("Sub-topics:");
	  Serial.println(result[0]);
	  Serial.println(result[1]);
	  Serial.println(result[2]);
	#endif
	
	/* 
    Now figure out what to do:
      0 -> OFF
      1 -> ON
      2 -> toggle
      9 on channel 0 -> reset board 
	 */

  #ifdef KEEP_BUG
    if ( !strcmp (result[1] , settings.subTopicSet) && channel <= OUTPUT_COUNT+10 ){       // Simple, just switch on or off
    #else
    if ( !strcmp (result[1] , settings.subTopicSet) && channel <= OUTPUT_COUNT ){          // Simple, just switch on or off
  #endif
    if ( payload[0] == '0' ) doOFF(channel);    // Switch OFF
		if ( payload[0] == '1' ) doON(channel);		  // Switch ON			
		if ( payload[0] == '2' )
			if (!isON[channel]) doON(channel);
			else doOFF(channel) ; // Toggles the output state		
		if ( payload[0] == '9' && channel == 0 ) needReset = true ;
	}
  
	free(msg);
	for (int i=0 ; i<3 ; i++) free (result[i]);
	
}

void doON(uint8_t channel){

	char topicOut [MQTT_STRING_LEN];
	char convert [5];
		
	digitalWrite(GPIO_OUTPUT[channel], 1);
 	
	strcpy (topicOut,dynamicTopic);
	sprintf (convert, "%i", channel);
	strcat (topicOut, convert);
	strcat (topicOut, "/");
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "1", true);
	
	#ifdef SERIALDEBUG9
    Serial.println ("Got it! ON");
	#endif
		
	isON[channel] = 1 ;
}


void doOFF(uint8_t channel){
		
	char topicOut [MQTT_STRING_LEN];
	char convert [5];
	
	digitalWrite(GPIO_OUTPUT[channel], 0);

	strcpy (topicOut,dynamicTopic);
	sprintf (convert, "%i", channel);
	strcat (topicOut, convert);
	strcat (topicOut, "/");
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "0", true);
	
	#ifdef SERIALDEBUG9
	  Serial.println ("Got it! OFF");
	#endif
	
	isON[channel] = 0 ;

}

void handleHTTPPost() {
	// DeviceName
	int index1 = httpPostRequest.indexOf("deviceName=") + 11;
	int index2 = httpPostRequest.indexOf("&", index1);
	
	for (int i= index1, j=0 ; i<index2; i++, j++)
		settings.deviceName[j] = httpPostRequest.charAt(i);
	settings.deviceName[(index2 - index1 )] = 0;
			
	// mqttserverIP  :Convert char array IP address to IPAddress format...
	char ipString[STRING_LEN];
	bool successIPAdress = false;
	index1 = httpPostRequest.indexOf("mqttServerIP=") + 13;  	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) ipString[j] = httpPostRequest.charAt(i);
	ipString[(index2 - index1 )] = 0;
	sscanf(ipString, "%u.%u.%u.%u", &settings.mqttServerIP[0], &settings.mqttServerIP[1], &settings.mqttServerIP[2], &settings.mqttServerIP[3]);

	index1 = httpPostRequest.indexOf("mqttPrefix=") + 11;	    index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) settings.mqttPrefix[j] = httpPostRequest.charAt(i);
	settings.mqttPrefix[(index2 - index1)] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicStatus=") + 15;	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) settings.subTopicStatus[j] = httpPostRequest.charAt(i);
	settings.subTopicStatus[(index2 - index1)] = 0;
	
  index1 = httpPostRequest.indexOf("subTopicSet=") + 12;    index2 = httpPostRequest.indexOf("&", index1);
  for (int i= index1, j=0 ; i<index2; i++, j++) settings.subTopicSet[j]    = httpPostRequest.charAt(i);
  settings.subTopicSet[(index2 - index1 )] = 0;
  
  index1 = httpPostRequest.indexOf("subTopicUptime=") + 15; index2 = httpPostRequest.length();
  for (int i= index1, j=0 ; i<index2; i++, j++) settings.subTopicUptime[j] = httpPostRequest.charAt(i);
  settings.subTopicUptime[(index2 - index1 )] = 0;
  
	#ifdef SERIALDEBUG1
  	Serial.println("Valeurs enregistrées:");
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
    { long now = millis(); Serial.print(now); } Serial.println(" - EEPROM - Effacement Signature -XX- ");
	#endif
  
  IWatchdog.reload();EEPROM.put(0, "XX");
  	
	#ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); } Serial.println(" - EEPROM - Ecriture paramètres");
	#endif
	
  IWatchdog.reload();EEPROM.put(STRING_LEN, settings);

  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }  Serial.println(" - EEPROM - Ecriture Signature OK");
  #endif

  IWatchdog.reload();EEPROM.put(0, "OK");
		
	#ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }  Serial.println(" - EEPROM - Toutes les écritures sont terminées");
	#endif

	delay(200);
 
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }  Serial.println(" - Redémarrage...");
  #endif
  
	NVIC_SystemReset();
}



void readStoredVariables() {
	char buf ;
	char readString [STRING_LEN];
	
	//Lecture du tag
	for (uint8_t i=0 ; i< STRING_LEN; i++){
		EEPROM.get(i, buf);
		readString[i] = buf;		
	}
	
	if  (strcmp(readString, "OK") != 0){
		#ifdef SERIALDEBUG1
			Serial.println("Mise à zéro EEPROM");
		#endif

    EEPROM.put(STRING_LEN, settings);
    EEPROM.put(0, "OK");
    } else {
		#ifdef SERIALDEBUG1
		  Serial.println("EEPROM déjà initialisée");
      EEPROM.get(STRING_LEN, settings);
		#endif
	  }
}

void writeHTTPResponse(EthernetClient client) {	// send a standard http response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
	client.println(R"FOO(<!DOCTYPE html><html><head><title>Module sortie relais: r&eacute;glages</title></head><body>)FOO");

  client.println("<fieldset><legend><h2>Etat des sorties :</h2></legend>");

  client.println(R"FOO(<table border="1" style="border-collapse: collapse; width: 100%;">)FOO");

  client.println("<tr>");
	for (int pinNumber = 0; pinNumber < OUTPUT_COUNT+1; pinNumber++) {
    client.print(R"FOO(<td style="text-align: center;">)FOO");
	  client.print(pinNumber);
	  client.println("</td>");   
	}
  client.println("</tr><tr>");
  for (int pinNumber = 0; pinNumber < OUTPUT_COUNT+1; pinNumber++) {
    client.print(R"FOO(<td style="text-align: center;">)FOO");
    if (isON[pinNumber])
      client.print(R"FOO(<span style="color:#339966;"><strong>ON</strong></span>)FOO");
      else
      client.print(R"FOO(<span style="color:#ff0000;">OFF</span>)FOO");
      
    client.println("</td>");
	}
  client.println("</tr></table>");
  client.println("<p></p></fieldset>");
	
  client.println("<fieldset><legend><h2>Param&egrave;tres actuels :</h2></legend>");
    client.print("<p>Uptime: "); client.print(millis()/1000); client.println("s </p>");
    client.print("<p>freeMemory: "); client.print(freeMemory()); client.println("</p>");
    client.print("<p>Adresse MAC: ");
      client.print(mac[0], HEX);client.print("-");
      client.print(mac[1], HEX);client.print("-");
      client.print(mac[2], HEX);client.print("-<strong>");
      client.print(mac[3], HEX);client.print("-");
      client.print(mac[4], HEX);client.print("-");
      client.print(mac[5], HEX);client.println("</strong></p>");
    client.print("<p>Adresse IP: ");client.print(Ethernet.localIP());client.println("</p>");
    
    client.print(R"FOO(<p>Connexion MQTT: <span style="color: )FOO");
    if (mqttClient.connected()) client.println(R"FOO(#339966;">OK </span></p>)FOO"); else client.println(R"FOO(#ff0000;">absente</span></p>)FOO");	
  	client.print(R"FOO(<p>Topics MQTT:</p><ul><li>Base:&nbsp;)FOO");
  	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.println("/");
  	client.print (R"FOO(</li><li>Status:&nbsp;)FOO");
  	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/"); client.print(settings.subTopicStatus); client.println("/");
    client.print(R"FOO(</li><li>Actions:&nbsp;)FOO");
    client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/(canal 0-28)/");client.print(settings.subTopicSet); client.println("/");
    client.print(R"FOO(</li><li>Uptime:&nbsp;)FOO");
    client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/"); client.print(settings.subTopicUptime); client.println("/");
	client.println("</li></ul><p></p></fieldset>");


//XXXXXXXXX
    
  client.print(R"FOO(<fieldset><legend><h2>Mise &agrave; jour des param&egrave;tres MQTT :</h2></legend> 
    <form accept-charset="UTF-8" autocomplete="off" method="POST">
    <table>
    <tr>
    <td><label for="deviceNameLabel">Nom :</label></td> 
    <td><input name="deviceName" type="text" value=")FOO");
  client.print(settings.deviceName);client.print(R"FOO(" /></td>
    </tr><tr>
    <td><label for="serverIPLabel">Adresse serveur MQTT :</label></td> 
    <td><input name="mqttServerIP" type="text" value=")FOO");
  for (int i=0 ; i<4 ; i++){client.print(settings.mqttServerIP[i]);if (i<3) client.print(".");}
  client.print(R"FOO(" /> </td>
    </tr><tr>
    <td><label for="mqttPrefixLabel">Pr&eacute;fixe MQTT :</label></td>
    <td><input name="mqttPrefix" type="text" value=")FOO");
  client.print(settings.mqttPrefix);client.print(R"FOO(" /> </td>
    </tr><tr>
    <td><label for="supTopicStatusLabel">Sous-topic retour d'&eacute;tat :</label></td>
    <td><input name="subTopicStatus" type="text" value=")FOO");
  client.print(settings.subTopicStatus);client.print(R"FOO(" /> </td>
    </tr><tr>
    <td><label for="subTopicSetLabel">Sous-topic action :</label></td>
    <td><input name="subTopicSet" type="text" value=")FOO");
  client.print(settings.subTopicSet);client.print(R"FOO(" /> </td>
    </tr><tr>
    <td><label for="subTopicUptimeLabel">Sous-topic uptime :</label></td> 
    <td><input name="subTopicUptime" type="text" value=")FOO");
  client.print(settings.subTopicUptime);client.print(R"FOO(" /> </td>
    </tr>
    </table>
    <button type="submit" value="Submit">Valider les modifications et red&eacute;marrer</button>
    </fieldset>
    </form>)FOO");
    
  }

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
		httpPostRequest = req_str.substring(req_str.indexOf("deviceName="));
		httpPostRequest.trim();
		
		#ifdef SERIALDEBUG9
		Serial.println("Chaîne POST:");
		Serial.println(httpPostRequest);
		#endif
		
		handleHTTPPost();
	}
    } // end if (client)	  
}
