//---------------------------------------------------------------------------
#ifndef InfoH
#define InfoH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
//---------------------------------------------------------------------------
class TInfoForm : public TForm
{
__published:	// IDE-managed Components
    TLabel *Text;
    TButton *OKBtn;
    TImage *Image1;
private:	// User declarations
public:		// User declarations
    __fastcall TInfoForm(TComponent* Owner);
    void __fastcall Display(char *Msg);
};
//---------------------------------------------------------------------------
extern PACKAGE TInfoForm *InfoForm;
//---------------------------------------------------------------------------
#endif
