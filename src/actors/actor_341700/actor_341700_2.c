#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"

/// Values of `_Actor341700PropWork::state`.
enum {
    ACTOR_341700_PROP_STATE_HIDDEN = 0, // not drawn; entered by every dumping-hole command but 1
    ACTOR_341700_PROP_STATE_SHOWN  = 1, // drawn where it stands; the state the spawn leaves it in
    ACTOR_341700_PROP_STATE_RISE   = 2  // dropped 1800 below its origin and shaken back up, shedding the room's rising sprites; entered by dumping-hole command 1
};

/// Work block of the package's scripted prop, the model it carries beside its
/// Mad Chasers for the Shelter B3 dumping hole's event.
///
/// The prop's spawn handler allocates it zeroed and keeps it at `Task::work`;
/// its size is that allocation's. The prop has no hit points and can never be
/// locked onto. It only stands hidden, stands shown, or rises shaking from
/// below its origin, as the room's actor commands select.
typedef struct {
    s16    state;            // `ACTOR_341700_PROP_STATE_*`
    s16    prevState;        // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16    stateEntered;     // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16    stateFrame;       // ticks `ACTOR_341700_PROP_STATE_RISE` has run, counted from 1; times its shake phases and sprite bursts
    byte   pad_8[0x10];      // never accessed
    u8     lastCommandStage; // stage tag of the last actor command received, whatever its namespace; never read
    u8     lastCommandArea;  // area tag of that command; never read
    u8     lastCommand;      // low byte of that command's selector; never read
    byte   pad_1B[0x1];      // never accessed
    MATRIX lightMtx;         // storage for the model's `TmdObject::lightMtx`
    MATRIX colorMtx;         // storage for the model's `TmdObject::colorMtx`
    byte   pad_5C[0x24];     // never accessed
} _Actor341700PropWork;
STATIC_ASSERT_SIZEOF(_Actor341700PropWork, 0x80);

/// Whole-unit part of the last movement step `func_actor_341700_8016B804`
/// applied, rounded away from zero when the step had a fraction.
static SVECTOR ActorContact_ScratchPosition;

extern SVECTOR D_actor_341700_80175F7C[];
extern u8      D_actor_341700_801760FC[150][4];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_341700_80175F5C[4]; // stored into `Task::msgTable` by func_actor_341700_8016D130

/// Psy-Q `RotMatrixY`.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_341700_8016D130(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016D2B8(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016D2E8(Enemy* arg0, Task* arg1);

static TmdSource _gActor341700Model13558;
static void      func_actor_341700_8016D32C(Task*);

s32 func_actor_341700_8016CE28(Task*, s32, s32, s32);
s32 func_actor_341700_8016CEB4(Task* task, s32 msgId, ActorCommand* cmd, s32 arg3);

#include "../../shared/actor_contacts.h"

static TmdBone _gActor341700Model13558Skeleton[11] = {
#include "assets/actor_341700_model_13558_skeleton.inc"
};

static u32 _gActor341700Model13558PartVerts[11] = {
#include "assets/actor_341700_model_13558_partVerts.inc"
};

static SVECTOR _gActor341700Model13558Verts[81] = {
#include "assets/actor_341700_model_13558_verts.inc"
};

static SVECTOR _gActor341700Model13558Normals[46] = {
#include "assets/actor_341700_model_13558_normals.inc"
};

static u32 _gActor341700Model13558Stream[753] = {
#include "assets/actor_341700_model_13558_stream.inc"
};

static TmdSource _gActor341700Model13558 = {
    0,
    3184,
    1720,
    11,
    _gActor341700Model13558PartVerts,
    _gActor341700Model13558Verts,
    _gActor341700Model13558Normals,
    _gActor341700Model13558Skeleton,
    _gActor341700Model13558Stream,
};

