/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "WiFi.h"

#include <errno.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(WiFi, LOG_LEVEL_NONE);

namespace {

constexpr wifi_security_type encryptionTypeToSecType(wl_enc_type enc_type) {
	switch (enc_type) {
	case ENC_TYPE_WEP:
		return WIFI_SECURITY_TYPE_WEP;
	case ENC_TYPE_WPA:
		return WIFI_SECURITY_TYPE_WPA_PSK;
	case ENC_TYPE_WPA2:
		return WIFI_SECURITY_TYPE_PSK;
	case ENC_TYPE_WPA3:
		return WIFI_SECURITY_TYPE_SAE;
	case ENC_TYPE_NONE:
		return WIFI_SECURITY_TYPE_NONE;
	case ENC_TYPE_UNKNOWN:
	case ENC_TYPE_AUTO:
	default:
		return WIFI_SECURITY_TYPE_UNKNOWN;
	}
}

constexpr wl_enc_type secTypeToEncryptionType(wifi_security_type security) {
	switch (security) {
	case WIFI_SECURITY_TYPE_WEP:
		return ENC_TYPE_WEP;
	case WIFI_SECURITY_TYPE_PSK:
		return ENC_TYPE_WPA2;
	case WIFI_SECURITY_TYPE_WPA_AUTO_PERSONAL:
		return ENC_TYPE_WPA2;
	case WIFI_SECURITY_TYPE_SAE:
		return ENC_TYPE_WPA3;
	case WIFI_SECURITY_TYPE_SAE_AUTO:
		return ENC_TYPE_WPA3;
	case WIFI_SECURITY_TYPE_NONE:
		return ENC_TYPE_NONE;
	default:
		return ENC_TYPE_UNKNOWN;
	}
}

} // namespace

WiFiClass WiFi;

WiFiClass::WiFiClass() {
}

void WiFiClass::wifiScanEventHandler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
									 struct net_if *iface) {
	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_WIFI_SCAN_RESULT) {
		const struct wifi_scan_result *entry =
			reinterpret_cast<const struct wifi_scan_result *>(cb->info);
		WiFi.handleScanResult(entry);
	} else if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
		WiFi.wifiState.flags |= WIFI_FLAG_SCAN_SEQUENCE_FINISHED;
	}
}

void WiFiClass::registerScanEventCallback() {
	net_mgmt_del_event_callback(&wifiScanCb);
	net_mgmt_init_event_callback(&wifiScanCb, wifiScanEventHandler,
								 NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&wifiScanCb);
}

const char *WiFiClass::driverVersion() {
	struct wifi_version ver = {};

	if (netif == nullptr) {
		wifiState.sta_iface = net_if_get_wifi_sta();
		netif = wifiState.sta_iface;
	}

	if (netif == nullptr ||
		net_mgmt(NET_REQUEST_WIFI_VERSION, netif, &ver, sizeof(ver)) != 0 ||
		ver.drv_version == nullptr) {
		return "unknown";
	}

	return ver.drv_version;
}

const char *WiFiClass::firmwareVersion() {
	static constexpr const char unknownVersion[] = "v0.0.0";
	struct wifi_version version = {};
	struct net_if *iface = net_if_get_wifi_sta();

	if (netif == nullptr) {
		wifiState.sta_iface = net_if_get_wifi_sta();
		netif = wifiState.sta_iface;
	}

	if (iface == nullptr ||
		net_mgmt(NET_REQUEST_WIFI_VERSION, iface, &version, sizeof(version)) != 0) {
		return unknownVersion;
	}

	if (version.fw_version != nullptr && version.fw_version[0] != '\0') {
		return version.fw_version;
	}

	return unknownVersion;
}

int WiFiClass::begin(const char *ssid, const char *passphrase, wl_enc_type security,
					 bool blocking) {
	wifi_security_type wifi_security = encryptionTypeToSecType(security);

	wifiState.sta_iface = net_if_get_wifi_sta();
	netif = wifiState.sta_iface;
	wifiState.sta_config.ssid = (const uint8_t *)ssid;
	wifiState.sta_config.ssid_length = strlen(ssid);
	if (passphrase != nullptr) {
		wifiState.sta_config.psk = (const uint8_t *)passphrase;
		wifiState.sta_config.psk_length = strlen(passphrase);
	} else {
		wifiState.sta_config.psk = nullptr;
		wifiState.sta_config.psk_length = 0u;
	}

	wifiState.sta_config.channel = WIFI_CHANNEL_ANY;
	wifiState.sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;
	wifiState.sta_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;
	/* security will be updated in case of ENC_TYPE_UNKNOWN in handleScanResult when the network is
	 * found during scanning */
	wifiState.sta_config.security = wifi_security;

	if (!net_if_is_up(netif)) {
		net_if_up(netif);
	}

	if (security == ENC_TYPE_UNKNOWN) {
		int8_t scan_result_count = scanNetworks();

		if (scan_result_count < 0 || !isNetworkFound()) {
			/* When scan is flaky try PSK connect anyway. */
			wifiState.sta_config.security = WIFI_SECURITY_TYPE_PSK;
			LOG_WRN("Scan did not determine security (scan_result=%d), falling back to PSK connect",
					scan_result_count);
		}

		/*
		 * Small delay to ensure the driver is ready for the next command: the Arduino
		 * WIFI_FLAG_SCAN_SEQUENCE_FINISHED flag that we use as a marker can be set slightly before
		 * the driver finishes its internal scan cleanup.
		 */
		k_sleep(K_MSEC(100));
	}

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifiState.sta_iface, &wifiState.sta_config,
					   sizeof(struct wifi_connect_req_params));
	if (ret) {
		return false;
	}

	ret = status();
	if (ret != WL_CONNECTED && blocking) {
		net_mgmt_event_wait_on_iface(wifiState.sta_iface, NET_EVENT_WIFI_CONNECT_RESULT, NULL, NULL,
									 NULL, K_FOREVER);
	}
	NetworkInterface::begin(blocking, NET_EVENT_WIFI_MASK);
	return status();
}

