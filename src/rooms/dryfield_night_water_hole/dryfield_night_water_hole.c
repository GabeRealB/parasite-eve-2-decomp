#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Parameter block of `func_dryfield_night_water_hole_8017D6AC`, the room-local
/// resolver `func_dryfield_night_water_hole_8017DC28` calls with one pointer as
/// both its input and its output.
///
/// `field_0` is the code the resolver switches on: its jump table spans 2..0x2D
/// and anything outside that range falls through untouched. `field_2` passes
/// through unchanged, `field_3` is the byte the resolver writes, and `field_5`
/// is a busy flag - non-zero makes the resolver return immediately without
/// reading or writing anything else. The caller stages the block from the
/// `RoomDeparture` it is about to publish and copies `field_3` back into it.
typedef struct DnwhUtilParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
} DnwhUtilParam;
STATIC_ASSERT_SIZEOF(DnwhUtilParam, 0x6);

/// One entry of the NULL-terminated override list
/// `func_dryfield_night_water_hole_8017DE88` walks: the record the entry
/// installs in the room's parameter table, `Gp_RoomParamTables[stage][room]`,
/// and the slot it goes in.
typedef struct DnwhParamOverride {
    /* 0x0 */ GpRoomParamRec* rec;
    /* 0x4 */ s32             index;
} DnwhParamOverride;
STATIC_ASSERT_SIZEOF(DnwhParamOverride, 0x8);

/// One rectangle of water surface drawn by
/// `func_dryfield_night_water_hole_8017DF28`, in world coordinates: it spans
/// `width` along X from `x` and `depth` along Z from `z`, at height `y`. The
/// table ends at the first entry whose `y` word is -1; the drawing code reads
/// only its low half as the height.
typedef struct DnwhSurface {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} DnwhSurface;

/// Block the room's splash task receives as `spawnArg2`. Only the halfword at
/// 0x26 is touched: an effect strength, set from how far a tracked part moved
/// this frame and used as the odds of spawning each of the two effects.
typedef struct _DryfieldNightWaterHoleSplash {
    byte pad_0[0x26];
    s16  strength;
} _DryfieldNightWaterHoleSplash;

extern s32 D_80115738;
extern s32 D_8011574C;
/// Resident task table the ending task is spawned from, descriptor 1.
extern TaskDesc D_801351FC[];

