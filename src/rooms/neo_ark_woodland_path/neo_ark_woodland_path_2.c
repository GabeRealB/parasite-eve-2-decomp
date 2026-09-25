#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/stdio.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// The distance between `a` and `b`, spelled as a conditional subtraction.
#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

/// Gameplay's effect ids for the two effects this room scatters, set by
/// `func_neo_ark_woodland_path_8017EA08` on its first run and passed to
/// `Gp_SpawnEff`. Gameplay clears both when its effect state is reset.
extern s32 D_80115738;
extern s32 D_8011574C;

/// The room's frame countdown at `D_neo_ark_woodland_path_8018498E`. Signed,
/// although the arithmetic reads compile as `lhu` (`func_...8018154C` adds to
/// it, `func_...80180DDC` counts it down): a load whose result is truncated by
/// the following `sh` only has to supply the low half, so GCC picks the
/// unsigned form by itself, while the `lh` comparisons and the -1 the 0x7DB
/// handler stores need the signed declaration. A union offering both views
/// compiles the same instructions but marks every access `in_struct`, and that
/// flag decides the scheduler's dependence analysis - it pinned a load after a
/// store in `func_neo_ark_woodland_path_80180C6C`. See
/// `DECOMPILATION_LEARNINGS.md`, "A union that only names a view costs
/// `in_struct`".
extern s16 D_neo_ark_woodland_path_8018498E;

/// The object `Task::spawnArg2` holds for this room: the pending spawn
/// parameter handed over by `D_neo_ark_woodland_path_80184A60`, and the state
/// byte the handler clears once it has been taken. Trailing pad keeps pointer
/// alignment; the full object size is not known yet.
///
/// The halfword at 0x40 is reached through a cast rather than as a member,
/// because the original compiles it as a *scalar* reference: `MEM_IN_STRUCT_P`
/// (the `/s` on a MEM) feeds the scheduler's dependence analysis, and as a
/// member access the neighbouring `D_neo_ark_woodland_path_80184A60` store is
/// scheduled differently - see `DECOMPILATION_LEARNINGS.md`, "Scalar memory
/// references".
typedef struct NeoArkWoodlandPathObj {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[3];
} NeoArkWoodlandPathObj;

extern RoomActorMsg D_neo_ark_woodland_path_80184A5C;

/// The room's five spawn slots: `func_neo_ark_woodland_path_8018046C` fills the
/// first free one with a countdown and `func_neo_ark_woodland_path_80180B18`
/// hands slot 0 to the spawn it triggers and clears it. Read as `lhu` by the
/// handler and as `lh` by the slot filler, so each site names the view it uses
/// (`[0]` here, an `s16*` cast there).
extern u16 D_neo_ark_woodland_path_80184A60[5];

/// Ceiling `func_neo_ark_woodland_path_8018046C` clamps a spawn slot to
/// (0x1A4, 420 frames). Only the first halfword is this unit's; the run
/// continues into the room's parameter block, so the extent is splat's.
extern u16 D_neo_ark_woodland_path_8018494C[18];

/// The same run reached through its leading label, which is how
/// `func_neo_ark_woodland_path_80180C6C` reads the ceiling: element 2 is
/// `D_...8494C[0]`, 420 frames. splat names both addresses because the compiled
/// code names both, and the two are different code - an index keeps this
/// symbol in a register and takes the offset as the load's displacement, while
/// naming `D_...8494C` addresses it directly.
extern u16 D_neo_ark_woodland_path_80184948[];

/// The room's arming count, packed into game flag 0x10A as a nibble:
/// `func_neo_ark_woodland_path_80180C6C` adds the slot's spawn count to it and
/// then caps it at 5, the number of slots `D_...84A60` has. Signed, though the
/// add reads it as `lhu` - the result is truncated by the following `sh`, so
/// only the low half matters and GCC picks the unsigned load by itself.
extern s16 D_neo_ark_woodland_path_80184990;

/// How many spawns each slot arms, indexed by `gGameSession->at4.loc.place` (the
/// slot the session is in): the byte `func_...80180C6C` adds to
/// `D_...80184990`, and the gate `func_...80180DDC` tests against zero.
extern u8 D_neo_ark_woodland_path_80184970[];

