#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"
extern s32 D_actor_403200_80141C54;

extern s16 D_actor_403200_80141C5A;

extern Task* D_actor_403200_8015F8F0;

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down and, once it has run out, re-arms the enemy's
/// `field_F16`. Same body as `func_actor_444000_80143E68` without its tracked
/// escort slots.
s32 func_actor_403200_80141A94(Task* arg0, s32 arg1, s32 arg2)
{
    Actor403200Work* work  = (Actor403200Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;

    switch (arg2) {
        case 0:
            work->field_F1A++;
            func_800DA6E8(&enemy->node, -0x64, 0);
            if (enemy->hp > 0) {
                enemy->hp += 0x64;
            }
            break;
        case 1:
            if (work->field_F1C > 0) {
                work->field_F1C--;
                if (work->field_F1C > 0) {
                    break;
                }
            }
            work->field_F16 = 2;
            break;
    }
    return 1;
}

s32 func_actor_403200_80141B30(void)
{
    D_actor_403200_80141C54 = 0;
    return 1;
}

/// State-change reset: once the dispatcher has flagged the change in
/// `field_4`, drop the re-arm marker and push the host model's `field_C` onto
/// every live escort's own model object. Same body as
/// `func_actor_444000_80143F4C`.
void func_actor_403200_80141B40(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    s16              i;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        escorts                          = (Actor403200Work*)arg0->work;
        work->field_7F3                  = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    } else {
        func_actor_403200_80133DD8(arg0);
    }
}
