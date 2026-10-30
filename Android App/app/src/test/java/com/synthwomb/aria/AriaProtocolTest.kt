package com.synthwomb.aria

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class AriaProtocolTest {
    private val up = Sticks(1f, 0f, 0f, 0f)
    private val none = Sticks(null, 0f, 0f, 0f)

    private fun fly(f: Flight, s: Sticks, seconds: Float): Command {
        var c = Command(0f, 0f, 0f, 0)
        repeat((seconds / 0.033f).toInt()) { c = f.tick(0.033f, s) }
        return c
    }

    @Test fun packetIsCrtpSetpointWithChecksum() {
        val p = AriaProtocol.setpoint(1.5f, 2f, -3f, 40000)
        assertEquals(16, p.size)
        assertEquals(0x30, p[0].toInt())
        val b = ByteBuffer.wrap(p).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(1.5f, b.getFloat(1), 0f)
        assertEquals(-2f, b.getFloat(5), 0f)     // forward is negative on the wire
        assertEquals(-3f, b.getFloat(9), 0f)
        assertEquals(40000, b.getShort(13).toInt() and 0xFFFF)
        var sum = 0
        for (i in 0 until 15) sum += p[i].toInt() and 0xFF
        assertEquals((sum and 0xFF).toByte(), p[15])
    }

    @Test fun thrustIsClamped() {
        val p = AriaProtocol.setpoint(0f, 0f, 0f, 99999)
        assertEquals(60000, ByteBuffer.wrap(p).order(ByteOrder.LITTLE_ENDIAN).getShort(13).toInt() and 0xFFFF)
    }

    @Test fun disarmedSendsNothingButZeros() {
        assertEquals(Command(0f, 0f, 0f, 0), Flight().tick(0.033f, Sticks(1f, 1f, 1f, 1f)))
    }

    @Test fun onlyArmsWithTheThrottleDown() {
        val f = Flight()
        assertFalse(f.arm(0.5f))
        assertFalse(f.armed)
        assertTrue(f.arm(0f))
        assertTrue(Flight().arm(null))
    }

    @Test fun thrustClimbsAndStopsAtTheLimit() {
        val f = Flight(0.5f)
        f.arm(0f)
        val first = f.tick(0.033f, up).thrust
        assertTrue("no jump: $first", first < 3000)
        assertEquals(30000f, fly(f, up, 2f).thrust.toFloat(), 1f)
    }

    @Test fun liftingTheThumbPowersDownInHalfASecond() {
        val f = Flight(0.8f)
        f.arm(0f)
        fly(f, up, 2f)
        assertEquals(0, fly(f, none, 0.6f).thrust)
    }

    @Test fun stopCutsThrustAtOnceAndStaysOff() {
        val f = Flight()
        f.arm(0f)
        fly(f, up, 2f)
        f.stop()
        assertEquals(0, f.tick(0.033f, up).thrust)
        assertFalse(f.armed)
    }

    @Test fun tiltAndYawAreCapped() {
        val f = Flight()
        f.arm(0f)
        val c = f.tick(0.033f, Sticks(0f, 5f, 5f, -5f))
        assertEquals(Flight.MAX_TILT_DEG, c.roll, 0f)
        assertEquals(-Flight.MAX_TILT_DEG, c.pitch, 0f)
        assertEquals(Flight.MAX_YAW_DEG_S, c.yawRate, 0f)
    }

    @Test fun limitStaysInItsRange() {
        val f = Flight()
        f.thrustLimit = 5f
        assertEquals(Flight.MAX_LIMIT, f.thrustLimit, 0f)
    }
}
