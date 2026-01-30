# YASP - Yet Another Sound Player (控制台版)

YASP 是一个为 SPFM 系列硬件设计的 VGM/S98 音乐播放器。本项目是其控制台版本，专注于提供一个轻量级、高效的命令行播放体验。

## 功能特性

-   支持 VGM, VGZ, S98 文件格式。
-   通过文件浏览器或命令行参数指定音乐文件/目录。
-   支持顺序播放和随机播放模式。
-   实时调整播放速度和 OPN LFO 幅度。
-   支持多种高精度计时器模式，以实现精确的音频同步。
-   **芯片转换**：能够将 YM2612/YM2203/AY8910/SN76489 的音乐实时转换为 YM2151 (OPM) 格式播放，并自动生成缓存。

## 编译与运行

### 编译

在项目根目录下，使用 `make` 命令进行编译：

```bash
make -C console_player
```

### 运行

直接运行生成的可执行文件：

```bash
./console_player/yasp_test.exe
```

## Bug 修复与改进日志 (v1.1)

本次更新修复了多个关键问题，并对核心功能进行了改进：

1.  **修复了转换后 VGM 文件的循环逻辑**
    *   **问题**: 转换后的缓存 VGM 文件（例如，从 YM2612 转换到 YM2151）没有正确继承原始文件的循环点，而是强制从文件头开始循环。
    *   **修复过程**:
        1.  在 `vgm.c` 中引入了 `g_original_loop_offset` 和 `g_converted_loop_offset` 两个全局变量，用于在转换过程中追踪循环点的位置。
        2.  修改了 `vgm_convert_and_cache_from_mem` 函数。在遍历原始 VGM 数据流时，检查当前的文件指针是否到达了原始头部记录的循环点 (`g_original_loop_offset`)。
        3.  当到达该点时，记录下当前**缓存文件**的写入位置 (`ftell(g_cache_fp)`) 到 `g_converted_loop_offset`。
        4.  最后，在 `vgm_play` 函数中，当回写缓存文件的头部信息时，使用这个新计算出的 `g_converted_loop_offset` 作为新的循环起始点，从而确保了循环的准确性。

2.  **修复了播放完成后的自动切歌问题**
    *   **问题**: 在某些情况下（尤其是在播放期间按过“下一首”或“上一首”之后），一首歌曲在完成所有循环后不会自动切换到播放列表的下一首。
    *   **修复过程**:
        1.  问题定位在 `main.c` 的 `player_thread_func` 线程函数中。
        2.  原逻辑在 `play_file` 函数返回后，会检查 `g_next_track_flag` 等标志位。如果这些标志位被置位，它会错误地将 `g_is_playing` 设为 `false` 并中断播放循环。
        3.  修改了该逻辑判断。现在，如果歌曲是自然播放结束（即没有被用户手动切歌中断），它会正常地获取下一首歌的索引并继续播放。如果歌曲是被用户中断的，则由主循环的其他部分来处理切歌逻辑，避免了线程的提前退出。

3.  **修正了 UI 中总时间的计算**
    *   **问题**: UI 显示的“Total Time”没有将循环次数计算在内，导致显示的时间与实际播放时间不符。
    *   **修复过程**: 在 `play.c` 的 `update_ui` 函数中，修改了总时间的计算公式。新的计算方式为：`总样本数 + (循环部分样本数 * (循环次数 - 1))`。这样，显示的总时间就能准确反映包括所有循环在内的完整播放时长。

4.  **修正了 UI 中 VGM 芯片信息的显示**
    *   **问题**: 当播放一个经过转换的缓存文件时，UI 的“VGM Chip”字段显示的是目标芯片（如 YM2151），而不是原始 VGM 文件中记录的芯片（如 YM2612）。
    *   **修复过程**:
        1.  在 `vgm.c` 中，当解析头部时，除了 `g_vgm_chip_type` 外，还使用 `g_original_vgm_chip_type` 来专门存储原始文件的芯片类型。
        2.  在 `play.c` 的 `update_ui` 函数中，修改了显示逻辑。现在它会检查 `g_is_playing_from_cache` 标志。如果正在播放缓存文件，则强制使用 `g_original_vgm_chip_type` 和原始头部信息来显示芯片名称和时钟频率。

5.  **增加了对无效 VGM 文件的检测**
    *   **问题**: 对于头部信息不完整或损坏的 VGM 文件（例如所有芯片时钟都为 0），播放器仍然会尝试猜测一个时钟频率并播放，这可能导致不正确的行为。
    *   **修复过程**: 在 `play.c` 的 `update_ui` 函数中，增加了一个检查。在显示芯片信息前，它会判断 VGM 头部中所有已知芯片的时钟是否都为 0。如果是，则直接显示 "VGM Chip: Invalid/Unsupported"，向用户明确指出文件可能存在问题。

## 芯片转换

### OPN (YM2612/YM2203) -> OPM (YM2151)

为了在没有 OPN 芯片的情况下播放 OPN 音乐，YASP 实现了一个实时的寄存器级转换器。它将 OPN 的寄存器写入转换为功能上等效的 OPM 寄存器写入。

#### 寄存器映射总览

