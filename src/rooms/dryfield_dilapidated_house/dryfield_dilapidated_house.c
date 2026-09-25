#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
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
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block of the task family whose state-0 init is
/// `func_dryfield_dilapidated_house_80180B84`, which allocates it with
/// `Mem_Malloc(0x6C, 0)` and parks it in the `Task::work` slot (0x1C) -- that
/// slot is *not* a `TaskIdMap` here. Reach it with
/// `(DdhCoordWork*)task->work`.
///
/// `func_dryfield_dilapidated_house_80180F5C` writes the same ramp value (from
/// `func_dryfield_dilapidated_house_80180FD8`, 0..0x1000) into all three of
/// `field_0` / `field_4` / `field_8`; `func_dryfield_dilapidated_house_80181028`
/// rebuilds `mtx` as the identity and then composes it against the parent's
/// `GsCOORDINATE2` chain, and `func_dryfield_dilapidated_house_80180B84` copies
/// `mtx` verbatim into a spawned child's `GsCOORDINATE2::coord`.
typedef struct DdhCoordWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ MATRIX mtx;
    /* 0x2C */ byte   pad_2C[0x40];
} DdhCoordWork;
STATIC_ASSERT_SIZEOF(DdhCoordWork, 0x6C);

/// Vertex-morph record for the model at `D_dryfield_dilapidated_house_8018669C`.
/// Setup snapshots the model's vertices into `field_8` (`field_10` of them) and,
/// when `field_4` is set, its normals into `field_C` (`field_12` of them). The
/// morph restores that snapshot into the model from part index `field_14` and
/// blends `field_16` vertices toward `field_0` and the normals toward
/// `field_4`, which is null when the model has no normal pass.
typedef struct DdhRoomRec {
    /* 0x00 */ SVECTOR* field_0;
    /* 0x04 */ SVECTOR* field_4;
    /* 0x08 */ SVECTOR* field_8;
    /* 0x0C */ SVECTOR* field_C;
    /* 0x10 */ s16      field_10;
    /* 0x12 */ s16      field_12;
    /* 0x14 */ s16      field_14;
    /* 0x16 */ s16      field_16;
} DdhRoomRec;
STATIC_ASSERT_SIZEOF(DdhRoomRec, 0x18);

/// Work block of the handler table at `D_dryfield_dilapidated_house_8017D61C`,
/// whose state 0 is `func_dryfield_dilapidated_house_8018118C`: allocated with
/// `Mem_Malloc(0x24, 0)` and parked in the `Task::work` slot. It holds a
/// snapshot of the placed model coordinate's matrix (`mtx`, copied from
/// `GpCoordExt::coord`) plus one 0x1000 word.
typedef struct DdhModelWork {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ s32    field_20;
} DdhModelWork;
STATIC_ASSERT_SIZEOF(DdhModelWork, 0x24);

/// Work block of the state family at `D_dryfield_dilapidated_house_8017D634`,
/// whose state 0 is `func_dryfield_dilapidated_house_801814B4`: allocated with
/// `Mem_Malloc(0x40, 0)` and parked in the `Task::work` slot. One angle step per
/// model part, each the matching entry of `D_dryfield_dilapidated_house_80186804`
/// scaled by the task's spawn arg 1 and wrapped into the 0x4000 angle period.
/// `func_dryfield_dilapidated_house_80180738` advances the same table against a
/// running per-part angle.
typedef struct DdhAngleStep {
    /* 0x00 */ s32 step[16];
} DdhAngleStep;
STATIC_ASSERT_SIZEOF(DdhAngleStep, 0x40);

/// Work block of the three effect handlers `func_dryfield_dilapidated_house_80182744`,
/// `func_dryfield_dilapidated_house_80183C8C` and
/// `func_dryfield_dilapidated_house_80183D5C`, reached as `task->spawnArg2`
/// and handed to `Gp_ReleaseState1CMem` when their ramp runs out. `field_24` is a
/// scale and `field_26` an angle in the 0x100-step rotation space: the pair starts
/// at 0x80 / 0x100, steps by -8 and +0x80 per frame and drives one
/// `Gfx_RotMatrixZ` + `Gp_UpdateCoord` + draw call per frame. `field_22` is the
/// per-frame tick the task rolls back while the `Gp_State1C` fade is armed;
/// `field_20` and `field_28` are a third ramp value the two `80182744` states
/// seed from one `Gp_LcgState` draw and hand to the same draw routine.
typedef struct DdhEffWork {
    /* 0x00 */ byte pad_00[0x20];
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ s16  field_24;
    /* 0x26 */ s16  field_26;
    /* 0x28 */ s16  field_28;
} DdhEffWork;
STATIC_ASSERT_SIZEOF(DdhEffWork, 0x2A);

/// One `gte_rtps` result kept on the stack: the screen position, and the slot
/// the depth-cue value is written to. Only the first entry's slot is ever
/// written.
typedef struct DdhScreenPoint {
    DVECTOR sxy;
    s32     depthCue;
} DdhScreenPoint;

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

extern s16 D_800691CA;
extern u8  D_80071075;
extern s16 D_80071076;
extern s16 D_8007107A;
extern s8  D_80114C12;
extern u8  D_801156F9;

/// Script pair handed to `Gp_SpawnScript18`. Both live in gameplay's image, so
/// the room overlay imports them by absolute address and passes them as `s32`.
extern s32 D_80114A24;
extern s32 D_80114A34;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_dryfield_dilapidated_house_80183E60;

/// The ramp and tint the wave task was spawned with.
extern OverlayWaveCtx* D_dryfield_dilapidated_house_80189B74;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern OverlayWaveRec6 D_dryfield_dilapidated_house_80189B84[11];
extern OverlayWaveRec6 D_dryfield_dilapidated_house_80189BD4[30];

extern RECT D_dryfield_dilapidated_house_80183E7C;
extern RECT D_dryfield_dilapidated_house_80183E84;

extern s32            D_dryfield_dilapidated_house_80189B70;
extern s32            D_dryfield_dilapidated_house_80189B6C;
extern s32            D_dryfield_dilapidated_house_80183EFC;
extern s32            D_dryfield_dilapidated_house_80184408;
extern s32            D_dryfield_dilapidated_house_80184C60;
extern TaskDesc       D_dryfield_dilapidated_house_80183EB4[];
extern s32            D_dryfield_dilapidated_house_80184EA0;
extern s32            D_dryfield_dilapidated_house_801855F0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189AA0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189B24;

/// The room's cutscene task, spawned from entry 0 of
/// `D_dryfield_dilapidated_house_80183EB4` when `Gp_LookupSlot4(1)` is non-zero
/// as the room starts, and NULL otherwise.
extern Task* D_dryfield_dilapidated_house_80189B78;

extern TaskDesc D_dryfield_dilapidated_house_80183E64;
extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854[];
extern Task*    D_dryfield_dilapidated_house_80189B7C;
/// Spawn argument the spawned task reads back; its address is also the
/// `Task_SpawnFromTable` arg, so the store and the call must stay ordered.
extern s16 D_dryfield_dilapidated_house_80189B80;
/// Declared as a one-element array rather than a bare `s16`: measured. With a
/// bare scalar, GCC 2.8.1's `fixed_scalar_and_varying_struct_p` concludes it
/// cannot alias the `Task*` store beside it in
/// `func_dryfield_dilapidated_house_8017E970` and swaps the two stores.
extern s16 D_dryfield_dilapidated_house_80189B82[1];

