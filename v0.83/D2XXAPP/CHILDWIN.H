//----------------------------------------------------------------------------
#ifndef ChildWinH
#define ChildWinH
//----------------------------------------------------------------------------
#include <vcl\Controls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\Classes.hpp>
#include <vcl\Windows.hpp>
#include <vcl\System.hpp>
#include <StdCtrls.hpp>
#include <ExtCtrls.hpp>
//----------------------------------------------------------------------------
#include "Main.h"
//----------------------------------------------------------------------------
#define NUMBER_OF_ROWS      50
#define NUMBER_OF_COLUMNS   80

#define FT_REC_COUNT_MAX    10
//----------------------------------------------------------------------------
class TMDIChild : public TForm
{
__published:
	TMemo *Memo1;
    TPanel *Panel1;
    TLabel *RtsLabel;
    TLabel *DtrLabel;
    TLabel *CtsLabel;
    TLabel *DsrLabel;
    TLabel *DcdLabel;
    TLabel *RingLabel;
    TTimer *ReadTimer;
    TPanel *Panel2;
    TLabel *RecLabel;
    void __fastcall FormActivate(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall RtsLabelClick(TObject *Sender);
    void __fastcall DtrLabelClick(TObject *Sender);
    void __fastcall RecLabelClick(TObject *Sender);
    void __fastcall OnKeyPress(TObject *Sender, char &Key);
    void __fastcall OnTimer(TObject *Sender);
    void __fastcall PutChar(char c);
    void __fastcall DisplayModemStatus(PVOID pDev);
    void __fastcall Memo1DblClick(TObject *Sender);
    void __fastcall PutStr(char *s,DWORD len);
    void __fastcall Flush(void);
private:
    int CurRow;
    int CurCol;
    int MemoBufIndex;
    char MemoBuf[256];
public:
    TFtDevice *ftDev;
	virtual __fastcall TMDIChild(TComponent *Owner);
	virtual __fastcall TMDIChild(String Name);
};
//----------------------------------------------------------------------------
#endif
