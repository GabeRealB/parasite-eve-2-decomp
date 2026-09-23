#include "common.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

extern u8       D_80071075;
extern s8       D_8007218A;
extern s8       D_80114C12;
extern TaskDesc D_80164FF8;

/// A position and orientation: the payload of message 0x7D4, and the shape of
/// the room's two-entry table of resting poses.
typedef struct {
    VECTOR  pos;
    SVECTOR rot;
} _Pose;

/// Work block of the task that moves its model while steering another task.
/// `target` is the task that receives each frame's pose, `start*` its
/// translation captured on the first frame, `state` the step and `timer` the
/// frames spent jittering once the model has come to rest.
typedef struct {
    byte  unknown_0[0x44];
    Task* target;
    s32   startX;
    s32   startY;
    s32   startZ;
    byte  unknown_54[0xC];
    u16   state;
    u16   timer;
} _DescentWork;

extern s32   D_80070F70;
extern _Pose D_shelter_b3_garbage_incinerator_80185B58[2];

void func_shelter_b3_garbage_incinerator_8017DCD4(Task* arg0)
{
    u8   param1[8];
    u8   param2[8];
    s32  msg[5];
    s32  out;
    s32  v;
    s32  w;
    s32* p;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                break;
            }
            SndEvt_EnqueueType6(0x5428000D, 0, 0);
            SndEvt_EnqueueType6(0x54280010, 0, 0);
            param1[2]       = 0x22;
            param1[3]       = 0;
            param1[0]       = 0;
            param2[0]       = 0x14;
            param2[1]       = 0;
            param2[2]       = 0;
            param2[3]       = 0;
            arg0->spawnArg1 = (u16)CdCmd_Enqueue(0x21, param1, param2);
            if ((u8)gGameSession->skipEventIntro == 0) {
                if (Player_Status.weapon == 0x17) {
                    p = msg;
                    w = Player_Status.weapon;
                    if (D_8007218A == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    msg[2] = 0;
                    msg[3] = 0;
                    msg[4] = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
                } else {
                    p = msg;
                    w = Player_Status.weapon;
                    if (D_8007218A == 1) {
                        v = w + 1;
                    } else {
                        v = w + 0x22;
                    }
                    msg[0] = v;
                    p[1]   = 1;
                    p[2]   = 1;
                    p[3]   = 10;
                    msg[4] = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
                }
                arg0->killCountdown = 0;
                arg0->state++;
            } else {
                arg0->state = 2;
            }
            break;
        case 1:
            if (++arg0->killCountdown >= 31) {
                arg0->state++;
            }
            break;
        case 2:
            if (CdCmd_IsSlotEmpty(arg0->spawnArg1)) {
                arg0->spawnArg2 = Task_SpawnFromTable(&D_80164FF8, 0, 0, 0);
                arg0->state++;
            }
            break;
        case 3:
            if (Task_PollKill(arg0->spawnArg2, &out) != 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Raises the Y translation of the task's model by 15 a frame until it passes
/// the second resting pose's (snapping to it at once when the session skips the
/// event intro in view 0x28), then jitters the sent height by 10 for 16 frames. Every frame it sends
/// the target task a pose built from its model's height, and returns 1 once
/// the sequence is over.
s32 func_shelter_b3_garbage_incinerator_8017DF24(Task* arg0)
{
    _Pose          msg;
    _DescentWork*  work  = arg0->work;
    GsCOORDINATE2* coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2* ref   = ((TmdObject*)work->target->extra)->coords;

    switch (work->state) {
        case 0:
            SndEvt_EnqueueType6(0x5428000E, 0, 0);
            work->startX = ref->coord.t[0];
            work->startY = ref->coord.t[1];
            work->startZ = ref->coord.t[2];
            work->state++;
            /* fallthrough */
        case 1:
            coord->flg         = 0;
            coord->coord.t[1] += 15;
            if (D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy < coord->coord.t[1] || (gGameSession->at4.loc.view == 0x28 && (u8)gGameSession->skipEventIntro != 0)) {
                SndEvt_EnqueueType7(0x5428000E, 1);
                SndEvt_EnqueueType6(0x5428000F, 0, 0);
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[1].pos.vy;
                work->state++;
                work->timer = 0;
                work->state++;
            }
            msg.pos.vx = ref->coord.t[0];
            msg.pos.vy = coord->coord.t[1];
            msg.pos.vz = ref->coord.t[2];
            break;
        case 2:
            msg.pos.vx = work->startX;
            msg.pos.vy = coord->coord.t[1];
            msg.pos.vz = work->startZ;
            if (++work->timer >= 16) {
                work->state++;
            } else {
                msg.pos.vy += (D_80070F70 & 1) ? 10 : -10;
            }
            break;
        case 3:
            return 1;
    }
    msg.rot.vz = 0;
    msg.rot.vx = 0;
    msg.rot.vy = 0x800;
    Gp_DispatchMsg(work->target, 0x7D4, (s32)&msg, 0);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017E158);
