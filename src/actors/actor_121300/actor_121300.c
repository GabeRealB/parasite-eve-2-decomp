#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actors_shared_80149e54.h"
#include "actors/actors_shared_80149ed0.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The overlay's spawn table: entries 1 and 2 are spawned by the one-line
/// spawners the scene script calls, 3 by the waypoint walker for each new
/// waypoint, 4 to 8 are the debris variants `func_actor_121300_80133064`
/// scatters around a waypoint, 9 is spawned once the session event has
/// ended, and 0xA by `func_actor_121300_80134224`.
extern TaskDesc D_actor_121300_8013D390;

/// Work block for the `actor_121300` overlay's cutscene actor.
///
/// `func_actor_121300_80133BFC` allocates it with `Mem_Malloc(0x4B0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::work`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor121300Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_121300_8013D418` and stores the
/// `gameGetPtrSlot(3)` task in `field_488`, which is the target of every
/// `Gp_DispatchMsg` the overlay sends.
///
/// The block opens with the animation prefix `actor_105100` and `actor_136100`
/// also carry: the 0x14-byte `GpAnimCtx` `func_800B3F84` is handed as its
/// `arg0`, the nineteen 0x28-byte `GpAnimSlot`s `Gp_AnimResetSlot` walks, and
/// the pose buffer at 0x30C.  The two `MATRIX`es at 0x43C / 0x45C are the
/// model's light and colour matrices, published through `TmdObject::lightMtx`
/// / `field_20`.
typedef struct Actor121300Work {
    /* 0x000 */ GpAnimCtx    anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot   slots[0x13];
    /* 0x30C */ byte         field_30C[0x130]; // pose buffer, `func_800B3F84` arg3
    /* 0x43C */ MATRIX       field_43C;        // light matrix, into TmdObject::lightMtx
    /* 0x45C */ MATRIX       field_45C;        // colour matrix, into TmdObject::colorMtx
    /* 0x47C */ ActorWaveCtx wave;             // ramp of the screen-wave task `func_actor_121300_80131EB0`
    /* 0x488 */ Task*        field_488;        // gameGetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x48C */ Task*        field_48C;
    /* 0x490 */ byte         pad_490[0x8];
    /* 0x498 */ s16          field_498; // set by func_actor_121300_80134250
    /* 0x49A */ s16          field_49A; // cleared alongside field_498
    /* 0x49C */ s16          field_49C;
    /* 0x49E */ s16          field_49E; // waypoint cursor: index into D_actor_121300_8013CC20
    /* 0x4A0 */ u16          field_4A0; // animation slot count, set by func_actor_121300_80133BFC
    /* 0x4A2 */ u16          field_4A2; // state of the waypoint walker func_actor_121300_80133730
    /* 0x4A4 */ u16          field_4A4; // frames spent on the current waypoint
    /* 0x4A6 */ s16          field_4A6; // waypoint index handed to func_8017F334 / Task_SpawnFromTable
    /* 0x4A8 */ s16          field_4A8; // effect-count reduction, bumped by func_actor_121300_80133580
    /* 0x4AA */ s16          field_4AA; // frame counter for field_4A8 (wraps at 20)
    /* 0x4AC */ s16          field_4AC; // GpAreaPlace::tpage, the TmdObject texture page
    /* 0x4AE */ byte         pad_4AE[0x2];
} Actor121300Work;
STATIC_ASSERT_SIZEOF(Actor121300Work, 0x4B0);

/// Animation-id table `func_actor_121300_80132818` indexes by
/// `Actor121300Work::field_4A0`, whose `>= 0` guard is what gates the slot
/// re-arm; the entry it holds is then written back over `field_4A0`.  All four
/// of its entries are -1, so the re-arm never runs in practice.
extern s16 D_actor_121300_8013CC18[];

/// One record of the cutscene's waypoint table `D_actor_121300_8013CC20`: a
/// position plus a fourth halfword `func_actor_121300_80133730` reads as a
/// liveness flag.  The table is 0xD records long and its last record is
/// `{0, 0, 0, -1}`, so the `!= -1` guard keeps the walker on the 0xC real
/// entries; `func_actor_121300_8013293C` reads the x/y/z of entry
/// `someWork->field_34` off the same table.
typedef struct Actor121300Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 field_6;
} Actor121300Waypoint;
STATIC_ASSERT_SIZEOF(Actor121300Waypoint, 0x8);

extern Actor121300Waypoint D_actor_121300_8013CC20[];