/// Shared in source with actor 136300: the ramp context the message handler
/// seeds and hands to the screen-wave task it starts, and that task's entry.
extern OverlayWaveCtx D_dryfield_dilapidated_house_80189C94;
extern TaskDesc       D_dryfield_dilapidated_house_80183E48;

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern s32        D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR    D_dryfield_dilapidated_house_80186844[2];
extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;
extern SVECTOR    D_dryfield_dilapidated_house_801866B4[];
extern s8         D_dryfield_dilapidated_house_801866F4[16][4];
extern u8         D_dryfield_dilapidated_house_80186734[24][4];
extern SVECTOR    D_dryfield_dilapidated_house_80186794[2];
extern SVECTOR    D_dryfield_dilapidated_house_801867A4[6];
extern SVECTOR    D_dryfield_dilapidated_house_801867D4[6];

void func_dryfield_dilapidated_house_8017E9A4(s32 arg0);
void func_dryfield_dilapidated_house_8017EBB8(Task* task);
void func_dryfield_dilapidated_house_8017EE58(Task* task);
void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2);
void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3);
void func_dryfield_dilapidated_house_80180A0C(Task* task, DdhRoomRec* rec, s32 arg2);
void func_dryfield_dilapidated_house_80180FB8(Task* task);
s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
void func_dryfield_dilapidated_house_80181028(Task* task);
void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src);
void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

void func_dryfield_dilapidated_house_8017E48C(void);
void func_dryfield_dilapidated_house_8017EAB4(Task* arg0);
void func_dryfield_dilapidated_house_8017E014(Task* task);
void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
void func_dryfield_dilapidated_house_80180B84(Task* task);
void func_dryfield_dilapidated_house_80180F5C(Task* arg0);
void func_dryfield_dilapidated_house_8018118C(Task* arg0);
void func_dryfield_dilapidated_house_80181264(Task* arg0);
void func_dryfield_dilapidated_house_80181340(Task* arg0);
void func_dryfield_dilapidated_house_801813DC(Task* task);
void func_dryfield_dilapidated_house_8018142C(Task* task);
void func_dryfield_dilapidated_house_801814B4(Task* arg0);
void func_dryfield_dilapidated_house_80181584(Task* task);
void func_dryfield_dilapidated_house_801815B8(Task* arg0);
void func_dryfield_dilapidated_house_80182A18(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
void func_dryfield_dilapidated_house_801832A8(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3);
void func_dryfield_dilapidated_house_80182F14(GsCOORDINATE2* coord, s32 arg1, s16 arg2);
void func_dryfield_dilapidated_house_80183728(GsCOORDINATE2* coord, s16 arg1, s32 arg2, s16 arg3);
void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s16 arg1);
void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts);
void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts);
void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags);

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `Display_ClampField126`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front. The state is read through a plain word
/// load at its offset, which keeps it ordered after the store to `D_800691CA`.
void func_dryfield_dilapidated_house_8017D64C(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + 0x30)) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase  = 0;
                D_dryfield_dilapidated_house_80189B84[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189B84[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase  = 0;
                D_dryfield_dilapidated_house_80189BD4[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189BD4[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_dryfield_dilapidated_house_80183E60        = 0;
            D_dryfield_dilapidated_house_80189B74        = arg0->spawnArg2;
            D_dryfield_dilapidated_house_80189B74->frame = 0;
            D_dryfield_dilapidated_house_80189B74->state = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_dryfield_dilapidated_house_80189B74;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_dryfield_dilapidated_house_80183E60 = D_dryfield_dilapidated_house_80189B74->frame * D_dryfield_dilapidated_house_80189B74->scale / D_dryfield_dilapidated_house_80189B74->span;
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase += D_dryfield_dilapidated_house_80189B84[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase += D_dryfield_dilapidated_house_80189BD4[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_dryfield_dilapidated_house_80189B74->blend == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_dryfield_dilapidated_house_80189B74->r;
                        p->g0 = D_dryfield_dilapidated_house_80189B74->g;
                        p->b0 = D_dryfield_dilapidated_house_80189B74->b;
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
                        waveX0 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
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

                        waveX2 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
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

/// The room's capture task, the body the actor family also carries as
/// `func_actor_460200_80131E24`: the whole image area is written into the
/// display buffer strip by strip and then desaturated in place.
///
/// It is spawned from entry 0 of `D_dryfield_dilapidated_house_80183E64` with
/// the `OverlayCaptureArgs` block as its `spawnArg2`. State 0 seeds the countdown
/// from the block's duration, picks the strip origin's y out of `gDisplayState`
/// (`field_1f` non-zero selects 0, clear selects 0x110) and hands the twenty
/// 0x1E00-byte strips of `Fs_ImgBuffers` to `StoreImage` -- or, while the buffer
/// is being read back (`field_112` is negative), only the single flat
/// `D_dryfield_dilapidated_house_80183E7C` rectangle -- and then marks the
/// display busy in `field_104`. State 1 waits for the transfer with `DrawSync`
/// and runs the desaturating invert. State 2 counts the duration down in
/// `Task::killCountdown`, raises the block's `done` when it runs out, and on
/// `done` releases `field_104` and kills the task.
void func_dryfield_dilapidated_house_8017DE88(Task* task)
{
    OverlayCaptureArgs* args;
    s32                 i;
    u32*                strip;

    args = task->spawnArg2;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    D_dryfield_dilapidated_house_80183E84.y = 0;
                } else {
                    D_dryfield_dilapidated_house_80183E84.y = 0x110;
                }
                if (gDisplayState.field_112 < 0) {
                    StoreImage(&D_dryfield_dilapidated_house_80183E7C, Fs_ImgBuffers->buffers[0]);
                } else {
                    strip = Fs_ImgBuffers->buffers[0];
                    for (i = 0; i < 20; i++) {
                        D_dryfield_dilapidated_house_80183E84.x = i * 16;
                        StoreImage(&D_dryfield_dilapidated_house_80183E84, strip);
                        strip += 1920;
                    }
                }
                gDisplayState.skipDraw = 1;
                goto advance;
            case 1:
                DrawSync(0);
                func_dryfield_dilapidated_house_8017E48C();
            advance:
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}

/// State handlers of the room task, indexed by `Task::state`: set-up, the room
/// gate, then `taskKill`.
const TaskFuncTable3 D_dryfield_dilapidated_house_8017D5C4 = {
    { func_dryfield_dilapidated_house_8017EAB4, func_dryfield_dilapidated_house_8017E014, taskKill },
};

/// Room gate task. While the session is in the room (`gGameSession->eventState`
/// is 0) it walks `D_dryfield_dilapidated_house_80183EFC` from 1 to 2 and then
/// to 3: the 1 -> 2 step is unconditional, the 2 -> 3 step waits for the room's
/// message (0x7D6) to be dispatched and answered with 0 by the slot-0 object,
/// and for no sound to be playing; reaching 3 spawns entry 3 of the room's task
/// table. Independently, once the stream file is open it starts the named
/// sequences `"AUNT"` and `"Player"` on the two slot objects.
void func_dryfield_dilapidated_house_8017E014(Task* task)
{
    if (gGameSession->eventState == 0) {
        if (D_dryfield_dilapidated_house_80183EFC == 1) {
            D_dryfield_dilapidated_house_80183EFC = 2;
        } else if ((D_dryfield_dilapidated_house_80183EFC == 2) &&
                   (Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0)) {
            if (D_80114C12 != 1) {
                if (D_80071075 == 0) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 3, 0, 0);
                }
            }
        }
    }
    if ((D_8007107A != 0) && (Gp_LookupSlot4(1) != 0)) {
        func_80724608(Gp_LookupSlot4(1), -0x8C, 0xA, "AUNT");
        func_80724608(gameGetPtrSlot(3), -0x8C, 0x14, "Player");
    }
}

/// Screen-blackout timer of task-table entry 2: `func_dryfield_dilapidated_house_8017E970`
/// arms it by writing state 2 and a frame count into `spawnArg1` (a 0 arg resets
/// it to state 0 instead). State 2 copies that count into the shared countdown
/// `D_dryfield_dilapidated_house_80189B70` and falls through to state 3, whose
/// `var_s1` is the shared "paint the screen black" flag; state 4 runs the
/// countdown and at 0 calls `func_dryfield_dilapidated_house_8017E9A4(0xF)`,
/// which starts the room's captured-image scene, then raises the flag again once
/// the count is 15 frames past that hand-off, keeping the screen black over it.
/// The flag paints the whole frame with a zeroed `TILE` carved out of
/// `gGpuPrimCursor` and links it into `gGpuCurrentOt`. When `D_801156F9` is set
/// the task does nothing at all.
void func_dryfield_dilapidated_house_8017E144(Task* task)
{
    TILE* tile;
    s32   var_s1;
    s32   temp_v0;
    s32   temp_v1;

    var_s1 = 0;
    if (D_801156F9 == 0) {
        temp_v1 = task->state;
        switch (temp_v1) {
            case 0:
                task->state = task->state + 1;
                break;
            case 1:
                break;
            case 2:
                D_dryfield_dilapidated_house_80189B70 = task->spawnArg1;
                task->state                           = task->state + 1;
                /* fallthrough */
            case 3:
                var_s1      = 1;
                task->state = task->state + var_s1;
                break;
            case 4:
                temp_v0                               = D_dryfield_dilapidated_house_80189B70 - 1;
                D_dryfield_dilapidated_house_80189B70 = temp_v0;
                if (temp_v0 == 0) {
                    func_dryfield_dilapidated_house_8017E9A4(0xF);
                }
                if (D_dryfield_dilapidated_house_80189B70 < -0xF) {
                    var_s1 = 1;
                }
                break;
        }
        if (var_s1 != 0) {
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            SetTile(tile);
            tile->x0 = -0xA0;
            tile->y0 = -0x80;
            tile->w  = 0x140;
            tile->h  = 0x100;
            tile->r0 = 0;
            tile->g0 = 0;
            tile->b0 = 0;
            addPrim(gGpuCurrentOt, tile);
        }
    }
}

/// Scene-clear task: the room's hand-off to the rest of the game. State 0
/// starts the streamed scene named by the two blocks `func_800E8634` takes,
/// state 1 fires when the session is back in play (`gGameSession->eventState`
/// is 2) and hands slot 0 the release event 0x1B, state 6 waits for the room
/// message (`gGameSession->field_126`), and state 7 -- reached once the save
/// has not already banked this clear (`Mc_SaveData.demoScene`) -- applies the
/// room's two area records, raises the progression flags, refills the party
/// and hands off to the results screen with `Task_Spawn(0, 0x11, 0, 0)`.
/// States 0..6 share the `advance` tail that walks the task one state on;
/// `goto advance` from state 1 is the `acropolis_patio` idiom, and the
/// `do/while (0)` around the shared increment is this project's allocation
/// lever, not a loop: it weights the task pointer's references by loop depth
/// so it outranks the `Mc_SaveData` base and takes `$s0` instead of `$s1`.
void func_dryfield_dilapidated_house_8017E2B0(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634((s32)&D_dryfield_dilapidated_house_80184EA0, 0, (s32)&D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 2) {
                Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1B);
                Gp_StateF0.field_1 = 3;
                goto advance;
            }
            return;
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            return;
        case 6:
            if (gGameSession->field_126 == 0) {
                return;
            }
        advance:
            do {
                task->state += 1;
            } while (0);
            return;
        case 7:
            if (Mc_SaveData.demoScene != 9) {
                Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189AA0);
                if (GameFlag_GetNibble(0xCE) != 0) {
                    Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189B24);
                }
                GameFlag_SetNibble(0x4B, 6);
                GameFlag_SetNibble(0x4C, 1);
                GameFlag_SetNibble(0x45, 1);
                GameFlag_SetNibble(0x62, 1);
                GameFlag_SetNibble(0x59, 1);
                GameFlag_SetNibble(0x5A, 2);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0);
                Gp_FillPlayerHpMp();
                Gp_FillAllyHp();
                Mc_SaveData.sceneEvent    = 1;
                Mc_SaveData.at4.loc.stage = 2;
                Mc_SaveData.at4.loc.warp  = 1;
                Mc_SaveData.at4.loc.room  = 1;
                Mc_SaveData.at4.loc.area  = 8;
                D_80071076                = 1;
                Task_Spawn(0, 0x11, 0, 0);
            }
            taskKill(task);
            return;
    }
}