/// The message-handler table `func_neo_ark_woodland_path_80180C6C` parks in
/// `Task::msgTable`: a placement request (0x13EF,
/// `func_neo_ark_woodland_path_80181568`), a countdown bump (0x13F4) and the
/// 0x7DB command handler `func_neo_ark_woodland_path_80180B18`.
extern GpMsgEntry D_neo_ark_woodland_path_801849F4[];

/// The same gate for the arm-state one step earlier: `func_...80180568` tests
/// it against zero and `func_...801806D8` reads the slot's count from it. One
/// byte per session slot, indexed by `gGameSession->at4.loc.place`, like
/// `D_...84970` above.
extern u8 D_neo_ark_woodland_path_80184980[];

/// `func_...80180568`'s own message-handler table, parked in `Task::msgTable`
/// as `D_...849F4` is by `func_...80180C6C`: the same three ids, answered by
/// the placement request `func_neo_ark_woodland_path_8018147C`, the spawn-slot
/// filler `func_neo_ark_woodland_path_8018046C` and a 0x7DB handler that
/// ignores the message.
extern GpMsgEntry D_neo_ark_woodland_path_80184998[];

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 D_neo_ark_woodland_path_80184996;

/// Spawn point requested by the room's 0x13EF handlers (the message's third
/// byte, taken only while `D_...8498E` has run out and the byte differs from
/// the previous request). One-based index into the placement table of the
/// running sequence; zero means no request, and the per-frame states clear it
/// every frame whether or not they placed a spawn.
extern s16 D_neo_ark_woodland_path_80184992;

/// The byte the last 0x13EF message carried, kept so that a repeated request
/// is dropped rather than placed again.
extern s16 D_neo_ark_woodland_path_80184994;

/// A placement `func_...801806D8` puts a spawned task at: the x and z it writes
/// into the task's coordinate translation (y is always zero) and the Y
/// rotation it hands `Gfx_RotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkWoodlandPathSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkWoodlandPathSpawnPos;

/// The room's spawn placements, indexed by `D_...80184992 - 1`.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_801849B8[];

/// The second arming sequence's spawn placements, which
/// `func_...80180DDC` picks from by `D_...80184992 - 1`. Five of them; any
/// request past the fourth takes the last.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_80184A14[5];

/// `Gp_StateF0.field_6` as `func_...801806D8` saw it on the previous frame, so
/// that it can tell the reference count was non-zero before the frame began.
extern s16 D_neo_ark_woodland_path_801849F0;

/// The object `Task::spawnArg2` holds for the task that runs
/// `func_neo_ark_woodland_path_8017EA08`. Only the halfword at 0x26 is known:
/// the chance, out of 0x200, of spawning an effect this frame. It is recomputed
/// from how far the tracked model parts moved. Nothing yet shows whether this
/// is the same object as `NeoArkWoodlandPathObj`.
typedef struct NeoArkWoodlandPathTrailObj {
    /* 0x00 */ byte pad_0[0x26];
    /* 0x26 */ s16  chance;
} NeoArkWoodlandPathTrailObj;

/// World position (`workm.t`) of model parts 15 and 18 of the slot-3 task, as
/// `func_neo_ark_woodland_path_8017EA08` last saw them. Each frame it compares
/// the parts against these entries to measure how far they moved.
extern SVECTOR D_neo_ark_woodland_path_80181684[2];

void func_neo_ark_woodland_path_8017F154(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_neo_ark_woodland_path_8017F5F4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_neo_ark_woodland_path_8017FDE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_neo_ark_woodland_path_801801D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);

