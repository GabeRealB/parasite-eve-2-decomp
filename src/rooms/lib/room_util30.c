#include "common.h"
#include "main/session.h"

extern u8 D_8007216D;

void Room_Util30(u8 arg0)
{
    gGameSession->loc.room = arg0;
    D_8007216D             = arg0;
}
