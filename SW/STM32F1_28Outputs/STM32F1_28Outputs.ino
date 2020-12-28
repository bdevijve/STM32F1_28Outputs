#include <ArduinoUniqueID.h> // Library from https://github.com/ricaun/ArduinoUniqueID (Standard on the Arduino IDE Library Manager)
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <PubSubClient.h> // Library from https://github.com/knolleary/pubsubclient (Standard on the Arduino IDE Library Manager)

#define STRING_LEN 30
#define MQTT_STRING_LEN 128
#define OUTPUT_COUNT 28

const int MQTT_PORT = 1883 ;
char dynamicTopic [MQTT_STRING_LEN];

uint8_t output[] = {
PC13, PE12, PE13, PE14, PB8, PA0, PA1, PA2, PA3, PB9, PB0, PB1, PE8, PE9, PE10, PE11, PB4, PB3, PA15, PB10, PB11, PD12, PD13, PD14, PD15, PC6, PC7, PC8, PC9
};

char MQTT_CLIENT_ID[STRING_LEN] ; 


byte mac[] = {0x00, 0x80, 0xE1, 0x03, 0x04, 0x05} ; 

IPAddress ip(192, 168, 1, 177);

#define SERIALDEBUG 		// Comment to remove serial debugging info
#define SELF_TEST			// teste toutes les sorties au démarrage
#define ETH_CS_PIN PA4
#define STATUS_PIN PC13


// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);
EthernetClient client;
EthernetClient client2;
PubSubClient mqttClient (client2);
long lastReconnectAttempt = 0;							// For automatic reconnection, non-blocking


#define MaxHeaderLength 350    //maximum length of http header required
String httpPostRequest = String(MaxHeaderLength);

#define defaultMqttServerIP 192,168,1,22
#define defaultDeviceName "testOutput"
#define defaultMqttPrefix "relais"
#define defaultSubTopicStatus  "status" 
#define defaultSubTopicSet  "set"

struct mySettings {	
	char deviceName [STRING_LEN] ;
	IPAddress mqttServerIP;
	char mqttPrefix [STRING_LEN] ;
	char subTopicStatus [STRING_LEN] ;
	char subTopicSet [STRING_LEN] ;
};


struct mySettings settings;

struct mySettings defaultSettings = (mySettings) {
	defaultDeviceName,
	IPAddress(192,168,1,22),
	defaultMqttPrefix,
	defaultSubTopicStatus, 
	defaultSubTopicSet
};

bool needReset = false;
uint8_t isON [] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} ;

bool mqttReconnect() {							// Code to test the MQTT connection and reconnect if necessary, does not block the processor
  if (mqttClient.connect(MQTT_CLIENT_ID)) {		// Unique clientID
    char topicOut[MQTT_STRING_LEN] ;
	
	strcpy (topicOut, dynamicTopic);
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "Client MQTT STM32 connecté. Bonjour !");
    
	strcpy (topicOut, dynamicTopic);
	strcat (topicOut, "#");
	
	#ifdef SERIALDEBUG
	 Serial.print ("Subscribe to topic: ") ;
	 Serial.println (topicOut);
	#endif
	
	mqttClient.subscribe(topicOut);
  }
  return mqttClient.connected();
}

