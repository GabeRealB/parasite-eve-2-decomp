#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_control_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/streamed_scene.h"

#define D_shelter_b1_control_room_80181C3C (D_shelter_b1_control_room_80181BD4 + 13)

// Indexed views below share one contiguous table.
void func_shelter_b1_control_room_8017F100(Task*);

TaskDesc D_shelter_b1_control_room_80181BBC[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_control_room_8017F100, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, streamedScenePlayThenHold, { .value = 0 } },
};

SVECTOR D_shelter_b1_control_room_80181BD4[18] = {
    { 8480, -3330, -2490, 0 },
    { 9080, -3330, -2490, 0 },
    { 9280, -3330, -2490, 0 },
    { 9880, -3330, -2490, 0 },
    { 10090, -3330, -2490, 0 },
    { 10680, -3330, -2490, 0 },
    { 8480, -3330, -6090, 0 },
    { 9080, -3330, -6090, 0 },
    { 9280, -3330, -6090, 0 },
    { 9880, -3330, -6090, 0 },
    { 10090, -3330, -6090, 0 },
    { 10680, -3330, -6090, 0 },
    { 10970, -3000, -3040, 0 },
    { 1180, -850, -2530, 0 },
    { 2180, -850, -2530, 0 },
    { 3180, -850, -2530, 0 },
    { 4180, -850, -2530, 0 },
    { 5180, -850, -2530, 0 },
};

#include "../../shared/streamed_scene_play_then_hold.inc.c"

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    Display_SpawnWithOt(D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_shelter_b1_control_room_8017F150(Task* task)
{
    u8 view;

    if (task->state == 0) {
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_CONTROL_ROOM_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_CONTROL_ROOM_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_CONTROL_ROOM_ORANGE_BURST_2;
        task->state               = 1;
    }

    view = viewGetMappedIndex();
    switch (view) {
        case 2:
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            break;
        case 3:
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[14], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[15], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[16], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[17], 0x200, 0x23);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[3], 0x200, 0x23);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
