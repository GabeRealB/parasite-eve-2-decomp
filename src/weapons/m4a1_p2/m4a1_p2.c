#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"

/// Package header word 0: the overlay id, `item id - 0x78`
/// (item 0x94, M4A1(+2)).
const s32 D_m4a1_p2_8011D1C0 = 0x1C;

void func_m4a1_p2_8011D1C4(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* tmp;
    s16            anim;
    s32            fade;
    s32            count;
    s32            state;

    tmp                              = (GsCOORDINATE2*)(*(u8**)G_SCRATCH_HEAD - 0x50);
    *(GsCOORDINATE2**)G_SCRATCH_HEAD = tmp;
    actor                            = arg0->actor;
    coord                            = (GsCOORDINATE2*)arg0->extra->field_8;

    state = actor->field_95E;

    switch (state) {
        case 0:
            actor->field_956 = 4;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_95E = 1;
            actor->field_981 = 0;
            actor->field_934 = 0;
            actor->field_979 = 9;
            anim             = 1;
            if (actor->field_97F == 1) {
                anim = 3;
            }
            actor->field_93E = anim;
            func_80106238(arg0, 0, actor->field_97F == 1);
            fade              = 1;
            actor->field_12A |= 0xC00;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                fade = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, fade);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            if (actor->field_93E != 0) {
                if (actor->field_934 == 0) {
                    actor->field_93E  = actor->field_93E - 1;
                    actor->field_934  = 3;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x94, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20150004, 1);
                    Gp_SpawnEff(0x6006B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                0x15, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                    break;
                }
                count            = actor->field_934 - 1;
                actor->field_934 = count;
                if (count != state) {
                    break;
                }
                actor->field_12A &= 0x3FFF;
                if (Gp_PickNearestRec18(actor->field_32C, coord, tmp) != 0) {
                    Gp_PlayObjSfx((GpObj38*)tmp, 0x17, 1);
                }
            } else {
                actor->field_95E  = 3;
                actor->field_12A &= 0x3FFF;
                if (Gp_PickNearestRec18(actor->field_32C, coord, tmp) != 0) {
                    Gp_PlayObjSfx((GpObj38*)tmp, 0x17, 1);
                }
            }
            break;
        case 3:
            if (actor->field_979 != 0) {
                actor->field_979 = actor->field_979 - 1;
            }
            if ((func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) ||
                (((actor->field_962 & actor->field_96A) != 0) && (actor->field_979 == 0))) {
                actor->field_940 = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x50;
}