void mqttCallback (char* topic , byte* payload , unsigned int length) {			// We will place here all that happens when a message is received on the subscribed MQTT topic(s) 

	#ifdef SERIALDEBUG
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
	
	#ifdef SERIALDEBUG
		Serial.print("Message reçu: ");
		for (unsigned int i = 0; i < length; i++) Serial.print((char)msg[i]);
	  Serial.println(); 
	  Serial.print("Detection nombre:");
	#endif
		
	String s = String((char*)msg);			// If the payload is an integer, it goes inside this value
	uint16_t valeur = s.toInt();
	
	#ifdef SERIALDEBUG
	  Serial.println(valeur);
	#endif
	
	// Detection of the channel and saving the value:
	
	char myArray [MQTT_STRING_LEN];
	char slash[5] ;
 	
	sprintf(slash,"/");

	char *result[10];		
	uint8_t channel=0;
	
	// Remove from the TOPIC, the first part which we don't need:
	#ifdef SERIALDEBUG
	  Serial.print("Remove first part... Length of dynamicTopic: ");
	  Serial.println(strlen(dynamicTopic));
	#endif

	uint8_t Alength = strlen(dynamicTopic)-1;
	uint8_t Blength = strlen(topic)- Alength;
	
	strncpy(myArray, &topic[Alength], Blength);
	
	myArray[Blength]= '\0';
	
	#ifdef SERIALDEBUG
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

	  #ifdef SERIALDEBUG
	    Serial.print(j); Serial.print (" :"); Serial.println (result[j]);
	  #endif
	 	 
    token = strtok(NULL,"/");
	  j++;
	  }
	
	#ifdef SERIALDEBUG
	  Serial.printf("all Done!\n");
	#endif
	
	if (numberArguments < 3) {
		result[2] = (char*) malloc (3);
		sprintf(result[2],"0");
	  }
	
	#ifdef SERIALDEBUG
	  for (int i=0 ; i<3 ; i++) Serial.println(result[i]);
	#endif
	
	
	// get the channel number  
	#ifdef SERIALDEBUG
	  Serial.println("Get channel number...");
	#endif
	
	channel = atoi (result[0]);
	
	//int erreur;
	//if (sscanf( result[0], "%u", channel) != 1)  erreur = 1 ;
	//if (erreur)  Serial.println("Erreur numéro de canal");
		
	#ifdef SERIALDEBUG
	  Serial.print("Canal:");
	  Serial.println(channel);
	  Serial.println("Sub-topics:");
	  Serial.println(result[0]);
	  Serial.println(result[1]);
	  Serial.println(result[2]);
	#endif
	
	// Now figure out what to do: 0-> OFF, 1-> ON, 2-> toggle, 3-> nothing, 4-> nothing, 5-> reset board
	if ( !strcmp (result[1] , settings.subTopicSet)){ 			// Simple, just switch on or off
    if (payload[0] == '0') doOFF(channel, 1);    // Switch OFF
		if (payload[0] == '1') doON(channel, 1);		  // Switch ON			
		if (payload[0] == '2')
			if (!isON[channel]) doON(channel, 1);
			else doOFF(channel, 1) ; // Toggles the output state		
		if (payload[0] == '3');
		if (payload[0] == '4');
		if (payload[0] == '5') needReset = true ;
	}
  
	free(msg);
	for (int i=0 ; i<3 ; i++) free (result[i]);
	
}

void doON(uint8_t channel, bool changeValue){

	char topicOut [MQTT_STRING_LEN];
	char convert [5];
		
	if (changeValue) digitalWrite(output[channel], 1);
 	
	strcpy (topicOut,dynamicTopic);
	sprintf (convert, "%i", channel);
	strcat (topicOut, convert);
	strcat (topicOut, "/");
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "1", 1);
	
	#ifdef SERIALDEBUG
    Serial.println ("Got it! ON");
	#endif
		
	isON[channel] = 1 ;
}


void doOFF(uint8_t channel, bool changeValue){
		
	char topicOut [MQTT_STRING_LEN];
	char convert [5];
	
	if (changeValue) digitalWrite(output[channel], 0);

	strcpy (topicOut,dynamicTopic);
	sprintf (convert, "%i", channel);
	strcat (topicOut, convert);
	strcat (topicOut, "/");
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "0", 1);
	
	#ifdef SERIALDEBUG
	  Serial.println ("Got it! OFF");
	#endif
	
	isON[channel] = 0 ;

}

