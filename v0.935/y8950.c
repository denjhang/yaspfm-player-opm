#include "y8950.h"
#include "spfm.h"
#include "chiptype.h"
#include <math.h>
#include <stdio.h>

// Y8950 a.k.a. MSX-Audio
extern volatile int g_flush_mode;

// Y8950时序参数(从udpl_dll移植)
// wait12: 地址写入后的延时 = (12.0us * 1000000 / clock) + 0.99
// wait84: 数据写入后的延时 = (84.0us * 1000000 / clock) + 0.99
static uint8_t g_y8950_wait12 = 0;  // 微秒
static uint8_t g_y8950_wait84 = 0;  // 微秒

// 计算Y8950的精确延时参数
static void y8950_calculate_waits(uint32_t clock) {
    // 参考udpl_dll的计算方法
    g_y8950_wait12 = (uint8_t)((12.0f * 1000000.0f / (float)clock) + 0.99f);
    g_y8950_wait84 = (uint8_t)((84.0f * 1000000.0f / (float)clock) + 0.99f);
}

// Y8950初始化寄存器序列(从udpl_dll移植)
static const uint32_t Y8950_INIT_REGISTER[][2] = {
    { 0x00b0, 0x00 },  // key off
    { 0x00b1, 0x00 },
    { 0x00b2, 0x00 },
    { 0x00b3, 0x00 },
    { 0x00b4, 0x00 },
    { 0x00b5, 0x00 },
    { 0x00b6, 0x00 },
    { 0x00b7, 0x00 },
    { 0x00b8, 0x00 },
    { 0x0040, 0x3f },  // level 0
    { 0x0041, 0x3f },
    { 0x0042, 0x3f },
    { 0x0043, 0x3f },
    { 0x0044, 0x3f },
    { 0x0045, 0x3f },
    { 0x0046, 0x3f },
    { 0x0047, 0x3f },
    { 0x0048, 0x3f },
    { 0x0049, 0x3f },
    { 0x004a, 0x3f },
    { 0x004b, 0x3f },
    { 0x004c, 0x3f },
    { 0x004d, 0x3f },
    { 0x004e, 0x3f },
    { 0x004f, 0x3f },
    { 0x0050, 0x3f },
    { 0x0051, 0x3f },
    { 0x0052, 0x3f },
    { 0x0053, 0x3f },
    { 0x0054, 0x3f },
    { 0x0055, 0x3f },
    { 0x0001, 0x00 },
    { 0x0002, 0x00 },
    { 0x0003, 0x00 },
    { 0x0004, 0x00 },
    { 0x0008, 0x00 },
    { 0x0020, 0x00 },
    { 0x0021, 0x00 },
    { 0x0022, 0x00 },
    { 0x0023, 0x00 },
    { 0x0024, 0x00 },
    { 0x0025, 0x00 },
    { 0x0026, 0x00 },
    { 0x0027, 0x00 },
    { 0x0028, 0x00 },
    { 0x0029, 0x00 },
    { 0x002a, 0x00 },
    { 0x002b, 0x00 },
    { 0x002c, 0x00 },
    { 0x002d, 0x00 },
    { 0x002e, 0x00 },
    { 0x002f, 0x00 },
    { 0x0030, 0x00 },
    { 0x0031, 0x00 },
    { 0x0032, 0x00 },
    { 0x0033, 0x00 },
    { 0x0034, 0x00 },
    { 0x0035, 0x00 },
    { 0x0060, 0x00 },
    { 0x0061, 0x00 },
    { 0x0062, 0x00 },
    { 0x0063, 0x00 },
    { 0x0064, 0x00 },
    { 0x0065, 0x00 },
    { 0x0066, 0x00 },
    { 0x0067, 0x00 },
    { 0x0068, 0x00 },
    { 0x0069, 0x00 },
    { 0x006a, 0x00 },
    { 0x006b, 0x00 },
    { 0x006c, 0x00 },
    { 0x006d, 0x00 },
    { 0x006e, 0x00 },
    { 0x006f, 0x00 },
    { 0x0070, 0x00 },
    { 0x0071, 0x00 },
    { 0x0072, 0x00 },
    { 0x0073, 0x00 },
    { 0x0074, 0x00 },
    { 0x0075, 0x00 },
    { 0x0080, 0x00 },
    { 0x0081, 0x00 },
    { 0x0082, 0x00 },
    { 0x0083, 0x00 },
    { 0x0084, 0x00 },
    { 0x0085, 0x00 },
    { 0x0086, 0x00 },
    { 0x0087, 0x00 },
    { 0x0088, 0x00 },
    { 0x0089, 0x00 },
    { 0x008a, 0x00 },
    { 0x008b, 0x00 },
    { 0x008c, 0x00 },
    { 0x008d, 0x00 },
    { 0x008e, 0x00 },
    { 0x008f, 0x00 },
    { 0x0090, 0x00 },
    { 0x0091, 0x00 },
    { 0x0092, 0x00 },
    { 0x0093, 0x00 },
    { 0x0094, 0x00 },
    { 0x0095, 0x00 },
    { 0x00a0, 0x00 },
    { 0x00a1, 0x00 },
    { 0x00a2, 0x00 },
    { 0x00a3, 0x00 },
    { 0x00a4, 0x00 },
    { 0x00a5, 0x00 },
    { 0x00a6, 0x00 },
    { 0x00a7, 0x00 },
    { 0x00a8, 0x00 },
    { 0x00bd, 0x00 },
    { 0x00c0, 0x00 },
    { 0x00c1, 0x00 },
    { 0x00c2, 0x00 },
    { 0x00c3, 0x00 },
    { 0x00c4, 0x00 },
    { 0x00c5, 0x00 },
    { 0x00c6, 0x00 },
    { 0x00c7, 0x00 },
    { 0x00c8, 0x00 },
    { 0x00e0, 0x00 },
    { 0x00e1, 0x00 },
    { 0x00e2, 0x00 },
    { 0x00e3, 0x00 },
    { 0x00e4, 0x00 },
    { 0x00e5, 0x00 },
    { 0x00e6, 0x00 },
    { 0x00e7, 0x00 },
    { 0x00e8, 0x00 },
    { 0x00e9, 0x00 },
    { 0x00ea, 0x00 },
    { 0x00eb, 0x00 },
    { 0x00ec, 0x00 },
    { 0x00ed, 0x00 },
    { 0x00ee, 0x00 },
    { 0x00ef, 0x00 },
    { 0x00f0, 0x00 },
    { 0x00f1, 0x00 },
    { 0x00f2, 0x00 },
    { 0x00f3, 0x00 },
    { 0x00f4, 0x00 },
    { 0x00f5, 0x00 },
    { 0x0007, 0x00 },
    { 0x0008, 0x00 },
    { 0x0009, 0x00 },
    { 0x000a, 0x00 },
    { 0x000b, 0xff },
    { 0x000c, 0xff },
    { 0x000d, 0x00 },
    { 0x000e, 0x00 },
    { 0x0010, 0x00 },
    { 0x0011, 0x00 },
    { 0x0012, 0x00 }
};

