#define MQTT_STRING_LEN 128
char MQTT_CLIENT_ID[STRING_LEN] ; 
const int MQTT_PORT = 1883 ;
char dynamicTopic [MQTT_STRING_LEN];
long MQTT_LastReconnectAttempt = 0;              // For automatic reconnection, non-blocking
long MQTT_LastUptimeSent = 0;                        // For sending MQTT Uptime every nn seconds
long MQTT_LoopIterationCount = 0;                    // For counting loop runs in the nn seconds timeframe

PubSubClient mqttClient (MqttEthernetClient);

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

void doTOGGLE(uint8_t channel){ if (isON[channel]) doOFF(channel); else doON(channel); }

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

void mqttCallback (char* topic , byte* payload , unsigned int length) {      // We will place here all that happens when a message is received on the subscribed MQTT topic(s) 

  #ifdef SERIALDEBUG2
    Serial.print ("Nouveau message, topic: ") ;
    Serial.println(topic);
  

  for (unsigned int i = 0; i < length; i++) {               // Output the payload as text on serial
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
    
  String s = String((char*)msg);      // If the payload is an integer, it goes inside this value
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
    if ( payload[0] == '1' ) doON(channel);     // Switch ON      
    if ( payload[0] == '2' ) doTOGGLE(channel); // Toggle ON <-> OFF
    if ( payload[0] == '9' && channel == 0 ) needReset = true ;
  }
  
  free(msg);
  for (int i=0 ; i<3 ; i++) free (result[i]);
  
}

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
