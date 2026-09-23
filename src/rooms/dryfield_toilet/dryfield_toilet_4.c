#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern u8  D_801153F4;
extern s32 Gp_LcgState;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

void func_dryfield_toilet_8017DCF0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        if (D_801153F4 == 1) {
            return;
        }
        if (arg0->state == 0) {
            coord->sub        = mem->parent;
            coord->coord.t[0] = mem->pos.vx;
            coord->coord.t[1] = mem->pos.vy;
            coord->coord.t[2] = mem->pos.vz;
            coord->flg        = 0;
            arg0->state       = 1;
            mem->scale        = 0x30;
            mem->angle        = arg0->spawnArg1;
            if (((u16)mem->pos.vx | (u16)mem->pos.vy | (u16)mem->pos.vz) == 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vx = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vy = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vz = (((u32)Gp_LcgState >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS(&mem->pos, &mem->move);
        }
        Gp_UpdateCoord(coord);
        spawned = Gp_SpawnEff(0x6009F, coord, 0x11180, 0);
        if (spawned != NULL) {
            gte_lddp(mem->scale - mem->age);
            gte_ldsv(&mem->move);
            gte_gpf12_real();
            gte_stsv(&spawned->move);
        }
        mem->age++;
        if (mem->age < mem->angle) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017DEF4);

void func_dryfield_toilet_8017E64C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6028A;
        D_80115730  = 0x6028B;
        D_80115754  = 0x6028C;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_4", func_dryfield_toilet_8017E69C);