#define Y8950_INIT_REG_COUNT (sizeof(Y8950_INIT_REGISTER) / sizeof(Y8950_INIT_REGISTER[0]))

void y8950_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // Y8950需要精确的寄存器写入延时(从udpl_dll移植)
    // 不同寄存器地址需要不同的延时

    // 根据参考代码的延时逻辑:
    // - 所有寄存器: 地址写入后需要wait12延时
    // - 寄存器地址 < 0x20: 数据写入后需要wait12延时
    // - 寄存器地址 >= 0x20: 数据写入后需要wait84延时

    uint32_t total_wait_us;

    if ((reg & 0xE0) == 0) {  // 地址 < 0x20
        // 地址延时 + 数据延时(都是wait12)
        total_wait_us = g_y8950_wait12 + g_y8950_wait12;
    } else {  // 地址 >= 0x20
        // 地址延时(wait12) + 数据延时(wait84)
        total_wait_us = g_y8950_wait12 + g_y8950_wait84;
    }

    // 将微秒转换为样本数(44.1kHz采样率)
    uint32_t wait_samples = (uint32_t)((uint64_t)total_wait_us * 44100 / 1000000);

    // 使用带延时的写入函数
    if (wait_samples > 0) {
        spfm_wait_and_write_reg(wait_samples, slot, 0, reg, data);
    } else {
        spfm_write_reg(slot, 0, reg, data);
    }

    if (g_flush_mode == 1) {
        spfm_flush();
    }
}

