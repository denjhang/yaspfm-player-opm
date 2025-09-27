# YASP - Yet Another Sound Player (技术文档)

## 摘要

YASP (Yet Another Sound Player) 是一款专为 SPFM (Sound-Processing FM-synthesis) 系列硬件设计的、功能强大的命令行音乐播放器。它专注于高效、精确地播放经典的 VGM 和 S98 格式音乐文件。本文档旨在深入剖析其核心技术实现，包括高级计时系统、SPFM 通信协议、以及创新的芯片转换与缓存机制。

---

## 目录

1.  [源码参考与致谢](#1-源码参考与致谢)
2.  [项目背景与架构](#2-项目背景与架构)
3.  [核心功能与特色](#3-核心功能与特色)
4.  [关键技术剖析](#4-关键技术剖析)
    *   [4.1. VGM 文件处理流程](#41-vgm-文件处理流程)
    *   [4.2. 智能芯片转换](#42-智能芯片转换)
    *   [4.3. 高级计时与刷新系统](#43-高级计时与刷新系统)
    *   [4.4. SPFM 通信协议](#44-spfm-通信协议)
5.  [操作指南](#5-操作指南)
6.  [编译与构建](#6-编译与构建)

---

## 1. 源码参考与致谢

YASP 的开发深度参考了以下优秀项目，在此对这些项目的作者表示由衷的敬意和感谢。他们的工作为 YASP 的实现提供了坚实的基础和关键的灵感。

*   **`yasp (Yet Another SPFM Light Player)`**
    *   **作者**: uobikiemukot
    *   **贡献**: **本项目的起点和最初的灵感来源。** YASP 的名称继承自此项目，其早期的代码结构为本项目的开发提供了宝贵的初始框架和方向。

*   **`node-spfm`**
    *   **作者**: Mitsutaka Okazaki (digital-sound-antiques), Denjhang
    *   **贡献**: 提供了本项目的核心逻辑和算法基础，特别是在 SPFM 通信协议 (`spfm.c`)、芯片控制以及 OPN 到 OPM 的转换算法 (`opn_to_opm.c`) 方面。

*   **VGMPlay**
    *   **作者**: Valley Bell
    *   **贡献**: 本项目的高级计时系统 (`util.c` 中的 `yasp_usleep`) 完全基于 VGMPlay 的核心计时模型，从根本上解决了播放稳定性的问题。模式 6 (VGMPlay Mode) 和模式 7 (Optimized VGMPlay Mode) 直接来源于其思想。

*   **`vgm-conv`**
    *   **作者**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **贡献**: 芯片频率转换的精确数学模型来源于此项目。

*   **`vgm-parser-js`**
    *   **作者**: Mitsutaka Okazaki (digital-sound-antiques)
    *   **贡献**: 在解决 GD3 标签缓存问题时，本项目借鉴了其 `vgm-parser-main` 模块中健壮的“完整文件载入内存”处理策略 (`vgm.c`)，为最终的成功实现提供了关键思路。

## 2. 项目背景与架构

YASP 旨在为 SPFM 硬件爱好者提供一个功能强大且高度可定制的命令行音乐播放器。项目由 **Denjhang** (项目所有者/主要开发者) 和 **Cline** (AI 软件工程师) 合作完成，前者负责需求、架构和关键技术验证，后者负责具体实现、集成和文档撰写。

**核心架构:**
*   `main.c`: 主程序入口，负责初始化、配置加载、主播放循环和键盘输入处理。
*   `play.c`: 播放器 UI 渲染和状态管理的核心。
*   `vgm.c` / `s98.c`: 分别负责 VGM 和 S98 文件的解析、命令处理和播放逻辑。
*   `opn_to_opm.c`, `ay_to_opm.c`, `sn_to_ay.c`: 实现芯片指令转换的模块。
*   `util.c`: 包含高精度定时器、`yasp_usleep` 实现和 `ini` 文件读写等辅助工具。
*   `spfm.c`: 封装了与 SPFM 硬件通过 FTDI D2XX 驱动进行通信的底层逻辑。

## 3. 核心功能与特色

*   **广泛的芯片支持:** YASP 支持多种经典的音源芯片，无论是直接播放还是通过自动转换，覆盖了大量怀旧游戏音乐的需求。支持的芯片列表如下：
    *   YM2608, YM2151, YM2612, YM2203, YM2413, YM3526, YM3812, Y8950, AY8910, SN76489, YMF262, SEGAPCM, RF5C68, YM2610
*   **多格式播放:** 支持 `.vgm`, `.vgz`, 和 `.s98` 音乐文件。
*   **GD3 标签保真:** 采用先进的内存处理技术，确保在转换和缓存过程中，VGM 文件内的 GD3 音乐标签被完整保留。
*   **高级计时系统:** 提供多种基于高精度性能计数器的计时策略，确保在各种系统负载下都能实现精确、无抖动的音频播放。
*   **内置文件浏览器:** 方便用户在文件系统中导航和选择音乐。

## 4. 关键技术剖析

### 4.1. VGM 文件处理流程

为了解决在文件转换和缓存过程中遇到的数据一致性问题（特别是 GD3 标签的丢失），YASP 采用了一种健壮的、基于内存的处理流程。该机制的核心是：在需要创建缓存时，**先将整个原始 VGM 文件一次性完整读入内存**，然后关闭文件句柄。后续的转换、GD3 标签提取和新缓存文件的写入，都基于这个内存副本进行操作。这种“先完整载入，再隔离处理”的策略，确保了数据源的唯一性和不可变性，是保证播放流畅、转换无误和 GD3 标签不丢失的基石。

以下是 `vgm.c` 中 `vgm_play` 函数实现此流程的关键代码：

**1. 将整个原始 VGM 文件读入内存**
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
**解说**: 这是“先完整载入”策略的直接体现。代码获取文件大小，分配等大的内存，然后一次性将整个文件读入 `original_file_data` 缓冲区。完成后，立即关闭原始文件句柄，后续所有操作都与磁盘上的原始文件无关，从根本上避免了文件指针混乱的问题。

**2. 从内存中提取 GD3 标签并写入缓存**
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
**解说**: 这是“隔离处理”策略的关键一步。代码直接从内存缓冲区 `original_file_data` 中读取 GD3 标签的偏移和长度，然后将完整的 GD3 数据块（包括头部和内容）原封不动地写入新的缓存文件。这确保了 GD3 信息的无损迁移。

**3. 从内存中进行数据转换**
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
**解说**: 转换函数 `vgm_convert_and_cache_opn_to_opm_from_mem` 的输入不再是文件指针，而是指向内存中 VGM 数据部分的指针 `vgm_data`。这彻底避免了在文件流上进行读写操作可能引发的数据不一致问题，保证了转换过程的纯粹和可靠。

### 4.2. 智能芯片转换

当检测到用户硬件上缺少 VGM 文件所需的芯片，但存在可替代的目标芯片时，YASP 会自动执行转换。

#### 4.2.1. 转换路径概览

| 源芯片 (Source) | 目标芯片 (Target) | 转换模块 | 备注 |
| :--- | :--- | :--- | :--- |
| **YM2612/2203/2608 (OPN)** | YM2151 (OPM) | `opn_to_opm.c` | 核心 FM 音源转换，支持频率、包络等参数映射。 |
| **AY8910 (PSG)** | YM2151 (OPM) | `ay_to_opm.c` | 使用 OPM 的 FM 合成来模拟 PSG 的方波和噪声。 |
| **SN76489 (DCSG)** | AY8910 (PSG) | `sn_to_ay.c` | 中间步骤，将 SN76489 指令转为 AY8910 指令。 |
| **SN76489 (DCSG)** | YM2151 (OPM) | `sn_to_ay.c` -> `ay_to_opm.c` | **转换链**：SN76489 -> AY8910 -> YM2151。 |

#### 4.2.2. 核心转换源码详解

**1. OPN (YM2612/2203/2608) -> OPM (YM2151)**

这是最核心的 FM 音源转换。主要在 `opn_to_opm.c` 中实现。OPN 系列使用 `F-Number` 和 `Block` 定义频率，而 OPM 使用 `Key Code` 和 `Key Fraction`。转换必须通过以下数学公式完成：

```c
// 1. OPN 参数转为实际频率 (Hz)
double freq = (_source_clock * fnum) / ((72.0 * _clock_div) * (1 << (20 - blk)));

// 2. 频率转为 OPM 的音高值 (Key)
const double BASE_FREQ_OPM = 277.2; // OPM 基准频率
double key = 60.0 + log2((freq * _clock_ratio) / BASE_FREQ_OPM) * 12.0;

// 3. Key 值拆分为 Key Code (KC) 和 Key Fraction (KF)
*kc = (oct << 4) | note;
*kf = (uint8_t)floor(frac * 64.0);
```
这段代码精确地将一个音源的频率体系映射到了另一个完全不同的体系上。

**2. AY8910 (PSG) -> OPM (YM2151)**

此转换在 `ay_to_opm.c` 中实现，目标是用 OPM 的 FM 合成来**模拟** PSG 的方波和噪声。OPM 本身没有方波通道，因此代码使用了一个特殊的 FM 音色（`CON=4`）来模拟方波。

```c
// 在 ay_to_opm_init 中，为 OPM 通道 4,5,6 设置模拟方波的音色
_y(0x20 + opmCh, 0xfc); // RL=ON FB=7 CON=4
_y(0x60 + opmCh, 0x1b); // M1: TL=27 (设置一个基础音量)
_y(0x80 + opmCh, 0x1f); // M1: AR=31 (快速起音)
```

同时，PSG 的对数式音量需要通过查找表 `VOL_TO_TL` 转换为 OPM 的线性 `Total Level` (TL)。

```c
// VOL_TO_TL 数组
static const int VOL_TO_TL[] = {127, 62, 56, 52, 46, 42, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0};

// 在 _updateTone 函数中应用
const int tVol = (v & 0x10) ? 0 : (v & 0xf); // 获取音量值
_y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf])); // 查表并写入 OPM 的 TL 寄存器
```

**3. SN76489 (DCSG) -> AY8910 (PSG)**

这是一个中间转换步骤，在 `sn_to_ay.c` 中实现。其核心是处理 SN76489 的通道3与噪声的混合逻辑，因为这与 AY8910 的行为不同。

```c
// 在 _updateSharedChannel 函数中处理混合逻辑
static void _updateSharedChannel() {
    int noiseChannel = _mixChannel; // 通常是通道 2
    bool enableTone = _atts[noiseChannel] != 0xf;
    bool enableNoise = _atts[3] != 0xf;

    // 混合解析：如果噪声和音调都启用，噪声优先
    if (enableTone && enableNoise) {
        enableTone = false;
    }
    
    // 根据解析结果，设置 AY8910 的通道和噪声使能寄存器 (R7)
    const uint8_t toneMask = enableTone ? 0 : (1 << noiseChannel);
    const uint8_t noiseMask = enableNoise ? (7 & ~(1 << noiseChannel)) : 7;
    _y(7, (noiseMask << 3) | toneMask); // _y() 会调用 ay_to_opm_write_reg()
}
```
这段代码是整个转换链能够正确发声的关键。

### 4.3. 高级计时与刷新系统

YASP 的播放精度和稳定性由“刷新模式”和“定时器模式”共同决定。

#### 4.3.1. 刷新模式 (Flush Mode)

刷新模式决定了播放器将寄存器数据发送到 SPFM 硬件的**粒度**。

| 模式 (按键) | 名称 | 原理 |
| :---: | :--- | :--- |
| **1** | 寄存器级 (Register-Level) | **每一次**寄存器写入后，立刻调用 `spfm_flush()`。这保证了最高的实时性，但 USB 通信开销巨大。 |
| **2** | **命令级 (Command-Level)** | 仅在处理完一个完整的 VGM 命令后（如 `0x61 nn nn` 这样的等待命令，或一个芯片写入命令），才调用 `spfm_flush()`。这是**性能和实时性的最佳平衡**，也是默认设置。 |

#### 4.3.2. 定时器模式 (Timer Mode)

定时器模式决定了播放器在处理完 VGM 命令后，如何精确地**等待**到下一个事件发生。所有模式都基于高精度性能计数器 (`QueryPerformanceCounter`) 进行误差补偿。

| 模式 (按键) | 源码名称 | 原理 |
| :---: | :--- | :--- |
| **3** | High-Precision Sleep | 调用 `Sleep(1)` 等待，并用性能计数器补偿其巨大的不精确性。适用于低负载环境。 |
| **4** | Hybrid Sleep | 使用 `CreateWaitableTimer` 等待 1ms，并补偿误差。比 `Sleep(1)` 更精确。 |
| **5** | Multimedia Timer | 使用 `timeSetEvent` 设置一个 1ms 的高精度多媒体定时器。播放线程等待该定时器触发的事件。精度高，但高负载下事件可能延迟。 |
| **6** | **VGMPlay Mode** | “忙等”的变体，不断调用 `Sleep(0)` 让出时间片，响应极快，但 CPU 占用率极高。 |
| **7** | **Optimized VGMPlay Mode** | 模式 5 的优化版，增加了“防失控”机制，防止声音滞后和崩溃。**这是默认和最佳选择。** |

#### 4.3.3. 定时器核心源码详解

以下是 `util.c` 中 `yasp_usleep` 函数针对不同模式的核心实现：

**模式 3: High-Precision Sleep**
```c
case 0: // High-Precision Sleep
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        Sleep(1);
    }
    break;
```
**解说**: 这是最简单的实现。循环调用 `Sleep(1)`，每次让出至少 1ms 的时间片，直到性能计数器达到目标时间。`Sleep` 的精度很差，但 CPU 占用率低。

**模式 4: Hybrid Sleep**
```c
case 1: // Hybrid Sleep
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        WaitForSingleObject(g_waitable_timer, 1);
    }
    break;
```
**解说**: 使用 Windows 的可等待定时器 `WaitForSingleObject` 等待 1ms。它比 `Sleep(1)` 更精确，是精度和 CPU 占用的一个折中。

**模式 5: Multimedia Timer**
```c
case 2: // Multimedia Timer
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        WaitForSingleObject(g_timer_event, 1);
    }
    break;
```
**解说**: 依赖于一个由 `timeSetEvent` 创建的、在后台每 1ms 触发一次的 `g_timer_event` 事件。线程等待这个事件，精度很高。

**模式 6: VGMPlay Mode**
```c
case 3: // VGMPlay Mode
    tick_to_wait = get_tick() + us_to_tick(us);
    while (get_tick() < tick_to_wait) {
        Sleep(0);
    }
    break;
```
**解说**: 这是“忙等”的一种形式。`Sleep(0)` 会让当前线程立即放弃剩余的时间片，让其他线程运行。这使得循环检测时间的频率非常高，响应极快，但会持续占用 CPU。

**模式 7: Optimized VGMPlay Mode**
此模式的优化不在 `yasp_usleep` 中，而在 `play.c` 的 `vgm_play` 函数里。它使用与模式 5 相同的等待机制，但增加了“防失控”逻辑：
```c
// 在 play.c 的 vgm_play 中
if (g_timer_mode == 7) {
    vgm_slice_to_do = min(vgm_slice_to_do, g_vgm_slice_limiter);
}
```
**解说**: 如果因为系统繁忙导致定时器事件严重延迟，积压了大量需要播放的样本 (`vgm_slice_to_do`)，此代码会将其限制在一个最大值 (`g_vgm_slice_limiter`) 内。这可以防止声音出现长时间的滞后，并快速追上实时进度，代价是丢弃少量积压的样本。

### 4.4. SPFM 通信协议

YASP 通过 FTDI D2XX 驱动与 SPFM 硬件通信。其核心是**写入缓冲机制**和**混合等待策略**。对于极短的等待（<10个样本），发送硬件等待命令 `0x80`；对于较长的等待，则调用由上述定时器模式决定的高精度软件等待函数 `yasp_usleep`。

## 5. 操作指南

| 按键 | 功能 |
| :--: | :-- |
| `q` | 退出 |
| `p` | 暂停/继续 |
| `n` / `b` | 下一首 / 上一首 |
| `r` | 切换随机/顺序播放 |
| `f` | 打开/关闭文件浏览器 |
| `+` / `-` | 加速/减速 |
| `1` / `2` | 切换刷新模式 |
| `3`-`7` | 切换定时器模式 |

## 6. 编译与构建

在 `console_player` 目录下运行 `make` 即可。
