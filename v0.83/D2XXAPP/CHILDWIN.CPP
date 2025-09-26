//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <stdlib.h>
#include "ChildWin.h"
//---------------------------------------------------------------------
#include "Main.h"
//---------------------------------------------------------------------
#pragma resource "*.dfm"
//---------------------------------------------------------------------
#define FT_COLOR_HI     clGreen
#define FT_COLOR_LO     clRed
//---------------------------------------------------------------------

__fastcall TMDIChild::TMDIChild(TComponent *Owner)
	: TForm(Owner)
{
    ReadTimer->Enabled = true;

    Memo1->Lines->Clear();

    CurRow = 0;
    CurCol = 0;
}
//---------------------------------------------------------------------

__fastcall TMDIChild::TMDIChild(String Name)
	: TForm(Application)
{
    Caption = Name;

    int DevNum = atoi((Name.c_str())+3);
    ftDev = &Devices[DevNum];

    CurRow = 0;
    CurCol = 0;

    Memo1->Lines->Clear();

    ReadTimer->Enabled = true;
}
//---------------------------------------------------------------------

void __fastcall TMDIChild::FormActivate(TObject *Sender)
{
    if (ftDev)
        DisplayModemStatus(&ftDev->Data);

    CurDev = ftDev;
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::FormClose(TObject *Sender, TCloseAction &Action)
{
	Action = caFree;
    FT_Close(ftDev->Handle);
    ftDev->Window = NULL;

    CurDev = NULL;
}
//---------------------------------------------------------------------

void __fastcall TMDIChild::FormShow(TObject *Sender)
{
    RtsLabel->Font->Color = ConfigForm->RtsCB->Checked ? FT_COLOR_HI : FT_COLOR_LO;
    DtrLabel->Font->Color = ConfigForm->DtrCB->Checked ? FT_COLOR_HI : FT_COLOR_LO;
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::RtsLabelClick(TObject *Sender)
{
    RtsLabel->Font->Color =
        RtsLabel->Font->Color == FT_COLOR_LO ? FT_COLOR_HI : FT_COLOR_LO;
    if (RtsLabel->Font->Color == FT_COLOR_HI)
        FT_SetRts(ftDev->Handle);
    else
        FT_ClrRts(ftDev->Handle);
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::DtrLabelClick(TObject *Sender)
{
    DtrLabel->Font->Color =
        DtrLabel->Font->Color == FT_COLOR_LO ? FT_COLOR_HI : FT_COLOR_LO;
    if (DtrLabel->Font->Color == FT_COLOR_HI)
        FT_SetDtr(ftDev->Handle);
    else
        FT_ClrDtr(ftDev->Handle);
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::RecLabelClick(TObject *Sender)
{
    if (RecLabel->Visible) {
        RecLabel->Visible = false;
        MainForm->CloseFileItemClick(Sender);
    }
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::OnKeyPress(TObject *Sender, char &Key)
{
    ULONG BytesWritten;

    FT_Write(ftDev->Handle,&Key,1,&BytesWritten);

    Key = 0;
}
//---------------------------------------------------------------------------

#if 0
void __fastcall TMDIChild::OnTimer(TObject *Sender)
{
    char Buf[256];
    DWORD Status = 0;
    DWORD BytesRead = 0;
    DWORD RxBytes = 0;
    DWORD EventDWord = 0;

    if (CurDev->RxFile) {
        if (++CurDev->RecCount == FT_REC_COUNT_MAX) {
            CurDev->RecCount = 0;
            RecLabel->Visible = RecLabel->Visible ? false : true;
        }
    }
    else {
        RecLabel->Visible = false;
    }

#if 0
    FT_GetEventStatus(CurDev->Handle,&EventDWord);

    if (EventDWord & FT_EVENT_MODEM_STATUS) {

        FT_GetModemStatus(CurDev->Handle,&Status);
	    Status &= 0x000000ff;

        if (Status & 0x00000010)
        	CurDev->Data.CtsHi = true;
        else
        	CurDev->Data.CtsHi = false;

        if (Status & 0x00000020)
        	CurDev->Data.DsrHi = true;
        else
        	CurDev->Data.DsrHi = false;

        DisplayModemStatus();

    }

    FT_GetQueueStatus(CurDev->Handle,&RxBytes);
#else
    DWORD TxBytes;

    FT_GetStatus(CurDev->Handle,&RxBytes,&TxBytes,&EventDWord);

    if (EventDWord & FT_EVENT_MODEM_STATUS) {

        FT_GetModemStatus(CurDev->Handle,&Status);

        if (Status & 0x00000010)
        	CurDev->Data.CtsHi = true;
        else
        	CurDev->Data.CtsHi = false;

        if (Status & 0x00000020)
        	CurDev->Data.DsrHi = true;
        else
        	CurDev->Data.DsrHi = false;

        CurDev->Window->DisplayModemStatus();

    }
#endif

    while (RxBytes) {
        FT_ExRead(CurDev->Handle,Buf,256,&BytesRead);
        RxBytes -= BytesRead;
        if (!CurDev->RxFile) {
            char *s = Buf;
            while (BytesRead--)
                CurDev->Window->PutChar(*s++);
        }
        else {
            fwrite(Buf,1,BytesRead,CurDev->RxFile);
        }
    }

}
#else
void __fastcall TMDIChild::OnTimer(TObject *Sender)
{
    char Buf[256];
    DWORD Status = 0;
    DWORD BytesRead = 0;
    DWORD RxBytes = 0;
    DWORD EventDWord = 0;
    BOOL gotIt = FALSE;

    TFtDevice *Dev;
    TMDIChild *W;

    for (Dev = Devices; Dev < &Devices[NUMBER_OF_DEVICES]; Dev++) {

        W = (TMDIChild *) Dev->Window;

        if (!W)
            continue;

        if (W->ReadTimer == Sender) {
            gotIt = TRUE;
            break;

       }

    }

    if (!gotIt)
        return;

    if (Dev->RxFile) {
        if (++Dev->RecCount == FT_REC_COUNT_MAX) {
            Dev->RecCount = 0;
            W->RecLabel->Visible = W->RecLabel->Visible ? false : true;
        }
    }
    else {
        W->RecLabel->Visible = false;
    }

    DWORD TxBytes;

    FT_GetStatus(Dev->Handle,&RxBytes,&TxBytes,&EventDWord);

//  if (EventDWord & FT_EVENT_MODEM_STATUS) {

        FT_GetModemStatus(Dev->Handle,&Status);

        if (Status & 0x00000010)
        	Dev->Data.CtsHi = true;
        else
       	    Dev->Data.CtsHi = false;

        if (Status & 0x00000020)
           	Dev->Data.DsrHi = true;
        else
       	    Dev->Data.DsrHi = false;

        W->DisplayModemStatus(Dev);

//  }

    if (RxBytes == 0) {
        W->Flush();
        return; //continue;
    }

    while (RxBytes > 0) {
        FT_Read(Dev->Handle,Buf,RxBytes < 256 ? RxBytes : 256,&BytesRead);
        RxBytes -= BytesRead;
        if (!Dev->RxFile) {
#if 0
            char *s = Buf;
            while (BytesRead--)
                W->PutChar(*s++);
#else
            W->PutStr(Buf,BytesRead);
            W->Flush();
#endif
        }
        else {
            fwrite(Buf,1,BytesRead,Dev->RxFile);
        }
    }

}
#endif
//---------------------------------------------------------------------------

void __fastcall TMDIChild::PutChar(char c)
{
    if (c == 13 /*|| c == 10*/)
        CurCol = NUMBER_OF_COLUMNS-1;
    else if (c < 0x20)
        return;
    else {
        char buf[2];
        buf[0] = c;
        buf[1] = 0;
        Memo1->Text = Memo1->Text + buf;
        Memo1->SelStart = CurRow*NUMBER_OF_COLUMNS + CurCol + 1;
        Memo1->SelLength = 1;
    }

    if (++CurCol == NUMBER_OF_COLUMNS) {
        ++CurRow;
        CurCol = 0;
        Memo1->Lines->Add("");
        Memo1->SelStart = CurRow*NUMBER_OF_COLUMNS + CurCol + 1;
        Memo1->SelLength = 1;
    }

}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::PutStr(char *s,DWORD len)
{
    char c;

    MemoBufIndex = 0;

    while (len--) {

        c = *s++;

        if (c == 13)
            CurCol = NUMBER_OF_COLUMNS-1;
        else if (c < 0x20)
            continue;
        else {
            MemoBuf[MemoBufIndex++] = c;
        }

        if (++CurCol == NUMBER_OF_COLUMNS) {
            MemoBuf[MemoBufIndex] = 0;
            Memo1->Text = Memo1->Text + MemoBuf;
            MemoBufIndex = 0;
            ++CurRow;
            CurCol = 0;
            Memo1->Lines->Add("");
            Memo1->SelStart = CurRow*NUMBER_OF_COLUMNS + CurCol + 1;
            Memo1->SelLength = 1;
        }

    }

}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::Flush(void)
{
    MemoBuf[MemoBufIndex] = 0;
    Memo1->Text = Memo1->Text + MemoBuf;
    Memo1->SelStart = CurRow*NUMBER_OF_COLUMNS + CurCol + 1;
    Memo1->SelLength = 1;
    MemoBufIndex = 0;
}
//---------------------------------------------------------------------------

void __fastcall TMDIChild::DisplayModemStatus(PVOID pDev)
{
    TFtDevice *Dev = (TFtDevice *) pDev;

    CtsLabel->Font->Color = Dev->Data.CtsHi ? FT_COLOR_HI : FT_COLOR_LO;
    DsrLabel->Font->Color = Dev->Data.DsrHi ? FT_COLOR_HI : FT_COLOR_LO;
    DcdLabel->Font->Color = Dev->Data.DcdHi ? FT_COLOR_HI : FT_COLOR_LO;
    RingLabel->Font->Color = Dev->Data.RingHi ? FT_COLOR_HI : FT_COLOR_LO;
}
//---------------------------------------------------------------------------


void __fastcall TMDIChild::Memo1DblClick(TObject *Sender)
{
    Memo1->Clear();
    CurRow = 0;
    CurCol = 0;
}
//---------------------------------------------------------------------------

