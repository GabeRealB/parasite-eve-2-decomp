#include "common.h"

#include "actors/actor_210700.h"

#include "gameplay/3A34.h"

extern GpImgRec D_actor_210700_80157F4C;
extern GpImgRec D_actor_210700_8015826C;
extern GpImgRec D_actor_210700_8015858C;

/// Texture-upload state of the actor: runs the countdown at
/// `Actor210700Work::field_53A` down one a frame while `field_53C` names the
/// upload in progress, and on the frame it underflows posts that step's image
/// over the 0x18x0x10 rect at y 0x28 -- reloading the countdown from
/// `field_538` and advancing `field_53C` for steps 1 and 2, or clearing it and
/// starting over for step 3. Steps 1 and 2 share their whole tail, which is
/// what makes the compiler emit one copy of it that step 1 jumps into; step 3
/// only differs in clearing the step instead of advancing it.
void func_actor_210700_80149E30(GpActorWork* arg0)
{
    Actor210700Work* work;
    RECT             rect;

    work   = (Actor210700Work*)arg0->actor;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_53C) {
        case 1:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015858C, &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 2:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015826C, &rect);
                work->field_53A = work->field_538;
                work->field_53C = work->field_53C + 1;
            }
            break;
        case 3:
            work->field_53A = work->field_53A - 1;
            if ((s16)work->field_53A < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_80157F4C, &rect);
                work->field_53C = 0;
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_210700/actor_210700", D_actor_210700_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_210700/actor_210700", ActorsShared80138404Table);
