#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b2_breeding_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#define GLOW_DRAW_DISC_SCRATCH RoomDraw31Scratch
#include "../../shared/glow_draw.h"

#define D_shelter_b2_breeding_room_80180470 (D_shelter_b2_breeding_room_80180450 + 4)
#define D_shelter_b2_breeding_room_80180480 (D_shelter_b2_breeding_room_80180450 + 6)
#define D_shelter_b2_breeding_room_801804C0 (D_shelter_b2_breeding_room_80180450 + 14)

// Indexed views below share one contiguous table.

SVECTOR D_shelter_b2_breeding_room_80180450[32] = {
    { 3450, -2450, -2000, 0 },
    { 3000, -2450, -2620, 0 },
    { 880, -2450, -2740, 0 },
    { 2410, -2450, -2740, 0 },
    { -260, -2240, -1360, 0 },
    { -260, -2240, -660, 0 },
    { 7470, -2160, 5560, 0 },
    { 8200, -2160, 5560, 0 },
    { 8990, -2160, 5560, 0 },
    { 9720, -2160, 5560, 0 },
    { 12130, -1430, 6540, 0 },
    { 12480, -1430, 6540, 0 },
    { 1410, -2350, 90, 0 },
    { 1410, -2350, 1890, 0 },
    { 1410, -2350, 3140, 0 },
    { 1410, -2350, 4760, 0 },
    { 2650, -2350, 90, 0 },
    { 2650, -2350, 1890, 0 },
    { 2650, -2350, 3140, 0 },
    { 2650, -2350, 4760, 0 },
    { 4340, -2350, 3830, 0 },
    { 4340, -2350, 5060, 0 },
    { 6780, -2350, 3830, 0 },
    { 6780, -2350, 5060, 0 },
    { 9190, -2350, 3830, 0 },
    { 9190, -2350, 5060, 0 },
    { 11510, -2350, 3830, 0 },
    { 11510, -2350, 5060, 0 },
    { 1210, -2350, -1890, 0 },
    { 2450, -2350, -1890, 0 },
    { 1400, -2350, 620, 0 },
    { 2650, -2350, 620, 0 },
};

/// Room light task. Its first frame sets the effect ids in `D_80115734`,
/// `D_80115730` and `D_80115754`; every frame it draws the glows of the lights
/// the current camera view shows, from the room's light position tables.
void func_shelter_b2_breeding_room_8017D898(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x6027C;
        D_80115730  = 0x6027D;
        D_80115754  = 0x6027E;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180470[0], 0x100, 0x142);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[24], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[25], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[26], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[27], 0x200, 0x444);
            break;
        case 3:
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[0], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[1], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180450[2], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[12], 0x200, 0x333);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[13], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[16], 0x200, 0x333);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[17], 0x200, 0x444);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[0], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[1], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[4], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[5], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[7], 0x200, 0x444);
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[8], 0x200, 0x111);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[9], 0x200, 0x111);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[12], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[13], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[14], 0x200, 0x333);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[15], 0x200, 0x333);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            break;
        case 6:
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[0], 0x100, 0x444);
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[2], 0x100, 0x444);
            glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[4], 0x100, 0x142);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[16], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[17], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[18], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[19], 0x200, 0x222);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[20], 0x200, 0x111);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[21], 0x200, 0x111);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
