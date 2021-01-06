#define MQTT_STRING_LEN 128
char dynamicTopic [MQTT_STRING_LEN];
#define STRING_LEN 30

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

uint16_t GPIOINIT_TABLE[] = {
  PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10, PA11, PA12, PA13, PA14, PA15,
  PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, PB8, PB9, PB10, PB11, PB12, PB13, PB14, PB15,
  PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13, PC14, PC15,
  PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7, PD8, PD9, PD10, PD11, PD12, PD13, PD14, PD15,
  PE0, PE1, PE2, PE3, PE4, PE5, PE6, PE7, PE8, PE9, PE10, PE11, PE12, PE13, PE14, PE15,
  0xffff
};

#define OUTPUT_COUNT 28       // Nb de sortie externe réel, sachant que la LED interne d'état sera comptabilisé en +

uint8_t GPIO_OUTPUT[] = {
  PC13, PE12, PE13, PE14, PB8, PA0, PA1, PA2, PA3, PB9, PB0, PB1, PE8, PE9, PE10, PE11, PB4, PB3, PA15, PB10, PB11, PD12, PD13, PD14, PD15, PC6, PC7, PC8, PC9
};

uint8_t isON [] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} ;


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

void doON(uint8_t channel){

  char topicOut [MQTT_STRING_LEN];
  char convert [5];

  #ifndef KEEP_BUG
    if (channel <= OUTPUT_COUNT) 
  #endif
  {
    digitalWrite(GPIO_OUTPUT[channel], 1);
    isON[channel] = 1 ;
    #ifdef SERIALDEBUG9
      { long now = millis(); Serial.print(now); }  Serial.print (" - Got it! ON - channel "); Serial.println (channel);
    #endif
    strcpy (topicOut,dynamicTopic);
    sprintf (convert, "%i", channel);
    strcat (topicOut, convert);
    strcat (topicOut, "/");
    strcat (topicOut, settings.subTopicStatus);
    mqttClient.publish(topicOut, "1", true);
  }
}

void doOFF(uint8_t channel){
  char topicOut [MQTT_STRING_LEN];
  char convert [5];
  
  #ifndef KEEP_BUG
    if (channel <= OUTPUT_COUNT) 
  #endif
  {
    digitalWrite(GPIO_OUTPUT[channel], 0);
    isON[channel] = 0 ;
    #ifdef SERIALDEBUG9
      { long now = millis(); Serial.print(now); }  Serial.print (" - Got it! OFF - channel "); Serial.println (channel);
    #endif
    strcpy (topicOut,dynamicTopic);
    sprintf (convert, "%i", channel);
    strcat (topicOut, convert);
    strcat (topicOut, "/");
    strcat (topicOut, settings.subTopicStatus);
    mqttClient.publish(topicOut, "0", true);
  }
}

void doTOGGLE(uint8_t channel){
  #ifndef KEEP_BUG
    if (channel <= OUTPUT_COUNT) 
  #endif
  {
   if (isON[channel]) doOFF(channel); else doON(channel);
  }
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
