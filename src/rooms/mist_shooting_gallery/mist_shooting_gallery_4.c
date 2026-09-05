#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "rooms/mist_shooting_gallery.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
extern TaskDesc                  D_mist_shooting_gallery_801856B8;
extern TaskDesc                  D_mist_shooting_gallery_801856D0;
extern TaskDesc                  D_80134F94;
extern u8                        D_801153F4;
extern u8                        D_80073BA9;
extern MistShootingGalleryRounds D_mist_shooting_gallery_8017DB8C;
/// The wave script the round loop walks: a run of records sharing
/// `field_00` is spawned together, `0xFFF1` waits for the current wave to
/// clear and `0xFFFF` ends the course.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186904;
/// The second course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186910;
/// The first bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C;
/// The second bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186908;
/// The five course wave scripts as the one array they are: element 0 is the
/// bonus course's script, and elements 1..4 are the same pointers the round
/// scripts above reach by their own addresses (`0x80186904` .. `0x80186910`).
/// The array type is what `func_mist_shooting_gallery_80182C58` needs - an
/// array element counts as a struct reference to GCC 2.8.1's alias analysis,
/// so the load is ordered against the `work->field_04` store that precedes it.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[];
/// Main-executable flag gating the countdown steps: while it is set the
/// gallery holds its current step instead of advancing the digit sprite.
extern u8 D_80071075;
/// Bonus-course variant selected before the round starts. It picks both the
/// banner sprite (`variant + 0xB`) and the colour it is drawn in (variant 2
/// uses 2 instead of 0x10).
extern s8 D_80072310;
/// Gameplay-side abort request. While it is 1 the bonus course tears itself
/// down: the state machine remembers where it was in `field_06` / `field_21`
/// and jumps to the state-9 shutdown banner.
extern s8   D_80114C0B;
extern s32  Gp_LcgState;
extern void func_8014A908(void);
extern void func_8014A9A0(void);
extern void func_8014B0D4(void);
void        func_mist_shooting_gallery_80182294(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3);
void        func_mist_shooting_gallery_801826C4(GsCOORDINATE2* coord, SVECTOR* arg1, s32 arg2, s16 arg3);
void        func_mist_shooting_gallery_80184A80(Task* arg0);
void        func_mist_shooting_gallery_8018458C(MistShootingGalleryWork* work);
u16         func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work);
void        func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2);
GpEnemy*    func_mist_shooting_gallery_80184CD0(Task* arg0, MistShootingGallerySpawn* arg1);
void        func_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
void        func_mist_shooting_gallery_801846F4(s32 arg0, s16 arg1, s32 arg2);

/// Per-frame update for one gallery muzzle-flash / tracer effect. The task's
/// `GpEffWork` holds the tracer's endpoint (`field_18`..`field_1C`), its spin
/// angle (`field_26`) and its brightness ramp (`field_24`); the handwritten GTE
/// routines below draw the beam and its glow from the task's own coordinate.
/// While `Gp_State1C` is fading (`field_4 != 0`) the effect only redraws; once
/// the fade is over it seeds a random endpoint around the coordinate's world
/// position, then fades out by 8 per frame and releases its pool block.
void func_mist_shooting_gallery_80182064(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            rand0;
    u32            rand1;
    u32            rand2;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;

    if (Gp_State1C->field_4 != 0) {
        func_mist_shooting_gallery_80182294(coord, work->field_20, 0x600, work->field_26);
        func_mist_shooting_gallery_801826C4(coord, (SVECTOR*)&work->field_18, work->field_20, 0x600);
        rgb[0] = (u16)work->field_24 >> 1;
        rgb[1] = (u16)work->field_24 >> 1;
        rgb[2] = work->field_24;
        Gp_DrawFadeQuad(rgb, 1);
        return;
    }

    work->field_22++;
    switch (task->state) {
        case 0:
            coord->sub        = work->field_8;
            coord->coord.t[0] = 0;
            coord->coord.t[1] = 0;
            coord->coord.t[2] = 0;
            coord->flg        = 0;
            task->state       = 1;

            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            rand0          = Gp_LcgState;
            work->field_18 = *(u16*)&coord->workm.t[0] - ((rand0 >> 16 & 0x3FF) - 0x200);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            rand1          = Gp_LcgState;
            work->field_1A = coord->workm.t[1] - 0x800;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            rand2          = Gp_LcgState;
            work->field_1C = *(u16*)&coord->workm.t[2] - ((rand1 >> 16 & 0x3FF) - 0x200);
            work->field_24 = 0x80;
            work->field_26 = rand2 >> 16 & 0xFFF;
        case 1:
            if (work->field_22 & 1) {
                func_mist_shooting_gallery_80182294(coord, ++work->field_20, 0x400, work->field_26);
                func_mist_shooting_gallery_801826C4(coord, (SVECTOR*)&work->field_18, work->field_20, 0x400);
            }
            rgb[0] = (u16)work->field_24 >> 1;
            rgb[1] = (u16)work->field_24 >> 1;
            rgb[2] = work->field_24;
            Gp_DrawFadeQuad(rgb, 1);
            work->field_24 -= 8;
            if (work->field_24 < 8) {
                Gp_ReleaseState1CMem(work, task);
            }
            return;
    }
}