/// Inverts the grey of the whole image buffer in place, two 16-bit texels per
/// step. Each packed pair is averaged with weights 3:4:1 over its R, G and B
/// fields, the average is complemented against the 5-bit field mask, and the
/// result is spread back over 15 bits. The 0x4B00 passes cover the buffer's
/// 38400 words exactly.
void func_dryfield_dilapidated_house_8017E48C(void)
{
    s32          i;
    u32          maskR;
    u32          maskG;
    u32          maskB;
    u32          maskAll;
    u32*         p0;
    u32*         p1;
    u32          a0;
    u32          a2;
    u32          a1;
    register u32 v0 asm("v0");
    register u32 v1 asm("v1");

    p0      = Fs_ImgBuffers->buffers[0];
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    p1      = p0 + 1;

    do {
        i += 1;
        a0 = *p1;
        a2 = *p0;

        v1 = (a0 & maskR) << 8;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 1;
        a1 = v0 + v1;

        v1 = (a0 & maskG) << 3;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 2;
        a1 = a1 + v0;

        a0 = a0 >> 2;
        v1 = a0 & maskB;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        a1 = a1 + v1;

        v0 = a1 >> 3;
        a1 = v0 & maskAll;
        a1 = maskAll - a1;

        a2 = a1 & maskR;
        v0 = a2 << 10;
        v1 = a2 << 5;
        v0 = v0 | v1;
        a2 = a2 | v0;

        a0 = a1 & maskB;
        a0 = a0 >> 8;
        v0 = a0 << 10;
        v1 = a0 << 5;
        v0 = v0 | v1;
        a0 = a0 | v0;

        *p0 = a2;
        *p1 = a0;
        p1 += 2;
        p0 += 2;
    } while (i < 0x4B00);
}

s32 func_dryfield_dilapidated_house_8017E56C(void)
{
    return 0;
}

