#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>
#include <psyq/strings.h>

#include "actors/actor_215100.h"
#include "actors/actor_342100.h"
#include "actors/actors_shared_80133c6c.h"
#include "actors/actors_shared_80149e54.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

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

extern u32            Gp_LcgState;
extern s8             D_8007216D;
extern u8             D_801153F4;
extern _Pose          D_shelter_b3_garbage_incinerator_80185B58[2];
extern GpMsgEntry     D_shelter_b3_garbage_incinerator_80185B40[];
extern _Pose          D_shelter_b3_garbage_incinerator_80185B88;
extern GpAreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C;
extern Task*          D_shelter_b3_garbage_incinerator_8018FC34;

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

extern s16           D_800691CA;
extern u8            D_80070F87;
extern s32           D_shelter_b3_garbage_incinerator_80185BC4;
extern ActorWaveCtx* D_shelter_b3_garbage_incinerator_8018FC38;
extern ActorWaveRec  D_shelter_b3_garbage_incinerator_8018FC60[9];
extern ActorWaveRec  D_shelter_b3_garbage_incinerator_8018FCB0[30];
extern POLY_FT4      D_shelter_b3_garbage_incinerator_8018FEE0[][30][8];

/* Shared in source with actors 342100 (the encounter's fade and spawn) and
   215100 (the caption drawing): their data here, with their types. */
extern u8         D_shelter_b3_garbage_incinerator_80186F70[];
extern TaskDesc   D_shelter_b3_garbage_incinerator_80185BAC;
extern GlyphUvwh  D_8010FB70[];
extern GlyphUvwh* D_shelter_b3_garbage_incinerator_8018FC44;
extern s16        D_shelter_b3_garbage_incinerator_8018FC4C;
extern s16        D_shelter_b3_garbage_incinerator_8018FC4E;
extern s16        D_shelter_b3_garbage_incinerator_8018FC50;
extern s16        D_shelter_b3_garbage_incinerator_8018FC54;
extern u16        D_shelter_b3_garbage_incinerator_8018FC58;
extern u16        D_shelter_b3_garbage_incinerator_8018FC5A;
extern s16        D_shelter_b3_garbage_incinerator_80187180;
extern s16        D_shelter_b3_garbage_incinerator_80187182;

extern Actor215100Caption** D_shelter_b3_garbage_incinerator_8018FC40;
extern Actor215100Caption*  D_shelter_b3_garbage_incinerator_8018FC48;
extern s16                  D_shelter_b3_garbage_incinerator_8018FC52;
extern s16                  D_shelter_b3_garbage_incinerator_8018FC56;
extern u8                   D_shelter_b3_garbage_incinerator_8018FC5C;
extern s32                  D_shelter_b3_garbage_incinerator_80187278;
extern s32                  D_shelter_b3_garbage_incinerator_8018727C;

