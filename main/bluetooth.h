#pragma once
#include "ancs_protocol.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#define SD_NOTIFICATION_MAX 5

typedef struct {
    sd_ancs_attributes text;
    uint8_t category, flags;
    int64_t received_at;
} sd_ble_notification;
typedef struct {
    bool connected, encrypted, bonded, ready, details, pairing;
    uint32_t passkey, session;
    char action_status[24];
    int action_error;
    char status[32];
    int error;
    unsigned dropped, source_events, data_bytes, decoded;
    int last_error;
    int count;
    int64_t overlay_until;
    sd_ble_notification items[SD_NOTIFICATION_MAX];
} sd_ble_state;
esp_err_t sd_ble_start(void);
void sd_ble_snapshot(sd_ble_state *out);
/* Queues an action on the NimBLE host; no Bluetooth calls from HTTP tasks. */
esp_err_t sd_ble_action(uint32_t uid, uint8_t action, uint32_t session);
esp_err_t sd_ble_forget_bonds(void);
