//----------------------------------------------------------------------------
#ifndef MainH
#define MainH
//----------------------------------------------------------------------------
#include <vcl\ComCtrls.hpp>
#include <vcl\ExtCtrls.hpp>
#include <vcl\Messages.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\Dialogs.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Menus.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\Classes.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Windows.hpp>
#include <vcl\System.hpp>
#include <ActnList.hpp>
#include <ImgList.hpp>
#include <StdActns.hpp>
#include <ToolWin.hpp>
//----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "FTD2XX.h"
#include "Config.h"
#define NUMBER_OF_DEVICES   4
//----------------------------------------------------------------------------
typedef struct _TFtDevice {
//    TMDIChild *Window;
    PVOID Window;
    TFtConfigData Data;
    FT_HANDLE Handle;
    FILE *RxFile;
    int RecCount;
} TFtDevice;
extern TFtDevice Devices[];
extern TFtDevice *CurDev;
//----------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:
	TMainMenu *MainMenu1;
	TMenuItem *File1;
    TMenuItem *PortCloseItem;
	TMenuItem *Window1;
	TMenuItem *Help1;
	TMenuItem *N1;
    TMenuItem *PortExitItem;
	TMenuItem *WindowCascadeItem;
	TMenuItem *WindowTileItem;
	TMenuItem *WindowArrangeItem;
	TMenuItem *HelpAboutItem;
	TOpenDialog *OpenDialog;
	TMenuItem *WindowMinimizeItem;
        TStatusBar *StatusBar;
        TActionList *ActionList1;
        TEditCut *EditCut1;
        TEditCopy *EditCopy1;
        TEditPaste *EditPaste1;
        TAction *FileNew1;
        TAction *FileSave1;
        TAction *FileExit1;
        TAction *FileOpen1;
        TAction *FileSaveAs1;
        TWindowCascade *WindowCascade1;
        TWindowTileHorizontal *WindowTileHorizontal1;
        TWindowArrange *WindowArrangeAll1;
        TWindowMinimizeAll *WindowMinimizeAll1;
        TAction *HelpAbout1;
        TWindowClose *FileClose1;
        TWindowTileVertical *WindowTileVertical1;
        TMenuItem *WindowTileItem2;
        TToolBar *ToolBar2;
        TToolButton *ToolButton1;
        TToolButton *ToolButton2;
        TToolButton *ToolButton3;
        TToolButton *ToolButton4;
        TToolButton *ToolButton5;
        TToolButton *ToolButton6;
        TToolButton *ToolButton7;
        TToolButton *ToolButton8;
        TToolButton *ToolButton9;
        TToolButton *ToolButton10;
        TToolButton *ToolButton11;
        TImageList *ImageList1;
    TMenuItem *PortOpenItem;
    TMenuItem *DEV0Item;
    TMenuItem *DEV1Item;
    TMenuItem *DEV2Item;
    TMenuItem *DEV3Item;
    TMenuItem *Transfer1;
    TMenuItem *SendFileItem;
    TMenuItem *ReceiveFileItem;
    TMenuItem *CloseFileItem;
    TAction *EnableAction;
    TAction *ConfigureAction;
    TAction *SendFileAction;
    TAction *ReceiveFileAction;
    TAction *CloseFileAction;
    TMenuItem *N2;
    TMenuItem *ConfigureItem;
    TAction *PortCloseAction;
    TAction *Dev0OpenAction;
        void __fastcall HelpAbout1Execute(TObject *Sender);
        void __fastcall PortExit1Execute(TObject *Sender);
    void __fastcall DEV0ItemExecute(TObject *Sender);
    void __fastcall ConfigureItemClick(TObject *Sender);
    void __fastcall DEV1ItemClick(TObject *Sender);
    void __fastcall SendFileItemClick(TObject *Sender);
    void __fastcall ReceiveFileItemClick(TObject *Sender);
    void __fastcall CloseFileItemClick(TObject *Sender);
    void __fastcall EnableActionUpdate(TObject *Sender);
    void __fastcall CloseFileActionUpdate(TObject *Sender);
private:
	void __fastcall CreateMDIChild(const String Name);
	BOOL __fastcall OpenPort(const String Name);
    BOOL __fastcall SetPort(FT_HANDLE ftHandle,TFtConfigData *ftData);
    void __fastcall WhirlRcv(void);
public:
	virtual __fastcall TMainForm(TComponent *Owner);
};
//----------------------------------------------------------------------------
extern TMainForm *MainForm;
//extern TMDIChild *__fastcall MDIChildCreate(void);
//----------------------------------------------------------------------------
#endif
