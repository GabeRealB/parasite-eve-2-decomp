#include "pe/apobiosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x28-byte scratch block `func_apobiosis_80130630` takes from
/// `G_SCRATCH_HEAD` to draw one burst shard. `v0` is the effect coordinate's
/// world position and `v1` that position plus the offset vector `arg1`;
/// both are projected through `GsWSMATRIX` with one `RTPS` each,
/// giving `sx0`/`sy0` and `sx1`/`sy1`. `flag` is the `gte_stflg` of whichever
/// projection ran last (a negative value drops the quad) and `otz` is the
/// first projection's `gte_stszotz`, incremented by 1 before it becomes both
/// the radius divisor and the OT bucket. `dx` / `dy` hold the current
/// `(arg3 * 23 / otz) * rsin|rcos(angle) >> 12` half-extents; only their low
/// halves are read back.
typedef struct ApobiosisShardScratch {
    /* 0x00 */ SVECTOR v0;
    /* 0x08 */ SVECTOR v1;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ s32     dx;
    /* 0x1C */ s32     dy;
    /* 0x20 */ s16     sx0;
    /* 0x22 */ s16     sy0;
    /* 0x24 */ s16     sx1;
    /* 0x26 */ s16     sy1;
} ApobiosisShardScratch;
STATIC_ASSERT_SIZEOF(ApobiosisShardScratch, 0x28);

/// One 8-byte row of `D_apobiosis_80130B5C`, indexed by the effect's
/// `GpEffWork.index` / `step` (`Gp_StateC08.field_0 % 10 - 1`, so the
/// burst scales with the combo counter). `field_0` is half the number of ring
/// points the cast lays out, `field_2` the ring radius it draws them at and
/// `field_4` the per-frame growth added to the cast's `GpEffWork.scale`.
/// `field_6` is the shard radius `func_apobiosis_8012FE10` hands to
/// `func_apobiosis_8013017C` / `func_apobiosis_80130630` - doubled while the
/// shard is still parented to the cast (state 1), plain once it flies free
/// (state 2).
typedef struct ApobiosisStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ u16 field_6;
} ApobiosisStep;
STATIC_ASSERT_SIZEOF(ApobiosisStep, 0x8);

static void func_apobiosis_8012F808(s16 bright);
static void func_apobiosis_8012F9D0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Per-level tuning for the apobiosis pulse, one row per PE level 1-3,
/// weakest first.
static ApobiosisStep D_apobiosis_80130B5C[] = {
    { 0x0004, 0x0400, 0x00C0, 0x0280 },
    { 0x0006, 0x0500, 0x0100, 0x0300 },
    { 0x0008, 0x0600, 0x0140, 0x0400 },
};

/// The `SndEvt_EnqueueType6` id the cast plays, one per `D_apobiosis_80130B5C`
/// row, so the sound follows the cast's level like the burst does.
static s32 D_apobiosis_80130B74[] = { 0xE0170001, 0xE01A0001, 0xE01D0001 };

