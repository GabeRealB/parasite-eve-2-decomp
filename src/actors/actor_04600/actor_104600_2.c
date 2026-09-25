#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_801673f8.h"
#include "actors/actor_104600.h"

/// Payload a message sender hands the first enemy's message handler as its
/// third argument; the handler reads only the halfword at 0x2, as a command
/// word whose low byte is the mode and whose bits 8..11 pick a spawn point.
typedef struct Actor104600Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor104600Msg;

extern GpEnemyTaskFuncTable3 Actor04600_D00004;

void Actor04600_Fn00EC8(Task* arg0, s32 arg1);
void Actor04600_Fn01604(Task* arg0, u8 arg1);

/// Message handler of the first enemy. While the task is in state 1, modes 4
/// and 5 start the collapse: the 0x60080 effect is spawned, animation 1 is
/// bound and the reaction state moves to 4 (mode 5 also restarts the spawn
/// count). Mode 1 reveals a dormant or dropping enemy: on maps 0x27 and 0x28
/// the root is placed at the spawn point the command selects (playing the
/// appearance sound on 0x27), the heading is taken from it and folded into
/// -0x800..0x800, the model's buffers are allocated and shown, the bodies are
/// re-armed and the drop begins at the live stage. Mode 3 hides the model,
/// disarms the bodies, resets the root and returns the task to state 3.
s32 Actor04600_Fn01F54(Task* arg0, s32 arg1, Actor104600Msg* arg2)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    u16              word;
    s16              heading;
    s32              magnitude;
    s32              mode;
    s32              state;
    s32              sound;
    s32              pan;

    obj   = (TmdObject*)arg0->extra;
    enemy = arg0->spawnArg2;
    state = arg0->state;
    work  = (Actor104600Work*)arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = arg2->field_2;
        if (mode == 4) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor04600_D058A8);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2B2 = 4;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor04600_D058A8);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2D4 = 0;
            work->field_2B2 = 4;
            return 0;
        }
    }
    word = arg2->field_2 & 0xFF;
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->field_2 >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->field_2 >> 8].z;
            }
            heading         = rot.vy;
            work->field_2B0 = heading;
            magnitude       = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->field_2B0 = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->field_2B0 = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers((TmdObject*)arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            enemy->node.flags                 = 0;
            work->objFC.flags                |= 0x8000;
            work->obj134.flags               |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_2BE                        = 0xC8;
            work->field_2E2                        = 1;
            work->field_2DE                        = 0x64;
            work->field_2E0                        = 0;
            work->field_2B2                        = 1;
            work->field_2C8                        = 1;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        enemy->node.flags                 = 1;
        work->objFC.flags                &= 0x7FFF;
        work->obj134.flags               &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 3;
        work->field_2E2 = 0;
        work->field_2B2 = 0;
        work->field_2C8 = 0;
    }
    return 0;
}

void Actor04600_Fn024A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame mode handler of the actor, shared with the other enemy actors
/// that gate on `D_801153F4`: mode 1 runs only the tail, mode 2 puts the model
/// in its hidden pose and returns, mode 0 clears both flags before falling
/// into the update, and any other mode updates directly. The update drives the
/// actor's four handlers, clears the display flags of the model's first two
/// coordinate parts and recomputes the second one's world matrix; the tail
/// then colours the actor from that second part and draws its ground shadow.
void Actor04600_Fn02500(Actor104600Ctx* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->field_14                   = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->field_14                   = one;
    return;
default_body:
    Actor04600_Fn003D4(arg1);
    Actor04600_Fn02618(arg1);
    Actor04600_Fn00978(arg1);
    Actor04600_Fn027BC(arg1);
    Actor04600_Fn0294C(arg1, &((TmdObject*)arg1->extra)->coords[1]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor04600_Fn02870(arg0, arg1);
    Actor04600_Fn028E0(arg1);
}

/// Per-frame reaction dispatch of the first enemy, on its `reactionFlags`:
/// bit 0x1 counts the death frames and grows the scale factor, killing the
/// enemy on the fifth; bit 0x2 is consumed and moves the reaction state to 3
/// with the step and the rebind stopped; bits 0xC tick the flag-4 helper,
/// feed the damage it reports to `Actor04600_Fn00EC8` and are cleared once it
/// expires.
void Actor04600_Fn02618(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = (Actor104600Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            work->field_2D4 += 1;
            work->field_2AC += 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor04600_Fn01604(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = 2;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_2B2       = 3;
            work->field_2B6       = 0;
            work->field_2BE       = 0;
            work->field_2D2       = 1;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (tick != 0) {
                Actor04600_Fn00EC8(arg0, tick);
            }
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}
