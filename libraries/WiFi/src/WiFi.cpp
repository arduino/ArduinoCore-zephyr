#include "WiFi.h"

WiFiClass WiFi;

String WiFiClass::firmwareVersion() { // TODO integrate fw version detection
#if defined(ARDUINO_PORTENTA_C33)
    return "v1.5.0";
#elif defined(ARDUINO_PORTENTA_H7) || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA) ||\
    defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_NICLA_SENSE_ME)
    return "v0.0.0";
#else
    return "v0.0.0";
#endif
}


int WiFiClass::begin(const char* ssid, const char* passphrase, wl_enc_type security, bool blocking) {
    sta_iface = net_if_get_wifi_sta();
    netif = sta_iface;
    sta_config.ssid = (const uint8_t *)ssid;
    sta_config.ssid_length = strlen(ssid);
    sta_config.psk = (const uint8_t *)passphrase;
    sta_config.psk_length = strlen(passphrase);
    // TODO: change these fields with scan() results
    sta_config.security = WIFI_SECURITY_TYPE_PSK;
    sta_config.channel = WIFI_CHANNEL_ANY;
    sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;
    sta_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
            sizeof(struct wifi_connect_req_params));
    if (ret) {
        return false;
    }

    if (blocking) {
        net_mgmt_event_wait_on_iface(sta_iface, NET_EVENT_WIFI_CONNECT_RESULT, NULL, NULL, NULL, K_FOREVER);
    }
    // FIXME verify that non in blocking version dhcp can be called even if connect is not completed
    NetworkInterface::begin(blocking, NET_EVENT_WIFI_MASK);

    return status();
}

int WiFiClass::beginAP(char* ssid, char* passphrase, int channel, bool blocking) {
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

    return status();
}

int WiFiClass::status() {
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

char* WiFiClass::SSID() {
    if (status() == WL_CONNECTED) {
        return (char *)sta_state.ssid;
    }
    return nullptr;
}

int32_t WiFiClass::RSSI() {
    if (status() == WL_CONNECTED) {
        return sta_state.rssi;
    }
    return 0;
}
