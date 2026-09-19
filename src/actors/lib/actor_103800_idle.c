#include "common.h"

#include "actors/actor_103800.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include <psyq/inline_c.h>

extern u8         D_801153F2;
extern s32        Gp_LcgState;
extern GpPairSrcE Actor03800_D05F44;

void Gp_UpdateCoord(GsCOORDINATE2* coord);
s32  SndEvt_EnqueueType6(s32 id, s32 pan, s32 depth);

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// Second copy of the idle "look around" tick; identical body to
/// `Actor03800_Fn02068`, which the overlay carries twice.
void Actor03800_Fn01EEC(Actor103800* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->field_1C;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((D_801153F2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

/// Idle "look around" tick. State 0 counts `field_356` down and, on expiry,
/// picks a new facing `field_364` within +/-0x3FF of the current one; state 1
/// waits for the turn to finish and re-arms the countdown. Either way, an
/// active `D_801153F2` bit (1 or 4) or a non-zero `field_36C` aborts
/// back to state 0 with a short delay.
void Actor03800_Fn02068(Actor103800* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->field_1C;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((D_801153F2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

void Actor03800_Fn021E4(Actor103800* arg0)
{
    Actor103800Ctx*        ctx;
    Actor103800Work*       work;
    GsCOORDINATE2*         coord;
    Actor03800TurnScratch* scratch;
    s32                    sound;
    s32                    pan;

    scratch = (Actor03800TurnScratch*)(*(u32*)0x1F8003FC -= sizeof(*scratch));
    SCHED_BARRIER();
    work  = arg0->field_1C;
    ctx   = arg0->field_20;
    coord = arg0->field_2C->field_8;
    switch (work->field_354) {
        case 0:
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
            }
            break;
        case 1:
            work->field_366  = 0x100;
            work->field_372  = 0x80;
            work->field_22A |= 0x4200;
            if (work->field_374 != 0) {
                work->field_374  = 0;
                work->field_354  = 2;
                work->field_366  = 0x80;
                work->field_2AA &= 0x7FFF;
                sound            = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40260002;
                pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 2:
            work->field_348 = 8;
            work->field_354 = 3;
            break;
        case 3:
            scratch->rotation.vx = 0;
            scratch->rotation.vy = (u16)work->field_362;
            scratch->rotation.vz = 0;
            RotMatrix(&scratch->rotation, &scratch->matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&scratch->matrix.m[0][0]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&scratch->matrix.m[0][1]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&scratch->matrix.m[0][2]);
            gte_rtir_real();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_354 = 4;
            work->field_344 = coord;
            work->field_37C = ((Actor03800_D05F44.field_4 - ctx->field_40) * 100 / Actor03800_D05F44.field_4) * 10 + 240;
            break;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    *(u32*)0x1F8003FC += 0x28;
}
