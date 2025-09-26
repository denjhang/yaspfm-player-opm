#include "util.h"
#include <unistd.h> // For usleep on POSIX
#ifdef _WIN32
#include <windows.h>
#endif

extern volatile double g_speed_multiplier;

#ifdef _WIN32
static LARGE_INTEGER g_freq;
#endif

void yasp_timer_init(void) {
#ifdef _WIN32
    QueryPerformanceFrequency(&g_freq);
#endif
}

void yasp_timer_release(void) {
    // No release needed for QueryPerformanceCounter
}

int read_2byte_le(FILE *fp, uint16_t *value)
{
    uint8_t buf[2];
    if (fread(buf, 1, 2, fp) != 2) return -1;
    *value = buf[0] | (buf[1] << 8);
    return 0;
}

int read_4byte_le(FILE *fp, uint32_t *value)
{
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return -1;
    *value = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return 0;
}

uint32_t read_le32(const uint8_t* ptr)
{
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

uint64_t read_variable_length_7bit_le(FILE *fp)
{
    uint64_t result = 0;
    uint8_t byte;
    int shift = 0;
    do {
        if (fread(&byte, 1, 1, fp) != 1) return 0; // Error or EOF
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}

bool read_string(FILE *fp, char *str, int size)
{
    if (fread(str, 1, size, fp) != (size_t)size) return false;
    return true;
}

uint8_t low_byte(uint32_t value)
{
    return (uint8_t)(value & 0xFF);
}

uint8_t high_byte(uint32_t value)
{
    return (uint8_t)((value >> 8) & 0xFF);
}

// Helper function for high-precision spin-wait using QPC
static void qpc_spin(unsigned int usec) {
    if (usec == 0) return;
    LARGE_INTEGER start, current, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    LONGLONG wait_ticks = (freq.QuadPart * usec) / 1000000;
    LONGLONG target_ticks = start.QuadPart + wait_ticks;
    do {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart < target_ticks);
}

void yasp_usleep(unsigned int usec) {
    unsigned int final_usec = (unsigned int)(usec / g_speed_multiplier);
    if (final_usec == 0) {
        return;
    }

#ifdef _WIN32
    const unsigned int spin_threshold_us = 2000; // 2ms

    if (final_usec <= spin_threshold_us) {
        qpc_spin(final_usec);
        return;
    }

    // "Sleep" for the bulk of the duration, then spin for the final part.
    unsigned int sleep_ms = (final_usec - spin_threshold_us) / 1000;
    Sleep(sleep_ms);
    
    // Spin for the remaining precise duration
    qpc_spin(final_usec - (sleep_ms * 1000));

#else
    usleep(final_usec);
#endif
}

uint64_t get_current_time_us(void) {
#ifdef _WIN32
    if (g_freq.QuadPart == 0) {
        yasp_timer_init();
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (uint64_t)count.QuadPart * 1000000 / g_freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}

void yasp_usleep_precise(unsigned int usec) {
    unsigned int final_usec = (unsigned int)(usec / g_speed_multiplier);
    if (final_usec == 0) {
        return;
    }

#ifdef _WIN32
    // For very short delays, a spin-wait is still more accurate than a context switch.
    // Let's set the threshold to 100us.
    if (final_usec < 100) {
        LARGE_INTEGER start, current;
        QueryPerformanceCounter(&start);
        LONGLONG wait_ticks = (g_freq.QuadPart * final_usec) / 1000000;
        LONGLONG target_ticks = start.QuadPart + wait_ticks;
        do {
            QueryPerformanceCounter(&current);
        } while (current.QuadPart < target_ticks);
        return;
    }

    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer) {
        LARGE_INTEGER due_time;
        due_time.QuadPart = -((LONGLONG)final_usec * 10); // Convert to 100-nanosecond intervals, and make it relative
        SetWaitableTimer(timer, &due_time, 0, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    } else {
        // Fallback to the older method if timer creation fails
        yasp_usleep(final_usec);
    }
#else
    usleep(final_usec);
#endif
}
