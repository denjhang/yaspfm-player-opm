//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "xmt_thrd.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------
//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TFtTransmitFile::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
__fastcall TFtTransmitFile::TFtTransmitFile(bool CreateSuspended)
    : TThread(CreateSuspended)
{
}
//---------------------------------------------------------------------------
void __fastcall TFtTransmitFile::Execute()
{
    //---- Place thread code here ----
}
//---------------------------------------------------------------------------
