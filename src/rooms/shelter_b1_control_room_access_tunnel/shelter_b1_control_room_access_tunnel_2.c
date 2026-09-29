#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
extern RoomHaloShade D_shelter_b1_control_room_access_tunnel_80181EF4[];

static void func_shelter_b1_control_room_access_tunnel_801809E8(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b1_control_room_access_tunnel_80180C6C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b1_control_room_access_tunnel_80181090(GpCoord* arg0, s32 arg1, u8* rgb);
static void func_shelter_b1_control_room_access_tunnel_801815D0(GpCoord* coord, s16 size);
static void func_shelter_b1_control_room_access_tunnel_80181AFC(GpCoord* arg0, s32 arg1);

RoomHaloShade D_shelter_b1_control_room_access_tunnel_80181EF4[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

u8* D_shelter_b1_control_room_access_tunnel_80181F00[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_control_room_access_tunnel_80181F04[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_b1_control_room_access_tunnel_80181F08[2] = {
    { { .words = { 3072, 5389, -25, 194 } }, { 0, 0, 0, 0 }, { .words = { 3072, 5389, -25, 194 } }, { 0, 0, 0, 0 }, 0x54190002, 0x54190001, 0, 2, 0, 0 },
    { { .words = { 1024, 831, -24, 119 } }, { 0, 0, 0, 0 }, { .words = { 1024, 831, -24, 119 } }, { 0, 0, 0, 0 }, 0x54190004, 0x54190003, 0, 3, 0, 0 },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181F78[6] = {
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 4096, 0, 0 },
    { 0, -4096, 0, 0 },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181FA8[12] = {
    { 361, -2305, 949, 0 },
    { 361, -25, 949, 0 },
    { 361, -25, -797, 0 },
    { 361, -2305, -797, 0 },
    { 6101, -25, -797, 0 },
    { 6101, -2305, -797, 0 },
    { 6101, -25, 949, 0 },
    { 6101, -2305, 949, 0 },
    { 106, -25, 1543, 0 },
    { 6355, -25, 1543, 0 },
    { 6355, -25, -1242, 0 },
    { 106, -25, -1242, 0 },
};

GpGridFace D_shelter_b1_control_room_access_tunnel_80182008[6] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
    { { 0, 3, 7, 5 }, 4, 0 },
    { { 9, 10, 8, 11 }, 5, 1 },
};

s16 D_shelter_b1_control_room_access_tunnel_80182050[6] = {
    0,
    1,
    3,
    4,
    5,
    -1,
};

s16 D_shelter_b1_control_room_access_tunnel_8018205C[6] = {
    1,
    2,
    3,
    4,
    5,
    -1,
};

s16* D_shelter_b1_control_room_access_tunnel_80182068[2] = {
    D_shelter_b1_control_room_access_tunnel_80182050,
    D_shelter_b1_control_room_access_tunnel_8018205C,
};

GpGridParams D_shelter_b1_control_room_access_tunnel_80182070 = { NULL, D_shelter_b1_control_room_access_tunnel_80181F78, D_shelter_b1_control_room_access_tunnel_80181FA8, D_shelter_b1_control_room_access_tunnel_80182008, D_shelter_b1_control_room_access_tunnel_80182068, -106, 1242, 2, 1, 4000, 6 };

GpViewRec D_shelter_b1_control_room_access_tunnel_80182094[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3648, 8565, 0 } }, 235 },
    { { { { -16, 0, -4095 }, { -708, 4034, 2 }, { 4034, 708, -16 } }, { -837, 1954, -102 } }, 257 },
    { { { { 32, 0, 4095 }, { 347, 4081, -2 }, { -4081, 347, 32 } }, { -6111, 1435, -85 } }, 246 },
};

GpSprtCmd D_shelter_b1_control_room_access_tunnel_80182100[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_control_room_access_tunnel_80182110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_control_room_access_tunnel_80182120[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_control_room_access_tunnel_80182130[3] = {
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182100 }, D_shelter_b1_control_room_access_tunnel_80182100, NULL },
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182110 }, D_shelter_b1_control_room_access_tunnel_80182110, NULL },
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182120 }, D_shelter_b1_control_room_access_tunnel_80182120, NULL },
};

GpPointLight D_shelter_b1_control_room_access_tunnel_80182154[4] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4469, -223, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2498, 2539, 2560, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1886, -223, 190 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2498, 2539, 2560, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1923, -223, 280 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2498, 2539, 2560, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3158, -223, 433 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2498, 2539, 2560, { 0, 0 } }, 1500, 2000 },
};

GpRoomCoordSet D_shelter_b1_control_room_access_tunnel_801822D4 = { 0, NULL, 4, D_shelter_b1_control_room_access_tunnel_80182154, 0, NULL };

GpObj4C D_shelter_b1_control_room_access_tunnel_801822EC[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 2, 3, 129, 0 },
};

