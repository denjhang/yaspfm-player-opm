//---------------------------------------------------------------------------
#ifndef ErrorH
#define ErrorH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
//---------------------------------------------------------------------------
class TErrorForm : public TForm
{
__published:	// IDE-managed Components
    TButton *OKBtn;
    TLabel *Text;
    TImage *Image1;
private:	// User declarations
public:		// User declarations
    __fastcall TErrorForm(TComponent* Owner);
    void __fastcall Display(char *Msg);
};
//---------------------------------------------------------------------------
extern PACKAGE TErrorForm *ErrorForm;
//---------------------------------------------------------------------------
#endif
