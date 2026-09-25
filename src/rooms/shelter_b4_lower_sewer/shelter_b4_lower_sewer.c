#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// One water surface: its near edge starts at (`x`, `z`) and runs `step` along
/// X; its far edge sits `dz` further along Z. A surface whose `end` is -1
/// terminates the list.
typedef struct {
    s16 x;
    s16 z;
    s16 step;
    s16 dz;
    s16 end;
} _Surface;

/// Working copy of one surface's extents, carved off the scratchpad stack.
typedef struct {
    s16 y;
    s16 step;
    s16 dy;
    s16 dz;
    s16 x;
    s16 z;
} _SurfaceScratch;

extern s32 D_8007107C;
extern s8  D_8007217B;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern GpMsgEntry D_shelter_b4_lower_sewer_80181E44[];
/// The room's water height: the level the surfaces are drawn at and the value
/// the water task publishes to the session.
extern s16      D_shelter_b4_lower_sewer_80181E6C;
extern TaskDesc D_shelter_b4_lower_sewer_80181E70[];
extern _Surface D_shelter_b4_lower_sewer_80181E7C[];
extern _Surface D_shelter_b4_lower_sewer_80181E90[];

/// Primitive cursor the water drawers allocate their quads and draw-mode
/// packets from; the water task's drawing state points it into the primitive
/// area each frame before drawing.
extern u8* D_shelter_b4_lower_sewer_80183E14;

void func_shelter_b4_lower_sewer_8017E33C(Task* arg0);
void func_shelter_b4_lower_sewer_8017E37C(Task* task);

