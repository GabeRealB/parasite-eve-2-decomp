#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_water_tank.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80180b2c.h"

extern TaskDesc D_dryfield_water_tank_80184DF4;

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C::field_A` variant index.
extern u16 D_dryfield_water_tank_801868CC[];

/// The tank's wobble spring, the four words at 0x801868BC: `801868BC` is the
/// accumulated yaw `Gfx_RotMatrixY` is handed (`>> 8`), `801868C0` its velocity,
/// `801868C4` the yaw it steps toward and `801868C8` the target that step
/// chases.
extern s32 D_dryfield_water_tank_801868BC;
extern s32 D_dryfield_water_tank_801868C0;
extern s32 D_dryfield_water_tank_801868C4;
extern s32 D_dryfield_water_tank_801868C8;

/// The placement the room sends slot 3 with message 0x3E9.
extern RoomPlacement D_dryfield_water_tank_801804F4;

/// The water tank's run: one `SVECTOR` position per frame, `y` fixed at -12000
/// and `z` stepping up the room, 52 entries of movement before the tail clamps.
extern SVECTOR D_dryfield_water_tank_80184530[];

/// The second leg of the tank's run, same `SVECTOR` shape and one entry per
/// frame: the seam repeats the first table's last entry (`x` 2532, `y` -12000,
/// `z` 972), after which `x` steps down while `z` holds. Exactly the 52 entries
/// its walk consumes, so unlike the first table it has no clamp tail.
extern SVECTOR D_dryfield_water_tank_801847C0[];

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Stages the room's cutscene: tells the script task the water-tank scene is
/// entered (msg 0x3E9 with the room's fixed placement), then hands slot 3 the
/// 0x3E8 record that installs the equipped weapon's animation set -- the same
/// five-word record `acropolis_observatory_4` fills, `field_4` 1 and the rest
/// zero. The display is turned back on last, because entering the scene is what
/// blanked it.
void func_dryfield_water_tank_8017EBA0(void)
{
    GpRec14 rec;
    s32     weaponId;
    s32     anim;

    Gp_DispatchMsg(((RoomsShared80180b2cWork*)RoomsShared80180b2cTask->work)->owner, 0x3E9,
                   (s32)&D_dryfield_water_tank_801804F4, 0);
    weaponId     = D_80073BA9;
    anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0  = anim;
    rec.field_4  = 1;
    rec.field_8  = 0;
    rec.field_C  = 0;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
    SetDispMask(1);
}

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

/// Walks the water tank one step along `D_dryfield_water_tank_80184530` per
/// frame: sends slot 3 that entry as a `RoomPlacement` -- the spline position
/// with the tank's fixed half-turn about `y` -- and advances `killCountdown`.
/// At 0x34 the tank has finished its run, and the task kills itself.
void func_dryfield_water_tank_8017EC6C(Task* arg0)
{
    RoomPlacement rec;

    if (arg0->killCountdown >= 0x34) {
        Task_Kill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_80184530[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_80184530[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_80184530[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = -0x7FF;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// The tank's second run leg, the continuation of `func_dryfield_water_tank_8017EC6C`:
/// walks it one step along `D_dryfield_water_tank_801847C0` per frame and sends
/// slot 3 that entry as a `RoomPlacement`, this time with a quarter-turn about
/// `y` (0x400) instead of the first leg's half-turn. At 0x34 the tank has
/// finished its run a second time and the task kills itself.
void func_dryfield_water_tank_8017ED30(Task* arg0)
{
    RoomPlacement rec;

    if (arg0->killCountdown >= 0x34) {
        Task_Kill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_801847C0[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_801847C0[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_801847C0[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = 0x400;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// Per-frame model update for the tank: the callback word at 0x801868A8 in the
/// room's task table `D_dryfield_water_tank_801868A4`. State 0 parents the
/// model's coordinate to `Gfx_ViewCoord` and places it against the room's north
/// wall, then advances to state 1. State 1 drives the tank's slow wobble about
/// `y`:
/// an occasional roll re-picks the target yaw, the step moves toward it 0x100
/// at a time, and the velocity follows 19/20 of the way to that step. Either
/// way the frame ends by publishing the model's `workm` translation as a
/// `VECTOR` to `func_800D7A9C`, rebuilding the coordinate's yaw matrix from the
/// accumulated angle, and clearing `flg` so the parent recomputes the world
/// matrix next frame.
///
/// The coordinate's load is written through the cast expression, *before* the
/// object pointer is assigned, because the pointer assignment has to stay a
/// separate register copy: assigned first, cse.c's `(set REG0 REG1)` swap folds
/// the load and the copy into one and the overlay comes up an `addu` short (see
/// DECOMPILATION_LEARNINGS.md, "A load the pointer variable must copy").
void func_dryfield_water_tank_8017EDF4(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord = ((TmdObject*)arg0->extra)->field_8;
    obj   = (TmdObject*)arg0->extra;
    switch (arg0->state) {
        case 0:
            obj->field_C      = 0;
            coord->sub        = &Gfx_ViewCoord;
            coord->coord.t[0] = 0xBB8;
            coord->coord.t[1] = -0x34A8;
            coord->coord.t[2] = -0x4D8;
            arg0->state++;
            break;
        case 1:
            if (((s32)(rand() * 100) >> 15) <= 0) {
                if (((s32)(rand() * 100) >> 15) < 0x50) {
                    D_dryfield_water_tank_801868C8 = (s32)(rand() * 20) >> 7;
                } else {
                    D_dryfield_water_tank_801868C8 = 0;
                }
            }
            if (D_dryfield_water_tank_801868C4 < D_dryfield_water_tank_801868C8) {
                D_dryfield_water_tank_801868C4 += 0x100;
            } else if (D_dryfield_water_tank_801868C8 < D_dryfield_water_tank_801868C4) {
                D_dryfield_water_tank_801868C4 -= 0x100;
            }
            D_dryfield_water_tank_801868C0 =
                (D_dryfield_water_tank_801868C0 + D_dryfield_water_tank_801868C4) * 19 / 20;
            D_dryfield_water_tank_801868BC += D_dryfield_water_tank_801868C0;
            break;
    }
    if (gGameSession->loc.view == 7) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gfx_RotMatrixY(&coord->coord, D_dryfield_water_tank_801868BC >> 8, 1);
    coord->flg = 0;
}

/// Toggle the room's cutscene-“watched” state over the view's two per-view
/// objects. Every use goes through one pointer variable: the compiler keeps it
/// in a global allocno, which is what pushes the two literals' constant into
/// `$v0` (see DECOMPILATION_LEARNINGS.md, "A one-constant toggle…").
void func_dryfield_water_tank_8017EFF4(s32 arg0)
{
    GameSessionFrom4* sess;
    DwtSprtRec*       rec;
    DwtSprtView*      view;

    sess = &gGameSession->loc;
    if (sess->stage == 2) {
        rec = (DwtSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if (!(arg0 & 0xFF)) {
            view           = rec->field_1C;
            view->field_1C = 0;
            view           = rec->field_58;
            view->field_C  = 1;
            return;
        }
        view           = rec->field_1C;
        view->field_1C = 1;
        view           = rec->field_58;
        view->field_C  = 0;
    }
}

/// Publishes the variant index the current camera view maps to: reads the view
/// index back and stores `D_dryfield_water_tank_801868CC[view - 1]` into the
/// shared work block's `field_A`. Gameplay holds this address in its data
/// (0x80110614, pointing at the room overlay), and `dryfield_parking_lot` and
/// `dryfield_water_tower` carry the same body.
void func_dryfield_water_tank_8017F084(void)
{
    Gp_State1C->field_A = D_dryfield_water_tank_801868CC[(Gp_GetViewIndex() & 0xFF) - 1];
}
