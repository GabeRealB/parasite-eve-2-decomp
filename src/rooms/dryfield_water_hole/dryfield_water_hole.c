#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// One rectangle of water surface drawn by `func_dryfield_water_hole_8017D898`,
/// in world coordinates: it spans `width` along X from `x` and `depth` along Z
/// from `z`, at height `y`. The table ends at the first entry whose `y` word is
/// -1; the drawing code reads only its low half as the height.
typedef struct {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} _DryfieldWaterHoleSurface;

/// Block the room's splash task receives as `spawnArg2`. Only the halfword at
/// 0x26 is touched: an effect strength, set from how far a tracked part moved
/// this frame and used as the odds of spawning each of the two effects.
typedef struct {
    byte pad_0[0x26];
    s16  strength;
} _DryfieldWaterHoleSplash;

extern s32 D_80115738;
extern s32 D_8011574C;

/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF, 0x13F0
/// and 0x13F2.
extern GpMsgEntry D_dryfield_water_hole_8017FC5C[];
/// Descriptor of the room's water task, spawned by the room task's entry tick.
/// Its callback is `func_dryfield_water_hole_8017DFA0`.
extern TaskDesc D_dryfield_water_hole_8017FC8C[];
/// The room's water surfaces, terminated by an entry with `y == -1`.
extern _DryfieldWaterHoleSurface D_dryfield_water_hole_8017FC98[];
/// Point pairs of the glowing beams the splash task draws, one table per group
/// of views.
extern SVECTOR D_dryfield_water_hole_8017FCC4[];
extern SVECTOR D_dryfield_water_hole_8017FCDC[];
extern SVECTOR D_dryfield_water_hole_8017FD04[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_water_hole_8017FD1C[];
/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_water_hole_801828CC;
/// Frame counter the water surface's wave is phased by.
extern s16 D_dryfield_water_hole_801828D0;

void func_dryfield_water_hole_8017E000(Task* arg0);
void func_dryfield_water_hole_8017E410(GpCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);
void func_dryfield_water_hole_8017EDE4(GpCoord* arg0, s32 arg1, s32 arg2);

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_water_hole_8017D5E8(void)
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
s32 func_dryfield_water_hole_8017D5F0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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

/// Handler for message 0x13F0 in the room's message table. Only the command 2
/// in `arg2` concerns this room: it arms cap command 2, records it in progress
/// nibble 0x1BD and plays sound event 0x52200004. Always returns 0.
s32 func_dryfield_water_hole_8017D73C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(2);
        GameFlag_SetNibble(0x1BD, 2);
        SndEvt_EnqueueType6(0x52200004, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_water_hole_8017D784(void)
{
    return 0;
}

/// Handler for message 0x13F2 in the room's message table: plays sound event
/// 0x52200004 for the command 4 in `arg2` and 0x52200005 for 5. Always
/// returns 0.
s32 func_dryfield_water_hole_8017D78C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            SndEvt_EnqueueType6(0x52200004, 0, 0);
            break;
        case 5:
            SndEvt_EnqueueType6(0x52200005, 0, 0);
            break;
    }
    return 0;
}

