#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b4_reservoir.h"

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80115730;
extern s32 Gp_LcgState;

/// Per-colour right shifts for the red, green and blue channels of the disc's
/// brightness, one row per spawn argument.
extern s16 D_shelter_b4_reservoir_801850BC[][3];

void func_shelter_b4_reservoir_8018351C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_reservoir_80183940(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);

void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.field_0 = arg0;
    D_shelter_b4_reservoir_80187684.field_2 = arg1;
    D_shelter_b4_reservoir_80187684.field_4 = arg2;
}

/// Effect task of a glowing disc attached to its parent at the work's
/// position. State 1 grows the disc and, every fourth tick, spawns effect
/// `D_80115730` at a random joint of the player's model; state 2 adds a
/// flickering second disc at half brightness; state 3 drifts the disc away,
/// shrinking and fading it inside an expanding ring, then releases the effect.
/// The spawn argument picks the disc's colour shifts from
/// `D_shelter_b4_reservoir_801850BC`.
void func_shelter_b4_reservoir_80182B1C(Task* arg0)
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
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_reservoir_80183940(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
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
            func_shelter_b4_reservoir_8018351C(coord, (s16)(mem->period + 0x80), 0x100, col);
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
