//---------------------------------------------------------------------------
#ifndef txthrdH
#define txthrdH
//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------
class TFtTxThread : public TThread
{
private:
protected:
    void __fastcall Execute();
public:
    __fastcall TFtTxThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