// Silences the Y8950 chip.
void y8950_mute(uint8_t slot) {
    int i;
    // Key off all 9 FM channels
    for (i = 0; i < 9; i++) {
        y8950_write_reg(slot, 0xB0 + i, 0x00);
    }
    // Set volume to max attenuation for all 9 FM channels
    for (i = 0; i < 9; i++) {
        y8950_write_reg(slot, 0x40 + i, 0x3F);
    }
    // Mute rhythm channels
    y8950_write_reg(slot, 0xBD, 0x00);
    // Mute ADPCM channel
    y8950_write_reg(slot, 0x07, 0x80);
}

// Initializes the Y8950 to a clean state with complete register sequence.
void y8950_init(uint8_t slot) {
    unsigned int i;

    // 获取Y8950的时钟频率
    uint32_t clock = get_chip_default_clock(CHIP_TYPE_Y8950);

    // 计算精确的延时参数
    y8950_calculate_waits(clock);

    // 使用完整的初始化寄存器序列(从udpl_dll移植)
    for (i = 0; i < Y8950_INIT_REG_COUNT; i++) {
        uint8_t reg = Y8950_INIT_REGISTER[i][0] & 0xFF;
        uint8_t data = Y8950_INIT_REGISTER[i][1] & 0xFF;
        y8950_write_reg(slot, reg, data);
    }

    spfm_flush();
}

