#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")

extern u8  D_801153F4;
extern s32 Gp_LcgState;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;
extern s16 D_dryfield_toilet_80181120[][3];

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

void func_dryfield_toilet_8017E69C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            Room_Draw10(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            Room_Draw10(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                Room_Draw10(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_dryfield_toilet_80181120[arg0->spawnArg1][2];
            Room_Draw10(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            Room_Draw07(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}
