# YASP - Yet Another Sound Player

## 摘要 (Abstract)

YASP (Yet Another Sound Player) 是一款专为 SPFM (Sound-Processing FM-synthesis) 系列硬件设计的命令行音乐播放器。它专注于高效、精确地播放经典的 VGM 和 S98 格式音乐文件，并通过多种可配置的刷新模式和计时器选项，为用户提供极致的复古音频体验。

YASP (Yet Another Sound Player) is a command-line music player designed for the SPFM (Sound-Processing FM-synthesis) family of hardware. It focuses on efficiently and accurately playing classic VGM and S98 format music files, providing users with the ultimate retro audio experience through various configurable refresh modes and timer options.

---

## 目录 (Table of Contents)

1.  [项目背景 (Project Background)](#项目背景-project-background)
2.  [功能特色 (Features)](#功能特色-features)
3.  [操作方法 (Controls)](#操作方法-controls)
4.  [编译方法 (Build Instructions)](#编译方法-build-instructions)
5.  [参考源码 (Source Code References)](#参考源码-source-code-references)

---

## 正文 (Body)

### 项目背景 (Project Background)

本项目旨在为 SPFM 硬件爱好者打造一款功能强大且高度可定制的命令行音乐播放器。项目的开发由以下成员合作完成：

*   **Denjhang (项目所有者/主要开发者):** 负责提出项目需求、核心架构设计、提供关键技术实现（如 MMCSS + IAudioClient3 示例代码），并进行最终测试与验证。
*   **Cline (AI 软件工程师):** 负责根据需求进行具体的代码实现、功能集成、错误修复、UI 调整以及文档撰写。

This project aims to create a powerful and highly customizable command-line music player for SPFM hardware enthusiasts. The development was a collaborative effort between the following members:

*   **Denjhang (Project Owner/Lead Developer):** Responsible for defining project requirements, core architecture design, providing key technical implementations (such as the MMCSS + IAudioClient3 example code), and conducting final testing and validation.
*   **Cline (AI Software Engineer):** Responsible for implementing specific code based on requirements, feature integration, bug fixing, UI adjustments, and documentation writing.

### 功能特色 (Features)

*   **广泛的芯片支持 (Broad Chip Support):** 支持多种经典的 FM 合成芯片，包括 YM2608, YM2151, YM2612, YM2203, YM2413 等。
*   **多格式播放 (Multi-Format Playback):** 支持 `.vgm`, `.vgz`, 和 `.s98` 音乐文件格式。
*   **高级刷新模式 (Advanced Refresh Modes):**
    *   **Flush Mode:** 提供两种数据刷新模式：`Register-Level`（逐寄存器写入）和 `Command-Level`（命令级写入），可通过按键 `1` 和 `2` 切换。
    *   **Timer Mode:** 提供多种高精度计时器模式以适应不同系统和性能需求，包括 `Default Sleep`, `Hybrid Sleep`, `Multimedia Timer` 等，可通过按键 `3` 至 `9` 切换。
*   **自动配置保存 (Automatic Configuration Saving):**
    *   芯片选择、播放速度和刷新模式等设置会自动保存到 `config.ini` 文件中，并在下次启动时自动加载。
*   **实时播放控制 (Real-time Playback Control):** 支持在播放过程中进行下一首、上一首、暂停/继续、随机播放和播放速度微调。
*   **跨平台设计 (Cross-Platform Design):** 主要为 Windows 设计，但代码结构也兼容类 POSIX 系统（如 Linux）。

### 操作方法 (Controls)

在播放器运行时，您可以使用以下键盘快捷键：

| 按键 (Key) | 功能 (Function)                               |
| :--------: | :-------------------------------------------- |
|     `q`    | 退出播放器 (Quit the player)                  |
|     `p`    | 暂停/继续播放 (Pause/Resume playback)         |
|     `n`    | 下一首 (Next track)                           |
|     `b`    | 上一首 (Previous track)                       |
|     `r`    | 开启/关闭随机播放 (Toggle random mode)        |
|     `+`    | 加快播放速度 (Increase playback speed)        |
|     `-`    | 减慢播放速度 (Decrease playback speed)        |
|     `1`    | 切换到 `Register-Level` 刷新模式 (Switch to `Register-Level` flush mode) |
|     `2`    | 切换到 `Command-Level` 刷新模式 (Switch to `Command-Level` flush mode) |
|     `3`    | 切换到 `Default Sleep` 计时器模式 (Switch to `Default Sleep` timer mode) |
|     `4`    | 切换到 `Hybrid Sleep` 计时器模式 (Switch to `Hybrid Sleep` timer mode) |
|     `5`    | 切换到 `Multimedia Timer` 计时器模式 (Switch to `Multimedia Timer` timer mode) |
|    `6`-`9` | 切换到其他改良/实验性计时器模式 (Switch to other improved/experimental timer modes) |

### 编译方法 (Build Instructions)

#### Windows (使用 MinGW/MSYS2)

1.  确保您已安装 MinGW-w64 或 MSYS2，并已将其 `bin` 目录添加到系统 PATH。
2.  将 FTDI D2XX 驱动文件 (`ftd2xx.h`, `ftd2xx.lib` 或 `libftd2xx.a`) 放置在项目根目录下的 `ftdi_driver` 文件夹中。
3.  打开命令行，导航到 `console_player` 目录。
4.  运行 `make` 命令进行编译：
    ```bash
    make -C console_player
    ```
5.  编译成功后，将在 `console_player` 目录下生成 `yasp_test.exe`。

#### Linux / macOS

编译方法与 Windows 类似，但需要确保已安装 `libftdi` 开发库。`makefile` 会自动处理平台特定的链接选项。

### 参考源码 (Source Code References)

在开发过程中，我们对以下核心文件进行了修改和参考，以实现各项功能：

*   `console_player/main.c`: 主程序入口，负责初始化、配置加载、播放循环和键盘输入处理。
*   `console_player/play.c`: 播放器 UI 显示和文件播放逻辑的核心。
*   `console_player/util.c` & `console_player/util.h`: 实现了各种计时器模式和辅助工具函数。
*   `console_player/vgm.c`: VGM 文件解析和播放逻辑。
*   `console_player/s98.c`: S98 文件解析和播放逻辑。
*   `console_player/makefile`: 项目的构建脚本，负责编译和链接所有源文件。
