#include <GreenBeacon.h>

const String hwid = "[your line beacon hardware id here]";
GreenBeacon beacon; 

void setup() {
  // put your setup code here, to run once:
  beacon = GreenBeacon(hwid, "M5SmartKeyBeacon");
  beacon.start();
}

void loop() {
  // put your main code here, to run repeatedly:

}
