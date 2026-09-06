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

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// Health the drain has banked so far, in HP. Every mote that reaches the
/// collector adds its share here; the cast pays it into `Wip_SysConfig.field_18`
/// when it ends.
extern u16 D_80115404;

void PeShared801305c0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Runs one frame of the life-drain cast: a five-state machine driven by
/// `Task::state`, published in `D_lifedrain_80130B0C` so every mote can find
/// it. Cancelling (`D_80114C0B == -2` or `Gp_State1C->field_E >= 4`) releases
/// the work block, and states 0 and 1 first cash the banked `D_80115404` into
/// `Wip_SysConfig.field_18`, clamped to the max in `field_1a`.
///
/// State 0 parents the effect coordinate at the origin with an identity
/// rotation, seeds the combo level `field_20` from `Gp_StateC08.field_0`, takes
/// the funnel radii `field_24` / `field_28` from that row of
/// `D_lifedrain_80130AB4`, rolls one yaw per wedge into `D_lifedrain_80130AEC`
/// and spawns the three `0x600EA` motes 0x2AA apart around the circle. State 1
/// fades the entry quad out 0x10 a frame, plays the row's cue on tick 3 and on
/// tick 0x1E either banks the drain and moves to state 2 or, with nothing
/// banked, skips straight to the state-4 release.
///
/// State 2 is the funnel proper: it grows `field_24` towards the row's
/// `field_2` cap, steps `field_26` by `unk8`, redraws the wedges, the two rings
/// and the arcs, and each frame throws one `0x600AD` spark on an LCG yaw at
/// `field_26` radius. Once `field_26` passes the row's `unk6` it moves to state
/// 3, which shrinks `field_24` by 0x10 a frame and redraws the same funnel
/// until it drops below 0x11, then releases through state 4.
void func_lifedrain_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
        if ((arg0->state < 2) && (arg0->spawnArg1 != 0)) {
            Wip_SysConfig.field_18 = (u16)Wip_SysConfig.field_18 + D_80115404;
            if (Wip_SysConfig.field_18 > Wip_SysConfig.field_1a) {
                Wip_SysConfig.field_18 = Wip_SysConfig.field_1a;
            }
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0: {
            GpEffWork*  spawned;
            GpMtxWords* rot;

            D_lifedrain_80130B0C = arg0;
            rot                  = (GpMtxWords*)&coord->coord;
            coord->sub           = mem->field_8;
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
            arg0->state   = 1;
            mem->field_20 = (Gp_StateC08.field_0 % 10) - 1;
            mem->field_24 = D_lifedrain_80130AB4[mem->field_20].field_2;
            mem->field_26 = 0x80;
            mem->field_28 = D_lifedrain_80130AB4[mem->field_20].field_2;
            i             = 0;
            if (D_lifedrain_80130AB4[mem->field_20].unk0 > 0) {
                do {
                    s32 rng;

                    rng                     = Gp_LcgState * 5 + 0x71357911;
                    D_lifedrain_80130AEC[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    Gp_LcgState             = rng;
                } while (++i < D_lifedrain_80130AB4[mem->field_20].unk0);
            }
            i = 0;
            do {
                spawned = Gp_SpawnEff(0x600EA, coord, i, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                i += 0x2AA;
            } while (i < 0x556);
            Gp_StateC08.field_6 |= 8;
            return;
        }
        case 1:
            if (mem->field_24 != 0) {
                mem->field_24 = (u16)mem->field_24 - 0x10;
                rgb[0]        = (u16)mem->field_24 >> 1;
                rgb[1]        = (u16)mem->field_24 >> 1;
                rgb[2]        = *(u8*)&mem->field_24;
                Gp_DrawFadeQuad(rgb, 1);
            }
            if (mem->field_22 == 0x1E) {
                if (arg0->spawnArg1 != 0) {
                    Wip_SysConfig.field_18 = (u16)Wip_SysConfig.field_18 + D_80115404;
                    if (Wip_SysConfig.field_18 > Wip_SysConfig.field_1a) {
                        Wip_SysConfig.field_18 = Wip_SysConfig.field_1a;
                    }
                    arg0->state = 2;
                } else {
                    arg0->state = 4;
                }
                return;
            }
            if (mem->field_22 != 3) {
                return;
            }
            Gp_UpdateCoord(coord);
            if (arg0->spawnArg1 != 0) {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->field_20 + 3],
                                    (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            } else {
                SndEvt_EnqueueType6(D_lifedrain_80130AD4[mem->field_20],
                                    (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            return;
        case 2: {
            LifeDrainScale* t2;
            GpEffWork*      spawned;
            s16*            p;
            s32             val;

            Gp_UpdateCoord(coord);
            if (mem->field_28 != 0) {
                mem->field_28 = (u16)mem->field_28 - 0x10;
                rgb[0]        = (u16)mem->field_28 >> 1;
                rgb[1]        = (u16)mem->field_28 >> 1;
                rgb[2]        = *(u8*)&mem->field_28;
                Gp_DrawFadeQuad(rgb, 1);
            }
            val = mem->field_24;
            if (val < (s16)D_lifedrain_80130AB4[mem->field_20].field_2) {
                val += 0x10;
            }
            mem->field_24 = val;
            mem->field_26 = (u16)mem->field_26 + (u16)D_lifedrain_80130AB4[mem->field_20].unk8;
            rgb[0]        = (u16)mem->field_24 >> 1;
            rgb[1]        = (u16)mem->field_24 >> 1;
            rgb[2]        = *(u8*)&mem->field_24;
            i             = 0;
            if (D_lifedrain_80130AB4[mem->field_20].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    PeShared801305c0(coord, mem->field_26, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->field_20].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->field_26 >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->field_26 >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->field_26, 0x80, rgb);
            if ((u16)mem->field_22 & 1) {
                Gp_DrawArc(coord, 0x80, mem->field_26, rgb);
            }
            if (mem->field_20 != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->field_26 + D_lifedrain_80130AB4[mem->field_20].field_4),
                           0x80, rgb);
                if (mem->field_20 == 2) {
                    if ((u16)mem->field_22 & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->field_26 + D_lifedrain_80130AB4[2].field_4),
                                   rgb);
                    }
                }
            }
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_2A = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 0);
            gte_SetRotMatrix((MATRIX*)&coord->coord);
            gte_ldv0(&mem->field_10);
            gte_rtv0_real();
            gte_stsv(&mem->field_10);
            mem->field_10 = (rcos(mem->field_2A) * mem->field_26) >> 12;
            mem->field_12 = (rsin(mem->field_2A) * mem->field_26) >> 12;
            mem->field_14 = 0;
            spawned       = Gp_SpawnEff(0x600AD, coord, D_lifedrain_80130AB4[mem->field_20].unk6,
                                        (SVECTOR*)&mem->field_10);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->field_0);
            }
            if (mem->field_26 >= D_lifedrain_80130AB4[mem->field_20].unk6) {
                arg0->state = 3;
            }
            return;
        }
        case 3: {
            LifeDrainScale* t2;
            s16*            p;

            Gp_UpdateCoord(coord);
            mem->field_24 = (u16)mem->field_24 - 0x10;
            mem->field_26 = (u16)mem->field_26 + (u16)D_lifedrain_80130AB4[mem->field_20].unk8;
            if (mem->field_24 < 0x11) {
                arg0->state = 4;
            }
            rgb[0] = (u16)mem->field_24 >> 1;
            rgb[1] = (u16)mem->field_24 >> 1;
            rgb[2] = *(u8*)&mem->field_24;
            i      = 0;
            if (D_lifedrain_80130AB4[mem->field_20].unk0 > 0) {
                t2 = D_lifedrain_80130AB4;
                p  = D_lifedrain_80130AEC;
                do {
                    PeShared801305c0(coord, mem->field_26, *p, rgb);
                    p += 1;
                } while (++i < t2[mem->field_20].unk0);
            }
            Gp_DrawRing(coord, (s16)(u16)mem->field_26 >> 1, rgb);
            Gp_DrawRing(coord, (s16)(u16)mem->field_26 >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->field_26, 0x80, rgb);
            if ((u16)mem->field_22 & 1) {
                Gp_DrawArc(coord, 0x80, mem->field_26, rgb);
            }
            if (mem->field_20 != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord,
                           (s16)((u16)mem->field_26 + D_lifedrain_80130AB4[mem->field_20].field_4),
                           0x80, rgb);
                if (mem->field_20 == 2) {
                    if ((u16)mem->field_22 & 1) {
                        Gp_DrawArc(coord, 0x80,
                                   (s16)((u16)mem->field_26 + D_lifedrain_80130AB4[2].field_4),
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
