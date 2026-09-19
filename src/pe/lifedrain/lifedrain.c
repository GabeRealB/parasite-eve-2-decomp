#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "pe/lifedrain.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Per-level tuning for the life drain: rows are PE levels 1-3.
LifeDrainScale D_lifedrain_80130AB4[] = {
    { 0x0008, 0x0080, 0x0100, 0x0400, 0x0040 },
    { 0x000C, 0x00B0, 0x0200, 0x0500, 0x0048 },
    { 0x0010, 0x00E0, 0x0300, 0x0600, 0x0050 },
};

/// The `SndEvt_EnqueueType6` ids: three drain cues then three release cues.
s32 D_lifedrain_80130AD4[] = {
    0xE0210001,
    0xE0240001,
    0xE0270001,
    0xE0210002,
    0xE0240002,
    0xE0270002,
};

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// Health the drain has banked so far, in HP. Every mote that reaches the
/// collector adds its share here; the cast pays it into `Player_Status.hp`
/// when it ends.
extern u16 D_80115404;

void PeShared801305c0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Runs one frame of the life-drain cast: a five-state machine driven by
/// `Task::state`, published in `D_lifedrain_80130B0C` so every mote can find
/// it. Cancelling (`D_80114C0B == -2` or `Gp_State1C->fadeState >= 4`) releases
/// the work block, and states 0 and 1 first cash the banked `D_80115404` into
/// `Player_Status.hp`, clamped to the max in `field_1a`.
///
/// State 0 parents the effect coordinate at the origin with an identity
/// rotation, seeds the combo level `index` from `Gp_StateC08.field_0`, takes
/// the funnel radii `scale` / `period` from that row of
/// `D_lifedrain_80130AB4`, rolls one yaw per wedge into `D_lifedrain_80130AEC`
/// and spawns the three `0x600EA` motes 0x2AA apart around the circle. State 1
/// fades the entry quad out 0x10 a frame, plays the row's cue on tick 3 and on
/// tick 0x1E either banks the drain and moves to state 2 or, with nothing
/// banked, skips straight to the state-4 release.
///
/// State 2 is the funnel proper: it grows `scale` towards the row's
/// `field_2` cap, steps `angle` by `unk8`, redraws the wedges, the two rings
/// and the arcs, and each frame throws one `0x600AD` spark on an LCG yaw at
/// `angle` radius. Once `angle` passes the row's `unk6` it moves to state
/// 3, which shrinks `scale` by 0x10 a frame and redraws the same funnel
/// until it drops below 0x11, then releases through state 4.
/// Scratch for the drain ribbon, plus the task handle it spawns.
/// lists an object in the linker script at its first subsegment, and this has
s16          D_lifedrain_80130AEC[16] = { 0 };
struct Task* D_lifedrain_80130B0C     = NULL;

