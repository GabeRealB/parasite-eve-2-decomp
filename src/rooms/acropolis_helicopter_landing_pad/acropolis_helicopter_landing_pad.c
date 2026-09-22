#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "main/gfx.h"
#include "main/fs.h"
#include "gameplay/3A34.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

extern s16 D_80071076;
extern s32 D_acropolis_helicopter_landing_pad_80184D9C;
extern s32 D_acropolis_helicopter_landing_pad_80184124;
extern s32 D_acropolis_helicopter_landing_pad_801844B4;
extern s32 D_acropolis_helicopter_landing_pad_80184E0C;
extern s32 D_acropolis_helicopter_landing_pad_80187F84;
extern s16 D_acropolis_helicopter_landing_pad_80187F7C;
/// Turn-to-heading task state: current unwrapped yaw and signed step.
extern s32 D_acropolis_helicopter_landing_pad_80187F74;
extern s32 D_acropolis_helicopter_landing_pad_80187F78;

/// Main-executable globals with no module header yet: `D_80071075` gates the
/// phase advance, `D_80114C12` is the cutscene/among-us mode flag.
extern u8         D_80071075;
extern s8         D_80114C12;
extern TaskDesc   D_acropolis_helicopter_landing_pad_80184E68;
extern GpMsgEntry D_acropolis_helicopter_landing_pad_80182328[];
extern GsF_LIGHT  D_acropolis_helicopter_landing_pad_80182340[3];
/// Per-camera-view visibility table indexed by `(u8)gGameSession->at4.loc.view`:
/// a non-zero byte keeps the enemy model visible in that view.
extern s8 D_acropolis_helicopter_landing_pad_80182370[];

void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task);
s32  func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, RoomPlacement* placement, s32 arg3);
void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1);

/// Three `Gp_SpawnScript18` argument pairs used by the state timeline in
/// `func_acropolis_helicopter_landing_pad_8017DE78`, one pair per phase.
extern s32 D_acropolis_helicopter_landing_pad_80187D40;
extern s32 D_acropolis_helicopter_landing_pad_80187D48;
extern s32 D_acropolis_helicopter_landing_pad_80187D50;
extern s32 D_acropolis_helicopter_landing_pad_80187D60;
extern s32 D_acropolis_helicopter_landing_pad_80187D68;
extern s32 D_acropolis_helicopter_landing_pad_80187D78;

extern RoomPlacement D_acropolis_helicopter_landing_pad_80182394;
extern RoomPlacement D_acropolis_helicopter_landing_pad_801823AC;

/// Main-executable byte with no module header yet; `+ 1` seeds the slot-3
/// msg 0x3E8 record's `field_0` in `func_acropolis_helicopter_landing_pad_8017DA9C`.
extern u8 D_80073BA9;
/// Script / cutscene blocks and message payloads used by the room's
/// state-machine task `func_acropolis_helicopter_landing_pad_8017DA9C`:
/// `..._801837B0` is the slot-3 msg 0x3E9 argument, `..._8018467C` /
/// `..._80184CF4` the `func_800E8634` script pair started at the end,
/// `..._80184E28` the 0x7D3 payload sent to the spawned enemy task and
/// `..._80184E3C` the 0x14-byte msg 0x3E8 record.
extern s32        D_acropolis_helicopter_landing_pad_801837B0;
extern s32        D_acropolis_helicopter_landing_pad_8018467C;
extern s32        D_acropolis_helicopter_landing_pad_80184CF4;
extern AhlpMsg7D3 D_acropolis_helicopter_landing_pad_80184E28;
extern GpRec14    D_acropolis_helicopter_landing_pad_80184E3C;