/// Draws one frame of the gallery's muzzle flash: a semi-transparent
/// `POLY_FT4` centred on the effect coordinate's world position, projected by
/// a single `RTPS`. Its half-size is `arg2 * 39 / otz`, and the four corners
/// are that half-size swung to `arg3` and to `arg3 + 0x400`, so the sprite
/// spins with the effect's angle. `arg1` picks one of six 40-pixel-wide
/// frames out of the texture page, and the primitive is queued twice into the
/// same OT slot. Nothing is drawn if the centre projects off-screen.
void func_mist_shooting_gallery_80182294(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3)
{
    void**                           scratch;
    u8*                              head;
    MistShootingGalleryFlashScratch* block;
    MistShootingGalleryFlashScratch* vecp;
    POLY_FT4*                        prim;
    s16                              u;
    u16                              vz;

    scratch                                                   = (void**)G_SCRATCH_HEAD;
    head                                                      = *scratch;
    ((MistShootingGalleryFlashScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                                     = (MistShootingGalleryFlashScratch*)(head - 0x1C);
    block->vec.vy                                             = *(u16*)&coord->workm.t[1];
    vz                                                        = *(u16*)&coord->workm.t[2];
    *scratch                                                  = block;
    block->vec.vz                                             = vz;
    vecp                                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((MistShootingGalleryFlashScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((MistShootingGalleryFlashScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((MistShootingGalleryFlashScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        prim->v2    = 0x5F;
        prim->v3    = 0x5F;
        u           = arg1 % 6;
        prim->u0    = u * 40;
        prim->u1    = u * 40 + 0x27;
        prim->u2    = u * 40;
        prim->u3    = u * 40 + 0x27;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws one frame of the gallery's tracer beam: a semi-transparent
/// `POLY_FT4` stretched between the effect coordinate's world position and
/// `arg1`, the endpoint the effect picked when it spawned. Both points are
/// projected with their own `RTPS` and the quad is given a half-width of
/// `arg3 * 23 / otz`, rotated onto the beam's own screen-space angle so the
/// strip stays perpendicular to it. `arg2` selects the strip out of the
/// texture page: bit 0 picks the left or right half and bit 1 the upper or
/// lower row. Nothing is drawn if either endpoint projects off-screen.
void func_mist_shooting_gallery_801826C4(GsCOORDINATE2* coord, SVECTOR* arg1, s32 arg2, s16 arg3)
{
    void**                          scratch;
    u8*                             head;
    MistShootingGalleryBeamScratch* block;
    MistShootingGalleryBeamScratch* vecp;
    POLY_FT4*                       prim;
    s16                             ang;
    u16                             vz;

    scratch                                                  = (void**)G_SCRATCH_HEAD;
    head                                                     = *scratch;
    ((MistShootingGalleryBeamScratch*)(head - 0x20))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                                    = (MistShootingGalleryBeamScratch*)(head - 0x20);
    block->vec.vy                                            = *(u16*)&coord->workm.t[1];
    vz                                                       = *(u16*)&coord->workm.t[2];
    *scratch                                                 = block;
    block->vec.vz                                            = vz;
    vecp                                                     = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((MistShootingGalleryBeamScratch*)(head - 0x20))->sxy0);
    gte_stflg(&((MistShootingGalleryBeamScratch*)(head - 0x20))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((MistShootingGalleryBeamScratch*)(head - 0x20))->otz);
        gte_ldv0(arg1);
        gte_rtps_real();
        gte_stsxy(&((MistShootingGalleryBeamScratch*)(head - 0x20))->sxy1);
        gte_stflg(&((MistShootingGalleryBeamScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4287;
            prim->u0    = (arg2 & 1) << 7;
            prim->v0    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u1    = ((arg2 & 1) << 7) + 0x7F;
            prim->v1    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u2    = (arg2 & 1) << 7;
            prim->v2    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            prim->u3    = ((arg2 & 1) << 7) + 0x7F;
            prim->v3    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            ang         = ratan2(block->sxy1.vy - block->sxy0.vy, block->sxy1.vx - block->sxy0.vx);
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang)) >> 12;
            prim->x0    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy1.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy1.vy + *(u16*)&block->dy;
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy1.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy1.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

void func_mist_shooting_gallery_80182B1C(Task* arg0)
{
    GpActorWork*             slot;
    GameActor*               actor;
    MistShootingGalleryWork* work;

    slot  = (GpActorWork*)Game_GetPtrSlot(3);
    actor = slot->actor;

    work        = (MistShootingGalleryWork*)Mem_Calloc(0x24, 0);
    arg0->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }

    D_mist_shooting_gallery_8018E0C4 = arg0;
    arg0->exitCallback               = func_mist_shooting_gallery_80184A80;
    arg0->state++;
    work->difficulty = arg0->spawnArg1 & 0xF;
    work->field_0C   = -0xDC;

    ((GpActorD4Rec*)actor->field_14C)->field_4 =
        (((GpActorD4Rec*)actor->field_14C)->field_C + D_80112F60[D_80073BA9]) << 1;
    func_801066DC(slot, 1);

    if (work->difficulty < 3) {
        Gp_StateC08.field_6 |= 2;
        if (work->difficulty < 2) {
            actor->field_97B = 1;
            Display_AcquireRef();
        }
    }
    Gp_StateF0.field_0      = 0;
    Game_Session->field_126 = 0;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
}

/// Per-frame update for the gallery's bonus course. START (`0x100`) aborts the
/// whole mini-game; otherwise the seventeen states run the banner countdown
/// (`field_20` steps the sprite, `D_80072310` picks which variant), seed the
/// course by spawning individual records of `D_mist_shooting_gallery_80186900[0]`
/// on a timer, and finally enter the wave loop of state 15. State 16 is the
/// out-of-ammo banner: it is entered from anywhere the moment the equipped
/// weapon's stock drops below the round's minimum, remembers the interrupted
/// state in `field_06` and returns to it once the banner has played out.
void func_mist_shooting_gallery_80182C58(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    s32                       limit;
    s32                       bonus;
    u16                       t1;
    u16                       t2;
    u16                       t5;
    u16                       t6;
    u16                       t8;
    u16                       t9;
    u16                       t12;
    u16                       t13;
    u16                       key;
    u16                       wave;
    u16                       prev;
    u8                        step;

    work  = (MistShootingGalleryWork*)arg0->idMap;
    bonus = D_80072310;
    if (Pad_CheckButtons(0, 1, 0x100) != 0) {
        func_8014A9A0();
        return;
    }

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x708;
            work->field_0A = 0x3C;
            work->field_04++;
        case 1:
            t1             = work->field_0A - 1;
            work->field_0A = t1;
            if ((s32)(t1 << 16) <= 0) {
                work->field_0A = 0x1E;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, 0, 0x8E0);
            }
            break;
        case 2:
            t2             = work->field_0A - 1;
            work->field_0A = t2;
            if ((s32)(t2 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 1, 0x8E0);
            }
            break;
        case 3:
            work->field_04++;
            func_mist_shooting_gallery_80184BB8(0x11, 4, 0x8E0);
            break;
        case 4:
            work->field_0A = 0x1E;
            work->field_04++;
            func_mist_shooting_gallery_80184BB8(0x11, bonus + 5, 0x8E0);
            break;
        case 5:
            t5             = work->field_0A;
            work->field_0A = t5 - 1;
            if ((s32)(t5 << 16) <= 0) {
                work->field_0A = 0xF;
                work->field_04++;
                Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
            }
            break;
        case 6:
            t6             = work->field_0A - 1;
            work->field_0A = t6;
            if ((s32)(t6 << 16) <= 0) {
                work->field_04++;
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
            }
            break;
        case 7:
            if (work->field_1D != 0) {
                work->field_0A = 0xF;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 8, 0x8E0);
            } else if (work->field_0E == 0) {
                work->field_0A = 0xF;
                work->field_04++;
            }
            break;
        case 8:
            if (work->field_0E == 0) {
                t8             = work->field_0A - 1;
                work->field_0A = t8;
                if ((s32)(t8 << 16) <= 0) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                    spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                    work->field_08++;
                }
            }
            break;
        case 9:
            t9             = work->field_0A - 1;
            work->field_0A = t9;
            if ((s32)(t9 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xB, 0x8E0);
            }
            break;
        case 10:
            if (work->field_0E == 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
                if (work->field_08 == 7) {
                    work->field_04++;
                }
            }
            break;
        case 11:
            if (work->field_0E == 0) {
                work->field_0A = 0x1E;
                work->field_04++;
            }
            break;
        case 12:
            t12            = work->field_0A - 1;
            work->field_0A = t12;
            if ((s32)(t12 << 16) <= 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
                if (work->field_08 == 0xC) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xA;
                }
            }
            break;
        case 13:
            t13            = work->field_0A - 1;
            work->field_0A = t13;
            if ((s32)(t13 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xE, 0x8E0);
            }
            break;
        case 14:
            if (work->field_0E != 0) {
                break;
            }
            work->field_04++;
            Display_ReleaseRef();
        case 15:
            spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 16:
            func_mist_shooting_gallery_80184BB8(0x11, work->field_20, 0x8E0);
            step = work->field_20;
            if (step == 0x15) {
                work->field_04 = work->field_06;
                Display_ReleaseRef();
            } else {
                work->field_20 = step + 1;
            }
            break;
    }

    if (work->field_1E == 0) {
        limit = 2;
        if (Wip_SysConfig.field_21 == 2) {
            limit = 4;
        }
        if (Gp_ConsumeSlotQty(Wip_SysConfig.field_21 + 0x7F, 0) < limit) {
            prev           = work->field_04;
            work->field_04 = 0x10;
            work->field_1E = 1;
            work->field_20 = 0x11;
            work->field_06 = prev;
        }
    }
}

/// Per-frame update for the gallery course itself. START (`0x100`) aborts the
/// whole mini-game; otherwise the state runs a "3, 2, 1, GO" countdown
/// (`field_20` steps the digit sprite once a second) before releasing the
/// display reference and entering the wave loop. The loop spawns every record
/// of `D_mist_shooting_gallery_80186904` that carries the current wave number,
/// draws the remaining time, and restarts the state machine once the clock
/// runs out.
void func_mist_shooting_gallery_801831B0(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    u16                       intro;
    u16                       ready;
    u16                       start;
    u16                       key;
    u16                       wave;
    u8                        step;

    work = (MistShootingGalleryWork*)arg0->idMap;
    if (Pad_CheckButtons(0, 1, 0x100) != 0) {
        func_8014A9A0();
        return;
    }

    switch (work->field_04) {
        case 0:
            work->field_02 = 0xE10;
            work->field_0A = 0x3C;
            work->field_04++;
        case 1:
            intro          = work->field_0A - 1;
            work->field_0A = intro;
            if ((s32)(intro << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x12, work->field_20, 0x8E0);
                step = work->field_20;
                if (step == 4) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                    return;
                }
                work->field_20 = step + 1;
                return;
            }
        default:
            return;
        case 2:
            ready          = work->field_0A;
            work->field_0A = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->field_0A = 0xA;
                work->field_04++;
                Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_ArmStateF0(1);
                return;
            }
            break;
        case 3:
            start          = work->field_0A;
            work->field_0A = start - 1;
            if ((s32)(start << 16) <= 0) {
                work->field_04++;
                Display_ReleaseRef();
                case 4:
                    spawn = &D_mist_shooting_gallery_80186904[work->field_08];
                    key   = spawn->field_00;
                    if (key != 0xFFFF) {
                        if (key != 0xFFF1) {
                            if (work->field_00 == key) {
                                do {
                                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                                    spawn++;
                                    work->field_08++;
                                } while (work->field_00 == spawn->field_00);
                            }
                            wave = work->field_00;
                            if (wave <= 0xFFEF) {
                                work->field_00 = wave + 1;
                            }
                        } else if (work->field_0E == 0) {
                            work->field_08++;
                        }
                    }
                    func_mist_shooting_gallery_8018458C(work);
                    if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                        work->field_04 = 0;
                        arg0->state++;
                    }
            }
            break;
    }
}

/// Per-frame update for the gallery's second bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `Game_Session::field_4`, gated on
/// the countdown hold `D_80071075`; states 4-5 wait on the player picking up
/// item 0x40, states 6-8 count the banner up through `field_20` while
/// `D_801153F4` holds, state 9 spawns the start jingle and state 10 is the
/// wave loop over `D_mist_shooting_gallery_80186908`. `D_80072310` picks the
/// banner sprite the hand-off draws (`variant + 4`).
void func_mist_shooting_gallery_8018341C(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    s32                       bonus;
    s32                       stocked;
    u16                       key;
    u16                       wave;
    u16                       ready;
    u8                        step;

    work  = (MistShootingGalleryWork*)arg0->idMap;
    bonus = D_80072310;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (D_80071075 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    step = work->field_20;
                    if (step == 3) {
                        work->field_0A = 0xF;
                        D_80115768     = 0;
                        work->field_04++;
                    } else {
                        work->field_20 = step + 1;
                    }
                }
            } else {
                work->field_0A--;
            }
            break;
        case 2:
            if ((u8)Game_Session->field_4 == 0x12) {
                if (work->field_0A <= 0) {
                    if (D_80071075 == 0) {
                        work->field_0A = 1;
                        work->field_04++;
                        func_mist_shooting_gallery_80184BB8(0x13, bonus + 4, 0x8E0);
                    }
                } else {
                    work->field_0A--;
                }
            }
            break;
        case 3:
            if (D_80071075 == 0) {
                work->field_1F = 0;
                work->field_20 = 8;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x13, 7, 0x8E0);
            }
            break;
        case 4:
            if (work->field_1F != 0) {
                stocked = Gp_HasStockedItem(0x40);
                if (stocked != 1) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 9) {
                        work->field_1F = 0;
                        work->field_04++;
                    }
                    work->field_20++;
                } else {
                    work->field_04 = 6;
                    work->field_0A = 1;
                    work->field_20 = 0xA;
                }
            }
            break;
        case 5:
            if (Gp_HasStockedItem(0x40) == 1) {
                work->field_0A = 0xF;
                work->field_04++;
            } else if (work->field_1F != 0) {
                func_mist_shooting_gallery_80184BB8(0x13, 9, 0x8E0);
                work->field_1F = 0;
            }
            break;
        case 6:
            if (work->field_0A <= 0) {
                if (D_801153F4 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 0xB) {
                        work->field_0A = 0xF;
                        work->field_04++;
                        Gp_ArmStateF0(1);
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 7:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_80186908[work->field_08]);
                work->field_08++;
                if (work->field_08 == 3) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xF;
                }
            }
            break;
        case 8:
            if (work->field_0A <= 0) {
                if (D_801153F4 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 0x13) {
                        work->field_0A = 0xF;
                        work->field_04++;
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 9:
            ready          = work->field_0A;
            work->field_0A = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->field_0A = 0xA;
                work->field_04++;
                Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
            }
            break;
        case 10:
            spawn = &D_mist_shooting_gallery_80186908[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x49) {
                        work->field_08 = 0xE;
                        work->field_00 = 0xF0;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
    }
}

