#include "common.h"

#include "actors/actor_103800.h"
#include "actors/actors_shared_80135b58.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/gfx.h"

void Actor03800_Fn00974(Actor103800* arg0);
void Actor03800_Fn00A98(Actor103800* arg0);
void Actor03800_Fn026F8(Actor103800* arg0);
void Actor03800_Fn02848(Actor103800* arg0);
void Actor03800_Fn02E50(Actor103800* arg0);
void Actor03800_Fn03594(Actor103800* arg0);
void Actor03800_Fn03628(Actor103800* arg0);
void Actor03800_Fn036EC(Actor103800* arg0);
void Actor03800_Fn03744(Actor103800* arg0);
void Actor03800_Fn037E0(Actor103800* arg0);
void Gp_ArmStateF0(s32 arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern u8  D_801153F2;
extern u8  D_801153F4;
extern s32 Gp_LcgState;

void              Actor03800_Fn003B8(Task* arg0);
extern GpPairSrcE Actor03800_D05F44;
extern u8         Actor03800_D05F60[];

#include <psyq/inline_c.h>
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

void Actor03800_Fn01150(Actor103800* arg0)
{
    Actor103800Work* work;
    s32              turn;

    work = arg0->field_1C;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn            = ((u32)Gp_LcgState >> 16) & 0x3FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn = -turn;
                }
                work->field_348 = 2;
                work->field_36A = 1;
                work->field_364 = (work->field_362 + turn) & 0xFFF;
            }
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                if (work->field_37A == 0) {
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x5A;
                } else {
                    work->field_352 = 1;
                    work->field_354 = 0;
                    if (work->field_36A == 0) {
                        work->field_36A = 1;
                    }
                }
            }
            break;
    }

    if (D_801153F2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
        if (work->field_36A == 0) {
            work->field_36A = 1;
        }
        work->field_37A = 1;
    }
}

void Actor03800_Fn012B4(Actor103800* arg0)
{
    Actor103800Work* work;
    s32              turn;
    s32              delta;
    s32              turn2;

    work = arg0->field_1C;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35E = 2;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            turn            = ((u32)Gp_LcgState >> 16) & 0x1FF;
            if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                turn = -turn;
            }
            delta = turn;
            if (work->field_370 != 0) {
                delta           = turn + 0x800;
                work->field_370 = 0;
            }
            work->field_348 = 2;
            work->field_354 = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            turn            = 0; /* dead store: keeps `turn` cse-canonical over `delta` */
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_358 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn2           = ((u32)Gp_LcgState >> 16) & 0x1FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn2 = -turn2;
                }
                work->field_364 = (work->field_362 + turn2) & 0xFFF;
            }
            if (work->field_370 != 0) {
                if (work->field_35C < 0x1E) {
                    work->field_352 = 0;
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
                } else {
                    work->field_352 = 0xC;
                    work->field_354 = 0;
                }
            }
            work->field_358--;
            if (work->field_358 <= 0) {
                work->field_352 = 0;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }

    if (D_801153F2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
    }
}

void Actor03800_Fn01520(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    work  = arg0->field_1C;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            vec.vx          = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_364 = ratan2((s16)vec.vx, (s16)vec.vz) & 0xFFF;
            work->field_348 = 9;
            if (work->field_36A == 0) {
                work->field_36A = 1;
            }
            work->field_354 = 1;
            break;
        case 1:
            work->field_360 = 0x28;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                work->field_354 = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x78;
            }
            break;
        case 2:
            work->field_360 = 0;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            break;
    }
}

