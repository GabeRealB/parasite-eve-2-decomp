#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include "rooms/shelter_b1_control_room_access_tunnel.h"

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80115730;
extern s32 Gp_LcgState;

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
extern RoomsShared8017e4f8Shade D_shelter_b1_control_room_access_tunnel_80181EF4[];

/// Glowing disc anchored to its parent at the work block's position. State 1
/// grows the disc and, every fourth tick, spawns the effect `D_80115730` names
/// at a random joint of the player's model and adopts it as a child task;
/// state 2 keeps growing it and adds a half-bright second disc on odd ticks;
/// state 3 drifts the disc away while it fades inside an expanding ring, then
/// releases the work block, as does state 4. The spawn argument picks the
/// disc's colour. Nothing runs while the room's event state is set, and the
/// block is released once that state reaches 4.
void func_shelter_b1_control_room_access_tunnel_8018026C(Task* arg0)
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
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b1_control_room_access_tunnel_80181090(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
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
            func_shelter_b1_control_room_access_tunnel_80180C6C(coord, (s16)(mem->period + 0x80), 0x100, col);
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