bool WiFiClass::beginAP(char *ssid, char *passphrase, int channel, bool blocking) {
	if (wifiState.ap_iface != nullptr) {
		return false;
	}

	wifiState.ap_iface = net_if_get_wifi_sap();
	netif = wifiState.ap_iface;
	wifiState.ap_config.ssid = (const uint8_t *)ssid;
	wifiState.ap_config.ssid_length = strlen(ssid);
	wifiState.ap_config.psk = (const uint8_t *)passphrase;
	wifiState.ap_config.psk_length = strlen(passphrase);
	wifiState.ap_config.security = WIFI_SECURITY_TYPE_PSK;
	wifiState.ap_config.channel = channel;
	wifiState.ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;
	wifiState.ap_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;

	if (!net_if_is_up(netif)) {
		net_if_up(netif);
	}

	int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, wifiState.ap_iface, &wifiState.ap_config,
					   sizeof(struct wifi_connect_req_params));
	if (ret) {
		return false;
	}

	enable_dhcpv4_server(wifiState.ap_iface);

	if (blocking) {
		net_mgmt_event_wait_on_iface(wifiState.ap_iface, NET_EVENT_WIFI_AP_ENABLE_RESULT, NULL,
									 NULL, NULL, K_FOREVER);
	}

	return true;
}

void WiFiClass::handleScanResult(const struct wifi_scan_result *entry) {
	if (wifiState.scanResults.size() < MAX_SCAN_RESULTS) {
		wifiState.scanResults.push_back(*entry);
		wifiState.resultCount = wifiState.scanResults.size();
	} else {
		LOG_WRN("WiFi scan result buffer full (%u), dropping additional result", MAX_SCAN_RESULTS);
	}

	if (entry->ssid_length == wifiState.sta_config.ssid_length &&
		strncmp(reinterpret_cast<const char *>(entry->ssid),
				reinterpret_cast<const char *>(wifiState.sta_config.ssid),
				entry->ssid_length) == 0) {
		wifiState.sta_config.security = entry->security;
		wifiState.sta_config.channel = entry->channel;
		wifiState.sta_config.band = entry->band;
		wifiState.sta_config.bandwidth = WIFI_FREQ_BANDWIDTH_20MHZ;
		wifiState.flags |= WIFI_FLAG_NETWORK_FOUND;

		LOG_INF("Network found: %.*s, RSSI: %d, Security: %d", entry->ssid_length, entry->ssid,
				entry->rssi, entry->security);
	}
}

const struct wifi_scan_result *WiFiClass::getScanResults() const {
	return wifiState.scanResults.data();
}

int WiFiClass::status() {
	struct wifi_iface_status if_status;

	if (netif == nullptr) {
		wifiState.sta_iface = net_if_get_wifi_sta();
		netif = wifiState.sta_iface;
	}

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, netif, &if_status,
				 sizeof(struct wifi_iface_status))) {
		return WL_NO_SHIELD;
	}

	/* WIFI_STATE_UNKNOWN sorts *above* WIFI_STATE_COMPLETED in the enum, so it
	   must be rejected before the ordered comparison below. The esp_at driver
	   leaves the state at UNKNOWN whenever "AT+CWJAP?" reports no AP, i.e. any
	   time the module is not associated. */
	if (if_status.state == WIFI_STATE_UNKNOWN) {
		return WL_DISCONNECTED;
	}

	if (if_status.state == WIFI_STATE_INTERFACE_DISABLED) {
		return WL_DISCONNECTED;
	}

	if (if_status.iface_mode != WIFI_MODE_AP) {
		memcpy(&wifiState.sta_state, &if_status, sizeof(wifiState.sta_state));
	}

	if (if_status.state >= WIFI_STATE_ASSOCIATED) {
		return WL_CONNECTED;
	}

	return WL_DISCONNECTED;
}

