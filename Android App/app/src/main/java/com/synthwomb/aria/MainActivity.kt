package com.synthwomb.aria

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.hardware.input.InputManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import org.json.JSONObject
import java.util.Locale

private val SKY = Color.parseColor("#E6FAFD")
private val CLOUD = Color.WHITE
private val CYAN = Color.parseColor("#0891B2")
private val BRIGHT = Color.parseColor("#22D3EE")
private val EDGE = Color.parseColor("#B5EAF2")
private val INK = Color.parseColor("#0B3A44")
private val STOP = Color.parseColor("#B91C1C")

/** Her controller: left stick throttle and yaw, right stick roll and pitch. */
class MainActivity : Activity(), InputManager.InputDeviceListener {
    private val flight = Flight()
    private lateinit var link: AriaLink
    private lateinit var sticks: SticksView
    private lateinit var state: TextView
    private lateinit var armBtn: TextView
    private lateinit var bluetooth: AriaBluetooth
    private lateinit var linkBtn: TextView
    private lateinit var tiles: LinearLayout
    private lateinit var pairBtn: TextView
    @Volatile private var pad: Sticks? = null    // a gamepad's sticks, while it's in use
    private val ui = Handler(Looper.getMainLooper())

    private fun label(text: String, size: Float, color: Int) = TextView(this).apply {
        this.text = text
        textSize = size
        setTextColor(color)
        setPadding(24, 8, 24, 8)
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(saved: Bundle?) {
        super.onCreate(saved)
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        sticks = SticksView(this)
        tiles = LinearLayout(this)

        state = label("looking for her...", 14f, INK)
        armBtn = label("HOLD TO ARM", 16f, CLOUD).apply { setBackgroundColor(CYAN) }
        val stopBtn = label("STOP", 18f, CLOUD).apply { setBackgroundColor(STOP) }
        linkBtn = label("WIFI", 14f, CYAN).apply {
            setOnClickListener {                  // never changes link while she's armed
                if (flight.armed) {
                    state.text = "Disarm first"
                } else {
                    link.useBluetooth = !link.useBluetooth
                    text = if (link.useBluetooth) "BLUETOOTH" else "WIFI"
                    pairBtn.visibility = if (link.useBluetooth) View.VISIBLE else View.GONE
                }
            }
        }
        pairBtn = label("PAIR", 14f, CYAN).apply {
            visibility = View.GONE
            setOnClickListener { pair() }
        }
        val limit = label("Limit 50%", 13f, INK)
        val bar = SeekBar(this).apply {
            max = 60
            progress = 30
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, value: Int, fromUser: Boolean) {
                    flight.thrustLimit = (value + 20) / 100f
                    limit.text = String.format(Locale.ROOT, "Limit %d%%", value + 20)
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        }
        stopBtn.setOnTouchListener { _, e ->      // on the press, not the release
            if (e.action == MotionEvent.ACTION_DOWN) disarm()
            true
        }
        armBtn.setOnTouchListener { _, e ->       // a second's hold, so a stray thumb can't do it
            when (e.action) {
                MotionEvent.ACTION_DOWN -> ui.postDelayed(armNow, 1000)
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> ui.removeCallbacks(armNow)
            }
            true
        }

        val top = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(CLOUD)
            addView(label("ARIA", 20f, CYAN))
            addView(state, LinearLayout.LayoutParams(0, -2, 1f))
            addView(linkBtn)
            addView(pairBtn)
            addView(limit)
            addView(bar, LinearLayout.LayoutParams(260, -2))
            addView(armBtn)
            addView(stopBtn)
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(SKY)
            addView(top)
            addView(android.widget.HorizontalScrollView(context).apply { addView(tiles) })
            addView(sticks, LinearLayout.LayoutParams(-1, 0, 1f))
        })