/// Message gate for the room's second hotspot. It copies the incoming record to
/// the outgoing one and then writes the answer the caller acts on to the copy's
/// `field_3`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`gGameSession->at4.loc.stage` is 2), a
/// type-7 record with no sub-id answers 1, or the session's own value when flag
/// nibble 0x3C is set. A type-7 record in play (`Gp_StateF0.field_0` is 1) runs
/// CAP command 0x14 and a type-5 record runs 0x13, each only when the sub-id is
/// clear; everything else is left to the caller and answers 1.
s32 func_dryfield_dilapidated_house_8017E574(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->at4.loc.stage;
    if (s1 == 2) {
        if (in->msgId == 7) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = s1;
                }
            }
        }
    }
    if ((in->msgId == 7) && (Gp_StateF0.field_0 == 1)) {
        if (in->field_5 == 0) {
            Gp_SpawnIfCapIdle(0x14, 0);
        }
        return 0;
    }
    if (in->msgId == 5) {
        if (in->field_5 == 0) {
            Gp_SpawnIfCapIdle(0x13, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_dryfield_dilapidated_house_8017E684(void)
{
    return 0;
}

s32 func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_v1;

    temp_v1 = arg2->field_2;
    if ((temp_v1 == 1) && (D_dryfield_dilapidated_house_80183EFC == 0)) {
        D_dryfield_dilapidated_house_80183EFC = (s32)temp_v1;
        func_800E8634((s32)&D_dryfield_dilapidated_house_80184408, 0, (s32)&D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    s32   temp_a1;
    s32   temp_v1;

    temp_s1 = gameGetPtrSlot(3);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1 = 0;
            arg0->state    += 1;
            return;
        case 2:
            func_800B0928(temp_s1, (Task*)temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            Display_ClampField126((s8)var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_dilapidated_house_8017E858(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

/// State handlers of the task `func_dryfield_dilapidated_house_80181134` dispatches.
const TaskFuncTable3 D_dryfield_dilapidated_house_8017D61C = {
    { func_dryfield_dilapidated_house_8018118C, func_dryfield_dilapidated_house_80181264, taskKill },
};

/// State handlers of the task `func_dryfield_dilapidated_house_801812E8` dispatches.
const TaskFuncTable3 D_dryfield_dilapidated_house_8017D628 = {
    { func_dryfield_dilapidated_house_80181340, func_dryfield_dilapidated_house_801813DC,
      func_dryfield_dilapidated_house_8018142C },
};

/// State handlers of the task `func_dryfield_dilapidated_house_8018145C` dispatches.
const TaskFuncTable3 D_dryfield_dilapidated_house_8017D634 = {
    { func_dryfield_dilapidated_house_801814B4, func_dryfield_dilapidated_house_80181584,
      func_dryfield_dilapidated_house_801815B8 },
};

/// State handlers of the task `func_dryfield_dilapidated_house_80180F04`
/// dispatches.
const TaskFuncTable3 D_dryfield_dilapidated_house_8017D640 = {
    { func_dryfield_dilapidated_house_80180B84, func_dryfield_dilapidated_house_80180F5C, taskKill },
};

/// Script command that moves the cutscene task to the given state; does
/// nothing when that task was never spawned.
void func_dryfield_dilapidated_house_8017E8A8(s32 arg0)
{
    if (D_dryfield_dilapidated_house_80189B78 != NULL) {
        D_dryfield_dilapidated_house_80189B78->state = arg0;
    }
}

/// Script command that calls `Gp_PulseState1C`.
void func_dryfield_dilapidated_house_8017E8C8(void)
{
    Gp_PulseState1C();
}

/// Message handler for the start-countdown cue; actor 136300 carries the same
/// body. A positive argument is latched
/// and nothing else happens; otherwise the CD command queue is dropped into
/// Mdec_DecodeToVram mode 2 and -- except for the -2 "already ran" message --
/// the spawn block is filled and the `D_dryfield_dilapidated_house_80183E48` entry started.
///
/// Both halves of the block are written in *each* arm of the countdown test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the countdown collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting `unk2` out of the arms compiles to a different allocation.
void func_dryfield_dilapidated_house_8017E8E8(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    if (arg0 <= 0) {
        queue->field_22A = 2;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_dryfield_dilapidated_house_80189C94.span  = 0x64;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            } else {
                D_dryfield_dilapidated_house_80189C94.span  = 5;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            }
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183E48, 0, 0, (s32)&D_dryfield_dilapidated_house_80189C94);
        }
    } else {
        D_dryfield_dilapidated_house_80189C94.state = arg0;
    }
}

void func_dryfield_dilapidated_house_8017E970(s32 arg0)
{
    if (arg0 == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = 0;
        D_dryfield_dilapidated_house_80189B82[0]     = 1;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state     = 2;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1 = arg0;
}

void func_dryfield_dilapidated_house_8017E9A4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_SpawnScript18((s32)&D_80114A24, (s32)&D_80114A34);
        D_dryfield_dilapidated_house_80189B80 = arg0;
        Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183E64, 0, 0,
                            (s32)&D_dryfield_dilapidated_house_80189B80);
        return;
    }
    D_dryfield_dilapidated_house_80189B82[0] = 1;
}

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 =
            Task_SpawnFromTable(D_dryfield_dilapidated_house_80186854, 0, 3, (s32)gameGetPtrSlot(3));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        taskKill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}

/// Script command that calls `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.field_6`.
void func_dryfield_dilapidated_house_8017EA7C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->msgTable = D_dryfield_dilapidated_house_80183E8C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(1) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C94.state = 2;
    D_dryfield_dilapidated_house_80189B7C =
        Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    gGameSession->flowFlags = 0x83;
    arg0->state            += 1;
}

/// The room task: runs its current state out of
/// `D_dryfield_dilapidated_house_8017D5C4`, copied onto the stack - setup, the
/// room gate, then `taskKill`.
void func_dryfield_dilapidated_house_8017EB60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D5C4;
    sp.funcs[task->state](task);
}

/// Projects the eight local-space markers at
/// `D_dryfield_dilapidated_house_801866B4` through the parent task's
/// `DdhCoordWork` matrix and `Gfx_ViewWorldMtx`, then queues two red
/// `LINE_F2`s as an X at each screen point in `gGpuCurrentOt[10]`.
void func_dryfield_dilapidated_house_8017EBB8(Task* task)
{
    struct {
        SVECTOR vec;
        s32     sxy;
        s32     dp;
        s32     flag;
        s32     otz;
    } sc;
    MATRIX*  mtx;
    LINE_F2* line;
    u16      sx;
    s32      sy;
    s16      x0;
    s16      y0;
    s16      x1;
    s16      y1;
    s32      i;

    i   = 0;
    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2)->work)->mtx;
    do {
        sc.vec.vx = D_dryfield_dilapidated_house_801866B4[i].vx;
        sc.vec.vy = D_dryfield_dilapidated_house_801866B4[i].vy;
        sc.vec.vz = D_dryfield_dilapidated_house_801866B4[i].vz;
        gte_SetRotMatrix(mtx);
        gte_ldv0(&sc.vec);
        gte_rtv0();
        gte_stsv(&sc.vec);
        sc.vec.vx = *(u16*)&sc.vec.vx + *(u16*)&mtx->t[0];
        sc.vec.vy = *(u16*)&sc.vec.vy + *(u16*)&mtx->t[1];
        sc.vec.vz = *(u16*)&sc.vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&sc.vec);
        gte_rtps();
        gte_stsxy(&sc.sxy);
        gte_stdp(&sc.dp);
        gte_stflg(&sc.flag);
        gte_stszotz(&sc.otz);
        line           = (LINE_F2*)gGpuPrimCursor;
        sx             = sc.sxy;
        sy             = sc.sxy >> 16;
        gGpuPrimCursor = line + 1;
        x0             = sx - 5;
        y0             = sy - 5;
        x1             = sx + 5;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        y1       = sy + 5;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        i++;
        line->x0 = x1;
        line->y0 = y0;
        line->x1 = x0;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);
    } while (i < 8);
}

/// Debug view of the two cubic Bezier segments whose control points start at
/// `D_dryfield_dilapidated_house_801866B4`: samples each at 21 positions,
/// projects every point through the parent task's `DdhCoordWork` matrix and
/// `Gfx_ViewWorldMtx`, and queues a small `LINE_F2` X at it in
/// `gGpuCurrentOt[10]` - green for the first segment, blue for the second.
void func_dryfield_dilapidated_house_8017EE58(Task* task)
{
    SVECTOR  vec;
    DVECTOR  sx;
    DVECTOR  sy;
    s32      out[3];
    s32      sxy;
    s32      dp;
    s32      flag;
    s32      otz;
    MATRIX*  mtx;
    LINE_F2* line;
    s32      i;

    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2)->work)->mtx;
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4, D_dryfield_dilapidated_house_801866B4 + 3, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = *(u16*)&vec.vx + *(u16*)&mtx->t[0];
        vec.vy = *(u16*)&vec.vy + *(u16*)&mtx->t[1];
        vec.vz = *(u16*)&vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4 + 3, D_dryfield_dilapidated_house_801866B4 + 6, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = *(u16*)&vec.vx + *(u16*)&mtx->t[0];
        vec.vy = *(u16*)&vec.vy + *(u16*)&mtx->t[1];
        vec.vz = *(u16*)&vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
}

