//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <stdio.h>
#include "Config.h"
#include "FTD2XX.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TConfigForm *ConfigForm;
//---------------------------------------------------------------------------
__fastcall TConfigForm::TConfigForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------//---------------------------------------------------------------------------
void __fastcall TConfigForm::Init(TFtConfigData *Data)
{
    BaudRateCB->ItemIndex = Data->BaudRate;
    DataBitsCB->ItemIndex = Data->DataBits;
    StopBitsCB->ItemIndex = Data->StopBits;
    ParityCB->ItemIndex = Data->Parity;
    FlowControlCB->ItemIndex = Data->FlowControl;

    RtsCB->Checked = Data->RtsHi;
    DtrCB->Checked = Data->DtrHi;

    EventCharCB->Checked = Data->EventCharEnabled;
    EventCharME->Enabled = EventCharCB->Checked;
    EventCharLabel->Enabled = EventCharCB->Checked;

    ErrorCharCB->Checked = Data->ErrorCharEnabled;
    ErrorCharME->Enabled = ErrorCharCB->Checked;
    ErrorCharLabel->Enabled = ErrorCharCB->Checked;

    char Buf[16];

    wsprintf(Buf,"%02.2X",Data->Xon);
    XonME->Text = Buf;

    wsprintf(Buf,"%02.2X",Data->Xoff);
    XoffME->Text = Buf;

    wsprintf(Buf,"%02.2X",Data->EventChar);
    EventCharME->Text = Buf;

    wsprintf(Buf,"%02.2X",Data->ErrorChar);
    ErrorCharME->Text = Buf;

    wsprintf(Buf,"%d",Data->RxTimeout);
    RxTimeoutME->Text = Buf;

    wsprintf(Buf,"%d",Data->TxTimeout);
    TxTimeoutME->Text = Buf;

    RxCharCB->Checked = Data->EventMask & FT_EVENT_RXCHAR;
    ModemStatCB->Checked = Data->EventMask & FT_EVENT_MODEM_STATUS;
}
//---------------------------------------------------------------------------//---------------------------------------------------------------------------
void __fastcall TConfigForm::Get(TFtConfigData *Data)
{
    Data->BaudRate = BaudRateCB->ItemIndex;
    Data->DataBits = DataBitsCB->ItemIndex;
    Data->StopBits = StopBitsCB->ItemIndex;
    Data->Parity = ParityCB->ItemIndex;
    Data->FlowControl = FlowControlCB->ItemIndex;

    Data->RtsHi = RtsCB->Checked;
    Data->DtrHi = DtrCB->Checked;

    Data->EventCharEnabled = EventCharCB->Checked;
    Data->ErrorCharEnabled = ErrorCharCB->Checked;

    int c;

    sscanf(EventCharME->Text.c_str(),"%x",&c);
    Data->EventChar = (UCHAR) c;

    sscanf(ErrorCharME->Text.c_str(),"%x",&c);
    Data->ErrorChar = (UCHAR) c;

    DWORD dw;

    sscanf(RxTimeoutME->Text.c_str(),"%d",&dw);
    Data->RxTimeout = dw;

    sscanf(TxTimeoutME->Text.c_str(),"%d",&dw);
    Data->TxTimeout = dw;

    dw = 0;

    if (RxCharCB->Checked)
        dw |= FT_EVENT_RXCHAR;

    if (ModemStatCB->Checked)
        dw |= FT_EVENT_MODEM_STATUS;

    Data->EventMask = dw;
}
//---------------------------------------------------------------------------//---------------------------------------------------------------------------

void __fastcall TConfigForm::EventCharCBClick(TObject *Sender)
{
    EventCharME->Enabled = EventCharCB->Checked;
    EventCharLabel->Enabled = EventCharCB->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TConfigForm::ErrorCharCBClick(TObject *Sender)
{
    ErrorCharME->Enabled = ErrorCharCB->Checked;
    ErrorCharLabel->Enabled = ErrorCharCB->Checked;
}
//---------------------------------------------------------------------------