// Y8950 ADPCM数据写入函数(参考vgmplay-msx和mdplayer/hoot的慢速载入模式)
uint32_t y8950_write_adpcm_data(uint8_t slot, uint32_t rom_size, uint32_t start_addr, const uint8_t* data, uint32_t data_size) {
    (void)rom_size;  // 未使用，但保留用于完整性

    if (data == NULL || data_size == 0) {
        return 0;  // 没有消耗样本
    }

    // Y8950地址使用8-byte units (参考vgmplay-msx)
    // CRITICAL FIX: Calculate stop address like YM2608 does
    uint32_t start = start_addr;
    uint32_t stop = start_addr + data_size - 1;  // End address (not 0xFFFF!)

    // Shift addresses by 3 for Y8950 (8-byte units)
    start >>= 3;
    stop >>= 3;

    const uint32_t chunk_size = 256;  // USB flush单位

    // 估算时间：基于硬件特性
    // Y8950 ADPCM写入：~200μs/byte (libvgm BRDY分析)
    // 每256字节需要约50ms硬件处理时间
    // v12: 使用v7的准确时序参数
    uint32_t total_chunks = (data_size + chunk_size - 1) / chunk_size;
    float estimated_time = (total_chunks * 0.05f) + 0.1f;  // 50ms/chunk + 100ms初始化

    printf("Y8950 ADPCM: Loading %uKB @ 0x%05X (~%.1fs)...\n",
           (data_size + 1023) / 1024, start_addr, estimated_time);

    // 初始化序列 - 使用直接spfm_write_reg避免额外的时序延时
    // 1. 设置ADPCM起始地址
    spfm_write_reg(slot, 0, Y8950_START_ADDRESS_L, start & 0xFF);
    spfm_write_reg(slot, 0, Y8950_START_ADDRESS_H, (start >> 8) & 0xFF);

    // 2. 设置停止地址（正确计算，不再是0xFFFF）
    spfm_write_reg(slot, 0, Y8950_STOP_ADDRESS_L, stop & 0xFF);
    spfm_write_reg(slot, 0, Y8950_STOP_ADDRESS_H, (stop >> 8) & 0xFF);

    // 3. 启用ADPCM写入 (参考vgmplay-msx)
    spfm_write_reg(slot, 0, Y8950_ADPCM_CONTROL, 0x01);

    // 4. 设置为写模式 (参考vgmplay-msx: ADPCM_CONTROL = 0x60)
    spfm_write_reg(slot, 0, Y8950_ADPCM_CONTROL, 0x60);

    // 5. 启用FLAG_CONTROL写模式 (参考vgmplay-msx: FLAG_CONTROL = 0x70)
    spfm_write_reg(slot, 0, Y8950_FLAG_CONTROL, 0x70);

    // 刷新初始化命令
    spfm_flush();

    // 6. 写入ADPCM数据
    // 参考MSX vgmplay和libvgm的ADPCM写入时序
    //
    // v12修复：结合v7的准确时序 + v11的非阻塞方式
    // - v7发现：50ms/256B是基于BRDY分析的准确时序（~200μs/byte）
    // - v11发现：移除Sleep()阻塞，完全依赖VGM wait_samples
    // - v12结合：使用50ms时序参数，但通过wait_samples表达，无阻塞
    //
    // 硬件特性：Y8950 ADPCM写入约200μs/byte (libvgm BRDY分析)
    // 每256字节约需50ms硬件处理时间

    uint32_t chunk_count = 0;
    uint32_t last_percent = 0;

    for (uint32_t i = 0; i < data_size; i++) {
        // 每个字节写入前刷新FLAG_CONTROL (参考MSX汇编)
        // MSX：每字节都设置FLAG_CONTROL=0xF0然后重置ADPCM_DATA地址
        // 我们简化为每256字节刷新一次
        if (i % chunk_size == 0 && i > 0) {
            spfm_write_reg(slot, 0, Y8950_FLAG_CONTROL, 0xF0);  // 掩码
            spfm_write_reg(slot, 0, Y8950_FLAG_CONTROL, 0x70);  // 重置写模式
        }

        // 写入ADPCM数据字节
        spfm_write_reg(slot, 0, Y8950_ADPCM_DATA, data[i]);

        // 每256字节或最后一个字节时flush
        if (((i + 1) % chunk_size == 0) || (i == data_size - 1)) {
            spfm_flush();  // 等待USB传输完成
            chunk_count++;

            // 显示进度（每25%）
            uint32_t current_percent = ((i + 1) * 100) / data_size;
            if (current_percent >= last_percent + 25 || (i + 1) == data_size) {
                uint32_t total_chunks = (data_size + chunk_size - 1) / chunk_size;
                printf("  Chunk %u/%u: %u bytes (%u%%)%s\n",
                       chunk_count, total_chunks, i + 1, current_percent,
                       (i + 1) == data_size ? " - Complete!" : "...");
                last_percent = current_percent;
            }
        }
    }

    // 7. 完成ADPCM写入 (参考vgmplay-msx: FLAG_CONTROL = 0x78)
    spfm_write_reg(slot, 0, Y8950_FLAG_CONTROL, 0x78);

    // 8. 停止ADPCM写入模式 (参考vgmplay-msx: ADPCM_CONTROL = 0x01)
    spfm_write_reg(slot, 0, Y8950_ADPCM_CONTROL, 0x01);

    // 最终flush
    spfm_flush();

    printf("  ADPCM loading complete!\n");

    // v12修复：使用v7的准确时序（50ms/256B）+ 非阻塞方式
    // 基于libvgm BRDY分析：Y8950 ADPCM写入需要约200μs/byte
    // 每256字节chunk需要约50ms硬件处理时间
    // 采样率44100Hz
    uint32_t total_delay_ms = total_chunks * 50 + 100;
    uint32_t wait_samples = (uint32_t)((uint64_t)total_delay_ms * 44100 / 1000);
    return wait_samples;
}
