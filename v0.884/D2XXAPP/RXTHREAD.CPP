//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "rxthread.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------
//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TFtRxThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
__fastcall TFtRxThread::TFtRxThread(bool CreateSuspended)
    : TThread(false)
{
}
//---------------------------------------------------------------------------
void __fastcall TFtRxThread::Execute()
{
    //---- Place thread code here ----
}
//---------------------------------------------------------------------------
