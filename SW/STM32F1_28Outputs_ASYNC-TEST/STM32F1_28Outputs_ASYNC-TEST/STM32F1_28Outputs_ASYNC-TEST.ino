#include "MyTools.h"
#include "MyHTTP.h"
#include "MyMQTT.h"


void setup() {
  Init_WATCHDOG();
  Init_ALLGPIO_INPUT-PULLDOWN();
  Init_SERIAL-DEBUG();
  Init_OUTPUT();
  Init_NETWORK();
}

void loop() {
  // rien à faire dans loop puisque tout est en Async... peut-être juste la vérification de l'état des connections (IP-DHCP et MQTT)
}