GpObj4C D_shelter_b1_control_room_access_tunnel_80182384[2] = {
    { NULL, NULL, NULL, { 5568, -48, 64, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, 0, 15, 18, 2, 0 },
    { NULL, NULL, NULL, { 800, -48, 64, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1144, 0, 18, 33, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_8018241C[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 24, 24, 1, 0, { 0, 0 }, D_8014E47C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_80182440[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_80182464[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_8018247C[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 20, 20, 1, 0, { 0, 0 }, D_8015FDF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_801824A0[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b1_control_room_access_tunnel_801824AC[5] = {
    { 21, 2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, 2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 24, 0, 0, 2650, 0, -350, 700, 0, 2, 4, 0 },
    { 24, 0, 0, 2250, 0, 650, 1300, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_access_tunnel_801824FC[4] = {
    { 21, 2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, 2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 11, 0, 0, 2600, 0, 0, 3072, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_access_tunnel_8018253C[2] = {
    { 11, 0, 0, 1500, 0, 140, 0, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_access_tunnel_8018255C[4] = {
    { 21, 3, 0, 400, -2000, 800, 1024, 0, 0, 2, 4 },
    { 21, 3, 0, 400, -2000, -650, 1024, 0, 0, 2, 4 },
    { 20, 4, 1, 1500, 0, 0, 1024, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_access_tunnel_8018259C[1] = {
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_control_room_access_tunnel_801825AC[22] = {
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_801824AC, D_shelter_b1_control_room_access_tunnel_8018241C },
    { D_shelter_b1_control_room_access_tunnel_801824FC, D_shelter_b1_control_room_access_tunnel_80182440 },
    { D_shelter_b1_control_room_access_tunnel_8018253C, D_shelter_b1_control_room_access_tunnel_80182464 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_8018255C, D_shelter_b1_control_room_access_tunnel_8018247C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_8018259C, D_shelter_b1_control_room_access_tunnel_801824A0 },
};

s32 D_shelter_b1_control_room_access_tunnel_8018265C[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b1_control_room_access_tunnel_80182668[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_control_room_access_tunnel_80182670[1] = {
    { 0, 0, 1, 0, D_shelter_b1_control_room_access_tunnel_8018265C },
};

GpRoomParamRec* D_shelter_b1_control_room_access_tunnel_80182678[8] = {
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182670,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
};

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
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
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
            coord->sub                       = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->flg                       = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
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
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].b;
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
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].b;
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
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1.value].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
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

/// Moves the effect's coordinate toward the coordinate passed as the spawn
/// argument. State 0 turns their world-space displacement into the effect's
/// frame and scales it by 0xCC / 0x1000; state 1 adds that step each tick and
/// draws the sprite on odd ticks, advancing its animation cell. The work block
/// is released at tick 20, or once the room's event state reaches 4.
void func_shelter_b1_control_room_access_tunnel_801807C4(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    GpCoord*   target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.tmd->coords;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b1_control_room_access_tunnel_801809E8(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_shelter_b1_control_room_access_tunnel_801809E8(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// The same ring as `func_shelter_b1_control_room_access_tunnel_8017E57C`,
/// over a scratch block laid out differently: black at radius
/// `arg1 * 64 / (otz + 1)`, shading to `rgb` at radius
/// `(arg1 + arg2) * 64 / (otz + 1)`, drawn unless the GTE flags an error.
static void func_shelter_b1_control_room_access_tunnel_80180C6C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// The same disc as `func_shelter_b1_control_room_access_tunnel_8017E9A8`,
/// over a scratch block laid out differently: unless the GTE flags an error,
/// eight gouraud `POLY_G4` wedges of radius `arg1 * 64 / (otz + 1)`, coloured
/// `rgb` at the centre and black at the rim.
static void func_shelter_b1_control_room_access_tunnel_80181090(GpCoord* arg0, s32 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Flare at the task's coordinate: each tick grows a size by 0x10 and draws a
/// disc of twice that radius and the glow at that size. While the ring's level
/// is above 0x18 it also draws a ring that widens as it fades; after that the
/// disc dims by 0x18 a tick and the work block is released once it drops below
/// 0x18, or once the room's event state reaches 4.
void func_shelter_b1_control_room_access_tunnel_80181424(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         sp10[3];
    u16        temp;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age    = 1;
            work->scale  = 0xE0;
            work->angle  = 0x80;
            work->period = 0xE0;
            work->step   = 0x80;
            task->state  = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]     = (u8)work->scale;
        sp10[1]     = (u8)(work->scale >> 1);
        sp10[2]     = (u8)(work->scale >> 2);
        temp        = work->angle + 0x10;
        work->angle = temp;
        func_shelter_b1_control_room_access_tunnel_80181090(coord, (s16)(temp * 2), sp10);
        func_shelter_b1_control_room_access_tunnel_801815D0(coord, work->angle);
        if (work->period >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->period;
            sp10[1] = (u8)(work->period >> 1);
            sp10[2] = (u8)(work->period >> 2);
            temp_a1 = work->step * 3;
            func_shelter_b1_control_room_access_tunnel_80180C6C(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->period -= 0x18;
            work->step   += 0x30;
            return;
        }
        temp        = work->scale - 0x18;
        work->scale = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_shelter_b1_control_room_access_tunnel_801815D0(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = shifted >> 0x11;
    light->head.b               = shifted >> 0x12;
    light->head.u.at.local.t[0] = coord->coord.t[0];
    light->head.u.at.local.t[1] = coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    Gp_LcgState                 = random;
    block                       = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx               = coord->workm.t[0];
    block->vec.vy               = coord->workm.t[1];
    block->vec.vz               = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b1_control_room_access_tunnel_80181AFC(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b1_control_room_access_tunnel_80181AFC(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
