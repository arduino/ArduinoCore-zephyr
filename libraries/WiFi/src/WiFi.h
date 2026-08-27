/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <Arduino.h>
#include <optional>
#include <vector>

#include "SocketHelpers.h"
#include "utility/wl_definitions.h"
#include <zephyr/net/wifi_mgmt.h>
#include "ZephyrClient.h"

using WiFiClient = ZephyrClient;

#define MAX_SCAN_RESULTS 20u

#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                            \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                          \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED |                        \
	 NET_EVENT_WIFI_SCAN_RESULT)

class WiFiClass : public NetworkInterface {
public:
	WiFiClass();
	~WiFiClass() = default;

	int begin(const char *ssid, const char *passphrase, wl_enc_type security = ENC_TYPE_UNKNOWN,
			  bool blocking = true);
	bool beginAP(char *ssid, char *passphrase, int channel = WIFI_CHANNEL_ANY,
				 bool blocking = false);

	void handleScanResult(const struct wifi_scan_result *entry);

	const struct wifi_scan_result *getScanResults() const;

	int status();

	int8_t scanNetworks();

	char *SSID(std::optional<uint8_t> networkItem = std::nullopt);

	int32_t RSSI(std::optional<uint8_t> networkItem = std::nullopt);

	int8_t channel(std::optional<uint8_t> networkItem = std::nullopt);

	uint8_t *BSSID(uint8_t *bssid);
	uint8_t *BSSID(std::optional<uint8_t> networkItem, uint8_t *bssid);

	uint8_t encryptionType(std::optional<uint8_t> networkItem = std::nullopt);

	const char *firmwareVersion();
	const char *driverVersion();

	bool disconnect() override;

private:
	static constexpr uint8_t WIFI_FLAG_NETWORK_FOUND = 1u << 0;
	static constexpr uint8_t WIFI_FLAG_SCAN_SEQUENCE_FINISHED = 1u << 1;
	uint32_t timeout_ms = 30000u; // Default timeout for scanNetworks() is 30 seconds

	static void wifiScanEventHandler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
									 struct net_if *iface);
	void registerScanEventCallback();

	inline bool isNetworkFound() const {
		return (wifiState.flags & WIFI_FLAG_NETWORK_FOUND) != 0u;
	}

	struct WiFiState {
		struct net_if *sta_iface = nullptr;
		struct net_if *ap_iface = nullptr;
		struct wifi_connect_req_params ap_config;
		struct wifi_connect_req_params sta_config;
		struct wifi_iface_status sta_state = {0};
		char fw_version[26] = {0};
		std::vector<struct wifi_scan_result> scanResults;
		uint8_t resultCount = 0u;
		uint8_t flags = 0u;
	};

	struct net_mgmt_event_callback wifiScanCb;
	struct WiFiState wifiState;
};

extern WiFiClass WiFi; // Global Wi-Fi object
