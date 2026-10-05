#include "rooms/shelter_b2_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_north_maintenance_walkway_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
#include "../../shared/room_events.h"

/// Anchor points of the glows the room task draws.
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[];
extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[];

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.

/// Offsets from the anchor of the two points the smoke trail follows. The
/// second is also reached under its own name.

TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_north_maintenance_walkway_80183B60[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_north_maintenance_walkway_8017DA88 },
    { 5105, func_shelter_b2_north_maintenance_walkway_8017DC44 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_north_maintenance_walkway_8017DC54 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_north_maintenance_walkway_8017DC4C },
    { ROOM_MESSAGE_SOUND, func_shelter_b2_north_maintenance_walkway_8017DCE4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183B90[4] = {
    { 894, -197, -2271, 0 },
    { 894, -197, -3102, 0 },
    { 894, -197, 376, 0 },
    { 894, -197, -396, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183BB0[14] = {
    { 894, -197, 2648, 0 },
    { 894, -197, 2036, 0 },
    { 3106, -197, -2271, 0 },
    { 3106, -197, -3102, 0 },
    { 3106, -197, 376, 0 },
    { 3106, -197, -396, 0 },
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C20[1] = {
    { 749, -1283, 2310, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C28[1] = {
    { 1107, -1181, -4935, 0 },
};

SVECTOR D_shelter_b2_north_maintenance_walkway_80183C30[1] = {
    { 1107, -1125, -4900, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x9620 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

/// The room's per-frame glow task. Its first tick sets the gameplay effect ids
/// the room's effects use; every tick then draws the flares, discs and stars
/// visible from the current camera view. One star turns from red to blue once
/// flag 0xA8, the one the event gate writes for message 0x1D, is set.
void func_shelter_b2_north_maintenance_walkway_8017DDE8(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST;
        arg0->state               = 1;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            p = D_shelter_b2_north_maintenance_walkway_80183B90;
            glowDrawCone(&p[0], 0x200, 0);
            glowDrawCone(&p[6], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(p, 0x200);
            glowDrawCone(&p[-18], 0x200, 0);
            glowDrawCone(&p[-16], 0x200, 0);
            glowDrawCone(&p[-14], 0x200, 0);
            glowDrawCone(&p[-12], 0x200, 0x400);
            glowDrawCone(&p[-10], 0x200, 0x400);
            glowDrawCone(&p[-8], 0x200, 0x400);
            glowDrawCone(&p[-4], 0x200, 0x800);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(p, 0x200);
            glowDrawCone(&p[-14], 0x200, 0);
            glowDrawCone(&p[-8], 0x200, 0x400);
            glowDrawCone(&p[-4], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(p, 0x200);
            glowDrawCone(&p[-14], 0x200, 0);
            glowDrawCone(&p[-6], 0x200, 0x800);
            glowDrawCone(&p[-4], 0x200, 0x400);
            glowDrawCone(&p[-2], 0x200, -0x400);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_north_maintenance_walkway_80183C20;
            glowDrawRedDisc(p, 0x200);
            glowDrawCone(&p[-14], 0x200, 0);
            glowDrawCone(&p[-4], 0x200, 0x800);
            glowDrawCone(&p[-2], 0x200, 0);
            break;
        }
        case 7:
            if (gameFlagGetNibble(GAME_FLAG_OPERATING_ROOM_NORTH_DOOR_UNLOCKED) != 0) {
                glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C28, 0x100, 0x504C);
            } else {
                glowDrawTintedDisc(D_shelter_b2_north_maintenance_walkway_80183C30, 0x100, 0x5C40);
            }
            break;
        case 8:
            glowDrawCone(D_shelter_b2_north_maintenance_walkway_80183BB0, 0x200, 0x400);
            break;
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b2_north_maintenance_walkway_8017F590(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b2_north_maintenance_walkway_801802D8(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b2_north_maintenance_walkway_80180670(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_north_maintenance_walkway_80181A80(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b2_north_maintenance_walkway_80181BB4(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b2_north_maintenance_walkway_80182618(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_north_maintenance_walkway_80182F00(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
