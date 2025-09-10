#include "SocketHelpers.h"

#include "utility/wl_definitions.h"
#include <zephyr/net/wifi_mgmt.h>

// Max number of scan results to store
#define MAX_SCAN_RESULTS 20

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

    int begin(const char* ssid, const char* passphrase, wl_enc_type security = ENC_TYPE_UNKNOWN, bool blocking = true) {
        sta_iface = net_if_get_wifi_sta();
        netif = sta_iface;
        sta_config.ssid = (const uint8_t *)ssid;
        sta_config.ssid_length = strlen(ssid);
        sta_config.psk = (const uint8_t *)passphrase;
        sta_config.psk_length = strlen(passphrase);

       	// Register the Wi-Fi event callback
	    net_mgmt_init_event_callback(&wifiCb, scanEventDispatcher, NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);

	    net_mgmt_add_event_callback(&wifiCb);

	   	(void)scanNetworks();

	   	// Check if the network we were seekin was found and attempt to connect to it
	    if(getSoughtNetworkFound() != true)
	    {
	        int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
	                sizeof(struct wifi_connect_req_params));
	        if (ret) {
	            return false;
	        }

	        NetworkInterface::begin(false, NET_EVENT_WIFI_MASK);
	        if (blocking) {
	            net_mgmt_event_wait_on_iface(sta_iface, NET_EVENT_WIFI_CONNECT_RESULT, NULL, NULL, NULL, K_FOREVER);
	        }
	    }

        return status();
    }

    bool beginAP(char* ssid, char* passphrase, int channel = WIFI_CHANNEL_ANY, bool blocking = false) {
        if (ap_iface != NULL) {
            return false;
        }
        ap_iface = net_if_get_wifi_sap();
        netif = ap_iface;
        ap_config.ssid = (const uint8_t *)ssid;
        ap_config.ssid_length = strlen(ssid);
        ap_config.psk = (const uint8_t *)passphrase;
        ap_config.psk_length = strlen(passphrase);
        ap_config.security = WIFI_SECURITY_TYPE_PSK;
        ap_config.channel = channel;
        ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;
	    ap_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

        int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &ap_config,
                sizeof(struct wifi_connect_req_params));

        if (ret) {
            return false;
        }

        enable_dhcpv4_server(ap_iface);

        if (blocking) {
            net_mgmt_event_wait_on_iface(ap_iface, NET_EVENT_WIFI_AP_ENABLE_RESULT, NULL, NULL, NULL, K_FOREVER);
        }

        return true;
    }

    int status() {
        sta_iface = net_if_get_wifi_sta();
        netif = sta_iface;
        if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, netif, &sta_state,
                sizeof(struct wifi_iface_status))) {
            return WL_NO_SHIELD;
        }

	    if (sta_state.state >= WIFI_STATE_ASSOCIATED) {
            return WL_CONNECTED;
        } else {
            return WL_DISCONNECTED;
        }
        return WL_NO_SHIELD;
    }

    uint8_t scanNetworks() {
    	resultCount = 0u;
    	setScanSequenceFinished(false);
    	setSoughtNetworkFound(false);

    	// Trigger a new scan
        net_mgmt(NET_REQUEST_WIFI_SCAN, sta_iface, nullptr, 0u);

        // Wait for the scan to finish. This is by design a blocking call
        while(getScanSequenceFinished() != true);

        return resultCount;
    }

    char* SSID() {
        if (status() == WL_CONNECTED) {
            return (char *)sta_state.ssid;
        }
        return nullptr;
    }

    int32_t RSSI() {
        if (status() == WL_CONNECTED) {
            return sta_state.rssi;
        }
        return 0;
    }

	static void scanEventDispatcher(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface)
	{
		if (instance != nullptr)
		{
			instance->handleScanEvent(cb, mgmt_event, iface);
		}
	}

    void handleScanEvent(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
		    if (mgmt_event == NET_EVENT_WIFI_SCAN_RESULT) {
		       	const struct wifi_scan_result *entry = reinterpret_cast<const struct wifi_scan_result *>(cb->info);
		        if (resultCount < MAX_SCAN_RESULTS) {
		            memcpy(&scanResults[resultCount], entry, sizeof(struct wifi_scan_result));
		            resultCount++;

		            //for each new result found, compare network name with desired one
		            if(!memcmp(entry->ssid, sta_config.ssid, entry->ssid_length))
		            {
		            	// if a match is found, add missing info to config before attempting to connect
		        		sta_config.security = entry->security;
		        		sta_config.channel = entry->channel;
		        		sta_config.band = entry->band;
		        		sta_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

		        		setSoughtNetworkFound(true);
		            }
		        }
		    }

		    if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
		    	setScanSequenceFinished(true);

		        if (resultCount = 0) {
		        	printk("No networks found.\n");
		        }
		    }
		}

		void setScanSequenceFinished(bool scanFinished)
		{
			scanSequenceFinished = scanFinished;
		}

		void setSoughtNetworkFound(bool networkFound)
		{
			soughtNetworkFound = networkFound;
		}

		bool getScanSequenceFinished(void)
		{
			return scanSequenceFinished;
		}

		bool getSoughtNetworkFound(void)
		{
			return soughtNetworkFound;
		}

		static WiFiClass* instance;

private:
    struct net_if *sta_iface = nullptr;
    struct net_if *ap_iface = nullptr;

    struct wifi_connect_req_params ap_config;
    struct wifi_connect_req_params sta_config;

    struct wifi_iface_status sta_state = { 0 };

    struct wifi_scan_result scanResults[MAX_SCAN_RESULTS];
	uint8_t resultCount;
	struct net_mgmt_event_callback wifiCb;

	bool soughtNetworkFound = false;
	bool scanSequenceFinished = false;
};

extern WiFiClass WiFi;
