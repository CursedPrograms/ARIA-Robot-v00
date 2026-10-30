package com.synthwomb.aria

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.URL
import kotlin.concurrent.thread

/**
 * The link to her: flight commands 30 times a second, and her /status once a second. Her own WiFi has
 * no internet, so Android would rather use mobile data: the app ties itself to WiFi first.
 * Stop sending and her firmware levels off at 0.5 s and cuts the motors at 2 s.
 */
class AriaLink(private val context: Context, private val bluetooth: AriaBluetooth, private val onStatus: (JSONObject?, JSONObject?) -> Unit) {
    @Volatile var useBluetooth = false
    @Volatile private var running = false
    @Volatile var command = Command(0f, 0f, 0f, 0)
    @Volatile var tick: (Float) -> Command = { command }

    private var wifi: ConnectivityManager.NetworkCallback? = null

    fun start() {
        running = true
        val cm = context.getSystemService(ConnectivityManager::class.java)
        val cb = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                cm.bindProcessToNetwork(network)
            }
        }
        wifi = cb
        cm.requestNetwork(NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(), cb)

        thread(name = "flight", isDaemon = true) { flightLoop() }
        thread(name = "status", isDaemon = true) { statusLoop() }
    }

    fun stop() {
        running = false
        val cm = context.getSystemService(ConnectivityManager::class.java)
        wifi?.let { cm.unregisterNetworkCallback(it) }
        wifi = null
        cm.bindProcessToNetwork(null)
    }

    private fun flightLoop() {
        val socket = DatagramSocket()
        var last = System.nanoTime()
        try {
            while (running) {
                val now = System.nanoTime()
                val c = tick((now - last) / 1e9f)
                last = now
                command = c
                val bytes = AriaProtocol.setpoint(c.roll, c.pitch, c.yawRate, c.thrust)
                if (useBluetooth) {
                    bluetooth.send(AriaProtocol.frame(c.roll, c.pitch, c.yawRate, c.thrust))
                    Thread.sleep(33)
                    continue
                }
                try {
                    socket.send(DatagramPacket(bytes, bytes.size, InetAddress.getByName(AriaProtocol.HOST), AriaProtocol.UDP_PORT))
                } catch (_: java.io.IOException) {
                    // no WiFi this moment: her own watchdog handles a silence
                }
                Thread.sleep(33)
            }
        } finally {
            socket.close()
        }
    }

    private fun statusLoop() {
        while (running) {
            if (useBluetooth) bluetooth.readState() else onStatus(fetchStatus("/status"), fetchStatus("/near"))
            Thread.sleep(1000)
        }
    }

    private fun fetchStatus(path: String): JSONObject? = try {
        val http = URL("http://${AriaProtocol.HOST}:${AriaProtocol.PAGE_PORT}$path").openConnection() as HttpURLConnection
        http.connectTimeout = 700
        http.readTimeout = 700
        try {
            JSONObject(http.inputStream.bufferedReader().readText())
        } finally {
            http.disconnect()
        }
    } catch (_: Exception) {
        null
    }
}