TaskMessageEntry D_actor_341700_80175F5C[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_341700_8016CE28 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_341700_8016CEB4 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_341700_80175F7C[48] = {
    { 0, 0, 0, 0 },
    { 0, -2500, 3000, 0 },
    { 200, -2400, 2500, 0 },
    { 0, -2500, 2000, 0 },
    { 200, -2400, 1500, 0 },
    { 0, -2500, 1000, 0 },
    { 200, -2400, 500, 0 },
    { 0, -2500, 0, 0 },
    { 200, -2400, -500, 0 },
    { 0, -2500, -1000, 0 },
    { 200, -2400, -1500, 0 },
    { 0, -2500, -2000, 0 },
    { 200, -2400, -2500, 0 },
    { 0, -2500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1500, -1500, 3000, 0 },
    { 1700, -1100, 2500, 0 },
    { 1500, -1500, 2000, 0 },
    { 1700, -1100, 1500, 0 },
    { 1500, -1500, 1000, 0 },
    { 1700, -1100, 500, 0 },
    { 1500, -1500, 0, 0 },
    { 1700, -1100, -500, 0 },
    { 1500, -1500, -1000, 0 },
    { 1700, -1100, -1500, 0 },
    { 1500, -1500, -2000, 0 },
    { 1700, -1100, -2500, 0 },
    { 1500, -1500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2500, -500, 3000, 0 },
    { 2700, -400, 2500, 0 },
    { 2500, -500, 2000, 0 },
    { 2700, -400, 1500, 0 },
    { 2500, -500, 1000, 0 },
    { 2700, -400, 500, 0 },
    { 2500, -500, 0, 0 },
    { 2700, -400, -500, 0 },
    { 2500, -500, -1000, 0 },
    { 2700, -400, -1500, 0 },
    { 2500, -500, -2000, 0 },
    { 2700, -400, -2500, 0 },
    { 2500, -500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

u8 D_actor_341700_801760FC[150][4] = {
    { 2, 0, 0, 0 },
    { 19, 5, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 20, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 33, 0, 0, 0 },
    { 19, 0, 0, 0 },
    { 36, 0, 0, 0 },
    { 21, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 27, 9, 0, 0 },
    { 45, 0, 0, 0 },
    { 44, 0, 0, 0 },
    { 19, 0, 0, 0 },
    { 33, 0, 0, 0 },
    { 27, 0, 0, 0 },
    { 39, 0, 0, 0 },
    { 21, 0, 0, 0 },
    { 45, 0, 0, 0 },
    { 21, 0, 0, 0 },
    { 8, 0, 0, 0 },
    { 2, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 20, 0, 0, 0 },
    { 41, 25, 0, 0 },
    { 22, 0, 0, 0 },
    { 27, 9, 0, 0 },
    { 34, 0, 0, 0 },
    { 42, 0, 0, 0 },
    { 20, 0, 0, 0 },
    { 37, 0, 0, 0 },
    { 27, 0, 0, 0 },
    { 41, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 33, 0, 0, 0 },
    { 37, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 34, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 18, 0, 0, 0 },
    { 35, 41, 0, 0 },
    { 26, 9, 0, 0 },
    { 38, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 20, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 28, 0, 0, 0 },
    { 35, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 42, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 2, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 36, 0, 0, 0 },
    { 38, 0, 0, 0 },
    { 27, 9, 0, 0 },
    { 34, 0, 0, 0 },
    { 42, 45, 0, 0 },
    { 36, 0, 0, 0 },
    { 37, 40, 0, 0 },
    { 27, 42, 0, 0 },
    { 41, 0, 0, 0 },
    { 22, 39, 0, 0 },
    { 33, 0, 0, 0 },
    { 21, 33, 0, 0 },
    { 8, 40, 0, 0 },
    { 2, 45, 0, 0 },
    { 3, 7, 0, 0 },
    { 18, 0, 0, 0 },
    { 35, 25, 0, 0 },
    { 26, 9, 0, 0 },
    { 38, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 20, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 28, 0, 0, 0 },
    { 35, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 42, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 8, 0, 0, 0 },
    { 2, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 20, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 27, 9, 0, 0 },
    { 34, 0, 0, 0 },
    { 42, 0, 0, 0 },
    { 20, 0, 0, 0 },
    { 37, 0, 0, 0 },
    { 27, 0, 0, 0 },
    { 41, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 33, 0, 0, 0 },
    { 21, 0, 0, 0 },
    { 8, 0, 0, 0 },
    { 34, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 50, 0, 0, 0 },
    { 35, 25, 0, 0 },
    { 58, 9, 0, 0 },
    { 38, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 52, 0, 0, 0 },
    { 40, 0, 0, 0 },
    { 28, 0, 0, 0 },
    { 35, 0, 0, 0 },
    { 38, 42, 0, 0 },
    { 42, 0, 0, 0 },
    { 23, 45, 0, 0 },
    { 40, 36, 0, 0 },
    { 34, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 20, 40, 0, 0 },
    { 38, 41, 0, 0 },
    { 43, 9, 0, 0 },
    { 34, 43, 0, 0 },
    { 42, 0, 0, 0 },
    { 20, 45, 0, 0 },
    { 37, 0, 0, 0 },
    { 27, 45, 0, 0 },
    { 41, 33, 0, 0 },
    { 22, 0, 0, 0 },
    { 33, 0, 0, 0 },
    { 21, 36, 0, 0 },
    { 8, 0, 0, 0 },
    { 34, 0, 0, 0 },
    { 3, 7, 0, 0 },
    { 18, 43, 0, 0 },
    { 35, 25, 0, 0 },
    { 26, 42, 0, 0 },
    { 38, 0, 0, 0 },
    { 39, 0, 0, 0 },
    { 36, 45, 0, 0 },
    { 40, 45, 0, 0 },
    { 44, 0, 0, 0 },
    { 35, 0, 0, 0 },
    { 22, 0, 0, 0 },
    { 42, 0, 0, 0 },
    { 23, 0, 0, 0 },
    { 8, 0, 0, 0 },
    { 2, 0, 0, 0 },
    { 35, 39, 0, 0 },
    { 20, 0, 0, 0 },
};

TaskDesc D_actor_341700_80176354 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_341700_8016D32C, { .model = &_gActor341700Model13558 } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_actor_341700_8016C0F4(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016CC9C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

static void func_actor_341700_8016C0F4(Enemy* arg0, Task* arg1)
{
    SVECTOR               vec;
    _Actor341700PropWork* work = arg1->work;
    s16                   i;
    s16                   r;
    u32                   t;
    u32                   rnd;

    if (work->stateEntered != 0) {
        work->stateFrame                    = 0;
        arg1->extra.tmd->coords->coord.t[1] = 0x708;
    }
    work->stateFrame++;
    if (arg1->extra.tmd->coords->coord.t[1] > 200) {
        if (work->stateFrame >= 0xA6) {
            if (work->stateFrame % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 90;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 100;
            }
            if (work->stateFrame % 6 < 3) {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, 12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, 24, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, -24, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, 12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, -6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, 18, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, -6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, -3, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, -27, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, -12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, -24, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, 24, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, -12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, 6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, -18, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, 6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, 3, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, 27, GRAPHICS_ROTATION_COMPOSE);
            }
        } else if (work->stateFrame >= 0x79) {
            if (work->stateFrame % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 50;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 58;
            }
            if (work->stateFrame % 6 < 3) {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, 8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, 16, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, -16, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, 8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, -4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, 12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, -4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, -2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, -18, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, -8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, -16, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, 16, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, -8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, 4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, -12, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, 4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, 2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, 18, GRAPHICS_ROTATION_COMPOSE);
            }
        } else if (work->stateFrame >= 11 && work->stateFrame < 18) {
            if (work->stateFrame % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 30;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 33;
            }
            if (work->stateFrame % 6 < 3) {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, 4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, 8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, -8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, 4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, -2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, 6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, -2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, -1, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, -9, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, -4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, -8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, 8, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, -4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, 2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, -6, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, 2, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, 1, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, 9, GRAPHICS_ROTATION_COMPOSE);
            }
            switch ((work->stateFrame - 11) % 8) {
                case 0:
                    vec = D_actor_341700_80175F7C[35];
                    shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
                    break;
                case 1:
                    vec = D_actor_341700_80175F7C[8];
                    shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
                    break;
                case 2:
                    vec = D_actor_341700_80175F7C[25];
                    shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
                    break;
                case 4:
                    vec = D_actor_341700_80175F7C[24];
                    shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
                    break;
                case 5:
                    vec = D_actor_341700_80175F7C[26];
                    shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
                    break;
            }
        }
    } else {
        if (work->stateFrame % 4 < 2) {
            arg1->extra.tmd->coords->coord.t[1] += 100;
        } else {
            arg1->extra.tmd->coords->coord.t[1] -= 100;
        }
        if (work->stateFrame % 6 < 3) {
            gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, 12, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, 24, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, -24, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, 12, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, -6, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, 18, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, -6, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, -3, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, -27, GRAPHICS_ROTATION_COMPOSE);
        } else {
            gfxRotMatrixX(&arg1->extra.tmd->coords[1].coord, -12, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[2].coord, -24, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[3].coord, 24, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[4].coord, -12, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[5].coord, 6, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[6].coord, -18, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[8].coord, 6, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[9].coord, 3, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&arg1->extra.tmd->coords[10].coord, 27, GRAPHICS_ROTATION_COMPOSE);
        }
    }
    arg1->extra.tmd->coords[0].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[2].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[3].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[4].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[5].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[6].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[7].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[8].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[9].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    i                                        = 0;
    for (; i < 4; i++) {
        if (work->stateFrame > 240) {
            return;
        }
        if (work->stateFrame < 120) {
            return;
        }
        if (D_actor_341700_801760FC[work->stateFrame - 120][i] == 0) {
            return;
        }
        vec             = D_actor_341700_80175F7C[D_actor_341700_801760FC[work->stateFrame - 120][i]];
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        t               = (rnd >> 16) & 0xFF;
        r               = t;
        gRandomLcgState = rnd;
        if (r % 2) {
            r = -t;
        }
        vec.vx += r / 2;
        vec.vy -= r;
        vec.vz += r;
        shelterB3DumpingHoleSpawnActorSprite(arg1->extra.tmd->coords, &vec);
    }
}

