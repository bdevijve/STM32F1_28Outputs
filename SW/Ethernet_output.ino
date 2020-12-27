#include <ArduinoUniqueID.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <PubSubClient.h>
#include "myGamma.h"


#define STRING_LEN 30
#define MQTT_STRING_LEN 128
#define OUTPUT_COUNT 28



uint16_t 	MAX_POWER = 2047;
const int MQTT_PORT = 1883 ;
char dynamicTopic [MQTT_STRING_LEN];



uint8_t output[] = {
PE12, PE13, PE14, PB8, PA0, PA1, PA2, PA3, PB9, PB0, PB1, PE8, PE9, PE10, PE11, PB4, PB3, PA15, PB10, PB11, PD12, PD13, PD14, PD15, PC6, PC7, PC8, PC9
};

uint8_t outputCount = 28 ; 
char MQTT_CLIENT_ID[STRING_LEN] ; 


byte mac[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05} ; 

IPAddress ip(192, 168, 1, 177);

//#define SERIALDEBUG 		// Comment to remove serial debugging info
//#define SELF_TEST			// teste toutes les sorties au démarrage
#define MIN_DELAY 0
#define MAX_DELAY 1000			// in ms
#define MIN_INTERVAL 20			// Minimum fading time, in milliseconds
#define MAX_INTERVAL 10000		// maximum fading time, in milliseconds
#define DEFAULT_FADING_TIME 200   
#define MIN_POWER 0
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

#define  defaultMqttServerIP 192,168,1,22
#define defaultDeviceName "testOutput"
#define defaultMqttPrefix "relais"
#define  defaultSubTopicStatus  "status" 
#define   defaultSubTopicSet  "set"
#define   defaultSubTopicPower  "power"
#define   defaultSubTopicTransition  "transition"
#define   defaultSubTopicDelay  "delay"
#define   defaultSubTopicUpdate  "update"
#define   defaultSubTopicPwmFrequency  "frequency"


struct mySettings {	
	char deviceName [STRING_LEN] ;
	IPAddress mqttServerIP;
	char mqttPrefix [STRING_LEN] ;
	char subTopicStatus [STRING_LEN] ;
	char subTopicSet [STRING_LEN] ;
	char subTopicPower [STRING_LEN] ;
	char subTopicTransition [STRING_LEN] ;
	char subTopicDelay [STRING_LEN] ;
	char subTopicUpdate [STRING_LEN] ;
	char subTopicPwmFrequency [STRING_LEN] ;
};


struct mySettings settings;

struct mySettings defaultSettings = (mySettings) {
	defaultDeviceName,
	IPAddress(192,168,1,22),
	defaultMqttPrefix,
	defaultSubTopicStatus, 
	defaultSubTopicSet, 
	defaultSubTopicPower, 
	defaultSubTopicTransition, 
	defaultSubTopicDelay, 
	defaultSubTopicUpdate, 
	defaultSubTopicPwmFrequency
};


bool needMqttConnect = false;
bool needReset = false;
unsigned long lastMqttConnectionAttempt = 0;