/// Evaluates a cubic Bezier segment at frame `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_dryfield_dilapidated_house_80181290(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_dryfield_dilapidated_house_80181290(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_dryfield_dilapidated_house_80181290(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad                 = D_dryfield_dilapidated_house_801866F4[0];
    scale                = ((DdhCoordWork*)((Task*)task->spawnArg2)->work)->field_4;
    task->killCountdown += 0x40;
    if (task->killCountdown >= 0x800) {
        task->killCountdown = 0;
    }
    rsin(task->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_dryfield_dilapidated_house_80186734[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_dryfield_dilapidated_house_80186734[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_dryfield_dilapidated_house_80186734[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, tpage);
        quad += 4;
    }
}

/// Lays out 24 screen-space points in `verts` as four rings of six around two
/// ends of a segment. The ends come from `D_dryfield_dilapidated_house_80186794`,
/// mirrored in x when the task's spawn arg 1 is 1, rotated by the parent task's
/// `DdhCoordWork` matrix; the far end is pulled toward the near one by the
/// parent's `field_4` ramp before both are moved by the matrix translation and
/// projected. Each ring's offsets are rotated to the segment's screen angle and
/// scaled by the projection distance over the last projected depth, which is
/// left in `*arg2` (`*arg3` gets the GTE flags). Rings 0 and 1 use the tables at
/// their natural size, rings 2 and 3 scaled by a factor that pulses with
/// `killCountdown`.
void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3)
{
    SVECTOR          a;
    SVECTOR          b;
    OverlayMat       rot;
    DdhScreenPoint   proj[2];
    DdhCoordWork*    work;
    MATRIX*          mtx;
    SVECTOR*         src;
    s16              t;
    s16              r;
    s32              scale;
    OverlayMatWords* words;
    s32              i;
    u16              f;
    u16              x0;
    s32              y0;
    u16              x1;
    s32              y1;
    s32              dx;
    s32              dy;
    s32              side;

    side = task->spawnArg1;
    work = ((Task*)task->spawnArg2)->work;
    mtx  = &work->mtx;
    f    = work->field_4;
    a.vx = D_dryfield_dilapidated_house_80186794[0].vx;
    a.vy = D_dryfield_dilapidated_house_80186794[0].vy;
    a.vz = D_dryfield_dilapidated_house_80186794[0].vz;
    src  = &D_dryfield_dilapidated_house_80186794[1];
    b.vx = src->vx;
    b.vy = src->vy;
    b.vz = src->vz;
    if (side == 1) {
        a.vx *= -1;
        b.vx *= -1;
    }
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_rtv0();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_rtv0();
    gte_stsv(&b);
    t     = (s16)f * 0.75 + 1024.0;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&a);
    gte_rtps();
    gte_stsxy(&proj[0].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    gte_ldv0(&b);
    gte_rtps();
    gte_stsxy(&proj[1].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    dy                = proj[0].sxy.vy - proj[1].sxy.vy;
    dx                = proj[1].sxy.vx - proj[0].sxy.vx;
    x1                = proj[1].sxy.vx;
    x0                = proj[0].sxy.vx;
    y0                = proj[0].sxy.vy;
    y1                = proj[1].sxy.vy;
    i                 = ratan2(dx, dy);
    scale             = gDisplayState.screenDistance;
    rot.ident.m00_m01 = 0x1000;
    rot.ident.m02_m10 = 0;
    words             = &rot.ident;
    words->m11_m12    = 0x1000;
    rot.ident.m20_m21 = 0;
    words->m22        = 0x1000;
    RotMatrixZ(i, &rot.mat);
    gte_SetRotMatrix(&rot.mat);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867A4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867A4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i].vx = b.vx + x0;
        verts[i].vy = b.vy + y0;
    }
    USE_REG(x0);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867D4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867D4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 6].vx = b.vx + x1;
        verts[i + 6].vy = b.vy + y1;
    }
    r = 4096.0 - rsin(task->killCountdown) * 0.5 + 4096.0;
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867A4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867A4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 12].vx = b.vx + x0;
        verts[i + 12].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867D4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867D4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 18].vx = b.vx + x1;
        verts[i + 18].vy = b.vy + y1;
    }
}

/// Projects the two 16-vertex rings in `verts` (inner at 0..15, outer at
/// 16..31) and joins them with 16 semi-transparent `POLY_G4`s, wrapping the
/// last quad back to vertex 0. The inner edge is a grey whose level is the
/// parent task's `DdhCoordWork::field_8` clamped to 0x400 and scaled to 0..0xFF;
/// the outer edge is black. Each quad goes into the ordering table four entries
/// past its average depth, preceded by a `DR_TPAGE` selecting blend mode 3.
void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts)
{
    s32       sxy[32];
    s32       sz[16];
    CVECTOR   c0;
    CVECTOR   c1;
    POLY_G4*  prim;
    DR_TPAGE* tp;
    s16       level;
    s32       i;
    s32*      xy;
    SVECTOR*  v;
    s32*      p;
    s32*      z;

    v     = verts;
    p     = sxy;
    z     = sz;
    level = ((DdhCoordWork*)((Task*)task->spawnArg2)->work)->field_8;
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    for (i = 0; i < 16; i++) {
        gte_ldv3(v, v + 1, v + 16);
        gte_rtpt();
        gte_stsxy3(p, p + 1, p + 16);
        gte_stszotz(z);
        p++;
        z++;
        v++;
    }
    if (level > 0x400) {
        level = 0x400;
    }
    c0.r = level * 0xFF / 0x400;
    c0.g = level * 0xFF / 0x400;
    c0.b = 0;
    c1.r = 0;
    c1.g = 0;
    c1.b = 0;
    for (i = 0; i < 16; i++) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setlen(prim, 8);
        setcode(prim, 0x3A);
        prim->r0 = c0.r;
        prim->g0 = c0.g;
        prim->b0 = c0.b;
        prim->r1 = c0.r;
        prim->g1 = c0.g;
        prim->b1 = c0.b;
        prim->r2 = c1.r;
        prim->g2 = c1.g;
        prim->b2 = c1.b;
        prim->r3 = c1.r;
        prim->g3 = c1.g;
        prim->b3 = c1.b;
        // Packed screen words, one per vertex; each `xy` word is two words past
        // the previous one because a colour word sits between them.
        xy = (s32*)&prim->x0;
        if (i < 15) {
            xy[0] = sxy[i];
            xy[2] = sxy[i + 1];
            xy[4] = sxy[i + 16];
            xy[6] = sxy[i + 17];
        } else {
            xy[0] = sxy[15];
            xy[2] = sxy[0];
            xy[4] = sxy[31];
            xy[6] = sxy[16];
        }
        addPrim((u_long*)(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) + 4, prim);
        tp             = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tp + 1);
        setlen(tp, 1);
        tp->code[0] = 0xE1000465;
        addPrim((u_long*)(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) + 4, tp);
    }
}

