#include "common.h"
#include "main/session.h"

extern u8 D_8007216D;

void func_dryfield_night_motel_loft_8017D7EC(u8 arg0)
{
    gGameSession->at4.loc.room = arg0;
    D_8007216D                 = arg0;
}
