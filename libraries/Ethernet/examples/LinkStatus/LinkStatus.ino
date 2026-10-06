/*
  Link Status
  This sketch prints the ethernet link status once per second. When the
  ethernet cable is connected the link status should go to "ON".
  DHCP is started once, when the link first comes up.
*/

#include <ZephyrEthernet.h>
#include <stdio.h>

bool networkStarted = false;

void setup() {
  Serial.begin(9600);
}

void loop() {
  EthernetLinkStatus link = Ethernet.linkStatus();
  String report;
  report.reserve(256);

  switch (link) {
    case Unknown:
      report = "Link status: UNKNOWN\r\n";
      break;

    case LinkON: {
      // begin() starts DHCP; calling it on every poll would unnecessarily restart DHCP.
      if (!networkStarted) {
        Ethernet.begin();
        networkStarted = true;
      }

      uint8_t mac[6] = {};
      Ethernet.MACAddress(mac);
      char macAddress[18];
      snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
               static_cast<unsigned int>(mac[0]), static_cast<unsigned int>(mac[1]),
               static_cast<unsigned int>(mac[2]), static_cast<unsigned int>(mac[3]),
               static_cast<unsigned int>(mac[4]), static_cast<unsigned int>(mac[5]));

      report = "Link status: ON\r\n";
      report += "  IP address:  ";
      report += Ethernet.localIP().toString();
      report += "\r\n  MAC address: ";
      report += macAddress;
      report += "\r\n  Subnet mask: ";
      report += Ethernet.subnetMask().toString();
      report += "\r\n  Gateway:     ";
      report += Ethernet.gatewayIP().toString();
      report += "\r\n";
      break;
    }

    case LinkOFF:
      report = "Link status: OFF\r\n";
      break;
  }

  report += "\r\n";
  Serial.print(report);
  delay(1000);
}
