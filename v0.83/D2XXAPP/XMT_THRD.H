//---------------------------------------------------------------------------
#ifndef xmt_thrdH
#define xmt_thrdH
//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------
class TFtTransmitFile : public TThread
{
private:
protected:
    void __fastcall Execute();
public:
    __fastcall TFtTransmitFile(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