int8_t WiFiClass::scanNetworks() {
	wifiState.sta_iface = net_if_get_wifi_sta();
	netif = wifiState.sta_iface;

	if (!net_if_is_up(wifiState.sta_iface)) {
		net_if_up(wifiState.sta_iface);
	}

	wifiState.flags = 0u;
	wifiState.scanResults.clear();
	wifiState.scanResults.reserve(MAX_SCAN_RESULTS);
	wifiState.resultCount = 0u;

	struct wifi_scan_params params = {
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
	};

	registerScanEventCallback();
	int ret = 0;
	constexpr int max_scan_start_retries = 3;
	for (int attempt = 1; attempt <= max_scan_start_retries; ++attempt) {
		ret = net_mgmt(NET_REQUEST_WIFI_SCAN, wifiState.sta_iface, &params, sizeof(params));
		if (ret >= 0) {
			break;
		}

		/* if the driver is busy, retry */
		if ((ret == -EAGAIN || ret == -EBUSY) && attempt < max_scan_start_retries) {
			LOG_WRN("WiFi scan start retry %d/%d (err=%d)", attempt, max_scan_start_retries - 1,
					ret);
			k_sleep(K_MSEC(100));
			continue;
		}
		break;
	}

	if (ret < 0) {
		LOG_WRN("WiFi scan request failed (%d)", ret);
		return ret;
	}

	if ((wifiState.flags & WIFI_FLAG_SCAN_SEQUENCE_FINISHED) != 0u) {
		return wifiState.resultCount;
	}

	ret = net_mgmt_event_wait_on_iface(wifiState.sta_iface, NET_EVENT_WIFI_SCAN_DONE, nullptr,
									   nullptr, nullptr,
									   timeout_ms == 0 ? K_FOREVER : K_MSEC(timeout_ms));
	if (ret == -ETIMEDOUT) {
		LOG_INF("WiFi scan timed out ");
	} else if (ret < 0) {
		LOG_WRN("WiFi scan wait failed (%d)", ret);
	}

	return wifiState.resultCount;
}

bool WiFiClass::disconnect() {
	int ret;
	if (status() == WL_CONNECTED) {
		ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, netif, NULL, 0);

		if (ret != 0) {
			LOG_ERR("Error on Wifi Disconnect %d", ret);
		}
	}

	ret = NetworkInterface::disconnect();

	return ret;
}

char *WiFiClass::SSID(std::optional<uint8_t> networkItem) {
	if (networkItem.has_value()) {
		if (*networkItem >= wifiState.resultCount) {
			return nullptr;
		}

		return (char *)wifiState.scanResults[*networkItem].ssid;
	}

	if (status() == WL_CONNECTED) {
		return (char *)wifiState.sta_state.ssid;
	}

	return nullptr;
}

int32_t WiFiClass::RSSI(std::optional<uint8_t> networkItem) {
	if (networkItem.has_value()) {
		if (*networkItem >= wifiState.resultCount) {
			return 0;
		}

		return wifiState.scanResults[*networkItem].rssi;
	}

	if (status() == WL_CONNECTED) {
		return wifiState.sta_state.rssi;
	}

	return 0;
}

int8_t WiFiClass::channel(std::optional<uint8_t> networkItem) {
	if (networkItem.has_value()) {
		if (*networkItem >= wifiState.resultCount) {
			return -1;
		}

		return wifiState.scanResults[*networkItem].channel;
	}

	if (status() == WL_CONNECTED) {
		return wifiState.sta_state.channel;
	}

	return -1;
}

uint8_t *WiFiClass::BSSID(uint8_t *bssid) {
	return BSSID(std::nullopt, bssid);
}

uint8_t *WiFiClass::BSSID(std::optional<uint8_t> networkItem, uint8_t *bssid) {
	if (networkItem.has_value()) {
		if (*networkItem >= wifiState.resultCount) {
			return nullptr;
		}

		if (bssid != nullptr) {
			memcpy(bssid, wifiState.scanResults[*networkItem].mac, WIFI_MAC_ADDR_LEN);
			return bssid;
		}
	}

	if (status() == WL_CONNECTED) {
		if (bssid != nullptr) {
			memcpy(bssid, wifiState.sta_state.bssid, WIFI_MAC_ADDR_LEN);
			return bssid;
		}
	}

	return nullptr;
}

uint8_t WiFiClass::encryptionType(std::optional<uint8_t> networkItem) {
	if (networkItem.has_value()) {
		if (*networkItem >= wifiState.resultCount) {
			return WIFI_SECURITY_TYPE_UNKNOWN;
		}

		return secTypeToEncryptionType(wifiState.scanResults[*networkItem].security);
	}

	return secTypeToEncryptionType(wifiState.sta_state.security);
}
