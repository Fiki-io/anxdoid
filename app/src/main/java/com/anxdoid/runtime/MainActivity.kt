package com.anxdoid.runtime

import android.annotation.SuppressLint
import android.os.Bundle
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.anxdoid.runtime.core.NativeBridge
import java.io.File

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "AnxdoidHost"
    }

    private lateinit var guestSurfaceView: SurfaceView
    private lateinit var statusText: TextView
    private lateinit var btnBoot: Button
    private lateinit var btnReset: Button
    private lateinit var hudOverlay: View

    private var isSurfaceReady = false
    private var isGuestRunning = false

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        guestSurfaceView = findViewById(R.id.guestSurfaceView)
        statusText = findViewById(R.id.statusText)
        btnBoot = findViewById(R.id.btnBoot)
        btnReset = findViewById(R.id.btnReset)
        hudOverlay = findViewById(R.id.hudOverlay)

        guestSurfaceView.holder.addCallback(this)

        // Setup touch forwarding to guest
        guestSurfaceView.setOnTouchListener { _, event ->
            if (isGuestRunning) {
                NativeBridge.sendTouchEvent(
                    action = event.actionMasked,
                    x = event.x,
                    y = event.y,
                    pressure = event.pressure
                )
            }
            true
        }

        // Initialize Native Runtime directories
        val baseDir = filesDir.absolutePath
        val rootfsDir = File(filesDir, "rootfs").absolutePath
        File(rootfsDir).mkdirs()

        val initialized = NativeBridge.initRuntime(baseDir, rootfsDir)
        if (initialized) {
            statusText.text = getString(R.string.status_ready)
            Log.i(TAG, "Native runtime initialized. Rootfs: $rootfsDir")
        } else {
            statusText.text = "Failed to initialize native C++ runtime!"
            Log.e(TAG, "Failed to initialize native runtime.")
        }

        btnBoot.setOnClickListener {
            if (!isSurfaceReady) {
                Toast.makeText(this, "Surface not ready yet", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            statusText.text = "Starting Guest Android Process…"
            val pid = NativeBridge.startGuestRuntime()
            if (pid > 0) {
                isGuestRunning = true
                statusText.text = "Guest Android running (PID: $pid)"
                btnBoot.isEnabled = false
                Log.i(TAG, "Guest runtime launched successfully with PID: $pid")
            } else {
                statusText.text = "Guest runtime launcher returned: $pid"
                Log.w(TAG, "Guest runtime not started, code: $pid")
            }
        }

        btnReset.setOnClickListener {
            val rootfs = File(filesDir, "rootfs")
            val dataDir = File(rootfs, "data")
            if (dataDir.exists()) {
                dataDir.deleteRecursively()
                Toast.makeText(this, "Guest data wiped cleanly (Instant Reset)", Toast.LENGTH_SHORT).show()
                Log.i(TAG, "Guest data partition purged.")
            } else {
                Toast.makeText(this, "Guest data is already clean.", Toast.LENGTH_SHORT).show()
            }
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "Surface created, attaching to NativeBridge...")
        val attached = NativeBridge.setSurface(holder.surface)
        isSurfaceReady = attached
        Log.i(TAG, "Surface attached result: $attached")
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "Surface changed: ${width}x${height}, format: $format")
        NativeBridge.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "Surface destroyed")
        isSurfaceReady = false
        NativeBridge.clearSurface()
    }

    override fun onDestroy() {
        super.onDestroy()
        if (isGuestRunning) {
            NativeBridge.stopGuestRuntime()
        }
    }
}
