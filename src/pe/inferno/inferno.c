#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/inferno.h"

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// `gpf 12` / `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")

void func_inferno_8012F3EC(s16 arg0);
void func_inferno_8012F978(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map);
void func_inferno_8012FF34(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map);

/// Runs one frame of the inferno cast: a state machine driven by
/// `Task::state`, with the chain it takes chosen in state 0 from
/// `Gp_StateC08.field_0 % 10 - 1` (the combo counter), which also picks the
/// roar from `D_inferno_801304F0` and lands the task on state 1, 5 or 9.
/// State 1 spawns the two ignition effects, state 5 fans six flames around a
/// 0x400 step, state 9 the ground burst; states 10 and 11 fade the effect
/// brightness `GpEffWork::field_26` down and back up and each fire one ring of
/// flames on their own tick, and state 12 fades out and releases. Every state
/// updates the effect coordinate first, and any state releases immediately if
/// the player is dying (`D_80114C0B`) or the room is fading (`Gp_State1C`).
void func_inferno_8012EF88(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            pan;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
        goto release;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_24 = 0x200;
            mem->field_26 = 0xFF;
            pan           = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_inferno_801304F0[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            arg0->state = ((u16)(Gp_StateC08.field_0 % 10) - 1) * 4 + 1;
            return;
        case 1:
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            Gp_SpawnEff(0x800600DA, coord, 5, NULL);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x10, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 5:
            i = 0x200;
            func_inferno_8012F3EC(mem->field_26);
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            mem->field_24 = 0x600;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x400;
                Gp_SpawnEff(0x800600DA, coord, 4, (SVECTOR*)&mem->field_10);
            } while (i < 0x1200);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x14, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 9:
            Gp_SpawnEff(0x800600DA, coord, 0, NULL);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xA;
            return;
        case 10:
            func_inferno_8012F3EC(mem->field_26);
            mem->field_26 = (u16)mem->field_26 - 0x10;
            if (mem->field_22 != 0xC) {
                return;
            }
            mem->field_24 = 0x600;
            i             = 0x155;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 1, (SVECTOR*)&mem->field_10);
            } while (i < 0x1151);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xB;
            return;
        case 11:
            func_inferno_8012F3EC(mem->field_26);
            if (mem->field_26 < 0xF0) {
                mem->field_26 = (u16)mem->field_26 + 0x10;
            }
            if (mem->field_22 != 0x18) {
                return;
            }
            mem->field_24 = 0x900;
            i             = 0;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 2, (SVECTOR*)&mem->field_10);
            } while (i < 0xFFC);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x18, 0xFF, 8);
            arg0->state   = 0xC;
            mem->field_26 = 0xFF;
            return;
        case 12:
            func_inferno_8012F3EC(mem->field_26);
            if (mem->field_26 >= 9) {
                mem->field_26 = (u16)mem->field_26 - 8;
                return;
            }
            break;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012F3EC);

INCLUDE_RODATA("pe/nonmatchings/inferno/inferno", D_inferno_8012EF68);

/// Companion inferno-cast task: state 0 allocates a 12-byte `InfernoIdMap`
/// of LCG jitter, scales `GpEffWork::field_18` by 0x80 (`gte_gpf12`) and
/// rotates it into `field_10`. States 1–6 fade `field_24` while spinning
/// `field_26` / `field_28` / `field_2A` and drawing through
/// `func_inferno_8012F978` (kind 0) and `func_inferno_8012FF34` (kind 1).
/// State 3 also walks the effect coordinate by `field_10`. Releases if the
/// player is dying, the room is fading, or the state's brightness floor is
/// hit. `Task::spawnArg1 + 1` selects the chain from state 0.
void func_inferno_8012F530(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    InfernoIdMap*  map;
    s8*            p;
    s32            i;
    s32            rng;
    s32            tz;

    map   = (InfernoIdMap*)arg0->idMap;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
        goto release;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            map = Mem_Calloc(0xC, 0);
            if (map == NULL) {
                mem->field_22 = 0;
                return;
            }
            arg0->idMap   = (TaskIdMap*)map;
            mem->field_24 = 0x80;
            i             = 0;
            do {
                p           = &map->field_0[i];
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                p[0]        = (u32)rng >> 16;
                i++;
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                p[6]        = (u32)rng >> 16;
            } while (i < 6);
            arg0->state = arg0->spawnArg1 + 1;
            gte_lddp(0x80);
            gte_ldsv((SVECTOR*)&mem->field_18);
            gte_gpf12_real();
            gte_stsv((SVECTOR*)&mem->field_10);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0((SVECTOR*)&mem->field_10);
            gte_rtv0_real();
            gte_stsv((SVECTOR*)&mem->field_10);
            return;
        case 1:
            if (mem->field_24 >= 5) {
                if (mem->field_28 < 0xC00) {
                    mem->field_28 = (u16)mem->field_28 + 0xC0;
                } else {
                    mem->field_24 = (u16)mem->field_24 - 4;
                }
                mem->field_26 = (u16)mem->field_26 + 0x20;
                mem->field_2A = (u16)mem->field_2A + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 2:
            if (mem->field_24 >= 9) {
                mem->field_24 = (u16)mem->field_24 - 8;
                mem->field_26 = (u16)mem->field_26 + 0x20;
                mem->field_28 = (u16)mem->field_28 + 0xC0;
                mem->field_2A = (u16)mem->field_2A + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 3:
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            tz                 = coord->coord.t[2] + mem->field_14;
            coord->flg         = 0;
            coord->coord.t[2]  = tz;
            if (mem->field_24 >= 9) {
                mem->field_24 = (u16)mem->field_24 - 8;
                mem->field_26 = (u16)mem->field_26 + 0x20;
                mem->field_28 = (u16)mem->field_28 + 0xC0;
                mem->field_2A = (u16)mem->field_2A + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 4:
            if (mem->field_24 >= 7) {
                mem->field_24 = (u16)mem->field_24 - 6;
                mem->field_26 = (u16)mem->field_26 + 0x40;
                mem->field_28 = (u16)mem->field_28 + 0xC0;
                mem->field_2A = (u16)mem->field_2A + 0x10;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 5:
            if (mem->field_24 >= 7) {
                mem->field_24 = (u16)mem->field_24 - 6;
                mem->field_26 = (u16)mem->field_26 + 0x40;
                mem->field_28 = (u16)mem->field_28 + 0x40;
                mem->field_2A = (u16)mem->field_2A + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 6:
            if (mem->field_24 >= 7) {
                mem->field_24 = (u16)mem->field_24 - 6;
                mem->field_26 = (u16)mem->field_26 + 0x80;
                mem->field_28 = (u16)mem->field_28 + 0x20;
                mem->field_2A = (u16)mem->field_2A + 0x20;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012F978);

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012FF34);
