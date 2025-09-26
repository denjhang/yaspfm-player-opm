#ifndef FTD2XX_H
#define FTD2XX_H

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the FTD2XX_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FTD2XX_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef FTD2XX_EXPORTS
#define FTD2XX_API __declspec(dllexport)
#else
#define FTD2XX_API __declspec(dllimport)
#endif


typedef ULONG FT_HANDLE;

//
// Device status
//
enum FT_STATUS {
    FT_OK,
    FT_INVALID_HANDLE,
    FT_DEVICE_NOT_FOUND,
    FT_DEVICE_NOT_OPENED,
    FT_IO_ERROR,
    FT_INSUFFICIENT_RESOURCES,
};


#define FT_SUCCESS(status) ((status) == FT_OK)

//
// Baud Rates
//

#define FT_BAUD_300			0x0000
#define FT_BAUD_600			0x0000
#define FT_BAUD_1200		0x0000
#define FT_BAUD_2400		0x0000
#define FT_BAUD_4800		0x0000
#define FT_BAUD_9600		0x0000
#define FT_BAUD_14400		0x0000
#define FT_BAUD_19200		0x0000
#define FT_BAUD_38400		0x0000
#define FT_BAUD_57600		0x0000
#define FT_BAUD_115200		0x0000
#define FT_BAUD_230400		0x0000
#define FT_BAUD_460800		0x0000
#define FT_BAUD_921600		0x0000

//
// Word Lengths
//

#define FT_BITS_8			(UCHAR) 8
#define FT_BITS_7			(UCHAR) 7
#define FT_BITS_6			(UCHAR) 6
#define FT_BITS_5			(UCHAR) 5

//
// Stop Bits
//

#define FT_STOP_BITS_1		(UCHAR) 0
#define FT_STOP_BITS_1_5	(UCHAR) 1
#define FT_STOP_BITS_2		(UCHAR) 2

//
// Parity
//

#define FT_PARITY_NONE		(UCHAR) 0
#define FT_PARITY_ODD		(UCHAR) 1
#define FT_PARITY_EVEN		(UCHAR) 2
#define FT_PARITY_MARK		(UCHAR) 3
#define FT_PARITY_SPACE		(UCHAR) 4

//
// Flow Control
//

#define FT_FLOW_NONE        0x0000
#define FT_FLOW_RTS_CTS     0x0100
#define FT_FLOW_DTR_DSR     0x0200
#define FT_FLOW_XON_XOFF    0x0400

//
// Purge rx and tx buffers
//
#define FT_PURGE_RX         1
#define FT_PURGE_TX         2

//
// Events
//

typedef void (*PFT_EVENT_HANDLER)(DWORD,DWORD);

#define FT_EVENT_RXCHAR		    1
#define FT_EVENT_MODEM_STATUS   2

//
// Timeouts
//

#define FT_DEFAULT_RX_TIMEOUT   300
#define FT_DEFAULT_TX_TIMEOUT   300

#ifdef __cplusplus
extern "C" {
#endif


FTD2XX_API
FT_STATUS WINAPI FT_Open(
	int deviceNumber,
	FT_HANDLE *pHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_Close(
	FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_Read(
    FT_HANDLE ftHandle,
    LPVOID lpBuffer,
    DWORD nBufferSize,
    LPDWORD lpBytesReturned
    );

FTD2XX_API 
FT_STATUS WINAPI FT_Write(
    FT_HANDLE ftHandle,
    LPVOID lpBuffer,
    DWORD nBufferSize,
    LPDWORD lpBytesWritten
    );

FTD2XX_API 
FT_STATUS WINAPI FT_IoCtl(
    FT_HANDLE ftHandle,
    DWORD dwIoControlCode,
    LPVOID lpInBuf,
    DWORD nInBufSize,
    LPVOID lpOutBuf,
    DWORD nOutBufSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped
    );

FTD2XX_API
FT_STATUS WINAPI FT_SetBaudRate(
    FT_HANDLE ftHandle,
	ULONG BaudRate
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetDataCharacteristics(
    FT_HANDLE ftHandle,
	UCHAR WordLength,
	UCHAR StopBits,
	UCHAR Parity
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetFlowControl(
    FT_HANDLE ftHandle,
    USHORT FlowControl,
    UCHAR XonChar,
    UCHAR XoffChar
	);

FTD2XX_API
FT_STATUS WINAPI FT_ResetDevice(
    FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetDtr(
    FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_ClrDtr(
    FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetRts(
    FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_ClrRts(
    FT_HANDLE ftHandle
	);

FTD2XX_API
FT_STATUS WINAPI FT_GetModemStatus(
    FT_HANDLE ftHandle,
	ULONG *pModemStatus
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetChars(
    FT_HANDLE ftHandle,
	UCHAR EventChar,
	UCHAR EventCharEnabled,
	UCHAR ErrorChar,
	UCHAR ErrorCharEnabled
    );

FTD2XX_API
FT_STATUS WINAPI FT_Purge(
    FT_HANDLE ftHandle,
	ULONG Mask
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetTimeouts(
    FT_HANDLE ftHandle,
	ULONG ReadTimeout,
	ULONG WriteTimeout
	);

FTD2XX_API
FT_STATUS WINAPI FT_GetQueueStatus(
    FT_HANDLE ftHandle,
	DWORD *dwRxBytes
	);

FTD2XX_API
FT_STATUS WINAPI FT_SetEventNotification(
    FT_HANDLE ftHandle,
	DWORD Mask,
	PFT_EVENT_HANDLER Fun
	);

FTD2XX_API
FT_STATUS WINAPI FT_GetEventStatus(
    FT_HANDLE ftHandle,
    DWORD *dwEventDWord
	);

FTD2XX_API
FT_STATUS WINAPI FT_GetStatus(
    FT_HANDLE ftHandle,
    DWORD *dwRxBytes,
    DWORD *dwTxBytes,
    DWORD *dwEventDWord
	);


#ifdef __cplusplus
}
#endif


#endif  /* FTD2XX_H */
