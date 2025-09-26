//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "Main.h"
#include "About.h"
#include "Info.h"
#include "Error.h"
#include "ChildWin.h"
//---------------------------------------------------------------------------
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
TFtDevice Devices[NUMBER_OF_DEVICES];
TFtDevice *CurDev = NULL;
//---------------------------------------------------------------------------

__fastcall TMainForm::TMainForm(TComponent *Owner)
	: TForm(Owner)
{
    for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
        Devices[i].Window = NULL;
        Devices[i].RxFile = NULL;
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::HelpAbout1Execute(TObject *Sender)
{
	AboutBox->ShowModal();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::PortExit1Execute(TObject *Sender)
{
	Close();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::DEV0ItemExecute(TObject *Sender)
{
	if (!OpenPort(DEV0Item->Caption)) {
        ErrorForm->Display("Can't open DEV0");
        return;
    }

  	CreateMDIChild(DEV0Item->Caption);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::DEV1ItemClick(TObject *Sender)
{
	if (!OpenPort(DEV1Item->Caption)) {
        ErrorForm->Display("Can't open DEV1");
        return;
    }

   	CreateMDIChild(DEV1Item->Caption);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ConfigureItemClick(TObject *Sender)
{
    ConfigForm->Init(&CurDev->Data);

    if (ConfigForm->ShowModal() == mrOk) {
        ConfigForm->Get(&CurDev->Data);
        SetPort(CurDev->Handle,&CurDev->Data);
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SendFileItemClick(TObject *Sender)
{
    if (OpenDialog->Execute()) {

        FILE *f = fopen(OpenDialog->FileName.c_str(),"rb");
        if (f == NULL)
            return;

        char Buf[256];
        ULONG bytesRead;
        ULONG bytesWritten;

        while (1) {
            char *s;
            bytesRead = fread(Buf,1,256,f);
            if (bytesRead == 0)
                break;
            s = Buf;
            while (bytesRead) {
                bytesWritten = 0;
                FT_Write(CurDev->Handle,s,bytesRead,&bytesWritten);
                if (bytesWritten == 0)
                    WhirlRcv();
                s += bytesWritten;
                bytesRead -= bytesWritten;
            }
        }

        fclose(f);

    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ReceiveFileItemClick(TObject *Sender)
{
    if (OpenDialog->Execute()) {

        FILE *f = fopen(OpenDialog->FileName.c_str(),"wb");
        if (f == NULL)
            return;

        CurDev->RxFile = f;

        CloseFileItem->Enabled = true;

    }

}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CloseFileItemClick(TObject *Sender)
{
    fclose(CurDev->RxFile);
    CurDev->RxFile = NULL;
    CloseFileItem->Enabled = false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CreateMDIChild(String Name)
{
	TMDIChild *Child;

	//--- create a new MDI child window ----
	Child = new TMDIChild(Name);

    CurDev = (TFtDevice *) Child->ftDev;
    CurDev->Window = (PVOID) Child;
}
//---------------------------------------------------------------------------

BOOL __fastcall TMainForm::OpenPort(String Name)
{
    FT_HANDLE ftHandle;
    FT_STATUS ftStatus;
	ULONG Status;

    int DevNum = atoi((Name.c_str())+3);

    TFtDevice *Dev = &Devices[DevNum];

    if (Dev->Window)
        return false;

    ftStatus = FT_Open(DevNum,&ftHandle);
    if (!FT_SUCCESS(ftStatus))
        return false;

    Dev->Data.BaudRate = FT_INDEX_BAUD_115200;
    Dev->Data.DataBits = FT_INDEX_DATA_BITS_8;
    Dev->Data.StopBits = FT_INDEX_STOP_BITS_1;
    Dev->Data.Parity = FT_INDEX_PARITY_NONE;
    Dev->Data.FlowControl = FT_INDEX_FLOW_NONE;
    Dev->Data.RtsHi = true;
    Dev->Data.DtrHi = true;
    Dev->Data.Xon = 0x11;
    Dev->Data.Xoff = 0x13;
    Dev->Data.EventChar = 0;
    Dev->Data.ErrorChar = 0;
    Dev->Data.EventCharEnabled = false;
    Dev->Data.ErrorCharEnabled = false;
    Dev->Data.CtsHi = false;
    Dev->Data.DsrHi = false;
    Dev->Data.DcdHi = false;
    Dev->Data.RingHi = false;
    Dev->Data.RxTimeout = FT_DEFAULT_RX_TIMEOUT;
    Dev->Data.TxTimeout = FT_DEFAULT_TX_TIMEOUT;
    Dev->Data.EventMask = 0;

    if (!SetPort(ftHandle,&Dev->Data))
        goto close;

    ftStatus = FT_GetModemStatus(ftHandle,&Status);
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    if (Status & 0x00000010)
    	Dev->Data.CtsHi = true;
    else
    	Dev->Data.CtsHi = false;

    if (Status & 0x00000020)
    	Dev->Data.DsrHi = true;
    else
    	Dev->Data.DsrHi = false;

    Dev->Handle = ftHandle;

    return true;

close:
    FT_Close(ftHandle);

    return false;
}
//---------------------------------------------------------------------------

BOOL __fastcall TMainForm::SetPort(FT_HANDLE ftHandle,TFtConfigData *ftData)
{
    ULONG BaudRate;
    UCHAR WordLength;
    UCHAR StopBits;
    USHORT FlowControl;
    UCHAR XonChar;
    UCHAR XoffChar;
    FT_STATUS ftStatus;

	ftStatus = FT_ResetDevice(ftHandle);
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    switch (ftData->BaudRate) {
    case 0 :
		BaudRate = 300;
        break;
    case 1 :
		BaudRate = 600;
        break;
    case 2 :
		BaudRate = 1200;
        break;
    case 3 :
		BaudRate = 2400;
        break;
    case 4 :
		BaudRate = 4800;
        break;
    case 5 :
		BaudRate = 9600;
        break;
    case 6 :
		BaudRate = 14400;
        break;
    case 7 :
		BaudRate = 19200;
        break;
    case 8 :
		BaudRate = 38400;
        break;
    case 9 :
		BaudRate = 57600;
        break;
    case 10 :
    	BaudRate = 115200;
        break;
    case 11 :
    	BaudRate = 230400;
        break;
    case 12 :
    	BaudRate = 460800;
        break;
    case 13 :
    	BaudRate = 921600;
        break;
    default :
    	BaudRate = 115200;
        break;
	}

	ftStatus = FT_SetBaudRate(ftHandle,BaudRate);
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    WordLength = (UCHAR) (ftData->DataBits + FT_BITS_7);

    switch (ftData->StopBits) {
    default :
        StopBits = FT_STOP_BITS_1;
        break;
    case 1 :
        StopBits = FT_STOP_BITS_2;
        break;
    }

    ftStatus = FT_SetDataCharacteristics(
                    ftHandle,
                    WordLength,
                    StopBits,
                    (UCHAR) ftData->Parity
                    );
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    FlowControl = FT_FLOW_NONE;
    XonChar = 0;
    XoffChar = 0;

    switch (ftData->FlowControl) {

    case 0 :	// NONE
    default :
        break;

	case 1 :	// RTS
        FlowControl = FT_FLOW_RTS_CTS;
    	break;

	case 2 :	// DTR
        FlowControl = FT_FLOW_DTR_DSR;
    	break;

	case 3 :	// XON/XOFF
        FlowControl = FT_FLOW_XON_XOFF;
        XonChar = ftData->Xon;
        XoffChar = ftData->Xoff;
    	break;

    }

    ftStatus = FT_SetFlowControl(
                    ftHandle,
                    FlowControl,
                    XonChar,
                    XoffChar
                    );
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    ftStatus = FT_SetChars(
                    ftHandle,
                    ftData->EventChar,
                    ftData->EventCharEnabled ? (UCHAR) 1 : (UCHAR) 0,
                    ftData->ErrorChar,
                    ftData->ErrorCharEnabled ? (UCHAR) 1 : (UCHAR) 0
                    );
    if (!FT_SUCCESS(ftStatus)) {
        goto close;
    }

    FT_SetEventNotification(ftHandle,ftData->EventMask,NULL);

    if (ftData->DtrHi) {
    	FT_SetDtr(ftHandle);
	}
    else {
    	FT_ClrDtr(ftHandle);
    }

    if (ftData->RtsHi) {
    	FT_SetRts(ftHandle);
	}
    else {
    	FT_ClrRts(ftHandle);
    }

    FT_SetTimeouts(ftHandle,ftData->RxTimeout,ftData->TxTimeout);

    FT_Purge(ftHandle,FT_PURGE_TX | FT_PURGE_RX);

    return true;

close:

    return false;
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::EnableActionUpdate(TObject *Sender)
{
    ((TAction *)Sender)->Enabled = CurDev != NULL;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CloseFileActionUpdate(TObject *Sender)
{
    ((TAction *)Sender)->Enabled = CurDev && CurDev->RxFile;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::WhirlRcv(void)
{
    char Buf[256];
    DWORD BytesRead = 0;
    DWORD RxBytes = 0;
    DWORD TxBytes = 0;
    DWORD EventDWord = 0;
    DWORD Status = 0;

    TFtDevice *Dev;
    TMDIChild *W;

    for (Dev = Devices; Dev < &Devices[NUMBER_OF_DEVICES]; Dev++) {

        W = (TMDIChild *) Dev->Window;

        if (!W)
            continue;

        if (Dev->RxFile) {
            if (++Dev->RecCount == FT_REC_COUNT_MAX) {
                Dev->RecCount = 0;
                W->RecLabel->Visible = W->RecLabel->Visible ? false : true;
            }
        }
        else {
            W->RecLabel->Visible = false;
        }

        FT_GetStatus(Dev->Handle,&RxBytes,&TxBytes,&EventDWord);

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

        if (RxBytes == 0) {
            W->Flush();
            continue;
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

}
//---------------------------------------------------------------------------


