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
#include "main/wipsys.h"

/// Per-frame firing state machine for the AS12 automatic shotgun. Case 0 arms
/// the shot and queues the ready animation, choosing the long variant when the
/// weapon was left dirty (`field_958`) or the actor is flagged in `field_975`;
/// the muzzle-flash grip bit in `field_12A` is set only for the 0xE weapon
/// variant. Case 1 waits for that animation to reach its second slot. Case 2
/// fires - consuming ammo 0x8E, playing the report and spawning the flash -
/// and case 4 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 5 runs out the
/// `field_979` grace, re-fires while the trigger is held and otherwise hands
/// back to `func_80106550`.
void func_as12_8011D1DC(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    s32            anim;
    s32            hit;

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
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_95E++;
            actor->field_12A |= 0x400;
            if (Wip_SysConfig.field_22 == 0xE) {
                actor->field_12A |= 0x800;
            } else {
                actor->field_12A &= ~0x800;
            }
            anim = 1;
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
        fire:
            actor->field_95E = 3;
            actor->field_940 = 0x21;
            actor->field_981 = 0;
            func_80106238(arg0, 0, actor->field_97F != 1);
            /* fallthrough */
        case 3:
            actor->field_95E++;
            actor->field_12A |= 0xC000;
            Gp_ConsumeSlotQty(0x8E, 1);
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                          ((Wip_SysConfig.field_22 - 0xD) << 0x18) | 0x200F0005, 1);
            Gp_SpawnEff(0x600A1,
                        (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                        (Wip_SysConfig.field_22 << 0x10) | 0xF, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 4:
            actor->field_979 = 0x16;
            actor->field_95E++;
            actor->field_12A &= 0x3FFF;
            if (Wip_SysConfig.field_22 != 0xD) {
                hit = Gp_PickNearestRec18(actor->field_32C, coord, spot);
                if (Wip_SysConfig.field_22 == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                        spot->workm.t[0] = actor->field_32C[0].field_8;
                        spot->workm.t[1] = actor->field_32C[0].field_A;
                        spot->workm.t[2] = actor->field_32C[0].field_C;
                        Gp_PlayObjSfx((GpObj38*)spot,
                                      ((Wip_SysConfig.field_22 - 0xD) << 0x18) | 0x200F0004, 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 5:
            if ((s8)func_801060E0(arg0) != 0 && func_80106264(1) > 0 && actor->field_940 == 0) {
                goto fire;
            }
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