/// Room task entry tick: publishes the room's message table in
/// `Task::msgTable`, claims game pointer slot 7, spawns the room's water task
/// from `D_dryfield_water_hole_8017FC8C` and advances state.
void func_dryfield_water_hole_8017D7DC(Task* arg0)
{
    arg0->msgTable = D_dryfield_water_hole_8017FC5C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_water_hole_8017FC8C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state, entry 1 of its three-state table: does
/// nothing.
void func_dryfield_water_hole_8017D838(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_water_hole_8017D840`: the entry tick, the idle state, then
/// `taskKill`.
const TaskFuncTable3 D_dryfield_water_hole_8017D5C4 = {
    { func_dryfield_water_hole_8017D7DC, func_dryfield_water_hole_8017D838, taskKill },
};

/// The room task: copies the three-state table
/// `D_dryfield_water_hole_8017D5C4` onto the stack and runs the entry for the
/// task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_water_hole_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_hole_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_dryfield_water_hole_8017FC98` as two strips of 64
/// semi-transparent Gouraud quads laid side by side along Z, projected through
/// the view matrix. The seam between the strips is lifted by a sine wave that
/// runs along X and scrolls with `D_dryfield_water_hole_801828D0`, which only
/// advances while `Gp_StateF0.field_4` is clear. The outer edges are coloured
/// (0xFF, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. `task` is unused.
void func_dryfield_water_hole_8017D898(Task* task)
{
    SVECTOR                    v0, v1, v2, v3;
    s32                        sxy0, sxy1, sxy2, sxy3;
    s32                        p, flag;
    s32                        step;
    s32                        phase;
    _DryfieldWaterHoleSurface* e;
    POLY_G4*                   poly;
    DR_MODE*                   dr;
    s32                        otz;
    s32                        i;
    s32                        half;
    s32                        wave;

    e = D_dryfield_water_hole_8017FC98;
    if (Mc_SaveData.companionType == 0) {
        D_dryfield_water_hole_801828CC = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_dryfield_water_hole_801828CC = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (Gp_StateF0.field_4 == 0) {
        D_dryfield_water_hole_801828D0++;
    }
    phase             = -(D_dryfield_water_hole_801828D0 * 16);
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
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
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
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
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
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
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
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
}

/// The room's water task: runs its current state -
/// `func_dryfield_water_hole_8017E000` once, then
/// `func_dryfield_water_hole_8017D898`, which draws the surfaces - and each
/// tick sets the session's water height to -0x1A4.
void func_dryfield_water_hole_8017DFA0(Task* task)
{
    TaskFunc states[2] = { func_dryfield_water_hole_8017E000, func_dryfield_water_hole_8017D898 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}

/// The water task's first state: clears the session halfword `field_80`, or
/// `field_7E` while `Mc_SaveData.companionType` is set, then advances to the drawing state.
void func_dryfield_water_hole_8017E000(Task* arg0)
{
    if (Mc_SaveData.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task. State 0 installs effect ids 0x600FD / 0x600FE in the two shared
/// effect-id slots, records the world positions of parts 14 and 17 of the
/// slot-3 task's model, and advances. State 1, while no event is running and
/// `waterY` is below that model's root, spawns each effect at water level under
/// each part with odds that grow with how far the part moved since last frame,
/// then, once game-flag nibble 0x51 is 1, draws the glowing beams
/// `func_dryfield_water_hole_8017E410` renders between the point pairs the
/// current view selects.
void func_dryfield_water_hole_8017E040(Task* arg0)
{
    Task*                     ctl;
    s32                       mask;
    _DryfieldWaterHoleSplash* splash;
    GpCoord*                  coord;
    GpCoord*                  ctlCoords;
    GpCoord*                  part;
    GpCoord*                  view;
    GpCoord                   surface;
    s32                       i;
    u32                       rnd;

    ctl       = gameGetPtrSlot(3);
    mask      = 1 << gGameSession->at4.loc.view;
    splash    = arg0->spawnArg2;
    coord     = arg0->extra.tmd->coords;
    ctlCoords = ctl->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            D_8011574C  = 0x600FD;
            D_80115738  = 0x600FE;
            arg0->state = 1;
            for (i = 0; i < 2; i++) {
                part                                 = &ctl->extra.tmd->coords[14 + i * 3];
                D_dryfield_water_hole_8017FD1C[i].vx = part->workm.t[0];
                D_dryfield_water_hole_8017FD1C[i].vy = part->workm.t[1];
                D_dryfield_water_hole_8017FD1C[i].vz = part->workm.t[2];
            }
            break;
        case 1:
            if (Gp_State1C->eventState == 0 && gGameSession->waterY < ctlCoords->coord.t[1]) {
                view = &gGfxViewCoord;
                for (i = 0; i < 2; i++) {
                    part = &ctl->extra.tmd->coords[14 + i * 3];
                    Gp_UpdateCoord(part);
                    splash->strength = ABS(D_dryfield_water_hole_8017FD1C[i].vx - part->workm.t[0]) +
                                       ABS(D_dryfield_water_hole_8017FD1C[i].vy - part->workm.t[1]) +
                                       ABS(D_dryfield_water_hole_8017FD1C[i].vz - part->workm.t[2]) + 0x20;
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
                    D_dryfield_water_hole_8017FD1C[i].vx = part->workm.t[0];
                    D_dryfield_water_hole_8017FD1C[i].vy = part->workm.t[1];
                    D_dryfield_water_hole_8017FD1C[i].vz = part->workm.t[2];
                }
            }
            if (GameFlag_GetNibble(0x51) == 1) {
                if (mask & 0x18) {
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FCC4[0], &D_dryfield_water_hole_8017FCC4[-1], 0x100);
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FCC4[2], &D_dryfield_water_hole_8017FCC4[1], 0x100);
                }
                if (mask & 0x50) {
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FCDC[0], &D_dryfield_water_hole_8017FCDC[1], 0x100);
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FCDC[2], &D_dryfield_water_hole_8017FCDC[3], 0x100);
                }
                if (mask & 0x80) {
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FD04[0], &D_dryfield_water_hole_8017FD04[-1], 0x100);
                    func_dryfield_water_hole_8017E410(coord, &D_dryfield_water_hole_8017FD04[2], &D_dryfield_water_hole_8017FD04[1], 0x100);
                }
            }
            break;
    }
}