/// Handler for message 0x13F1 in the room's message table
/// `D_shelter_b4_lower_sewer_80181E44`: does nothing and returns 0.
s32 func_shelter_b4_lower_sewer_8017D608(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both on to `func_80179A04`. Always returns 1.
s32 func_shelter_b4_lower_sewer_8017D610(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// returns 0.
s32 func_shelter_b4_lower_sewer_8017D654(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// returns 0.
s32 func_shelter_b4_lower_sewer_8017D65C(void)
{
    return 0;
}

/// First state of the room task: installs the room's message table, takes
/// game pointer slot 7 and, once GameFlag nibble 0xB7 is set, spawns the
/// tasks of `D_shelter_b4_lower_sewer_80181E70`.
void func_shelter_b4_lower_sewer_8017D664(Task* task)
{
    task->msgTable = D_shelter_b4_lower_sewer_80181E44;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xB7) != 0) {
        Task_SpawnFromTable(D_shelter_b4_lower_sewer_80181E70, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state.
void func_shelter_b4_lower_sewer_8017D6CC(Task* task)
{
}

/// State handlers of the room task `func_shelter_b4_lower_sewer_8017D6D4`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
const TaskFuncTable3 D_shelter_b4_lower_sewer_8017D5C4 = {
    { func_shelter_b4_lower_sewer_8017D664, func_shelter_b4_lower_sewer_8017D6CC, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b4_lower_sewer_8017D5C4`, copying the table onto the stack and
/// calling the entry for the task's current state.
void func_shelter_b4_lower_sewer_8017D6D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_lower_sewer_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E7C` at height
/// `D_shelter_b4_lower_sewer_80181E6C` as two strips of 32 semi-transparent
/// Gouraud quads laid side by side along Z, each strip running along X. The
/// seam between the strips is lifted by a sine wave whose phase advances with
/// the frame counter. The outer edges are coloured (0, 0x20, 0x80) and the seam
/// (0x20, 0x20, 0x20); each quad is followed by a draw-mode packet selecting
/// blend mode 2. Quads the projection flags as invalid are skipped. Called
/// from the water task's drawing state with the task, which it does not read.
void func_shelter_b4_lower_sewer_8017D72C(Task* task)
{
    SVECTOR          v0, v1, v2, v3;
    s32              sxy0, sxy1, sxy2, sxy3;
    s32              p, flag;
    _Surface*        e;
    u8*              head;
    _SurfaceScratch* s;
    s32              phase;
    POLY_G4*         poly;
    DR_MODE*         dr;
    s32              otz;
    s32              i;

    e                 = D_shelter_b4_lower_sewer_80181E7C;
    gGfxViewCoord.flg = 0;
    head              = SCRATCH_HEAD(u8);
    phase             = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)  = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; e->end != -1; e++) {
        s->step = e->step / 32;
        s->dz   = e->dz / 2;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < 32; i++) {
            v0.vx = s->x + s->step * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->dz;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0;
                poly->g0         = 0x20;
                poly->b0         = 0x80;
                poly->r1         = 0;
                poly->g1         = 0x20;
                poly->b1         = 0x80;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
        for (i = 0; i < 32; i++) {
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx = s->x + s->step * i;
            v0.vy = s->y + s->dy;
            v0.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y + s->dy;
            v1.vz = s->z + s->dz;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y;
            v2.vz = s->z + s->dz * 2;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y;
            v3.vz = s->z + s->dz * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0;
                poly->g2         = 0x20;
                poly->b2         = 0x80;
                poly->r3         = 0;
                poly->g3         = 0x20;
                poly->b3         = 0x80;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Draws each surface in `D_shelter_b4_lower_sewer_80181E90` as a strip of 8
/// Gouraud semi-transparent quads laid along X at height
/// `D_shelter_b4_lower_sewer_80181E6C`. The far edge of each quad is lifted by
/// a sine wave whose phase advances with the frame counter, so the surface
/// ripples. Each quad is followed by a draw-mode packet selecting blend mode 2;
/// quads the projection flags as invalid are skipped. Called from the water
/// task's drawing state with the task, which it does not read.
void func_shelter_b4_lower_sewer_8017DE8C(Task* task)
{
    SVECTOR          v0, v1, v2, v3;
    s32              sxy0, sxy1, sxy2, sxy3;
    s32              p, flag;
    s32              phase;
    u8*              head;
    _SurfaceScratch* s;
    _Surface*        e;
    POLY_G4*         poly;
    DR_MODE*         dr;
    s32              otz;
    s32              i;

    e                 = D_shelter_b4_lower_sewer_80181E90;
    gGfxViewCoord.flg = 0;
    head              = SCRATCH_HEAD(u8);
    phase             = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)  = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_lower_sewer_80181E6C;
    for (; e->end != -1; e++) {
        s->step = e->step / 8;
        s->dz   = e->dz;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < 8; i++) {
            v0.vx = s->x + s->step * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->step * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx = s->x + s->step * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->dz;
            s->dy = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx = s->x + s->step * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->dz;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0;
                poly->g0         = 0x20;
                poly->b0         = 0x80;
                poly->r1         = 0;
                poly->g1         = 0x20;
                poly->b1         = 0x80;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b4_lower_sewer_80183E14;
                D_shelter_b4_lower_sewer_80183E14 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)((((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt + 4),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The room's water task: runs its state (`func_shelter_b4_lower_sewer_8017E33C`
/// once, then `func_shelter_b4_lower_sewer_8017E37C` every frame) and publishes
/// `D_shelter_b4_lower_sewer_80181E6C` as the session's water height.
void func_shelter_b4_lower_sewer_8017E2D4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_lower_sewer_8017E33C, func_shelter_b4_lower_sewer_8017E37C };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_lower_sewer_80181E6C;
}

/// First state of the water task: clears the session's `field_80` or
/// `field_7E`, chosen by `D_8007217B`, and advances to the next state.
void func_shelter_b4_lower_sewer_8017E33C(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Drawing state of the water task: points the primitive cursor
/// `D_shelter_b4_lower_sewer_80183E14` at `D_8005C374` or `D_8005C370`, chosen
/// by `D_8007217B`, plus 0xC000 bytes per `D_8007107C`, then draws both sets
/// of water surfaces.
void func_shelter_b4_lower_sewer_8017E37C(Task* task)
{
    if (D_8007217B == 0) {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b4_lower_sewer_8017D72C(task);
    func_shelter_b4_lower_sewer_8017DE8C(task);
}
