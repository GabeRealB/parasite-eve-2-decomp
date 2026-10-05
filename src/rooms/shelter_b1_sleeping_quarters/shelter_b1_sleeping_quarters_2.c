#include "rooms/shelter_b1_sleeping_quarters.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_sleeping_quarters_private.h"

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

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

extern SVECTOR D_shelter_b1_sleeping_quarters_8018054C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018055C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018056C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018058C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018059C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805BC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805EC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018060C[];

SVECTOR D_shelter_b1_sleeping_quarters_8018054C[2] = {
    { -350, -2200, -580, 0 },
    { -350, -2200, -1440, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018055C[2] = {
    { 0x2E18, -2350, 6330, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018056C[4] = {
    { 1760, -2930, 4850, 0 },
    { 2600, -2930, 4850, 0 },
    { 1760, -2930, 2130, 0 },
    { 2600, -2930, 2130, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018058C[2] = {
    { 1760, -2930, -1610, 0 },
    { 2600, -2930, -1610, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018059C[4] = {
    { 4910, -2930, 4710, 0 },
    { 4910, -2930, 3880, 0 },
    { 8680, -2930, 4020, 0 },
    { 8680, -2930, 4850, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_801805BC[6] = {
    { 0x2C24, -2930, 4850, 0 },
    { 0x2C24, -2930, 4030, 0 },
    { 7150, -2290, 4950, 0 },
    { 7840, -2290, 4950, 0 },
    { 7150, -2290, 3350, 0 },
    { 7840, -2290, 3350, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_801805EC[4] = {
    { 5080, -1880, -50, 0 },
    { 5080, -1880, 350, 0 },
    { 6420, -1880, -50, 0 },
    { 6420, -1880, 350, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018060C[8] = {
    { 8130, -2660, -30, 0 },
    { 7740, -2660, -30, 0 },
    { 9540, -2660, -30, 0 },
    { 9080, -2660, -30, 0 },
    { 0x2AC6, -2660, -30, 0 },
    { 0x2904, -2660, -30, 0 },
    { 0x3048, -2660, -30, 0 },
    { 0x2E7C, -2660, -30, 0 },
};

void func_shelter_b1_sleeping_quarters_8017D8E0(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_ORANGE_BURST_2;
        arg0->state               = 1;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 3:
            glowDrawBeam(D_shelter_b1_sleeping_quarters_8018058C, 0x200, 0, 0x111);
        case 2:
            glowDrawBeam(D_shelter_b1_sleeping_quarters_8018054C, 0x200, 0, 0x10);
            break;
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[2], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[6], 0x200, -0x400, 0x111);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018059C;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[6], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[8], 0x200, 0, 0x111);
            break;
        }
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_801805EC;
            glowDrawBeam(&p[0], 0x200, 0x400, 0x111);
            glowDrawBeam(&p[2], 0x200, 0x400, 0x111);
            break;
        }
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018056C;
            glowDrawBeam(&p[0], 0x200, -0x400, 0x111);
            glowDrawBeam(&p[2], 0x200, 0, 0x111);
            glowDrawBeam(&p[8], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[12], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[14], 0x200, 0, 0x111);
            break;
        }
        case 8:
            glowDrawBeam(D_shelter_b1_sleeping_quarters_801805BC, 0x200, 0x800, 0x111);
        case 9:
            glowDrawBitDisc(D_shelter_b1_sleeping_quarters_8018055C, 0x300, 0x100);
            break;
        case 10: {
            SVECTOR* p;
            p = D_shelter_b1_sleeping_quarters_8018060C;
            glowDrawBeam(&p[0], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[2], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[4], 0x200, 0x800, 0x111);
            glowDrawBeam(&p[6], 0x200, 0x800, 0x111);
            break;
        }
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_bit_disc.inc.c"
