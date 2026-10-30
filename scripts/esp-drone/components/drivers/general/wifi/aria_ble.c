/*
 * aria_ble.c - flying ARIA over Bluetooth LE, from her app, paired like a remote control.
 *
 * ESP32-S3 only: the S2 has no Bluetooth. Frames are the browser's (roll, pitch, yaw, thrust), capped by
 * the same ariaFlyPacket(), into the same queue as the app's UDP, so her thrust lock and watchdog hold.
 *   - writing a frame needs an encrypted link, so only a paired phone can fly her;
 *   - new phones can only pair in the first ARIA_PAIR_WINDOW_MS after power-up; after that only ones
 *     she already knows get in, so a stranger can't pair with her in the air;
 *   - the link goes quiet (out of range, app closed): her watchdog levels her at 0.5 s, cuts at 2 s.
 */
#include "sdkconfig.h"

#ifdef CONFIG_ARIA_BLE

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "aria_fleet.h"
#include "aria_fleet_logic.h"
#include "motors.h"
#include "pm_esplane.h"

void ble_store_config_init(void);

#define ARIA_PAIR_WINDOW_MS 120000

// a21a0001-5b1d-4c3e-9f2a-0d7e1c5a7f10 and its two characteristics (...0002 fly, ...0003 state),
// least significant byte first: the same three as AriaBluetooth.kt
#define ARIA_UUID(n) BLE_UUID128_INIT(0x10, 0x7f, 0x5a, 0x1c, 0x7e, 0x0d, 0x2a, 0x9f, 0x3e, 0x4c, 0x1d, 0x5b, n, 0x00, 0x1a, 0xa2)
static const ble_uuid128_t SERVICE = ARIA_UUID(0x01);
static const ble_uuid128_t FLY = ARIA_UUID(0x02);
static const ble_uuid128_t STATE = ARIA_UUID(0x03);

static uint8_t ownAddrType;
static void advertise(void);

static int flyAccess(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t in[ARIA_FLY_BYTES], packet[ARIA_FLY_PACKET];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, in, sizeof(in), &len) == 0 && len == sizeof(in)) {
        ariaFlyPacket(in, packet);
        wifiInjectPacket(packet, sizeof(packet));
    }
    return 0;
}

// Battery volts (float), then 1 if her motors are spinning
static int stateAccess(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t out[5] = {0};
    const float volts = pmGetBatteryVoltage();
    memcpy(out, &volts, sizeof(volts));
    for (uint32_t i = 0; i < NBR_OF_MOTORS; i++) {
        if (motorsGetRatio(i) > 0) {
            out[4] = 1;
        }
    }
    return os_mbuf_append(ctxt->om, out, sizeof(out)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {.uuid = &FLY.u, .access_cb = flyAccess,
             .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC},
            {.uuid = &STATE.u, .access_cb = stateAccess,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC},
            {0},
        },
    },
    {0},
};

static int gapEvent(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            advertise();
        } else {
            const struct ble_gap_upd_params fast = {.itvl_min = 12, .itvl_max = 24, .latency = 0,
                                                    .supervision_timeout = 200};   // 15-30 ms, gone after 2 s
            ble_gap_update_params(event->connect.conn_handle, &fast);
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        // Encrypted: a phone she already knows, or a new one while pairing's open. Otherwise, out.
        if (event->enc_change.status == 0 && ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0 &&
            !desc.sec_state.bonded && xTaskGetTickCount() > pdMS_TO_TICKS(ARIA_PAIR_WINDOW_MS)) {
            ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_AUTH_FAIL);
        }
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        if (xTaskGetTickCount() > pdMS_TO_TICKS(ARIA_PAIR_WINDOW_MS)) {
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        return 0;
    }
}

static void advertise(void)
{
    struct ble_hs_adv_fields adv = {0}, scan = {0};
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.uuids128 = (ble_uuid128_t *)&SERVICE;
    adv.num_uuids128 = 1;
    adv.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&adv);

    const char *name = ble_svc_gap_device_name();
    scan.name = (const uint8_t *)name;
    scan.name_len = strlen(name);
    scan.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&scan);

    struct ble_gap_adv_params params = {.conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN};
    ble_gap_adv_start(ownAddrType, NULL, BLE_HS_FOREVER, &params, gapEvent, NULL);
}

static void onSync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &ownAddrType);
    advertise();
}

static void hostTask(void *arg)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ariaBleStart(void)
{
    if (nimble_port_init() != ESP_OK) {
        return;
    }
    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;     // she has no screen: pair like a gamepad
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(SERVICES);
    ble_gatts_add_svcs(SERVICES);
    ble_svc_gap_device_name_set("ARIA");
    ble_store_config_init();
    nimble_port_freertos_init(hostTask);
}

#endif  // CONFIG_ARIA_BLE
