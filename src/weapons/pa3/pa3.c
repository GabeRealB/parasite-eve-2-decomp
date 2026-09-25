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
#include "weapons/weapon.h"

/// The PA3 and SP12 shotguns are this source built once each, and each declares
/// these values in the manifest. `WEAPON_ID` is the weapon's index (0xD and
/// 0xE), which keys the sound bank, the shot effect and the item.
/// `PA3_FIELD_979` is the value `field_979` is primed with when the shot is
/// armed.
#if !defined(WEAPON_ID) || !defined(PA3_FIELD_979)
#error "WEAPON_ID and PA3_FIELD_979 are per-package build parameters"
#endif

/// Per-frame firing state machine for the shotgun. Case 0 arms the shot -
/// clearing the recoil counters, priming the `field_979` grace at `PA3_FIELD_979` and the
/// `field_934` frame delay at 0x1F - and queues the ready animation, using the
/// long variant when the weapon was left dirty (`field_958`) or the actor is
/// flagged in `field_975`; the muzzle grip bit in `field_12A` is set only for
/// the 0xE weapon variant. Case 1 waits for that animation to reach its second
/// slot. Case 2 fires, consuming the weapon's item, playing the report and spawning the
/// flash. Case 3 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 4 runs out the
/// `field_934` delay before playing the pump-action sound, and case 5 runs out
/// the `field_979` grace and otherwise hands back to `func_80106550`.
void func_pa3_8011D1DC(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    s32            anim;
    s32            hit;

    SCRATCH_PUSH_BYTES(0x50);
    spot  = SCRATCH_HEAD(GsCOORDINATE2);
    actor = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_95E = 1;
            actor->field_979 = PA3_FIELD_979;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_981 = 0;
            actor->field_934 = 0x1F;
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0x400;
            if (Player_Status.weaponSlotItem == 0xE) {
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
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_95E++;
            actor->field_12A |= 0xC000;
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(((TmdObject*)arg0->extra)->coords,
                          ((Player_Status.weaponSlotItem - 0xD) << 0x18) | 0x20000005 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(0x600A1,
                        (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->coords,
                        (Player_Status.weaponSlotItem << 0x10) | WEAPON_ID, NULL);
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            break;
        case 3:
            actor->field_95E++;
            actor->field_12A &= 0x3FFF;
            if (Player_Status.weaponSlotItem != 0xD) {
                hit = Gp_PickNearestRec18(actor->field_32C, coord, spot);
                if (Player_Status.weaponSlotItem == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                        spot->workm.t[0] = actor->field_32C[0].point.vx;
                        spot->workm.t[1] = actor->field_32C[0].point.vy;
                        spot->workm.t[2] = actor->field_32C[0].point.vz;
                        Gp_PlayObjSfx(spot,
                                      ((Player_Status.weaponSlotItem - 0xD) << 0x18) | 0x20000004 | (WEAPON_ID << 16), 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 4:
            if (--actor->field_934 == 0) {
                actor->field_95E++;
                Gp_PlayObjSfx(((TmdObject*)arg0->extra)->coords,
                              ((Player_Status.weaponSlotItem - 0xD) << 0x18) | 0x20000002 | (WEAPON_ID << 16), 0);
            }
            /* fallthrough */
        case 5:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.characterId][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 1;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_POP_BYTES(0x50);
}