| OPN (YM2612/2203) | OPM (YM2151) | 简要说明 |
| :--- | :--- | :--- |
| `0x22` (LFRQ) | `0x18` (LFRQ) | LFO 频率转换 |
| `0x28` (Key On/Off) | `0x08` (Key On/Off) | 通道与操作员的开关 |
| `0x30..0x8F` (Operator) | `0x40..0xBF` (Operator) | 操作员参数（DT, MUL, TL, KS, AR, DR, SR, RR, SL, SSG-EG） |
| `0xA0..0xA6` (F-Num/Block) | `0x28..0x37` (KC/KF) | 频率（音高）转换 |
| `0xB0..0xB2` (FB/CONNECT) | `0x20..0x27` (RL/FB/CONNECT) | 反馈与算法 |
| `B4..B6` (L/R/AMS/PMS) | `0x38..0x3F` (PMS/AMS) | 声道、LFO 灵敏度 |

#### 详细转换规则

1.  **LFO 频率 (LFRQ)**
    *   **OPN `0x22` -> OPM `0x18`**
    *   OPN 的 LFO 频率只有 8 级，而 OPM 更加精细。转换器使用一个查找表（LUT）来映射 OPN 的值到 OPM 上一个最接近的等效值。这个 LUT 基于 `OPNFMOnSFG.asm` 的实现，以确保听感上的一致性。

2.  **Key On/Off**
    *   **OPN `0x28` -> OPM `0x08`**
    *   这个寄存器控制哪个通道的哪些操作员（Slot）发声或停止。OPN 的通道选择在低 3 位，而 OPM 在低 3 位。OPN 的操作员选择在高 4 位，而 OPM 在高 3 位。转换时需要进行相应的位移和屏蔽。
    *   **关键代码**: `_write_func(0x08, (slots << 3) | ch);`

3.  **操作员参数 (DT, MUL, TL, etc.)**
    *   **OPN `0x30..0x8F` -> OPM `0x40..0xBF`**
    *   这些寄存器控制了 FM 合成的核心参数，如Detune, Multiple, Total Level, Attack Rate等。幸运的是，OPN 和 OPM 在这些参数的位定义上大部分是兼容的。转换主要是地址的重新计算。
    *   OPN 的地址格式为 `Base + Channel + SlotOffset`，而 OPM 是 `Base + Channel + SlotOffset`。转换函数需要根据输入的 OPN 地址计算出对应的 OPM 地址。
    *   **关键代码**: `uint8_t base = 0x40 + ((addr & 0xf0) - 0x30) * 2; uint8_t offset = slot * 8 + ch; _write_func(base + offset, data);`

4.  **频率 (F-Number/Block 到 Key Code/Fraction)**
    *   **OPN `0xA0..0xA6` -> OPM `0x28..0x37`**
    *   这是最复杂的转换。OPN 使用 `F-Number` 和 `Block`（八度）来定义频率，而 OPM 使用 `Key Code` (音符) 和 `Key Fraction` (精细调谐)。
    *   转换过程如下：
        1.  根据 OPN 的 F-Num 和 Block 计算出实际的频率值 (Hz)。
        2.  `freq = (source_clock * fnum) / (divisor * (1 << (20 - blk)))`
        3.  将计算出的频率 (Hz) 再转换为 OPM 的音符和音分。
        4.  `key = 60 + log2((freq * clock_ratio) / BASE_FREQ_OPM) * 12.0`
        5.  最后将 `key` 分解为 OPM 的 `KC` (八度+音符) 和 `KF` (音分) 并写入相应寄存器。
    *   **关键代码**: `opn_freq_to_opm_key(fnum, blk, &kc, &kf); _write_func(0x28 + ch, kc); _write_func(0x30 + ch, kf << 2);`

5.  **反馈/算法 (FB/CONNECT)**
    *   **OPN `0xB0..0xB2` -> OPM `0x20..0x27`**
    *   OPN 和 OPM 在此处的位定义相似，但 OPM 的寄存器还包含了声道信息（RL）。因此，在写入时，需要先从缓存中读取该通道的声道设置，然后与反馈/算法值合并后再写入。
    *   **关键代码**: `_write_func(0x20 + ch, (get_rl_flags(ch) << 6) | (data & 0x3f));`

6.  **声道/LFO 灵敏度 (L/R/AMS/PMS)**
    *   **OPN `0xB4..0xB6` -> OPM `0x38..0x3F`**
    *   OPN 将声道（L/R）和 LFO 灵敏度（AMS/PMS）打包在同一个寄存器中，而 OPM 将它们分开。
    *   转换时：
        1.  从 OPN 寄存器中提取出声道信息并缓存起来（供其他寄存器转换时使用）。
        2.  提取 PMS（音高调制灵敏度）和 AMS（幅度调制灵敏度）。
        3.  特别地，为了让用户可以实时控制 LFO 效果的强度，代码中会对 PMS 值应用一个全局的幅度乘数 `g_opn_lfo_amplitude`。
        4.  将处理后的 PMS 和 AMS 写入 OPM 的 `0x38-0x3F` 寄存器。
    *   **关键代码**: `uint8_t scaled_pms = (uint8_t)(pms * g_opn_lfo_amplitude); _write_func(0x38 + ch, (scaled_pms << 4) | ams);`