        bluetooth = AriaBluetooth(this) { text -> runOnUiThread { state.text = text } }
        link = AriaLink(this, bluetooth) { s, near -> runOnUiThread { showStatus(s, near) } }
        link.tick = { dt -> flight.tick(dt, pad ?: sticks.sticks()) }
        getSystemService(InputManager::class.java).registerInputDeviceListener(this, null)
    }

    private val armNow = Runnable {
        if (flight.arm((pad ?: sticks.sticks()).throttle)) {
            armBtn.text = "ARMED"
            armBtn.setBackgroundColor(BRIGHT)
        } else {
            state.text = "Throttle down first"
        }
    }

    private fun pair() {
        val need = if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        if (need.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) {
            requestPermissions(need, 1)
        } else {
            bluetooth.pair()
        }
    }

    override fun onRequestPermissionsResult(code: Int, names: Array<out String>, results: IntArray) {
        if (results.isNotEmpty() && results.all { it == PackageManager.PERMISSION_GRANTED }) bluetooth.pair()
    }

    // ---- a gamepad or flight stick, over Bluetooth or a USB-C adapter: Android treats them alike.
    // Gamepad: the right trigger is the throttle, so letting go powers her down like lifting a thumb.
    // Left stick turns, right stick tilts; hold A to arm, B stops. Flight stick (it has a throttle
    // lever): the stick tilts, its twist turns; hold the trigger to arm, button 2 stops. A lever
    // stays where it's left, so take it down or press stop before you let go ----
    private fun isPad(e: android.view.InputEvent) =
        e.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            e.source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD

    override fun onGenericMotionEvent(e: MotionEvent): Boolean {
        if (!isPad(e) || e.action != MotionEvent.ACTION_MOVE) return super.onGenericMotionEvent(e)
        fun dead(v: Float) = if (kotlin.math.abs(v) < 0.1f) 0f else v
        if (e.device?.getMotionRange(MotionEvent.AXIS_THROTTLE) != null) {
            val lever = (1 - e.getAxisValue(MotionEvent.AXIS_THROTTLE)) / 2     // full forward reads -1
            pad = Sticks(if (lever > 0.02f) lever else null, dead(e.getAxisValue(MotionEvent.AXIS_RZ)),
                         dead(e.getAxisValue(MotionEvent.AXIS_X)), -dead(e.getAxisValue(MotionEvent.AXIS_Y)))
            return true
        }
        val trigger = maxOf(e.getAxisValue(MotionEvent.AXIS_RTRIGGER), e.getAxisValue(MotionEvent.AXIS_GAS))
        pad = Sticks(if (trigger > 0.02f) trigger else null, dead(e.getAxisValue(MotionEvent.AXIS_X)),
                     dead(e.getAxisValue(MotionEvent.AXIS_Z)), -dead(e.getAxisValue(MotionEvent.AXIS_RZ)))
        return true
    }

    override fun dispatchKeyEvent(e: KeyEvent): Boolean {
        if (isPad(e)) {
            when {
                (e.keyCode == KeyEvent.KEYCODE_BUTTON_B || e.keyCode == KeyEvent.KEYCODE_BUTTON_2) && e.action == KeyEvent.ACTION_DOWN -> { disarm(); return true }
                (e.keyCode == KeyEvent.KEYCODE_BUTTON_A || e.keyCode == KeyEvent.KEYCODE_BUTTON_1) && e.action == KeyEvent.ACTION_DOWN && e.repeatCount == 0 -> { ui.postDelayed(armNow, 1000); return true }
                (e.keyCode == KeyEvent.KEYCODE_BUTTON_A || e.keyCode == KeyEvent.KEYCODE_BUTTON_1) && e.action == KeyEvent.ACTION_UP -> { ui.removeCallbacks(armNow); return true }
            }
        }
        return super.dispatchKeyEvent(e)
    }

    override fun onInputDeviceAdded(id: Int) {}
    override fun onInputDeviceChanged(id: Int) {}
    override fun onInputDeviceRemoved(id: Int) {    // the pad's battery died: nobody's holding her
        if (pad != null) {
            pad = null
            disarm()
            state.text = "Gamepad gone"
        }
    }

    private fun disarm() {
        flight.stop()
        armBtn.text = "HOLD TO ARM"
        armBtn.setBackgroundColor(CYAN)
    }

    private fun showStatus(s: JSONObject?, near: JSONObject?) {
        tiles.removeAllViews()
        if (s != null) {
            val list = listOf(
                "Battery" to String.format(Locale.ROOT, "%.2f V", s.optDouble("battery_v")),
                "Pitch" to String.format(Locale.ROOT, "%.1f\u00b0", s.optDouble("pitch")),
                "Roll" to String.format(Locale.ROOT, "%.1f\u00b0", s.optDouble("roll")),
                "Signal" to if (s.optInt("rssi") != 0) "${s.optInt("rssi")} dBm" else "--",
                "Network" to s.optString("network").ifEmpty { "her WiFi" },
            ) + Tiles.near(near?.toString())
            for ((title, value) in list) {
                tiles.addView(label("$title\n$value", 12f, INK).apply {
                    gravity = android.view.Gravity.CENTER
                    setBackgroundColor(CLOUD)
                }, LinearLayout.LayoutParams(-2, -2).apply { setMargins(4, 4, 4, 4) })
            }
        }
        state.text = if (s == null) "No answer: is this phone on her WiFi?" else String.format(
            Locale.ROOT, "%s   %.2f V", s.optString("mode"), s.optDouble("battery_v"))
    }

    override fun onResume() {
        super.onResume()
        link.start()
    }

    override fun onPause() {                  // a call, the home button, the screen going off: disarmed
        disarm()
        link.stop()
        bluetooth.close()
        super.onPause()
    }
}

