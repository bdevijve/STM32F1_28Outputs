#define MQTT_PORT 1883
char MQTT_CLIENT_ID[STRING_LEN] ; 




void MQTT_SendDebug() {
    char topicOut[MQTT_STRING_LEN] ;  
    char MQTT_PayLoad[STRING_LEN];
    
    #ifdef SERIALDEBUG1
      { long now = millis(); Serial.print(now); }
      Serial.print (" - uptime: "); Serial.print (millis() / 1000);
      Serial.print (" - LoopIterationCount: "); Serial.print (LoopIterationCount);
      Serial.print (" - ActionCount: "); Serial.print (ActionCount);
      Serial.print (" - freeMemory: "); Serial.println(freeMemory());
    #endif

    sprintf(topicOut,"%s%s/Uptime",dynamicTopic,settings.subTopicDebug);
    sprintf(MQTT_PayLoad,"%d",millis()/1000);
    mqttClient.publish(topicOut, MQTT_PayLoad);
    
    sprintf(topicOut,"%s%s/LoopIterationCount",dynamicTopic,settings.subTopicDebug);
    sprintf(MQTT_PayLoad,"%d",LoopIterationCount);
    mqttClient.publish(topicOut, MQTT_PayLoad);

    sprintf(topicOut,"%s%s/ActionCount",dynamicTopic,settings.subTopicDebug);
    sprintf(MQTT_PayLoad,"%d",ActionCount);
    mqttClient.publish(topicOut, MQTT_PayLoad);

    sprintf(topicOut,"%s%s/freeMemory",dynamicTopic,settings.subTopicDebug);
    sprintf(MQTT_PayLoad,"%d",freeMemory());
    mqttClient.publish(topicOut, MQTT_PayLoad);

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

  if ( !strcmp (result[1] , settings.subTopicSet) ){          // Simple, just switch on or off or toogle
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
    strcat (topicOut, "+/");
    strcat (topicOut, settings.subTopicSet);
    
    #ifdef SERIALDEBUG1
      { long now = millis(); Serial.print(now); } Serial.print(" - Subscribe to topic: ") ; Serial.println(topicOut);
    #endif
    
    mqttClient.subscribe(topicOut);
  }
  return mqttClient.connected();
}