static void func_apobiosis_8013017C(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_apobiosis_80130630(GpCoord* arg0, SVECTOR* arg1, s16 arg2, s16 arg3);

/// Ring azimuths, two rows of up to eight. `func_apobiosis_8012EF4C` lays out
/// `ApobiosisStep::field_0 * 2` of them at `(i << 10) + rand()` in state 0 and
/// then jitters each by +-0x80 a frame; the first row is the shard's own angle
/// and the row `ApobiosisStep::field_4` entries later is its elevation.
static s16 D_apobiosis_80130B80[16] = { 0 };

/// The running cast task, cached by `func_apobiosis_8012EF4C` so each shard
/// can reparent itself onto the cast when it starts.
static Task* D_apobiosis_80130BA0 = NULL;

/// The apobiosis cast. Six states drive one screen flash plus a growing ring
/// of shards, scaled by `D_apobiosis_80130B5C[Gp_StateC08.field_0 % 10 - 1]`
/// so a longer combo casts a wider burst. State 0 parents the effect
/// coordinate on `GpEffWork.parent` at the origin, publishes the task in
/// `D_apobiosis_80130BA0` so every shard can reparent onto it, plays the row's
/// `SndEvt_EnqueueType6` id panned at the coordinate, and seeds
/// `D_apobiosis_80130B80` with `field_0 * 2` angles - the ring's two rows of
/// azimuths. State 1 flashes at `step`, drags the coordinate down 0x400,
/// grows `scale` by the row's `field_4` each frame and redraws both the
/// player's ring and the shard ring, jittering every angle by +-0x80 per frame.
/// States 2..4 fade the flash out at 0x10 / 0xC / 8 a frame while spawning
/// 0x600F7 sparks on random polar offsets - one in four frames in state 2, one
/// a frame in state 3, two a frame in state 4 - and state 5 fades the last of
/// the flash before releasing the work block. `Gp_SpawnPadLerp` rumbles at each
/// state change, hardest on the widest row.
void func_apobiosis_8012EF4C(Task* arg0)
{
    GpEffWork*  mem;
    GpCoord*    coord;
    GpMtxWords* rot;
    s32         i;
    s32         n;
    s32         pan;
    u8          rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if ((Gp_StateC08.field_3 != -2) && (Gp_State1C->fadeState < 4)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                D_apobiosis_80130BA0 = arg0;
                rot                  = (GpMtxWords*)&coord->coord;
                coord->sub           = mem->parent;
                rot->m00_m01         = 0x1000;
                rot->m02_m10         = 0;
                rot->m11_m12         = 0x1000;
                rot->m20_m21         = 0;
                rot->m22             = 0x1000;
                coord->coord.t[0]    = 0;
                coord->coord.t[1]    = 0;
                coord->coord.t[2]    = 0;
                coord->flg           = 0;
                Gp_UpdateCoord(coord);
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(D_apobiosis_80130B74[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                    (s8)gpGetObjDepth(coord));
                arg0->state = 1;
                mem->index  = Gp_StateC08.field_0 % 10 - 1;
                mem->scale  = 0x200;
                mem->period = 0x80;
                mem->step   = 0xF0;
                for (i = 0; i < D_apobiosis_80130B5C[mem->index].field_0 * 2; i++) {
                    Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[i] = (i << 10) + (((u32)Gp_LcgState >> 16) & 0x3FF);
                }
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                /* fallthrough */
            case 1:
                Gp_UpdateCoord(coord);
                if (mem->age == 4) {
                    Gp_StateC08.field_6 |= 8;
                }
                func_apobiosis_8012F808(mem->step);
                rgb[0] = rgb[1]    = mem->step >> 2;
                rgb[2]             = mem->step >> 1;
                coord->workm.t[1] -= 0x400;
                mem->scale         = mem->scale + D_apobiosis_80130B5C[mem->index].field_4;
                func_apobiosis_8013017C(
                    &(gameGetPtrSlot(3))->extra.tmd->coords[1], mem->age,
                    D_apobiosis_80130B5C[mem->index].field_2, 0);
                func_apobiosis_8012F9D0(coord, mem->scale, 0x80, rgb);
                if (mem->age & 1) {
                    func_apobiosis_8012F9D0(coord, 0x80, mem->scale, rgb);
                }
                for (i = 0; i < D_apobiosis_80130B5C[mem->index].field_0; i++) {
                    Gp_LcgState              = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[i] -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
                    n                        = i + D_apobiosis_80130B5C[mem->index].field_4;
                    Gp_LcgState              = Gp_LcgState * 5 + 0x71357911;
                    D_apobiosis_80130B80[n] -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
                    mem->pos.vx              = mem->scale * rsin(D_apobiosis_80130B80[i]) >> 12;
                    mem->pos.vy              = mem->scale * rcos(D_apobiosis_80130B80[i]) >> 12;
                    mem->pos.vz =
                        mem->pos.vx *
                            rcos(D_apobiosis_80130B80
                                     [i + D_apobiosis_80130B5C[mem->index].field_4]) >>
                        12;
                    func_apobiosis_80130630(coord, &mem->pos, mem->age,
                                            D_apobiosis_80130B5C[mem->index].field_6);
                }
                coord->workm.t[1] += 0x400;
                if (mem->step >= 0x19) {
                    mem->step = mem->step - 0x18;
                    return;
                }
                arg0->state = 2;
                Gp_SpawnPadLerp(0x14, 0xFF, 8);
                return;
            case 2:
                Gp_UpdateCoord(coord);
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 0x41) {
                    mem->step = mem->step - 0x10;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->scale   = ((u32)Gp_LcgState >> 16) & 0x3FF;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    mem->move.vx = mem->scale * rsin(mem->angle) >> 12;
                    mem->move.vz = mem->scale * rcos(mem->angle) >> 12;
                    Gp_SpawnEff(0x600F7, coord, 0, &mem->move);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    mem->step   = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x60;
                }
                if (mem->age == 0x14) {
                    mem->step   = 0xF0;
                    arg0->state = 3;
                }
                return;
            case 3:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 0x21) {
                    mem->step = mem->step - 0xC;
                }
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = ((u32)Gp_LcgState >> 16) & 0x7FF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->move.vx = mem->scale * rsin(mem->angle) >> 12;
                mem->move.vz = mem->scale * rcos(mem->angle) >> 12;
                Gp_SpawnEff(0x600F7, coord, 0, &mem->move);
                if (mem->age == 0x1E) {
                    if (mem->index <= 0) {
                        arg0->state = 5;
                        Gp_SpawnPadLerp(mem->index * 8 + 0x12, 0xFF, 8);
                    } else {
                        mem->step   = 0xF0;
                        arg0->state = 4;
                        Gp_SpawnPadLerp(0x22, 0xFF, 8);
                    }
                }
                return;
            case 4:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 9) {
                    mem->step = mem->step - 8;
                }
                for (i = 0; i < 2; i++) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                    mem->move.vx = mem->scale * rsin(mem->angle) >> 12;
                    mem->move.vz = mem->scale * rcos(mem->angle) >> 12;
                    Gp_SpawnEff(0x600F7, coord, 0, &mem->move);
                }
                if (mem->age == 0x28) {
                    arg0->state = 5;
                }
                return;
            case 5:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 9) {
                    mem->step = mem->step - 8;
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Flashes a screen-filling `POLY_F4` over the whole 320x240 frame, offset by
/// `gDisplayState.vramYOffset` so it tracks the active draw buffer. `bright`
/// is the flash level: normally the quad is blue-tinted (red and green
/// halved), but on stage `Gp_StateC08.field_0 % 10 == 3` one draw in four
/// comes out yellow instead (blue halved). The prim is linked at a fixed
/// `otz` of 0x30, in front of the scene.
static void func_apobiosis_8012F808(s16 bright)
{
    POLY_F4* prim;

    prim           = (POLY_F4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyF4(prim);
    if ((u16)(Gp_StateC08.field_0 % 10U) - 1 == 2 && (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) == 0) {
        setRGB0(prim, bright, bright, bright >> 1);
    } else {
        setRGB0(prim, bright >> 1, bright >> 1, bright);
    }
    setXY4(prim, -0xA0, -0x78 - gDisplayState.vramYOffset, 0xA0,
           -0x78 - gDisplayState.vramYOffset, -0xA0, 0x78 - gDisplayState.vramYOffset,
           0xA0, 0x78 - gDisplayState.vramYOffset);
    addPrim(Gpu_OtEntryAtByteOffset((((u32)(0x30 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x30);
}

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when the GTE
/// flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that form a
/// ring around it. The projected depth is pulled 0x40 towards the eye (never
/// nearer than 0x10) and both on-screen radii divide by it: `inner` is
/// `(s16)arg1 * 64 / otz` and `outer` is `(s16)(arg1 + arg2) * 64 / otz`, so
/// the ring is an annulus `arg2` wide. `rgb` tints the outer rim of every
/// wedge while its inner rim stays black. Each wedge is linked into the OT
/// bucket its own depth names and then handed to `Gp_AddTpageShift`. Same
/// shape as `func_plasma_8012FB10`, which grows its ring from `otz + 1`
/// instead.
static void func_apobiosis_8012F9D0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz -= 0x40;
        if (block->otz < 0x10) {
            block->otz = 0x10;
        }
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// One shard of the apobiosis burst. Every frame it ticks the shard's life
/// counter `GpEffWork.age` and bails out - handing the work block back -
/// once the player is dying (`Gp_StateC08.field_3`), the room is fading (`Gp_State1C`)
/// or the shard has outlived its state. State 0 reparents the shard onto the
/// cast task and splits on `spawnArg1`: a non-zero arg pins the shard to the
/// cast's coordinate at the origin (state 1), a zero arg gives it a random
/// drift `move` and lets it fly (state 2). Either way the tail
/// seeds the shard's `pos` offset, its radius `angle` and
/// the intensity `step` that picks a `D_apobiosis_80130B5C` row. Both live
/// states redraw the shard every other frame, at twice the row's radius while
/// pinned and at the plain radius once free.
void func_apobiosis_8012FE10(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if ((Gp_StateC08.field_3 != -2) && (Gp_State1C->fadeState < 4)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_apobiosis_80130BA0, arg0);
                if (arg0->spawnArg1.value != 0) {
                    coord->sub        = mem->parent;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->flg        = 0;
                    Gp_UpdateCoord(coord);
                    arg0->state = 1;
                } else {
                    mem->move.vy = 0;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    arg0->state  = 2;
                }
                mem->pos.vy = -0x1000;
                mem->scale  = 0x80;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vx = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vz = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->angle  = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->step   = Gp_StateC08.field_0 % 10 - 1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_apobiosis_8013017C(coord, mem->index,
                                            D_apobiosis_80130B5C[mem->step].field_6 * 2,
                                            mem->angle);
                    func_apobiosis_80130630(coord, &mem->pos, mem->index,
                                            D_apobiosis_80130B5C[mem->step].field_6 * 2);
                }
                if (mem->age < 0x19) {
                    return;
                }
                break;
            case 2:
                coord->coord.t[0] += mem->move.vx;
                coord->coord.t[1] += mem->move.vy;
                coord->coord.t[2] += mem->move.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_apobiosis_8013017C(coord, mem->index,
                                            D_apobiosis_80130B5C[mem->step].field_6,
                                            mem->angle);
                    func_apobiosis_80130630(coord, &mem->pos, mem->index,
                                            D_apobiosis_80130B5C[mem->step].field_6);
                }
                if (mem->age < 0x11) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// One textured shard of the apobiosis burst. Projects `arg0`'s world
/// position through `GsWSMATRIX` with a single `RTPS` and, when the flag comes
/// back non-negative, queues one semi-transparent `POLY_FT4` at the projected
/// point. `arg1 % 6` picks one of the six 0x28-wide frames on tpage 0x2A - the
/// caller passes the shard's life counter, so the sprite animates - and `arg2`
/// sizes it: the corners sit `arg2 * 0x27 / otz` from the centre along `arg3`
/// and `arg3 + 0x400`, so the shard shrinks with depth and spins with `arg3`.
/// The CLUT is 0x4293 except on the widest combo row
/// (`Gp_StateC08.field_0 % 10 - 1 == 2`), where one draw in four rolls the
/// brighter 0x42C9 palette. Same shape as Combustion's and Pyrokinesis's flame
/// quad (`func_combustion_8012FB14`), which uses a fixed CLUT and 0x20-wide
/// frames.
static void func_apobiosis_8013017C(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s16              frame;
    s32              u0;
    s32              u1;
    s32              ang2;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        if ((u16)(Gp_StateC08.field_0 % 10) - 1 == 2) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                prim->clut = 0x42C9;
            } else {
                prim->clut = 0x4293;
            }
        } else {
            prim->clut = 0x4293;
        }
        frame = arg1 % 6;
        u0    = frame * 0x28;
        u1    = u0 + 0x27;
        setUV4(prim, u0, 0x38, u1, 0x38, u0, 0x5F, u1, 0x5F);
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws one apobiosis burst shard as a semi-transparent raw-tex `POLY_FT4`
/// (tpage 0x28). The effect coordinate's world position and that position plus
/// `arg1` are each projected through `GsWSMATRIX` with one `RTPS`; the quad is
/// laid along the line joining the two projected points, `ratan2` of their
/// screen delta giving the spin applied at that angle and at `+ 0x400`. `arg2`
/// selects the 128-texel UV tile: u = `(arg2 & 1) * 128`, v =
/// `((arg2 & 3) >> 1) * 24 - 0x30`. `arg3` is a signed half-extent, so the
/// on-screen half-width is `arg3 * 23 / otz`. Clut is 0x4287, or 0x42C8 on
/// one in four LCG rolls when the combo row is 2. Nothing is drawn if either
/// projection sets a negative `gte_stflg`.
static void func_apobiosis_80130630(GpCoord* arg0, SVECTOR* arg1, s16 arg2, s16 arg3)
{
    ApobiosisShardScratch* block;
    POLY_FT4*              prim;
    s32                    u0;
    s32                    u1;
    s32                    va;
    s32                    vb;
    s16                    ang;

    block        = SCRATCH_PUSH(ApobiosisShardScratch);
    block->v1.vx = block->v0.vx = arg0->workm.t[0];
    block->v1.vy = block->v0.vy = arg0->workm.t[1];
    block->v1.vz = block->v0.vz = arg0->workm.t[2];
    block->v1.vx               += arg1->vx;
    block->v1.vy               += arg1->vy;
    block->v1.vz               += arg1->vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->v0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        gte_ldv0(&block->v1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            if ((u16)(Gp_StateC08.field_0 % 10U) - 1 == 2 &&
                (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) == 0) {
                prim->clut = 0x42C8;
            } else {
                prim->clut = 0x4287;
            }
            u0 = (arg2 & 1) << 7;
            u1 = u0 + 0x7F;
            va = ((arg2 & 3) >> 1) * 24 - 0x30;
            vb = ((arg2 & 3) >> 1) * 24 - 0x19;
            setUV4(prim, u0, va, u1, va, u0, vb, u1, vb);
            ang       = ratan2(block->sy1 - block->sy0, block->sx1 - block->sx0);
            block->dx = (((arg3 * 0x17) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg3 * 0x17) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx0 + block->dx;
            prim->x3  = block->sx1 - block->dx;
            prim->y0  = block->sy0 - block->dy;
            prim->y3  = block->sy1 + block->dy;
            block->dx = (((arg3 * 0x17) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy = (((arg3 * 0x17) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1  = block->sx1 + block->dx;
            prim->x2  = block->sx0 - block->dx;
            prim->y1  = block->sy1 - block->dy;
            prim->y2  = block->sy0 + block->dy;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x28);
}
