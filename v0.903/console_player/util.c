#include "util.h"
#include <unistd.h> // For usleep on POSIX
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <conio.h>
#else
#include <termios.h>
#include <fcntl.h>
#endif

extern volatile double g_speed_multiplier;
extern volatile int g_timer_mode;

#ifdef _WIN32
static LARGE_INTEGER g_freq;
static HANDLE g_mm_timer_event = NULL;
#endif

// Multimedia Timer callback
#ifdef _WIN32
static void CALLBACK mm_timer_callback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    SetEvent(g_mm_timer_event);
}
#endif

void yasp_timer_init(void) {
#ifdef _WIN32
    QueryPerformanceFrequency(&g_freq);
    g_mm_timer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
}

void yasp_timer_release(void) {
#ifdef _WIN32
    if (g_mm_timer_event) {
        CloseHandle(g_mm_timer_event);
        g_mm_timer_event = NULL;
    }
#endif
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

// Hybrid Sleep implementation
#ifdef _WIN32
static void hybrid_sleep_us(unsigned int usec) {
    if (usec == 0) return;

    LARGE_INTEGER start, current;
    QueryPerformanceCounter(&start);
    LONGLONG wait_ticks = (g_freq.QuadPart * usec) / 1000000;
    LONGLONG target_ticks = start.QuadPart + wait_ticks;

    // If the wait time is more than 1ms, use Sleep
    if (usec > 1000) {
        Sleep((usec - 1000) / 1000);
    }

    // Spin for the remaining time
    do {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart < target_ticks);
}
#endif

// Multimedia Timer implementation
#ifdef _WIN32
static void mm_timer_sleep_us(unsigned int usec) {
    if (usec == 0) return;
    unsigned int ms = usec / 1000;
    if (ms == 0) ms = 1; // timeSetEvent requires at least 1ms

    MMRESULT timer_id = timeSetEvent(ms, 1, (LPTIMECALLBACK)mm_timer_callback, 0, TIME_ONESHOT);
    if (timer_id != 0) {
        WaitForSingleObject(g_mm_timer_event, ms + 5); // Wait for the event with a small timeout
        timeKillEvent(timer_id);
    } else {
        // Fallback to hybrid sleep
        hybrid_sleep_us(usec);
    }
}
#endif


void yasp_usleep(uint64_t usec) {
    uint64_t final_usec = (uint64_t)(usec / g_speed_multiplier);
    if (final_usec == 0) {
        return;
    }

#ifdef _WIN32
    switch (g_timer_mode) {
        case 1: // Hybrid Sleep
            hybrid_sleep_us((unsigned int)final_usec);
            break;
        case 2: // Multimedia Timer
            mm_timer_sleep_us((unsigned int)final_usec);
            break;
        case 0: // High-Precision Sleep (Default)
        default:
            yasp_usleep_precise((unsigned int)final_usec);
            break;
    }
#else
    usleep(final_usec);
#endif
}

unsigned long yasp_get_tick_count(void) {
    return (unsigned long)(get_current_time_us() / 1000);
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
        hybrid_sleep_us(final_usec);
    }
#else
    usleep(final_usec);
#endif
}

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int get_single_char() {
#ifdef _WIN32
    return _getch();
#else
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
#endif
}
