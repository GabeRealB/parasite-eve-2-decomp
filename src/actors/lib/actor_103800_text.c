#include "common.h"

#include "actors/actor_103800.h"
#include "gameplay/3CD8.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void Gp_UnlinkNode(void* node);
void Gp_UnlinkObj(void* node);
void Gp_SetLightMode(void* arg0, s32 arg1);
void Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void Gp_DestroyEnemy(void* enemy, void* task);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void Gp_AnimTickIndex(void* arg0, s32 arg1);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern u8  D_801153F2;
extern u8  D_801153F4;
extern s32 Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn000B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0010C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L001CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00300);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0031C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00320);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00330);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00398);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn003B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00434);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00450);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00938);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00944);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0095C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn00974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L009D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L009F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A58);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn00A98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B2C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00DCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EC0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00FF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01030);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01034);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0109C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01150);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01178);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L011D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L011F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01274);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01278);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L012A4);

void Actor03800_L012AC(void)
{
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
            vec.vx          = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn0166C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L016D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L016E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01754);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01800);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01808);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0185C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01910);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01918);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01948);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01980);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01AB0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01ABC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01AD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01B24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01B60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01BB4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01BF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01C50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01D08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01EA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01ED4);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn021E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02258);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02280);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02324);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L024EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02504);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02524);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0254C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02550);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02584);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L025B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L025D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0261C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02654);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02664);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02684);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026F0);

void Actor03800_Fn026F8(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)(SCRATCH_SP -= 8);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_364;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_362 = ang;
    if (adiff < 0x800) {
        step = work->field_360;
        if (step >= adiff) {
            work->field_362 = want;
        } else {
            next = work->field_362;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_362 = next;
        }
    } else {
        step = work->field_360;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_362 = work->field_364;
        goto done;
    turn:
        wrapStep = work->field_360;
        cur      = work->field_362;
        if (diff > 0) {
            work->field_362 = cur - wrapStep;
        } else {
            work->field_362 = cur + wrapStep;
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_362;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_SP += 8;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02848);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0288C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L028E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L028E8);

void Actor03800_Fn02998(Actor103800Ctx* arg0, Actor103800* arg1)
{
    Actor103800Work*  work;
    Actor103800Obj2C* obj;
    GsCOORDINATE2*    coord;
    Actor103800Work*  work2;
    GsCOORDINATE2*    c;
    VECTOR            vec;
    s32               state;
    s32               i;
    s16               st;
    s16               phase;
    s16               anim;
    s16               val;
    s32               snd;
    s32               pan;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    state = D_801153F4;
    coord = work->field_344;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C = 0x80;
    return;
default_body:
    st = work->field_354;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    if (st == 3) {
        goto case3;
    }
    return;
death:
    if (work->field_378 == 0) {
        anim = 1;
        if (work->field_36E != 0) {
            anim = 5;
        }
        work->field_348 = anim;
    }
    work->field_356 = 0;
    work->field_35A = 0x1000;
    work->field_2CC = coord->coord;
    arg0->field_54  = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(work->field_1A4);
    Gp_UnlinkObj(work->field_20C);
    Gp_UnlinkObj(work->field_28C);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 0x26);
    work->field_354 = 1;
    if (work->field_368 != 0) {
        obj->field_C    = 0x80;
        work->field_354 = 3;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 6);
    }
    c      = arg1->field_1C->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    snd = ((arg0->field_8 >> 12) << 8) | 0x40260004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    return;
dying:
    Actor03800_Fn037E0(arg1);
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if (work->field_356 == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
    }
    if (work->field_356 >= 0x3C) {
        work->field_354 = 2;
        obj->field_C    = 0x80;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 6);
    }
    c      = arg1->field_1C->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
case3:
    if (work->field_368 == 0) {
        goto timer;
    }
    if (work->field_368 < 2) {
        goto inc368;
    }
    work->field_368 = 0;
    Tmd_FreeBuffers((TmdObject*)obj);
    obj->field_C |= 4;
    Actor03800_Fn02E50(arg1);
    goto timer;
inc368:
    work->field_368++;
timer:
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_354 = 2;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02E50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02EFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F68);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02FE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03008);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03044);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03054);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03064);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03074);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03084);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03090);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03094);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03148);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn0315C);

void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = D_801153F4;
    one   = 1;
    work  = arg1->field_1C;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg1->field_2C->field_C = 0;
    arg0->node.field_4      = 0;
    goto default_body;
case2:
    arg1->field_2C->field_C = 0x80;
    arg0->node.field_4      = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->flg = 0;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

void Actor03800_Fn01150(Actor103800* arg0);
void Actor03800_Fn012B4(Actor103800* arg0);
void Actor03800_Fn0166C(Actor103800* arg0);
void Actor03800_Fn03420(Actor103800* arg0);
void Actor03800_Fn01948(Actor103800* arg0);
void Actor03800_Fn01AD0(Actor103800* arg0);
void Actor03800_Fn01C50(Actor103800* arg0);
void Actor03800_Fn021E4(Actor103800* arg0);
void Actor03800_Fn034B0(Actor103800* arg0);
void Actor03800_Fn02584(Actor103800* arg0);

void Actor03800_Fn032D8(Actor103800* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->field_1C;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03420);

void Actor03800_L03440(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03448);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0345C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03470);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03498);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn034B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L034F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03508);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03514);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03560);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03584);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03594);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03614);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03628);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0367C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn036EC);

void Actor03800_Fn03744(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->field_1C;
    coord = work->field_344;
    if (work->field_350 == 0) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x1F4, work->field_372);
        return;
    }
    hit = func_800EA1A8((VECTOR3*)coord->workm.t, &vec);
    if (hit != 0) {
        Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn037E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0382C);
