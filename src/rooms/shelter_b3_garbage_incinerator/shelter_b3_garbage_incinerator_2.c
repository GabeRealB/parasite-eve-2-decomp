#include "common.h"
#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
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
/// `lightMtx` and `colorMtx` are the model's own light and colour matrices,
/// `target` is the task that receives each frame's pose, `start*` its
/// translation captured on the first frame, `state` the step and `timer` the
/// frames spent jittering once the model has come to rest. `view` is the
/// session view recorded when the model lands, and `room` the session room the
/// model was last placed for.
typedef struct {
    MATRIX lightMtx;
    MATRIX colorMtx;
    Task*  field_40;
    Task*  target;
    s32    startX;
    s32    startY;
    s32    startZ;
    byte   unknown_54[0xC];
    u16    state;
    u16    timer;
    u16    view;
    s16    room;
} _DescentWork;

extern s32            D_80070F70;
extern s8             D_8007216D;
extern u8             D_801153F4;
extern _Pose          D_shelter_b3_garbage_incinerator_80185B58[2];
extern GpMsgEntry     D_shelter_b3_garbage_incinerator_80185B40[];
extern _Pose          D_shelter_b3_garbage_incinerator_80185B88;
extern GpAreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C;
extern Task*          D_shelter_b3_garbage_incinerator_8018FC34;

void RoomsShared801830f0(s16 arg0, s16 arg1, s16 arg2);
void func_shelter_b3_garbage_incinerator_8018507C(void);
void func_shelter_b3_garbage_incinerator_80185220(void);
void func_shelter_b3_garbage_incinerator_801853C4(void);

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
s16 func_shelter_b3_garbage_incinerator_8017DF24(Task* arg0)
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

/// Drives the room's moving model through the session's stage for it
/// (`field_132`, 0 to 3). The first frame sets up the work block and places the
/// model at the pose for the recorded stage. In stage 0 it waits for pending
/// event 5 of kind 1, then sets the session room to 2 (5 when the room was 4 or
/// above); it then moves the model 3 units a frame until it reaches the first
/// rest pose's height, and once the view changes sets the room to 3 (or 6).
/// State 5 runs `func_shelter_b3_garbage_incinerator_8017DF24` until it
/// finishes, then applies the room's area records and ends the task. Every
/// frame the model is re-placed when the session room changes. Nothing runs
/// while any of the four flags tested on entry is set.
void func_shelter_b3_garbage_incinerator_8017E158(Task* task)
{
    VECTOR         pos;
    u16            id;
    s8             kind;
    u8             arg;
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    _DescentWork*  work;
    s16            landed;
    TmdObject*     tail;
    GsCOORDINATE2* lift;
    _DescentWork*  done_work;
    s32            want;
    s32            t;

    if (gGameSession->field_65 != 0 || (s8)Gp_StateC08.field_9 != 0 || D_801153F4 != 0 || Gp_StateC08.field_A == 1) {
        return;
    }
    switch (task->state) {
        case 0:
            obj        = task->extra;
            coord      = obj->coords;
            work       = memCalloc(0x68, 0);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(work, 0, 0x68);
                coord->sub                                = &gGfxViewCoord;
                obj->flags                                = 0;
                obj->otOffset                             = 0x1F;
                work->field_40                            = gameGetPtrSlot(3);
                obj->colorMtx                             = &work->colorMtx;
                D_shelter_b3_garbage_incinerator_8018FC34 = task;
                obj->lightMtx                             = &work->lightMtx;
                task->msgTable                            = D_shelter_b3_garbage_incinerator_80185B40;
                work->target                              = (Task*)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
            }
            if (gGameSession->field_135 != 0) {
                func_shelter_b3_garbage_incinerator_8018507C();
                goto kill;
            }
            switch (gGameSession->field_132) {
                case 0:
                    Gp_DispatchMsg(task, 0x7D4, (s32)&D_shelter_b3_garbage_incinerator_80185B88, 0);
                    func_shelter_b3_garbage_incinerator_80185220();
                    task->state = 1;
                    break;
                case 1:
                case 2:
                    Gp_DispatchMsg(task, 0x7D4, (s32)D_shelter_b3_garbage_incinerator_80185B58, 0);
                    task->state = 4;
                    break;
                case 3:
                    goto kill;
            }
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            break;
        case 1:
            if (Gp_TakePendingObj4C(&id, (u8*)&kind, &arg) == 0) {
                break;
            }
            t    = id & 0x7FFF;
            want = 5;
            if (t != want) {
                break;
            }
            /* MATCHING CARRIER: ends both compare operands here. Otherwise cse
             * carries `t == 5` into the else arm below and stores its 5 from
             * that register, keeping it live across the calls. */
            DEF_REG(t);
            DEF_REG(want);
            if (kind == 1) {
                SndEvt_EnqueueType6(0x5428000D, 0, 0);
                SndEvt_EnqueueType6(0x54280003, 0, 0);
                if (gGameSession->at4.loc.room < 4) {
                    gGameSession->at4.loc.room   = 2;
                    D_8007216D                   = 2;
                    gGameSession->eventRoomIndex = 1;
                    gGameSession->roomObjsDirty  = 1;
                    gGameSession->eventRoomIndex = gGameSession->at4.loc.room - 1;
                    gGameSession->field_133      = 0;
                } else {
                    gGameSession->at4.loc.room   = 5;
                    D_8007216D                   = 5;
                    gGameSession->eventRoomIndex = 4;
                    gGameSession->roomObjsDirty  = 1;
                    gGameSession->eventRoomIndex = gGameSession->at4.loc.room - 1;
                    gGameSession->field_133      = 1;
                }
                RoomsShared801830f0(5, 0, 0x3C);
                gGameSession->field_132 = 1;
                task->state++;
            }
            break;
        case 2:
            lift              = ((TmdObject*)task->extra)->coords;
            lift->flg         = 0;
            lift->coord.t[1] -= 3;
            if (lift->coord.t[1] < D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy) {
                SndEvt_EnqueueType7(0x54280003, 1);
                SndEvt_EnqueueType6(0x54280004, 0, 0);
                lift->coord.t[1] = D_shelter_b3_garbage_incinerator_80185B58[0].pos.vy;
                landed           = 1;
            } else {
                landed = 0;
            }
            if (landed) {
                done_work               = task->work;
                gGameSession->field_132 = 2;
                func_shelter_b3_garbage_incinerator_801853C4();
                done_work->view = gGameSession->at4.loc.view;
                task->state++;
            }
            break;
        case 3:
            if (((_DescentWork*)task->work)->view != gGameSession->at4.loc.view) {
                if (gGameSession->at4.loc.room < 4) {
                    gGameSession->at4.loc.room   = 3;
                    D_8007216D                   = 3;
                    gGameSession->eventRoomIndex = 2;
                    gGameSession->roomObjsDirty  = 1;
                } else {
                    gGameSession->at4.loc.room   = 6;
                    D_8007216D                   = 6;
                    gGameSession->eventRoomIndex = 5;
                    gGameSession->roomObjsDirty  = 1;
                }
                task->state++;
            }
            break;
        case 5:
            if (!func_shelter_b3_garbage_incinerator_8017DF24(task)) {
                break;
            }
            gGameSession->field_132 = 3;
            Gp_ApplyAreaRecs(&D_shelter_b3_garbage_incinerator_8018FB6C);
        kill:
            taskKill(task);
            return;
    }
    work = task->work;
    if (gGameSession->at4.loc.room != work->room) {
        tail   = task->extra;
        pos.vx = tail->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(tail, &pos, 0, 3);
        work->room = gGameSession->at4.loc.room;
    }
}
