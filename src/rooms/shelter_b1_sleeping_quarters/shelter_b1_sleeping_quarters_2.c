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

static void func_shelter_b1_sleeping_quarters_8017E338(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

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
        D_80115734  = 0x60220;
        D_80115730  = 0x6022B;
        D_80115754  = 0x60236;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
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
            func_shelter_b1_sleeping_quarters_8017E338(D_shelter_b1_sleeping_quarters_8018055C, 0x300, 0x100);
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

/// Queues a gouraud disc of four quads at the projected point `worldPoint`, of
/// radius `radiusScale` scaled by depth. The centre colour takes its red, green and
/// blue from bits 8-15, 4 and 0 of `packedColor`, scaled by a brightness alternating
/// between 0x20 and 0x28 on successive frames. Nothing is drawn nearer than
/// OTZ 0x11.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
static void func_shelter_b1_sleeping_quarters_8017E338(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    DisplayState*      displayBase;
    DisplayState*      ds;
    s32                radius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    s32                shiftedColor;
    u8                 brightness;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (RoomDraw25Scratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((RoomDraw25Scratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 1);
        b             = brightness * (packedColor & 1);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*block));
}
