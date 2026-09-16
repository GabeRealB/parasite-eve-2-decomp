#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E24;

/// The three texture records the tick state's upload steps post, one per
/// `field_4D0` value -- the same trio `func_actor_511000_80132904` selects
/// from by mode. Each is a lone `GpImgRec` whose 0x18x0x10 source rect
/// repeats the size the state's scratch `RECT` carries.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000", func_actor_511000_80131E78);

/// Texture-upload state: runs the countdown at `field_4CE` down one a frame
/// while `field_4D0` names the upload in progress, and on the frame it
/// underflows posts that step's image over the 0x18x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4CC` and advancing `field_4D0` for
/// steps 1 and 2, or clearing it and starting over for step 3. Steps 1 and 2
/// share their whole tail, which is what makes the compiler emit one copy of
/// it that step 1 jumps into; step 3 only differs in clearing the step
/// instead of advancing it.
void func_actor_511000_80132048(GpActorWork* arg0)
{
    Actor511000Work2* work;
    RECT              rect;

    work   = (Actor511000Work2*)arg0->actor;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_4D0) {
        case 1:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_801472B4, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 2:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146F94, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 3:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146C74, &rect);
                work->field_4D0 = 0;
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E24);

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E3C);
