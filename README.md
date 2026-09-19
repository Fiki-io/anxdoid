# Anxdoid: Userspace Android-in-Android ARM64 Runtime

[![Build Anxdoid Runtime](https://github.com/OWNER/REPOSITORY/actions/workflows/build.yml/badge.svg)](https://github.com/OWNER/REPOSITORY/actions/workflows/build.yml)

**Anxdoid** adalah proyek eksperimental tingkat lanjut untuk menjalankan ROM Android AOSP ARM64 sekunder secara *live* dan *native* di dalam aplikasi Android unprivileged (non-root sandbox `untrusted_app`).

---

## ⚡ Karakteristik & Terobosan Utama

1. **100% Native ARM64 Execution**: Tidak menggunakan QEMU / DBT / CPU Emulation. Instruksi CPU guest dieksekusi langsung oleh prosesor fisik ARM64 host.
2. **Pure Non-Root Sandbox**: Berjalan pada domain SELinux standar `untrusted_app` tanpa memerlukan hak root, tanpa Magisk/KernelSU, dan tanpa UBL (Unlock Bootloader).
3. **Tanpa pKVM / EL2 Hypervisor**: Universal untuk semua SoC Android (Qualcomm Snapdragon, MediaTek, Exynos).
4. **Tanpa PRoot / ptrace**: Menghindari latensi context switch kernel dan batasan `neverallow` SELinux Android modern.
5. **AOSP GSI Mentah (Raw Image)**: Menggunakan partisi `system.img` mentah yang diekstrak langsung ke storage privat aplikasi (`/data/data/com.anxdoid.runtime/files/rootfs/`).
6. **Init Bypass & Direct Linker Trampoline**: Langsung mengeksekusi biner runtime melalui dynamic linker guest (`linker64`) dengan injeksi environment `BOOTCLASSPATH`, `ANDROID_ROOT`, dan `ANDROID_DATA`.
7. **Virtualization Layer**:
   - **Display**: Rendering via SwiftShader (CPU software rasterizer) disalurkan ke `ANativeWindow` (`SurfaceView`).
   - **Input**: Translasi `MotionEvent` Android ke struktur biner `input_event` Linux.
   - **Network**: lwIP user-space TCP/IP stack (translasi paket IP ke POSIX sockets host tanpa `/dev/net/tun`).
   - **Binder IPC**: Virtual Binder Router di C++ User-space.

---

## 🛠️ Cara Build & Test via GitHub Actions (GHA)

Anda tidak perlu menginstal Android Studio atau NDK di komputer lokal. Proyek ini sudah dilengkapi pipeline CI/CD otomatis:

1. **Push ke GitHub**:
   ```bash
   git add .
   git commit -m "feat: initial anxdoid runtime engine"
   git push origin main
   ```
2. **Lihat Proses Build**:
   - Buka tab **Actions** di repositori GitHub Anda.
   - Pilih workflow **Build Anxdoid Runtime**.
3. **Unduh APK Hasil Build**:
   - Setelah workflow selesai (tanda centang hijau), buka detail build.
   - Di bagian **Artifacts**, klik **anxdoid-runtime-debug-apk** untuk mengunduh `app-debug.apk`.
4. **Instal & Jalankan**:
   - Instal `app-debug.apk` di HP Android fisik (ARM64).
   - Buka aplikasi **Anxdoid Runtime**.

---

## 📂 Struktur Repositori

```
anxdoid/
├── .github/
│   └── workflows/
│       └── build.yml               # CI/CD otomatis build APK & C++ NDK
├── app/
│   ├── build.gradle.kts            # targetSdk 28, NDK arm64-v8a filter, CMake
│   └── src/
│       └── main/
│           ├── AndroidManifest.xml
│           ├── cpp/
│           │   ├── CMakeLists.txt
│           │   ├── include/
│           │   │   └── anxdoid_common.h
│           │   └── runtime/
│           │       └── native_bridge.cpp   # JNI bridge, SurfaceView pipe, Process launcher
│           ├── java/com/anxdoid/runtime/
│           │   ├── MainActivity.kt         # Host UI & touch event dispatcher
│           │   └── core/
│           │       └── NativeBridge.kt     # Kotlin JNI bindings
│           └── res/                        # Layout & resources
└── README.md
```
