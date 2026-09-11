#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016AC64);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016C0F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016CC9C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_27", ActorsShared80135df4Table);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2)
{
    TmdObject* obj = arg0->field_2C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->field_C |= 4;
            break;
        case 3:
            obj->field_C = 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016CEB4);
