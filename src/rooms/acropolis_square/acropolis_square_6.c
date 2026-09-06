#include "common.h"

#include "decomp/common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern TaskDesc D_acropolis_square_80183808;
extern s32      D_acropolis_square_80183B98;
extern s32      D_acropolis_square_80183B58;
extern s32      D_acropolis_square_80183830;
extern s32      D_acropolis_square_8018399C;
extern s32      D_acropolis_square_80183A5C;

void func_acropolis_square_801822A4(void)
{
    char pad[0x10];

    if (Mc_SaveData.field_8 == 7 && D_acropolis_square_80183830 == 0) {
        D_acropolis_square_80183830 = 1;
        Mc_SaveData.field_5C5       = 2;
        func_800E8634((s32)&D_acropolis_square_8018399C, 0, (s32)&D_acropolis_square_80183A5C);
    }
}
