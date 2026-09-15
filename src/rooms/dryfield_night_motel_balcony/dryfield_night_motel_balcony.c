#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene / among-us mode byte, `Gp_StateC08.field_A`; the gameplay map
/// names only the struct's base, so splat keeps the raw address.
extern s8 D_80114C12;

/// Cutscene script blob handed to `func_800E8614`; unnamed in the gameplay
/// map and shared by several rooms.
extern u8 D_80165720;

s32 func_dryfield_night_motel_balcony_8017DC18(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC20(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC28(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony", func_dryfield_night_motel_balcony_8017DC30);

/// One-shot balcony event: while no cutscene is running and the event flag is
/// still unset, play the motel script and mark the flag done.
void func_dryfield_night_motel_balcony_8017DD0C(Task* task)
{
    if (Game_Session->field_1 == 0 && D_80114C12 != 1 && GameFlag_GetNibble(0x10E) == 1) {
        func_800E8614((s32)&D_80165720, 0);
        GameFlag_SetNibble(0x10E, 2);
    }
}
