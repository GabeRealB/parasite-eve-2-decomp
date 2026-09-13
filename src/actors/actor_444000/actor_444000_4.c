#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_actor_444000_80144A70;

/// Pose the fifth escort -- the seven-part model whose coordinate array hangs
/// off `field_ECC[4]` -- for the arena phase `field_7A4` names. Every part is
/// reset to the same spot (the whole body dropped by a fifth of the fight's
/// progress counter, which is floored at 0x1CC here), then the phase picks a
/// target yaw per part in `field_784` and each `field_794` walks toward its
/// target by at most `field_7A6`, which is what drives the part rotations.
/// Phase 2 targets the angles the parts are already at, so it holds the pose it
/// was handed; anything past the six it knows poses like the 0x54-step phase.
void func_actor_444000_80133010(Actor444000* task)
{
    Actor444000Work* work = task->field_1C;
    s16              i;

    if (((TmdObject*)work->field_ECC[4]->task->extra)->field_18 == NULL) {
        return;
    }

    if (D_actor_444000_80144A70 < 0x1CC) {
        D_actor_444000_80144A70 = 0x1CC;
    }

    for (i = 0; i < 7; i++) {
        ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].coord.t[0]     = ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].coord.t[1] =
            ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].coord.t[2] = 0;
        if ((u16)i >= 2) {
            ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].coord.t[2] = (s16)(D_actor_444000_80144A70 / 5);
        }
        ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i]);
    }

    switch (work->field_7A4) {
        case 4:
            work->field_7A6    = 2;
            work->field_784[1] = 0x180;
            work->field_784[2] = 0x20;
            work->field_784[3] = 0;
            work->field_784[4] = 0;
            work->field_784[5] = 0;
            work->field_784[6] = 0;
            break;

        case 3:
            work->field_7A6 = 8;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x80;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 6; i < 7; i++) {
                    work->field_784[i] = -0x60;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 5; i++) {
                    work->field_784[i] = -0x40;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x40;
                work->field_784[2] = 0;
                for (i = 3; i < 4; i++) {
                    work->field_784[i] = -0x60;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x40;
                }
            }
            break;

        case 2:
            work->field_7A6    = 0x10;
            work->field_784[1] = 0xC0;
            work->field_784[2] = 0x60;
            work->field_784[3] = 0x20;
            work->field_784[4] = work->field_794[4];
            work->field_784[5] = work->field_794[5];
            work->field_784[6] = work->field_794[6];
            break;

        case 1:
            work->field_7A6 = 0x59;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = 0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            }
            break;

        case 0:
            work->field_7A6 = 0x10;
            if (D_actor_444000_80144A70 < 0x258) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x100;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            }
            break;

        case 5:
        default:
            work->field_7A6 = 0x54;
            if (D_actor_444000_80144A70 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_444000_80144A70 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_444000_80144A70 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_444000_80144A70 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            }
            break;
    }

    for (i = 1; i < 7; i++) {
        if (abs(work->field_794[i] - work->field_784[i]) < work->field_7A6) {
            work->field_794[i] = work->field_784[i];
        } else if (work->field_794[i] < work->field_784[i]) {
            work->field_794[i] = work->field_794[i] + work->field_7A6;
        } else {
            work->field_794[i] = work->field_794[i] - work->field_7A6;
        }
        Gfx_RotMatrixX(&((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].coord, work->field_794[i], 1);
        ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[i].flg = 0;
    }
}

/// Walk the actor's stored yaw toward `arg1`, moving at most 0x71 per call and
/// clamping the request to +/-0x200, then push the new angle onto the model:
/// part 3's coordinate is rebuilt twice around the yaw helper, the fifth
/// escort's root coordinate is refreshed, and part 4 follows.
void func_actor_444000_80133C58(Actor444000* task, s16 arg1)
{
    Actor444000Work* work = task->field_1C;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    ActorsShared80132808(&((TmdObject*)task->extra)->field_8[3], work->field_7C8);
    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0]);
    ((TmdObject*)task->extra)->field_8[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[4]);
}
