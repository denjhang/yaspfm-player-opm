# YASP - Yet Another Sound Player OPM v0.86

## Abstract

YASP (Yet Another Sound Player) is a command-line music player designed for the SPFM (Sound-Processing FM-synthesis) family of hardware. It focuses on efficiently and accurately playing classic VGM and S98 format music files, providing users with the ultimate retro audio experience through various configurable refresh modes and timer options.

---

## Table of Contents

1.  [Project Background](#project-background)
2.  [Features](#features)
3.  [Controls](#controls)
4.  [Build Instructions](#build-instructions)
5.  [Source Code References](#source-code-references)

---

## Body

### Project Background

This project aims to create a powerful and highly customizable command-line music player for SPFM hardware enthusiasts. The development was a collaborative effort between the following members:

*   **Denjhang (Project Owner/Lead Developer):** Responsible for defining project requirements, core architecture design, providing key technical implementations (such as high-precision timer example code), and conducting final testing and validation.
*   **Cline (AI Software Engineer):** Responsible for implementing specific code based on requirements, feature integration, bug fixing, UI adjustments, and documentation writing.

### Features

*   **Broad Chip Support:** Supports a wide range of classic FM synthesis chips, including YM2608, YM2151, YM2612, YM2203, YM2413, and more.
*   **Multi-Format Playback:** Supports `.vgm`, `.vgz`, and `.s98` music file formats.
*   **Advanced Refresh Modes:**
    *   **Flush Mode:** Provides two data flush modes to control how frequently commands are sent to the SPFM hardware.
    *   **Timer Mode:** Offers multiple high-precision timing strategies for VGM playback to achieve the most accurate performance under various system loads.
*   **Built-in File Browser:**
    *   Press `F` to open the file browser, allowing you to easily switch between directories and select music files.
*   **Intuitive Player Interface:**
    *   Clearly displays the currently playing song, total duration, chip configuration, and various mode statuses.
    ```
    YASP - Yet Another Sound Player
    --------------------------------------------------
    Now Playing  : IF09_.vgm
    Total Time   : 01:28
    Slot 0: YM2608 | Slot 1: YM2151
    Flush Mode (1,2): Register-Level
    Timer Mode (3-0): VGMPlay-Optimized 1
    --------------------------------------------------
    [N] Next | [B] Prev | [P] Pause | [R] Random | [F] Browser | [+] Speed Up | [-] Speed Down | [Q] Quit
    --------------------------------------------------
    Status: Playing... | Random: Off | Speed: 1.00x
    ```
*   **Easy-to-Use File Browser:**
    *   Features pagination and clear navigation to let you browse your music library with ease.
    ```
    File Browser
    --------------------------------------------------
    Current Dir: ./console_player/music/if-vgm
    Now Playing: ./console_player/music/if-vgm/IF14_.vgm
    --------------------------------------------------
    -> ../
       IF01_.vgm
       IF02_.vgm
       IF03_.vgm
       IF04_.vgm
       IF05_.vgm
       IF06_.vgm
       IF07_.vgm
       IF08_.vgm
       IF09_.vgm
    --------------------------------------------------
    Page 1/2 | [Up/Down] Navigate | [Left/Right] Page | [Enter] Select | [F] Exit
    ```
*   **Automatic Configuration Saving:**
    *   Settings such as chip selection, playback speed, and refresh modes are automatically saved to `config.ini` and loaded on the next launch.
*   **Real-time Playback Control:** Supports next/previous track, pause/resume, random mode, and fine-tuning of playback speed during playback.
*   **Cross-Platform Design:** Primarily designed for Windows, but the code structure is also compatible with POSIX-like systems (e.g., Linux).

### Flush Modes Explained

The flush mode determines how frequently YASP sends commands to the SPFM hardware.

| Key | Mode | How It Works | Pros & Cons |
| :---: | :-- | :-- | :-- |
| `1` | **Register-Level** | Commands are continuously written to an internal buffer (`spfm_write_buf`). The `spfm_flush` function is only called to send the entire buffer over USB when the buffer is full or when a precise timing wait is encountered. | **Pro:** Maximizes throughput. By merging multiple commands into a single USB transfer, it significantly reduces driver and hardware overhead, resulting in the highest performance.<br>**Con:** Command transmission is not real-time, which may introduce microscopic latency. |
| `2` | **Command-Level** | **(Default)** The `spfm_flush` function is called immediately after **every** command from the VGM file (like a register write or a short wait) is processed. | **Pro:** Provides the most immediate response, ensuring each command is sent to the hardware with minimal delay.<br>**Con:** USB transfers are very frequent, leading to higher driver overhead and theoretically lower performance than Register-Level mode. |

### Timer Modes Explained (for VGM Playback)

YASP provides several advanced timer modes for VGM playback. The core idea is to **no longer trust** the operating system's `sleep` function, but instead to use the high-precision performance counter (`QueryPerformanceCounter`) to **compensate for** or **drive** the playback loop.

*Note: These modes only affect `.vgm` and `.vgz` files. `.s98` files use their own hardware-based synchronization mechanism (the `0xFF` command) and are not affected by these settings.*

#### A. Compensated Sleep Modes
These modes share a common strategy: in a main loop, they first calculate the "time debt" (number of samples to process) based on real elapsed time, then "pay off" the debt by processing samples, and finally call a `sleep` function to yield the CPU. Even if the `sleep` is inaccurate, the next loop automatically compensates for the error.

| Key | Mode | `g_timer_mode` | `sleep` Implementation | Recommended Use Case |
| :---: | :-- | :---: | :-- | :-- |
| `3` | **H-Prec Compensated (Default)** | 0 | Uses `CreateWaitableTimer` (a high-precision kernel timer) for the `sleep`. | **General Recommendation**. Low CPU usage, high precision. The best balance of stability and performance. |
| `4` | **Hybrid Compensated** | 1 | Uses a hybrid of `Sleep()` and busy-waiting (spin-wait) for the `sleep`. | An alternative option. May be more responsive on some systems than Mode 3, but with slightly higher CPU usage. |
| `5` | **MM-Timer Compensated** | 2 | Uses `timeSetEvent` (a multimedia timer) for the `sleep`. | An alternative option. Theoretically very precise, but the timer callback introduces some extra overhead. |

#### B. Event-Driven Modes
These modes do not rely on a `sleep` function. Instead, the playback loop is "awakened" by an external timer.

| Key | Mode | `g_timer_mode` | How It Works | Recommended Use Case |
| :---: | :-- | :---: | :-- | :-- |
| `6` | **Classic VGMPlay** | 3 | **(Highly Recommended)** The main loop is awakened by a 1ms high-precision multimedia timer (`timeSetEvent`). After each wakeup, it processes the appropriate number of samples based on real elapsed time. Because its pace is controlled by an external timer, it is theoretically more precise than compensated modes. | Very stable in most conditions and the gold standard for accurate playback. |
| `7` | **Optimized VGMPlay** | 7 | **(Most Stable)** This is the **"anti-runaway"** version of Mode 6. It adds a "max samples per frame" limit (equivalent to 1/60th of a second) to the classic VGMPlay mode. This prevents the audio from suddenly "fast-forwarding" or stuttering after a system freeze. | **Highest Load Scenarios**. Provides the smoothest experience when running CPU-intensive tasks in the background. |

### SPFM Communication Protocol Explained

YASP communicates with the SPFM hardware via the FTDI D2XX driver. The core of the protocol is designed to efficiently send register writes and wait commands to the device.

#### Device Identification and Handshake

During initialization, YASP attempts to identify the type of SPFM device.

1.  **Handshake Signal**: The program sends an `0xFF` byte to the device.
2.  **Device Response**:
    *   **SPFM Light** devices respond with the string "LT".
    *   **SPFM Standard** devices respond with the string "OK".
3.  **Compatibility Mode**: According to comments in the code, the current version of YASP **bypasses** this handshake process and defaults to identifying the device as `SPFM_TYPE_SPFM_LIGHT`. This is done to resolve compatibility issues on specific hardware where a strict handshake might fail, thus improving device compatibility.

#### Command Format

The command format varies depending on the device type:

*   **SPFM Light (4 bytes):**
    *   `{ slot, (port << 1), addr, data }`
    *   `slot`: The chip's slot (0 or 1).
    *   `port`: The chip's port.
    *   `addr`: The register address to write to.
    *   `data`: The data to be written.

*   **SPFM Standard (3 bytes):**
    *   `{ (slot << 4) | port, addr, data }`
    *   `slot`: The chip's slot (0-7).
    *   `port`: The chip's port (0-3).
    *   `addr`: The register address to write to.
    *   `data`: The data to be written.

*Note: The current version of YASP defaults to SPFM Light mode for compatibility reasons.*

#### Write Buffering Mechanism

To maximize throughput and reduce the overhead of FTDI driver calls, YASP implements a write buffer (`spfm_write_buf`).

1.  When functions like `spfm_write_reg` are called, the command data is **not** sent immediately. Instead, it is staged in this buffer.
2.  The `spfm_flush` function is only called when the buffer is full or when a precise timing operation (like a sleep) is required.
3.  `spfm_flush` sends the entire buffer's content in a single `FT_Write` call to the SPFM device, significantly improving efficiency.

#### Hardware vs. Software Waits

YASP employs a hybrid waiting strategy to balance precision and CPU usage:

*   **Hardware Wait:**
    *   When the device is `SPFM_Light` and the wait duration is extremely short (less than 10 audio samples), YASP sends an `0x80` command.
    *   This command is handled by the SPFM hardware itself, enabling a highly precise, near-zero CPU-cost wait.
*   **Software Wait:**
    *   For longer waits, or on devices that do not support hardware waits, YASP calls the `yasp_usleep` function.
    *   This function executes a high-precision software wait based on the user-selected timer mode (e.g., High-Precision Sleep, Multimedia Timer).

This design makes YASP highly efficient at processing the numerous short wait commands found in VGM/S98 files, while also yielding the CPU during longer waits to avoid wasting resources.

### Controls

#### Player Interface

| Key | Function |
| :---: | :-- |
| `q` | Quit the player |
| `p` | Pause/Resume playback |
| `n` | Next track |
| `b` | Previous track |
| `r` | Toggle random mode |
| `f` | Open file browser |
| `+` | Increase playback speed (by 0.01) |
| `-` | Decrease playback speed (by 0.01) |
| `1` | Switch to `Register-Level` flush mode |
| `2` | Switch to `Command-Level` flush mode (Default) |
| `3`-`7` | Switch between different VGM timer modes (see previous section) |

#### File Browser Interface

| Key | Function |
| :---: | :-- |
| `Up` / `Down` Arrow | Navigate up/down |
| `Left` / `Right` Arrow | Page up/down |
| `Enter` | Enter a directory or select a file to play |
| `Backspace` | Go to the parent directory |
| `f` | Exit the browser and return to the player |

### Build Instructions

#### Windows (using MinGW/MSYS2)

1.  Ensure you have MinGW-w64 or MSYS2 installed and its `bin` directory added to your system PATH.
2.  Place the FTDI D2XX driver files (`ftd2xx.h`, `ftd2xx.lib` or `libftd2xx.a`) in the `ftdi_driver` folder in the project root.
3.  Open a command line, navigate to the `console_player` directory.
4.  Run the `make` command to compile:
    ```bash
    make -C console_player
    ```
5.  Upon successful compilation, `yasp_test.exe` will be generated in the `console_player` directory.

#### Linux / macOS

The build process is similar to Windows, but you need to ensure the `libftdi` development library is installed. The `makefile` will automatically handle platform-specific linking options.

### Source Code References

This C language version of the player heavily referenced the following projects:

*   **`node-spfm`** (developed by Denjhang):
    *   **Location**: `D:\working\vscode-projects\yasp11\node-spfm-denjhang-main`
    *   **Contribution**: A full-featured Node.js implementation that provided the core logic and algorithmic foundation for this C port, especially in the areas of SPFM communication protocol, VGM/S98 file parsing, and chip control logic.

*   **VGMPlay** (developed by Valley Bell):
    *   **Contribution**: In the project's early stages, the traditional `sleep`-based timing method caused unstable playback speeds during high CPU load operations like window switching or file decompression. To solve this, the project adopted and implemented VGMPlay's core timing model. This model does not rely on fixed `sleep` intervals but instead dynamically calculates the number of audio samples to process based on the real elapsed time (`QueryPerformanceCounter`), fundamentally ensuring stable and accurate playback under various system loads.

#### Key Referenced Areas:

*   **SPFM Communication Protocol:**
    The logic for handshaking, sending commands, and data to the SPFM hardware, as defined in `node-spfm/src/spfm.ts`, served as the foundation for the implementation in `console_player/spfm.c`. This includes key operations like device initialization, chip reset, and register writing.
*   **VGM File Parsing:**
    The logic for parsing VGM file headers, processing data blocks, and interpreting commands from `node-spfm/src/vgm.ts` was directly adapted and re-implemented in C in `console_player/vgm.c`. This ensured accurate support for the VGM format.
*   **Chip Control Logic:**
    The control classes for different FM chips (e.g., YM2151, YM2608) in the `node-spfm/src/chips/` directory provided accurate references for register addresses and control commands in our project's files like `console_player/ym2151.c` and `console_player/ym2608.c`.

In addition to `node-spfm`, the project involved modifications and implementations in the following files:

*   `console_player/main.c`: The main program entry point, responsible for initialization, configuration loading, the playback loop, and keyboard input handling.
*   `console_player/play.c`: Core of the player's UI display and file playback logic.
*   `console_player/util.c` & `console_player/util.h`: Implementation of various timer modes and utility functions.
*   `console_player/makefile`: The project's build script, responsible for compiling and linking all source files.
