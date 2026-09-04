#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "weapons/m249.h"

void func_m249_8011D1DC(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    M249Scratch*   scratch;
    s32            anim;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - 0x68;
        scratch                 = (M249Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    actor = arg0->actor;
    coord = arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_95A  = 2;
            actor->field_954  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            actor->field_12A |= 0xC00;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
        fire:
            actor->field_95E = 3;
            actor->field_940 = 0;
            actor->field_981 = 0;
            actor->field_934 = 2;
            func_80106238(arg0, 0, actor->field_97F != 1);
            /* fallthrough */
        case 3:
            if (--actor->field_934 == 0) {
                actor->field_95E++;
                actor->field_12A |= 0xC000;
                Gp_ConsumeSlotQty(0x90, 1);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20110004, 1);
                Gp_SpawnEff(0x6006B,
                            (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 0x11,
                            NULL);
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 2);
            }
            break;
        case 4:
            spot             = &scratch->coord;
            actor->field_973 = 0;
            actor->field_95E++;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
            }
            /* fallthrough */
        case 5:
            if ((s8)func_801060E0(arg0) != 0 && func_80106264(1) > 0) {
                goto fire;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x68;
}