/// Scatters effects around the slot-3 task's model while its root coordinate
/// is at a y of 0x12C or more (y grows downward) and no event is running.
/// Once per frame, the spawn chance is set from how far model parts 15 and 18
/// moved since the previous frame. Two effects are rolled at the model's x and
/// z with y fixed at 0xC8: effect `D_8011574C` against that chance, then effect
/// `D_80115738` against the chance less 0x20. The same function also sets the
/// room effect mode to 2 while the root y is below 0x11. On its first run it
/// stores the two effect ids and the starting part positions.
void func_neo_ark_woodland_path_8017EA08(Task* task)
{
    NeoArkWoodlandPathTrailObj* obj;
    Task*                       owner;
    GsCOORDINATE2*              root;
    GsCOORDINATE2*              part;
    GsCOORDINATE2               coord;
    s32                         i;

    obj   = task->spawnArg2;
    owner = gameGetPtrSlot(3);
    root  = ((TmdObject*)owner->extra)->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60058;
        D_80115738  = 0x60187;
        task->state = 1;
        for (i = 0; i < 2; i++) {
            part                                   = &((TmdObject*)owner->extra)->coords[i * 3 + 15];
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
    Gp_State1C->roomEffectMode = (root->coord.t[1] < 0x11) * 2;
    if (Gp_State1C->eventState == 0 && root->coord.t[1] >= 0x12C) {
        for (i = 0; i < 2; i++) {
            part             = &((TmdObject*)owner->extra)->coords[i * 3 + 15];
            obj->chance      = ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vx, part->workm.t[0]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vy, part->workm.t[1]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vz, part->workm.t[2]) + 0x20;
            coord.sub        = root->sub;
            coord.coord      = root->coord;
            coord.coord.t[0] = root->coord.t[0];
            coord.coord.t[1] = 0xC8;
            coord.coord.t[2] = root->coord.t[2];
            coord.flg        = 0;
            Gp_UpdateCoord(&coord);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_8011574C, &coord, 0x40, 0);
            }
            obj->chance -= 0x20;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_80115738, &coord, 0x1202180, 0);
            }
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
}

