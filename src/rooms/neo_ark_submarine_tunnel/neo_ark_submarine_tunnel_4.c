#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;
extern s32 Gp_LcgState;

/// Right shifts applied to the red, green and blue channels of the burst's
/// level, one row per spawn argument.
extern s16 D_neo_ark_submarine_tunnel_80181DF4[][3];

void func_neo_ark_submarine_tunnel_8017FEDC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_submarine_tunnel_80180300(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);

void func_neo_ark_submarine_tunnel_8017F48C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6027F;
        D_80115730  = 0x60280;
        D_80115754  = 0x60281;
        arg0->state = 1;
    }
}

/// A glowing burst effect task riding a frame of its spawner. The first tick
/// attaches its frame to the work block's parent at the stored position. While
/// it grows, every fourth tick spawns an effect from `D_80115730` that flies
/// from a random frame of the player's model to this one, and the disc of
/// `func_neo_ark_submarine_tunnel_80180300` is drawn at a growing size, tinted
/// by the per-colour shifts of row `spawnArg1` of
/// `D_neo_ark_submarine_tunnel_80181DF4`; a later state adds a dimmer second
/// disc every other tick, and the last one moves the frame by a step fixed
/// in its own axes while a widening ring fades out, then releases the work block. It pauses while the
/// room's event state is set and releases the block when that state reaches 4.
void func_neo_ark_submarine_tunnel_8017F4DC(Task* arg0)
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
            col[0] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][2];
            func_neo_ark_submarine_tunnel_80180300(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][2];
            func_neo_ark_submarine_tunnel_80180300(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_neo_ark_submarine_tunnel_80180300(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_submarine_tunnel_80181DF4[arg0->spawnArg1][2];
            func_neo_ark_submarine_tunnel_80180300(coord, mem->angle, col);
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
            func_neo_ark_submarine_tunnel_8017FEDC(coord, (s16)(mem->period + 0x80), 0x100, col);
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
