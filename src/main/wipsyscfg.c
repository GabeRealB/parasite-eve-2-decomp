#include "wipsys.h"

#include "main/wipsys.h"
#include "main/wipsys_types.h"

void Player_InitNewGameStats(void)
{
    gPlayerStatus.hpMax           = 0x64;
    gPlayerStatus.hp              = 0x64;
    gPlayerStatus.mpMax           = 0x64;
    gPlayerStatus.mp              = 0x64;
    gPlayerStatus.weapon          = 2;
    gPlayerStatus.exp             = 0;
    gPlayerStatus.field_20        = 0;
    gPlayerStatus.resourceVariant = 4;
}
