#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

void Mc_InitSaveSlotDefaults(void)
{
    Wip_SysConfig.field_1a = 0x64;
    Wip_SysConfig.field_18 = 0x64;
    Wip_SysConfig.field_1e = 0x64;
    Wip_SysConfig.field_1c = 0x64;
    Wip_SysConfig.field_21 = 2;
    Wip_SysConfig.field_8  = 0;
    Wip_SysConfig.field_20 = 0;
    Wip_SysConfig.field_26 = 4;
}