void func_lifedrain_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((D_80114C0B == -2) || (Gp_State1C->fadeState >= 4)) {
        if ((arg0->state < 2) && (arg0->spawnArg1 != 0)) {
            Player_Status.hp = (u16)Player_Status.hp + D_80115404;
            if (Player_Status.hp > Player_Status.hpMax) {
                Player_Status.hp = Player_Status.hpMax;
            }
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age = (u16)mem->age + 1;
    switch (arg0->state) {
        case 0: {
            GpEffWork*  spawned;
            GpMtxWords* rot;

            D_lifedrain_80130B0C = arg0;
            rot                  = (GpMtxWords*)&coord->coord;
            coord->sub           = mem->parent;
            rot->w0              = 0x1000;
            rot->w1              = 0;
            rot->w2              = 0x1000;
            rot->w3              = 0;
            rot->h4              = 0x1000;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->flg           = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            mem->index  = (Gp_StateC08.field_0 % 10) - 1;
            mem->scale  = D_lifedrain_80130AB4[mem->index].field_2;
            mem->angle  = 0x80;
            mem->period = D_lifedrain_80130AB4[mem->index].field_2;
            i           = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                do {
                    s32 rng;

                    rng                     = Gp_LcgState * 5 + 0x71357911;
                    D_lifedrain_80130AEC[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    Gp_LcgState             = rng;
                } while (++i < D_lifedrain_80130AB4[mem->index].unk0);
            }
            i = 0;
            do {
                spawned = Gp_SpawnEff(0x600EA, coord, i, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            Gp_StateC08.field_6 |= 8;
            return;
        }
        case 1:
            if (mem->scale != 0) {
                mem->scale = (u16)mem->scale - 0x10;
                rgb[0]     = (u16)mem->scale >> 1;
                rgb[1]     = (u16)mem->scale >> 1;
                rgb[2]     = *(u8*)&mem->scale;
                Gp_DrawFadeQuad(rgb, 1);
            }
            if (mem->age == 0x1E) {
                if (arg0->spawnArg1 != 0) {
                    Player_Status.hp = (u16)Player_Status.hp + D_80115404;
                    if (Player_Status.hp > Player_Status.hpMax) {
                        Player_Status.hp = Player_Status.hpMax;
                    }
                    arg0->state = 2;
                } else {
                    arg0->state = 4;
                }
                return;
            }
            if (mem->age != 3) {
                return;
            }
            Gp_UpdateCoord(coord);
            if (arg0->spawnArg1 != 0) {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index + 3],
                                    (s8)Gp_GetObjPan(coord),
                                    (s8)Gp_GetObjDepth(coord));
            } else {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->index],
                                    (s8)Gp_GetObjPan(coord),
                                    (s8)Gp_GetObjDepth(coord));
            }
            return;
        case 2: {
            LifeDrainScale* t2;
            GpEffWork*      spawned;
            s16*            p;
            s32             val;

            Gp_UpdateCoord(coord);
            if (mem->period != 0) {
                mem->period = (u16)mem->period - 0x10;
                rgb[0]      = (u16)mem->period >> 1;
                rgb[1]      = (u16)mem->period >> 1;
                rgb[2]      = *(u8*)&mem->period;
                Gp_DrawFadeQuad(rgb, 1);
            }
            val = mem->scale;
            if (val < (s16)D_lifedrain_80130AB4[mem->index].field_2) {
                val += 0x10;
            }
            mem->scale = val;
            mem->angle = (u16)mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            rgb[0]     = (u16)mem->scale >> 1;
            rgb[1]     = (u16)mem->scale >> 1;
            rgb[2]     = *(u8*)&mem->scale;
            i          = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    PeShared801305c0(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if ((u16)mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if ((u16)mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->angle + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->step   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 0);
            gte_SetRotMatrix((MATRIX*)&coord->coord);
            gte_ldv0(&mem->move);
            gte_rtv0_real();
            gte_stsv(&mem->move);
            mem->move.vx = (rcos(mem->step) * mem->angle) >> 12;
            mem->move.vy = (rsin(mem->step) * mem->angle) >> 12;
            mem->move.vz = 0;
            spawned      = Gp_SpawnEff(0x600AD, coord, D_lifedrain_80130AB4[mem->index].unk6,
                                       &mem->move);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->task);
            }
            if (mem->angle >= D_lifedrain_80130AB4[mem->index].unk6) {
                arg0->state = 3;
            }
            return;
        }
        case 3: {
            LifeDrainScale* t2;
            s16*            p;

            Gp_UpdateCoord(coord);
            mem->scale = (u16)mem->scale - 0x10;
            mem->angle = (u16)mem->angle + (u16)D_lifedrain_80130AB4[mem->index].unk8;
            if (mem->scale < 0x11) {
                arg0->state = 4;
            }
            rgb[0] = (u16)mem->scale >> 1;
            rgb[1] = (u16)mem->scale >> 1;
            rgb[2] = *(u8*)&mem->scale;
            i      = 0;
            if (D_lifedrain_80130AB4[mem->index].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    PeShared801305c0(coord, mem->angle, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->index].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if ((u16)mem->age & 1) {
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->angle + D_lifedrain_80130AB4[mem->index].field_4),
                           0x80, rgb);
                if (mem->index == 2) {
                    if ((u16)mem->age & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->angle + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            return;
        }
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}