void Actor03800_Fn0166C(Actor103800* arg0)
{
    Actor03800MoveScratch* scratch;
    Actor103800Work*       work;
    Actor103800Ctx*        ctx;
    GsCOORDINATE2*         coord;
    s16                    state;
    s32                    snd;
    s32                    pan;
    s32                    pan2;

    scratch = (Actor03800MoveScratch*)(SCRATCH_SP -= 0x18);
    work    = arg0->field_1C;
    ctx     = arg0->field_20;
    state   = work->field_354;
    coord   = work->field_344;
    switch (state) {
        case 0:
            work->field_348  = 3;
            work->field_354  = 1;
            work->field_36A  = 0;
            work->field_36E  = 1;
            work->field_2AA &= 0x7FFF;
            snd              = ((arg0->field_20->field_8 >> 12) << 8) | 0x40260003;
            pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            if ((u32)(work->field_34C - 2) < 12) {
                scratch->delta.vx = coord->coord.t[0] - Player_Status.coordMtx->t[0];
                scratch->delta.vy = coord->coord.t[1] - Player_Status.coordMtx->t[1];
                scratch->delta.vz = coord->coord.t[2] - Player_Status.coordMtx->t[2];
                VectorNormalS(&scratch->delta, &scratch->normal);
                coord->coord.t[0] += (scratch->normal.vx * 17) >> 9;
                coord->coord.t[2] += (scratch->normal.vz * 17) >> 9;
            } else {
                work->field_35C = 0;
                work->field_35E = 0;
            }
            if ((s16)work->field_34C == 12) {
                snd  = ((arg0->field_20->field_8 >> 12) << 8) | 0x40260002;
                pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_34C >= 29) {
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F48 - ctx->field_40) * 100 / Actor03800_D05F48) * 10 + 240;
            }
            break;
        case 2:
            work->field_37C--;
            if (work->field_37C <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_SP += 0x18;
}

void Actor03800_Fn01948(Actor103800* arg0)
{
    Actor103800Work* work = arg0->field_1C;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    state = work->field_354;
    coord = work->field_344;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                if (work->field_36E == 0) {
                    work->field_348 = 0xB;
                    work->field_356 = 0xC;
                } else {
                    work->field_348 = 6;
                    work->field_356 = 0x13;
                }
            } else {
                work->field_348 = 7;
                work->field_356 = 0;
                work->field_352 = 0xA;
            }
            work->field_354 = 1;
            work->field_34A = 1;
            work->field_36A = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_360 = 0;
            snd             = ((arg0->field_20->field_8 >> 12) << 8) | 0x40260003;
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            work->field_356 -= 1;
            if (work->field_356 > 0) {
                break;
            }
            if (work->field_36E == 0) {
                if (work->field_37E == 0) {
                    work->field_352 = 2;
                    work->field_354 = 0;
                } else {
                    work->field_352 = 6;
                    work->field_354 = 0;
                    work->field_356 = 0;
                }
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
            }
            break;
    }
}

void Actor03800_Fn01AD0(Actor103800* arg0)
{
    Actor103800Ctx*  ctx;
    Actor103800Work* work;

    work            = arg0->field_1C;
    ctx             = arg0->field_20;
    work->field_360 = 0;
    work->field_35C = 0;
    work->field_35E = 0;
    work->field_356--;
    if (work->field_356 <= 0) {
        if (work->field_36E == 0) {
            work->field_348 = 0xB;
        } else {
            work->field_348 = 6;
        }
        work->field_34A = 1;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_356 = (((u32)Gp_LcgState >> 16) & 7) + 3;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
        work->field_37E = 0;
        if (work->field_350 == 0) {
            if (work->field_36E == 0) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F48 - ctx->field_40) * 100 / Actor03800_D05F48) * 10 + 240;
            }
        } else {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
}

void Actor03800_Fn01C50(Actor103800* arg0)
{
    SVECTOR          rotation;
    MATRIX           matrix;
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_354;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                arg0->field_30  = 2;
                work->field_354 = 0;
                return;
            }
            work->field_366  = 0x80;
            work->field_372  = 0x80;
            work->field_354  = 1;
            work->field_22A |= 0x4200;
            return;
        case 1:
            if (work->field_374 != 0) {
                work->field_354 = 2;
                return;
            }
        default:
            return;
        case 2:
            work->field_348 = 5;
            work->field_354 = 3;
            return;
        case 3:
            rotation.vx = 0;
            rotation.vy = (u16)work->field_362;
            rotation.vz = 0;
            RotMatrix(&rotation, &matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&matrix.m[0][0]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&matrix.m[0][1]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&matrix.m[0][2]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->sub        = &gGfxViewCoord;
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_356 = 0xF;
            work->field_344 = coord;
            work->field_354 = 4;
            return;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_378 = 1;
                work->field_354 = 0;
                arg0->field_30  = 2;
            }
            break;
    }
}
