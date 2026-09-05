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
/// (item 0x81, M93R).
const s32 D_m93r_8011D1C0 = 0x09;

/// Per-frame firing state machine for the M93R burst pistol. Case 0 arms the
/// shot (four-tick reload window, `field_979` grace of 10) and queues the
/// ready animation, choosing the long variant when the weapon was left dirty
/// (`field_958`) or the actor is flagged in `field_975`; a two-handed grip
/// (`field_97F == 1`) turns the single shot into a three-round burst. Case 1
/// waits for that animation to reach its second slot. Case 2 fires one round
/// per two frames - consuming ammo 0x81, playing the muzzle report and spawning
/// the flash effect - and re-acquires the lock-on target on the off frame and
/// again once the burst runs dry. Case 3 runs out the grace counter and hands
/// back to `func_80106550`, parking `field_940` at 10 when the player is still
/// holding the fire button after the grace expired and at 0 otherwise.
void func_m93r_8011D1C4(GpActorWork* arg0)
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
    /* Declared before the switch so it is initialised in the first case test's
       delay slot, as the ROM does; case 3 reads it twice. */
    s32 lockedOut;

    spot                    = (GsCOORDINATE2*)((u8*)*(void**)G_SCRATCH_HEAD - 0x50);
    *(void**)G_SCRATCH_HEAD = spot;
    actor                   = arg0->actor;
    coord                   = arg0->extra->field_8;
    lockedOut               = 0;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_95E = 1;
            actor->field_981 = 0;
            actor->field_934 = 0;
            actor->field_979 = 0xA;
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
                    actor->field_934  = 1;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x81, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20020004, 1);
                    Gp_SpawnEff(0x6002B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 2,
                                NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 2);
                    break;
                }
                /* Decrement through the local rather than storing `delay - 1`
                   and re-testing `delay - 1 == 0`: the latter keeps `delay`
                   live and turns the second test into a compare against the
                   switch's `1`. */
                delay--;
                actor->field_934 = delay;
                if (delay == 0) {
                    actor->field_12A &= 0x3FFF;
                    if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                        Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                    }
                }
            } else {
                /* Spelled out in both arms rather than shared after the `if`;
                   GCC cross-jumps the common tail itself, keeping only the
                   `field_12A` load duplicated, which is what the ROM has. */
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
            if ((actor->field_962 & actor->field_96A) != 0) {
                lockedOut = actor->field_979 == 0;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 || lockedOut) {
                if (lockedOut) {
                    actor->field_940 = 0xA;
                } else {
                    actor->field_940 = 0;
                }
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