void handlePost(){
	
	// DeviceName
	int index1 = httpPostRequest.indexOf("deviceName=") + 11;
	int index2 = httpPostRequest.indexOf("&", index1);
	
	for (int i= index1, j=0 ; i<index2; i++, j++)
		settings.deviceName[j] = httpPostRequest.charAt(i);
	settings.deviceName[(index2 - index1 )] = 0;
	
	#ifdef SERIALDEBUG 
	  Serial.println();
	#endif
		
	// mqttserverIP  :Convert char array IP address to IPAddress format...
	char ipString[STRING_LEN];
	bool successIPAdress = false;
	index1 = httpPostRequest.indexOf("mqttServerIP=") + 13;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		ipString[j] = httpPostRequest.charAt(i);
	ipString[(index2 - index1 )] = 0;
	sscanf(ipString, "%u.%u.%u.%u", &settings.mqttServerIP[0], &settings.mqttServerIP[1], &settings.mqttServerIP[2], &settings.mqttServerIP[3]);

	index1 = httpPostRequest.indexOf("mqttPrefix=") + 11;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.mqttPrefix[j] = httpPostRequest.charAt(i);
	settings.mqttPrefix[(index2 - index1)] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicStatus=") + 15;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicStatus[j] = httpPostRequest.charAt(i);
	settings.subTopicStatus[(index2 - index1)] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicSet=") + 12;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicSet[j] = httpPostRequest.charAt(i);
	settings.subTopicSet[(index2 - index1 )] = 0;
  
	#ifdef SERIALDEBUG
  	Serial.println("Valeurs enregistrées:");
  	Serial.println(settings.deviceName);
  	Serial.print(settings.mqttServerIP[0]); 
  	Serial.print(".");
  	Serial.print(settings.mqttServerIP[1]); 
  	Serial.print(".");
  	Serial.print(settings.mqttServerIP[2]);
  	Serial.print(".");
  	Serial.println(settings.mqttServerIP[3]); 
  	Serial.println(settings.mqttPrefix);
  	Serial.println(settings.subTopicStatus);
  	Serial.println(settings.subTopicSet);
   	Serial.println("Effacement mémoire");
	#endif
	
	for (int i = 0 ; i < STRING_LEN + sizeof(settings) ; i++) EEPROM.write(i, 0);
	
	#ifdef SERIALDEBUG
	  Serial.println("Ecriture paramètres");
	#endif
	
	EEPROM.update (0, 'O');
	EEPROM.update(1, 'K');
	EEPROM.update(2, 0);
	
	for (int i= 3; i<STRING_LEN; i++)EEPROM.update(i, 0);
	EEPROM.put(STRING_LEN, settings);
	
	
	#ifdef SERIALDEBUG
	  Serial.println("Ecriture terminée. Redémarrage...");
	#endif
	
	delay(200);NVIC_SystemReset();
	
}



void readStoredVariables(){
	 
	char buf ;
	char readString [STRING_LEN];
	bool toDefault = false;
	
	//Serial.println("Effacement mémoire...");
	//for (int i = 0 ; i < STRING_LEN + sizeof(settings) ; i++) EEPROM.write(i, 0);
	
	//Lecture du tag
	for (uint8_t i=0 ; i< STRING_LEN; i++){
		EEPROM.get(i, buf);
		readString[i] = buf;		
	}
	
	if  (strcmp(readString, "OK") != 0){
		#ifdef SERIALDEBUG
			Serial.println("Mise à zéro EEPROM");
		#endif
			
		toDefault = true;	
		EEPROM.update (0, 'O');
		EEPROM.update(1, 'K');
		EEPROM.update(2, 0);
  	} else {
		#ifdef SERIALDEBUG
		  Serial.println("EEPROM déjà initialisée");
		#endif
	  }
	if (toDefault) EEPROM.put(STRING_LEN, defaultSettings);
	EEPROM.get(STRING_LEN, settings);
}

