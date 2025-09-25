//---------------------------------------------------------------------------
#ifndef ConfigH
#define ConfigH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Mask.hpp>
//---------------------------------------------------------------------------
typedef struct _TFtConfigData {
    int BaudRate;
    int DataBits;
    int StopBits;
    int Parity;
    int FlowControl;
    UCHAR Xon;
    UCHAR Xoff;
    UCHAR EventChar;
    UCHAR ErrorChar;
    BOOL EventCharEnabled;
    BOOL ErrorCharEnabled;
    BOOL RtsHi;
    BOOL DtrHi;
    BOOL CtsHi;
    BOOL DsrHi;
    BOOL DcdHi;
    BOOL RingHi;
    DWORD RxTimeout;
    DWORD TxTimeout;
    DWORD EventMask;
} TFtConfigData;
//---------------------------------------------------------------------------
enum {
    FT_INDEX_BAUD_300 = 0,
    FT_INDEX_BAUD_600,
    FT_INDEX_BAUD_1200,
    FT_INDEX_BAUD_2400,
    FT_INDEX_BAUD_4800,
    FT_INDEX_BAUD_9600,
    FT_INDEX_BAUD_14400,
    FT_INDEX_BAUD_19200,
    FT_INDEX_BAUD_38400,
    FT_INDEX_BAUD_57600,
    FT_INDEX_BAUD_115200,
    FT_INDEX_BAUD_230400,
    FT_INDEX_BAUD_460800,
    FT_INDEX_BAUD_921600,

    FT_INDEX_DATA_BITS_7 = 0,
    FT_INDEX_DATA_BITS_8,

    FT_INDEX_STOP_BITS_1 = 0,
    FT_INDEX_STOP_BITS_2,

    FT_INDEX_PARITY_NONE = 0,
    FT_INDEX_PARITY_ODD,
    FT_INDEX_PARITY_EVEN,
    FT_INDEX_PARITY_MARK,
    FT_INDEX_PARITY_SPACE,

    FT_INDEX_FLOW_NONE = 0,
    FT_INDEX_FLOW_RTS_CTS,
    FT_INDEX_FLOW_DTR_DSR,
    FT_INDEX_FLOW_XON_XOFF
};

//---------------------------------------------------------------------------
class TConfigForm : public TForm
{
__published:	// IDE-managed Components
    TGroupBox *OptionsGB;
    TComboBox *BaudRateCB;
    TComboBox *DataBitsCB;
    TComboBox *StopBitsCB;
    TComboBox *ParityCB;
    TLabel *Label1;
    TLabel *Label2;
    TLabel *Label3;
    TLabel *Label4;
    TGroupBox *FlowControlGB;
    TComboBox *FlowControlCB;
    TLabel *Label5;
    TCheckBox *RtsCB;
    TCheckBox *DtrCB;
    TGroupBox *SpecialCharsCB;
    TMaskEdit *XonME;
    TMaskEdit *XoffME;
    TLabel *Label6;
    TLabel *Label7;
    TMaskEdit *EventCharME;
    TMaskEdit *ErrorCharME;
    TLabel *EventCharLabel;
    TLabel *ErrorCharLabel;
    TCheckBox *EventCharCB;
    TCheckBox *ErrorCharCB;
    TButton *Button1;
    TButton *Button2;
    TGroupBox *TimeoutsGB;
    TMaskEdit *RxTimeoutME;
    TMaskEdit *TxTimeoutME;
    TLabel *Label8;
    TLabel *Label9;
    TGroupBox *EventsGB;
    TCheckBox *RxCharCB;
    TCheckBox *ModemStatCB;
    void __fastcall EventCharCBClick(TObject *Sender);
    void __fastcall ErrorCharCBClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TConfigForm(TComponent* Owner);
    void __fastcall Init(TFtConfigData *Data);
    void __fastcall Get(TFtConfigData *Data);
};
//---------------------------------------------------------------------------
extern PACKAGE TConfigForm *ConfigForm;
//---------------------------------------------------------------------------
#endif
