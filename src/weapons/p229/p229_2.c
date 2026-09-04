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

/// Per-frame firing state machine for the P229. State 0 arms the shot and
/// starts the raise animation (clip 5 instead of 1 when the weapon was already
/// up), state 1 waits for that clip, and state 2 is the frame the round leaves
/// the barrel. That frame branches on `field_97F`: single fire (`== 1`) spends
/// one round, plays `0x20050004`, spawns the plain muzzle flash and restarts
/// the child slots, while burst fire spends 0x101, plays `0x20050005`, holds
/// the pose with the `0xC00` recoil pulse and reparents the longer flash effect
/// under the weapon task. States 3/4 pick the lock-on target once (only while
/// still in state 3) and state 5 counts `field_979` down, dropping back out of
/// the firing pose once the aim check fails or the trigger has been released.
void func_p229_8011DDA0(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    GpActorD4Rec*  rec;
    GpEffWork*     eff;
    s32            anim;
    s16            frames;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    rec   = (GpActorD4Rec*)actor->field_14C;
    /* Pinned to `$v0`: the scratch block's address is stored back to
       `G_SCRATCH_HEAD` from `$v0` and copied into the callee-saved `spot`,
       so the two uses must not be coalesced into one register. This block
       must stay *after* the three loads above, or the `lui`/`ori` of the
       scratch-head address wins the ready list and reschedules the entry. */
    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - 0x50;
        spot                    = (GsCOORDINATE2*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95A  = 0;
            actor->field_95C  = 0;
            actor->field_981  = 0;
            actor->field_95E += 1;
            actor->field_12A |= 0x400;
            anim              = 1;
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
            if (actor->field_97F == 1) {
                actor->field_95E  = 3;
                actor->field_979  = 0xA;
                actor->field_124  = Wip_SysConfig.field_22 | 0x20500;
                rec->field_10     = rec->field_12;
                actor->field_12A |= 0x800;
                func_80106238(arg0, 0, 0);
                Gp_ConsumeSlotQty(0x84, 1);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20050004, 0);
                Gp_SpawnEff(0x6002B,
                            (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 5,
                            NULL);
                Gp_AnimResetChildSlots(arg0, 0xA);
            } else {
                actor->field_95E  = 4;
                actor->field_979  = 6;
                actor->field_124  = 0x20516;
                rec->field_10     = 0xC00;
                actor->field_12A &= 0xF7FF;
                func_80106238(arg0, 0, 1);
                Gp_ConsumeSlotQty(0x84, 0x101);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20050005, 0);
                eff = Gp_SpawnEff(0x60040,
                                  (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 5,
                                  NULL);
                if (eff != NULL) {
                    Task_Reparent(actor->field_91C, eff->field_0);
                }
            }
            actor->field_12A |= 0xC000;
            break;
        case 3:
        case 4:
            if (actor->field_95E == 3 && Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 0);
            }
            actor->field_95E  = 5;
            actor->field_12A &= 0x3FFF;
            /* fallthrough */
        case 5:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                frames = 0x12;
                if (actor->field_97F == 1) {
                    frames = 0xC;
                }
                actor->field_940 = frames;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
