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
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Per-frame firing state machine for the M4A1 hammer. State 0 arms the shot
/// and raises the weapon (clip 8 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: a held trigger (bit 0)
/// drops into the three-round burst of state 3, a tap (bit 1) swings the hammer
/// (state 5) after telling the hammer task (`field_914`) to go to sub-state 2,
/// and anything else falls straight into the burst. Both branches re-aim the
/// muzzle record at `field_14C`, the hammer with a 0x400 spread and the burst
/// with 0x100 plus the attach-0x19 row of `D_80112F60`. State 3 counts
/// `field_934` down to each round, spending one magazine round, playing
/// `0x20190004` and spawning the muzzle flash, and picks the lock-on target on
/// the frame after. State 4 picks that target once and hands over to state 6.
/// State 5 counts the swing out over `field_934`, flagging the wind-up at 1 and
/// releasing the hammer task at 0. State 6 counts `field_979` down and drops
/// out of the firing pose once the aim check fails or the trigger has been
/// released.
void func_m4a1_hammer_8011E710(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    GpActorD4Rec*  rec;
    Task*          hammer;
    s32            anim;
    s32            delay;
    u16            flags;

    actor                 = arg0->actor;
    coord                 = arg0->extra->field_8;
    rec                   = (GpActorD4Rec*)actor->field_14C;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD - 0x50;
    spot                  = (GsCOORDINATE2*)*(u8**)G_SCRATCH_HEAD;
    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            actor->field_12A |= 0x400;
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
            actor->field_981 = 0;
            if (actor->field_97F & 1) {
                actor->field_95E  = 3;
                actor->field_95A  = 0;
                actor->field_934  = 0;
                actor->field_979  = 9;
                actor->field_93E  = 3;
                actor->field_124  = Wip_SysConfig.field_22 | 0x21900;
                rec->field_10     = 0x100;
                rec->field_12     = 0x100;
                rec->field_4      = rec->field_C + D_80112F60[0x19];
                actor->field_12A |= 0x800;
                func_80106238(arg0, 0, 1);
            } else if (actor->field_97F & 2) {
                actor->field_95E  = 5;
                actor->field_95A  = 2;
                actor->field_979  = 0x1C;
                actor->field_93E  = 0x14;
                actor->field_934  = 3;
                actor->field_124  = 0x2191C;
                rec->field_10     = 0x400;
                rec->field_12     = 0x400;
                rec->field_4      = rec->field_C + 0xA00;
                actor->field_12A &= 0xF7FF;
                func_80106238(arg0, 0, 0);
                hammer = actor->field_914;
                if (hammer != NULL) {
                    hammer->spawnArg1 = 2;
                }
                Gp_ConsumeSlotQty(0x98, 0x101);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20190005, 1);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 2);
                break;
            }
            /* fallthrough */
        case 3:
            if (actor->field_93E != 0) {
                delay = actor->field_934;
                if (delay == 0) {
                    actor->field_93E--;
                    actor->field_934  = 3;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x98, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20190004, 1);
                    Gp_SpawnEff(0x6006B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                0x19, NULL);
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
                break;
            }
            /* fallthrough */
        case 4:
            actor->field_95E  = 6;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
            }
            break;
        case 5:
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay == 1) {
                flags            = actor->field_12A | 0xC000;
                actor->field_12A = flags;
            } else if (delay == 0) {
                actor->field_95E = 6;
                if (func_80106264(2) == 0) {
                    actor->field_914->spawnArg1 = 0;
                }
                flags            = actor->field_12A & 0x3FFF;
                actor->field_12A = flags;
            }
            /* fallthrough */
        case 6:
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
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x50;
}
