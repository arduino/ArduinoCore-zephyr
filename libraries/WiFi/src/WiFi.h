#include "SocketHelpers.h"

#include "utility/wl_definitions.h"
#include <zephyr/net/wifi_mgmt.h>

#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                      \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED |                    \
	 NET_EVENT_WIFI_SCAN_RESULT)


class WiFiClass: public NetworkInterface
{
public:
    WiFiClass() {}
    ~WiFiClass() {}

    int begin(const char* ssid, const char* passphrase, wl_enc_type security = ENC_TYPE_UNKNOWN, bool blocking = true);

    int beginAP(char* ssid, char* passphrase, int channel = WIFI_CHANNEL_ANY, bool blocking = true);

    int status();

    int8_t scanNetworks() {
        // TODO: borrow code from mbed core for scan results handling
    }

    char* SSID();
    int32_t RSSI();

    String firmwareVersion();

private:
    struct net_if *sta_iface = nullptr;
    struct net_if *ap_iface = nullptr;

    struct wifi_connect_req_params ap_config;
    struct wifi_connect_req_params sta_config;

    struct wifi_iface_status sta_state = { 0 };
};

extern WiFiClass WiFi;
