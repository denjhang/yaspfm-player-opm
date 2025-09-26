# YASP - Yet Another Sound Player

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

*   **Denjhang (Project Owner/Lead Developer):** Responsible for defining project requirements, core architecture design, providing key technical implementations (such as the MMCSS + IAudioClient3 example code), and conducting final testing and validation.
*   **Cline (AI Software Engineer):** Responsible for implementing specific code based on requirements, feature integration, bug fixing, UI adjustments, and documentation writing.

### Features

*   **Broad Chip Support:** Supports a wide range of classic FM synthesis chips, including YM2608, YM2151, YM2612, YM2203, YM2413, and more.
*   **Multi-Format Playback:** Supports `.vgm`, `.vgz`, and `.s98` music file formats.
*   **Advanced Refresh Modes:**
    *   **Flush Mode:** Provides two data flush modes: `Register-Level` and `Command-Level`, switchable via keys `1` and `2`.
    *   **Timer Mode:** Offers multiple high-precision timer modes to suit different systems and performance needs, including `Default Sleep`, `Hybrid Sleep`, `Multimedia Timer`, etc., switchable via keys `3` through `9`.
*   **Automatic Configuration Saving:**
    *   Settings such as chip selection, playback speed, and refresh modes are automatically saved to `config.ini` and loaded on the next launch.
*   **Real-time Playback Control:** Supports next/previous track, pause/resume, random mode, and fine-tuning of playback speed during playback.
*   **Cross-Platform Design:** Primarily designed for Windows, but the code structure is also compatible with POSIX-like systems (e.g., Linux).

### Controls

While the player is running, you can use the following keyboard shortcuts:

| Key | Function |
| :---: | :--- |
| `q` | Quit the player |
| `p` | Pause/Resume playback |
| `n` | Next track |
| `b` | Previous track |
| `r` | Toggle random mode |
| `+` | Increase playback speed |
| `-` | Decrease playback speed |
| `1` | Switch to `Register-Level` flush mode |
| `2` | Switch to `Command-Level` flush mode |
| `3` | Switch to `Default Sleep` timer mode |
| `4` | Switch to `Hybrid Sleep` timer mode |
| `5` | Switch to `Multimedia Timer` timer mode |
| `6`-`9` | Switch to other improved/experimental timer modes |

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

This C language version of the player heavily referenced the **`node-spfm`** project (located at `D:\working\vscode-projects\yasp11\node-spfm-denjhang-main`), developed by Denjhang. `node-spfm` is a full-featured Node.js implementation that provided the core logic and algorithmic foundation for this C port.

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
