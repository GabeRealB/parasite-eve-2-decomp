#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8 D_8007216D;

void func_dryfield_night_saloon_g_r_8017E0A8(u8 arg0)
{
    D_8007216D            = arg0;
    Game_Session->field_5 = arg0;
}
