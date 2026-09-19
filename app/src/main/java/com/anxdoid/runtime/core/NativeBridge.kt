package com.anxdoid.runtime.core

import android.view.Surface

/**
 * JNI Bridge to communicate between Host Android UI and the native C++
 * Userspace ARM64 Engine.
 */
object NativeBridge {

    init {
        System.loadLibrary("anxdoid_runtime")
    }

    /**
     * Initializes the runtime environment with paths to internal sandbox storage.
     */
    external fun initRuntime(
        rootDir: String,
        rootfsDir: String
    ): Boolean

    /**
     * Passes the ANativeWindow handle to C++ for SwiftShader / Framebuffer blitting.
     */
    external fun setSurface(surface: Surface): Boolean

    /**
     * Clears the active surface when destroyed.
     */
    external fun clearSurface()

    /**
     * Dispatches motion/touch events from host to the guest input pipeline.
     */
    external fun sendTouchEvent(
        action: Int,
        x: Float,
        y: Float,
        pressure: Float
    ): Boolean

    /**
     * Starts the guest runtime process (fork + execve of linker64 / app_process).
     */
    external fun startGuestRuntime(): Int

    /**
     * Stops/terminates the running guest environment.
     */
    external fun stopGuestRuntime(): Boolean
}
