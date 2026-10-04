#include "wipsys.h"

#include "main/wipsys.h"

void playerSeedNewGameStatus(void)
{
    enum {
        PLAYER_NEW_GAME_HP               = 100,
        PLAYER_NEW_GAME_MP               = 100,
        PLAYER_NEW_GAME_WEAPON_M93R      = 2,
        PLAYER_NEW_GAME_RESOURCE_VARIANT = 4,
    };

    gPlayerStatus.hpMax           = PLAYER_NEW_GAME_HP;
    gPlayerStatus.hp              = PLAYER_NEW_GAME_HP;
    gPlayerStatus.mpMax           = PLAYER_NEW_GAME_MP;
    gPlayerStatus.mp              = PLAYER_NEW_GAME_MP;
    gPlayerStatus.weapon          = PLAYER_NEW_GAME_WEAPON_M93R;
    gPlayerStatus.exp             = 0;
    gPlayerStatus.field_20        = 0;
    gPlayerStatus.resourceVariant = PLAYER_NEW_GAME_RESOURCE_VARIANT;
}