void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0);
void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0);
void func_shelter_b3_garbage_incinerator_8017F968(void);
s32  func_shelter_b3_garbage_incinerator_8017FD64(s16 arg0, s16 arg1, s32 arg2);
s32  func_shelter_b3_garbage_incinerator_8017FE74(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
s16  func_shelter_b3_garbage_incinerator_801808A8(u16* arg0);
s16  func_shelter_b3_garbage_incinerator_80180B18(u16* arg0);
s16  func_shelter_b3_garbage_incinerator_80180C28(u16* arg0, s32 arg1);
s16  func_shelter_b3_garbage_incinerator_80180D44(u16* arg0);
s32  func_shelter_b3_garbage_incinerator_80180EC4(s32 arg0);
void func_shelter_b3_garbage_incinerator_80180994(void);
s32  func_shelter_b3_garbage_incinerator_80180E0C(u16* arg0);

/// Work block of the task in `D_shelter_b3_garbage_incinerator_8018FC3C`.
/// `field_2C` is the task animation messages are dispatched to, `child` the
/// task spawned from the room's table, `field_34` the task started from spawn
/// entry 2 when the encounter is armed, `field_38` the last animation set
/// selected, and `field_3A` the arming state.
typedef struct {
    byte  pad_0[0x2C];
    Task* field_2C;
    Task* child;
    Task* field_34;
    s16   field_38;
    s16   field_3A;
} GarbageIncineratorWork;

/// Payload of message 0x3F7: a null-terminated pointer table and the number
/// of live entries the sender counted in it.
typedef struct {
    s32* table;
    s32  count;
} GarbageIncineratorMsg3F7;

/// One window of the caption schedule: while the scene clock lies in
/// (`field_4 * 30`, `field_0 * 30`], caption script `field_8` is started at
/// line key `field_C`. A `field_0` of -1 ends the table.
typedef struct {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    s32 field_C;
} GarbageIncineratorCapWindow;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[];

/// Null-terminated table counted and sent with message 0x3F7 on arming.
extern s32 D_shelter_b3_garbage_incinerator_80186F78[];

/// Table indexed by `field_38 - 0x2F`: each entry is the following animation
/// set less 0x2F, and a negative entry means there is none.
extern s16 D_shelter_b3_garbage_incinerator_80186F88[];

/// Model/animation set installed with `func_800E8614` on arming.
extern u8 D_shelter_b3_garbage_incinerator_80186FB8[];

/// Effect record handed to `func_800FDB18`: `coord` is the chosen part of the
/// model and `spawnArgLo` the scale that goes with it.
extern GpEffArg D_shelter_b3_garbage_incinerator_80186F90;

/// Model parts the effect record is aimed at, as indices into the
/// display object's coordinate array.
extern u16 D_shelter_b3_garbage_incinerator_80186F98[];

s32          func_shelter_b3_garbage_incinerator_8017F318(Task* arg0);
extern Task* D_shelter_b3_garbage_incinerator_8018FC3C;

/// Main-executable global with no module header yet: the base animation-set
/// id, whose alternate range `D_8007218A` selects when it is 1.
extern u8 D_80073BA9;

/// Caption schedule scanned by `func_shelter_b3_garbage_incinerator_8017FA58`.
extern GarbageIncineratorCapWindow D_shelter_b3_garbage_incinerator_801871A8[];
extern u8                          D_80114CF8;

/// Task table entry spawned once when the controller starts.
extern TaskDesc D_shelter_b3_garbage_incinerator_80187184;
void            func_shelter_b3_garbage_incinerator_8017FB80(void);

extern TaskDesc D_shelter_b3_garbage_incinerator_80187190;

extern TaskDesc D_shelter_b3_garbage_incinerator_8018719C;

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
                msg.pos.vy += (gDisplayState.animFrame & 1) ? 10 : -10;
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
                func_shelter_b3_garbage_incinerator_80180FE4(5, 0, 0x3C);
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

/// Sets how the task's model is treated from `arg2`: 0 hides it and leaves its
/// primitive buffer to be allocated on demand, 1 shows it with the same
/// allocation, 2 hides it and exempts it from that allocation.
void func_shelter_b3_garbage_incinerator_8017E690(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Places the task's model: re-parents its coordinate to the world frame, takes
/// the three longs of `placement` as the translation and applies the three
/// shorts as yaw, pitch and roll.
void func_shelter_b3_garbage_incinerator_8017E70C(Task* task, s32 arg1, ActorShared80133c6cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

void func_shelter_b3_garbage_incinerator_8017E7A4(GarbageIncineratorState* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->field_30 = 5;
}

/// Screen-wave effect task, driven by the context passed as its spawn
/// argument. Its first run seeds a random phase offset and speed for each of
/// the 9 column and 30 row waves and builds two 8x30 grids of `POLY_FT4`s that
/// sample the two frame-buffer halves. Every later frame it ramps the
/// context's strength up to its target, or back down once the context asks
/// and then kills itself, advances the waves while a gameplay state flag is
/// clear, and draws the current buffer's grid with each vertex displaced by
/// sine waves scaled by that strength.
void func_shelter_b3_garbage_incinerator_8017E7D0(Task* arg0)
{
    ActorWaveScratch* scratch;
    ActorWaveScratch* head;
    ActorWaveCtx*     ctx;
    ActorWaveRec*     cols;
    POLY_FT4*         p;
    DR_STP*           stp;
    s32               i;
    s32               j;
    s32               k;
    s32               rowIndex;
    s32               rowBack;
    s32               u0;
    s32               u1;
    s32               v0;
    s32               v1;
    s32               waveX0;
    s32               waveY0;
    s32               waveX1;
    s32               waveY1;
    s32               waveX2;
    s32               waveY2;
    s32               waveX3;
    s32               waveY3;
    ActorWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                                = *(ActorWaveScratch**)G_SCRATCH_HEAD;
    D_800691CA                          = 2;
    *(ActorWaveScratch**)G_SCRATCH_HEAD = head - 1;
    cols                                = head[-1].cols;
    scratch                             = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_shelter_b3_garbage_incinerator_8018FC60[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FC60[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FC60[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b3_garbage_incinerator_8018FCB0[i].phase  = 0;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].offset = (u32)rand() >> 3;
                D_shelter_b3_garbage_incinerator_8018FCB0[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_shelter_b3_garbage_incinerator_80185BC4          = 0;
            D_shelter_b3_garbage_incinerator_8018FC38          = arg0->spawnArg2;
            D_shelter_b3_garbage_incinerator_8018FC38->field_6 = 0;
            D_shelter_b3_garbage_incinerator_8018FC38->field_4 = 0;
            Display_ClampField126(-8);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = D_shelter_b3_garbage_incinerator_8018FEE0[i];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_shelter_b3_garbage_incinerator_8018FC38->field_8 == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_shelter_b3_garbage_incinerator_8018FC38->field_9;
                            p->g0 = D_shelter_b3_garbage_incinerator_8018FC38->field_A;
                            p->b0 = D_shelter_b3_garbage_incinerator_8018FC38->field_B;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b3_garbage_incinerator_8018FC38;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        if (D_801153F4 == 0) {
                            ctx->field_6--;
                        }
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_shelter_b3_garbage_incinerator_80185BC4 = D_shelter_b3_garbage_incinerator_8018FC38->field_6 * D_shelter_b3_garbage_incinerator_8018FC38->field_2 / D_shelter_b3_garbage_incinerator_8018FC38->field_0;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FC60[i].phase += D_shelter_b3_garbage_incinerator_8018FC60[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FC60[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b3_garbage_incinerator_8018FCB0[i].phase += D_shelter_b3_garbage_incinerator_8018FCB0[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_shelter_b3_garbage_incinerator_8018FCB0[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = D_shelter_b3_garbage_incinerator_8018FEE0[D_80070F87];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        ActorWaveRec* next = row + 1;
                        waveX2             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2              = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2              = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3              = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3             = D_shelter_b3_garbage_incinerator_80185BC4 * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3              = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    *(ActorWaveScratch**)G_SCRATCH_HEAD += 1;
}

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `D_shelter_b3_garbage_incinerator_80186F70` in `Task::msgTable`. States 2 and
/// 3 step `field_2` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `field_4` / `field_6` by 8; once
/// `field_4` passes 0xFF the display mode is switched, `Fs_ImgBuffers` is
/// filled white, the parent work block's `field_24` is raised, and state 5
/// draws the full-screen white `TILE` + `DR_TPAGE` packed into
/// `gGpuPrimCursor` before returning without the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void func_shelter_b3_garbage_incinerator_8017F0A8(Task* arg0)
{
    Actor342100FadeWork* work;
    Actor342100FadeWork* alloc;
    Actor342100Work*     parent;
    TILE*                tile;
    DR_TPAGE*            dr;

    work = (Actor342100FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342100FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work           = alloc;
            work->field_6  = 0;
            work->field_4  = 0;
            work->field_2  = 0;
            arg0->msgTable = &D_shelter_b3_garbage_incinerator_80186F70;
            arg0->state   += 1;
            break;
        case 2:
            work->field_2 += 0xA;
            if ((s16)work->field_2 >= 0x51) {
                work->field_2 = 0x50;
                arg0->state   = 1;
            }
            break;
        case 3:
            work->field_2 += 1;
            if ((s16)work->field_2 >= 0x100) {
                work->field_2 = 0xFF;
                arg0->state   = 1;
            }
            break;
        case 4:
            work->field_4 += 8;
            work->field_6 += 8;
            if ((s16)work->field_4 >= 0x100) {
                parent           = (Actor342100Work*)((Task*)arg0->spawnArg2)->work;
                parent->field_24 = 2;
                Display_SetMode(0xD010);
                Mem_Set(Fs_ImgBuffers, 0xFF, 0x25800);
                work->field_6 = 0xFF;
                work->field_4 = 0xFF;
                arg0->state   = 5;
            }
            break;
        case 5:
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt - 16, dr);
            return;
    }
    Fade_DrawOverlay((u8)work->field_2, (u8)work->field_4, (u8)work->field_6, 1);
}

/// Step `field_2C` to the next animation set in the table. Returns 0 when
/// message 0x3ED to it returns nonzero, and 1 otherwise: with no `field_2C`,
/// with `field_38` below 0x2F, or with a negative table entry nothing is sent;
/// else the entry plus 0x2F is recorded in `field_38` and sent with message
/// 0x3E8. The set's block is `D_80073BA9 + 1` when `D_8007218A` is 1 and
/// `D_80073BA9 + 0x22` otherwise.
s32 func_shelter_b3_garbage_incinerator_8017F318(Task* arg0)
{
    GarbageIncineratorWork* work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork* msgWork;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    if (work->field_2C == NULL) {
    ret1:
        COMPILER_BARRIER();
        return 1;
    }
    if (Gp_DispatchMsg(work->field_2C, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if (work->field_38 < 0x2F) {
        return 1;
    }
    if (D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] < 0) {
        goto ret1;
    }
    anim              = (u16)D_shelter_b3_garbage_incinerator_80186F88[work->field_38 - 0x2F] + 0x2F;
    msgWork           = (GarbageIncineratorWork*)arg0->work;
    weaponId          = D_80073BA9;
    setId             = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0       = (void*)setId;
    msgWork->field_38 = anim;
    msg.field_4       = anim;
    msg.field_8       = 1;
    msg.field_C       = 0xA;
    msg.field_10      = 0;
    Gp_DispatchMsg(msgWork->field_2C, 0x3E8, (s32)&msg, 0);
    return 1;
}

/// Each tick rolls the LCG and aims the effect record at one part of the
/// model owned by `gameGetPtrSlot(3)`. State 0 fires with one of the first
/// four parts at scale 0x100 and steps to state 1. State 1 fires only on some
/// frames: with `spawnArg1` zero, one of the first four parts at scale 0x10
/// every sixteenth frame; otherwise one of the first sixteen at scale 0x100
/// every eighth frame.
void func_shelter_b3_garbage_incinerator_8017F410(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot        = gameGetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    idx         = Gp_LcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                                                 &= 3;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1 == 0) {
                if (gDisplayState.animFrame & 0xF) {
                    return;
                }
                idx                                                 &= 3;
                D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x10;
                D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
                func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
                return;
            }
            if (gDisplayState.animFrame & 7) {
                return;
            }
            idx                                                 &= 0xF;
            D_shelter_b3_garbage_incinerator_80186F90.spawnArgLo = 0x100;
            D_shelter_b3_garbage_incinerator_80186F90.coord      = &((TmdObject*)slot->extra)->coords[D_shelter_b3_garbage_incinerator_80186F98[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->coords, NULL, &D_shelter_b3_garbage_incinerator_80186F90);
            return;
    }
}

/// Arms the encounter on state 0: sends `field_2C` message 0x3F7 with the
/// table and its live-entry count, raises `Gp_StateC08.field_6` bit 0,
/// installs the model set, hands slot 6 message 0xFA4, starts spawn entry 2
/// with the task itself and steps to state 1. State 1 returns 1 while
/// `gGameSession->eventState` is clear; every other path calls
/// `func_shelter_b3_garbage_incinerator_8017F318` with the task and returns 0.
s32 func_shelter_b3_garbage_incinerator_8017F588(Task* arg0)
{
    GarbageIncineratorWork*  work = (GarbageIncineratorWork*)arg0->work;
    GarbageIncineratorWork*  msgWork;
    GarbageIncineratorMsg3F7 msg;
    s32                      n;

    switch (work->field_3A) {
        case 0:
            msgWork = work;
            n       = 0;
            while (D_shelter_b3_garbage_incinerator_80186F78[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.table = &D_shelter_b3_garbage_incinerator_80186F78[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsg(msgWork->field_2C, 0x3F7, (s32)&msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614((s32)&D_shelter_b3_garbage_incinerator_80186FB8, 0);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            work->field_34 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 2, 0, (s32)arg0);
            work->field_3A = work->field_3A + 1;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    func_shelter_b3_garbage_incinerator_8017F318(arg0);
    return 0;
}

/// Does nothing while `gGameSession->field_65`, `Gp_StateC08.field_9`,
/// `D_801153F4` or `D_80114CF8` is set. State 0 allocates and clears the work block (killing the task if that
/// fails), records `gameGetPtrSlot(3)` in `field_2C` and the task in
/// `D_shelter_b3_garbage_incinerator_8018FC3C`, spawns the table entry and,
/// with `spawnArg1` zero, queues sound event 0x54280005. State 1 advances once
/// the scene clock has run out while the player is alive, unless
/// `field_135` is 1 in view 0x21. State 2 advances when
/// `func_shelter_b3_garbage_incinerator_8017F588` returns nonzero.
void func_shelter_b3_garbage_incinerator_8017F6D8(Task* arg0)
{
    GameSession*            session = gGameSession;
    GarbageIncineratorWork* work;
    s32                     ok;
    PlayerStatus*           ps;

    if (session->field_65 != 0 || (s8)Gp_StateC08.field_9 != 0 || D_801153F4 != 0 || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || D_80071075 != 0) {
                return;
            }
            work       = Mem_Malloc(0x40, false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x40);
                work->field_2C                            = gameGetPtrSlot(3);
                D_shelter_b3_garbage_incinerator_8018FC3C = arg0;
            }
            Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80187184, 0, 0xD0, 0);
            if (arg0->spawnArg1 == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
            }
            break;
        case 1:
            ps = &Player_Status;
            if (session->sceneClock > 0) {
                ok = 0;
            } else if (ps->hp <= 0) {
                ok = 0;
            } else if (session->field_135 != 1 || session->at4.loc.view != 0x21) {
                ok = 1;
            } else {
                ok = 0;
            }
            if (!ok) {
                return;
            }
            break;
        case 2:
            if ((s16)func_shelter_b3_garbage_incinerator_8017F588(arg0) == 0) {
                return;
            }
            break;
        default:
            return;
    }
    arg0->state++;
}

void func_shelter_b3_garbage_incinerator_8017F8A4(GarbageIncineratorState* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

/// Select animation set `arg0 + 0x2F`, record it in the work block, and send
/// it to `field_2C` with message 0x3E8. The set's block is `D_80073BA9 + 1`
/// when `D_8007218A` is 1 and `D_80073BA9 + 0x22` otherwise.
void func_shelter_b3_garbage_incinerator_8017F8AC(s32 arg0)
{
    GarbageIncineratorWork* work;
    GpAnimArg               msg;
    s16                     anim;
    s32                     weaponId;
    s32                     setId;

    work           = D_shelter_b3_garbage_incinerator_8018FC3C->work;
    anim           = arg0 + 0x2F;
    weaponId       = D_80073BA9;
    setId          = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0    = (void*)setId;
    work->field_38 = anim;
    msg.field_4    = anim;
    msg.field_8    = 1;
    msg.field_C    = 0xF;
    msg.field_10   = 0;
    Gp_DispatchMsg(work->field_2C, 0x3E8, (s32)&msg, 0);
}

void func_shelter_b3_garbage_incinerator_8017F930(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_shelter_b3_garbage_incinerator_8017F968(void)
{
    Actor342100Work* work = (Actor342100Work*)D_shelter_b3_garbage_incinerator_8018FC3C->work;

    work->field_20 = 0x258;
    work->field_22 = 0x100;
    Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80185BAC, 0, 0, (s32)&work->field_20);
}

void func_shelter_b3_garbage_incinerator_8017F9B4(s32 arg0)
{
    GarbageIncineratorWork* work = D_shelter_b3_garbage_incinerator_8018FC3C->work;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54280008, 0, 0);
        Gp_PulseState1C();
        gGameSession->enemyCullZone = 0x10;
        work->child                 = Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 3, 0, 0);
        return;
    }
    work->child->spawnArg1 = 1;
}

void func_shelter_b3_garbage_incinerator_8017FA3C(void)
{
    D_80073BA0                = 0;
    gGameSession->restartMode = 3;
}

/// Drives the caption schedule: state 0 arms it, and state 1 scans
/// `D_shelter_b3_garbage_incinerator_801871A8` for the first window containing
/// `gGameSession->sceneClock`; when one is found its script is started at its
/// line key with the low half of the task's `spawnArg1`. The clock then ticks
/// down one unless the caption system is busy or `D_801153F4` is set.
void func_shelter_b3_garbage_incinerator_8017FA58(Task* task, s32 arg1)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            script = 0;
            key    = arg1;
            for (i = 0; D_shelter_b3_garbage_incinerator_801871A8[i].field_0 != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_shelter_b3_garbage_incinerator_801871A8[i].field_0 * 30 >= time) &&
                    (D_shelter_b3_garbage_incinerator_801871A8[i].field_4 * 30 < time)) {
                    script = D_shelter_b3_garbage_incinerator_801871A8[i].field_8;
                    key    = D_shelter_b3_garbage_incinerator_801871A8[i].field_C;
                    break;
                }
            }
            if (script != 0) {
                func_shelter_b3_garbage_incinerator_8017FD64(script, key, (s16)task->spawnArg1);
                func_shelter_b3_garbage_incinerator_8017FB80();
            }
            if ((Gp_CapBusy() == 0) && (D_801153F4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

/// Draws the caption line the running script is on, when a script is
/// running, the line is not the terminator and the caption system is idle,
/// followed by the "more text" caret unless the line holds itself on screen.
void func_shelter_b3_garbage_incinerator_8017FB80(void)
{
    if ((D_shelter_b3_garbage_incinerator_8018FC48 != NULL) &&
        (D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_8 != -1) &&
        (Gp_CapBusy() == 0)) {
        func_shelter_b3_garbage_incinerator_8017FE74(D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_8, 0x80, 1,
                                                     D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_0 |
                                                         ((D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_1 & 0x10) * 0x10));
        if (!(D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_4 & 1)) {
            func_shelter_b3_garbage_incinerator_80180994();
        }
    }
}

/// Relocates a loaded caption file in place (its three table offsets, the
/// script pointers of its event records and its pointer table) and keeps its
/// glyph table and script table for the caption code. Returns 0 when the file
/// does not start with "CAP".
s32 func_shelter_b3_garbage_incinerator_8017FC5C(GpCapFile* file)
{
    s32            i;
    s32            count;
    s32            flag;
    GpEvt12*       rec;
    s32*           ptr;
    GpCapEvtTable* evts;
    GpCapPtrTable* ptrs;

    if (strncmp(file->magic, "CAP", 3) != 0) {
        return 0;
    }

    i = 0;
    if (file->field_8 > 0) {
        file->field_8  += (s32)file;
        file->field_C  += (s32)file;
        file->field_10 += (s32)file;
        evts            = (GpCapEvtTable*)file->field_C;
        rec             = (GpEvt12*)(evts + 1);
        count           = evts->count;
        if (count > 0) {
            flag = -1;
            do {
                if (rec->field_8 != flag) {
                    rec->field_8 += (s32)file;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        ptrs  = (GpCapPtrTable*)file->field_10;
        i     = 0;
        count = ptrs->count;
        ptr   = (s32*)(ptrs + 1);
        if (count > 0) {
            do {
                if (*ptr != 0) {
                    *ptr += (s32)file;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    D_shelter_b3_garbage_incinerator_8018FC44 = (GlyphUvwh*)file->field_8;
    D_shelter_b3_garbage_incinerator_8018FC40 = (Actor215100Caption**)((GpCapPtrTable*)file->field_10 + 1);
    return 1;
}

/// Starts caption script `arg0` at the first line keyed `arg1` and caches that
/// line's metrics; `arg2` is kept alongside them. Returns 1 when the script
/// table has no such script, 0 once it is started.
s32 func_shelter_b3_garbage_incinerator_8017FD64(s16 arg0, s16 arg1, s32 arg2)
{
    Actor215100Caption* caption;
    s16                 entry;

    caption                                   = D_shelter_b3_garbage_incinerator_8018FC40[arg0];
    D_shelter_b3_garbage_incinerator_8018FC48 = caption;
    if (caption == NULL) {
        return 1;
    }
    D_shelter_b3_garbage_incinerator_8018FC56 = arg1;
    entry                                     = func_shelter_b3_garbage_incinerator_80180EC4(1);
    D_shelter_b3_garbage_incinerator_8018FC52 = entry;
    D_shelter_b3_garbage_incinerator_8018FC50 = arg2;
    D_shelter_b3_garbage_incinerator_8018FC4C = func_shelter_b3_garbage_incinerator_80180B18((u16*)D_shelter_b3_garbage_incinerator_8018FC48[entry].field_8);
    D_shelter_b3_garbage_incinerator_8018FC4E = func_shelter_b3_garbage_incinerator_801808A8((u16*)D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_8);
    D_shelter_b3_garbage_incinerator_8018FC54 = func_shelter_b3_garbage_incinerator_80180D44((u16*)D_shelter_b3_garbage_incinerator_8018FC48[D_shelter_b3_garbage_incinerator_8018FC52].field_8);
    D_shelter_b3_garbage_incinerator_8018FC5C = 0x1E;
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017FE74(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16*       text;
    u16*       body;
    s32        title;
    s16        sc;
    u32        shifted;
    s32        titleWidth;
    s16        lineIdx;
    s16        x;
    s32        y;
    s16        i;
    u16        code;
    s16        centered;
    s32        palette;
    s16        t;
    s16        t2;
    s16        glyphY;
    s32        top;
    POLY_G4*   bg;
    POLY_G4*   bg2;
    DR_MODE*   dm;
    POLY_FT4*  ft;
    POLY_GT4*  gt;
    POLY_GT4*  gt2;
    GlyphUvwh* icon;

    lineIdx = 0;
    title   = arg3;
    text    = (u16*)arg0;
    x       = func_shelter_b3_garbage_incinerator_80180C28((u16*)arg0, 0) - 0xA0;
    y       = (u16)D_shelter_b3_garbage_incinerator_8018FC4E - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
    bg->y0 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x1 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - D_shelter_b3_garbage_incinerator_8018FC4C * 2 + 0xAB;
    bg->y1 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x2 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
    bg->y2 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54 + (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    bg->x3 = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - D_shelter_b3_garbage_incinerator_8018FC4C * 2 + 0xAB;
    bg->y3 = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_garbage_incinerator_8018FC54 + (u16)D_shelter_b3_garbage_incinerator_8018FC54;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg2 + 1);
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dm + 1);
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(ft + 1);
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)D_shelter_b3_garbage_incinerator_8018FC50 - 0x77) - (u16)D_shelter_b3_garbage_incinerator_8018FC54;
        ft->x0     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        titleWidth = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->x2     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_shelter_b3_garbage_incinerator_8018FC4C + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u;
        ft->v0     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v;
        ft->u1     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w;
        ft->v1     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v;
        ft->u2     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u;
        ft->v2     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->u3     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].u + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].w;
        ft->v3     = D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].v + D_shelter_b3_garbage_incinerator_8018FC44[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                                        = lineIdx + 1;
            lineIdx                                   = t2;
            D_shelter_b3_garbage_incinerator_8018FC5A = y - 2;
            D_shelter_b3_garbage_incinerator_8018FC58 = x + 4;
            y                                        += func_shelter_b3_garbage_incinerator_80180E0C(&body[i + 1]);
            if (centered != 0) {
                x = func_shelter_b3_garbage_incinerator_80180C28((u16*)arg0, t2) - 0xA0;
            } else {
                x = (u16)D_shelter_b3_garbage_incinerator_8018FC4C - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(ft + 1);
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->h;
            ft->x1    = x + icon->w;
            ft->y1    = t - icon->h;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->w;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->w;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->h;
            ft->u3    = icon->u + icon->w;
            ft->v3    = icon->v + icon->h;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->w;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt + 1);
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
            gt->y0    = glyphY - D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->x1    = x + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->y1    = glyphY - D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u;
            gt->v0    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v;
            gt->u1    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->v1    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v;
            gt->u2    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u;
            gt->v2    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            gt->u3    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].u + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].w;
            gt->v3    = D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].v + D_shelter_b3_garbage_incinerator_8018FC44[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_shelter_b3_garbage_incinerator_80187180, D_shelter_b3_garbage_incinerator_80187182);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_shelter_b3_garbage_incinerator_8018FC44[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_shelter_b3_garbage_incinerator_8018FC50`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0, and the two pins are
/// what that twin carries; unpinned the body lands at 92%.
s16 func_shelter_b3_garbage_incinerator_801808A8(u16* arg0)
{
    s32                 lineH;
    s32                 total;
    s32                 i;
    s32                 seenBreak;
    u16                 code;
    s32                 shifted;
    register s32        next asm("v1");
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");

    lineH     = 0;
    total     = lineH;
    i         = lineH;
    code      = arg0[0];
    shifted   = code << 16;
    seenBreak = lineH;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        do {
            v0tmp = seenBreak;
            if (shifted >> 16 == -2) {
                if (v0tmp != 0) {
                    if (lineH == 0) {
                        lineH = 2;
                    }
                    total += lineH;
                } else {
                    seenBreak = 1;
                }
                lineH = 0;
            } else if (shifted >> 16 != -3) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)D_shelter_b3_garbage_incinerator_8018FC44);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            next    = i + 1;
            code    = arg0[(s16)next];
            i       = next;
            shifted = code << 16;
            v0tmp   = -1;
        } while (shifted >> 16 != v0tmp);
    }
    return (s16)(D_shelter_b3_garbage_incinerator_8018FC50 - total);
}

/// Draws the pulsing "more text" caret, a Gouraud triangle whose grey level
/// swings between 9 and 15, once the caption's initial delay has run out.
void func_shelter_b3_garbage_incinerator_80180994(void)
{
    POLY_G3* prim;
    s32      c1;
    s32      c2;

    if (D_shelter_b3_garbage_incinerator_8018FC5C != 0) {
        D_shelter_b3_garbage_incinerator_8018FC5C -= 1;
        return;
    }
    prim           = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    c1 = (D_shelter_b3_garbage_incinerator_80187278 << 7) / 15;
    setRGB0(prim, c1, c1, c1);
    c1 = (D_shelter_b3_garbage_incinerator_80187278 * 192) / 15;
    c2 = c1;
    setRGB1(prim, c2, c2, c2);
    setRGB2(prim, c2, c2, c2);
    prim->x0 = D_shelter_b3_garbage_incinerator_8018FC58 + 3;
    prim->y0 = D_shelter_b3_garbage_incinerator_8018FC5A;
    prim->x1 = D_shelter_b3_garbage_incinerator_8018FC58;
    prim->x2 = D_shelter_b3_garbage_incinerator_8018FC58 + 7;
    prim->y1 = D_shelter_b3_garbage_incinerator_8018FC5A - 7;
    prim->y2 = D_shelter_b3_garbage_incinerator_8018FC5A - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_shelter_b3_garbage_incinerator_8018727C == 0) {
        D_shelter_b3_garbage_incinerator_80187278 += 1;
        if (D_shelter_b3_garbage_incinerator_80187278 >= 0xF) {
            D_shelter_b3_garbage_incinerator_8018727C = 1;
        }
    } else {
        D_shelter_b3_garbage_incinerator_80187278 -= 1;
        if (D_shelter_b3_garbage_incinerator_80187278 < 9) {
            D_shelter_b3_garbage_incinerator_8018727C = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_shelter_b3_garbage_incinerator_8018FC44`)
/// advances it by that glyph's `w - 1`.
///
/// The three pins are what gameplay's twin carries; leaving them out keeps the
/// block structure and instruction count but moves 71 register choices.
s16 func_shelter_b3_garbage_incinerator_80180B18(u16* arg0)
{
    register s32        lineW asm("t0");
    s32                 maxW;
    s32                 i;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW   = 0;
    maxW    = lineW;
    i       = lineW;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_garbage_incinerator_8018FC44;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((lineW << 16) > (maxW << 16)) {
                    maxW = lineW;
                }
                lineW = 0;
                goto do_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)maxW;
    return (0x140 - width) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes, pins included.
s16 func_shelter_b3_garbage_incinerator_80180C28(u16* arg0, s32 arg1)
{
    register s32        lineW asm("t1");
    s32                 selectedW;
    s32                 i;
    s32                 lineIndex;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW     = 0;
    selectedW = lineW;
    i         = lineW;
    lineIndex = lineW;
    code      = arg0[0];
    shifted   = code << 16;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_garbage_incinerator_8018FC44;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((s16)lineIndex == arg1) {
                    selectedW = lineW;
                }
                lineW = 0;
                v0tmp = i + 1;
                i     = v0tmp;
                lineIndex++;
                goto after_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
            after_inc:
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)selectedW;
    return (0x140 - width) / 2 - 5;
}

/// Total height of the caption text `arg0`: each `-2` line break adds the
/// height of the line it closes (its tallest glyph plus 2, or 2 for an empty
/// line), and `-1` ends the text.
s16 func_shelter_b3_garbage_incinerator_80180D44(u16* arg0)
{
    s32                 lineH;
    s32                 i;
    s32                 total;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;
    s32                 newline;
    s32                 skip;

    lineH   = 0;
    total   = lineH;
    code    = arg0[0];
    shifted = code << 16;
    i       = lineH;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        newline = -2;
        skip    = -3;
        table   = D_shelter_b3_garbage_incinerator_8018FC44;
        do {
            if (shifted >> 16 == newline) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
                lineH  = 0;
            } else if (shifted >> 16 != skip) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            v0tmp   = i + 1;
            code    = arg0[(s16)v0tmp];
            i       = v0tmp;
            shifted = code << 16;
        } while (shifted >> 16 != -1);
    }
    return total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_shelter_b3_garbage_incinerator_8018FC44`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_shelter_b3_garbage_incinerator_80180E0C(u16* arg0)
{
    s32                 height;
    s32                 i;
    s32                 cont;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    GlyphUvwh*          table;
    s32                 next;
    s32                 htmp;
    s32                 v0tmp;

    height  = 0;
    i       = height;
    cont    = 1;
    code    = arg0[0];
    table   = D_shelter_b3_garbage_incinerator_8018FC44;
    shifted = code << 16;
    for (;;) {
        shifted = shifted >> 16;
        if (shifted == -2) {
            cont = 0;
        } else if (shifted == -1) {
            cont   = 0;
            height = 0xD;
        } else {
            if (shifted >= 0) {
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                if (height < glyph->h + 2) {
                    htmp   = glyph->h;
                    height = htmp + 2;
                    goto do_inc;
                }
                next = i + 1;
            } else {
            do_inc:
                next = i + 1;
            }
            i = next;
            TOUCH_REG(next);
            code = arg0[(s16)next];
        }
        v0tmp = cont;
        TOUCH_REG(v0tmp);
        if (v0tmp == 0) {
            break;
        }
        shifted = code << 16;
    }
    if (height == 0) {
        height = 2;
    }
    return height;
}

/// Index of the first line at or after `arg0` in the running caption script
/// whose key is the requested one, or of the script's terminator.
s32 func_shelter_b3_garbage_incinerator_80180EC4(s32 arg0)
{
    s32                 flag;
    s32                 id;
    s32                 base;
    Actor215100Caption* p;

    flag = -1;
    id   = D_shelter_b3_garbage_incinerator_8018FC56;
    base = (s32)D_shelter_b3_garbage_incinerator_8018FC48;
    p    = (Actor215100Caption*)(arg0 * sizeof(Actor215100Caption) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

/// Caption display task: draws the caption each frame for the number of
/// frames given as its spawn argument, then kills itself.
void func_shelter_b3_garbage_incinerator_80180F18(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1 - 1;
    task->spawnArg1 = remaining;
    if (remaining <= 0) {
        taskKill(task);
    }
    func_shelter_b3_garbage_incinerator_8017FB80();
}

/// Caption display task: draws the caption each frame until the number of
/// frames given as its spawn argument runs out or cancel is pressed, then
/// kills itself and calls `Stage_SetEndingFlag`.
void func_shelter_b3_garbage_incinerator_80180F54(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1 - 1;
            task->spawnArg1 = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                taskKill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_shelter_b3_garbage_incinerator_8017FB80();
}

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_garbage_incinerator_8017FD64(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80187190, 0, arg2, 0);
}

/// Starts caption script `arg0` at line key `arg1` and spawns the caption
/// display task that shows it for `arg2` frames.
void func_shelter_b3_garbage_incinerator_80181038(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_garbage_incinerator_8017FD64(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_shelter_b3_garbage_incinerator_8018719C, arg2, 0, 0);
}

/// Sets up the caption system: `arg0`, `arg1` are the VRAM position of the
/// caption glyph texture page, and the caption file relocated is the
/// `arg2`-th resident file slot of type 3.
void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                                     = 0;
    D_shelter_b3_garbage_incinerator_80187180 = arg0;
    D_shelter_b3_garbage_incinerator_80187182 = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_shelter_b3_garbage_incinerator_8017FC5C((GpCapFile*)D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}