/// Per-frame update for the gallery's first bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `Game_Session::field_4`, state 4
/// seeds the first two records of `D_mist_shooting_gallery_8018690C`, states
/// 5-7 hand the player over to actor mode 2 while the banner counts up through
/// `field_20`, and state 8 is the wave loop proper. `D_80114C0B` is the abort
/// request: once it is raised the machine saves its place in `field_06` /
/// `field_21` and jumps to the state-9 shutdown banner, which restores them.
void func_mist_shooting_gallery_801838FC(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    GpXformArg                xform;
    GameActor*                actor;
    s32                       mode;
    s32                       bonus;
    u16                       key;
    u16                       wave;
    u16                       prev;
    s32                       abort;
    u8                        step;
    u8                        hold;

    work  = (MistShootingGalleryWork*)arg0->idMap;
    bonus = D_80072310;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (D_80071075 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x14, 0, 0x8E0);
                    work->field_0A = 0xF;
                    D_80115768     = 0;
                    work->field_04++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 2:
            if ((u8)Game_Session->field_4 == 0x12) {
                if (work->field_0A <= 0) {
                    if (D_80071075 == 0) {
                        work->field_0A = 1;
                        work->field_04++;
                        func_mist_shooting_gallery_80184BB8(0x14, 7, 0x8E0);
                    }
                } else {
                    work->field_0A--;
                }
            }
            break;
        case 3:
            if (work->field_1F != 0) {
                work->field_0A = 0x1E;
                work->field_04++;
                Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_ArmStateF0(1);
            }
            break;
        case 4:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_8018690C[work->field_08]);
                work->field_08++;
                if (work->field_08 == 2) {
                    work->field_20 = 8;
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xF;
                }
            }
            break;
        case 5:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                if (work->field_20 == 0xA) {
                    work->field_0A = 0x1E;
                    work->field_04++;
                    func_800E9BDC(5, 0xA);
                    xform.field_12 = 0xC00;
                    ((void (*)(GpActorWork*, s32, GpXformArg*, s32))func_80104E00)(
                        (GpActorWork*)Game_GetPtrSlot(3), 0, &xform, 0);
                }
                work->field_20++;
            }
            break;
        case 6:
            actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
            func_800E9BDC(5, 0xA);
            if (actor->field_982 == 0) {
                ((void (*)(GpActorWork*, s32, s32, s32))Gp_EnterActorMode2)(
                    (GpActorWork*)Game_GetPtrSlot(3), 0, 2, 0);
                work->field_04++;
                mode                 = 0x10;
                Gp_StateC08.field_6 |= 0x10;
                work->field_0A       = 4;
                work->field_20       = 0xE;
                if (bonus == 2) {
                    mode = 2;
                }
                func_mist_shooting_gallery_80184BB8(0x14, bonus + 0xB, mode);
            }
            break;
        case 7:
            func_800E9BDC(5, 0xA);
            if (work->field_0A <= 0) {
                if ((u32)((u8)Gp_StateC08.field_A - 2) >= 2) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                    if (work->field_20 == 0x12) {
                        work->field_04++;
                        func_800E9BDC(0, 0xA);
                        Gp_StateC08.field_6 &= 0xFD;
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 8:
            spawn = &D_mist_shooting_gallery_8018690C[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x68) {
                        work->field_08 = 9;
                        work->field_00 = 0x90;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 9:
            if (work->field_0A <= 0) {
                if (D_801153F4 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                    step = work->field_20;
                    if (step == 0x17) {
                        work->field_04 = work->field_06;
                        work->field_20 = work->field_21;
                    } else {
                        work->field_20 = step + 1;
                    }
                }
            } else {
                work->field_0A--;
            }
            break;
    }

    if (work->field_1E == 0 && work->field_20 >= 0x13) {
        abort = D_80114C0B;
        if (abort == 1) {
            prev           = work->field_04;
            work->field_1E = abort;
            hold           = work->field_20;
            work->field_04 = 9;
            work->field_0A = 0x1E;
            work->field_20 = 0x13;
            work->field_06 = prev;
            work->field_21 = hold;
        }
    }
}

/// Per-frame update for the gallery's second course. Same shape as
/// `func_mist_shooting_gallery_801831B0`: a countdown that steps the digit
/// sprite through `field_20` (gated on `D_80071075`), a hand-off wait on
/// `Game_Session::field_4`, then the wave loop over
/// `D_mist_shooting_gallery_80186910`. `field_22` is the abort request - once
/// it is raised the state machine jumps to the 8 -> 9 shutdown, which releases
/// the `Gp_StateF0` reference and kills the task.
void func_mist_shooting_gallery_80183E78(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    u16                       timer;
    u16                       wave;
    u16                       key;
    u8                        step;

    work = (MistShootingGalleryWork*)arg0->idMap;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (D_80071075 == 0) {
                    func_mist_shooting_gallery_80184BB8(0x15, 0, 0x8E0);
                    work->field_0A = 0xF;
                    D_80115768     = 0;
                    work->field_04++;
                }
            } else {
                work->field_0A = (u16)work->field_0A - 1;
            }
            break;
        case 2:
            if ((u8)Game_Session->field_4 != 0x12) {
                break;
            }
            if (work->field_0A <= 0) {
                if (D_80071075 == 0) {
                    work->field_0A = 1;
                    work->field_04++;
                    func_mist_shooting_gallery_80184BB8(0x15, 7, 0x8E0);
                    work->field_20 = 8;
                }
            } else {
                work->field_0A = (u16)work->field_0A - 1;
            }
            break;
        case 3:
            if (work->field_1F == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
            if (work->field_20 == 0xA) {
                work->field_1F = 0;
                work->field_04++;
            }
            work->field_20++;
            break;
        case 4:
            if (work->field_1F == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
            if (work->field_20 == 0x10) {
                work->field_0A = 0x1E;
                work->field_04++;
            }
            work->field_20++;
            break;
        case 5:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                work->field_0A = 0xF;
                work->field_04++;
                Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_StateC08.field_6 &= 0xFD;
                Gp_ArmStateF0(1);
            }
            break;
        case 6:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                work->field_04++;
            }
            break;
        case 7:
            spawn = &D_mist_shooting_gallery_80186910[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x4E) {
                        work->field_08 = 0xB;
                        work->field_00 = 0x2B0;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 8:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
                step = work->field_20;
                if (step == 0x12) {
                    work->field_0A = 4;
                    D_80115768     = 0;
                    work->field_04++;
                    func_8014A9A0();
                } else {
                    work->field_20 = step + 1;
                }
            }
            break;
        case 9:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
                Task_Kill(arg0);
                return;
            }
            break;
    }

    if (work->field_1E == 0 && work->field_22 != 0) {
        work->field_04 = 8;
        work->field_1E = 1;
        work->field_0A = 3;
        work->field_20 = 0x11;
        D_80115768     = 1;
    }
}

