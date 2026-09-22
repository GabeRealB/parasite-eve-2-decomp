#include "common.h"

#include "actors/actor_113000.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// The actor's three texture records, one per mode of the message-0x7E0
/// handler. Each is a lone `GpImgRec` whose 0x20x0x10 source rect repeats the
/// size the scratch `RECT` carries; the three sit 0x420 bytes apart in the
/// overlay's data segment.
extern GpImgRec D_actor_113000_8013A32C;
extern GpImgRec D_actor_113000_8013A74C;
extern GpImgRec D_actor_113000_8013AB6C;

/// Texture-upload state: runs the countdown at `field_4C2` down one a frame
/// while `field_4C4` names the upload step in progress, and on the frame it
/// underflows posts that step's image over the 0x20x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4C0` and advancing `field_4C4` for
/// steps 1 and 2, or clearing it and starting over for step 3. Steps 1 and 2
/// share their whole tail, which is what makes the compiler emit one copy of
/// it that step 1 jumps into; step 3 only differs in clearing the step
/// instead of advancing it.
void func_actor_113000_80131E30(GpActorWork* arg0)
{
    Actor113000Work* work;
    RECT             rect;

    work   = (Actor113000Work*)arg0->actor;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x20;
    rect.h = 0x10;

    switch (work->field_4C4) {
        case 1:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013AB6C, &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 2:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A74C, &rect);
                work->field_4C2 = work->field_4C0;
                work->field_4C4 = work->field_4C4 + 1;
            }
            break;
        case 3:
            work->field_4C2 = work->field_4C2 - 1;
            if ((s16)work->field_4C2 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_113000_8013A32C, &rect);
                work->field_4C4 = 0;
            }
            break;
    }
}
INCLUDE_RODATA("actors/nonmatchings/actor_113000/actor_113000", ActorsShared80138404Table);