/// Descriptor the room's event task is spawned from, index 0 of the table
/// `func_dryfield_night_water_hole_8017DC28` hands `Task_SpawnFromTable`. Its
/// callback is that same task, `func_dryfield_night_water_hole_8017D7E8`.
extern TaskDesc D_dryfield_night_water_hole_801805EC;
/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0.
extern GpMsgEntry D_dryfield_night_water_hole_801805F8[];
/// The two four-byte records this room hands the slot-4 task as the message
/// 0x7DB payload, picked by `gGameSession::at4.loc.warp`. They are the last two of
/// the four-record run at 0x80180654, which differ only in the halfword at 0x2.
extern s32 D_dryfield_night_water_hole_8018065C;
extern s32 D_dryfield_night_water_hole_80180660;
/// The records message 0x13EF passes to `func_800E8614` on the first visit
/// through sub-id 1, for `field_2` 2 and 1 respectively.
extern s32 D_dryfield_night_water_hole_8018067C;
extern s32 D_dryfield_night_water_hole_801807FC;
/// Descriptor of the room's water task, spawned while progress nibble 0xB8 is
/// still clear. Its callback is `func_dryfield_night_water_hole_8017E630`.
extern TaskDesc D_dryfield_night_water_hole_80180964[];
/// The room's water surfaces, terminated by an entry with `y == -1`.
extern DnwhSurface D_dryfield_night_water_hole_80180970[];
/// Point pairs of the glowing beams the splash task draws, one table per group
/// of views.
extern SVECTOR D_dryfield_night_water_hole_80180994[];
extern SVECTOR D_dryfield_night_water_hole_801809B4[];
extern SVECTOR D_dryfield_night_water_hole_801809D4[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_night_water_hole_801809F4[];
/// Override list applied once nibble 0xB8 is set.
extern DnwhParamOverride D_dryfield_night_water_hole_801835D8[];
/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_night_water_hole_80183628;
/// Frame counter the water surface's wave is phased by.
extern s16 D_dryfield_night_water_hole_8018362C;
/// The staged event descriptor, read by the room's event task.
extern RoomDeparture D_dryfield_night_water_hole_80183630;

void func_dryfield_night_water_hole_8017DE20(Task* task);
void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list);
void func_dryfield_night_water_hole_8017E690(Task* arg0);
void func_dryfield_night_water_hole_8017EA6C(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_water_hole_8017F3A8(GpCoord* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_water_hole_8017FB98(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_night_water_hole_8017FF84(GpCoord* arg0, s32 arg1, s32 arg2);

/// Answers the code in `in->field_0` in `out->field_3`, unless `in->field_5`
/// is set. Six codes have an answer, each from a progress nibble: 2 is 2 once
/// nibble 0x10F is set and 3 once nibble 0x11A reaches 2; 5, 41 and 45 are
/// nibbles 0xA4, 0xB6 and 0xB7 plus one; 16 is 3 once nibble 0x7A reaches 6;
/// and 20 maps nibble 0xF4's values 0-3 to 1, 6, 7 and 8 (1 otherwise). Every
/// other code leaves `out` untouched. Always returns 1.
s32 func_dryfield_night_water_hole_8017D6AC(DnwhUtilParam* in, DnwhUtilParam* out)
{
    if (in->field_5 == 0) {
        switch (in->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    out->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    out->field_3 = 3;
                }
                break;
            case 5:
                out->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    out->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        out->field_3 = 1;
                        break;
                    case 1:
                        out->field_3 = 6;
                        break;
                    case 2:
                        out->field_3 = 7;
                        break;
                    case 3:
                        out->field_3 = 8;
                        break;
                    default:
                        out->field_3 = 1;
                        break;
                }
                break;
            case 45:
                out->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                out->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// The room's event task, run on the descriptor staged in
/// `D_dryfield_night_water_hole_80183630`. State 0 sends the
/// descriptor's `facing` to the slot-3 game pointer as message 0x3EE, skipping
/// to state 2 when it is -1; state 1 waits until that pointer answers 0x3F0
/// with 0. States 2 and 3 play the sound event `sndEvent`, if any, and wait for
/// its voice to go quiet. State 4 queues type-7 sound event 0x80000000, commits
/// the save location in the descriptor's first four bytes (stage, area, warp,
/// room), re-spawns the player task as type 0x11 and kills itself.
void func_dryfield_night_water_hole_8017D7E8(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_dryfield_night_water_hole_80183630.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_dryfield_night_water_hole_80183630.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_dryfield_night_water_hole_80183630.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_dryfield_night_water_hole_80183630.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.stage = D_dryfield_night_water_hole_80183630.stage;
            Mc_SaveData.at4.loc.area  = D_dryfield_night_water_hole_80183630.area;
            Mc_SaveData.at4.loc.warp  = D_dryfield_night_water_hole_80183630.warp;
            Mc_SaveData.at4.loc.room  = D_dryfield_night_water_hole_80183630.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Room entry task tick: publish the room's message table in `Task::msgTable`
/// and claim game pointer slot 7. Progress nibble 0xB8 then picks the opening
/// move: while it is clear the room's water task is spawned from
/// `D_dryfield_night_water_hole_80180964`, and once it is set the parameter
/// overrides are applied instead.
///
/// On the visit whose sub-id (`gGameSession::at4.loc.place`) is 1 and that has
/// already latched nibble 0x95, and with the slot-4 task present, the room
/// announces itself to it with message 0x7DB, carrying the payload record
/// `gGameSession::at4.loc.warp` selects. On sub-id 0xA, with pointer slot 0xA
/// filled and nibble 0xCF still clear, it latches 0xCF, arms
/// `func_800E3FAC(0xA2, 0x25)` and spawns the ending task. Then advances state.
void func_dryfield_night_water_hole_8017D958(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_water_hole_801805F8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0xB8) == 0) {
        Task_SpawnFromTable(D_dryfield_night_water_hole_80180964, 0, 0, 0);
    } else {
        func_dryfield_night_water_hole_8017DE88(D_dryfield_night_water_hole_801835D8);
    }
    if (gGameSession->at4.loc.place == 1 && Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x95) != 0) {
        if (gGameSession->at4.loc.warp == 2) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_80180660, 0);
        } else {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_water_hole_8018065C, 0);
        }
    }
    if (gGameSession->at4.loc.place == 0xA && gameGetPtrSlot(0xA) != 0 && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0xCF, 2);
        func_800E3FAC(0xA2, 0x25);
        Task_SpawnFromTable(D_801351FC, 1, 0, 0);
    }
    arg0->state = arg0->state + 1;
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_night_water_hole_8017DE30`: the entry tick, the idle state,
/// then `taskKill`.
const TaskFuncTable3 D_dryfield_night_water_hole_8017D688 = {
    { func_dryfield_night_water_hole_8017D958, func_dryfield_night_water_hole_8017DE20, taskKill },
};

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_night_water_hole_8017DAD4(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. It copies the
/// incoming record to `out` and, unless `in->field_5` is set, answers two
/// queries in `out->field_3`:
///
/// - 0x19: while the session's stage is 2, 2 once progress nibble 0x3A has
///   reached 2 and 1 before; in any other stage, nibble 0x61 plus one.
/// - 0x26: with nibble 0xC9 set, 2 or 1 by nibble 0x53, plus 2 while nibble
///   0x51 is clear; with 0xC9 clear, 5 or 6 by whether nibble 0x51 is set.
///
/// Always returns 1.
s32 func_dryfield_night_water_hole_8017DADC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->msgId == 0x19) {
        temp = gGameSession->at4.loc.stage;
        if (temp == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = temp;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->field_3 = 2;
            } else {
                out->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 += 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->field_3 = 5;
            } else {
                out->field_3 = 6;
            }
        }
    }
    return 1;
}

/// Message 0x13F0 handler. Slot 7 dispatches it with the sender's command in
/// `arg2`, and only 2 concerns this room.
///
/// With progress nibble 0xB8 set the room's event task is spawned: this stages
/// a `RoomDeparture` for it, hands the code in `area` to the room's resolver
/// for one last say over `room`, publishes the descriptor to
/// `D_dryfield_night_water_hole_80183630` and spawns the task from
/// `D_dryfield_night_water_hole_801805EC`. The code staged is 0x2E, past the end
/// of the resolver's jump table, so the byte comes back as it went in.
///
/// Without it the event never ran: cap command 2 is armed, nibble 0x1BD records
/// it, and the sound is enqueued here instead of by the spawned task.
s32 func_dryfield_night_water_hole_8017DC28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    RoomDeparture work;
    DnwhUtilParam param;

    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB8) != 0) {
            RoomDeparture* wp;
            s32            (*resolve)(DnwhUtilParam*, DnwhUtilParam*) = func_dryfield_night_water_hole_8017D6AC;

            work.stage    = 4;
            work.area     = 0x2E;
            work.room     = 1;
            work.warp     = 3;
            work.sndEvent = 0x53200007;
            work.facing   = 0xC00;
            Gp_MsgPlayerWeapon(0);
            wp            = &work;
            param.field_0 = wp->area;
            param.field_2 = wp->warp;
            param.field_3 = wp->room;
            param.field_5 = 0;
            resolve(&param, &param);
            wp->area                             = param.field_0;
            wp->warp                             = param.field_2;
            wp->room                             = param.field_3;
            D_dryfield_night_water_hole_80183630 = work;
            Task_SpawnFromTable(&D_dryfield_night_water_hole_801805EC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(2);
            GameFlag_SetNibble(0x1BD, 2);
            SndEvt_EnqueueType6(0x53200004, 0, 0);
        }
    }
    return 0;
}

/// Message 0x13EF handler. On a visit through sub-id 1 with progress nibble
/// 0x95 still clear, a record whose `field_2` is 2 or 1 latches the nibble and
/// passes `D_dryfield_night_water_hole_8018067C` or
/// `D_dryfield_night_water_hole_801807FC` respectively to `func_800E8614`.
/// Always returns 0.
s32 func_dryfield_night_water_hole_8017DD5C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_s0;

    if ((in->field_2 == 2) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->at4.loc.place == 1)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_8018067C, 0);
    }
    temp_s0 = in->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->at4.loc.place == temp_s0)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_801807FC, 0);
    }
    return 0;
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
void func_dryfield_night_water_hole_8017DE20(Task* task)
{
    char pad[0x10];
}

/// The room task: copies the three-state table
/// `D_dryfield_night_water_hole_8017D688` onto the stack and runs the entry for
/// the task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_night_water_hole_8017DE30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_hole_8017D688;
    sp.funcs[task->state](task);
}

/// Applies the override list `func_dryfield_night_water_hole_8017D958` holds:
/// each entry replaces the room's parameter slot, both the `GpRoomParamRec`
/// pointer and the byte `Gp_LoadRoomParams` would have copied into
/// `Gp_RoomParams` out of it. The list ends at the first NULL record.
void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list)
{
    GpAreaKey*       sess;
    s32              i;
    GpRoomParamRec** recs;

    sess = &gGameSession->at4.loc;
    for (i = 0; list[i].rec != 0; i++) {
        recs                         = Gp_RoomParamTables[sess->stage - 1][sess->area - 1];
        recs[list[i].index]          = list[i].rec;
        Gp_RoomParams[list[i].index] = recs[list[i].index]->field_3;
    }
}

/// Draws each surface in `D_dryfield_night_water_hole_80180970` as two strips
/// of 64 semi-transparent Gouraud quads laid side by side along Z, projected
/// through the view matrix. The seam between the strips is lifted by a sine
/// wave that runs along X and scrolls with
/// `D_dryfield_night_water_hole_8018362C`, which only advances while
/// `Gp_StateF0.field_4` is clear. The outer edges are coloured (0xFF, 0, 0) and the seam
/// (0x20, 0x20, 0x20); each quad is followed by a draw-mode packet selecting
/// blend mode 2. Quads the projection flags as invalid are skipped. `task` is
/// unused.
void func_dryfield_night_water_hole_8017DF28(Task* task)
{
    SVECTOR      v0, v1, v2, v3;
    s32          sxy0, sxy1, sxy2, sxy3;
    s32          p, flag;
    s32          step;
    s32          phase;
    DnwhSurface* e;
    POLY_G4*     poly;
    DR_MODE*     dr;
    s32          otz;
    s32          i;
    s32          half;
    s32          wave;

    e = D_dryfield_night_water_hole_80180970;
    if (Mc_SaveData.companionType == 0) {
        D_dryfield_night_water_hole_80183628 = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_dryfield_night_water_hole_80183628 = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (Gp_StateF0.field_4 == 0) {
        D_dryfield_night_water_hole_8018362C++;
    }
    phase             = -(D_dryfield_night_water_hole_8018362C * 16);
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (; e->y != -1; e++) {
        step = e->width / 64;
        half = (s16)e->depth / 2;
        for (i = 0; i < 64; i++) {
            v0.vx = e->x + step * i;
            v0.vy = e->y;
            v0.vz = e->z;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y;
            v1.vz = e->z;
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v2.vx = e->x + step * i;
            v2.vy = e->y + wave;
            v2.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y + wave;
            v3.vz = e->z + half;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                 = (POLY_G4*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0xFF;
                poly->r1              = 0xFF;
                poly->g0              = 0;
                poly->b0              = 0;
                poly->g1              = 0;
                poly->b1              = 0;
                poly->r2              = 0x20;
                poly->g2              = 0x20;
                poly->b2              = 0x20;
                poly->r3              = 0x20;
                poly->g3              = 0x20;
                poly->b3              = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                   = (DR_MODE*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 64; i++) {
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v0.vx = e->x + step * i;
            v0.vy = e->y + wave;
            v0.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y + wave;
            v1.vz = e->z + half;
            v2.vx = e->x + step * i;
            v2.vy = e->y;
            v2.vz = e->z + half * 2;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y;
            v3.vz = e->z + half * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                 = (POLY_G4*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r2              = 0xFF;
                poly->r3              = 0xFF;
                poly->g2              = 0;
                poly->b2              = 0;
                poly->g3              = 0;
                poly->b3              = 0;
                poly->r0              = 0x20;
                poly->g0              = 0x20;
                poly->b0              = 0x20;
                poly->r1              = 0x20;
                poly->g1              = 0x20;
                poly->b1              = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                   = (DR_MODE*)D_dryfield_night_water_hole_80183628;
                D_dryfield_night_water_hole_80183628 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
}

/// The room's water task: runs its current state -
/// `func_dryfield_night_water_hole_8017E690` once, then
/// `func_dryfield_night_water_hole_8017DF28`, which draws the surfaces - and
/// each tick sets the session's water height to -0x1A4.
void func_dryfield_night_water_hole_8017E630(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_water_hole_8017E690, func_dryfield_night_water_hole_8017DF28 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}

/// The water task's first state: clears the session halfword `field_80`, or
/// `field_7E` while `Mc_SaveData.companionType` is set, then advances to the drawing state.
void func_dryfield_night_water_hole_8017E690(Task* arg0)
{
    if (Mc_SaveData.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task. State 0 installs effect ids 0x600FF / 0x6011F in the two shared
/// effect-id slots while progress nibble 0xB8 is clear, records the world
/// positions of parts 14 and 17 of the slot-3 task's model, and advances.
/// State 1, while nibble 0xB8 is clear, no event is running and `waterY` is
/// below that model's root, spawns each effect at water level under each part
/// with odds that grow with how far the part moved since last frame, then, once
/// game-flag nibble 0x51 is 1, draws the glowing beams
/// `func_dryfield_night_water_hole_8017EA6C` renders between the point pairs
/// the current view selects.
void func_dryfield_night_water_hole_8017E6D0(Task* arg0)
{
    Task*                          ctl;
    s32                            mask;
    _DryfieldNightWaterHoleSplash* splash;
    GpCoord*                       ctlCoords;
    GpCoord*                       part;
    GpCoord*                       view;
    GpCoord                        surface;
    s32                            i;
    u32                            rnd;

    ctl       = gameGetPtrSlot(3);
    splash    = arg0->spawnArg2;
    mask      = 1 << gGameSession->at4.loc.view;
    ctlCoords = ctl->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0xB8) == 0) {
                D_8011574C = 0x600FF;
                D_80115738 = 0x6011F;
            }
            arg0->state = 1;
            for (i = 0; i < 2; i++) {
                part                                       = &ctl->extra.tmd->coords[14 + i * 3];
                D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
            }
            break;
        case 1:
            if (GameFlag_GetNibble(0xB8) == 0 && Gp_State1C->eventState == 0 &&
                gGameSession->waterY < ctlCoords->coord.t[1]) {
                view = &gGfxViewCoord;
                for (i = 0; i < 2; i++) {
                    part = &ctl->extra.tmd->coords[14 + i * 3];
                    Gp_UpdateCoord(part);
                    splash->strength = ABS(D_dryfield_night_water_hole_801809F4[i].vx - part->workm.t[0]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vy - part->workm.t[1]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vz - part->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &part->workm, &surface.coord);
                    surface.sub        = view;
                    surface.coord.t[1] = gGameSession->waterY;
                    surface.flg        = 0;
                    Gp_UpdateCoord(&surface);
                    rnd = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
                    }
                    splash->strength -= 0x20;
                    rnd               = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
                    }
                    D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                    D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                    D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
                }
            }
            if (GameFlag_GetNibble(0x51) == 1) {
                if (mask & 0x18) {
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_80180994[0], 0x100);
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_80180994[2], 0x100);
                }
                if (mask & 0xA50) {
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_801809B4[0], 0x100);
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_801809B4[2], 0x100);
                }
                if (mask & 0x80) {
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_801809D4[0], 0x100);
                    func_dryfield_night_water_hole_8017EA6C(&D_dryfield_night_water_hole_801809D4[2], 0x100);
                }
            }
            break;
    }
}

/// Draws a glowing beam between the two points `arg0[0]` and `arg0[1]`,
/// projected through the view matrix. Each end is a gouraud half-disc of
/// radius `(s16)arg1 * 64 / otz` around its projected point, lit at the centre
/// and dark at the rim, turned to face the other end by the screen-space angle
/// between them; quads join the two discs. The centre brightness flickers
/// between 0x20 and 0x30 with the display frame counter. Nothing is drawn when
/// either projection is invalid. The work block lives on the scratchpad stack.
void func_dryfield_night_water_hole_8017EA6C(SVECTOR* arg0, s32 arg1)
{
    void**                   scratch;
    u8*                      head;
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Per-frame driver of an expanding, fading flash effect. While the room's
/// event state is 0 it updates the task's coordinate, ticks the age counter
/// `age` and draws the flash through
/// `func_dryfield_night_water_hole_8017F3A8` at size `angle` and brightness
/// `scale`. The first frame sets the brightness to 0x40, takes the size from
/// the spawn argument's low 12 bits and turns the coordinate about Y by a
/// random angle; every frame then grows the size by 0x20 and dims the
/// brightness by 2, releasing the work block once it falls under 2. Once the
/// event state is non-zero it only draws, releasing the block from event state
/// 4 on.
void func_dryfield_night_water_hole_8017F254(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_dryfield_night_water_hole_8017F3A8(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->angle += 0x20;
        func_dryfield_night_water_hole_8017F3A8(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
void func_dryfield_night_water_hole_8017F3A8(GpCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &arg0->workm;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = (u16)block->sxy0.vx;
        prim->y0 = (u16)block->sxy0.vy;
        prim->x1 = (u16)block->sxy1.vx;
        prim->y1 = (u16)block->sxy1.vy;
        prim->x2 = (u16)block->sxy2.vx;
        prim->y2 = (u16)block->sxy2.vy;
        prim->x3 = (u16)block->sxy3.vx;
        prim->y3 = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Per-frame driver of a particle effect, drawn as the spinning sprite of
/// `func_dryfield_night_water_hole_8017FB98` (state 1) or, when the spawn
/// argument's top nibble is set, the upright sprite of
/// `func_dryfield_night_water_hole_8017FF84` (state 2). The first frame takes
/// the size from the argument's low 12 bits, a random spin angle, and the ticks
/// per animation frame from bits 12-15. Unless the work block already carries a
/// velocity it picks one by the kind in bits 24-27 - none, a random upward
/// burst, a random spray, a narrow upward jet, or the block's stored direction
/// - scaled to the speed in bits 16-23 (0x40 when zero). Every later tick
/// draws, moves the coordinate by the velocity with gravity pulling it down,
/// and releases the block after animation frame 7. While the room's event
/// state is non-zero it only draws, releasing the block from event state 4 on.
void func_dryfield_night_water_hole_8017F6DC(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_dryfield_night_water_hole_8017FB98(coord, (u16)work->index, work->scale, work->angle);
            } else {
                func_dryfield_night_water_hole_8017FF84(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_dryfield_night_water_hole_8017FB98(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            func_dryfield_night_water_hole_8017FF84(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued, its texture the 32-texel
/// column `arg1` of the strip at v 0xE0..0xFF. Its corners sit at
/// `(s16)arg2 * 31 / otz` from the projected point, rotated by the angle
/// `arg3`. The work block lives on the scratchpad stack.
void func_dryfield_night_water_hole_8017FB98(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued, its texture the 56-texel
/// cell `arg1 & 7` of a four-wide, two-row grid starting at v 0x70. The quad
/// is `2 * r` on a side with `r = (s16)arg2 * 55 / otz`, and the projected
/// point sits a quarter of the way up from its bottom edge. The work block
/// lives on the scratchpad stack.
void func_dryfield_night_water_hole_8017FF84(GpCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    u32            cell;
    s32            u1;
    s32            vbase;
    s32            v0;
    s32            v1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    tex                                     = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = (u16)tex;
        tex         = (cell & 3) * 0x38;
        vbase       = ((cell & 7) >> 2) * 0x38;
        v0          = vbase + 0x70;
        SOFT_USE_REG(v0);
        u1       = tex + 0x37;
        v1       = vbase - 0x59;
        prim->v2 = v1;
        prim->v3 = v1;
        TOUCH_REG(u1);
        sarg        = (s16)arg2;
        prim->v0    = v0;
        prim->v1    = v0;
        t           = sarg * 0x38;
        prim->u0    = tex;
        prim->u1    = u1;
        prim->u2    = tex;
        prim->u3    = u1;
        block->step = (t - sarg) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        v1          = ((u16)block->sy - (u16)block->step) - (block->step >> 1);
        xy          = v1;
        ds          = &gDisplayState;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (block->step >> 1);
        prim->y3    = xy;
        prim->y2    = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}
