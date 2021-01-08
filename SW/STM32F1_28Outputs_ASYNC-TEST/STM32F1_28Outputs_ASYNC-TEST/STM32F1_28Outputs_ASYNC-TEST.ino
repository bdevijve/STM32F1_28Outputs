#define SERIALDEBUG1    // Comment to remove serial debugging info level 1
//#define SERIALDEBUG2    // Comment to remove serial debugging info level 2
//#define SERIALDEBUG9    // Comment to remove serial debugging info level 9
#define SELF_TEST       // teste toutes les sorties au démarrage
#define KEEP_BUG        // ne corrige pas le bug des channels 29 et +

#include "MyTools.h"
#include "MyHTTP.h"
#include "MyMQTT.h"

void setup() {
  Init_WATCHDOG();                  // MyTools.h
  Init_ALLGPIO_INPUTPULLDOWN();    // MyTools.h
  Init_SERIALDEBUG();              // MyTools.h
  Init_GPIO_OUTPUT();               // MyTools.h
  Init_NETWORK();                   // MyTools.h
}

void loop() {
  // rien à faire dans loop puisque tout est en Async... peut-être juste la vérification de l'état des connections (IP-DHCP et MQTT)
}
