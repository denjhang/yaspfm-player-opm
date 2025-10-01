//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
//---------------------------------------------------------------------
USEFORM("Main.cpp", MainForm);
USEFORM("ChildWin.cpp", MDIChild);
USERES("d2xxapp.res");
USEFORM("about.cpp", AboutBox);
USEFORM("Config.cpp", ConfigForm);
USEFORM("Info.cpp", InfoForm);
USEFORM("Error.cpp", ErrorForm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	Application->Initialize();
	Application->CreateForm(__classid(TMainForm), &MainForm);
         Application->CreateForm(__classid(TAboutBox), &AboutBox);
         Application->CreateForm(__classid(TConfigForm), &ConfigForm);
         Application->CreateForm(__classid(TInfoForm), &InfoForm);
         Application->CreateForm(__classid(TErrorForm), &ErrorForm);
         Application->Run();

	return 0;
}
//---------------------------------------------------------------------
