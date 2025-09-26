//----------------------------------------------------------------------------
#ifndef D2XXIncsH
#define D2XXIncsH
//----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
//----------------------------------------------------------------------------
#include "FTD2XX.h"
#include "Main.h"
#include "Config.h"
#include "ChildWin.h"
//----------------------------------------------------------------------------
#define NUMBER_OF_DEVICES   4
//----------------------------------------------------------------------------
typedef struct _TFtDevice {
    TMDIChild *Window;
    TFtConfigData Data;
    FT_HANDLE Handle;
    FILE *RxFile;
    int RecCount;
} TFtDevice;
extern TFtDevice Devices[];
extern TFtDevice *CurDev;
//----------------------------------------------------------------------------
#endif
