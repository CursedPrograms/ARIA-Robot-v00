package com.synthwomb.aria

import java.nio.ByteBuffer
import java.nio.ByteOrder

// How she takes flight commands (ESP-Drone): UDP to her own WiFi address, one CRTP packet with a
// byte-sum checksum on the end. Thrust stays locked in her firmware until she's seen a thrust of 0.
object AriaProtocol {
    const val HOST = "192.168.43.42"
    const val UDP_PORT = 2390
    const val PAGE_PORT = 5014
    const val FULL_THRUST = 60000            // her firmware's MAX_THRUST
    private const val CRTP_SETPOINT = 0x30   // CRTP port 3, channel 0

    /** roll + = right, pitch + = forward, yawRate + = clockwise (deg, deg, deg/s); thrust 0..60000. */
    fun setpoint(roll: Float, pitch: Float, yawRate: Float, thrust: Int): ByteArray {
        val b = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)
        b.put(CRTP_SETPOINT.toByte())
        b.putFloat(roll)
        b.putFloat(-pitch)                   // on the wire, forward is negative pitch
        b.putFloat(yawRate)
        b.putShort(thrust.coerceIn(0, FULL_THRUST).toShort())
        var sum = 0
        for (i in 0 until 15) sum += b.get(i).toInt() and 0xFF
        b.put(sum.toByte())
        return b.array()
    }

    /** The browser's and Bluetooth's frame: no CRTP header or checksum, her firmware adds those. */
    fun frame(roll: Float, pitch: Float, yawRate: Float, thrust: Int): ByteArray {
        val b = ByteBuffer.allocate(14).order(ByteOrder.LITTLE_ENDIAN)
        b.putFloat(roll)
        b.putFloat(-pitch)
        b.putFloat(yawRate)
        b.putShort(thrust.coerceIn(0, FULL_THRUST).toShort())
        return b.array()
    }
}

/** The sticks right now. throttle is null while no finger is on it. Everything else -1..1. */
data class Sticks(val throttle: Float?, val yaw: Float, val roll: Float, val pitch: Float)

data class Command(val roll: Float, val pitch: Float, val yawRate: Float, val thrust: Int)

/**
 * What the sticks may ask of her. It's all here, away from the screen and the network, so it can be
 * tested with no drone:
 *  - she only arms with the throttle down;
 *  - disarmed, she's sent a thrust of 0, which is also what unlocks her firmware;
 *  - lift your thumb off the throttle and she powers down in about half a second: a drop from low
 *    down beats a drone with nobody holding it;
 *  - thrust can't jump: it climbs over about a second;
 *  - tilt and yaw are capped, and so is thrust (thrustLimit, 0.2-0.8).
 */
class Flight(thrustLimit: Float = 0.5f) {
    var armed = false
        private set
    var thrustLimit = thrustLimit.coerceIn(MIN_LIMIT, MAX_LIMIT)
        set(v) { field = v.coerceIn(MIN_LIMIT, MAX_LIMIT) }
    private var thrust = 0f                  // 0..1 of full thrust, where it's got to

    fun arm(throttle: Float?): Boolean {
        if ((throttle ?: 0f) > ARM_THROTTLE) return false
        armed = true
        thrust = 0f
        return true
    }

    fun stop() {
        armed = false
        thrust = 0f
    }

    fun tick(seconds: Float, s: Sticks): Command {
        if (!armed) return Command(0f, 0f, 0f, 0)
        val want = (s.throttle ?: 0f).coerceIn(0f, 1f) * thrustLimit
        val step = if (want > thrust) seconds / CLIMB_S else seconds / DROP_S
        thrust = if (want > thrust) minOf(want, thrust + step) else maxOf(want, thrust - step)
        return Command(
            s.roll.coerceIn(-1f, 1f) * MAX_TILT_DEG,
            s.pitch.coerceIn(-1f, 1f) * MAX_TILT_DEG,
            s.yaw.coerceIn(-1f, 1f) * MAX_YAW_DEG_S,
            (thrust * AriaProtocol.FULL_THRUST).toInt(),
        )
    }

    companion object {
        const val MIN_LIMIT = 0.2f
        const val MAX_LIMIT = 0.8f
        const val ARM_THROTTLE = 0.05f
        const val MAX_TILT_DEG = 15f
        const val MAX_YAW_DEG_S = 120f
        const val CLIMB_S = 1.0f
        const val DROP_S = 0.5f
    }
}