void func_mist_shooting_gallery_801842D0(Task* arg0)
{
    MistShootingGalleryWork* work;
    GameActor*               actor;

    work  = (MistShootingGalleryWork*)arg0->idMap;
    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;

    switch (work->field_04) {
        case 0:
            if (work->field_0E != 0) {
                return;
            }
            work->field_0A = 0x1E;
            work->field_04++;
            Display_AcquireRef();
        case 1:
            if ((s16)work->field_0A-- > 0) {
                return;
            }
            work->field_0A = 0x5A;
            work->field_04++;
            Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 1, 0, 0);
            SndEvt_EnqueueType6(0x5114000F, 0, 0);
            return;
        case 2:
            if ((s16)--work->field_0A > 0) {
                return;
            }
            work->field_04++;
            actor->field_97B                  = 0;
            actor->field_983                  = 7;
            ((GpObj*)actor->field_AC)->flags |= 0x2000;
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
            func_8014A908();
            return;
        case 3:
            if (Game_Session->field_126 == 0) {
                work->field_04++;
            }
            return;
        case 4:
            if (Game_Session->field_126 == 1) {
                Display_ReleaseRef();
                Task_Kill(arg0);
            }
            return;
    }
}

s32 func_mist_shooting_gallery_80184470(s32 score)
{
    s32 bonus = 0;

    switch (((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap)->difficulty) {
        case 0:
            if (score >= 0x2710) {
                bonus = 0x12C;
            } else if (score >= 0x2328) {
                bonus = 0xC8;
            } else if (score >= 0x1F40) {
                bonus = 0x64;
            }
            break;
        case 1:
            if (score >= 0x43F8) {
                bonus = 0x12C;
            } else if (score >= 0x41A0) {
                bonus = 0xC8;
            } else if (score >= 0x3E80) {
                bonus = 0x64;
            }
            break;
        case 2:
            if (score > 0xC34F) {
                bonus = 0x12C;
            } else if (score > 0xB3AF) {
                bonus = 0xC8;
            } else if (score > 0x9857) {
                bonus = 0x64;
            }
            break;
        case 3:
            if (score > 0xEA5F) {
                bonus = 0x12C;
            } else if (score > 0xDABF) {
                bonus = 0xC8;
            } else if (score > 0xCB1F) {
                bonus = 0x64;
            }
            break;
        case 4:
            if (score > 0xD6D7) {
                bonus = 0x12C;
            } else if (score > 0xCF07) {
                bonus = 0xC8;
            } else if (score > 0xC34F) {
                bonus = 0x64;
            }
            break;
    }
    return bonus;
}

void func_mist_shooting_gallery_8018458C(MistShootingGalleryWork* work)
{
    s32 digit0;
    s32 digit1;
    s32 digit2;
    s32 digit3;
    s32 frames;

    frames = work->field_02;
    if (work->field_0C < -0x78) {
        work->field_0C += 0xA;
    }

    digit0 = frames / 18000;
    if (digit0 != 0) {
        frames %= 18000;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C, 0x46, digit0);

    digit1 = frames / 1800;
    if (digit1 != 0) {
        frames %= 1800;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0xC, 0x46, digit1);

    digit2 = frames / 300;
    if (digit2 != 0) {
        frames %= 300;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0x24, 0x46, digit2);

    digit3 = frames / 30;
    if (digit3 != 0) {
        frames %= 30;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0x30, 0x46, digit3);

    func_mist_shooting_gallery_801846F4(work->field_0C + 0x18, 0x46, 0xA);
}

void func_mist_shooting_gallery_801846F4(s32 arg0, s16 arg1, s32 arg2)
{
    SPRT*     p;
    DR_TPAGE* dr;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    p->w           = 0xF;
    p->h           = 0x13;
    p->clut        = 0x4140;
    setlen(p, 4);
    p->y0 = arg1;
    p->u0 = arg2 * 16;
    p->v0 = 0;
    setcode(p, 0x65);
    p->x0 = arg0;
    addPrim(Gpu_CurrentOt, p);

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000215;
    addPrim(Gpu_CurrentOt, dr);
}

void func_mist_shooting_gallery_801847D4(u8 arg0)
{
    TILE*     p;
    DR_TPAGE* dr;

    p              = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    p->x0          = -0xA8;
    p->y0          = -0x7C;
    p->w           = 0x180;
    p->h           = 0x100;
    setlen(p, 3);
    p->r0 = arg0;
    p->g0 = 0;
    p->b0 = 0;
    setcode(p, 0x62);
    addPrim(Gpu_CurrentOt, p);

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000235;
    addPrim(Gpu_CurrentOt, dr);
}

void func_mist_shooting_gallery_801848B4(void)
{
    GpEnemy*     enemy;
    TmdObject*   obj;
    GpCoordPose* coord;

    enemy = Gp_SpawnEnemyFromTable(&D_80134F94, 0, 0x200D, NULL);
    if (enemy != NULL) {
        obj           = (TmdObject*)enemy->task->extra;
        obj->field_24 = 0;
        obj->field_25 = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        coord             = (GpCoordPose*)((TmdObject*)enemy->task->extra)->field_8;
        coord->coord.t[0] = 0x1770;
        coord->coord.t[2] = 0xBB8;
        coord->coord.t[1] = 0;
        enemy->field_A    = 0x900;
    }
}

void func_mist_shooting_gallery_80184954(void)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap;

    work->field_1F = 1;
}

