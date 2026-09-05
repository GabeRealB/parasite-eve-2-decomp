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

/// Per-frame firing state machine for the SP12 shotgun. Case 0 arms the shot -
/// clearing the recoil counters, priming the `field_979` grace at 0x24 and the
/// `field_934` frame delay at 0x1F - and queues the ready animation, using the
/// long variant when the weapon was left dirty (`field_958`) or the actor is
/// flagged in `field_975`; the muzzle grip bit in `field_12A` is set only for
/// the 0xE weapon variant. Case 1 waits for that animation to reach its second
/// slot. Case 2 fires, consuming ammo 0x8D, playing the report and spawning the
/// flash. Case 3 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 4 runs out the
/// `field_934` delay before playing the pump-action sound, and case 5 runs out
/// the `field_979` grace and otherwise hands back to `func_80106550`.
void func_sp12_8011D1DC(GpActorWork* arg0)
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
            actor->field_95E = 1;
            actor->field_979 = 0x24;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_981 = 0;
            actor->field_934 = 0x1F;
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0x400;
            if (Wip_SysConfig.field_22 == 0xE) {
                actor->field_12A |= 0x800;
            } else {
                actor->field_12A &= ~0x800;
            }
            anim = 1;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            /* fallthrough */
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_95E++;
            actor->field_12A |= 0xC000;
            Gp_ConsumeSlotQty(0x8D, 1);
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                          ((Wip_SysConfig.field_22 - 0xD) << 0x18) | 0x200E0005, 1);
            Gp_SpawnEff(0x600A1,
                        (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                        (Wip_SysConfig.field_22 << 0x10) | 0xE, NULL);
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            break;
        case 3:
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
                                      ((Wip_SysConfig.field_22 - 0xD) << 0x18) | 0x200E0004, 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 4:
            if (--actor->field_934 == 0) {
                actor->field_95E++;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                              ((Wip_SysConfig.field_22 - 0xD) << 0x18) | 0x200E0002, 0);
            }
            /* fallthrough */
        case 5:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 1;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
