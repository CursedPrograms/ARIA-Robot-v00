package com.synthwomb.aria

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/**
 * Her Bluetooth link (aria_ble.c, ESP32-S3): scan for her service, connect, and Android pairs on the
 * first write, because her flight characteristic needs an encrypted link. New phones can only pair in
 * her first two minutes after power-up. A paired phone is remembered, so it reconnects straight away.
 */
@SuppressLint("MissingPermission")     // MainActivity asks for them before PAIR does anything
class AriaBluetooth(private val context: Context, private val onState: (String) -> Unit) {
    private val adapter: BluetoothAdapter? = context.getSystemService(BluetoothManager::class.java)?.adapter
    private var gatt: BluetoothGatt? = null
    private var fly: BluetoothGattCharacteristic? = null
    private var state: BluetoothGattCharacteristic? = null
    private var scanning = false
    @Volatile var connected = false
        private set

    private val scan = object : ScanCallback() {
        override fun onScanResult(type: Int, result: ScanResult) {
            stopScan()
            connect(result.device)
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                g.discoverServices()
            } else {
                connected = false
                fly = null
                onState("Bluetooth: lost her")
                g.close()
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(SERVICE) ?: return onState("Bluetooth: that isn't her")
            fly = service.getCharacteristic(FLY)?.apply { writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE }
            state = service.getCharacteristic(STATE)
            connected = fly != null
            onState(if (connected) "Bluetooth: paired" else "Bluetooth: no flight link")
            state?.let { g.readCharacteristic(it) }    // needs encryption: this is what asks Android to pair
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) onState(stateText(c.value))
        }
    }

    private fun stateText(v: ByteArray?): String {
        if (v == null || v.size < 5) return "Bluetooth: paired"
        val volts = ByteBuffer.wrap(v).order(ByteOrder.LITTLE_ENDIAN).float
        return String.format(java.util.Locale.ROOT, "Bluetooth: %s   %.2f V", if (v[4].toInt() != 0) "flying" else "ground", volts)
    }

    fun pair() {
        val a = adapter ?: return onState("This phone has no Bluetooth")
        if (!a.isEnabled) return onState("Turn Bluetooth on")
        val known = a.bondedDevices.firstOrNull { it.name == "ARIA" }
        if (known != null) return connect(known)
        val scanner = a.bluetoothLeScanner ?: return onState("Turn Bluetooth on")
        onState("Looking for her: power her on, you have 2 minutes")
        scanning = true
        scanner.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()),
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scan)
    }

    private fun stopScan() {
        if (scanning) adapter?.bluetoothLeScanner?.stopScan(scan)
        scanning = false
    }

    private fun connect(device: BluetoothDevice) {
        gatt?.close()
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    /** One flight frame, no reply: a dropped one is replaced in 33 ms. */
    @Suppress("DEPRECATION")
    fun send(frame: ByteArray) {
        val g = gatt ?: return
        val c = fly ?: return
        c.value = frame
        g.writeCharacteristic(c)
    }

    @Suppress("DEPRECATION")
    fun readState() {
        val g = gatt ?: return
        val c = state ?: return
        if (connected) g.readCharacteristic(c)
    }

    fun close() {
        stopScan()
        connected = false
        fly = null
        gatt?.close()
        gatt = null
    }

    companion object {
        val SERVICE: UUID = UUID.fromString("a21a0001-5b1d-4c3e-9f2a-0d7e1c5a7f10")
        val FLY: UUID = UUID.fromString("a21a0002-5b1d-4c3e-9f2a-0d7e1c5a7f10")
        val STATE: UUID = UUID.fromString("a21a0003-5b1d-4c3e-9f2a-0d7e1c5a7f10")
    }
}
