#define MAX_HTTP_HEADER_LENGTH 350    //maximum length of http header required

void handleHTTPPost_deviceName(String req_str) {
  String httpPostRequest = String(MAX_HTTP_HEADER_LENGTH);

  httpPostRequest = req_str.substring(req_str.indexOf("deviceName="));
  httpPostRequest.trim();
    
  #ifdef SERIALDEBUG1
    Serial.println("Chaîne POST:"); Serial.println(httpPostRequest);
  #endif

  int index1 = httpPostRequest.indexOf("deviceName=") + 11;
  int index2 = httpPostRequest.indexOf("&", index1);
    
  for (int i= index1, j=0 ; i<index2; i++, j++)
    settings.deviceName[j] = httpPostRequest.charAt(i);
  settings.deviceName[(index2 - index1 )] = 0;

  // mqttserverIP  :Convert char array IP address to IPAddress format...
  char ipString[STRING_LEN];
  bool successIPAdress = false;
  index1 = httpPostRequest.indexOf("mqttServerIP=") + 13;   index2 = httpPostRequest.indexOf("&", index1);
  for (int i= index1, j=0 ; i<index2; i++, j++) ipString[j] = httpPostRequest.charAt(i);
  ipString[(index2 - index1 )] = 0;
  sscanf(ipString, "%u.%u.%u.%u", &settings.mqttServerIP[0], &settings.mqttServerIP[1], &settings.mqttServerIP[2], &settings.mqttServerIP[3]);
  
  index1 = httpPostRequest.indexOf("mqttPrefix=") + 11;     index2 = httpPostRequest.indexOf("&", index1);
  for (int i= index1, j=0 ; i<index2; i++, j++) settings.mqttPrefix[j] = httpPostRequest.charAt(i);
  settings.mqttPrefix[(index2 - index1)] = 0;
    
  index1 = httpPostRequest.indexOf("subTopicStatus=") + 15; index2 = httpPostRequest.indexOf("&", index1);
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
    
  needReset = true ;
}


void handleHTTPPost_switch_output(String req_str) {
  String httpPostRequest = String(MAX_HTTP_HEADER_LENGTH);

  httpPostRequest = req_str.substring(req_str.indexOf("switch_output="));
  httpPostRequest.trim();
    
  #ifdef SERIALDEBUG1
    Serial.println("Chaîne POST:"); Serial.println(httpPostRequest);
  #endif

//  int index1 = httpPostRequest.indexOf("deviceName=") + 11;
//  int index2 = httpPostRequest.indexOf("&", index1);

  #ifdef SERIALDEBUG1
    Serial.println("startWith switch_output=");
  #endif
  
  doTOGGLE(10);
}

void writeHTTPResponse(EthernetClient client) {  // send a standard http response header
  #ifdef SERIALDEBUG1
    { long now = millis(); Serial.print(now); }  Serial.println(" - writeHTTPResponse");
  #endif
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
  client.println(R"FOO(<!DOCTYPE html><html><head><title>Module sortie relais: r&eacute;glages</title></head><body>)FOO");

  client.println(R"FOO(<fieldset><legend><h2>Etat des sorties :</h2></legend> 
    <form accept-charset="UTF-8" autocomplete="off" method="POST">
    <table border="1" style="border-collapse: collapse; width: 100%;">)FOO");

  client.println("<tr>");
  for (int pinNumber = 0; pinNumber < OUTPUT_COUNT+1; pinNumber++) {
    client.print(R"FOO(<td style="text-align: center;"><button type="submit" name="switch_output" value=")FOO");
    client.print(pinNumber);
    client.print(R"FOO(">)FOO");
    client.print(pinNumber);    
    client.println("</button></td>");   
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
  client.println("</tr></table></form>");
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
    <button type="submit" value="EcrireEEPROM">Valider les modifications et red&eacute;marrer</button>
    </form>
    </fieldset>
    )FOO");
}