/// Draws a glowing beam between the points `arg1` and `arg2` of `arg0`'s local
/// space. Both are moved to world space through `arg0->workm` and projected
/// through `GsWSMATRIX`; nothing is drawn when the far end's `otz` is below
/// 0x11, and the near end's is clamped up to 0x10. Each end is a Gouraud
/// half-disc of radius `(s16)arg3 * 64 / otz`, black at the rim and lit at the
/// centre - the near end over angles 0..0x800, the far end over 0x800..0x1000 -
/// and a quad at angles 0 and 0x800 joins the two discs. The centre brightness
/// flickers between 0x20 and 0x30 with the display frame counter. Each
/// primitive takes a `Gp_AddTpageShift` tpage; the far disc sorts by the far
/// end's `otz`, everything else by the near end's. The work block lives on the
/// scratchpad stack.
void func_dryfield_water_hole_8017E410(GpCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    (u16) block->vec0.vx = (u16)block->vec0.vx + (u16)arg0->workm.t[0];
    (u16) block->vec0.vy = (u16)block->vec0.vy + (u16)arg0->workm.t[1];
    (u16) block->vec0.vz = (u16)block->vec0.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    (u16) block->vec1.vx = (u16)block->vec1.vx + (u16)arg0->workm.t[0];
    (u16) block->vec1.vy = (u16)block->vec1.vy + (u16)arg0->workm.t[1];
    (u16) block->vec1.vz = (u16)block->vec1.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
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
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x28);
}

/// Per-frame driver of an expanding, fading flash effect. While the room's
/// event state is 0 it updates the task's coordinate, ticks the age counter
/// `age` and draws the flash through
/// `func_dryfield_water_hole_8017EDE4` at size `angle` and brightness
/// `scale`. The first frame sets the brightness to 0x40, takes the size from
/// the spawn argument's low 12 bits and turns the coordinate about Y by a
/// random angle; every frame then grows the size by 0x20 and dims the
/// brightness by 2, releasing the work block once it falls under 2. Once the
/// event state is non-zero it only draws, releasing the block from event state
/// 4 on.
void func_dryfield_water_hole_8017EC90(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_dryfield_water_hole_8017EDE4(coord, work->angle, work->scale);
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
        func_dryfield_water_hole_8017EDE4(coord, work->angle, work->scale);
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
void func_dryfield_water_hole_8017EDE4(GpCoord* arg0, s32 arg1, s32 arg2)
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
        TOUCH_REG(v);
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
