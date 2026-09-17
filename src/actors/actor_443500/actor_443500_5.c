#include "common.h"

#include "actors/actor_443500.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation preset handler of message 0x7D3, the twenty-slot twin of
/// `func_actor_335800_801632A4`: when the preset's bank index changes the slot
/// array is re-seeded off bank table `D_actor_443500_80158724` through
/// `func_800B3F84`, then the preset's `field_4` is latched into `field_475` and
/// every slot 1..0x13 is either started -- through `func_800B4114`, the path
/// `field_8` selects and the only one that reads `field_C`, taken only once
/// `field_474` has been raised -- or cleared through `Gp_AnimResetSlot`; either
/// way all of them are advanced once by `Gp_AnimTickIndex`. The trailing store
/// raises the `field_474` latch the start branch above reads and clears
/// `field_4BA`.
s32 func_actor_443500_801327E0(Task* task, s32 anim, GpAnimArg* params, s32 arg3)
{
    Actor443500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor443500Work*)task->idMap;
    ext  = task->extra;
    if (params->field_0 != work->field_476) {
        work->field_476 = params->field_0;
        func_800B3F84(&work->anim, D_actor_443500_80158724[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    work->field_475 = params->field_4;
    if (params->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, params->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    work->field_4BA = 0;
    return 0;
}
