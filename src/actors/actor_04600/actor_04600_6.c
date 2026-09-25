#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_104600.h"

/// Turns the first enemy toward the player by at most 0x20 a frame: the heading
/// `field_2B0` takes the XZ direction to the player outright when within 0x20,
/// and otherwise steps 0x20 the short way round the 0x1000 circle. The root's
/// rotation is then rebuilt from that heading alone, in 0x18 bytes of the
/// scratch stack.
void Actor04600_Fn00FD8(Task* arg0)
{
    Actor104600Work*       work;
    GsCOORDINATE2*         coord;
    Actor104600RotScratch* sc;
    s16                    cur;
    s32                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;
    s32                    current;

    coord      = ((TmdObject*)arg0->extra)->coords;
    work       = (Actor104600Work*)arg0->work;
    sc         = (Actor104600RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    cur        = work->field_2B0 & 0xFFF;
    diff       = want - cur;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
    if (adiff < 0x21) {
        work->field_2B0 = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->field_2B0;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->field_2B0 = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->field_2B0;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

/// Death-state handler of the first enemy, under the `D_801153F4` mode byte:
/// mode 2 hides the model and mode 1 does nothing. Otherwise `field_2B4` steps
/// the death through three phases. Phase 0 shrinks the model and counts the
/// kill countdown down; when it runs out the death sound plays, state 0xF0 is
/// released, an optional final effect is spawned, the root transform is saved
/// and the enemy's node and four bodies are unlinked. Phase 1 folds the saved
/// transform back with a decaying Y scale for up to 0x3D frames, and phase 2
/// destroys the enemy once that count is spent. Outside reaction states 5 and 6
/// the first two phases also tick the animation, scale and recompute the
/// second part and re-colour the enemy.
void Actor04600_Fn01110(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor104600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj   = task->extra;
    work  = (Actor104600Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            model->flags     |= 0x80;
            enemy->node.flags = 1;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->obj1EC.flags &= 0x7FFF;
                    work->field_2AC    -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = 0x80;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor04600_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, ((TmdObject*)task->extra)->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&work->objFC);
                        Gp_UnlinkObj(&work->obj134);
                        Gp_UnlinkObj(&work->obj1B4);
                        Gp_UnlinkObj(&work->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    Actor04600_Fn02B14(task);
                    if (work->field_2B6 == 0xA) {
                        ((TmdObject*)task->extra)->flags = 2;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor04600_TickAnim(task);
                Actor04600_Fn0294C(task, &((TmdObject*)task->extra)->coords[1]);
                ((TmdObject*)task->extra)->coords[0].flg = 0;
                ((TmdObject*)task->extra)->coords[1].flg = 0;
                Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
                Actor04600_UpdateColor(enemy, &((TmdObject*)task->extra)->coords[1]);
            }
            break;
    }
}
