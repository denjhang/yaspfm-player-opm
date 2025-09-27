# YASP - Yet Another Sound Player Technical Documentation OPM v0.87

## Abstract

YASP (Yet Another Sound Player) is a powerful command-line music player designed for the SPFM (Sound-Processing FM-synthesis) family of hardware. It focuses on efficiently and accurately playing classic VGM and S98 format music files. This document aims to provide a deep dive into its core technical implementations, including the advanced timing system, SPFM communication protocol, and the innovative chip conversion and caching mechanism.

---

## Table of Contents

1.  [Source Code References and Acknowledgments](#1-source-code-references-and-acknowledgments)
2.  [Project Background and Architecture](#2-project-background-and-architecture)
3.  [Core Features](#3-core-features)
4.  [Analysis of Key Technologies](#4-analysis-of-key-technologies)
    *   [4.1. VGM File Processing Workflow](#41-vgm-file-processing-workflow)
    *   [4.2. Intelligent Chip Conversion](#42-intelligent-chip-conversion)
    *   [4.3. Advanced Timing and Flush System](#43-advanced-timing-and-flush-system)
    *   [4.4. SPFM Communication Protocol](#44-spfm-communication-protocol)
5.  [User Guide](#5-user-guide)
6.  [Compilation and Build](#6-compilation-and-build)

---

## 1. Source Code References and Acknowledgments

The development of YASP was heavily influenced by the following outstanding projects. We express our sincere respect and gratitude to their authors. Their work provided a solid foundation and critical inspiration for YASP's implementation.

*   **`yasp (Yet Another SPFM Light Player)`**
    *   **Author**: uobikiemukot
    *   **Contribution**: **The starting point and initial inspiration for this project.** The name YASP is inherited from this project, and its early code structure provided a valuable initial framework and direction for development.

*   **`node-spfm`**
    *   **Authors**: Mitsutaka Okazaki (digital-sound-antiques), Denjhang
    *   **Contribution**: Provided the core logic and algorithmic foundation for this project, especially in the areas of SPFM communication protocol (`spfm.c`), chip control, and the OPN to OPM conversion algorithm (`opn_to_opm.c`).

*   **VGMPlay**
    *   **Author**: Valley Bell
    *   **Contribution**: The advanced timing system in this project (the `yasp_usleep` function in `util.c`) is entirely based on VGMPlay's core timing model, which fundamentally solved playback stability issues. Mode 6 (VGMPlay Mode) and Mode 7 (Optimized VGMPlay Mode) are directly derived from its ideas.

*   **`vgm-conv`**
    *   **Author**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **Contribution**: The precise mathematical models for chip frequency conversion were derived from this project.

*   **`vgm-parser-js`**
    *   **Author**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **Contribution**: When solving the GD3 tag caching issue, this project borrowed the robust "load entire file into memory" processing strategy from its `vgm-parser-main` module (`vgm.c`), which was key to the final successful implementation.

## 2. Project Background and Architecture

YASP aims to provide SPFM hardware enthusiasts with a powerful and highly customizable command-line music player. The project is a collaboration between **Denjhang** (Project Owner/Lead Developer) and **Cline** (AI Software Engineer). Denjhang was responsible for requirements, architecture, and key technology validation, while Cline handled implementation, integration, and documentation.

**Core Architecture:**
*   `main.c`: The main program entry point, responsible for initialization, configuration loading, the main playback loop, and keyboard input handling.
*   `play.c`: The core of the player's UI rendering and state management.
*   `vgm.c` / `s98.c`: Responsible for parsing, command processing, and playback logic for VGM and S98 files, respectively.
*   `opn_to_opm.c`, `ay_to_opm.c`, `sn_to_ay.c`: Modules that implement chip instruction conversion.
*   `util.c`: Contains utility functions such as high-precision timers, the `yasp_usleep` implementation, and `ini` file I/O.
*   `spfm.c`: Encapsulates the low-level logic for communicating with SPFM hardware via the FTDI D2XX driver.

## 3. Core Features

*   **Extensive Chip Support:** YASP supports a wide variety of classic sound chips, either directly or through automatic conversion, covering a vast range of retro game music. The supported chips are:
    *   YM2608, YM2151, YM2612, YM2203, YM2413, YM3526, YM3812, Y8950, AY8910, SN76489, YMF262, SEGAPCM, RF5C68, YM2610
*   **Multi-Format Playback:** Supports `.vgm`, `.vgz`, and `.s98` music files.
*   **GD3 Tag Fidelity:** Employs an advanced memory-based processing technique to ensure that GD3 music tags within VGM files are fully preserved during conversion and caching.
*   **Advanced Timing System:** Provides multiple timing strategies based on the high-precision performance counter to ensure accurate, jitter-free audio playback under various system loads.
*   **Built-in File Browser:** Allows users to easily navigate the file system and select music.

## 4. Analysis of Key Technologies

### 4.1. VGM File Processing Workflow

To resolve data consistency issues encountered during file conversion and caching (especially the loss of GD3 tags), YASP adopts a robust, memory-based processing workflow. The core of this mechanism is: when a cache needs to be created, the **entire original VGM file is first read into memory in a single operation**, and the file handle is then closed. Subsequent conversion, GD3 tag extraction, and writing of the new cache file are all performed on this in-memory copy. This "load completely, then process in isolation" strategy ensures the data source is unique and immutable, which is the cornerstone of smooth playback, error-free conversion, and the preservation of GD3 tags.

The following are key code snippets from `vgm.c`'s `vgm_play` function that implement this workflow:

**1. Read the Entire Original VGM File into Memory**
```c
// 1. Read entire original file into memory
fseek(input_fp, 0, SEEK_END);
long original_file_size = ftell(input_fp);
fseek(input_fp, 0, SEEK_SET);
uint8_t* original_file_data = malloc(original_file_size);
if (!original_file_data || fread(original_file_data, 1, original_file_size, input_fp) != original_file_size) {
    logging(LOG_ERROR, "Failed to read original VGM file into memory.");
    if (original_file_data) free(original_file_data);
    return;
}
fclose(input_fp); // Close original file, we have it in memory now
```
**Explanation**: This is a direct implementation of the "load completely" strategy. The code gets the file size, allocates an equivalent amount of memory, and reads the entire file into the `original_file_data` buffer in one go. After this, the original file handle is closed, and all subsequent operations are independent of the on-disk file, fundamentally preventing file pointer confusion.

**2. Extract GD3 Tags from Memory and Write to Cache**
```c
// 4. Write GD3 block from memory to cache
long gd3_start_in_cache = 0;
uint32_t gd3_offset_in_header = read_le32(original_file_data + 0x14);
if (gd3_offset_in_header > 0) {
    uint32_t gd3_abs_offset = 0x14 + gd3_offset_in_header;
    uint32_t gd3_length = read_le32(original_file_data + gd3_abs_offset + 8);
    uint32_t total_gd3_size = 12 + gd3_length;
    
    gd3_start_in_cache = ftell(g_cache_fp);
    fwrite(original_file_data + gd3_abs_offset, 1, total_gd3_size, g_cache_fp);
}
```
**Explanation**: This is a critical step in the "process in isolation" strategy. The code reads the offset and length of the GD3 tag directly from the `original_file_data` memory buffer, then writes the complete GD3 data block (including its header and content) verbatim into the new cache file. This ensures a lossless transfer of GD3 information.

**3. Perform Data Conversion from Memory**
```c
// In vgm_play:
const uint8_t* vgm_data_ptr = original_file_data + g_vgm_header.vgm_data_offset;
size_t vgm_data_size = (g_vgm_header.eof_offset + 4) - g_vgm_header.vgm_data_offset;
vgm_convert_and_cache_opn_to_opm_from_mem(vgm_data_ptr, vgm_data_size, &g_vgm_header);

// The conversion function itself:
static bool vgm_convert_and_cache_opn_to_opm_from_mem(const uint8_t* vgm_data, size_t vgm_data_size, const vgm_header_t* original_header) {
    // ... loop through vgm_data (memory buffer) and convert ...
}
```
**Explanation**: The input to the conversion function `vgm_convert_and_cache_opn_to_opm_from_mem` is no longer a file pointer, but a pointer `vgm_data` to the VGM data section in memory. This completely avoids data inconsistency issues that could arise from read/write operations on a file stream, ensuring the conversion process is pure and reliable.

### 4.2. Intelligent Chip Conversion

When YASP detects that a chip required by a VGM file is missing on the user's hardware but a viable alternative exists, it automatically performs a conversion.

#### 4.2.1. Conversion Path Overview

| Source Chip | Target Chip | Conversion Module(s) | Remarks |
| :--- | :--- | :--- | :--- |
| **YM2612/2203/2608 (OPN)** | YM2151 (OPM) | `opn_to_opm.c` | Core FM-synthesis conversion, supporting mapping of frequency, envelope, etc. |
| **AY8910 (PSG)** | YM2151 (OPM) | `ay_to_opm.c` | Simulates PSG's square waves and noise using OPM's FM synthesis. |
| **SN76489 (DCSG)** | AY8910 (PSG) | `sn_to_ay.c` | Intermediate step, converts SN76489 instructions to AY8910 instructions. |
| **SN76489 (DCSG)** | YM2151 (OPM) | `sn_to_ay.c` -> `ay_to_opm.c` | **Conversion Chain**: SN76489 -> AY8910 -> YM2151. |

#### 4.2.2. Core Conversion Source Code Explained

**1. OPN (YM2612/2203/2608) -> OPM (YM2151)**

This is the core FM-synthesis conversion, primarily implemented in `opn_to_opm.c`. The OPN series uses `F-Number` and `Block` to define frequency, while OPM uses `Key Code` and `Key Fraction`. The conversion must be done through the following mathematical formulas:

```c
// 1. Convert OPN parameters to actual frequency (Hz)
double freq = (_source_clock * fnum) / ((72.0 * _clock_div) * (1 << (20 - blk)));

// 2. Convert frequency to OPM's pitch value (Key)
const double BASE_FREQ_OPM = 277.2; // OPM base frequency
double key = 60.0 + log2((freq * _clock_ratio) / BASE_FREQ_OPM) * 12.0;

// 3. Split Key value into Key Code (KC) and Key Fraction (KF)
*kc = (oct << 4) | note;
*kf = (uint8_t)floor(frac * 64.0);
```
This code accurately maps the frequency system of one sound source to a completely different one.

**2. AY8910 (PSG) -> OPM (YM2151)**

This conversion, implemented in `ay_to_opm.c`, aims to **simulate** the PSG's square waves and noise using the OPM's FM synthesis. Since OPM does not have native square wave channels, the code uses a special FM patch (`CON=4`) to simulate them.

```c
// In ay_to_opm_init, set up a square-wave-like patch for OPM channels 4, 5, 6
_y(0x20 + opmCh, 0xfc); // RL=ON FB=7 CON=4
_y(0x60 + opmCh, 0x1b); // M1: TL=27 (set a base volume)
_y(0x80 + opmCh, 0x1f); // M1: AR=31 (fast attack)
```

Additionally, the PSG's logarithmic volume is converted to the OPM's linear `Total Level` (TL) via the `VOL_TO_TL` lookup table.

```c
// The VOL_TO_TL array
static const int VOL_TO_TL[] = {127, 62, 56, 52, 46, 42, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0};

// Applied in the _updateTone function
const int tVol = (v & 0x10) ? 0 : (v & 0xf); // Get volume value
_y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf])); // Look up and write to OPM's TL register
```

**3. SN76489 (DCSG) -> AY8910 (PSG)**

This is an intermediate conversion step implemented in `sn_to_ay.c`. Its core task is to handle the mixing logic of the SN76489's channel 3 and noise, which behaves differently from the AY8910.

```c
// Mixing logic is handled in the _updateSharedChannel function
static void _updateSharedChannel() {
    int noiseChannel = _mixChannel; // Usually channel 2
    bool enableTone = _atts[noiseChannel] != 0xf;
    bool enableNoise = _atts[3] != 0xf;

    // Mix resolution: if both are enabled, noise wins
    if (enableTone && enableNoise) {
        enableTone = false;
    }
    
    // Set the AY8910's channel and noise enable register (R7) accordingly
    const uint8_t toneMask = enableTone ? 0 : (1 << noiseChannel);
    const uint8_t noiseMask = enableNoise ? (7 & ~(1 << noiseChannel)) : 7;
    _y(7, (noiseMask << 3) | toneMask); // _y() calls ay_to_opm_write_reg()
}
```
This piece of code is central to making the entire conversion chain sound correct.

### 4.3. Advanced Timing and Flush System

YASP's playback precision and stability are determined by the combination of "Flush Mode" and "Timer Mode".

#### 4.3.1. Flush Mode

Flush mode determines the **granularity** at which the player sends register data to the SPFM hardware.

| Mode (Key) | Name | Principle |
| :---: | :--- | :--- |
| **1** | Register-Level | `spfm_flush()` is called immediately after **every single** register write. This ensures the highest real-time responsiveness but creates huge USB communication overhead. |
| **2** | **Command-Level** | `spfm_flush()` is only called after a complete VGM command is processed (e.g., a wait command like `0x61 nn nn`, or a chip write command). This is the **best balance between performance and real-time feel** and is the default setting. |

#### 4.3.2. Timer Mode

Timer mode determines how the player **waits** precisely for the next event after processing a VGM command. All modes use the high-precision performance counter (`QueryPerformanceCounter`) for error compensation.

| Mode (Key) | Source Code Name | Principle |
| :---: | :--- | :--- |
| **3** | High-Precision Sleep | Calls `Sleep(1)` to wait and compensates for its significant inaccuracy using the performance counter. Suitable for low-load environments. |
| **4** | Hybrid Sleep | Uses `CreateWaitableTimer` to wait for 1ms and compensates for errors. More precise than `Sleep(1)`. |
| **5** | Multimedia Timer | Uses `timeSetEvent` to set up a 1ms high-precision multimedia timer. The playback thread waits for the event triggered by this timer. High precision, but events can be delayed under high load. |
| **6** | **VGMPlay Mode** | A "busy-wait" variant that constantly calls `Sleep(0)` to yield its time slice. Extremely responsive, but causes very high CPU usage. |
| **7** | **Optimized VGMPlay Mode** | An optimized version of Mode 5 with an "anti-runaway" mechanism to prevent audio lag and crashes. **This is the default and best choice.** |

#### 4.3.3. Timer Core Source Code Explained

The following are the core implementations for each mode in the `yasp_usleep` function from `util.c`:

**Mode 3: High-Precision Sleep**
```c
case 0: // High-Precision Sleep
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        Sleep(1);
    }
    break;
```
**Explanation**: This is the simplest implementation. It loops, calling `Sleep(1)` to yield at least 1ms of CPU time until the performance counter reaches the target time. The precision of `Sleep` is poor, but it has low CPU usage.

**Mode 4: Hybrid Sleep**
```c
case 1: // Hybrid Sleep
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        WaitForSingleObject(g_waitable_timer, 1);
    }
    break;
```
**Explanation**: Uses Windows' waitable timer `WaitForSingleObject` to wait for 1ms. It is more precise than `Sleep(1)` and offers a compromise between precision and CPU usage.

**Mode 5: Multimedia Timer**
```c
case 2: // Multimedia Timer
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        WaitForSingleObject(g_timer_event, 1);
    }
    break;
```
**Explanation**: Relies on a `g_timer_event` that is triggered every 1ms in the background by `timeSetEvent`. The thread waits for this event, providing high precision.

**Mode 6: VGMPlay Mode**
```c
case 3: // VGMPlay Mode
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        Sleep(0);
    }
    break;
```
**Explanation**: This is a form of "busy-waiting". `Sleep(0)` immediately yields the remainder of the current thread's time slice, allowing other threads to run. This allows the loop to check the time very frequently, making it extremely responsive but at the cost of high CPU usage.

**Mode 7: Optimized VGMPlay Mode**
The optimization for this mode is not in `yasp_usleep` but in the `vgm_play` function in `play.c`. It uses the same waiting mechanism as Mode 5 but adds an "anti-runaway" logic:
```c
// In play.c's vgm_play
if (g_timer_mode == 7) {
    vgm_slice_to_do = min(vgm_slice_to_do, g_vgm_slice_limiter);
}
```
**Explanation**: If the system is busy and timer events are severely delayed, causing a large backlog of samples to be played (`vgm_slice_to_do`), this code limits the backlog to a maximum value (`g_vgm_slice_limiter`). This prevents long audio lags and allows the playback to quickly catch up to real-time, at the cost of dropping a few backlogged samples.

### 4.4. SPFM Communication Protocol

YASP communicates with SPFM hardware via the FTDI D2XX driver. Its core is a **write buffering mechanism** and a **hybrid wait strategy**. For extremely short waits (<10 samples), it sends a hardware wait command `0x80`; for longer waits, it calls the high-precision software wait function `yasp_usleep`, which is determined by the selected timer mode.

## 5. User Guide

| Key | Function |
| :--: | :-- |
| `q` | Quit |
| `p` | Pause/Resume |
| `n` / `b` | Next / Previous Track |
| `r` | Toggle Random/Sequential Mode |
| `f` | Open/Close File Browser |
| `+` / `-` | Speed Up / Slow Down |
| `1` / `2` | Switch Flush Mode |
| `3`-`7` | Switch Timer Mode |

## 6. Compilation and Build

Run `make` in the `console_player` directory.
