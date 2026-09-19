#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_8013851c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

/// Effect-setup records the spawned task picks up through `D_80062730`, one per
/// random variant the roll selects - the same role `D_actor_207200_801517F8`
/// plays for actor_207200. Only the address reaches the spawn, so the contents
/// stay opaque here.
extern u8 D_actor_107000_8013C960[];
extern u8 D_actor_107000_8013CFB4[];
extern u8 D_actor_107000_8013D550[];

/// Setup-argument slot in the main executable; a task spawned through
/// `Gp_SpawnEff` reads it as its own setup argument (`D_800626EC[5].arg.model`,
/// see include/actors/actor_207200.h).
extern s32 D_80062730;

/// Ground-burst tick of the specimen. The `Gp_LcgState` draw is folded to a
/// variant and each of the three effect-setup records, with its own part of the
/// model, is spawned as effect 0x80005: variant 2 uses part 5, variant 3 part 4,
/// and variants 0 and 1 share part 1. The spawned effect is re-coloured from the
/// specimen's own palette before the tick ends by arming effect 0x60030 on parts
/// 1 and 4.
void func_actor_107000_801367E0(Task* arg0)
{
    GpEffWork* effect;
    s32        r;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    r           = (Gp_LcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_80062730 = (s32)&D_actor_107000_8013D550;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 1, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, arg0);
            }
            break;
        case 2:
            D_80062730 = (s32)&D_actor_107000_8013CFB4;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 5, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, arg0);
            }
            break;
        case 3:
            D_80062730 = (s32)&D_actor_107000_8013C960;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 4, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 1, 0x300, NULL);
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 4, 0x300, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_8", func_actor_107000_80136938);
