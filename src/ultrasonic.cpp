
#include <Arduino.h>

long microsecondsToCentimeters(long microseconds) {
   return microseconds / 29 / 2;
}

void pingVessel(int ping) {
  digitalWrite(ping, LOW);
  delayMicroseconds(2);
  digitalWrite(ping, HIGH);
  delayMicroseconds(10);
  digitalWrite(ping, LOW);
}

long getDistance(int ping, int echo) {
  pingVessel(ping);
  return microsecondsToCentimeters(pulseIn(echo, HIGH,10000));
}