/// One drifting mote of the room's ambient effect, drawn through
/// `func_neo_ark_woodland_path_8017F154`. State 0 seeds it from `Gp_LcgState`:
/// a size of 0x20, a random tilt pair (`field_28` / `field_2A`) and a random
/// drift in `field_10`. State 1 flies it: the drift moves the coordinate's
/// translation and the tilt rotates it about X and Z; each drift axis eases
/// back towards zero by one a tick and re-rolls a fresh multiple of 8 when it
/// gets there, and the tilt wanders by a random step. Once the mote has risen
/// past the origin (`t[1] > 0`) state 2 fades it in by 0x10 a tick up to 0x80
/// and state 3 fades it out, releasing the work block when the fade runs out.
void func_neo_ark_woodland_path_8017ED00(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            vy;
    s32            vx;
    s32            vz;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24    = 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_2A    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state       = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->field_10.vx;
            coord->coord.t[1] += work->field_10.vy;
            coord->coord.t[2] += work->field_10.vz;
            Gfx_RotMatrixX(&coord->coord, (s16)work->field_28, 0);
            Gfx_RotMatrixZ(&coord->coord, (s16)work->field_2A, 0);
            coord->flg = 0;

            vy = work->field_10.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->field_10.vy = vy;

            vx = work->field_10.vx;
            if (vx == 0) {
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->field_10.vx = vx;
            }

            vz = work->field_10.vz;
            if (vz == 0) {
                work->field_10.vz += (s16)work->field_2A % 32;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->field_10.vz = vz;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_28 += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_neo_ark_woodland_path_8017F154(coord, (s16)work->field_24, 0);
            break;
        case 2:
            if ((s16)work->field_26 < 0x80) {
                work->field_26 += 0x10;
            } else {
                task->state = 3;
            }
            func_neo_ark_woodland_path_8017F154(coord, (s16)work->field_24, 0);
            break;
        case 3:
            if ((s16)work->field_26 >= 0x11) {
                work->field_26 -= 0x10;
                func_neo_ark_woodland_path_8017F154(coord, (s16)work->field_24, (s16)work->field_26);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one mote of the room's ambient effect: the unit quad `D_80111E38`
/// scaled by `arg1`, rotated by the mote's own coordinate and offset by its
/// world translation, then projected through `GsWSMATRIX` (the first corner
/// with `rtps`, the other three with `rtpt`) in a 0x38-byte `G_SCRATCH_HEAD`
/// block. When the GTE flag is non-negative it queues one `POLY_FT4` (tpage
/// 0x2B, clut 0x4390, an 8x8 texel tile at 0,0x28). `arg2` is the fade level:
/// zero draws the raw texture, otherwise the quad is semi-transparent and
/// modulated by the grey `(arg2, arg2, arg2)`.
void func_neo_ark_woodland_path_8017F154(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
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

    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &arg0->workm;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - 0x38;
    SOFT_TOUCH_REG(head);
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
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
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
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
        if (arg2 != 0) {
            setSemiTrans(prim, 1);
            setRGB0(prim, arg2, arg2, arg2);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        prim->v0    = 0x28;
        prim->v1    = 0x28;
        prim->u0    = 0;
        prim->u1    = 7;
        prim->u2    = 0;
        prim->v2    = 0x2F;
        prim->u3    = 7;
        prim->v3    = 0x2F;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// `Gp_State1C` effect task drawing a growing, fading quad through
/// `func_neo_ark_woodland_path_8017F5F4`. The first frame sets the brightness
/// to 0x40, takes the size from the spawn parameter's low 12 bits and turns
/// the coordinate to a random Y rotation. Every frame then rebuilds the
/// coordinate, grows the size by 0x20, draws, and dims by 2, releasing the
/// effect once the brightness falls under 2. Once the room's event state
/// leaves zero it only draws, and releases at state 4.
void func_neo_ark_woodland_path_8017F4A0(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_neo_ark_woodland_path_8017F5F4(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        if (task->state == 0) {
            work->field_24 = 0x40;
            work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->field_26 += 0x20;
        func_neo_ark_woodland_path_8017F5F4(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it by `arg0->workm`
/// (no GTE translation) and adds `workm.t`, then projects the four corners
/// through `GsWSMATRIX`. When `gte_stflg` is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x43D1, UV 0,0x38..0x37,0x6F)
/// coloured `(arg2, arg2, arg2)`.
void func_neo_ark_woodland_path_8017F5F4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
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
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
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
        prim->x0 = *(u16*)&block->sxy0.vx;
        prim->y0 = *(u16*)&block->sxy0.vy;
        prim->x1 = *(u16*)&block->sxy1.vx;
        prim->y1 = *(u16*)&block->sxy1.vy;
        prim->x2 = *(u16*)&block->sxy2.vx;
        prim->y2 = *(u16*)&block->sxy2.vy;
        prim->x3 = *(u16*)&block->sxy3.vx;
        prim->y3 = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// Per-frame update of an effect task drawn with
/// `func_neo_ark_woodland_path_8017FDE4` (state 1) or
/// `func_neo_ark_woodland_path_801801D0` (state 2). State 0 seeds the work
/// from `spawnArg1`: the two draw parameters (the second one random), the
/// frame period and, when the spawner left `field_10` zero, a velocity chosen
/// by the top nibble and scaled to `field_2A` through the GTE. Later ticks draw, drift the coordinate by
/// that velocity with `vy` growing by 6 each tick, and advance the frame every
/// `field_28` ticks, releasing the task after frame 7. While an event is
/// running the task only draws, and it is released once the event state
/// reaches 4.
void func_neo_ark_woodland_path_8017F928(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_neo_ark_woodland_path_8017FDE4(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_neo_ark_woodland_path_801801D0(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_neo_ark_woodland_path_8017FDE4(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_neo_ark_woodland_path_801801D0(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) spun about the projected centre.
/// `arg1` selects the 32-texel UV column `(arg1 & 0xFFFF) << 5` at v=0xE0..0xFF.
/// `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 31 / otz`. `arg3` is the spin angle, applied at `arg3` and
/// `arg3 + 0x400` through `rsin`/`rcos`.
void func_neo_ark_woodland_path_8017FDE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                ang;
    s32                ang2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                        = *scratch;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw19Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
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
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1` selects a 56-texel UV tile in
/// a 4-wide 2-row grid: u = `(arg1 & 3) * 56`, v = `((arg1 & 7) >> 2) * 56`,
/// then the quad is biased to v+0x70..v-0x59. `arg2` is a signed half-extent;
/// the on-screen radius is `(s16)arg2 * 55 / otz`. The quad is axis-aligned
/// and 2*radius on a side, shifted up so the projected point sits at
/// three-quarters height (`y0 = sy - r - r/2`, `y2 = sy + r/2`).
void func_neo_ark_woodland_path_801801D0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw23Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                vbase;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head                                        = *scratch;
    ((RoomDraw23Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw23Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    tex                                         = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw23Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw23Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw23Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        SOFT_BARRIER();
        cell  = (u16)tex;
        tex   = (cell & 3) * 0x38;
        vbase = ((cell & 7) >> 2) * 0x38;
        v0    = vbase + 0x70;
        SOFT_USE_REG(v0);
        u1       = tex + 0x37;
        v1       = vbase - 0x59;
        prim->v2 = v1;
        prim->v3 = v1;
        TOUCH_REG(u1);
        sarg          = (s16)arg2;
        prim->v0      = v0;
        prim->v1      = v0;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u1      = u1;
        prim->u2      = tex;
        prim->u3      = u1;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        v1            = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        xy            = v1;
        ds            = &gDisplayState;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + (block->radius >> 1);
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

void func_neo_ark_woodland_path_8018046C(Task* task, s32 arg1, s32 arg2)
{
    s16 i;
    s16 v;

    if (arg2 > 0) {
        for (i = 0; i < 5; i++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[i] == 0) {
                v                                           = arg2 * 0x6E / 100;
                ((s16*)D_neo_ark_woodland_path_80184A60)[i] = v;
                if (D_neo_ark_woodland_path_8018494C[0] < v) {
                    ((s16*)D_neo_ark_woodland_path_80184A60)[i] = D_neo_ark_woodland_path_8018494C[0];
                }
                if (Gp_StateF0.field_6 >= 2) {
                    Gp_ReleaseStateF0(task, 0xD);
                } else {
                    D_neo_ark_woodland_path_80184996 = 1;
                }
                D_neo_ark_woodland_path_8018498E += 0x5A;
                return;
            }
        }
        return;
    }
    D_neo_ark_woodland_path_8018498E += 0x5A;
}

/// Arming state, the sibling of `func_neo_ark_woodland_path_80180C6C` one step
/// earlier in the sequence: it parks its own 0x7DB handler table in the task,
/// folds the slot's spawn count into game flag 0x10C (remembering the slot in
/// 0x10D) and fills the five spawn slots with the room's ceiling - or zero.
/// Same shape as its sibling; only the flags, the slot-count array and the
/// handler table differ.
void func_neo_ark_woodland_path_80180568(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184980[gGameSession->at4.loc.place] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                   = D_neo_ark_woodland_path_80184998;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10C);
    nib                              = GameFlag_GetNibble(0x10D);
    if (gGameSession->at4.loc.place != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184980[gGameSession->at4.loc.place];
        GameFlag_SetNibble(0x10C, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10D, gGameSession->at4.loc.place);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948[2];
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

/// Per-frame state of the arming sequence `func_...80180568` sets up: counts
/// the room's countdown down, and once the reference count on `Gp_StateF0`
/// has dropped to zero folds the still-pending spawn slots back into game
/// flags 0x168 and 0x10C. When a spawn point has been requested it hands the
/// first pending slot to a waiting slot-4 task (one whose parameter is -999),
/// sends it the 0x7DB message and places it at that point.
void func_neo_ark_woodland_path_801806D8(Task* task)
{
    s16                    i;
    s16                    count;
    s32                    a;
    s32                    b;
    NeoArkWoodlandPathObj* obj;
    s16                    j;
    s16                    k;

    gameGetPtrSlot(3);
    if (D_neo_ark_woodland_path_80184980[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_woodland_path_8018498E > 0) {
        D_neo_ark_woodland_path_8018498E--;
    }
    if (D_neo_ark_woodland_path_80184996 == 1 && Gp_StateF0.field_6 >= 2) {
        D_neo_ark_woodland_path_80184996 = 0;
        Gp_ReleaseStateF0(task, 0xD);
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_woodland_path_801849F0 > 0) {
        D_neo_ark_woodland_path_8018498E = 0x96;
        a                                = GameFlag_GetNibble(0x168);
        b                                = GameFlag_GetNibble(0x10C);
        count                            = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x168, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10C, count);
        Gp_SyncAreaKeyIndex(&gGameSession->at4);
    }
    D_neo_ark_woodland_path_801849F0 = Gp_StateF0.field_6;
    if (gGameSession->field_126 == 1 && D_neo_ark_woodland_path_8018498E == 0) {
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_5      = 0;
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_8      = 0;
        Gp_StateF0.field_C      = 0;
        Gp_StateF0.field_10     = 0;
        gGameSession->field_126 = 0;
    }
    if (Gp_StateF0.field_0 != 2 && D_neo_ark_woodland_path_80184992 != 0) {
        D_neo_ark_woodland_path_80184A5C.from.loc.stage = 5;
        D_neo_ark_woodland_path_80184A5C.from.loc.area  = 0x1D;
        D_neo_ark_woodland_path_80184A5C.command        = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = ((Task*)Gp_LookupSlot4(i))->spawnArg2;
            if (obj == NULL) {
                break;
            }
            if ((s16)obj->field_40 == -999) {
                for (j = 0; j < D_neo_ark_woodland_path_80184990; j++) {
                    if (((s16*)D_neo_ark_woodland_path_80184A60)[j] > 0) {
                        obj->field_40                       = D_neo_ark_woodland_path_80184A60[j];
                        obj->field_4C                       = 0;
                        D_neo_ark_woodland_path_80184A60[j] = 0;
                        break;
                    }
                }
                if ((s16)obj->field_40 > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_woodland_path_8018498E += 0x5A;
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(i), 0x7DB, (s32)&D_neo_ark_woodland_path_80184A5C, 0);
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].x;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].z;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                   D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].rotY, 1);
                }
                break;
            }
        }
    }
    D_neo_ark_woodland_path_80184992 = 0;
}

void func_neo_ark_woodland_path_801814D4(Task* arg0);

/// State handlers of the first arming sequence's entry task
/// `func_neo_ark_woodland_path_801814E8`: arm, run, advance, then kill.
const TaskFuncTable4 D_neo_ark_woodland_path_8017D638 = {
    { func_neo_ark_woodland_path_80180568, func_neo_ark_woodland_path_801806D8,
      func_neo_ark_woodland_path_801814D4, taskKill }
};

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, RoomActorMsg* msg)
{
    s32                    result;
    u16                    cmd;
    NeoArkWoodlandPathObj* obj;

    result = 0;
    if (msg->from.key == 0xB05) {
        cmd = msg->command;
        switch (cmd) {
            case 0:
                D_neo_ark_woodland_path_8018498E = -1;
                result                           = 0;
                return result;
            case 2:
                D_neo_ark_woodland_path_80184A5C.from.loc.stage = 5;
                D_neo_ark_woodland_path_80184A5C.from.loc.area  = 0xB;
                D_neo_ark_woodland_path_80184A5C.command        = 0xC;
                result                                          = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB,
                                   (s32)&D_neo_ark_woodland_path_80184A5C, 0);
                    obj                                                                 = ((Task*)Gp_LookupSlot4(0))->spawnArg2;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[0] = 5;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        *(u16*)((u8*)obj + 0x40)            = D_neo_ark_woodland_path_80184A60[0];
                        D_neo_ark_woodland_path_80184A60[0] = 0;
                        obj->field_4C                       = 0;
                    }
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord,
                                   0x400, 1);
                    D_neo_ark_woodland_path_8018498E = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}

/// Arming state: with no spawns to arm for the session's slot it only advances;
/// otherwise it parks this room's 0x7DB handler table in the task, folds the
/// slot's spawn count into game flag 0x10A (remembering the slot in 0x10B), and
/// fills the five spawn slots with the room's ceiling - or zero.
void func_neo_ark_woodland_path_80180C6C(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184970[gGameSession->at4.loc.place] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                   = D_neo_ark_woodland_path_801849F4;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10A);
    nib                              = GameFlag_GetNibble(0x10B);
    if (gGameSession->at4.loc.place != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184970[gGameSession->at4.loc.place];
        GameFlag_SetNibble(0x10A, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10B, gGameSession->at4.loc.place);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948[2];
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

/// Per-frame state of the arming sequence `func_...80180C6C` sets up, the
/// sibling of `func_...801806D8`: counts the room's countdown down, and once the
/// reference count on `Gp_StateF0` has dropped to zero folds the still-pending
/// spawn slots back into game flags 0x167 and 0x10A. When a spawn point has
/// been requested it hands the first pending slot to a waiting slot-4 task,
/// sends it the 0x7DB message and places it at one of five fixed points.
void func_neo_ark_woodland_path_80180DDC(Task* task)
{
    s16                    i;
    s16                    count;
    s32                    a;
    s32                    b;
    NeoArkWoodlandPathObj* obj;
    s16                    j;
    s16                    k;

    gameGetPtrSlot(3);
    if (D_neo_ark_woodland_path_80184970[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_woodland_path_8018498E > 0) {
        D_neo_ark_woodland_path_8018498E--;
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_woodland_path_801849F0 > 0) {
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
        a     = GameFlag_GetNibble(0x167);
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x167, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10A, count);
        Gp_SyncAreaKeyIndex(&gGameSession->at4);
        D_neo_ark_woodland_path_8018498E = 0x96;
    }
    D_neo_ark_woodland_path_801849F0 = Gp_StateF0.field_6;
    if (gGameSession->field_126 == 1 && D_neo_ark_woodland_path_8018498E == 0) {
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_5      = 0;
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_8      = 0;
        Gp_StateF0.field_C      = 0;
        Gp_StateF0.field_10     = 0;
        gGameSession->field_126 = 0;
    }
    if (Gp_StateF0.field_0 != 2 && D_neo_ark_woodland_path_80184992 != 0) {
        D_neo_ark_woodland_path_80184A5C.from.loc.stage = 5;
        D_neo_ark_woodland_path_80184A5C.from.loc.area  = 0xB;
        D_neo_ark_woodland_path_80184A5C.command        = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = ((Task*)Gp_LookupSlot4(i))->spawnArg2;
            if (obj == NULL) {
                break;
            }
            if ((s16)obj->field_40 == -999) {
                for (j = 0; j < D_neo_ark_woodland_path_80184990; j++) {
                    if (((s16*)D_neo_ark_woodland_path_80184A60)[j] > 0) {
                        obj->field_40                       = D_neo_ark_woodland_path_80184A60[j];
                        obj->field_4C                       = 0;
                        D_neo_ark_woodland_path_80184A60[j] = 0;
                        break;
                    }
                }
                if ((s16)obj->field_40 > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_woodland_path_8018498E += 0x5A;
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(i), 0x7DB, (s32)&D_neo_ark_woodland_path_80184A5C, 0);
                    switch ((s16)(D_neo_ark_woodland_path_80184992 - 1)) {
                        case 0:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[0].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[0].z;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[0].rotY, 1);
                            break;
                        case 1:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[1].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[1].z;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[1].rotY, 1);
                            break;
                        case 2:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[2].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[2].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[2].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                        case 3:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[3].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[3].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[3].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                        case 4:
                        default:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[4].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[4].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[4].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                    }
                }
                break;
            }
        }
    }
    D_neo_ark_woodland_path_80184992 = 0;
}

void func_neo_ark_woodland_path_801815C0(Task* arg0);

/// State handlers of the second arming sequence's entry task
/// `func_neo_ark_woodland_path_801815D4`: arm, run, advance, then kill.
const TaskFuncTable4 D_neo_ark_woodland_path_8017D684 = {
    { func_neo_ark_woodland_path_80180C6C, func_neo_ark_woodland_path_80180DDC,
      func_neo_ark_woodland_path_801815C0, taskKill }
};

s32 func_neo_ark_woodland_path_80181474(void)
{
    return 0;
}

/// 0x13EF handler of the first sequence: records the message's third byte as the
/// requested spawn point, unless it repeats the previous request or the
/// room's countdown `D_neo_ark_woodland_path_8018498E` is still running, in
/// which case any pending request is cleared. Always answers 1.
s32 func_neo_ark_woodland_path_8018147C(Task* task, s32 msgId, u8* msg)
{
    s16 counter;

    if (msg[2] != D_neo_ark_woodland_path_80184994) {
        counter = D_neo_ark_woodland_path_8018498E;
        if (counter == 0) {
            D_neo_ark_woodland_path_80184992 = msg[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        D_neo_ark_woodland_path_80184992 = 0;
    }
    D_neo_ark_woodland_path_80184994 = msg[2];
    return 1;
}

void func_neo_ark_woodland_path_801814D4(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// Entry task of the first arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D638` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801814E8(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_woodland_path_8017D638;
    sp.funcs[task->state](task);
}

s32 func_neo_ark_woodland_path_8018154C(void)
{
    D_neo_ark_woodland_path_8018498E += 0x5A;
    return 1;
}

/// 0x13EF handler of the second sequence: records the message's third byte as the
/// requested spawn point, unless it repeats the previous request or the
/// room's countdown `D_neo_ark_woodland_path_8018498E` is still running, in
/// which case any pending request is cleared. Always answers 1.
s32 func_neo_ark_woodland_path_80181568(Task* task, s32 msgId, u8* msg)
{
    s16 counter;

    if (msg[2] != D_neo_ark_woodland_path_80184994) {
        counter = D_neo_ark_woodland_path_8018498E;
        if (counter == 0) {
            D_neo_ark_woodland_path_80184992 = msg[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        D_neo_ark_woodland_path_80184992 = 0;
    }
    D_neo_ark_woodland_path_80184994 = msg[2];
    return 1;
}

void func_neo_ark_woodland_path_801815C0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// Entry task of the second arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D684` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801815D4(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_woodland_path_8017D684;
    sp.funcs[task->state](task);
}