/** Two sticks. Yaw, roll and pitch spring back to the middle; the throttle stays where it's left while it's held. */
@SuppressLint("ViewConstructor", "ClickableViewAccessibility")
class SticksView(context: Context) : View(context) {
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 4f }
    private val fingers = HashMap<Int, Int>()   // pointer id -> 0 left stick, 1 right stick
    @Volatile private var left: Pair<Float, Float>? = null
    @Volatile private var right: Pair<Float, Float>? = null

    fun sticks(): Sticks {
        val l = left
        val r = right
        return Sticks(l?.let { (1 - it.second) / 2 }, l?.first ?: 0f, r?.first ?: 0f, r?.second?.let { -it } ?: 0f)
    }

    private fun radius() = minOf(width / 2f, height.toFloat()) * 0.42f

    private fun centreX(stick: Int) = if (stick == 0) width * 0.22f else width * 0.78f

    private fun place(stick: Int, x: Float, y: Float) {
        val r = radius()
        val at = ((x - centreX(stick)) / r).coerceIn(-1f, 1f) to ((y - height / 2f) / r).coerceIn(-1f, 1f)
        if (stick == 0) left = at else right = at
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = e.actionIndex
                val stick = if (e.getX(i) < width / 2f) 0 else 1
                fingers[e.getPointerId(i)] = stick
                place(stick, e.getX(i), e.getY(i))
            }
            MotionEvent.ACTION_MOVE -> for (i in 0 until e.pointerCount) {
                fingers[e.getPointerId(i)]?.let { place(it, e.getX(i), e.getY(i)) }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val stick = fingers.remove(e.getPointerId(e.actionIndex))
                if (stick == 0) left = null else if (stick == 1) right = null
            }
            MotionEvent.ACTION_CANCEL -> {
                fingers.clear()
                left = null
                right = null
            }
        }
        invalidate()
        return true
    }

    override fun onDraw(c: Canvas) {
        val r = radius()
        val y = height / 2f
        for ((i, at) in listOf(left, right).withIndex()) {
            val x = centreX(i)
            fill.color = CLOUD
            c.drawRoundRect(RectF(x - r, y - r, x + r, y + r), 28f, 28f, fill)
            line.color = EDGE
            c.drawRoundRect(RectF(x - r, y - r, x + r, y + r), 28f, 28f, line)
            c.drawLine(x - r, y, x + r, y, line)
            c.drawLine(x, y - r, x, y + r, line)
            fill.color = if (at == null) EDGE else BRIGHT
            c.drawCircle(x + (at?.first ?: 0f) * r, y + (at?.second ?: 0f) * r, r * 0.2f, fill)
        }
        fill.color = INK
        fill.textSize = 30f
        fill.textAlign = Paint.Align.CENTER
        c.drawText("throttle + turn: lift off to power down", centreX(0), y + r + 34f, fill)
        c.drawText("tilt", centreX(1), y + r + 34f, fill)
    }
}
