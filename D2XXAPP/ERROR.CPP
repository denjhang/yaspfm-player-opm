//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "Error.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TErrorForm *ErrorForm;
//---------------------------------------------------------------------------
__fastcall TErrorForm::TErrorForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TErrorForm::Display(char *Msg)
{
    Text->Caption = Msg;
    ShowModal();
}
//---------------------------------------------------------------------------