/// Scratch `func_actor_121300_80133D98` stages the three states that build a
/// payload in.  Their live ranges do not overlap -- the state-0 message 0x3E8
/// record is dead once the state advances, and state 3 kills the task without
/// reaching the tail -- so the three share one stack slot and the frame stays
/// 0x38 bytes.
typedef union Actor121300Scratch {
    /* 0x0 */ GpAnimArg msg;  // state 0: slot-3 weapon record, message 0x3E8
    /* 0x0 */ RECT      rect; // state 3: the area ClearImage blanks
    /* 0x0 */ VECTOR    vec;  // tail: model part-1 translation for func_800D7A9C
} Actor121300Scratch;

/// 8-byte fade block `func_actor_121300_8013400C` and
/// `func_actor_121300_801326EC` each allocate with `Mem_Malloc(8, 0)` and park
/// in `Task::work` -- a second, smaller work block in this overlay, distinct
/// from `Actor121300Work`.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws.  The
/// task seeded by `func_actor_121300_8013400C` seeds all three to 0 and raises
/// them by `Task::spawnArg1` every frame, then once the red channel has reached
/// 0x100 it blanks the display and kills itself; the blue channel is advanced
/// but never read back.  The task seeded by `func_actor_121300_801326EC` is the
/// mirror image: it seeds all three to 0xFF, unblanks the display one state
/// before it starts drawing, and lowers them by `Task::spawnArg1` per frame
/// until the red channel goes negative, reading all three channels back.
typedef struct Actor121300FadeWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b;
} Actor121300FadeWork;
STATIC_ASSERT_SIZEOF(Actor121300FadeWork, 0x8);

/// Frame counter `func_actor_121300_80133D98` bumps once a frame and the
/// effect spawners gate on: `func_actor_121300_8013343C` only runs on every
/// fourth frame (`& 3`), `func_actor_121300_80133580` too.
extern s32 D_actor_121300_8013CC00;

/// The gameplay LCG the effect spawners draw their jitter from,
/// `state = state * 5 + 0x71357911`; the draws are logical shifts of the high
/// half (`srl`), which a signed declaration would turn into arithmetic ones.
extern u32 Gp_LcgState;

/// The two position tables `func_actor_121300_8013343C` walks, each an array
/// of `SVECTOR`s ending on a zeroed one -- the walker's guard is `vx != 0`, so
/// the sentinel is read with the position.  Both trace the same ring around
/// the arena (`vx` 2500..6500 at `vz` 4700, then back at 1500) and differ only
/// in height: `8013CCB8` sits at ground level, `8013CD48` at `vy` -0xC8.
extern SVECTOR D_actor_121300_8013CCB8[];
extern SVECTOR D_actor_121300_8013CD48[];
/// Spawn points of `func_actor_121300_80133580`, of which the first
/// `6 - Actor121300Work::field_4A8` are used.
extern SVECTOR D_actor_121300_8013CDC8[];

/// 0x5C work block of the debris task `func_actor_121300_8013293C`, allocated
/// into `Task::work`.  The two matrices are published as the model's light
/// and colour matrices (`TmdObject::lightMtx` / `field_20`); the rest is a
/// per-frame spin and velocity, all rolled from `Gp_LcgState` on spawn, and a
/// short random delay before the model's buffers are allocated.
typedef struct Actor121300DebrisWork {
    /* 0x00 */ MATRIX lightMtx; // TmdObject::lightMtx
    /* 0x20 */ MATRIX colorMtx; // TmdObject::colorMtx
    /* 0x40 */ s16    rotX;
    /* 0x42 */ s16    rotY;
    /* 0x44 */ s16    rotZ;
    /* 0x46 */ s16    pad_46;
    /* 0x48 */ s16    spinX;
    /* 0x4A */ s16    spinY;
    /* 0x4C */ s16    spinZ;
    /* 0x4E */ s16    pad_4E;
    /* 0x50 */ s16    velX;
    /* 0x52 */ s16    velY;
    /* 0x54 */ s16    velZ;
    /* 0x56 */ s16    pad_56;
    /* 0x58 */ s16    delay;
    /* 0x5A */ s16    pad_5A;
} Actor121300DebrisWork;
STATIC_ASSERT_SIZEOF(Actor121300DebrisWork, 0x5C);

extern s16 D_800691CA;
/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 message 0x3E8 record is keyed on,
/// `D_8007218A` picks which of the two weapon-id bases that record uses, and
/// `D_80071075` / `D_80114C12` (the cutscene mode flag) gate the actor's setup.
extern u8  D_80073BA9;
extern u8  D_80071075;
extern s8  D_8007218A;
extern s8  D_80114C12;
extern s16 D_80071076;