s32 func_mist_shooting_gallery_80184970(s32 arg0)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap;
    s32                      ret  = 0;

    if (work->difficulty < 3) {
        ret = arg0 >= 0xC8;
    } else if (arg0 >= 0x12C) {
        ret = 1;
    }
    return ret;
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_4", func_mist_shooting_gallery_801849BC);

void func_mist_shooting_gallery_80184A14(Task* arg0)
{
    MistShootingGalleryWork*  work   = (MistShootingGalleryWork*)arg0->idMap;
    MistShootingGalleryRounds rounds = D_mist_shooting_gallery_8017DB8C;

    rounds.rounds[work->difficulty]();
}

void func_mist_shooting_gallery_80184A80(Task* arg0)
{
    GameActor* actor;

    actor                             = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_97B                  = 0;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    Display_ReleaseRef();
    ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
    Task_Kill(arg0);
}

u16 func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work)
{
    u16 temp = work->field_02;

    if ((temp != 0) && (D_801153F4 == 0)) {
        work->field_02 = temp - 1;
    }
    return work->field_02;
}

void func_mist_shooting_gallery_80184B10(Task* arg0)
{
    s16 count;

    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0x28;
            arg0->spawnArg1     = 0xFF;
        case 1:
            count = --arg0->killCountdown;
            if (count <= 0) {
                Task_Kill(arg0);
                return;
            }
            if (count < 0x1F) {
                if (arg0->spawnArg1 >= 9) {
                    arg0->spawnArg1 -= 8;
                }
                func_mist_shooting_gallery_801847D4((u8)arg0->spawnArg1);
            }
            return;
    }
}

