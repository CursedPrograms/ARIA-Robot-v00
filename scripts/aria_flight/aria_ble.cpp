/*
 * aria_ble.cpp - flying ARIA over Bluetooth LE, from her app, paired like a remote control.
 *
 * Frames are the browser's (roll, pitch, yaw, thrust) and go through the same link rules.
 *   - writing a frame needs an encrypted link, so only a paired phone can fly her;
 *   - new phones can only pair in the first PAIR_WINDOW_MS after power-up; after that only ones she
 *     already knows get in, so a stranger can't pair with her in the air;
 *   - the link goes quiet (out of range, app closed): she levels at 0.5 s and descends from 1 s.
 *
 * The UUIDs are the three in AriaBluetooth.kt. Arduino-ESP32 uses Bluedroid on a plain ESP32 and NimBLE
 * on an S3, hence the two halves of the pairing check.
 */
#include <Arduino.h>

#include "aria_fleet.h"

#if ARIA_BLE

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <string.h>

#include "aria_link_logic.h"

#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#endif

#define PAIR_WINDOW_MS 120000
#define SERVICE_UUID "a21a0001-5b1d-4c3e-9f2a-0d7e1c5a7f10"
#define FLY_UUID "a21a0002-5b1d-4c3e-9f2a-0d7e1c5a7f10"
#define STATE_UUID "a21a0003-5b1d-4c3e-9f2a-0d7e1c5a7f10"

extern bool armed;
extern float batteryVoltage;

static int knownBonds = 0;

static int bondCount()
{
#if defined(CONFIG_NIMBLE_ENABLED)
    int count = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC, &count);
    return count;
#else
    return esp_ble_get_bond_device_num();
#endif
}

// True if this was a phone she mustn't take: pairing's closed and the bond list just grew
static bool strangerPaired()
{
    const int bonds = bondCount();
    if (millis() < PAIR_WINDOW_MS) {
        knownBonds = bonds;
        return false;
    }
    return bonds > knownBonds;
}

class Security : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t) override {}
    bool onSecurityRequest() override { return true; }
    bool onConfirmPIN(uint32_t) override { return true; }
#if defined(CONFIG_NIMBLE_ENABLED)
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override
    {
        if (strangerPaired()) {
            ble_store_util_delete_peer(&desc->peer_id_addr);
            ble_gap_terminate(desc->conn_handle, BLE_ERR_AUTH_FAIL);
        }
    }
#else
    void onAuthenticationComplete(esp_ble_auth_cmpl_t desc) override
    {
        if (desc.success && strangerPaired()) {
            esp_ble_remove_bond_device(desc.bd_addr);
            esp_ble_gap_disconnect(desc.bd_addr);
        }
    }
#endif
};

class Connection : public BLEServerCallbacks {
    void onDisconnect(BLEServer *server) override { BLEDevice::startAdvertising(); }
};

class Fly : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override
    {
        if (c->getLength() == ARIA_FRAME_BYTES) {
            ariaLinkFrame(c->getData());
        }
    }
};

// Battery volts (float), then 1 if she's armed
class State : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *c) override
    {
        uint8_t out[5] = {0};
        const float volts = batteryVoltage;
        memcpy(out, &volts, sizeof(volts));
        out[4] = armed ? 1 : 0;
        c->setValue(out, sizeof(out));
    }
};

void ariaBleStart()
{
    BLEDevice::init("ARIA");
    BLESecurity security;
    security.setCapability(ESP_IO_CAP_NONE);      // she has no screen: pair like a gamepad
    security.setAuthenticationMode(true, false, true);
    security.setInitEncryptionKey();
    security.setRespEncryptionKey();
    BLEDevice::setSecurityCallbacks(new Security());

    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new Connection());
    BLEService *service = server->createService(SERVICE_UUID);
    BLECharacteristic *fly = service->createCharacteristic(
        FLY_UUID, BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE_ENC);
    BLECharacteristic *state = service->createCharacteristic(
        STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_READ_ENC);
#if defined(CONFIG_BLUEDROID_ENABLED)
    fly->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    state->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
#endif
    fly->setCallbacks(new Fly());
    state->setCallbacks(new State());
    service->start();

    knownBonds = bondCount();
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

#endif  // ARIA_BLE