extern void func_8017F334(s32 arg0);
extern void func_8017F340(u8 arg0, u8 arg1);
extern void func_8017F438(s32 arg0);

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running ramp, recomputed every frame.
extern s32 D_actor_121300_8013BBE4;

/// The ramp the running wave task was spawned with, parked at spawn so the
/// tick reads it back every frame.
extern ActorWaveCtx* D_actor_121300_8013D414;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ActorWaveRec6 D_actor_121300_8013D420[11];
extern ActorWaveRec6 D_actor_121300_8013D470[30];

extern TaskDesc D_actor_121300_8013BBCC;
extern u32      D_actor_121300_8013BBE8[];
extern u32      D_actor_121300_8013BFD0[];
extern u32      D_actor_121300_8013C3B8[];
extern u32      D_actor_121300_8013C7A0[];
extern u32      D_actor_121300_8013C9D0[];
extern s16      D_actor_121300_8013CC04;
extern s32      D_actor_121300_8013CC08;
extern s32      D_actor_121300_8013CC88;
extern s32      D_actor_121300_8013CCA0;
extern s32      D_actor_121300_8013CE08;
extern s32      D_actor_121300_8013D2E8;
extern Task*    D_actor_121300_8013D418;
extern u16      D_actor_121300_8013D41C;