/// Three state handlers, indexed by `_Actor341700PropWork::state`; copied onto
/// the stack before dispatch.
static const EnemyTaskFuncTable3 D_actor_341700_80162058 = { {
    func_actor_341700_8016D2B8,
    func_actor_341700_8016D2E8,
    func_actor_341700_8016C0F4,
} };

/// Per-frame callback of the `func_actor_341700_8016D130` task. It colours the
/// model from the world position of its *second* attach coordinate and then,
/// unless `gSceneCombatState.actorControl` hides the model, runs the handler
/// `_Actor341700PropWork::state` names, with `stateEntered` set on the first
/// tick of a state.
///
/// `case 0` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's tree tests `case 1` at the
/// root; dropping the case makes `case 2` the root and the emitted branches
/// come out with the wrong polarity and a stray low-bound test.
static void func_actor_341700_8016CC9C(Enemy* arg0, Task* arg1)
{
    VECTOR                block;
    _Actor341700PropWork* work = arg1->work;
    EnemyTaskFuncTable3   sp   = D_actor_341700_80162058;

    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    block.vx = arg1->extra.tmd->coords[1].workm.t[0];
    block.vy = arg1->extra.tmd->coords[1].workm.t[1];
    block.vz = arg1->extra.tmd->coords[1].workm.t[2];
    worldCoordUpdateActorColor(arg0, &block, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->prevState != work->state) {
                work->stateEntered = 1;
            } else {
                work->stateEntered = 0;
            }
            work->prevState = work->state;
            sp.funcs[work->state](arg0, arg1);
            if (gGameSession->viewReady != 0) {
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            return;
    }
}

