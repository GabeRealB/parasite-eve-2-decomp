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

void func_mongoose_8011D1D8(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    s32            anim;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - 0x50;
        spot                    = (GsCOORDINATE2*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    actor = arg0->actor;
    coord = arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_95E = 1;
            anim             = 1;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_981 = 0;
            actor->field_979 = 0xB;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 5;
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
            actor->field_95E++;
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0xCC00;
            Gp_ConsumeSlotQty(0x88, 1);
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20090004, 1);
            Gp_SpawnEff(0x6006A,
                        (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 9, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 3:
            actor->field_95E++;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
            }
            /* fallthrough */
        case 4:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 0xF;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