void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts)
{
    DdhAngleStep* work;
    DdhCoordWork* src;
    MATRIX*       mtx;
    SVECTOR*      ofs;
    SVECTOR*      ofs2;
    SVECTOR       pos[2];
    SVECTOR*      v0;
    SVECTOR*      v1;
    s16           tx;
    s16           ty;
    s16           tz;
    s32           i;
    s32           ang;
    s32           c;
    s32           s;

    v0 = verts;
    v1 = &verts[16];

    work = (DdhAngleStep*)task->work;
    src  = (DdhCoordWork*)((Task*)task->spawnArg2)->work;

    ofs       = D_dryfield_dilapidated_house_80186844;
    ofs2      = D_dryfield_dilapidated_house_80186844 + 1;
    pos[0].vx = ofs->vx;
    pos[0].vy = ofs->vy;
    pos[0].vz = ofs->vz;
    pos[1].vx = ofs2->vx;
    pos[1].vy = ofs2->vy;
    pos[1].vz = ofs2->vz;

    mtx = &src->mtx;

    tx = mtx->t[0];
    ty = mtx->t[1];
    tz = mtx->t[2];

    gte_SetRotMatrix(mtx);

    for (i = 0; i < 16; i++) {
        ang = (i << 12) >> 4;
        c   = rcos(ang);
        s   = rsin(ang);

        v0->vx = pos[0].vx + ((c * 0x96) >> 12);
        v0->vy = pos[0].vy + ((s * 0x4B) >> 12);
        v0->vz = pos[0].vz;

        gte_ldv0(v0);
        gte_rtv0();
        gte_stsv(v0);

        v0->vx += tx;
        v0->vy += ty;
        v0->vz += tz;
        v0++;

        v1->vx = pos[1].vx + ((c * 0xFA) >> 12);
        v1->vy = pos[1].vy + ((s * 0x7D) >> 12);
        v1->vz = pos[1].vz + ((rsin(work->step[i] >> 2) * 0x64) >> 12);

        work->step[i] = (work->step[i] + D_dryfield_dilapidated_house_80186804[i]) & 0x3FFF;

        gte_ldv0(v1);
        gte_rtv0();
        gte_stsv(v1);

        v1->vx += tx;
        v1->vy += ty;
        v1->vz += tz;
        v1++;
    }
}

/// Morphs the task's model by `arg2` (0..0x1000): restores `rec`'s vertex
/// snapshot into the model from part `rec->field_14`, interpolates those
/// vertices toward `rec->field_0` through the GTE, and, when `rec->field_4` is
/// set, blends each normal between `rec->field_4` and the snapshot in
/// `rec->field_C`.
void func_dryfield_dilapidated_house_80180A0C(Task* task, DdhRoomRec* rec, s32 arg2)
{
    s32        i;
    s32        count;
    s32        off;
    TmdSource* src;
    u16*       dst;
    u16*       from;
    u16*       dstMid;
    u16*       fromMid;
    SVECTOR*   nrm;
    SVECTOR*   nrmA;
    SVECTOR*   nrmB;
    SVECTOR*   nrmDst;
    s32        blend;
    s32        inv;
    u16        vx;
    u16        vz;

    i     = 0;
    count = rec->field_16;
    src   = ((TmdObject*)task->extra)->source;
    off   = rec->field_14 * 8;
    from  = (u16*)((u8*)rec->field_8 + off);
    nrm   = src->normals;
    dst   = (u16*)((u8*)src->verts + off);
    if (count > 0) {
        fromMid = from + 2;
        dstMid  = dst + 2;
        do {
            vx         = *from;
            from      += 4;
            i         += 1;
            *dst       = vx;
            dst       += 4;
            dstMid[-1] = fromMid[-1];
            vz         = fromMid[0];
            fromMid   += 4;
            dstMid[0]  = vz;
            dstMid    += 4;
        } while (i < count);
    }
    blend = arg2;
    inv   = 0x1000 - blend;
    gteMIMefunc(src->verts + rec->field_14, rec->field_0, rec->field_16, blend);
    nrmA = rec->field_4;
    if (nrmA != NULL) {
        count = rec->field_12;
        nrmB  = rec->field_C;
        i     = 0;
        if (count > 0) {
            do {
                gte_lddp(blend);
                gte_ldsv(nrmA);
                gte_gpf12();
                nrmDst = nrm + i;
                gte_lddp(inv);
                gte_ldsv(nrmB);
                gte_gpl12();
                nrmB++;
                i++;
                nrmA++;
                gte_stsv(nrmDst);
            } while (i < count);
        }
    }
}

void func_dryfield_dilapidated_house_80180B84(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parentCoord;
    DdhCoordWork*  work;
    DdhRoomRec*    rec;
    TmdSource*     source;
    SVECTOR*       dst;
    SVECTOR*       dst2;
    SVECTOR*       src2;
    SVECTOR*       verts;
    TaskDesc*      table;
    Task*          spawned;
    GsCOORDINATE2* childCoord;
    u16            flags;
    s32            i;

    parent      = (Task*)task->spawnArg2;
    obj         = (TmdObject*)task->extra;
    parentObj   = (TmdObject*)parent->extra;
    coord       = obj->coords;
    parentCoord = parentObj->coords;
    work        = (DdhCoordWork*)Mem_Malloc(0x6C, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_0 = 0;
    flags         = obj->flags | 0x80;
    obj->flags    = flags;
    if (!(parentObj->flags & 0x80)) {
        obj->flags = flags & 0xFF7F;
    }
    obj->otOffset = 4;
    obj->flags   |= 2;
    parentCoord  += task->spawnArg1;
    coord->flg    = 0;
    coord->sub    = parentCoord;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);

    rec    = &D_dryfield_dilapidated_house_8018669C;
    source = ((TmdObject*)task->extra)->source;
    dst    = rec->field_8;
    dst2   = rec->field_C;
    verts  = source->verts;
    for (i = 0; i < rec->field_10; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (rec->field_4 != 0) {
        src2 = source->normals;
        for (i = 0; i < rec->field_12; i++) {
            dst2[i].vx = src2[i].vx;
            dst2[i].vy = src2[i].vy;
            dst2[i].vz = src2[i].vz;
        }
    }

    func_dryfield_dilapidated_house_80180FD8(task);

    table   = D_dryfield_dilapidated_house_80186854;
    spawned = Task_SpawnFromTable(table, 3, 9, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 3, 0x11, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 0, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 1, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }

    task->exitCallback = func_dryfield_dilapidated_house_80180FB8;
    task->state       += 1;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D640`,
/// copied onto the stack: `func_dryfield_dilapidated_house_80180B84`,
/// `func_dryfield_dilapidated_house_80180F5C`, then `taskKill`.
void func_dryfield_dilapidated_house_80180F04(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D640;
    sp.funcs[task->state](task);
}

void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    DdhCoordWork* work;
    s32           temp_v0;

    work = (DdhCoordWork*)arg0->work;
    func_dryfield_dilapidated_house_801810F8((TmdObject*)arg0->extra,
                                             (TmdObject*)((Task*)arg0->spawnArg2)->extra);
    func_dryfield_dilapidated_house_80181028(arg0);
    temp_v0       = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->field_0 = temp_v0;
    work->field_8 = temp_v0;
    work->field_4 = temp_v0;
}

/// Exit callback `func_dryfield_dilapidated_house_80180B84` installs on its
/// task: it kills the task.
void func_dryfield_dilapidated_house_80180FB8(Task* task)
{
    taskKill(task);
}

/// Steps the task's 0..0x1000 ramp by 0x44, saturating at 0x1000, and feeds the
/// distance still to run (`0x1000 - ramp`) to the room record's matrix/vertex
/// interpolator. Returns the ramp value, which the caller stores into its
/// `DdhCoordWork`.
s32 func_dryfield_dilapidated_house_80180FD8(Task* task)
{
    s32 ramp;

    ramp = task->killCountdown + 0x44;
    if (ramp >= 0x1001) {
        ramp = 0x1000;
    }
    task->killCountdown = ramp;
    func_dryfield_dilapidated_house_80180A0C(task, &D_dryfield_dilapidated_house_8018669C, 0x1000 - ramp);
    return ramp;
}

/// Rebuilds the work block's `mtx` as the identity, then composes it against
/// the parent model's `GsCOORDINATE2` chain: each node's `coord` rotation is
/// multiplied in, and its translation is rotated by the accumulated matrix and
/// added to `mtx.t`. Steps one coordinate record at a time from the head of the
/// parent's array up to the record this task's own `coord` links with `sub`.
void func_dryfield_dilapidated_house_80181028(Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    DdhCoordWork*  work;
    GsCOORDINATE2* node;
    MATRIX*        mtx;

    coord                = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    work                 = (DdhCoordWork*)task->work;
    node                 = (GsCOORDINATE2*)((TmdObject*)((Task*)task->spawnArg2)->extra)->coords;
    mtx                  = &work->mtx;
    *(s32*)&work->mtx    = ONE;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = ONE;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = ONE;
    mtx->t[0]            = 0;
    mtx->t[1]            = 0;
    mtx->t[2]            = 0;
    do {
        ApplyMatrixLV(mtx, (VECTOR*)node->coord.t, &vec);
        mtx->t[0] += vec.vx;
        mtx->t[1] += vec.vy;
        mtx->t[2] += vec.vz;
        MulMatrix0(mtx, &node->coord, mtx);
    } while (node++ != coord->sub);
}

void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src)
{
    if (!(src->flags & 0x80)) {
        dst->flags &= ~0x80;
        return;
    }
    dst->flags |= 0x80;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D61C`,
/// copied onto the stack: `func_dryfield_dilapidated_house_8018118C`,
/// `func_dryfield_dilapidated_house_80181264`, then `taskKill`.
void func_dryfield_dilapidated_house_80181134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D61C;
    sp.funcs[task->state](task);
}

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D61C`:
/// snapshots the placed model coordinate's matrix into a fresh `DdhModelWork`,
/// seeds its 0x1000 word, marks the model's `TmdObject` hidden (bit 0x80 of
/// `field_C`), re-parents the task that spawned this one under it and advances
/// to state 1.
void func_dryfield_dilapidated_house_8018118C(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    DdhModelWork*  work;

    obj   = (TmdObject*)arg0->extra;
    coord = obj->coords;
    work  = (DdhModelWork*)Mem_Malloc(0x24, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work     = (TaskIdMap*)work;
    work->field_20 = 0x1000;
    work->mtx      = coord->coord;
    obj->flags    |= 0x80;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D628`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_801812E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D628;
    sp.funcs[task->state](task);
}

