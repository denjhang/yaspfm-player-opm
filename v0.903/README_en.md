# YASP - Yet Another Sound Player (Technical Documentation)

## Abstract

YASP (Yet Another Sound Player) is a powerful command-line music player designed for the SPFM (Sound-Processing FM-synthesis) family of hardware. It focuses on efficiently and accurately playing classic VGM and S98 format music files. This document aims to provide a deep dive into its core technical implementations, including the advanced timing system, SPFM communication protocol, and the innovative chip conversion and caching mechanism.

---

## Table of Contents
* [1. Source Code References and Acknowledgments](#1)
* [2. Project Background and Architecture](#2)
* [3. Core Features](#3)
* [4. Analysis of Key Technologies](#4)
  * [4.1. VGM File Processing Workflow](#4-1)
  * [4.2. Intelligent Chip Conversion](#4-2)
    * [4.2.1. Conversion Path Overview](#4-2-1)
    * [4.2.2. OPN to OPM Detailed Conversion Rules](#4-2-2)
    * [4.2.3. AY-8910 to OPM Detailed Conversion Rules](#4-2-3)
  * [4.3. Advanced Timing and Flush System](#4-3)
    * [4.3.1. Flush Mode](#4-3-1)
    * [4.3.2. Timer Mode](#4-3-2)
    * [4.3.3. Timer Core Source Code Explained](#4-3-3)
  * [4.4. SPFM Communication Protocol](#4-4)
* [5. Performance Optimization-Version Comparison](#5)
  * [5.1. The Root Cause: Why Was the Old Version Faster?](#5-1)
  * [5.2. The Fixes: How We Restored and Surpassed the Old Performance](#5-2)
* [6. User Guide](#6)
* [7. Compilation and Build](#7)
* [8. Troubleshooting and Changelog](#8)
  * [8.1. Troubleshooting: Uncontrolled, Rapid Skipping of Tracks](#8-1)
    * [8.1.1. Diagnostic Process](#8-1-1)
    * [8.1.2. The Final Solution](#8-1-2)
  * [8.2. Bug Fixes & Improvements (v0.881)](#8-2)
    * [8.2.1. Fixed Loop Logic for Converted VGM Files](#8-2-1)
    * [8.2.2. Fixed Auto-Skip Issue After Playback Completion](#8-2-2)
    * [8.2.3. Corrected Total Time Calculation in UI](#8-2-3)
    * [8.2.4. Corrected VGM Chip Info Display in UI](#8-2-4)
    * [8.2.5. Added Detection for Invalid VGM Files](#8-2-5)
  * [8.3. Feature Enhancements & Bug Fixes (v0.884)](#8-3)
    * [8.3.1. Optimized Startup and Directory Management](#8-3-1)
    * [8.3.2. Enhanced File Browser Functionality](#8-3-2)
    * [8.3.3. Added Clear Cache Functionality](#8-3-3)
  * [8.4. AY-8910 Envelope Waveform Conversion Fix (v0.888)](#8-4)
  * [8.5. AY-8910 Fast Arpeggio Conversion Fix (v0.903)](#8-5)

---

## 1. Source Code References and Acknowledgments
<a id="1"></a>
The development of YASP was heavily influenced by the following outstanding projects. We express our sincere respect and gratitude to their authors. Their work provided a solid foundation and critical inspiration for YASP's implementation.

*   **`yasp (Yet Another SPFM Light Player)`**
    *   **Author**: uobikiemukot
    *   **Contribution**: **The starting point and initial inspiration for this project.** The name YASP is inherited from this project, and its early code structure provided a valuable initial framework and direction for development.

*   **`node-spfm`**
    *   **Authors**: Mitsutaka Okazaki (digital-sound-antiques), Denjhang
    *   **Contribution**: Provided the core logic and algorithmic foundation for this project, especially in the areas of SPFM communication protocol (`spfm.c`), chip control, and the OPN to OPM conversion algorithm (`opn_to_opm.c`).

*   **`VGMPlay`**
    *   **Author**: Valley Bell
    *   **Contribution**: The advanced timing system in this project (the `yasp_usleep` function in `util.c`) is entirely based on VGMPlay's core timing model, which fundamentally solved playback stability issues. Mode 6 (VGMPlay Mode) and Mode 7 (Optimized VGMPlay Mode) are directly derived from its ideas.

*   **`vgmplay-msx`**
    *   **Author**: Laurens Holst (grauw)
    *   **Contribution**: Provided the precise look-up table for OPN to OPM LFO frequency conversion, originating from the file `vgmplay-msx/src/drivers/emulations/OPNFMOnSFG.asm` in his project. Without this reference, achieving an audibly correct LFO effect would not have been possible.

*   **`vgm-conv`**
    *   **Author**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **Contribution**: The precise mathematical models for chip frequency conversion were derived from this project.

*   **`vgm-parser-js`**
    *   **Author**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **Contribution**: When solving the GD3 tag caching issue, this project borrowed the robust "load entire file into memory" processing strategy from its `vgm-parser-main` module (`vgm.c`), which was key to the final successful implementation.

*   **`Vortex Tracker II`**
    *   **Author**: Sergey Bulba (salah)
    *   **Contribution**: Provided significant inspiration and reference for implementing advanced AY-8910 programming techniques, such as the "Envelope as Waveform" feature.

## 2. Project Background and Architecture
<a id="2"></a>
YASP aims to provide SPFM hardware enthusiasts with a powerful and highly customizable command-line music player. The project is a collaboration between **Denjhang** (Project Owner/Lead Developer) and **Cline** (AI Software Engineer). Denjhang was responsible for requirements, architecture, and key technology validation, while Cline handled implementation, integration, and documentation.

**Core Architecture:**
*   `main.c`: The main program entry point, responsible for initialization, configuration loading, the main playback loop, and keyboard input handling.
*   `play.c`: The core of the player's UI rendering and state management.
*   `vgm.c` / `s98.c`: Responsible for parsing, command processing, and playback logic for VGM and S98 files, respectively.
*   `opn_to_opm.c`, `ay_to_opm.c`, `sn_to_ay.c`: Modules that implement chip instruction conversion.
*   `util.c`: Contains utility functions such as high-precision timers, the `yasp_usleep` implementation, and `ini` file I/O.
*   `spfm.c`: Encapsulates the low-level logic for communicating with SPFM hardware via the FTDI D2XX driver.

## 3. Core Features
<a id="3"></a>
*   **Extensive Chip Support:** YASP supports a wide variety of classic sound chips, either directly or through automatic conversion, covering a vast range of retro game music. The supported chips are:
    *   YM2608, YM2151, YM2612, YM2203, YM2413, YM3526, YM3812, Y8950, AY8910, SN76489, YMF262, SEGAPCM, RF5C68, YM2610
*   **Multi-Format Playback:** Supports `.vgm`, `.vgz`, and `.s98` music files.
*   **GD3 Tag Fidelity:** Employs an advanced memory-based processing technique to ensure that GD3 music tags within VGM files are fully preserved during conversion and caching.
*   **Advanced Timing System:** Provides multiple timing strategies based on the high-precision performance counter to ensure accurate, jitter-free audio playback under various system loads.
*   **Built-in File Browser:** Allows users to easily navigate the file system and select music.

## 4. Analysis of Key Technologies
<a id="4"></a>
### 4.1. VGM File Processing Workflow
<a id="4-1"></a>
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
<a id="4-2"></a>
When YASP detects that a chip required by a VGM file is missing on the user's hardware but a viable alternative exists, it automatically performs a conversion.

#### 4.2.1. Conversion Path Overview
<a id="4-2-1"></a>
| Source Chip | Target Chip | Conversion Module(s) | Remarks |
| :--- | :--- | :--- | :--- |
| **YM2612/2203/2608 (OPN)** | YM2151 (OPM) | `opn_to_opm.c` | Core FM-synthesis conversion, supporting mapping of frequency, envelope, etc. |
| **AY8910 (PSG)** | YM2151 (OPM) | `ay_to_opm.c` | Simulates PSG's square waves and noise using OPM's FM synthesis. |
| **SN76489 (DCSG)** | AY8910 (PSG) | `sn_to_ay.c` | Intermediate step, converts SN76489 instructions to AY8910 instructions. |
| **SN76489 (DCSG)** | YM2151 (OPM) | `sn_to_ay.c` -> `ay_to_opm.c` | **Conversion Chain**: SN76489 -> AY8910 -> YM2151. |

#### 4.2.2. OPN to OPM Detailed Conversion Rules
<a id="4-2-2"></a>
To play OPN music without an OPN chip, YASP implements a real-time, register-level converter. It translates OPN register writes into functionally equivalent OPM register writes.

##### 4.2.2.1. Register Map Overview
| OPN (YM2612/2203) | OPM (YM2151) | Brief Description |
| :--- | :--- | :--- |
| `0x22` (LFRQ) | `0x18` (LFRQ) | LFO Frequency conversion |
| `0x28` (Key On/Off) | `0x08` (Key On/Off) | Channel and operator on/off switch |
| `0x30..0x8F` (Operator) | `0x40..0xBF` (Operator) | Operator parameters (DT, MUL, TL, KS, AR, DR, SR, RR, SL, SSG-EG) |
| `0xA0..0xA6` (F-Num/Block) | `0x28..0x37` (KC/KF) | Frequency (pitch) conversion |
| `0xB0..0xB2` (FB/CONNECT) | `0x20..0x27` (RL/FB/CONNECT) | Feedback and Algorithm |
| `B4..B6` (L/R/AMS/PMS) | `0x38..0x3F` (PMS/AMS) | Panning, LFO sensitivity |

##### 4.2.2.2. Detailed Conversion Rules
###### 1. LFO Frequency (LFRQ)
*   **OPN `0x22` -> OPM `0x18`**
*   OPN's LFO has only 8 frequency steps, while OPM's is more granular. The converter uses a Look-Up Table (LUT) to map the OPN value to the closest equivalent on the OPM. This LUT is based on the implementation in `OPNFMOnSFG.asm` to ensure auditory consistency.

    **Look-Up Table (C Array):**
    ```c
    const uint8_t lfo_lut[] = {0, 0, 0, 0, 0, 0, 0, 0, 0xC1, 0xC7, 0xC9, 0xCB, 0xCD, 0xD4, 0xF9, 0xFF};
    ```

    **Usage Example:**
    When a value is written to OPN's `0x22` register, the program takes its lower 4 bits (`data & 0x0F`) as an index to look up the corresponding OPM value in the `lfo_lut` array. This value is then written to OPM's `0x18` register.

    | OPN `0x22` Input (data) | Index (data & 0x0F) | OPM `0x18` Output (lfo_lut[index]) |
    | :---: | :---: | :---: |
    | `0x00` - `0x07` | 0 - 7 | `0x00` (LFO Off) |
    | `0x08` | 8 | `0xC1` |
    | `0x09` | 9 | `0xC7` |
    | `0x0A` | 10 | `0xC9` |
    | `0x0B` | 11 | `0xCB` |
    | `0x0C` | 12 | `0xCD` |
    | `0x0D` | 13 | `0xD4` |
    | `0x0E` | 14 | `0xF9` |
    | `0x0F` | 15 | `0xFF` |

###### 2. Key On/Off
*   **OPN `0x28` -> OPM `0x08`**
*   This register controls which operators (slots) of which channel are activated or silenced. The channel selection is in the lower 3 bits for both OPN and OPM. The operator selection is in the upper 4 bits for OPN and the upper 3 bits for OPM. The conversion requires appropriate bit shifting and masking.
*   **Key Code**: `_write_func(0x08, (slots << 3) | ch);`

###### 3. Operator Parameters (DT, MUL, TL, etc.)
*   **OPN `0x30..0x8F` -> OPM `0x40..0xBF`**
*   These registers control the core parameters of FM synthesis, such as Detune, Multiple, Total Level, Attack Rate, etc. Fortunately, OPN and OPM are largely compatible in their bit definitions for these parameters. The conversion is mainly a recalculation of addresses.
*   The address format for OPN is `Base + Channel + SlotOffset`, and for OPM it is `Base + Channel + SlotOffset`. The conversion function calculates the corresponding OPM address based on the input OPN address.
*   **Key Code**: `uint8_t base = 0x40 + ((addr & 0xf0) - 0x30) * 2; uint8_t offset = slot * 8 + ch; _write_func(base + offset, data);`

###### 4. Frequency (F-Number/Block to Key Code/Fraction)
*   **OPN `0xA0..0xA6` -> OPM `0x28..0x37`**
*   This is the most complex conversion. OPN uses `F-Number` and `Block` (octave) to define frequency, while OPM uses `Key Code` (note) and `Key Fraction` (fine-tuning).
*   The conversion process is as follows:
    1.  Calculate the actual frequency in Hz from the OPN's F-Num and Block.
    2.  `freq = (source_clock * fnum) / (divisor * (1 << (20 - blk)))`
    3.  Convert the calculated frequency (Hz) into an OPM note and fraction.
    4.  `key = 60 + log2((freq * clock_ratio) / BASE_FREQ_OPM) * 12.0`
    5.  Finally, decompose `key` into OPM's `KC` (octave + note) and `KF` (fraction) and write them to the respective registers.
*   **Key Code**: `opn_freq_to_opm_key(fnum, blk, &kc, &kf); _write_func(0x28 + ch, kc); _write_func(0x30 + ch, kf << 2);`

###### 5. Feedback/Algorithm (FB/CONNECT)
*   **OPN `0xB0..0xB2` -> OPM `0x20..0x27`**
*   The bit definitions for OPN and OPM are similar here, but the OPM register also includes panning information (RL). Therefore, when writing, the panning setting for that channel must first be read from a cache, then combined with the feedback/algorithm value before being written.
*   **Key Code**: `_write_func(0x20 + ch, (get_rl_flags(ch) << 6) | (data & 0x3f));`

###### 6. Panning/LFO Sensitivity (L/R/AMS/PMS)
*   **OPN `0xB4..0xB6` -> OPM `0x38..0x3F`**
*   OPN packs panning (L/R) and LFO sensitivity (AMS/PMS) into the same register, while OPM separates them.
*   During conversion:
    1.  The panning information is extracted from the OPN register and cached for use in other register conversions.
    2.  PMS (Pitch Modulation Sensitivity) and AMS (Amplitude Modulation Sensitivity) are extracted.
    3.  Notably, to allow the user to control the intensity of the LFO effect in real-time, the code applies a global amplitude multiplier `g_opn_lfo_amplitude` to the PMS value. After extensive testing, it was found that setting this scaling factor to **0.9** produces a sound that most closely matches the original OPN audio.
    4.  The processed PMS and AMS are written to OPM's `0x38-0x3F` registers.
*   **Key Code**: `uint8_t scaled_pms = (uint8_t)(pms * g_opn_lfo_amplitude); _write_func(0x38 + ch, (scaled_pms << 4) | ams);`

#### 4.2.3. AY-8910 to OPM Detailed Conversion Rules
<a id="4-2-3"></a>
Converting sound from an AY-8910 (PSG) to a YM2151 (OPM) is a unique challenge because it requires using FM synthesis to **simulate** a completely different sound generation method (Programmable Sound Generator). YASP uses 3 channels of the OPM to simulate the PSG's 3 square wave channels and a 4th channel to simulate its noise.

##### 4.2.3.1. Register Map Overview
| AY-8910 (PSG) | YM2151 (OPM) | Brief Description |
| :--- | :--- | :--- |
| `R0, R1` (Tone Period A) | `0x28, 0x30` (KC/KF Ch 4) | Frequency for Channel A |
| `R2, R3` (Tone Period B) | `0x29, 0x31` (KC/KF Ch 5) | Frequency for Channel B |
| `R4, R5` (Tone Period C) | `0x2A, 0x32` (KC/KF Ch 6) | Frequency for Channel C |
| `R6` (Noise Period) | `0x0F` (Noise Control) | Noise frequency |
| `R7` (Mixer) | `0x70-0x77` (TL) / `0x08` (Key On/Off) | Controls tone/noise enable and volume |
| `R8` (Amplitude A) | `0x74` (TL Ch 4) | Volume for Channel A |
| `R9` (Amplitude B) | `0x75` (TL Ch 5) | Volume for Channel B |
| `R10` (Amplitude C) | `0x76` (TL Ch 6) | Volume for Channel C |
| `R11, R12` (Envelope Period) | (Internal State) | Frequency of the envelope generator |
| `R13` (Envelope Shape) | (Internal State) / `0x28, 0x30` (KC/KF) | Envelope shape, or pitch when used as a waveform |

##### 4.2.3.2. Detailed Conversion Rules
###### 1. Square Wave Tone
*   **AY `R0-R5` -> OPM `0x28-0x32`**
*   The AY-8910 uses a 12-bit period value to define the frequency of its square wave. The converter first calculates the actual frequency in Hz based on this period.
*   **Frequency Calculation**: `freq = source_clock / (16 * tone_period)`
*   This frequency value is then fed into the same `freqToOPMNote` function used for OPN->OPM conversion, which calculates the OPM's `KC` (Key Code) and `KF` (Key Fraction) values. These are then written to the frequency registers of the corresponding OPM channel.
*   To simulate the timbre of a square wave, the OPM channels are preset with a simple FM configuration that produces harmonics close to those of a square wave. The following is the instrument definition in MML2VGM format:

    ```
    '@ PSG Square Wave
       AR  DR  SR  RR  SL  TL  KS  ML  DT1 DT2 AME
    '@ 031,000,000,000,000,027,000,002,000,000,000 ; M1 (Modulator)
    '@ 031,000,000,000,000,000,000,001,000,000,000 ; C1 (Carrier)
    '@ 000,000,000,000,000,000,000,000,000,000,000 ; M2 (Unused)
    '@ 000,000,000,000,000,000,000,000,000,000,000 ; C2 (Unused)
       ALG FB
    '@ 004,007
    ```
    **Explanation**:
    *   **Algorithm 4** is used, where one modulator (M1) affects one carrier (C1).
    *   **Feedback (FB) is set to 7**, adding harmonics to the modulator itself, making its timbre closer to a square wave.
    *   The modulator's **Multiplier (ML) is 2** and the carrier's is **1**. This non-integer relationship helps generate rich odd harmonics.
    *   The modulator's **Total Level (TL) is 27**, a fixed value optimized by ear to control the intensity of the harmonics. The carrier's TL is 0 in the definition (maximum volume), but is dynamically controlled by the PSG's volume registers during actual playback.

###### 2. Noise
*   **AY `R6` -> OPM `0x0F`**
*   The AY-8910's 5-bit noise period is directly mapped to the OPM's noise control register. The OPM's noise frequency is inverted (lower value means higher frequency), so an inversion is necessary.
*   **Key Code**: `_y(0x0f, 0x80 | (0x1f - nfreq));`
*   The noise volume is taken from the loudest of the three PSG channels that have their noise switch enabled.

###### 3. Volume & Mixer
*   **AY `R7-R10` -> OPM `0x70-0x77`**
*   This is a core part of the conversion logic. The AY-8910's `R7` mixer register determines whether each channel outputs a tone, noise, both, or is silent.
*   The converter handles this logic in the `_updateTone` function. For each channel, it checks the corresponding tone and noise enable bits in `R7`.
*   If the tone is enabled, the 4-bit volume from `R8-R10` is converted to a 7-bit OPM Total Level (TL) value using a look-up table `VOL_TO_TL`, and this is written to the OPM channel's volume register.
*   If the tone is disabled, the OPM channel is muted (TL set to 127).

###### 4. Exclusive Feature: Envelope as Waveform Conversion
*   **AY `R11-R13` -> OPM `0x28-0x32` (Frequency) & `0x70-0x77` (Volume)**
*   This is an exclusive feature of YASP that solves a common advanced technique in chiptune music. On the AY-8910, it's possible to set an extremely short envelope period, causing the hardware envelope itself to oscillate at high speed and thus be used **as a new sound source**.
*   **Detection Mechanism**: The converter checks the value of the envelope period registers (`R11`, `R12`) to determine if this mode is active. If the period is very small (the threshold is set to `200` in the code), it assumes the envelope is being used as a waveform generator.
    ```c
    // ay_to_opm.c
    const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);
    ```
*   **Frequency Calculation**: When this mode is active, the channel's pitch is no longer determined by its tone period register, but by the **frequency of the envelope**. The converter calculates the actual pitch based on the envelope's shape (which determines the number of steps in the waveform, e.g., 32 for sawtooth, 64 for triangle) and the envelope period.
    ```c
    // ay_to_opm.c
    int steps = 0;
    switch (_envelope_shape) {
        case 8: case 11: case 12: case 13: steps = 32; break; // Sawtooth
        case 10: case 14: steps = 64; break; // Triangle
        // ...
    }
    const double freq = (double)_source_clock / (16.0 * _envelope_period * steps);
    _updateFreq(ch, freq); // Update the OPM channel with this new frequency
    ```
*   **Volume & Mixer Correction**: This is the most critical step that makes the feature work. When a channel uses its envelope as a waveform, the composer often **disables the channel's tone switch** (in `R7`). The old logic would have incorrectly interpreted this as "mute". The new `_updateTone` function corrects this:
    ```c
    // ay_to_opm.c
    const int tone_enabled = ((1 << ch) & _regs[7]) == 0;
    const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);

    if (tone_enabled || envelope_as_waveform) {
        // Set volume as long as tone is on OR envelope is used as a waveform
        tVol = 15; // Volume is max when used as a waveform
        _y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf]));
    } else {
        // Mute otherwise
        _y(0x70 + opmCh, 0x7f);
    }
    ```
    This `if (tone_enabled || envelope_as_waveform)` check ensures that even if the tone switch is off, the channel will still produce sound as long as it's using an envelope waveform, thus perfectly reproducing the original chiptune's effect.

#### 4.2.3. AY-8910 to OPM Detailed Conversion Rules
<a id="4-2-3"></a>
Converting sound from an AY-8910 (PSG) to a YM2151 (OPM) is a unique challenge because it requires using FM synthesis to **simulate** a completely different sound generation method (Programmable Sound Generator). YASP uses 3 channels of the OPM to simulate the PSG's 3 square wave channels and a 4th channel to simulate its noise.

##### 4.2.3.1. Register Map Overview
| AY-8910 (PSG) | YM2151 (OPM) | Brief Description |
| :--- | :--- | :--- |
| `R0, R1` (Tone Period A) | `0x28, 0x30` (KC/KF Ch 4) | Frequency for Channel A |
| `R2, R3` (Tone Period B) | `0x29, 0x31` (KC/KF Ch 5) | Frequency for Channel B |
| `R4, R5` (Tone Period C) | `0x2A, 0x32` (KC/KF Ch 6) | Frequency for Channel C |
| `R6` (Noise Period) | `0x0F` (Noise Control) | Noise frequency |
| `R7` (Mixer) | `0x70-0x77` (TL) / `0x08` (Key On/Off) | Controls tone/noise enable and volume |
| `R8` (Amplitude A) | `0x74` (TL Ch 4) | Volume for Channel A |
| `R9` (Amplitude B) | `0x75` (TL Ch 5) | Volume for Channel B |
| `R10` (Amplitude C) | `0x76` (TL Ch 6) | Volume for Channel C |
| `R11, R12` (Envelope Period) | (Internal State) | Frequency of the envelope generator |
| `R13` (Envelope Shape) | (Internal State) / `0x28, 0x30` (KC/KF) | Envelope shape, or pitch when used as a waveform |

##### 4.2.3.2. Detailed Conversion Rules
###### 1. Square Wave Tone
*   **AY `R0-R5` -> OPM `0x28-0x32`**
*   The AY-8910 uses a 12-bit period value to define the frequency of its square wave. The converter first calculates the actual frequency in Hz based on this period.
*   **Frequency Calculation**: `freq = source_clock / (16 * tone_period)`
*   This frequency value is then fed into the same `freqToOPMNote` function used for OPN->OPM conversion, which calculates the OPM's `KC` (Key Code) and `KF` (Key Fraction) values. These are then written to the frequency registers of the corresponding OPM channel.
*   To simulate the timbre of a square wave, the OPM channels are preset with a simple FM configuration (algorithm 4, one carrier and one modulator) that produces harmonics close to those of a square wave.

###### 2. Noise
*   **AY `R6` -> OPM `0x0F`**
*   The AY-8910's 5-bit noise period is directly mapped to the OPM's noise control register. The OPM's noise frequency is inverted (lower value means higher frequency), so an inversion is necessary.
*   **Key Code**: `_y(0x0f, 0x80 | (0x1f - nfreq));`
*   The noise volume is taken from the loudest of the three PSG channels that have their noise switch enabled.

###### 3. Volume & Mixer
*   **AY `R7-R10` -> OPM `0x70-0x77`**
*   This is a core part of the conversion logic. The AY-8910's `R7` mixer register determines whether each channel outputs a tone, noise, both, or is silent.
*   The converter handles this logic in the `_updateTone` function. For each channel, it checks the corresponding tone and noise enable bits in `R7`.
*   If the tone is enabled, the 4-bit volume from `R8-R10` is converted to a 7-bit OPM Total Level (TL) value using a look-up table `VOL_TO_TL`, and this is written to the OPM channel's volume register.
*   If the tone is disabled, the OPM channel is muted (TL set to 127).

###### 4. Exclusive Feature: Envelope as Waveform Conversion
*   **AY `R11-R13` -> OPM `0x28-0x32` (Frequency) & `0x70-0x77` (Volume)**
*   This is an exclusive feature of YASP that solves a common advanced technique in chiptune music. On the AY-8910, it's possible to set an extremely short envelope period, causing the hardware envelope itself to oscillate at high speed and thus be used **as a new sound source**.
*   **Detection Mechanism**: The converter checks the value of the envelope period registers (`R11`, `R12`) to determine if this mode is active. If the period is very small (the threshold is set to `200` in the code), it assumes the envelope is being used as a waveform generator.
    ```c
    // ay_to_opm.c
    const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);
    ```
*   **Frequency Calculation**: When this mode is active, the channel's pitch is no longer determined by its tone period register, but by the **frequency of the envelope**. The converter calculates the actual pitch based on the envelope's shape (which determines the number of steps in the waveform, e.g., 32 for sawtooth, 64 for triangle) and the envelope period.
    ```c
    // ay_to_opm.c
    int steps = 0;
    switch (_envelope_shape) {
        case 8: case 11: case 12: case 13: steps = 32; break; // Sawtooth
        case 10: case 14: steps = 64; break; // Triangle
        // ...
    }
    const double freq = (double)_source_clock / (16.0 * _envelope_period * steps);
    _updateFreq(ch, freq); // Update the OPM channel with this new frequency
    ```
*   **Volume & Mixer Correction**: This is the most critical step that makes the feature work. When a channel uses its envelope as a waveform, the composer often **disables the channel's tone switch** (in `R7`). The old logic would have incorrectly interpreted this as "mute". The new `_updateTone` function corrects this:
    ```c
    // ay_to_opm.c
    const int tone_enabled = ((1 << ch) & _regs[7]) == 0;
    const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);

    if (tone_enabled || envelope_as_waveform) {
        // Set volume as long as tone is on OR envelope is used as a waveform
        tVol = 15; // Volume is max when used as a waveform
        _y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf]));
    } else {
        // Mute otherwise
        _y(0x70 + opmCh, 0x7f);
    }
    ```
    This `if (tone_enabled || envelope_as_waveform)` check ensures that even if the tone switch is off, the channel will still produce sound as long as it's using an envelope waveform, thus perfectly reproducing the original chiptune's effect.

###### 5. Exclusive Feature: ZX-Spectrum Style Stereo Panning
*   **Background**: The original AY-8910 chip is monophonic. However, in the ZX-Spectrum community, developers created various "pseudo-stereo" configurations by using two AY-8910 chips (or their variants). The most classic of these is the **ABC** setup (Channel A on the left, B in the center, C on the right). YASP's AY-8910 to OPM conversion not only supports this classic configuration but also extends it with multiple modes that the user can switch between in real-time to experience different stereo effects.
*   **Implementation**:
    1.  **Defining Panning Constants**: In the OPM (YM2151), panning is controlled by the top two bits (`RL`) of registers `0x20-0x27`. The code defines three constants to represent left, right, and center channels:
        ```c
        #define OPM_PAN_LEFT   0x40 // C2 (binary 01xxxxxx)
        #define OPM_PAN_RIGHT  0x80 // C1 (binary 10xxxxxx)
        #define OPM_PAN_CENTER 0xC0 // C1 & C2 (binary 11xxxxxx)
        ```
    2.  **Providing Multiple Stereo Modes**: YASP defines an enum, `ay_stereo_mode_t`, which includes several popular ZX-Spectrum stereo configurations:
        *   `AY_STEREO_ABC`: Left-Center-Right
        *   `AY_STEREO_ACB`: Left-Right-Center
        *   `AY_STEREO_BAC`: Center-Left-Right
        *   `AY_STEREO_MONO`: All channels centered
    3.  **Dynamic Switching Logic**: The `ay_to_opm_set_stereo_mode` function is the core of the dynamic switching. This function is called when the user switches modes via the keyboard (`Tab` key).
        ```c
        void ay_to_opm_set_stereo_mode(ay_stereo_mode_t mode) {
            _current_stereo_mode = mode;
            // ...
            switch (mode) {
                case AY_STEREO_ABC:
                    ch_pan[0] = OPM_PAN_LEFT; ch_pan[1] = OPM_PAN_CENTER; ch_pan[2] = OPM_PAN_RIGHT;
                    break;
                // ... other modes ...
            }
            // Apply the calculated panning values to OPM channels 4, 5, 6
            for (int i = 0; i < 3; i++) {
                int opmCh = toOpmCh(i);
                _y(0x20 + opmCh, (ch_pan[i] & 0xC0) | 0x3C);
            }
            // Also update the noise panning
            _updateNoise();
        }
        ```
    4.  **Intelligent Noise Panning**: The handling of the noise channel's panning is particularly clever. The `_updateNoise` function checks all three PSG channels. If any of them have noise enabled, it determines the final position of the noise based on that channel's panning setting. For example, if Channel A (left) and Channel C (right) both have noise enabled, the final noise output will be panned to the center to provide a balanced auditory experience.
*   **Result**: This exclusive feature greatly enhances the expressiveness of AY-8910 music. Originally monotonous mono tracks can now exhibit rich spatial depth and layering through different stereo configurations, offering a completely new experience to the listener. Users can switch between modes in real-time to find the stereo effect that best suits the current track.

### 4.3. Advanced Timing and Flush System
<a id="4-3"></a>
YASP's playback precision and stability are determined by the combination of "Flush Mode" and "Timer Mode".

#### 4.3.1. Flush Mode
<a id="4-3-1"></a>
Flush mode determines the **granularity** at which the player sends register data to the SPFM hardware.

| Mode (Key) | Name | Principle |
| :---: | :--- | :--- |
| **1** | Register-Level | `spfm_flush()` is called immediately after **every single** register write. This ensures the highest real-time responsiveness but creates huge USB communication overhead. |
| **2** | **Command-Level** | `spfm_flush()` is only called after a complete VGM command is processed (e.g., a wait command like `0x61 nn nn`, or a chip write command). This is the **best balance between performance and real-time feel** and is the default setting. |

#### 4.3.2. Timer Mode
<a id="4-3-2"></a>
Timer mode determines how the player **waits** precisely for the next event after processing a VGM command. All modes use the high-precision performance counter (`QueryPerformanceCounter`) for error compensation.

| Mode (Key) | Source Code Name | Principle |
| :---: | :--- | :--- |
| **3** | High-Precision Sleep | Calls `Sleep(1)` to wait and compensates for its significant inaccuracy using the performance counter. Suitable for low-load environments. |
| **4** | Hybrid Sleep | Uses `CreateWaitableTimer` to wait for 1ms and compensates for errors. More precise than `Sleep(1)`. |
| **5** | Multimedia Timer | Uses `timeSetEvent` to set up a 1ms high-precision multimedia timer. The playback thread waits for the event triggered by this timer. High precision, but events can be delayed under high load. |
| **6** | **VGMPlay Mode** | A "busy-wait" variant that constantly calls `Sleep(0)` to yield its time slice. Extremely responsive, but causes very high CPU usage. |
| **7** | **Optimized VGMPlay Mode** | An optimized version of Mode 5 with an "anti-runaway" mechanism to prevent audio lag and crashes. **This is the default and best choice.** |

#### 4.3.3. Timer Core Source Code Explained
<a id="4-3-3"></a>
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
<a id="4-4"></a>
YASP communicates with SPFM hardware via the FTDI D2XX driver. Its core is a **write buffering mechanism** and a **hybrid wait strategy**. For extremely short waits (<10 samples), it sends a hardware wait command `0x80`; for longer waits, it calls the high-precision software wait function `yasp_usleep`, which is determined by the selected timer mode.

## 5. Performance Optimization-Version Comparison
<a id="5"></a>
During development, we observed that the current version (before fixes) exhibited a noticeable regression in performance compared to the older `console_player862` version, specifically in the responsiveness of the file browser and the smoothness of music playback. This section provides an in-depth analysis of the root causes and details the optimization measures implemented to address these issues.

### 5.1. The Root Cause: Why Was the Old Version Faster?
<a id="5-1"></a>
After analyzing the `console_player862` source code, we identified two core reasons for the performance degradation:

**1. Synchronous File System Reads Blocking the Main Thread**

*   **Old Version (`862`) Behavior**: The file browser loaded directory contents asynchronously in a separate thread. This meant that even when entering a large directory with thousands of files, the main thread (responsible for UI rendering and keyboard input) **was not blocked**, and the interface remained responsive at all times.
*   **New Version (Before Fixes) Behavior**: The file browser called `scandir` **synchronously** in the main thread. When `scandir` traversed a large directory, it could take hundreds of milliseconds or even seconds, during which the entire program would freeze, unable to respond to any user input or refresh the UI, leading to a severe feeling of lag.

**2. Inefficient UI Refresh Mechanism**

*   **Old Version (`862`) Behavior**: UI refreshes were **event-driven**. The `update_ui()` function was only called to redraw the interface when the playback state changed (e.g., switching songs, pausing) or when there was user input. This approach is extremely efficient.
*   **New Version (Before Fixes) Behavior**: The UI refresh was placed in a **fixed, high-frequency loop**. Regardless of whether the state had changed, the main loop would call `update_ui()` at a very high frequency (every few milliseconds), causing a large number of redundant calculations and screen redraws. This not only wasted CPU resources but could also conflict with the playback thread's timer, affecting playback stability.

### 5.2. The Fixes: How We Restored and Surpassed the Old Performance
<a id="5-2"></a>
To address these issues, we implemented the following key optimizations:

**1. Introduced Asynchronous File List Loading (`browser.c`)**

We refactored the file browser to restore and improve the asynchronous loading mechanism.

```c
// In browser.c
static bool g_loading_thread_active = false;
static HANDLE g_loading_thread_handle = NULL;

// When a user enters a directory, instead of scanning directly, create a new thread
void refresh_file_list(const char* path) {
    // ...
    g_loading_thread_handle = CreateThread(NULL, 0, loading_thread_func, new_path, 0, NULL);
    // ...
}

// The new loading thread is responsible for the time-consuming scandir operation
static DWORD WINAPI loading_thread_func(LPVOID lpParam) {
    g_loading_thread_active = true;
    // ...
    int n = scandir(path, &g_namelist, 0, alphasort); // The heavy lifting happens here
    // ...
    g_loading_thread_active = false;
    return 0;
}
```
**Explanation**: When the user selects a directory, the main function `refresh_file_list` no longer executes `scandir` itself. Instead, it returns immediately and delegates this time-consuming operation to a newly created `loading_thread_func` thread. During the loading period, the `g_loading_thread_active` flag is set to `true`, allowing the UI to display a "Loading..." message, while the main thread remains fluid and responsive to user actions.

**2. Implemented Event-Driven UI Refreshes (`main.c`, `play.c`)**

We completely removed the inefficient, high-frequency UI redraw calls from the main loop and introduced a global flag, `g_ui_refresh_request`.

```c
// In the main loop of main.c
int main() {
    // ...
    while (!g_quit_flag) {
        if (g_ui_refresh_request) {
            update_ui(...);
            g_ui_refresh_request = false; // Clear the flag immediately after redrawing
        }
        // ... Handle keyboard input ...
        yasp_usleep(16000); // The main loop can now sleep peacefully
    }
    // ...
}
```
**Explanation**: `update_ui()` is now only called when `g_ui_refresh_request` is `true`. This flag is set only at moments when the UI genuinely needs to be updated, such as:
*   When a song starts playing (`play_file`)
*   When the user pauses/resumes or changes modes (in the keyboard handling section of `main.c`)
*   When the file browser finishes loading (`browser.c`)

This **event-driven** model reduces the UI refresh frequency from hundreds of times per second to only a few necessary updates, significantly lowering CPU usage and fundamentally resolving potential conflicts between UI refreshes and the playback timer.

With these two core optimizations, YASP has not only restored the smooth experience of the older `862` version but has also achieved further improvements in code structure and maintainability.

## 6. User Guide
<a id="6"></a>
YASP is operated entirely via the keyboard. The interface is divided into the **Main Player Interface** and the **File Browser**.

### 6.1. Main Player Interface
The main interface displays real-time information about the currently playing music, player status, and hardware configuration.

#### 6.1.1. Interface Layout (ASCII Diagram)
```
+------------------------------------------------------------------------------+
| YASP - Yet Another Sound Player                               (Line 0)       |
| --------------------------------------------------            (Line 1)       |
| Track: [Current Track Name]                                   (Line 2)       |
| Game: [Game Title]                                            (Line 3)       |
| System: [Game Platform]                                       (Line 4)       |
| Author: [Composer]                                            (Line 5)       |
| Date: [Release Date]                                          (Line 6)       |
| VGM By: [VGM File Creator]                                    (Line 7)       |
| VGM Chip: [Chip Type] ([Clock Speed]MHz)                      (Line 8)       |
| Status: [Playing.../Paused]                                   (Line 9)       |
| Total Time: [Total Duration] | Loops: [Loop Count]            (Line 10)      |
| Flush Mode (1,2): [Current Flush Mode]                        (Line 11)      |
| Timer (3-7): [Current Timer Mode]                             (Line 12)      |
| Mode: [Playback Mode] | Speed: [Playback Speed]x              (Line 13)      |
| OPN LFO Amp: [LFO Amplitude]                                  (Line 14)      |
| Slot 0: [Chip 0] | Slot 1: [Chip 1]                           (Line 15)      |
| Conversion: [Conversion Status]                               (Line 16)      |
| --------------------------------------------------            (Line 17)      |
| [Key Legend]                                                  (Line 18)      |
+------------------------------------------------------------------------------+
```

#### 6.1.2. Interface Description
*   **Lines 2-7**: Display metadata read from the VGM file's GD3 tag. If the tag is absent, the filename is shown.
*   **Line 8 (VGM Chip)**: Shows the sound chip and its clock speed as originally specified in the VGM file.
*   **Line 9 (Status)**: Indicates whether playback is active or paused.
*   **Line 10 (Total Time / Loops)**: Shows the calculated total playback duration (including loops) and the user-defined loop count.
*   **Line 11 (Flush Mode)**: The strategy for sending data to the SPFM hardware (see [4.3.1](#4-3-1)).
*   **Line 12 (Timer)**: The waiting strategy used by the player (see [4.3.2](#4-3-2)).
*   **Line 13 (Mode / Speed)**: Shows whether playback is sequential or random, and the playback speed multiplier.
*   **Line 14 (OPN LFO Amp)**: Only displayed during OPN->OPM conversion; adjusts the intensity of the LFO effect.
*   **Line 15 (Slot 0 / Slot 1)**: Shows the chip types actually installed in the two slots of the SPFM hardware.
*   **Line 16 (Conversion)**: Indicates the current playback method (direct play, real-time conversion, or from cache).

### 6.2. File Browser
Press `f` on the main screen to enter the file browser.

```
+------------------------------------------------------------------------------+
| Path: [Current Path]                                                         |
| --------------------------------------------------                           |
| > [.]                                                                        |
|   [..]                                                                       |
|   [Directory1]/                                                              |
|   [File1.vgm]                                                                |
|   [File2.s98]                                                                |
|   ...                                                                        |
+------------------------------------------------------------------------------+
```
*   Use the `Up/Down` arrow keys to select a file or directory.
*   Press `Enter` to play the selected file or enter the selected directory.
*   Press `f` at any time to close the browser and return to the main player interface.

### 6.3. Key Bindings
| Key | Function | Remarks |
| :--: | :-- | :-- |
| `q` | **Quit Program** | |
| `p` | **Pause / Resume** | Toggles the playback state. |
| `n` | **Next Track** | Immediately skips to the next song in the playlist. |
| `b` | **Previous Track** | Immediately skips to the previous song in the playlist. |
| `r` | **Toggle Playback Mode** | Switches between "Sequential" and "Random" playback. |
| `f` | **Toggle File Browser** | |
| `+` / `-` | **Adjust Playback Speed** | Increases or decreases the speed multiplier in steps of 0.05. |
| `Up/Down` | **Adjust OPN LFO Amplitude** | Only active during OPN->OPM conversion; enhances or reduces the LFO effect in real-time. |
| `Left/Right` | **Adjust Loop Count** | Decreases or increases the number of times the song will loop. |
| `1` / `2` | **Switch Flush Mode** | Toggles between "Register-Level" and "Command-Level" flushing. |
| `3` - `7` | **Switch Timer Mode** | Switches between different timing strategies to adapt to various system loads. |

## 7. Compilation and Build
<a id="7"></a>
The YASP project is built using `make` and depends on the `MinGW-w64` compilation environment and FTDI's official `D2XX` driver library.

### 7.1. Environment Requirements
*   **Compiler**: `MinGW-w64` (the `x86_64-w64-mingw32` version is recommended for 64-bit compilation).
*   **Build Tool**: `make` (usually installed with MinGW-w64).
*   **Dependencies**:
    *   `ftd2xx.h`: The FTDI D2XX driver header file, which must be placed in the `console_player` directory.
    *   `ftd2xx64.dll`: The 64-bit FTDI D2XX dynamic link library, which must be placed in the `console_player` directory.

### 7.2. Makefile Explained
The `console_player/makefile` in the project's root directory defines all the build rules.

*   **`CC`**: Defines the C compiler, defaulting to `gcc`.
*   **`CFLAGS`**: Compilation flags.
    *   `-m64`: Generate 64-bit code.
    *   `-Wall`, `-Wextra`: Enable all standard and extra warnings to improve code quality.
    *   `-g`: Generate debugging information, useful for tools like GDB.
    *   `-O0`: Disable all optimizations, ensuring that the code's behavior exactly matches the source during debugging.
    *   `-I"../ftdi_driver"`: Add the `ftdi_driver` directory to the header search path to find `ftd2xx.h`.
*   **`LDFLAGS`**: Linker flags, passed to the compiler during the linking phase.
    *   `-m64`: Create a 64-bit executable.
    *   `-s`: Strip the symbol table and relocation information from the final executable to reduce its size.
    *   `-L"../ftdi_driver/amd64"`: Add the `ftdi_driver/amd64` directory to the library search path so the linker can find `libftd2xx.a`.
*   **`LIBS`**: The libraries to link against.
    *   `-lftd2xx`: Links the FTDI D2XX driver library, used for communicating with the SPFM hardware.
    *   `-lm`: Links the math library (`libm`), which provides functions like `log2` used in frequency conversion.
    *   `-lwinmm`: (Windows only) Links the Windows Multimedia library (`winmm.lib`), providing high-precision timer functions like `timeSetEvent`.
    *   `-lavrt`: (Windows only) Links the Multimedia Class Scheduler Service library (`avrt.lib`), allowing the playback thread's priority to be boosted to "Pro Audio" level to reduce audio jitter.
    *   `-lole32`: (Windows only) Links the OLE32 library, which provides COM services and is a dependency for the `avrt` library.
    *   `-lpthread`: (POSIX systems like Linux only) Links the POSIX Threads library, used for creating and managing threads.
*   **`SOURCES`**: Defines all the `.c` source files to be compiled.
*   **`OBJECTS`**: Automatically generates a list of corresponding `.o` object files from `SOURCES`.
*   **`TARGET`**: Defines the name of the final executable, defaulting to `yasp_test.exe`.

### 7.3. Build and Run
1.  **Execute Build**: Open a terminal and run the `make` command from the **project root** (`yasp`), using the `-C` flag to specify the directory containing the `makefile`.
    ```bash
    make -C console_player
    ```
    `make` will automatically compile all source files according to the rules in `console_player/makefile` and produce the `yasp_test.exe` executable in the `console_player` directory.

2.  **Run the Program**:
    ```bash
    ./console_player/yasp_test.exe
    ```

3.  **Clean Build Files**: If you need to recompile all files, you can run `make clean`.
    ```bash
    make -C console_player clean
    ```
    This command will delete all generated `.o` object files and the `yasp_test.exe` executable from the `console_player` directory.

---

## 8. Troubleshooting and Changelog
<a id="8"></a>
### 8.1. Troubleshooting: Uncontrolled, Rapid Skipping of Tracks
<a id="8-1"></a>
While developing the chip conversion and caching features, we encountered a very tricky bug: when playing a VGM file that required real-time conversion (e.g., OPN -> OPM), the program would immediately end the current track and "auto-skip" to the next one in the playlist, making it impossible to listen to any song that needed conversion.

#### 8.1.1. Diagnostic Process
<a id="8-1-1"></a>
After multiple, detailed debugging sessions with the project owner, **Denjhang**, we eliminated several possibilities and ultimately pinpointed the problem to the cache file creation process.

1.  **Initial Suspicion: File Pointer Confusion**
    *   Initially, we suspected that the read and write operations were using the same `FILE*` pointer when creating the cache file, leading to a corrupted file stream state.
    *   **Fix Attempt**: We introduced a strategy to "first read the entire VGM file into memory, then perform conversion and writing from memory." While this architecturally resolved the risk of pointer misuse and successfully preserved GD3 tags, the "auto-skipping" issue persisted.

2.  **Deeper Analysis: The Silent Failure of `fopen`**
    *   In the code's logic, the program attempts to open a new cache file in write mode (`"wb"`). However, we overlooked a critical behavior of the C standard library: if `fopen` tries to create a file in a **non-existent directory**, it will **fail silently and return `NULL`** without automatically creating the directory.
    *   **The Critical Clue**: The project owner, **Denjhang**, astutely observed: "**I noticed that no cache files were being generated.**" This clue was the decisive turning point that led to solving the entire problem. It directly proved that the `fopen(cache_filename, "wb")` call was failing.

3.  **Final Root Cause Identification**
    *   When `fopen` failed, the subsequent code logic did not perform adequate `NULL` checks, causing the program to proceed with an invalid, `NULL` file handle for subsequent playback operations.
    *   When the playback thread (`vgm_player_thread`) attempted to read data from this `NULL` file handle, `fread` or other file operations would immediately return an error (usually 0 or -1). This instantly triggered the playback loop's exit condition, `g_is_playing = false`.
    *   The main playback loop detected that `g_is_playing` had become `false`, assumed the current song had "finished playing," and naturally switched to the next track. This perfectly explained the phenomenon of "uncontrolled, rapid skipping of tracks."

#### 8.1.2. The Final Solution
<a id="8-1-2"></a>
The solution was very direct: **forcefully ensure the `cache` directory exists** before calling `fopen` to create a cache file.

We added platform-specific directory creation code to the `vgm_play` function in `vgm.c`:

```c
// In vgm.c's vgm_play function, before attempting to create a cache file
// --- CACHE DOES NOT EXIST, CREATE IT ---
logging(INFO, "Cache not found. Converting %s to OPM...", chip_type_to_string(g_vgm_chip_type));

#ifdef _WIN32
    _mkdir("console_player/cache");
#else
    mkdir("console_player/cache", 0755);
#endif

// Then, safely call fopen
g_cache_fp = fopen(cache_filename, "wb");
if (!g_cache_fp) {
    // ... error handling ...
}
```
**Explanation**: This code uses `_mkdir` (Windows) or `mkdir` (Linux/macOS) to attempt to create the `console_player/cache` directory. If the directory already exists, this call is safely ignored; if it doesn't, it is created. This ensures that the subsequent `fopen` call always executes in a valid path, fundamentally solving the file creation failure.

This case study profoundly illustrates the importance of **validating preconditions (such as directory existence)** when performing low-level file I/O operations.

### 8.2. Bug Fixes & Improvements (v0.881)
<a id="8-2"></a>
This update addresses several critical issues and improves core functionality:

#### 8.2.1. Fixed Loop Logic for Converted VGM Files
<a id="8-2-1"></a>
*   **Problem**: Converted cache VGM files (e.g., YM2612 to YM2151) did not correctly inherit the loop point from the original file, forcing them to loop from the beginning.
*   **Fix**:
    1.  Introduced `g_original_loop_offset` and `g_converted_loop_offset` global variables in `vgm.c` to track loop points during conversion.
    2.  Modified `vgm_convert_and_cache_from_mem` to check if the current file pointer reaches the original loop offset.
    3.  When the point is reached, the current write position in the **cache file** is recorded to `g_converted_loop_offset`.
    4.  Finally, `vgm_play` uses this newly calculated offset to write the correct loop start point back into the cache file's header, ensuring accurate looping.

#### 8.2.2. Fixed Auto-Skip Issue After Playback Completion
<a id="8-2-2"></a>
*   **Problem**: In some cases (especially after using "Next" or "Previous" during playback), a song would not automatically advance to the next track in the playlist after finishing all its loops.
*   **Fix**:
    1.  The issue was located in the `player_thread_func` in `main.c`.
    2.  The original logic would incorrectly set `g_is_playing` to `false` and break the playback loop if user-initiated track change flags were set when a song ended.
    3.  The logic was revised. Now, if a song finishes naturally, it correctly gets the next song index and continues playing. If it was interrupted by the user, the main loop handles the track change, preventing the thread from exiting prematurely.

#### 8.2.3. Corrected Total Time Calculation in UI
<a id="8-2-3"></a>
*   **Problem**: The "Total Time" displayed in the UI did not account for the number of loops, resulting in an inaccurate duration.
*   **Fix**: The calculation in `play.c`'s `update_ui` function was changed to: `Total Samples + (Loop Samples * (Loop Count - 1))`. This ensures the displayed time accurately reflects the full playback duration, including all loops.

#### 8.2.4. Corrected VGM Chip Info Display in UI
<a id="8-2-4"></a>
*   **Problem**: When playing a converted cache file, the "VGM Chip" field showed the target chip (e.g., YM2151) instead of the original chip from the source VGM file (e.g., YM2612).
*   **Fix**:
    1.  `g_original_vgm_chip_type` is now used in `vgm.c` to specifically store the original file's chip type during header parsing.
    2.  The `update_ui` function in `play.c` now checks the `g_is_playing_from_cache` flag. If playing from cache, it forces the display to use `g_original_vgm_chip_type` and the original header info for the chip name and clock speed.

#### 8.2.5. Added Detection for Invalid VGM Files
<a id="8-2-5"></a>
*   **Problem**: For incomplete or corrupted VGM files (e.g., where all chip clocks are 0), the player would still attempt to guess a clock frequency and play, potentially leading to incorrect behavior.
*   **Fix**: A check was added to `play.c`'s `update_ui` function. Before displaying chip info, it now verifies if all known chip clocks in the VGM header are zero. If so, it displays "VGM Chip: Invalid/Unsupported" to clearly indicate a potential file issue to the user.

### 8.3. Feature Enhancements & Bug Fixes (v0.884)
<a id="8-3"></a>
This update focuses on improving user experience, startup logic, and the robustness of file management.

#### 8.3.1. Optimized Startup and Directory Management
<a id="8-3-1"></a>
*   **Automatic Directory Creation**: The program now checks for the existence of the `music` and `cache` directories upon startup. If they don't exist, they are created automatically, preventing potential errors caused by missing directories.
*   **Intelligent Music Scanning**:
    *   The program first checks if the `last_file` entry in the configuration file (`config.ini`) is valid. If the file does not exist, the entry is considered invalid.
    *   If `last_file` is invalid, the program prioritizes scanning the `music` directory.
    *   If the `music` directory is empty, the program falls back to scanning its current working directory to ensure playable music can be found.
*   **Direct to Browser on No Music**: If no supported music files are found in any of the default paths, the program no longer exits. Instead, it automatically enters the file browser mode, allowing the user to locate music files manually.

#### 8.3.2. Enhanced File Browser Functionality
<a id="8-3-2"></a>
*   **Context-Aware Startup**: When entering the file browser from the main player interface by pressing `f`, the browser now automatically navigates to the directory of the currently playing song, rather than always starting from the program's main directory. This significantly improves ease of use.
*   **Unified Path Separators**: All internal path handling in the browser now consistently uses forward slashes (`/`), resolving potential path parsing issues on the Windows platform caused by mixed use of backslashes (`\`) and forward slashes (`/`).
*   **Fixed Parent Directory Navigation**: Fixed an issue where selecting `..` in the file system's root directory (e.g., `C:/`) would incorrectly jump to the drive list. Now, navigating up from the root directory is correctly ignored.

#### 8.3.3. Added Clear Cache Functionality
<a id="8-3-3"></a>
*   After the chip selection process, the program now prompts the user whether to clear the conversion cache. The user can press `y` to delete all files in the `cache` directory, which is useful for resolving potential cache corruption issues or freeing up disk space.

### 8.4. AY-8910 Envelope Waveform Conversion Fix (v0.888)
<a id="8-4"></a>
This update resolves a critical, long-standing bug in the AY-8910 (PSG) to YM2151 (OPM) conversion, enabling the correct playback of certain types of AY-8910 music, especially chiptunes that use the envelope as a waveform generator.

*   **Problem**: In some AY-8910 music, composers utilize the high-speed repetition of the hardware envelope to generate a special "waveform" instead of using it to control volume. In this mode, the tone output switch for the channel is often disabled. The old conversion logic incorrectly interpreted "tone off" as "channel muted," causing these channels to be completely silent after conversion.
*   **Fix Process**:
    1.  **Pinpointing the Core Error**: Through iterative debugging and analysis with the project owner, **Denjhang**, the root cause was finally identified in the `_updateTone` function within `ay_to_opm.c`. The function only calculated volume if the tone switch was detected as on, completely ignoring the special case of "envelope-only output."
    2.  **Refactoring the Muting Logic**: The `_updateTone` function was refactored. The new logic is: a channel is considered active and should have its volume calculated if **either the tone switch is on OR it is in the mode of generating a waveform with its envelope** (i.e., envelope mode is on and the envelope period is extremely short).
    3.  **Code Implementation**:
        ```c
        // ay_to_opm.c
        static void _updateTone(int ch) {
            const int v = _regs[8 + ch];
            const int tone_enabled = ((1 << ch) & _regs[7]) == 0;
            const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);

            int opmCh = toOpmCh(ch);

            if (tone_enabled || envelope_as_waveform) {
                // ... calculate and set volume ...
            } else {
                // Mute only if neither condition is met
                _y(0x70 + opmCh, 0x7f);
            }
        }
        ```
*   **Result**: With this fix, AY-8910 bass and other special effects that were previously silent after conversion are now played back correctly and audibly, significantly improving compatibility with special chiptune tracks.

### 8.5. AY-8910 Fast Arpeggio Conversion Fix (v0.903)
<a id="8-5"></a>
This update addresses a severe bug in the AY-8910 (PSG) to YM2151 (OPM) conversion related to fast note sequences (arpeggios).

*   **Problem**: When playing AY-8910 music containing fast arpeggios, the converted OPM channels would drop notes and go silent after about 10 seconds of playback. In-depth analysis with the `vgm_parser` tool revealed that the issue was in the pitch and Key On/Off recognition mechanism in `ay_to_opm.c`. The old logic failed to correctly re-trigger notes amidst rapid pitch changes and channel toggles, leading to sound interruption.
*   **Fix Process**:
    1.  **Refactored Mixer (R7) Logic**: The section in `ay_to_opm_write_reg` that handles AY-8910 register 7 (the mixer) was completely refactored. The code can now precisely detect the "tone enable" bit for each channel changing from 1 to 0, which is treated as an explicit **Key-On** event.
    2.  **Precise Key-On/Key-Off Handling**:
        *   Upon detecting a **Key-On** event, the code immediately recalculates the frequency (`_recalculate_freq`), updates the volume (`_updateTone`), and sends a forced "Key On all slots" command (`_y(0x08, (0xf << 3) | opmCh)`) to the OPM for that channel.
        *   Simultaneously, if the channel uses a one-shot envelope, the envelope state is reset, ensuring it starts from the beginning with each note trigger.
        *   Upon detecting a **Key-Off** event (tone enable bit changing from 0 to 1), a "Key Off all slots" command (`_y(0x08, opmCh)`) is sent to the OPM.
    3.  **Simplified Amplitude (R8-R10) Logic**: The complex logic in the amplitude register handling, which previously tried to guess Key-On/Off events, was removed. Note on/off is now controlled precisely and reliably by the mixer (R7) logic, making the code cleaner and more robust.
*   **Result**: With this fix, fast arpeggios in converted AY-8910 tracks are now played back completely and clearly without dropping notes, significantly improving compatibility with and fidelity of complex chiptune tracks.