uint8_t LEDFaderChannelNumber = outputCount ;
uint8_t LEDFaderChannel[] = {PE12, PE13, PE14, PB8, PA0, PA1, PA2, PA3, PB9, PB0, PB1, PE8, PE9, PE10, PE11, PB4, PB3, PA15, PB10, PB11, PD12, PD13, PD14, PD15, PC6, PC7, PC8, PC9} ;							// PINs PC6-7-8-9 are channels 1-2-3-4 of timer TIM3
unsigned long LEDFaderLastStepTime [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
unsigned int LEDFaderInterval [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
uint16_t LEDFaderColor []= {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
uint16_t LEDFaderToColor [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
unsigned int LEDFaderDuration [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
float LEDFaderPercentDone [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;

uint16_t DELAY [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
uint16_t FADE_TIME [] = {DEFAULT_FADING_TIME,DEFAULT_FADING_TIME,DEFAULT_FADING_TIME,DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME, DEFAULT_FADING_TIME} ;  								// Time set for fading in ms
uint8_t isON [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;
uint16_t lightPowerMemory [] = {0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} ;	// Will store the last set power level




uint16_t LEDFaderExponential(uint16_t i) {						// Returns the value read from the exponential table. 
	return myGamma[i];
}

uint16_t LEDFaderLinear(uint16_t i) {
	return i;
}

uint16_t LEDFaderReverse(uint16_t i) {
	return MAX_POWER-i;
}

void LEDFaderSetPin(uint8_t channel, uint8_t pwm_pin) {			// For setup purposes
  LEDFaderChannel [channel] = pwm_pin;
}

uint8_t LEDFaderGetPin(uint8_t channel){						// Not used...
  return LEDFaderChannel [channel];
}

bool LEDFaderSetValue(uint8_t channel, uint16_t value) {				// Updates the PCA9685 controller with the current output values 


  LEDFaderColor [channel] = (uint16_t)constrain(value, 0, MAX_POWER);		// Keep the value within boundaries, in case...

  //analogWrite(LEDFaderChannel[channel], LEDFaderExponential(value));	// Updates the value for the given channel on the PWM pin
	
	/*
	if (value == 0 ) {
		digitalWrite(LEDFaderChannel[channel], 0);
	}else {
		digitalWrite(LEDFaderChannel[channel], 1);
	}*/
  
  
  #ifdef SERIALDEBUG
	Serial.print ("Set Value, channel: ");
	Serial.print (channel);
	Serial.print (", value: ");
	Serial.println (value);
	#endif
  
  return 1;

}


uint8_t LEDFaderFade(uint8_t channel, uint16_t value, unsigned int time) {			// Sets the target value for the light
  LEDFaderStopFade(channel);				// We stop the current fading if there's one
  LEDFaderPercentDone [channel] = 0;

  
  if (value == LEDFaderColor  [channel]) { 	// Color hasn't changed
    return 2;
  }

  if (time <= MIN_INTERVAL) {				// If the fade time is less than MIN_INTERVAL (default 20ms), there's no fade
    LEDFaderSetValue(channel, value);
    return 2;
  }

  LEDFaderDuration [channel] = time;												// This will be used to calculate the light value
  LEDFaderToColor [channel] = (uint16_t)constrain(value, 0, MAX_POWER);				// To keep it within boundaries...

  // Figure out what the interval should be so that we're changing the color by at least 1 each cycle
  // (minimum interval is MIN_INTERVAL)
  float color_diff = abs(LEDFaderColor [channel] - LEDFaderToColor [channel]);
  LEDFaderInterval [channel] = round((float)LEDFaderDuration [channel] / color_diff);
  if (LEDFaderInterval [channel] < MIN_INTERVAL) {
    LEDFaderInterval [channel] = MIN_INTERVAL;
  }

  LEDFaderLastStepTime [channel] = millis();

  return 1;
}

bool LEDFaderIsFading(uint8_t channel) {

  if (LEDFaderDuration [channel] > 0)  return true;
  else return false;
}



void LEDFaderStopFade(uint8_t channel) {			// Stops fading, the light intensity stays where it is at the moment.
  LEDFaderPercentDone [channel] = 100;
  LEDFaderDuration [channel] = 0;
}


uint8_t LEDFaderGetProgress(uint8_t channel) {
  return round(LEDFaderPercentDone [channel]);
}

bool LEDFaderUpdate(uint8_t channel) {				// this is called in the main loop to update each channel


  // No fade
  if (LEDFaderDuration [channel] == 0) {
    return false;
  }

  unsigned long now = millis();
  unsigned int time_diff = now - LEDFaderLastStepTime [channel];

  // Interval hasn't passed yet
  if (time_diff < LEDFaderInterval [channel] ) {
    return true;
  }

  // How far along have we gone since last update
  float percent = (float)time_diff / (float)LEDFaderDuration [channel];
  LEDFaderPercentDone[channel] += percent;

  // If we've hit 100%, set LED to the final color
  if (percent >= 1) {
    LEDFaderStopFade(channel);
    LEDFaderSetValue(channel, LEDFaderToColor [channel]);
	
  // Check if the light is on or off and update the status
  
	if (isON[channel]) if (!LEDFaderColor[channel]){
	  doOFF(channel, 0);
	  isON[channel] = 0;
  }
  
  if (!isON[channel]) if (LEDFaderColor[channel]) {
	  doON(channel, 0);
	  isON[channel] = 1;
  }
	
    return false;
  }

  // Else, move color to where it should be
  int color_diff = LEDFaderToColor [channel] - LEDFaderColor [channel] ;
  int increment = round(color_diff * percent);

  LEDFaderSetValue(channel, LEDFaderColor [channel] + increment);
  

  

		    

  // Update time and finish
  LEDFaderDuration [channel] -= time_diff;
  LEDFaderLastStepTime [channel] = millis();
  return true;
}

bool mqttReconnect() {							// Code to test the MQTT connection and reconnect if necessary, does not block the processor
  if (mqttClient.connect(MQTT_CLIENT_ID)) {		// Unique clientID
    char topicOut[MQTT_STRING_LEN] ;
	
	strcpy (topicOut, dynamicTopic);
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "Client ESP connecté. Bonjour!");
    
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
		
		for (unsigned int i = 0; i < length; i++) {
		
		Serial.print((char)msg[i]);
	}
	 Serial.println(); 
	
	 Serial.print("Détection nombre:");
	
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
		
	uint8_t channel =5;
   

	
	
	
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
	
	
	// Now figure out what to do: 0-> OFF, 1-> ON, 2-> toggle, 3-> enter "continuous fade", 4-> stop 'continuous fade", 5-> reset board
	
	if ( !strcmp (result[1] , settings.subTopicSet)){ 			// Simple, just switch on or off
	
		if (payload[0] == '1') {		// Switch ON
			doON(channel, 1);
			//isON[channel] = 1;				
		}
		if (payload[0] == '0') {		// Switch OFF
			doOFF(channel, 1);
			//isON[channel] = 0;
		}
		if (payload[0] == '2') {		// Toggles the light's state
			if (!isON[channel]) {
				doON(channel, 1) ;
				//isON[channel] = 1;
			}
			else {
				doOFF(channel, 1) ;
				//isON[channel] = 0;
			}
		}
		
		if (payload[0] == '3'){
			
			// do something to enter continous fade
		}
		
		if (payload[0] == '4'){
			
			// do something to stop continuous fade
		}
		
		if (payload[0] == '5'){
			
			needReset = true ;
		}
		
	}
	
	
	if ( !(strcmp (result[1] ,settings.subTopicPower) || strcmp (result[2] ,settings.subTopicSet))){		// We set the light to the required power, and save the power value
		
		setPower (channel, valeur);							
		lightPowerMemory[channel] = valeur;
	}
	
	if ( !(strcmp (result[1] ,settings.subTopicTransition) || strcmp (result[2] ,settings.subTopicSet))){		// We set the transition to the required value for this channel
		
		setTransition (channel,valeur);							
				
	}
	
	if ( !(strcmp (result[1] ,settings.subTopicDelay) || strcmp (result[2] ,settings.subTopicSet))){		// We set the light to the required delay for this channel
		
		setDelay (channel, valeur);							
		
	}
	
	free(msg);
	for (int i=0 ; i<3 ; i++) free (result[i]);
	
}



void doON(uint8_t channel, bool changeValue){

	char topicOut [MQTT_STRING_LEN];
	char convert [5];
		
	if (changeValue) digitalWrite(output[channel], 1);
	//LEDFaderFade(channel, lightPowerMemory[channel], FADE_TIME [channel]); 		// Switches light ON, to the previously set value
	
	strcpy (topicOut,dynamicTopic);
	sprintf (convert, "%i", channel);
	strcat (topicOut, convert);
	strcat (topicOut, "/");
	strcat (topicOut, settings.subTopicStatus);
	mqttClient.publish(topicOut, "1", 1);
	
		#ifdef SERIALDEBUG
	 Serial.println ("Got it! ON");
		#endif
		
	isON[channel]  = 1 ;

}


void doOFF(uint8_t channel, bool changeValue){
		
	char topicOut [MQTT_STRING_LEN];
	char convert [5];
	
	if (changeValue) digitalWrite(output[channel], 0);
	//LEDFaderFade(channel, 0, FADE_TIME [channel]);
  
	
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

void setPower(uint8_t channel,  uint16_t power){
	
		char topicOut [MQTT_STRING_LEN];
		char convert [5];
		
		//LEDFaderFade(channel, power, FADE_TIME [channel]);
		
			
		
		strcpy (topicOut, dynamicTopic);
		sprintf (convert, "%i", channel);
		strcat (topicOut, convert);
		strcat (topicOut, "/");
		strcat (topicOut, settings.subTopicPower);
	
		
		sprintf (convert, "%i", power);
		mqttClient.publish(topicOut, convert);
	
		#ifdef SERIALDEBUG
		 Serial.print ("Puissance réglée à :");
		 Serial.println (power);		
		#endif
	
}

void setTransition( uint8_t channel, uint16_t transition_value){
	
		char topicOut [MQTT_STRING_LEN];	
		char convert [5];
	
		FADE_TIME [channel] = transition_value ; 		// updates the value
		
					// then outputs acknowledgment
		strcpy (topicOut, dynamicTopic);
		sprintf (convert, "%i", channel);
		strcat (topicOut, convert);
		strcat (topicOut, "/");
		strcat (topicOut, settings.subTopicTransition);
	
		
		sprintf (convert, "%i", FADE_TIME [channel]);
		mqttClient.publish(topicOut, convert);
	
		#ifdef SERIALDEBUG
		 Serial.print ("Temps de transition réglé à :");
		 Serial.println (FADE_TIME [channel]);		
		#endif
	
}


void setDelay( uint8_t channel, uint16_t delay_value){
	
		char topicOut [MQTT_STRING_LEN];	
		char convert [5];
		
		DELAY [channel] = delay_value ; 		// updates the value
		
					// then outputs acknowledgment
		strcpy (topicOut, dynamicTopic);
		sprintf (convert, "%i", channel);
		strcat (topicOut, convert);
		strcat (topicOut, "/");
		strcat (topicOut, settings.subTopicDelay);
	
		
		sprintf (convert, "%i", DELAY [channel]);
		mqttClient.publish(topicOut, convert);
		
		#ifdef SERIALDEBUG
		 Serial.print ("Délai réglé à :");
		 Serial.println (FADE_TIME [channel]);		
		#endif
	
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
	
	index1 = httpPostRequest.indexOf("subTopicPower=") + 14;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicPower[j] = httpPostRequest.charAt(i);
	settings.subTopicPower[(index2 - index1 )] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicTransition=") + 19;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicTransition[j] = httpPostRequest.charAt(i);
	settings.subTopicTransition[(index2 - index1 )] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicDelay=") + 14;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicDelay[j] = httpPostRequest.charAt(i);
	settings.subTopicDelay[(index2 - index1 )] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicUpdate=") + 15;
	index2 = httpPostRequest.indexOf("&", index1);
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicUpdate[j] = httpPostRequest.charAt(i);
	settings.subTopicUpdate[(index2 - index1 )] = 0;
	
	index1 = httpPostRequest.indexOf("subTopicPwmFrequency=") + 21;
	index2 = httpPostRequest.length();
	for (int i= index1, j=0 ; i<index2; i++, j++) 
		settings.subTopicPwmFrequency[j] = httpPostRequest.charAt(i);
	settings.subTopicPwmFrequency[(index2 - index1 )] = 0;
	
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
	Serial.println(settings.subTopicPower);
	Serial.println(settings.subTopicTransition);
	Serial.println(settings.subTopicDelay);
	Serial.println(settings.subTopicUpdate);
	Serial.println(settings.subTopicPwmFrequency);
	
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
	Serial.println("Ecriture terminée. Redémarrrage...");
	#endif
	
	delay(200);
	
	NVIC_SystemReset();
	
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
				
	} else{
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
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/(canal 0-27)/");client.print(settings.subTopicSet); client.print("/");
	client.print(R"FOO(</p><p>R&eacute;glages:</p><p>Puissance:&nbsp;)FOO");
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/(canal 0-27)/");client.print(settings.subTopicPower); client.print("/");
	client.print(R"FOO(</p><p>Dur&eacute;e de transition:&nbsp;)FOO");
	client.print(settings.mqttPrefix); client.print("/"); client.print(settings.deviceName); client.print("/(canal 0-27)/");client.print(settings.subTopicTransition); client.print("/");
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
			<label for="subTopicPowerLabel">Sous-topic r&eacute;glage puissance :</label><br /> 
			<input name="subTopicPower" type="text" value=")FOO");
	client.print(settings.subTopicPower);
	client.print(R"FOO(" /> <br /> <br>
			<label for="subTopicTransitionLabel">Sous-topic dur&eacute;e de transition :</label><br /> 
			<input name="subTopicTransition" type="text" value=")FOO");
	client.print(settings.subTopicTransition);
	client.print(R"FOO(" /> <br /> <br>
			<label for="subTopicDelayLabel">Sous-topic D&eacute;lai :</label><br /> 
			<input name="subTopicDelay" type="text" value=")FOO");
	client.print(settings.subTopicDelay);
	client.print(R"FOO(" /> <br /> <br>
			<label for="subTopicUpdateLabel">Sous-topic mise &agrave; jour :</label><br /> 
			<input name="subTopicUpdate" type="text" value=")FOO");
	client.print(settings.subTopicUpdate);
	client.print(R"FOO(" /> <br /> <br>
			<label for="subTopicPwmFrequencyLabel">Sous-topic fr&eacute;quence PWM :</label><br /> 
			<input name="subTopicPwmFrequency" type="text" value=")FOO");
	client.print(settings.subTopicPwmFrequency);
	client.print(R"FOO(" /> <br /> <br>
			<button type="submit" value="Submit">Valider les modifications et red&eacute;marrer</button>
			</fieldset>
			</form>)FOO");

  }



void setup() {
  
  // initialize output pins
  
  for (int pinNumber = 0; pinNumber < outputCount; pinNumber++) {
    pinMode(output[pinNumber], OUTPUT);
	}

  for (int pinNumber = 0; pinNumber < outputCount; pinNumber++) {
    digitalWrite(output[pinNumber], LOW);
  }
  
  Serial.begin(115200);
  delay(200);
  
  #ifdef SELF_TEST
  for (int i=0; i<outputCount ; i++){
	  digitalWrite(output[i] , 1);
	  delay(20);
	  digitalWrite(output[i], 0);
  }
  
   for (int i=outputCount-1; i>=0; i--){
	  digitalWrite(output[i] , 1);
	  delay(20);
	  digitalWrite(output[i], 0);
  }
  #endif
  
  
  #ifdef SERIALDEBUG
  Serial.println("Hello");
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
	

  for(size_t i = 0; i < 6; i++)
  mac[i] = UniqueID8[i]; 
	
	
	// if invalid MAC, change first byte
  if((mac[0]&0x03)!=2) {   
    mac[0]&=0xFC;mac[0]|=0x2;
  }
  
  readStoredVariables();
  
  #ifdef SERIALDEBUG
  Serial.println ("Lecture EEPROM:");
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
  Serial.println(settings.subTopicPower);
  Serial.println(settings.subTopicTransition);
  Serial.println(settings.subTopicDelay);
  Serial.println(settings.subTopicUpdate);
  Serial.println(settings.subTopicPwmFrequency);
	#endif
  
  analogWriteResolution (11);
  analogWriteFrequency(35000);
    
  for (int i=0 ; i<outputCount ; i++) {
	LEDFaderSetValue(i,0);
  }

  
  #ifdef SERIALDEBUG
      Serial.println("Initialisation Ethernet...") ;
	#endif
	
  // start the Ethernet connection and the server:  
  Ethernet.init(ETH_CS_PIN);
  Ethernet.begin(mac);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    
	Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");

    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    
	Serial.println("Ethernet cable is not connected.");
	
  }

  // start the server
  server.begin();
  #ifdef SERIALDEBUG
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  #endif
  
  //delay(1500);
  
  	#ifdef SERIALDEBUG
   Serial.println("OK");
   Serial.println("Initialisation MQTT... ") ;
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
	Serial.println("Fin de l'initialisation");
    #endif
}


void loop() {
	
	if (needReset){
		Serial.println("Rebooting.");
		NVIC_SystemReset();  
	}
  
  for (int i=0 ; i<outputCount ; i++) {
	LEDFaderUpdate(i);
	}
	
	if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      #ifdef SERIALDEBUG
	  Serial.println ("MQTT connecting...");
	  #endif
	  
	  if (mqttReconnect()) {
        #ifdef SERIALDEBUG
		Serial.println ("MQTT connected");
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


