# YASP - Yet Another Sound Player

## 摘要

YASP (Yet Another Sound Player) 是一款专为 SPFM (Sound-Processing FM-synthesis) 系列硬件设计的命令行音乐播放器。它专注于高效、精确地播放经典的 VGM 和 S98 格式音乐文件，并通过多种可配置的刷新模式和计时器选项，为用户提供极致的复古音频体验。

---

## 目录

1.  [项目背景](#项目背景)
2.  [功能特色](#功能特色)
3.  [操作方法](#操作方法)
4.  [编译方法](#编译方法)
5.  [参考源码](#参考源码)

---

## 正文

### 项目背景

本项目旨在为 SPFM 硬件爱好者打造一款功能强大且高度可定制的命令行音乐播放器。项目的开发由以下成员合作完成：

*   **Denjhang (项目所有者/主要开发者):** 负责提出项目需求、核心架构设计、提供关键技术实现（如 MMCSS + IAudioClient3 示例代码），并进行最终测试与验证。
*   **Cline (AI 软件工程师):** 负责根据需求进行具体的代码实现、功能集成、错误修复、UI 调整以及文档撰写。

### 功能特色

*   **广泛的芯片支持:** 支持多种经典的 FM 合成芯片，包括 YM2608, YM2151, YM2612, YM2203, YM2413 等。
*   **多格式播放:** 支持 `.vgm`, `.vgz`, 和 `.s98` 音乐文件格式。
*   **高级刷新模式:**
    *   **Flush Mode:** 提供两种数据刷新模式：`Register-Level`（逐寄存器写入）和 `Command-Level`（命令级写入），可通过按键 `1` 和 `2` 切换。
    *   **Timer Mode:** 提供多种高精度计时器模式以适应不同系统和性能需求，包括 `Default Sleep`, `Hybrid Sleep`, `Multimedia Timer` 等，可通过按键 `3` 至 `9` 切换。
*   **自动配置保存:**
    *   芯片选择、播放速度和刷新模式等设置会自动保存到 `config.ini` 文件中，并在下次启动时自动加载。
*   **实时播放控制:** 支持在播放过程中进行下一首、上一首、暂停/继续、随机播放和播放速度微调。
*   **跨平台设计:** 主要为 Windows 设计，但代码结构也兼容类 POSIX 系统（如 Linux）。

### 操作方法

在播放器运行时，您可以使用以下键盘快捷键：

| 按键 | 功能 |
| :--: | :-- |
| `q` | 退出播放器 |
| `p` | 暂停/继续播放 |
| `n` | 下一首 |
| `b` | 上一首 |
| `r` | 开启/关闭随机播放 |
| `+` | 加快播放速度 |
| `-` | 减慢播放速度 |
| `1` | 切换到 `Register-Level` 刷新模式 |
| `2` | 切换到 `Command-Level` 刷新模式 |
| `3` | 切换到 `Default Sleep` 计时器模式 |
| `4` | 切换到 `Hybrid Sleep` 计时器模式 |
| `5` | 切换到 `Multimedia Timer` 计时器模式 |
| `6`-`9` | 切换到其他改良/实验性计时器模式 |

### 编译方法

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

### 参考源码

本 C 语言版本的播放器在开发过程中，深度参考了由 Denjhang 开发的 **`node-spfm`** (位于 `D:\working\vscode-projects\yasp11\node-spfm-denjhang-main`) 项目。`node-spfm` 是一个功能完备的 Node.js 实现，为本项目的 C 语言移植提供了核心逻辑和算法基础。

#### 主要参考部分:

*   **SPFM 通信协议:**
    `node-spfm/src/spfm.ts` 中定义的与 SPFM 硬件握手、发送命令和数据的逻辑，是本 C 项目中 `console_player/spfm.c` 实现的基础。包括设备初始化、芯片复位和寄存器写入等关键操作。
*   **VGM 文件解析:**
    `node-spfm/src/vgm.ts` 中的 VGM 文件头解析、数据块处理和命令解析逻辑，被直接借鉴并用 C 语言在 `console_player/vgm.c` 中重新实现。这确保了对 VGM 格式的精确支持。
*   **芯片控制逻辑:**
    `node-spfm/src/chips/` 目录下针对不同 FM 芯片（如 YM2151, YM2608）的控制类，为本项目的 `console_player/ym2151.c`, `console_player/ym2608.c` 等文件提供了寄存器地址和控制指令的准确参考。

除了 `node-spfm`，本项目还涉及以下文件的修改与实现：

*   `console_player/main.c`: 主程序入口，负责初始化、配置加载、播放循环和键盘输入处理。
*   `console_player/play.c`: 播放器 UI 显示和文件播放逻辑的核心。
*   `console_player/util.c` & `console_player/util.h`: 实现了各种计时器模式和辅助工具函数。
*   `console_player/makefile`: 项目的构建脚本，负责编译和链接所有源文件。
