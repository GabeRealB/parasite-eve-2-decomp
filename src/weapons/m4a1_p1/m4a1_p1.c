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

/// Package header word 0: the overlay id, `item id - 0x78`
/// (item 0x93, M4A1(+1)).
const s32 D_m4a1_p1_8011D1C0 = 0x1B;

void func_m4a1_p1_8011D1C4(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    s32            anim;
    s32            delay;
    /* Narrower than the field it feeds on purpose: an `s32 shots = 1` would join
       the switch's SImode `1` in the same cse class and steal its register for
       the `field_97F == 1` compare below. */
    s16 shots;

    spot                    = (GsCOORDINATE2*)((u8*)*(void**)G_SCRATCH_HEAD - 0x50);
    *(void**)G_SCRATCH_HEAD = spot;
    actor                   = arg0->actor;
    coord                   = arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_95E = 1;
            actor->field_981 = 0;
            actor->field_934 = 0;
            actor->field_979 = 9;
            shots            = 1;
            if (actor->field_97F == 1) {
                shots = 3;
            }
            actor->field_93E = shots;
            func_80106238(arg0, 0, actor->field_97F == 1);
            actor->field_12A |= 0xC00;
            anim              = 1;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 6;
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
            if (actor->field_93E != 0) {
                delay = actor->field_934;
                if (delay == 0) {
                    actor->field_93E--;
                    actor->field_934  = 3;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x93, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20140004, 1);
                    Gp_SpawnEff(0x6006B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                0x14, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                    break;
                }
                actor->field_934 = delay - 1;
                if (delay - 1 == 2) {
                    actor->field_12A &= 0x3FFF;
                    if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                        Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                    }
                }
            } else {
                /* The lock-on block is spelled out in both arms, not shared: with
                   one copy after the `if`, cross-jumping merges the `field_12A`
                   load into the tail and drops two instructions. */
                actor->field_95E  = 3;
                actor->field_12A &= 0x3FFF;
                if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                    Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                }
            }
            break;
        case 3:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