/// Task-state handlers of the `func_actor_341700_8016D130` task: set-up, the
/// per-frame callback, teardown. `func_actor_341700_8016D32C` dispatches them
/// on `Task::state`.
static const EnemyTaskFuncTable3 D_actor_341700_80162064 = { {
    func_actor_341700_8016D130,
    func_actor_341700_8016CC9C,
    enemyDestroy,
} };

s32 func_actor_341700_8016CE28(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj = task->extra.tmd;

    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags = TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// The `0x2704` command handler, reached through the task's `Task::msgTable`
/// table (`D_actor_341700_80175F5C`): the three leading bytes of `cmd` are
/// recorded in `_Actor341700PropWork::lastCommandStage`, `lastCommandArea`
/// and `lastCommand`, and the selector, when the command is the dumping
/// hole's, picks the `ACTOR_341700_PROP_STATE_*` the work block moves to.
///
/// `case 2` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's decision tree balances around
/// `case 1`; dropping `case 2` makes `case 0` the root and the emitted branches
/// come out in a different order.
s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, ActorCommand* cmd, s32 arg3)
{
    _Actor341700PropWork* work = task->work;

    work->lastCommandStage = cmd->context.loc.stage;
    work->lastCommandArea  = cmd->context.loc.area;
    work->lastCommand      = (u8)cmd->command;

    if (cmd->context.key == 0x2704) {
        switch (cmd->command) {
            case 0:
                work->state = ACTOR_341700_PROP_STATE_HIDDEN;
                return 1;
            case 1:
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_341700_PROP_STATE_RISE;
                break;
            case 2:
            default:
                work->state = ACTOR_341700_PROP_STATE_HIDDEN;
                task->state = 1;
                break;
        }
    }
    return 1;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

static void func_actor_341700_8016D130(Enemy* arg0, Task* arg1)
{
    _Actor341700PropWork* work;
    TmdObject*            model;
    GfxCoord*             coord;
    VECTOR                block;

    model      = arg1->extra.tmd;
    coord      = model->coords;
    arg1->work = work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    coord->parent                         = &gGfxViewCoord;
    arg1->extra.tmd->coords->coord.t[1]   = 0;
    arg1->extra.tmd->coords->coord.t[0]   = 0x1388;
    arg1->extra.tmd->coords->coord.t[2]   = -0x1770;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->msgTable                        = D_actor_341700_80175F5C;
    arg0->field_4                         = &coord->coord;
    arg0->field_48                        = 0;
    arg0->bodyPos.vx                      = 0;
    arg0->bodyPos.vy                      = 0;
    arg0->bodyPos.vz                      = 0;
    arg0->coord                           = &arg1->extra.tmd->coords[2];
    arg0->node.state.parts.flags          = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags                   = 0;
    arg0->hpMax                           = 0;
    arg0->hp                              = 0;
    model->lightMtx                       = &work->lightMtx;
    model->colorMtx                       = &work->colorMtx;
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    block.vx = coord->workm.t[0];
    block.vy = coord->workm.t[1];
    block.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0, &block, 0, 0);
    work->prevState = -1;
    work->state     = ACTOR_341700_PROP_STATE_SHOWN;
    arg1->state    += 1;
}

static void func_actor_341700_8016D2B8(Enemy* arg0, Task* arg1)
{
    _Actor341700PropWork* work = arg1->work;
    TmdObject*            model;

    if (work->stateEntered != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
}

static void func_actor_341700_8016D2E8(Enemy* arg0, Task* arg1)
{
    _Actor341700PropWork* work = arg1->work;
    TmdObject*            model;

    if (work->stateEntered != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}

/// Runs the controlled enemy's current state handler from
/// `D_actor_341700_80162064` - spawn/setup, per-frame tick or teardown -
/// copying the table onto the stack before the call.
static void func_actor_341700_8016D32C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_341700_80162064;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