void writeResponse(EthernetClient client) {
    
	// send a standard http response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
	client.print(R"FOO(<!DOCTYPE html><html><head><title>Module sortie relais: r&eacute;glages</title></head><body>)FOO");
	client.print(R"FOO(<p>Connexion MQTT: <span style="color: )FOO");
	if (mqttClient.connected()) { 
		client.print(R"FOO(#339966;">OK </span></p>)FOO");
	} else {
		client.print(R"FOO(#ff0000;">absente</span></p>)FOO");
	}
	client.print(R"FOO(<p>Topics MQTT:</p><p>Base:&nbsp;)FOO");
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/");
	client.print (R"FOO(</p><p>Statut:&nbsp;)FOO");
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/"); client.print(settings.subTopicStatus); client.print("/");
	client.print(R"FOO(</p><p>Actions:&nbsp;)FOO");
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/(canal 1-28)/");client.print(settings.subTopicSet); client.print("/");
	client.print(R"FOO(</p><p></p>)FOO");
	client.print(R"FOO(<fieldset><legend><h1>Mise &agrave; jour param&egrave;tres MQTT :</h1></legend> 
						<form accept-charset="UTF-8" autocomplete="off" method="POST">
						<label for="deviceNameLabel">Nom :</label><br /> 
						<input name="deviceName" type="text" value=")FOO");
	client.print(settings.deviceName);
	client.print(R"FOO(" /> <br /> <br>
			<label for="serverIPLabel">Adresse serveur MQTT :</label><br /> 
			<input name="mqttServerIP" type="text" value=")FOO");
	
	for (int i=0 ; i<4 ; i++){
		client.print(settings.mqttServerIP[i]); 
		if (i<3) client.print(".");
  	}
	client.print(R"FOO(" /> <br /> <br>
			<label for="mqttPrefixLabel">Pr&eacute;fixe MQTT :</label><br /> 
			<input name="mqttPrefix" type="text" value=")FOO");
	client.print(settings.mqttPrefix);
	client.print(R"FOO(" /> <br /> <br>
			<label for="supTopicStatusLabel">Sous-topic retour d'&eacute;tat :</label><br /> 
			<input name="subTopicStatus" type="text" value=")FOO");
	client.print(settings.subTopicStatus);
	client.print(R"FOO(" /> <br /> <br>
			<label for="subTopicSetLabel">Sous-topic action :</label><br /> 
			<input name="subTopicSet" type="text" value=")FOO");
	client.print(settings.subTopicSet);
	client.print(R"FOO(" /> <br /> <br>
			<button type="submit" value="Submit">Valider les modifications et red&eacute;marrer</button>
			</fieldset>
			</form>)FOO");

  }