void func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2)
{
    func_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_mist_shooting_gallery_801856D0, arg2, 0, 0);
}

void func_mist_shooting_gallery_80184C0C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0x10;
        case 1:
            if (arg0->killCountdown != 0) {
                arg0->killCountdown--;
                goto call_func;
            }
        pad_check:
            asm volatile("" : : "i"(&&pad_check));
            if (Pad_CheckButtons(0, 1, arg0->spawnArg1) != 0) {
                arg0->state = arg0->state + 1;
                return;
            }
        call_func:
            func_8014B0D4();
            return;
        case 2:
            Task_Kill(arg0);
            Stage_SetEndingFlag();
            return;
    }
}

GpEnemy* func_mist_shooting_gallery_80184CD0(Task* arg0, MistShootingGallerySpawn* arg1)
{
    MistShootingGalleryWork* work;
    GpEnemy*                 enemy;
    TmdObject*               obj;
    GpCoordPose*             coord;

    work  = (MistShootingGalleryWork*)arg0->idMap;
    enemy = Gp_SpawnEnemyFromTable(&D_80134F94, 0, arg1->idLo | (arg1->idHi << 16), NULL);
    if (enemy != NULL) {
        enemy->task->parent = arg0;
        Task_Reparent(arg0, enemy->task);
        obj           = (TmdObject*)enemy->task->extra;
        obj->field_24 = 0;
        obj->field_25 = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        coord             = (GpCoordPose*)((TmdObject*)enemy->task->extra)->field_8;
        coord->coord.t[0] = arg1->x;
        coord->coord.t[1] = arg1->y;
        coord->coord.t[2] = arg1->z;
        enemy->field_A    = 0x900;
        work->field_0E++;
    }
    return enemy;
}
