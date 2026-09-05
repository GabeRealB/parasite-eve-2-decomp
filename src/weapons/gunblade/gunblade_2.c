#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "weapons/gunblade.h"

/// `Wip_SysConfig.field_22`, the attachment id of the held weapon, read under
/// its own address wherever the value is wanted once rather than as one of a
/// run of accesses to the config block.
extern u8 D_80073BAA;

/// `Mc_SaveData.field_22`, the 1-based difficulty/mode row of `D_80112E04`.
extern s8 D_8007218A;

/// Per-frame firing state machine for the gunblade. State 0 arms the shot and
/// raises the weapon (clip 6 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: the blade swing (1)
/// goes to state 3, seeding the 0x12-frame swing timer and the 0x39-frame
/// recoil counter and re-aiming the muzzle record with the attach-0x17 spread,
/// while anything else fires the gun and drops straight into the lock-on of
/// state 6, spending a magazine round and spawning the muzzle flash. States
/// 3-5 count `field_934` down: at 0 state 3 hands over to state 4 and spawns
/// the beam effect (parented to the weapon task so `func_gunblade_8011E008`
/// can reach it), and any later state falls out to 7. State 4 asks
/// `func_801060E0` where the blade landed; a connecting swing (2) advances to
/// state 5 and, if there is still a round to spend, charges the beam and
/// re-grades the shot from the attachment id. Every frame in this group plays
/// the hit sound once the swing has collided and, while the recoil counter
/// runs, spins the actor 0xC units and flags the shake for the tail. State 6
/// picks the lock-on target - attachment 0xE aims the sound at the target's own
/// position rather than the scratch coordinate - and falls into state 7, which
/// drops out of the firing pose once the aim check fails.
///
/// The tail is common to every state: it reads the blade's forward column out
/// of the muzzle matrix and, only while `shake` is set, adds a 1/136th of it to
/// the muzzle coordinate.
void func_gunblade_8011E040(GpActorWork* arg0)
{
    GameActor*       actor;
    GsCOORDINATE2*   coord;
    GunbladeScratch* blk;
    GpActorD4Rec*    rec;
    GpEffWork*       eff;
    s32              sfx;
    s32              anim;
    s32              hit;
    s32              lvl;
    s16              spread;
    s32              shake;

    shake                 = 0;
    actor                 = arg0->actor;
    sfx                   = (D_80073BAA - 0xD) << 24;
    rec                   = (GpActorD4Rec*)actor->field_14C;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD - sizeof(GunbladeScratch);
    blk                   = (GunbladeScratch*)*(u8**)G_SCRATCH_HEAD;
    coord                 = arg0->extra->field_8;
    if (sfx < 0) {
        sfx = 0;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_954 = 0;
            actor->field_956 = 4;
            actor->field_95A = 0;
            actor->field_95C = 0;
            func_80106238(arg0, 0, 0);
            anim              = 1;
            actor->field_12A |= 0x400;
            actor->field_95E += anim;
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
            if (actor->field_97F == 1) {
                actor->field_95E = 3;
                actor->field_934 = 0x12;
                actor->field_93E = 0;
                actor->field_952 = 0x39;
                func_80106518(0x17);
                actor->field_124  = 0x2171B;
                rec->field_10     = 0x180;
                rec->field_12     = 0x180;
                rec->field_4      = rec->field_C + D_80112F8E;
                actor->field_12A &= 0xF7FF;
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 3);
                break;
            }
            actor->field_95E = 6;
            ((GpActorD4Rec*)actor->field_14C)->field_4 =
                ((GpActorD4Rec*)actor->field_14C)->field_C + 0x2200;
            actor->field_124 = Wip_SysConfig.field_22 | 0x21700;
            rec->field_12    = 0x100;
            rec->field_4     = rec->field_C + 0x2200;
            spread           = 0x900;
            if (Wip_SysConfig.field_22 != 0xD) {
                spread = 0x100;
            }
            rec->field_10 = spread;
            if (Wip_SysConfig.field_22 == 0xE) {
                actor->field_12A |= 0x800;
            } else {
                actor->field_12A &= 0xF7FF;
            }
            Gp_ConsumeSlotQty(0x96, 1);
            actor->field_12A |= 0xC000;
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, sfx | 0x20170005, 1);
            Gp_SpawnEff(0x600A1, (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                        (D_80073BAA << 16) | 0x17, NULL);
            Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 3);
            break;
        case 3:
        case 4:
        case 5:
            actor->field_934 = actor->field_934 - 1;
            if (actor->field_934 == 0) {
                if (actor->field_95E == 3) {
                    actor->field_95E  = 4;
                    actor->field_934  = 8;
                    actor->field_12A |= 0xC000;
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, sfx | 0x20170006, 0);
                    eff = Gp_SpawnEff(0x60186,
                                      (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                      0x17, NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->field_0);
                    }
                } else if (actor->field_95E >= 4) {
                    actor->field_95E = 7;
                }
            }
            if (actor->field_95E == 4 && (s8)func_801060E0(arg0) == 2) {
                actor->field_95E = 5;
                if (func_80106264(1) != 0) {
                    if (Wip_SysConfig.field_22 < 0xF) {
                        lvl = Wip_SysConfig.field_22 + 0xB;
                    } else {
                        lvl = 0x20;
                    }
                    actor->field_124 = lvl | 0x21700;
                    Gp_ConsumeSlotQty(0x96, 1);
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, sfx | 0x20170008, 1);
                    func_gunblade_8011E008(D_80073BAA);
                } else {
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, sfx | 0x20170001, 0);
                }
            }
            if (actor->field_93E != 1 && Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                actor->field_93E = 1;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, sfx | 0x20170007, 0);
            }
            if (actor->field_952 != 0) {
                shake             = 1;
                actor->field_52  += 0xC;
                actor->field_952 -= 1;
            }
            break;
        case 6:
            actor->field_95E++;
            if (Wip_SysConfig.field_22 != 0xD) {
                hit = Gp_PickNearestRec18(actor->field_32C, coord, &blk->coord);
                if (Wip_SysConfig.field_22 == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                        blk->coord.workm.t[0] = actor->field_32C[0].field_8;
                        blk->coord.workm.t[1] = actor->field_32C[0].field_A;
                        blk->coord.workm.t[2] = actor->field_32C[0].field_C;
                        Gp_PlayObjSfx((GpObj38*)blk, sfx | 0x20170004, 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx((GpObj38*)blk, 0x17, 1);
                }
            }
            /* fallthrough */
        case 7:
            actor->field_12A &= 0x3FFF;
            if (func_80105894(arg0, D_80112E04[D_8007218A][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gfx_MatrixCol2(&coord->coord, &blk->dir);
    actor->field_973      = shake;
    blk->step.vx          = (s16)(blk->dir.vx / 136) * shake;
    blk->step.vy          = (s16)(blk->dir.vy / 136) * shake;
    blk->step.vz          = (s16)(blk->dir.vz / 136) * shake;
    coord->coord.t[0]    += blk->step.vx;
    coord->coord.t[1]    += blk->step.vy;
    coord->coord.t[2]    += blk->step.vz;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(GunbladeScratch);
}