/// State-0 entry of the room's enemy task: allocates the 0x54-byte work block
/// into `Task::work`, marks the model (`field_E = 8`, clears bit 0x80 of
/// `field_C`), runs the placement setup and installs the message table.
void func_acropolis_helicopter_landing_pad_8017D658(Task* task)
{
    TmdObject* obj = task->extra;
    void*      mem;

    mem = memCalloc(0x54, false);
    if (mem == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work    = mem;
    obj->otOffset = 8;
    obj->flags   &= 0xFF7F;
    func_acropolis_helicopter_landing_pad_8017D7B0(task);
    task->msgTable      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Per-frame update of the enemy task's model. While the `field_50` countdown
/// armed by the 0x7D3 handler is running, the model's coordinate translation
/// is stepped by the work block's three velocity words and marked dirty; the
/// countdown is clamped at zero once it expires. When `gGameSession->viewReady`
/// is set, the model is hidden (bit 0x80 of `field_C`) in every camera view
/// whose entry in the per-view table is zero and shown again otherwise.
void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task)
{
    AhlpEnemyWork* work  = (AhlpEnemyWork*)task->work;
    RoomCoord*     coord = (RoomCoord*)((TmdObject*)task->extra)->coords;
    TmdObject*     obj   = task->extra;
    s16            n;

    n = --work->field_50;
    if (n >= 0) {
        coord->coord.t[0] += work->field_0;
        coord->coord.t[1] += work->field_4;
        coord->coord.t[2] += work->field_8;
        coord->flg         = 0;
    } else {
        work->field_50 = 0;
    }
    if (gGameSession->viewReady != 0) {
        if (D_acropolis_helicopter_landing_pad_80182370[(u8)gGameSession->at4.loc.view] != 0) {
            obj->flags &= 0xFF7F;
        } else {
            obj->flags |= 0x80;
        }
    }
}

/// Points the model's light / colour matrices at the work block's own copies
/// and loads the room's three flat lights into them.
void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->work;
    TmdObject*     obj  = task->extra;
    GsF_LIGHT*     light;
    s32            i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_acropolis_helicopter_landing_pad_80182340; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// Msg 0x7D3 handler: repositions the model by `msg->phase`. Phases 0 and 1
/// take the first / second placement, arm the 0x78 countdown and reset the
/// work block's first three words with the step set to -0x19 / +0x19; phase 2
/// returns to the first placement and clears the countdown.
s32 func_acropolis_helicopter_landing_pad_8017D824(Task* task, s32 msgId, AhlpMsg7D3* msg)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->work;

    switch (msg->phase) {
        case 0:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = -0x19;
            work->field_8  = 0;
            break;
        case 1:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_801823AC, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = 0x19;
            work->field_8  = 0;
            break;
        case 2:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0;
            break;
    }
    return 0;
}

/// Msg 0x7D4 handler, also called directly by the 0x7D3 handler. Places the
/// task's model at `placement`: copies the position onto the coordinate's
/// translation, the Euler angles onto its rotation, rebuilds the rotation
/// matrix and marks the coordinate dirty.
s32 func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, RoomPlacement* placement, s32 arg3)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D964);

/// Per-frame phase tick of the helipad script. In phase 1 it posts msg 0x7D6
/// to slot 4; once that is refused and no cutscene/among-us mode
/// (`D_80114C12`) or blocker (`D_80071075`) is active it advances to phase 2,
/// starts the second script block and queues sound 0xA2. Room 5 of the
/// session raises `D_acropolis_helicopter_landing_pad_80184E0C`; a cleared
/// `gGameSession->eventState` resets `D_acropolis_helicopter_landing_pad_80187F84`.
void func_acropolis_helicopter_landing_pad_8017D9BC(void)
{
    s32 phase = D_acropolis_helicopter_landing_pad_80184D9C;

    if (phase == 1) {
        if (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0) {
            if ((D_80114C12 != phase) && (D_80071075 == 0)) {
                D_acropolis_helicopter_landing_pad_80184D9C = 2;
                func_800E8634((s32)&D_acropolis_helicopter_landing_pad_80184124, 0,
                              (s32)&D_acropolis_helicopter_landing_pad_801844B4);
                func_800E3FAC(0xA2, 8);
            }
        }
    }
    if ((u8)gGameSession->at4.loc.view == 5) {
        D_acropolis_helicopter_landing_pad_80184E0C = 1;
    }
    if (gGameSession->eventState == 0) {
        D_acropolis_helicopter_landing_pad_80187F84 = 0;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5E4);