/// Screen-wave task spawned from `D_actor_121300_8013BBCC` with the cutscene
/// actor's `Actor121300Work::wave` ramp as its argument. State 0 seeds the
/// column and row phases, parks the ramp and clears its frame and ramp state;
/// state 1 ramps the frame up to the span (ramp state 0) or back down to zero
/// (ramp state 1, then 2, which kills the task and restores the display
/// field), and redraws the frame buffer as a 10 by 30 mesh of textured quads
/// displaced by sine waves of that amplitude, tinted when the ramp's tint flag
/// is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `D_800691CA` store, which a member read lets GCC hoist above it.
void func_actor_121300_80131EB0(Task* arg0)
{
    ActorWaveCtx* ctx;
    POLY_FT4*     p;
    DR_STP*       stp;
    s32           i, j, k;
    s32           drawY;
    s32           tpage0, tpage1;
    s32           u0, u1, v0, v1;
    s32           waveX0, waveY0, waveX1, waveY1;
    s32           waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_121300_8013D420[i].phase  = 0;
                D_actor_121300_8013D420[i].offset = (u32)rand() >> 3;
                D_actor_121300_8013D420[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_121300_8013D470[i].phase  = 0;
                D_actor_121300_8013D470[i].offset = (u32)rand() >> 3;
                D_actor_121300_8013D470[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_121300_8013BBE4          = 0;
            D_actor_121300_8013D414          = arg0->spawnArg2;
            D_actor_121300_8013D414->field_6 = 0;
            D_actor_121300_8013D414->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_actor_121300_8013D414;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        ctx->field_6--;
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_121300_8013BBE4 = D_actor_121300_8013D414->field_6 * D_actor_121300_8013D414->field_2 / D_actor_121300_8013D414->field_0;
            for (i = 0; i < 11; i++) {
                D_actor_121300_8013D420[i].phase += D_actor_121300_8013D420[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_121300_8013D470[i].phase += D_actor_121300_8013D470[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_121300_8013D414->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_121300_8013D414->field_9;
                        p->g0 = D_actor_121300_8013D414->field_A;
                        p->b0 = D_actor_121300_8013D414->field_B;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_actor_121300_8013BBE4 * (rsin((j << 9) + D_actor_121300_8013D420[k].phase + D_actor_121300_8013D420[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_121300_8013BBE4 * (rsin((k << 10) + D_actor_121300_8013D470[j].phase + D_actor_121300_8013D470[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_121300_8013BBE4 * (rsin((j << 9) + D_actor_121300_8013D420[k + 1].phase + D_actor_121300_8013D420[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_121300_8013BBE4 * (rsin(((k + 1) << 10) + D_actor_121300_8013D470[j].phase + D_actor_121300_8013D470[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_actor_121300_8013BBE4 * (rsin(((j + 1) << 9) + D_actor_121300_8013D420[k].phase + D_actor_121300_8013D420[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_121300_8013BBE4 * (rsin((k << 10) + D_actor_121300_8013D470[j + 1].phase + D_actor_121300_8013D470[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_121300_8013BBE4 * (rsin(((j + 1) << 9) + D_actor_121300_8013D420[k + 1].phase + D_actor_121300_8013D420[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_121300_8013BBE4 * (rsin(((k + 1) << 10) + D_actor_121300_8013D470[j + 1].phase + D_actor_121300_8013D470[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
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
}

void func_actor_121300_801326EC(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade    = alloc;
            fade->r = 0xFF;
            fade->g = 0xFF;
            fade->b = 0xFF;
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            goto state_inc;
        case 2:
            SetDispMask(1);
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
        state_inc:
            arg0->state += 1;
            break;
        case 3:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if ((s16)fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Slot re-arm of the cutscene actor: ticks all nineteen animation slots, and
/// once every one of slots 1..18 has `field_10` bit 0x100 set ("finished"),
/// hands them the animation id `D_actor_121300_8013CC18` holds for the current
/// `field_4A0`, blending it in over ten frames.  A negative table entry leaves
/// the slots alone and only the return value follows.  The gotos reproduce
/// retail's block layout.
s32 func_actor_121300_80132818(Task* arg0)
{
    Actor121300Work* work;
    Actor121300Work* ctx;
    u16              i;
    u16              done;
    u16              anim;

    work = (Actor121300Work*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x13; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_121300_8013CC18[work->field_4A0] >= 0) {
            anim           = D_actor_121300_8013CC18[work->field_4A0];
            ctx            = (Actor121300Work*)arg0->work;
            ctx->field_4A0 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&ctx->anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

void func_actor_121300_8013293C(Task* arg0)
{
    Actor121300DebrisWork* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    VECTOR                 pos;
    Actor121300DebrisWork* alloc;
    s16                    r;
    TmdObject*             tail;

    work  = (Actor121300DebrisWork*)arg0->work;
    obj   = arg0->extra;
    coord = obj->coords;
    if (D_actor_121300_8013D41C == 0) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300DebrisWork*)Mem_Malloc(0x5C, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work = alloc;
            Mem_Set(work, 0, 0x5C);
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = D_actor_121300_8013CC20[arg0->spawnArg1].x;
            coord->coord.t[1] = D_actor_121300_8013CC20[arg0->spawnArg1].y;
            coord->coord.t[2] = D_actor_121300_8013CC20[arg0->spawnArg1].z;
            switch ((u32)arg0->spawnArg2) {
                case 0:
                case 14:
                    break;
                case 1:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] += 50;
                    break;
                case 2:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] += 50;
                    break;
                case 3:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 4:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 5:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] += 80;
                    break;
                case 6:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] += 80;
                    break;
                case 7:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 8:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 10:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] += 120;
                    break;
                case 11:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] += 120;
                    break;
                case 12:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] -= 120;
                    break;
                case 13:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] -= 120;
                    break;
            }
            obj->lightMtx = &work->lightMtx;
            obj->colorMtx = &work->colorMtx;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = ((Gp_LcgState >> 16) + 10) & 7;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -10 - ((Gp_LcgState >> 16) & 7);
            }
            work->velX  = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->velY  = ((Gp_LcgState >> 16) & 3) + 3;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = ((Gp_LcgState >> 16) + 10) & 7;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -10 - ((Gp_LcgState >> 16) & 7);
            }
            work->velZ = r;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinX = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinY = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinZ = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->delay = (Gp_LcgState >> 16) & 3;
            arg0->state++;
            break;
        case 1:
            if (work->delay == 0) {
                Tmd_AllocBuffers(obj);
                obj->flags = 0;
                arg0->state++;
            } else {
                work->delay--;
            }
            break;
        case 2:
            work->velY        += D_actor_121300_8013CC04 * 3 / 100;
            coord->coord.t[0] += work->velX * D_actor_121300_8013CC04 / 100;
            coord->coord.t[1] += work->velY * D_actor_121300_8013CC04 / 100;
            coord->coord.t[2] += work->velZ * D_actor_121300_8013CC04 / 100;
            work->rotX        += work->spinX * D_actor_121300_8013CC04 / 100;
            work->rotY        += work->spinY * D_actor_121300_8013CC04 / 100;
            work->rotZ        += work->spinZ * D_actor_121300_8013CC04 / 100;
            Gfx_RotMatrixY(&coord->coord, work->rotY, 1);
            Gfx_RotMatrixX(&coord->coord, work->rotX, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rotZ, 0);
            coord->flg = 0;
            if (coord->coord.t[1] >= -499) {
                taskKill(arg0);
            }
            break;
    }
    tail   = arg0->extra;
    pos.vx = tail->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tail, &pos, 0, 3);
}

/// Spawns the fifteen debris variants for one waypoint, then releases this task.
void func_actor_121300_80133064(Task* task)
{
    Task* dispatch = task;
    void* alloc;

    u32 active = D_actor_121300_8013D41C;

    /* Keep the disabled path in a0. The ra dependency orders the entry
     * copy without fencing the s0 save out of the branch delay slot. */
    __asm__("" : "+r"(dispatch) : : "$31");
    if (active != 0) {
        switch (dispatch->state) {
            case 0:
                alloc          = Mem_Malloc(8, 0);
                dispatch->work = alloc;
                if (alloc != NULL) {
                    Mem_Set(alloc, 0, 8);
                    dispatch->state += 1;
                    return;
                }
                break;
            case 1:
                Task_SpawnFromTable(&D_actor_121300_8013D390, 4, dispatch->spawnArg1, 0);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 5, dispatch->spawnArg1, 1);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 6, dispatch->spawnArg1, 2);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 7, dispatch->spawnArg1, 3);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 8, dispatch->spawnArg1, 4);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 4, dispatch->spawnArg1, 5);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 5, dispatch->spawnArg1, 6);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 6, dispatch->spawnArg1, 7);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 7, dispatch->spawnArg1, 8);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 8, dispatch->spawnArg1, 9);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 4, dispatch->spawnArg1, 0xA);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 5, dispatch->spawnArg1, 0xB);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 6, dispatch->spawnArg1, 0xC);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 7, dispatch->spawnArg1, 0xD);
                Task_SpawnFromTable(&D_actor_121300_8013D390, 8, dispatch->spawnArg1, 0xE);
                break;
            default:
                return;
        }
        taskKill(dispatch);
    } else {
        taskKill(task);
    }
}

/// Texture loader: uploads CLUT/texel blocks into the texture page
/// `Actor121300Work::field_4AC` of the parent task named by `spawnArg2`,
/// picking the images by `spawnArg1`; the two-state variants upload one
/// block per frame before killing the task.
void func_actor_121300_8013322C(Task* arg0)
{
    RECT rect;
    s32  page;

    switch (arg0->spawnArg1) {
        case 0:
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x180;
            rect.x = page;
            rect.y = 0x140;
            rect.w = 0x19;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013BBE8);
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x18C;
            rect.x = page;
            rect.y = 0x1A0;
            rect.w = 0xE;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013C7A0);
            taskKill(arg0);
            break;
        case 1:
            switch (arg0->state) {
                case 0:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    arg0->state++;
                    break;
                case 1:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013C3B8);
                    taskKill(arg0);
                    break;
            }
            break;
        case 2:
            switch (arg0->state) {
                case 0:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    arg0->state++;
                    break;
                case 1:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BBE8);
                    taskKill(arg0);
                    break;
            }
            break;
        case 3:
            break;
        case 4:
        case 5:
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x18C;
            rect.x = page;
            rect.y = 0x1A0;
            rect.w = 0xE;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013C9D0);
            taskKill(arg0);
            break;
    }
}

/// Effect spawner: on every fourth frame, walks one of the two arena-ring
/// position tables `D_actor_121300_8013CCB8` / `D_actor_121300_8013CD48`
/// (`arg1` non-zero picks the lowered one) and spawns effect 0x601B7 at each
/// entry, jittered along `vx` by up to +/-70 -- two LCG draws, the second only
/// when the first one's bit 16 is set, which is also the sign of the step.
/// The walk stops on the zeroed `SVECTOR` that ends both tables.
void func_actor_121300_8013343C(Task* arg0, s16 arg1)
{
    SVECTOR  pos;
    SVECTOR* pts;
    s16      x;
    s32      flags;
    u32      seed;
    s32      vx;

    if (!(D_actor_121300_8013CC00 & 3)) {
        if (arg1 == 0) {
            pts = D_actor_121300_8013CCB8;
        } else {
            pts = D_actor_121300_8013CD48;
        }
        x = pts->vx;
        if (pts->vx != 0) {
            flags = 0x81202400;
            do {
                seed        = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = seed;
                vx          = x + (((seed >> 16) & 1) ? ((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7
                                                      : -(((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7)) *
                             10;
                pos.vx = vx;
                pos.vy = pts->vy;
                pos.vz = pts->vz;
                Gp_SpawnEff(0x601B7, NULL, flags, &pos);
                pts++;
                x = pts->vx;
            } while (pts->vx != 0);
        }
    }
}

/// Effect spawner: bumps the `Actor121300Work::field_4A8` falloff every 20
/// calls with `arg1` set, then on every fourth frame spawns effect 0x601B7 at
/// the first `6 - field_4A8` entries of `D_actor_121300_8013CDC8`, jittered
/// along `vx` by up to +/-70 as in `func_actor_121300_8013343C`.
void func_actor_121300_80133580(Task* arg0, s16 arg1)
{
    SVECTOR          pos;
    Actor121300Work* work;
    s16              i;
    u32              seed;
    s32              flags;
    SVECTOR*         tbl;
    s32              vx;

    work = (Actor121300Work*)arg0->work;
    if (arg1 != 0) {
        if (++work->field_4AA >= 20) {
            work->field_4AA = 0;
            work->field_4A8++;
        }
    }
    if (!(D_actor_121300_8013CC00 & 3)) {
        for (i = 0; i < 6 - work->field_4A8; i++) {
            flags       = 0x81202400;
            tbl         = D_actor_121300_8013CDC8;
            seed        = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState = seed;
            vx          = tbl[i].vx + (((seed >> 16) & 1) ? ((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7
                                                          : -(((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7)) *
                                 10;
            pos.vx = vx;
            pos.vy = tbl[i].vy;
            pos.vz = tbl[i].vz;
            Gp_SpawnEff(0x601B7, NULL, flags, &pos);
        }
    }
}

/// Waypoint walker: while the current `Actor121300Work::field_49E` waypoint of
/// `D_actor_121300_8013CC20` is live, counts three frames on it, then retunes
/// the view through `func_8017F340`, bumps the value `func_8017F334` passes on
/// and spawns the `D_actor_121300_8013D390[3]` child seeded with the new
/// waypoint index.
void func_actor_121300_80133730(Task* arg0)
{
    Actor121300Work* work = (Actor121300Work*)arg0->work;

    switch (work->field_4A2) {
        case 0:
            D_actor_121300_8013D41C = 1;
            work->field_4A4         = 0;
            work->field_4A6         = 0;
            work->field_4A2        += 1;
            break;
        case 1:
            if (D_actor_121300_8013CC20[work->field_49E].field_6 != -1) {
                if ((s16)++work->field_4A4 >= 3) {
                    if (work->field_4A6 < 6) {
                        func_8017F340((u8)work->field_4A6, 1);
                    } else if (work->field_4A6 >= 7) {
                        func_8017F340((u8)(work->field_4A6 - 1), 1);
                    }
                    func_8017F334(work->field_4A6 + 1);
                    Task_SpawnFromTable(&D_actor_121300_8013D390, 3, work->field_4A6, 0);
                    work->field_4A4 = 0;
                    work->field_4A6 = (s16)((u16)work->field_4A6 + 1);
                }
            }
            break;
    }
}

static inline void func_actor_121300_PlayAll(Task* arg0, s32 anim)
{
    Actor121300Work* work;
    u16              i;

    work            = (Actor121300Work*)arg0->work;
    work->field_4A0 = anim;
    SCHED_BARRIER();
    for (i = 1; i < 0x13; i++) {
        func_800B4114(&work->anim, i, anim, 0, 10);
    }
}

static inline void func_actor_121300_SetCC04(s32 v)
{
    D_actor_121300_8013CC04 = v;
}

void func_actor_121300_80133854(Task* arg0)
{
    Actor121300Work* work;
    CdCmdQueue*      queue;

    work  = (Actor121300Work*)arg0->work;
    queue = &CdCmd_Queue;
    func_actor_121300_80132818(arg0);
    switch ((u16)work->field_498) {
        case 1:
            Gp_DispatchMsg(work->field_488, 0x3F3, 2, 0);
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_121300_8013CCA0, 0);
            gGameSession->viewDirty = 1;
            {
                Actor121300Work* slotsWork;
                s32              i;

                slotsWork            = (Actor121300Work*)arg0->work;
                slotsWork->field_4A0 = 1;
                for (i = 1; (u16)i < 0x13U; i++) {
                    slotsWork->slots[(u16)i].rate = 0x10;
                    Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
                }
            }
            work->field_498 = 0;
            break;
        case 2:
            func_actor_121300_PlayAll(arg0, 2);
            work->field_498 = 0;
            break;
        case 4:
            if ((u16)work->field_49A == 0) {
                func_actor_121300_SetCC04(10);
                work->wave.field_0 = 0x3C;
                work->wave.field_2 = 0x100;
                work->field_48C    = Task_SpawnFromTable(&D_actor_121300_8013BBCC, 0, 0, (s32)&work->wave);
                work->field_49A++;
            }
        case 3:
            func_actor_121300_80133730(arg0);
            break;
        case 8:
            func_actor_121300_SetCC04(0x1E);
            func_actor_121300_80133730(arg0);
            break;
        case 5:
            work->wave.field_4      = 2;
            queue->field_22A        = 0;
            D_actor_121300_8013D41C = 0;
            work->field_498         = 0;
            break;
        case 6:
            if ((u16)work->field_49A == 0) {
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_121300_8013CCA0, 0);
                {
                    Actor121300Work* slotsWork;
                    s32              i;

                    slotsWork            = (Actor121300Work*)arg0->work;
                    slotsWork->field_4A0 = 1;
                    for (i = 1; (u16)i < 0x13U; i++) {
                        slotsWork->slots[(u16)i].rate = 0x10;
                        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
                    }
                }
                func_8017F438(1);
            }
            func_actor_121300_8013343C(arg0, 0);
            func_actor_121300_80133580(arg0, 0);
            break;
        case 7:
            if ((u16)work->field_49A == 0) {
                func_actor_121300_PlayAll(arg0, 3);
                work->field_49A++;
            }
            func_actor_121300_8013343C(arg0, 0);
            break;
        case 9:
            func_actor_121300_80133580(arg0, 1);
            func_actor_121300_8013343C(arg0, 0);
            break;
        case 10:
            switch ((u16)work->field_49A) {
                case 0:
                    work->wave.field_0 = 8;
                    work->wave.field_2 = 0x100;
                    work->field_48C    = Task_SpawnFromTable(&D_actor_121300_8013BBCC, 0, 0, (s32)&work->wave);
                    work->field_49C    = 0;
                    work->field_49A++;
                    break;
                case 1:
                    if (++work->field_49C >= 8) {
                        work->wave.field_4 = 1;
                        work->wave.field_0 = 8;
                        work->field_498    = 0;
                    }
                    break;
            }
            break;
        case 11:
            func_actor_121300_8013343C(arg0, 1);
            break;
        case 12:
            queue->field_22A = 2;
        case 0:
        default:
            work->field_498 = 0;
            break;
    }
}

/// First tick of the cutscene actor: allocates the 0x4B0-byte
/// `Actor121300Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- the work block's light and colour matrices into
/// `TmdObject::lightMtx` / `field_20`, `field_C` cleared and the animation
/// context handed to `func_800B3F84`, and slots 1..18 re-armed through
/// `Gp_AnimResetSlot`.  The texture page / CLUT row come from the placement
/// record at the nested area table's `field_0` list whose id matches neither
/// 0xFF (end) nor 0x84 (the skip marker).
///
/// The slot loop reaches the work block through `Task::work` again rather than
/// through the pointer the setup above uses: the compiler cannot prove
/// `Gp_AnimResetSlot` leaves the task alone, so it reloads, and the reload must
/// stay a separate local for the reload to land in `$s0` as retail does.
void func_actor_121300_80133BFC(Task* arg0)
{
    Actor121300Work* work;
    Actor121300Work* slotsWork;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;
    u8               id;

    tmd        = arg0->extra;
    coord      = tmd->coords;
    map        = Mem_Malloc(0x4B0, 0);
    arg0->work = map;
    if (map == NULL) {
        taskKill(arg0);
        return;
    }
    work = (Actor121300Work*)map;
    Mem_Set(work, 0, 0x4B0);
    work->field_488         = gameGetPtrSlot(3);
    D_actor_121300_8013D418 = arg0;
    coord->sub              = &gGfxViewCoord;
    tmd->lightMtx           = &work->field_43C;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->field_45C;
    place                   = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    id                      = place->entryId;
    while (id != 0xFF) {
        if (id == 0x84) {
            break;
        }
        place++;
        id = place->entryId;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    work->field_4AC = (s16)(s8)place->tpage;
    func_800B3F84(&work->anim, &D_actor_121300_8013CC08, tmd, work->field_30C,
                  work->slots);
    slotsWork            = (Actor121300Work*)arg0->work;
    slotsWork->field_4A0 = 1;
    i                    = 1;
    do {
        slotsWork->slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x13U);
    arg0->msgTable = &D_actor_121300_8013CC88;
}

/// State machine of the cutscene actor, run once per frame from its slot.
/// State 0 waits until no other cutscene is up -- a `D_80114C12` of 1 or a live
/// `D_80071075` means one is -- and then builds the work block through
/// `func_actor_121300_80133BFC` and arms the player's weapon: the slot-3
/// message 0x3E8 record is `D_80073BA9` plus 1 in the alternate weapon block
/// and plus 0x22 in the base one, with `field_4` 1 and the rest of the frame
/// zero.  State 1 hands the cutscene's two script blocks to `func_800E8634`,
/// state 2 spawns the `D_actor_121300_8013D390[9]` child while the session is
/// still down, and state 3 blanks the display, marks save slot 9 / the state
/// and re-arms the first tick before killing the task.
///
/// States 0, 1 and 2 all leave through the same `Task::state` increment; the
/// compiler cross-jumps the three copies, so it appears once, after state 2's
/// body.  Every path but state 3 also steps the actor through
/// `func_actor_121300_80133854` and hands the model's part-1 translation to
/// `func_800D7A9C`.
void func_actor_121300_80133D98(Task* arg0)
{
    Actor121300Scratch scratch;
    TmdObject*         extra;
    s32                state;
    s32                weaponId;
    s32                anim;

    state = arg0->state;
    switch (state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                weaponId                    = D_80073BA9;
                anim                        = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                scratch.msg.animBlock.index = anim;
                scratch.msg.field_4         = 1;
                scratch.msg.field_8         = 0;
                scratch.msg.field_C         = 0;
                scratch.msg.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&scratch.msg, 0);
                func_actor_121300_80133BFC(arg0);
                arg0->state += 1;
                break;
            }
            return;
        case 1:
            func_800E8634((s32)&D_actor_121300_8013CE08, 0, (s32)&D_actor_121300_8013D2E8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_SpawnFromTable(&D_actor_121300_8013D390, 9, 0, 0);
                arg0->state += 1;
            }
            break;
        case 3:
            scratch.rect.x = 0;
            scratch.rect.y = 0;
            scratch.rect.w = 0x140;
            scratch.rect.h = 0xF0;
            ClearImage(&scratch.rect, 0, 0, 0);
            scratch.rect.y = 0x110;
            ClearImage(&scratch.rect, 0, 0, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            Mc_SaveData.at4.loc.stage = state;
            Mc_SaveData.at4.loc.area  = 9;
            Mc_SaveData.at4.loc.warp  = state;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
    }
    func_actor_121300_80133854(arg0);
    extra          = (TmdObject*)arg0->extra;
    scratch.vec.vx = extra->coords[1].workm.t[0];
    scratch.vec.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    scratch.vec.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(extra, &scratch.vec, 0, 3);
    D_actor_121300_8013CC00 += 1;
}

void func_actor_121300_8013400C(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1);
            if ((s16)fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            taskKill(arg0);
            break;
    }
}

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}

/// Scene-script handler that places the task's model: `placement`'s position
/// becomes the translation of the `TmdObject`'s first coordinate, its angles
/// are applied Y, then X, then Z, and the coordinate is marked dirty.
void func_actor_121300_8013411C(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Scene-script handler that sets the draw bits of the task's `TmdObject`:
/// mode 0 hides the model (0x80) and clears 0x4, mode 1 shows it and clears
/// 0x4, mode 2 hides it and sets 0x4.
void func_actor_121300_801341A8(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)arg0->extra;
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

void func_actor_121300_80134224(s32 arg0, s32 arg1, s32 arg2)
{
    Task_SpawnFromTable(&D_actor_121300_8013D390, 0xA, arg2, arg0);
}

void func_actor_121300_80134250(s16 arg0)
{
    Actor121300Work* work = (Actor121300Work*)D_actor_121300_8013D418->work;

    work->field_498 = arg0;
    work->field_49A = 0;
}

void func_actor_121300_80134270(void)
{
    CdCmd_Queue.field_22A = 0;
}

void func_actor_121300_8013427C(void)
{
    Actor121300Work* work = (Actor121300Work*)D_actor_121300_8013D418->work;

    D_actor_121300_8013D41C = 0;
    work->wave.field_4      = 2;
    CdCmd_Queue.field_22A   = 0;
    Gp_DispatchMsg(work->field_488, 0x3F3, 1, 0);
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns entry 1 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_801342D4(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_121300_8013D390, 1, arg0, 0);
}

/// Spawns entry 2 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_80134304(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_121300_8013D390, 2, arg0, 0);
}

void func_actor_121300_80134334(s32 arg0)
{
    Gp_DispatchMsg(D_actor_121300_8013D418, 0x7D5, arg0, 0);
}

void func_actor_121300_80134364(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_actor_121300_80134384(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_121300_801343A4(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