void setup() {
  Serial.begin(115200);
  #ifdef SERIALDEBUG
    { long now = millis();Serial.print(now); }
    Serial.println(" - Begin of SETUP !!!");
    Serial.print("UniqueID  - ");UniqueIDdump(Serial);
    Serial.print("UniqueID8 - ");UniqueID8dump(Serial);
  #endif
  
  // initialize output pins
  for (int pinNumber = 0; pinNumber < OUTPUT_COUNT+1; pinNumber++) {
    pinMode(output[pinNumber], OUTPUT);
    digitalWrite(output[pinNumber], LOW);
	}

  #ifdef SELF_TEST
    for (int i=0; i<OUTPUT_COUNT+1 ; i++){ digitalWrite(output[i] , 1);delay(10);digitalWrite(output[i], 0); }
    for (int i=OUTPUT_COUNT; i>=0; i--)  { digitalWrite(output[i] , 1);delay(10);digitalWrite(output[i], 0); }
  #endif
  
  mac[0] = 0x00; // UniqueID8[2]; 
  mac[1] = 0x80; // UniqueID8[3]; 
  mac[2] = 0xE1; // UniqueID8[4]; 
  mac[3] = UniqueID8[5]; 
  mac[4] = UniqueID8[6]; 
  mac[5] = UniqueID8[7]; 

  #ifdef SERIALDEBUG
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
  
	#ifdef SERIALDEBUG
    Serial.print("MQTT CLIENT ID:");
    Serial.println(MQTT_CLIENT_ID);
  #endif
  
  for (int i= 0 ; i<8 ; i++){
	  if ( ((uint8_t)MQTT_CLIENT_ID[i] < 48) || ((uint8_t)MQTT_CLIENT_ID[i] > 122) || ( (uint8_t)MQTT_CLIENT_ID[i]>90 && (uint8_t)MQTT_CLIENT_ID[i]<97) || ( (uint8_t)MQTT_CLIENT_ID[i]>57 && (uint8_t)MQTT_CLIENT_ID[i]<65)){
		  #ifdef SERIALDEBUG
  		  Serial.print("Caractère interdit, remplacé par un A: ");
  		  Serial.println(MQTT_CLIENT_ID[i]);
		  #endif
		  MQTT_CLIENT_ID[i]= 'A';
	    }	  
    }
  #ifdef SERIALDEBUG
    Serial.print("MQTT CLIENT ID:");
    Serial.println(MQTT_CLIENT_ID);
  #endif
  
  readStoredVariables();
  
  #ifdef SERIALDEBUG
    { long now = millis(); Serial.print(now); }
    Serial.println (" - Lecture EEPROM:");
    Serial.println(settings.deviceName);
    Serial.print(settings.mqttServerIP[0]); 
    Serial.print(".");
    Serial.print(settings.mqttServerIP[1]); 
    Serial.print(".");
    Serial.print(settings.mqttServerIP[2]); 
    Serial.print(".");
    Serial.println(settings.mqttServerIP[3]); 
    Serial.println(settings.mqttPrefix);
    Serial.println(settings.deviceName);
    Serial.println(settings.subTopicStatus);
    Serial.println(settings.subTopicSet);
  #endif
  
  #ifdef SERIALDEBUG
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

  // start the server
  server.begin();
  #ifdef SERIALDEBUG
    { long now = millis(); Serial.print(now); }
    Serial.print(" - My IP Address is ");
    Serial.println(Ethernet.localIP());
  #endif
  
 	#ifdef SERIALDEBUG
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
	
	lastReconnectAttempt = 0;
	
	#ifdef SERIALDEBUG
	  { long now = millis(); Serial.print(now); }
	  Serial.println(" - End of SETUP");
  #endif
}


void loop() {
	
	if (needReset){
    long now = millis();
    Serial.print(now);
		Serial.println(" - Rebooting...");
    delay(200);NVIC_SystemReset();
	}
  
	if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 1000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      #ifdef SERIALDEBUG
        Serial.print(now);
	      Serial.println (" - MQTT connecting...");
	    #endif
	  
	  if (mqttReconnect()) {
      #ifdef SERIALDEBUG
		    Serial.print(now);
        Serial.println (" - MQTT connected !!");
		  #endif
		
		  lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    mqttClient.loop();
  }
		
  // listen for incoming clients
  client = server.available();
  if (client) {  // got client?
    bool currentLineIsBlank = true;
		String req_str = "";
		int data_length = -1;
		bool skip = true;
		bool dataPresent = false ;
		
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                //Serial.write(c);
				//store characters to string
				req_str += c; 
				
				// last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank && req_str.startsWith("GET")) {
					writeResponse(client);
					break;
				}
				
				if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST") && !skip) {
					writeResponse(client);
					break;
				}   
				
				if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST") && skip) {
					skip = false;
					dataPresent = true ;
					String temp = req_str.substring(req_str.indexOf("Content-Length:") + 15);
					temp.trim();
					data_length = temp.toInt();
					writeResponse(client);
					while(data_length-- > 0)
						{
						c = client.read();
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
            } // end if (client.available())
        } // end while (client.connected())
		
    //delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
	
	if (dataPresent){
		dataPresent  = false ;
		httpPostRequest = req_str.substring(req_str.indexOf("deviceName="));
		httpPostRequest.trim();
		
		#ifdef SERIALDEBUG
		Serial.println("Chaîne POST:");
		Serial.println(httpPostRequest);
		#endif
		
		handlePost();
	}
    } // end if (client)	  
}
