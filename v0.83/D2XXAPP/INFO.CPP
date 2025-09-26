//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "Info.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TInfoForm *InfoForm;
//---------------------------------------------------------------------------
__fastcall TInfoForm::TInfoForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TInfoForm::Display(char *Msg)
{
    Text->Caption = Msg;
    ShowModal();
}
//---------------------------------------------------------------------------

