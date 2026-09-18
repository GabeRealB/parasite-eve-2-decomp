#include "common.h"

#include "main/wipsys.h"

void Player_InitNewGameStats(void)
{
    Player_Status.hpMax    = 0x64;
    Player_Status.hp       = 0x64;
    Player_Status.mpMax    = 0x64;
    Player_Status.mp       = 0x64;
    Player_Status.weapon   = 2;
    Player_Status.exp      = 0;
    Player_Status.field_20 = 0;
    Player_Status.field_26 = 4;
}