void func_dryfield_dilapidated_house_80181340(Task* arg0)
{
    GsCOORDINATE2* coord;
    void*          work;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work  = Mem_Malloc(4, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;
    coord->sub = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->exitCallback = func_dryfield_dilapidated_house_8018142C;
    arg0->state       += 1;
}

void func_dryfield_dilapidated_house_801813DC(Task* task)
{
    SVECTOR verts[24];
    s32     sp0;
    s32     sp1;

    func_dryfield_dilapidated_house_8017FAD4(task, verts, &sp0, &sp1);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
}

void func_dryfield_dilapidated_house_8018142C(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord->sub = &gGfxViewCoord;
    taskKill(arg0);
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D634`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_8018145C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D634;
    sp.funcs[task->state](task);
}

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D634`,
/// dispatched by `func_dryfield_dilapidated_house_8018145C`: fills a fresh
/// `DdhAngleStep` with the shared per-part angle table scaled by this task's spawn
/// arg (each wrapped into the 0x4000 angle period), links the model coordinate
/// this task works on to the parent model's coordinate array, and re-parents the
/// task that spawned this one under it.
void func_dryfield_dilapidated_house_801814B4(Task* arg0)
{
    DdhAngleStep*  work;
    GsCOORDINATE2* coord;
    s32            i;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work  = (DdhAngleStep*)Mem_Malloc(0x40, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;
    for (i = 0; i < 0x10; i++) {
        work->step[i] = (D_dryfield_dilapidated_house_80186804[i] * arg0->spawnArg1) & 0x3FFF;
    }
    coord->sub = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181584(Task* task)
{
    SVECTOR verts[32];

    func_dryfield_dilapidated_house_80180738(task, verts);
    func_dryfield_dilapidated_house_801803A4(task, verts);
}

void func_dryfield_dilapidated_house_801815B8(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord->sub = &gGfxViewCoord;
    taskKill(arg0);
}

/// Prism corners in model space, eight per prism: `[0..3]` the lit ring and
/// `[4..7]` the far ring. Callers pick a prism by passing 0, 8 or 0x10.
extern SVECTOR D_dryfield_dilapidated_house_80186884[];

/// Draws one prism from `D_dryfield_dilapidated_house_80186884[arg1..]` as five
/// gouraud `POLY_G4`: four sides joining the lit ring to the far ring, then a
/// cap over the lit ring. Each corner is rotated by `coord`'s `workm` and moved
/// by its translation before projection through `GsWSMATRIX`. The lit corners
/// share a grey that pulses with the display frame; the far corners are black.
void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    u8               shade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomQuadScratch);
    blk                     = (RoomQuadScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
        blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
        blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
        blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
        blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
        blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
        blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
        blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
        blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
    blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
    blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
    blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
    blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
    blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
    blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
    blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
    blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomQuadScratch);
}

/// Near and far trail offsets. `[0]` seeds the object's coordinate on the first
/// frame and `[1]` the second ring; `D_dryfield_dilapidated_house_8018694C` is
/// `[1]` under its own name, because the per-frame path in state 1 rebuilds
/// its address from scratch.
extern SVECTOR D_dryfield_dilapidated_house_80186944[1];
extern SVECTOR D_dryfield_dilapidated_house_8018694C;

/// Eight-slot trail coordinates, one array per end of the pair. Every entry is
/// parented to `gGfxViewCoord`.
extern GsCOORDINATE2 D_dryfield_dilapidated_house_80189DE0[8];
extern GsCOORDINATE2 D_dryfield_dilapidated_house_8018A060[8];

/// Per-frame twin trail. State 0 places the object's coordinate at
/// `D_dryfield_dilapidated_house_80186944[0]` and the second ring at `[1]`,
/// then seeds all sixteen trail slots with that pose. State 1 re-poses both
/// frames every frame, writes them into slot `field_22 & 7`, re-runs the whole
/// ring so the older slots follow their parents, and hands the ribbon to
/// `func_dryfield_dilapidated_house_801823B8`. The task frees itself once
/// `age` reaches spawn arg 1. It idles whole while `Gp_State1C->eventState`
/// is 2 or more.
void func_dryfield_dilapidated_house_80181F08(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_dryfield_dilapidated_house_80186944[0].vx;
                objCoord->coord.t[1] = D_dryfield_dilapidated_house_80186944[0].vy;
                objCoord->coord.t[2] = D_dryfield_dilapidated_house_80186944[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_dryfield_dilapidated_house_80186944[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_dryfield_dilapidated_house_8018694C.vx;
                coord.coord.t[1] = D_dryfield_dilapidated_house_8018694C.vy;
                coord.coord.t[2] = D_dryfield_dilapidated_house_8018694C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &D_dryfield_dilapidated_house_80189DE0[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_dryfield_dilapidated_house_8018A060[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_dilapidated_house_801823B8(work->age & 7, 0x210);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `slot`. Each quad spans `workm.t` of two
/// adjacent slots on `D_dryfield_dilapidated_house_80189DE0` and
/// `D_dryfield_dilapidated_house_8018A060`. Dropped when `gte_stszotz` is
/// closer than 0x11. `flags` is the beam colour, three 2-bit channels at
/// bits 8, 4 and 0 that each multiply the 0x40-9i fade.
void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags)
{
    RoomFlaggedQuadScratch* blk;
    GsCOORDINATE2*          a;
    GsCOORDINATE2*          b;
    POLY_G4*                prim;
    s32                     i;
    s32                     j;
    s32                     i0;
    s32                     i1;
    s32                     hi;
    s32                     lo;
    s32                     fade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomFlaggedQuadScratch);
    blk                     = (RoomFlaggedQuadScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_dryfield_dilapidated_house_80189DE0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i0];
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        a            = &D_dryfield_dilapidated_house_80189DE0[i1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomFlaggedQuadScratch);
}

/// Per-frame state machine of the ``DdhEffWork`` effect family's fade-in
/// handler: state 0 seeds the work block (0xC0 / 0x500 scale and angle, a
/// 12-bit `Gp_LcgState` draw as the third ramp value, a `Gp_SpawnEff` and a
/// fade quad), maps the placed model's own coordinate onto
/// `Gp_RoomCoords[0]` and spawns the ring of `0x60275` flame effects, then
/// re-parents each onto this task. State 1 steps the angle by 0x40 per frame
/// and runs two more draws against the same coordinate. While the
/// `Gp_State1C` fade is armed the frame counter is rolled back and the work
/// block is released as soon as the fade reaches 4 or the angle passes
/// 0x580.
void func_dryfield_dilapidated_house_80182744(Task* task)
{
    DdhEffWork*    work;
    GsCOORDINATE2* coord;
    GpCoord64*     rc;
    GpPointLight*  tail;
    GpEffWork*     eff;
    u16            tick;
    u16            tick1;
    s16            size;
    s32            angle;
    s32            keep;
    s32            i;
    u8             rgb[3];

    work           = task->spawnArg2;
    coord          = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    tick           = work->field_22;
    tick1          = tick + 1;
    work->field_22 = tick1;
    rc             = &Gp_RoomCoords[0];
    tail           = &rc->data.light;

    switch (task->state) {
        case 0:
            if (Gp_State1C->eventState != 0) {
                s32 fade;

                work->field_22 = tick;
                fade           = Gp_State1C->eventState;
                SOFT_USE_REG(fade);
                keep = fade < 4;
                break;
            }
            work->field_24 = 0xC0;
            work->field_26 = 0x500;
            work->field_20 = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_28 = (Gp_LcgState >> 16) & 0xFFF;
            Gp_SpawnEff(0x60274, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_RoomCoords[0].framesLeft   = 4;
            tail->inner                   = 0x200;
            tail->outer                   = 0x2000;
            Gp_LcgState                   = Gp_LcgState * 5 + 0x71357911;
            size                          = ((Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->head.r                  = size;
            tail->head.g                  = size >> 1;
            tail->head.b                  = size >> 2;
            tail->head.u.coord.coord.t[0] = coord->coord.t[0];
            tail->head.u.coord.coord.t[1] = coord->coord.t[1];
            tail->head.u.coord.coord.t[2] = coord->coord.t[2];
            rc->data.coord.flg            = 0;
            i                             = 0;
            func_dryfield_dilapidated_house_801832A8(coord, (s16)work->field_22, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            work->field_26 = 0x380;
            do {
                eff = Gp_SpawnEff(0x60275, coord, i, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            task->state = 1;
            return;
        case 1:
            if (Gp_State1C->eventState != 0) {
                work->field_22 = tick;
                keep           = Gp_State1C->eventState < 4;
                break;
            }
            func_dryfield_dilapidated_house_801832A8(coord, (s16)tick1, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            func_dryfield_dilapidated_house_80182F14(coord, (s16)((u16)work->field_26 * 2), (s16)(u16)work->field_24 >> 1);
            angle          = (u16)work->field_26;
            angle         += 0x40;
            work->field_26 = angle;
            SOFT_USE_REG(angle);
            keep = (s16)angle < 0x581;
            break;
        default:
            return;
    }
    if (!keep) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws the flame column: two 16-vertex rings of radius `arg1` and
/// `arg1 + 0x100` are built in the XY plane (`vz` 0x100 / 0) by `rsin` /
/// `rcos`, rotated by `arg0`'s `workm` and offset by its translation, then
/// each of the 16 segments is projected through `GsWSMATRIX` as one `POLY_G4`.
/// The inner edge carries the unsigned `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)`
/// and the outer edge fades to black; a negative `gte_stflg` drops the segment.
/// Same body as `func_pyrokinesis_8012FC34`.
void func_dryfield_dilapidated_house_80182A18(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp     = (u32)arg2 << 16;
    red      = arg2;
    grn      = ramp >> 17;
    blu      = ramp >> 18;
    r1       = arg1 + 0x100;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
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
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring. Same body as
/// `func_pyrokinesis_80130130`.
void func_dryfield_dilapidated_house_80182F14(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;
    u16            red;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    red           = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, *(u8*)&red, arg2 >> 1, arg2 >> 2);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            ang = ang2;
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Draws a spinning textured sprite at `arg0`'s `workm` translation, projected
/// once through `GsWSMATRIX`. The `POLY_FT4` is taken from the primitive
/// cursor before the projection flag is checked, so a dropped sprite (negative
/// `gte_stflg`) still consumes its slot. The low bit of `arg1` alternates two
/// semi-transparent looks: odd draws the 0x428B cell tinted
/// `(0xC0, 0x60, 0x40)`, even draws the 0x428C cell untinted. The corners sit
/// `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
void func_dryfield_dilapidated_house_801832A8(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**                    scratch;
    u8*                       head;
    GpFxQuadScratch*          block;
    register GpFxQuadScratch* p asm("v0");
    register u16              vx asm("v0");
    POLY_FT4*                 prim;
    s32                       ang;
    u16                       vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    vx                                        = *(u16*)&arg0->workm.t[0];
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = vx;
    p                                         = (GpFxQuadScratch*)(head - 0x1C);
    block                                     = p;
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    block->vec.vz                             = vz;
    *scratch                                  = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        if (arg1 & 1) {
            setRGB0(prim, 0xC0, 0x60, 0x40);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
            setSemiTrans(prim, 1);
        } else {
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
        }
        ang       = arg3;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang       = ang + 0x400;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    DEF_REG(head);
}

/// Draws the flame band: two 16-vertex rings of radius `arg1` and
/// `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment. Same body
/// as `func_pyrokinesis_801312B4`.
void func_dryfield_dilapidated_house_80183728(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1       = arg1 + arg2;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
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
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

void func_dryfield_dilapidated_house_80183BF8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    mask  = 1 << (u8)gGameSession->at4.loc.view;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (mask & 0x84A9C) {
        func_dryfield_dilapidated_house_801815E8(coord, 0);
    }
    if (mask & 0x104B98) {
        func_dryfield_dilapidated_house_801815E8(coord, 8);
    }
    if (mask & 0xA55F8) {
        func_dryfield_dilapidated_house_801815E8(coord, 0x10);
    }
}

/// Per-frame handler that runs the `DdhEffWork` effect block one step further:
/// an early out while `Gp_State1C` is armed. It counts frames in `field_22`,
/// seeds the 0xC0 / 0x100 scale/angle pair on the first frame, feeds the pair to
/// `func_dryfield_dilapidated_house_80182A18` and then steps the scale by -0x10
/// and the angle by +0x40. Once the scale falls below 0x10 - and immediately
/// when the state word has already reached 4 - it releases the work block.
void func_dryfield_dilapidated_house_80183C8C(Task* arg0)
{
    DdhEffWork* mem;
    s16         flag;
    s32         scale;
    s32         angle;

    mem  = arg0->spawnArg2;
    flag = Gp_State1C->eventState;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0xC0;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }
    func_dryfield_dilapidated_house_80182A18(((TmdObject*)arg0->extra)->coords, mem->field_26, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x40;
    scale        -= 0x10;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 0x10) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Per-frame handler of the effect family whose work block is `DdhEffWork`
/// (`task->spawnArg2`). While the `Gp_State1C` state word at 0x4 is clear it
/// seeds the ramp (0x80 / 0x100) on the first frame and then, every frame,
/// clears the model coordinate's update flag, refreshes the coordinate and feeds
/// the angle/scale pair to `func_dryfield_dilapidated_house_80183728`, stepping
/// the scale by -8 and the angle by +0x80. Once the scale drops below 9 - and
/// immediately when that state word has already reached 4 - it releases the work
/// block through `Gp_ReleaseState1CMem`.
void func_dryfield_dilapidated_house_80183D5C(Task* arg0)
{
    DdhEffWork*    mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1, 0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    func_dryfield_dilapidated_house_80183728(coord, mem->field_26, 0x100, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
