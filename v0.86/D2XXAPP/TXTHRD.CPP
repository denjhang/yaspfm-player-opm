//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "txthrd.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------
//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TFtTxThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
__fastcall TFtTxThread::TFtTxThread(bool CreateSuspended)
    : TThread(CreateSuspended)
{
}
//---------------------------------------------------------------------------
void __fastcall TFtTxThread::Execute()
{
    //---- Place thread code here ----
}
//---------------------------------------------------------------------------
