#include "rooms/acropolis_cafeteria.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "acropolis_cafeteria_private.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_flags.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/actor_contacts.h"

/// Phases of the loose-prop task, held in `_AcropolisCafeteriaLoosePropWork::phase`.
enum {
    ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING,  // Waiting for anything to touch the body
    ACROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING,  // Knocked upwards: the lift wears off while the prop spins and slides
    ACROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING,  // Horizontal speed decaying to a stop
    ACROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING, // Pitch and roll decaying to level, then resting again
};

/// Work block of the cafeteria's loose-prop task, kept at `Task::work`.
///
/// The prop is a small model set down above and ahead of the player. It sinks
/// at a constant rate every frame, and whenever its collision sphere touches
/// another body it is knocked away along the player's facing with a hop and a
/// tumble, slides to a stop and levels out before it can be knocked again.
/// The block owns the body and the contact table the body borrows, so the
/// task's exit unlinks the body before the block is freed.
typedef struct {
    WorldCollisionBody    body;          // Sphere linked into the world's collision lists
    WorldCollisionContact contacts[6];   // Contact table the body borrows; emptied after every update
    s32                   kickStrength;  // Random 0x3000..0x3FFF at set-up, one less every frame; scales the launch speed and the spin
    VECTOR                velocity;      // World units added to the prop's position per frame; `vy` is the hop on top of the constant sink
    SVECTOR               rotation;      // Euler angles (4096 per turn) the model's rotation is rebuilt from every frame
    SVECTOR               kickDirection; // Player's forward axis at the contact, normalised to 4096
    u16                   phase;         // An `ACROPOLIS_CAFETERIA_LOOSE_PROP_*` phase
} _AcropolisCafeteriaLoosePropWork;
STATIC_ASSERT_SIZEOF(_AcropolisCafeteriaLoosePropWork, 0xD8);

extern MATRIX  D_acropolis_cafeteria_8018D5A0;
extern MATRIX  D_acropolis_cafeteria_8018D5C0;
extern MATRIX  D_acropolis_cafeteria_8018D5E0;
extern MATRIX  D_acropolis_cafeteria_8018D600;
extern MATRIX  D_acropolis_cafeteria_8018D620;
extern MATRIX  D_acropolis_cafeteria_8018D640;
extern MATRIX  D_acropolis_cafeteria_8018D660;
extern MATRIX  D_acropolis_cafeteria_8018D680;
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void _acropolisCafeteriaLoosePropExit(Task* task);

static u32     _gAcropolisCafeteriaModel0FDFCPartVerts[1];
static SVECTOR _gAcropolisCafeteriaModel0FDFCVerts[22];
static SVECTOR _gAcropolisCafeteriaModel0FDFCNormals[18];
static TmdBone _gAcropolisCafeteriaModel0FDFCSkeleton[1];
static u32     _gAcropolisCafeteriaModel0FDFCStream[112];

extern SpriteDrawArea D_acropolis_cafeteria_8018B3A4[2];
extern SpriteBatch    D_acropolis_cafeteria_8018AA30[2];
extern SpriteBatch    D_acropolis_cafeteria_8018AD60[10];
extern SpriteBatch    D_acropolis_cafeteria_8018B2EC[19];
extern SpriteBatch    D_acropolis_cafeteria_8018B394[2];
extern SpriteBatch    D_acropolis_cafeteria_8018B6C4[6];
extern SpriteBatch    D_acropolis_cafeteria_8018B6F4[2];
extern SpriteBatch    D_acropolis_cafeteria_8018B704[2];
extern SpriteBatch    D_acropolis_cafeteria_8018B714[2];
extern SpriteBatch    D_acropolis_cafeteria_8018B724[2];
extern SpriteBatch    D_acropolis_cafeteria_8018B7C0[3];
extern SpriteBatch    D_acropolis_cafeteria_8018B800[3];
extern SpriteBatch    D_acropolis_cafeteria_8018B818[2];
extern SpriteBatch    D_acropolis_cafeteria_8018BA94[17];
extern SpriteBatch    D_acropolis_cafeteria_8018BC0C[3];
extern SpriteBatch    D_acropolis_cafeteria_8018BC24[2];
extern SpriteBatch    D_acropolis_cafeteria_8018BC34[2];
extern SpriteBatch    D_acropolis_cafeteria_8018BC44[2];
extern SpriteBatch    D_acropolis_cafeteria_8018BC54[2];
extern SpriteBatch    D_acropolis_cafeteria_8018BC64[2];
extern SpriteBatch    D_acropolis_cafeteria_8018C250[19];
extern SpriteSource   D_acropolis_cafeteria_8018AA40[40];
extern SpriteSource   D_acropolis_cafeteria_8018ADB0[67];
extern SpriteSource   D_acropolis_cafeteria_8018B3B8[39];
extern SpriteSource   D_acropolis_cafeteria_8018B734[7];
extern SpriteSource   D_acropolis_cafeteria_8018B7D8[2];
extern SpriteSource   D_acropolis_cafeteria_8018B828[31];
extern SpriteSource   D_acropolis_cafeteria_8018BB1C[12];
extern SpriteSource   D_acropolis_cafeteria_8018BC74[75];

WorldCoordRoomLights D_acropolis_cafeteria_8018AA18[1] = {
    { 0, NULL, ARRAY_SIZE(gAcropolisCafeteriaPointLights), gAcropolisCafeteriaPointLights, ARRAY_SIZE(gAcropolisCafeteriaSpotLightStorage.liveLights), gAcropolisCafeteriaSpotLightStorage.liveLights },
};

SpriteBatch D_acropolis_cafeteria_8018AA30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018AA40[40] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 48, 751, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 56, 709, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 72, 64, 677, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 80, 627, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 96, 659, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 40, 874, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 56, 871, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 56, 867, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 56, 867, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 48, 770, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 56, 696, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 56, 770, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 72, 638, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 128, 72, 864, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 700, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 80, 697, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 80, 699, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 88, 672, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 96, 649, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 88, 650, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 96, 664, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 48, 104, 728, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 88, 660, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 104, 104, 616, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -8, 1350, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 1275, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 1225, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -24, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -16, 1050, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 136, -8, 975, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, -32, 925, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -24, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 900, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -24, 1000, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, -32, 1075, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, -40, 1175, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -48, 1175, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -104, 1000, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -120, 975, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, -120, 950, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018AD60[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 4, 0, 0, { 1, 0 } },
    { 9, 5, 0, 0, { 4, 0 } },
    { 14, 8, 0, 0, { 0, 0 } },
    { 22, 2, 0, 0, { 5, 0 } },
    { 24, 0, 0, 0, { 3, 0 } },
    { 24, 13, 0, 0, { 7, 0 } },
    { 37, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018ADB0[67] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, -24, 1444, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1443, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, 0, 1460, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -16, 1620, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 1601, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -16, 1365, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, -16, 1324, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -8, 1319, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 8, 1370, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -88, 16, 875, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1038, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1052, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 873, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1035, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -88, 40, 888, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 1052, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 902, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 72, 906, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -104, 32, 925, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 32, 894, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 56, 926, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 120, 48, 804, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 56, 688, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 80, 810, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 96, 780, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 40, 809, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 48, 808, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 64, 667, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, 80, 862, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 88, 647, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 601, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 678, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 88, 662, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 96, 622, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 96, 589, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 80, 112, 668, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -56, 1850, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -48, 1875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -32, 1900, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -48, 1775, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -40, 1825, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 48, -40, 1725, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -40, 1375, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 1775, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -32, 1625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, -32, 1375, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1675, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 72, -24, 1425, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -16, 1450, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 88, -16, 1250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -8, 1325, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 8, 1100, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 0, 1125, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 1200, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -8, 1200, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -96, 1700, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -96, 1525, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -104, 1450, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -104, 1325, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -104, 1200, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -104, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -56, 1150, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -16, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, -16, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 8, 1150, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B2EC[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 6, 0 } },
    { 0, 3, 0, 0, { 9, 0 } },
    { 3, 2, 0, 0, { 2, 0 } },
    { 5, 1, 0, 0, { 10, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 11, 0 } },
    { 9, 1, 0, 0, { 1, 0 } },
    { 10, 8, 0, 0, { 12, 0 } },
    { 18, 1, 0, 0, { 8, 0 } },
    { 19, 2, 0, 0, { 13, 0 } },
    { 21, 4, 0, 0, { 4, 0 } },
    { 25, 6, 0, 0, { 14, 0 } },
    { 31, 5, 0, 0, { 3, 0 } },
    { 36, 0, 0, 0, { 15, 0 } },
    { 36, 0, 0, 0, { 7, 0 } },
    { 36, 21, 0, 0, { 16, 0 } },
    { 57, 10, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B384[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B394[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_cafeteria_8018B3A4[2] = {
    { { 72, 0, 248, 239 }, 707 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_cafeteria_8018B3B8[39] = {
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -160, -120, 525, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -96, -120, 525, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -32, -120, 525, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, 32, -120, 525, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, 96, -120, 525, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -56, 925, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -56, 950, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 64, -8, 925, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 80, 0, 925, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 24, 0, 875, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -8, 875, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -16, 875, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 0, 850, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, 0, 825, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -96, -8, 800, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -128, 24, 800, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 800, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 8, 775, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 825, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 80, 850, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, -128, 40, 850, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -128, 80, 875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -72, 40, 875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -72, 72, 900, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 32, 900, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 64, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 24, 24, 925, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 24, 56, 950, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 16, 975, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 48, 1000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -8, 950, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 32, 1025, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 634, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 88, 250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 96, 225, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 96, 225, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 112, 200, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 104, 225, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 8, 112, 200, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B6C4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 3, 0 } },
    { 32, 4, 0, 0, { 0, 0 } },
    { 36, 1, 0, 0, { 2, 0 } },
    { 37, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B6F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B704[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B714[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018B734[7] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 64, 525, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 72, 492, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, 72, 636, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -24, 80, 499, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 32, 80, 515, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, -32, 96, 506, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, 40, 96, 250, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B7C0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018B7D8[2] = {
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 0, 675, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 0, 675, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B800[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B818[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018B828[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 40, 1100, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -16, 975, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 48, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -16, 975, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, -16, 1000, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 0, 1025, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 8, 1037, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 40, 1025, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 32, 1050, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 32, 1075, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 56, 787, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, 40, 825, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 32, 1163, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 8, 950, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 16, 925, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 32, 937, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 0, 950, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 40, 925, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -80, 16, 900, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, 8, 925, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 962, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 8, 975, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 16, 950, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -96, 0, 1350, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -104, -40, 1325, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -112, -72, 1300, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, -120, -112, 1275, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, -136, -120, 1125, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 72 } }, -160, -120, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -160, -48, 1100, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -136, -48, 1125, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018BA94[17] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 8, 0 } },
    { 10, 2, 0, 0, { 3, 0 } },
    { 12, 4, 0, 0, { 12, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 10, 0 } },
    { 23, 0, 0, 0, { 1, 0 } },
    { 23, 0, 0, 0, { 13, 0 } },
    { 23, 0, 0, 0, { 4, 0 } },
    { 23, 0, 0, 0, { 9, 0 } },
    { 23, 0, 0, 0, { 7, 0 } },
    { 23, 0, 0, 0, { 14, 0 } },
    { 23, 0, 0, 0, { 6, 0 } },
    { 23, 0, 0, 0, { 11, 0 } },
    { 23, 0, 0, 0, { 5, 0 } },
    { 23, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018BB1C[12] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -120, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -40, 750, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, 32, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -120, -120, 775, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, -40, 775, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, 32, 775, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -88, -120, 800, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -88, -40, 800, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, -88, 32, 800, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -56, -120, 825, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -56, -40, 825, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 56 } }, -56, 32, 825, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018BC0C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018BC74[75] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, -24, 1444, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1443, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, 0, 1460, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -16, 1620, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 1601, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -16, 1365, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, -16, 1324, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -8, 1319, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 8, 1370, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -88, 16, 875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1038, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1052, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 873, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1035, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -88, 40, 888, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 1052, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 902, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 72, 906, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -104, 32, 925, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 56, 724, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 40, 1019, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 40, 896, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 56, 688, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 48, 724, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 48, 804, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 56, 688, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 80, 810, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 96, 780, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 40, 809, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 80, 48, 808, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 80, 64, 667, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, 80, 862, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 88, 647, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 601, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 678, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 88, 662, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 96, 622, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 96, 589, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 80, 112, 668, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -32, 1900, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, -56, 1850, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -48, 1875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -48, 1775, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -40, 1825, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -40, 1375, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, -32, 1375, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 1775, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 8, 1100, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 0, 1125, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 1200, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -8, 1200, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -8, 1325, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1675, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -16, 1450, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -40, 1725, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -40, 1725, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -32, 1625, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -32, 1625, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -16, 1186, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1250, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 104, -24, 1196, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -24, 1425, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -24, 1375, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -96, 1700, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -96, 1525, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -104, 1450, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -104, 1325, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -104, 1200, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -104, 1150, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -56, 1150, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -16, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, -16, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 8, 1150, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C250[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 6, 0 } },
    { 0, 3, 0, 0, { 9, 0 } },
    { 3, 2, 0, 0, { 2, 0 } },
    { 5, 1, 0, 0, { 10, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 11, 0 } },
    { 9, 1, 0, 0, { 1, 0 } },
    { 10, 8, 0, 0, { 12, 0 } },
    { 18, 1, 0, 0, { 8, 0 } },
    { 19, 5, 0, 0, { 13, 0 } },
    { 24, 4, 0, 0, { 4, 0 } },
    { 28, 6, 0, 0, { 14, 0 } },
    { 34, 5, 0, 0, { 3, 0 } },
    { 39, 0, 0, 0, { 15, 0 } },
    { 39, 0, 0, 0, { 7, 0 } },
    { 39, 26, 0, 0, { 16, 0 } },
    { 65, 10, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018C2E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018C2F8[16] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -120, 525, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -120, -120, 475, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -96, -120, 425, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -120, 400, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -120, 375, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, -120, 350, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -120, 325, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -120, 300, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -120, 276, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 48, 300, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -144, 56, 287, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -88, 64, 275, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -40, 72, 275, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 8, 80, 275, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 56, 88, 237, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 96, 225, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C438[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 7, 0, 0, { 2, 0 } },
    { 16, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_cafeteria_8018C460[1] = {
    { 143, 0x3FC0, { .fields = { 88, 24 } }, -80, 96, 672, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C474[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_cafeteria_8018C48C[24] = {
    { { .empty = D_acropolis_cafeteria_8018AA30 }, D_acropolis_cafeteria_8018AA30, NULL },
    { { .elements = D_acropolis_cafeteria_8018AA40 }, D_acropolis_cafeteria_8018AD60, NULL },
    { { .elements = D_acropolis_cafeteria_8018ADB0 }, D_acropolis_cafeteria_8018B2EC, NULL },
    { { .empty = D_acropolis_cafeteria_8018AA30 }, D_acropolis_cafeteria_8018AA30, NULL },
    { { .empty = D_acropolis_cafeteria_8018B394 }, D_acropolis_cafeteria_8018B394, D_acropolis_cafeteria_8018B3A4 },
    { { .elements = D_acropolis_cafeteria_8018B3B8 }, D_acropolis_cafeteria_8018B6C4, NULL },
    { { .empty = D_acropolis_cafeteria_8018B6F4 }, D_acropolis_cafeteria_8018B6F4, NULL },
    { { .empty = D_acropolis_cafeteria_8018B704 }, D_acropolis_cafeteria_8018B704, NULL },
    { { .empty = D_acropolis_cafeteria_8018B714 }, D_acropolis_cafeteria_8018B714, NULL },
    { { .empty = D_acropolis_cafeteria_8018B724 }, D_acropolis_cafeteria_8018B724, NULL },
    { { .elements = D_acropolis_cafeteria_8018B734 }, D_acropolis_cafeteria_8018B7C0, NULL },
    { { .elements = D_acropolis_cafeteria_8018B7D8 }, D_acropolis_cafeteria_8018B800, NULL },
    { { .empty = D_acropolis_cafeteria_8018B818 }, D_acropolis_cafeteria_8018B818, NULL },
    { { .elements = D_acropolis_cafeteria_8018B828 }, D_acropolis_cafeteria_8018BA94, NULL },
    { { .elements = D_acropolis_cafeteria_8018BB1C }, D_acropolis_cafeteria_8018BC0C, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC24 }, D_acropolis_cafeteria_8018BC24, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC34 }, D_acropolis_cafeteria_8018BC34, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC44 }, D_acropolis_cafeteria_8018BC44, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC54 }, D_acropolis_cafeteria_8018BC54, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC64 }, D_acropolis_cafeteria_8018BC64, NULL },
    { { .elements = D_acropolis_cafeteria_8018BC74 }, D_acropolis_cafeteria_8018C250, NULL },
    { { .elements = D_acropolis_cafeteria_8018B828 }, D_acropolis_cafeteria_8018BA94, NULL },
    { { .elements = D_acropolis_cafeteria_8018C2F8 }, D_acropolis_cafeteria_8018C438, NULL },
    { { .elements = D_acropolis_cafeteria_8018C460 }, D_acropolis_cafeteria_8018C474, NULL },
};

ViewCamera D_acropolis_cafeteria_8018C5AC[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x59D8, 1500 } }, 380 },
    { { { { 3792, 0, -1548 }, { -816, 3480, -1998 }, { 1315, 2158, 3222 } }, { 4570, 3240, 520 } }, 207 },
    { { { { 3878, 0, -1317 }, { -530, 3748, -1562 }, { 1205, 1650, 3549 } }, { 4720, 2950, 4230 } }, 207 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { -1722, 0, -3715 }, { -621, 4038, 287 }, { 3663, 684, -1698 } }, { 3510, 2090, 3480 } }, 207 },
    { { { { -953, 0, -3983 }, { -1242, 3891, 297 }, { 3784, 1278, -905 } }, { 3870, 2600, -1340 } }, 257 },
    { { { { -3525, 0, -2084 }, { -966, 3629, 1634 }, { 1846, 1898, -3124 } }, { 150, 2740, -1420 } }, 207 },
    { { { { 3548, 0, -2045 }, { 441, 3999, 765 }, { 1997, -883, 3464 } }, { 150, 1000, 2080 } }, 207 },
    { { { { 3963, 0, -1031 }, { -276, 3945, -1063 }, { 993, 1098, 3818 } }, { -1180, 1880, -1890 } }, 257 },
    { { { { -1719, 0, -3717 }, { -972, 3953, 449 }, { 3588, 1071, -1660 } }, { -2450, 1700, 1840 } }, 207 },
    { { { { 3850, 0, -1396 }, { -577, 3728, -1593 }, { 1271, 1694, 3505 } }, { 3160, 2330, 3700 } }, 289 },
    { { { { -3566, 0, -2014 }, { 400, 4014, -709 }, { 1974, -814, -3494 } }, { 3790, 610, 520 } }, 207 },
    { { { { -3940, 0, -1116 }, { -1024, 1627, 3616 }, { 443, 3758, -1566 } }, { 10, 1940, 910 } }, 289 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { -1722, 0, -3715 }, { -621, 4038, 287 }, { 3663, 684, -1698 } }, { 3510, 2090, 3480 } }, 207 },
    { { { { -3831, 0, -1448 }, { -1361, 1402, 3599 }, { 495, 3848, -1311 } }, { 3820, 2840, 960 } }, 207 },
    { { { { 630, 0, -4047 }, { -3556, 1955, -553 }, { 1932, 3598, 300 } }, { 4220, 3110, 1600 } }, 230 },
    { { { { -4088, 0, -245 }, { 44, 4029, -735 }, { 241, -736, -4022 } }, { 3790, 1260, -590 } }, 329 },
    { { { { 4094, 0, -102 }, { 1, 4095, 45 }, { 102, -45, 4094 } }, { 3470, 1610, 840 } }, 447 },
    { { { { 3987, 0, -935 }, { 234, 3965, 997 }, { 905, -1024, 3860 } }, { 3900, 600, 3260 } }, 257 },
    { { { { 3878, 0, -1317 }, { -530, 3748, -1562 }, { 1205, 1650, 3549 } }, { 4720, 2950, 4230 } }, 207 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { 2289, 0, -3396 }, { 208, 4088, 140 }, { 3390, -251, 2285 } }, { 1520, 1730, -80 } }, 257 },
    { { { { -3916, 0, -1197 }, { 691, 3344, -2261 }, { 977, -2365, -3197 } }, { 3690, 380, -80 } }, 257 },
};

WorldCoordRoomAmbientEntry D_acropolis_cafeteria_8018C90C[25] = {
    { .viewCount = ARRAY_SIZE(D_acropolis_cafeteria_8018C90C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 500, 1000, 1831, 916 } },
    { .color = { 500, 1000, 1831, 916 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

AreaApplyRec D_acropolis_cafeteria_8018C9D4[3] = {
    { 1, 3, 2, 0 },
    { 1, 9, 2, 1 },
    { 255, 0, 0, 0 },
};

WorldCollisionFootstepSounds D_acropolis_cafeteria_8018C9E0 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_acropolis_cafeteria_8018C9EC = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionFootstepSounds D_acropolis_cafeteria_8018C9F8 = {
    0x10000001,
    0x10000003,
    0x10000005,
};

WorldCollisionSurfaceProperties D_acropolis_cafeteria_8018CA04[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_cafeteria_8018CA0C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_cafeteria_8018C9E0 },
};

WorldCollisionSurfaceProperties D_acropolis_cafeteria_8018CA14[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_cafeteria_8018C9EC },
};

WorldCollisionSurfaceProperties D_acropolis_cafeteria_8018CA1C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_cafeteria_8018C9F8 },
};

WorldCollisionSurfaceProperties D_acropolis_cafeteria_8018CA24[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_cafeteria_8018C9F8 },
};

WorldCollisionSurfaceProperties* D_acropolis_cafeteria_8018CA2C[8] = {
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA0C,
    D_acropolis_cafeteria_8018CA14,
    D_acropolis_cafeteria_8018CA1C,
    D_acropolis_cafeteria_8018CA24,
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA04,
};

static TmdBone _gAcropolisCafeteriaModel0F7A4Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_0F7A4_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel0F7A4PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_0F7A4_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0F7A4Verts[41] = {
#include "assets/acropolis_cafeteria_model_0F7A4_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0F7A4Normals[53] = {
#include "assets/acropolis_cafeteria_model_0F7A4_normals.inc"
};

static u32 _gAcropolisCafeteriaModel0F7A4Stream[307] = {
#include "assets/acropolis_cafeteria_model_0F7A4_stream.inc"
};

TmdSource gAcropolisCafeteriaModel0F7A4 = {
    0,
    2168,
    0,
    1,
    _gAcropolisCafeteriaModel0F7A4PartVerts,
    _gAcropolisCafeteriaModel0F7A4Verts,
    _gAcropolisCafeteriaModel0F7A4Normals,
    _gAcropolisCafeteriaModel0F7A4Skeleton,
    _gAcropolisCafeteriaModel0F7A4Stream,
};

static TmdBone _gAcropolisCafeteriaModel0FDFCSkeleton[1] = {
#include "assets/acropolis_cafeteria_model_0FDFC_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel0FDFCPartVerts[1] = {
#include "assets/acropolis_cafeteria_model_0FDFC_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0FDFCVerts[22] = {
#include "assets/acropolis_cafeteria_model_0FDFC_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0FDFCNormals[18] = {
#include "assets/acropolis_cafeteria_model_0FDFC_normals.inc"
};

static u32 _gAcropolisCafeteriaModel0FDFCStream[112] = {
#include "assets/acropolis_cafeteria_model_0FDFC_stream.inc"
};

TmdSource gAcropolisCafeteriaModel0FDFC = {
    0,
    756,
    0,
    1,
    _gAcropolisCafeteriaModel0FDFCPartVerts,
    _gAcropolisCafeteriaModel0FDFCVerts,
    _gAcropolisCafeteriaModel0FDFCNormals,
    _gAcropolisCafeteriaModel0FDFCSkeleton,
    _gAcropolisCafeteriaModel0FDFCStream,
};

MATRIX D_acropolis_cafeteria_8018D5A0 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D5C0 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D5E0 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D600 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D620 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D640 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D660 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D680 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

s32 D_acropolis_cafeteria_8018D6A0 = 0;

s32 D_acropolis_cafeteria_8018D6A4 = 0;

s32 D_acropolis_cafeteria_8018D6A8 = 0;

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _acropolisCafeteriaLoosePropInit(Task* task);
static void _acropolisCafeteriaLoosePropUpdate(Task* task);
static void _acropolisCafeteriaLoosePropRequestExit(Task* task);
static void func_acropolis_cafeteria_80182954(Task* task);
static void func_acropolis_cafeteria_80182A08(Task* task);

void func_acropolis_cafeteria_8017E47C(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            break;

        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            break;

        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->killCountdown   = 0;
            task->spawnArg1.value = 0;
            task->state           = task->state + 1;
            break;

        case 3:
            if (++task->killCountdown == 0x443) {
                Stage_RequestFromAreaTable(0);
                task->spawnArg1.value = 1;
            }
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                break;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            break;

        case 4:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            break;

        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            if (task->spawnArg1.value == 0) {
                Stage_RequestFromAreaTable(0);
            }
            taskKill(task);
            displayResumeGameLoop();
            break;
    }
}

void acropolisCafeteriaBlackoutTask(Task* task)
{
    enum {
        BLACKOUT_INTENSITY    = 0xFF,
        BLACKOUT_COUNTER_STEP = 4,
        BLACKOUT_COUNTER_END  = 0x100
    };
    u16 elapsedQuarterTicks;

    fadeDrawOverlay(BLACKOUT_INTENSITY, BLACKOUT_INTENSITY, BLACKOUT_INTENSITY, GPU_BLEND_SUBTRACT);
    elapsedQuarterTicks = task->killCountdown + BLACKOUT_COUNTER_STEP;
    task->killCountdown = elapsedQuarterTicks;
    if ((s16)elapsedQuarterTicks >= BLACKOUT_COUNTER_END) {
        taskKill(task);
    }
}

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_cafeteria_80184178, 2, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;

    work  = (EffectWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state != 0) {
        return;
    }
    task->msgTable = D_acropolis_cafeteria_80184CEC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
    vec                            = &work->move;
    D_acropolis_cafeteria_80184CFC = 0;
    work->move.vx                  = 0x220;
    work->move.vy                  = -0x12C;
    work->move.vz                  = -0x6A0;
    Gp_SpawnEff(EFFECT_064, coord, 0, vec);
    work->move.vx = 0x400;
    work->move.vy = -0x12C;
    work->move.vz = -0x260;
    Gp_SpawnEff(EFFECT_064, coord, 0, vec);
    work->move.vx = 0x370;
    work->move.vy = -0x12C;
    work->move.vz = -0x860;
    Gp_SpawnEff(EFFECT_064, coord, 0, vec);
    task->state   = task->state + 1;
    work->move.vx = 0xBB8;
    work->move.vy = -0x834;
    work->move.vz = -0x7D0;
    Gp_SpawnEff(EFFECT_064, coord, 1, vec);
    work->move.vx = 0xB22;
    work->move.vy = -0x834;
    work->move.vz = -0x900;
    Gp_SpawnEff(EFFECT_064, coord, 1, vec);
    gRoomEffectFlashId      = EFFECT_ACROPOLIS_CAFETERIA_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_ACROPOLIS_CAFETERIA_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_ACROPOLIS_CAFETERIA_SPARK_BURST;
}
/// Spawns 40 effects on entry to session mode 9, then two per tick while it
/// remains active. Releases the work block when the room effect gate clears.
void func_acropolis_cafeteria_8017E89C(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         i;
    u16         count;
    s32         flags;
    s32         spawnArg;
    u8          mode;
    u16         rnd;

    work  = (EffectWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (D_acropolis_cafeteria_80184CFC == 0) {
        effectKillTask(work, task);
        return;
    }
    mode = gGameSession->location.loc.view;
    if (mode == 9) {
        count = 0x28;
        if (work->scale != mode) {
            flags = 0x1000;
        } else {
            count = 2;
            flags = 0;
        }
        for (i = 0; i < count; i++) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vx   = (u32)rnd % 2620 + 0x230;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vy   = -0x12C - (u16)((u32)rnd % 5) * 0x190;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vz   = (rnd & 0x3FF) + 0xB00;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            spawnArg        = flags + 0x180;
            Gp_SpawnEff(EFFECT_ACROPOLIS_CAFETERIA_PUFF, coord, (rnd & 0xFF) + spawnArg, &work->move);
        }
    }
    work->scale = gGameSession->location.loc.view;
}

/// Sets the puff quad's UVs to its current 48 x 48 texel animation cell.
///
/// `work->age` is elapsed active ticks and `work->step` is ticks per cell.
/// The period must be positive, with 0 <= age < 10 * period. Cells 0..9 run
/// left to right across five columns, then continue on the second row.
/// UV endpoints include the last texel (origin + 47), so U stays in 0..239
/// and V in 0..95. The quad and work are distinct borrowed live objects;
/// only the eight UV bytes are written, without advancing the animation.
static inline void _acropolisCafeteriaSetPuffCellUvs(POLY_FT4* quad, const EffectWork* work)
{
    enum { PUFF_SHEET_COLUMNS = 5,
           PUFF_CELL_TEXELS   = 48,
           PUFF_UV_SPAN       = PUFF_CELL_TEXELS - 1 };

    quad->u0 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->v0 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->u1 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->v1 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->u2 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->v2 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->u3 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->v3 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
}

void acropolisCafeteriaPuffTask(Task* task)
{
    enum {
        PUFF_ACTIVE_VIEW        = 9,
        PUFF_SEED_MIN_DEPTH     = 16,
        PUFF_ROTATION_MASK      = 0xFFF,
        PUFF_SPAWN_SIZE_MASK    = 0xFFF,
        PUFF_SPAWN_SKIP_FADE    = 0x1000,
        PUFF_SKIP_FADE_AGE      = 10,
        PUFF_FADE_IN_TICKS      = 10,
        PUFF_MAX_SHADE          = 40,
        PUFF_CELL_TEXELS        = 48,
        PUFF_UV_SPAN            = PUFF_CELL_TEXELS - 1,
        PUFF_CELL_COUNT         = 10,
        PUFF_QUARTER_TURN       = 0x400,
        PUFF_TRIG_FRACTION_BITS = 12,
        PUFF_DRIFT_Z_LIMIT      = 0xB00
    };
    EffectWork*           work;
    GfxCoord*             coord;
    OverlaySpriteScratch* scratchEnd;
    OverlaySpriteScratch* scratch;
    POLY_FT4*             quad;
    u8                    view;
    u8                    shade;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (D_acropolis_cafeteria_80184CFC != 0) {
        view = gGameSession->location.loc.view;
        if (view == PUFF_ACTIVE_VIEW) {
            // Project the centre once; the billboard's rotation is in screen space.
            actorRenderComposeCoord(coord);
            scratchEnd = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
            SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
            scratch              = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
            scratch->worldPos.vx = coord->workm.t[0];
            scratch->worldPos.vy = coord->workm.t[1];
            scratch->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&scratchEnd[-1].worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&scratchEnd[-1].screenPos);
            gte_stszotz(&scratch->otz);
            // Depth gates seeding only; drawing and division below always proceed.
            if (scratchEnd[-1].otz > PUFF_SEED_MIN_DEPTH && work->age == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & PUFF_ROTATION_MASK;
                work->angle     = task->spawnArg1.halves.low & PUFF_SPAWN_SIZE_MASK;
                work->move.vx   = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = ((gRandomLcgState >> 16) & 0xF) + 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = -((gRandomLcgState >> 16) & 0xF) - 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step      = ((gRandomLcgState >> 16) & 3) + 3;
                if (task->spawnArg1.value & PUFF_SPAWN_SKIP_FADE) {
                    work->age = PUFF_SKIP_FADE_AGE;
                }
            }
            if (work->age < PUFF_FADE_IN_TICKS) {
                shade = work->age * 4;
                setRGB0(quad, shade, shade, shade);
            } else {
                shade = PUFF_MAX_SHADE;
                setRGB0(quad, shade, shade, shade);
            }
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(0, 270);
            setSemiTrans(quad, 1);
            _acropolisCafeteriaSetPuffCellUvs(quad, work);
            scratch->cornerDx = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rsin(work->scale)) >> PUFF_TRIG_FRACTION_BITS;
            scratch->cornerDy = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rcos(work->scale)) >> PUFF_TRIG_FRACTION_BITS;
            quad->x0          = scratch->screenPos.vx + scratch->cornerDx;
            quad->x3          = scratch->screenPos.vx - scratch->cornerDx;
            quad->y0          = scratch->screenPos.vy - scratch->cornerDy;
            quad->y3          = scratch->screenPos.vy + scratch->cornerDy;
            scratch->cornerDx = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rsin(work->scale + PUFF_QUARTER_TURN)) >> PUFF_TRIG_FRACTION_BITS;
            scratch->cornerDy = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rcos(work->scale + PUFF_QUARTER_TURN)) >> PUFF_TRIG_FRACTION_BITS;
            quad->x1          = scratch->screenPos.vx + scratch->cornerDx;
            quad->x2          = scratch->screenPos.vx - scratch->cornerDx;
            quad->y1          = scratch->screenPos.vy - scratch->cornerDy;
            quad->y2          = scratch->screenPos.vy + scratch->cornerDy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
            // Move toward the Z boundary first, then descend along positive Y.
            if (coord->coord.t[2] > PUFF_DRIFT_Z_LIMIT) {
                coord->coord.t[2] += work->move.vz;
            } else {
                coord->coord.t[1] += work->move.vy;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->age++;
            if (work->age <= work->step * PUFF_CELL_COUNT - 1) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}

void acropolisCafeteriaModelWanderTask(Task* task)
{
    enum {
        WANDER_INITIALIZE                    = 0,
        WANDER_TIMED_TURN                    = 0,
        WANDER_TIMED_MOVE                    = 1,
        WANDER_AMBIENT_TURN                  = 2,
        WANDER_AMBIENT_MOVE                  = 3,
        WANDER_DEPART                        = 4,
        WANDER_DEPART_AGE                    = 121,
        WANDER_TIMED_EXIT_X                  = 0xB00,
        WANDER_AMBIENT_EXIT_X                = 0xD90,
        WANDER_EXIT_YAW                      = 0x400,
        WANDER_MOVE_SPEED_Q4                 = 0x200,
        WANDER_DEPART_SPEED_Q4               = 0x300,
        WANDER_DIRECTION_SPEED_FRACTION_BITS = 16,
        WANDER_SOUND_VIEW                    = 7,
        WANDER_SOUND_SCRIPT                  = 6,
        WANDER_SOUND_PENDING                 = 0,
        WANDER_SOUND_PLAYED                  = 1
    };
    TmdObject*  model;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         previousYaw;
    s32         departureYaw;
    s32         nextYaw;
    s32         randomIncrement;
    s32         audioPan;

    model         = task->extra.tmd;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = model->coords;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    actorRenderComposeCoord(coord);
    work->age++;
    // A nonzero spawn argument selects ambient wandering without the departure timer.
    if (task->state == WANDER_INITIALIZE) {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (task->spawnArg1.value != 0) {
            work->period    = WANDER_AMBIENT_EXIT_X;
            work->angle     = 0;
            work->index     = WANDER_AMBIENT_TURN;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) & 0xF00;
        } else {
            work->scale  = WANDER_EXIT_YAW;
            work->angle  = 0;
            work->period = WANDER_TIMED_EXIT_X;
        }
        gfxRotMatrixY(&coord->coord, work->scale, GRAPHICS_ROTATION_COMPOSE);
        task->state++;
        return;
    }
    switch (work->index) {
        case WANDER_TIMED_TURN:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = 0;
            work->scale    -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 30) == 0) {
                work->index = WANDER_TIMED_MOVE;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_TIMED_MOVE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                previousYaw = work->scale;
                if (previousYaw > WANDER_EXIT_YAW) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    nextYaw         = previousYaw - 0x10;
                    nextYaw        -= (gRandomLcgState >> 16) & 0x3F;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    nextYaw         = previousYaw + 0x10;
                    nextYaw        += (gRandomLcgState >> 16) & 0x3F;
                }
                work->scale = nextYaw;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = WANDER_MOVE_SPEED_Q4;
            if ((u16)((gRandomLcgState >> 16) % 30) == 0) {
                work->index = WANDER_TIMED_TURN;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_AMBIENT_TURN:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale    -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 240) == 0) {
                work->scale = WANDER_EXIT_YAW;
                work->angle = WANDER_MOVE_SPEED_Q4;
                work->index = WANDER_AMBIENT_MOVE;
            }
            break;
        case WANDER_AMBIENT_MOVE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 7) == 0) {
                work->angle = 0;
                work->index = WANDER_AMBIENT_TURN;
            }
            break;
        case WANDER_DEPART:
            departureYaw = work->scale;
            if (departureYaw > WANDER_EXIT_YAW) {
                randomIncrement = RANDOM_LCG_INCREMENT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + randomIncrement;
                departureYaw   -= 0x10;
                departureYaw   -= (gRandomLcgState >> 16) & 0x3F;
            } else {
                randomIncrement = RANDOM_LCG_INCREMENT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + randomIncrement;
                departureYaw   += 0x10;
                departureYaw   += (gRandomLcgState >> 16) & 0x3F;
            }
            work->scale = departureYaw;
            work->angle = WANDER_DEPART_SPEED_Q4;
            if ((viewGetMappedIndex() & 0xFF) == WANDER_SOUND_VIEW && work->step == WANDER_SOUND_PENDING) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, WANDER_SOUND_SCRIPT), audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->step = WANDER_SOUND_PLAYED;
            }
            break;
    }
    // Rebuild yaw without changing translation, then turn Q12 forward into a Q4-speed step.
    gfxSetRotIdentity(&coord->coord);
    gfxRotMatrixY(&coord->coord, work->scale, GRAPHICS_ROTATION_COMPOSE);
    gte_ReadMatrixColumn(&coord->coord, 2, &work->move);
    work->move.vx       = (work->move.vx * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    work->move.vy       = (work->move.vy * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    work->move.vz       = (work->move.vz * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->period < coord->coord.t[0]) {
        effectKillTask(work, task);
    }
}

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GfxCoord* coord;

    coord                          = task->extra.tmd->coords;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(EFFECT_ACROPOLIS_CAFETERIA_PUFF_EMITTER, coord, 0, NULL);
    }
    return 0;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void acropolisCafeteriaRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void acropolisCafeteriaRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_acropolis_cafeteria_80180C94(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

/// Places a loose model above the player with a positive-Z offset and links its sphere.
///
/// Requires a fresh state-zero TMD task and a live player. Owns one primary-heap
/// work block with six contacts; allocation failure kills the uninitialized task.
/// The exit callback unlinks the sphere before task teardown frees the work.
static void _acropolisCafeteriaLoosePropInit(Task* task)
{
    TmdObject*                        model;
    GfxCoord*                         coord;
    _AcropolisCafeteriaLoosePropWork* work;
    GfxCoord*                         playerCoord;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work         = work;
    task->exitCallback = _acropolisCafeteriaLoosePropExit;
    task->state        = task->state + 1;
    memFillBytes(work, 0, sizeof(*work));
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags        = 0;
    RotMatrix(&work->rotation, &coord->coord);
    work->kickStrength = (rand() & 0xFFF) + 0x3000;
    playerCoord        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    coord->coord.t[0]  = playerCoord->coord.t[0];
    coord->coord.t[1]  = playerCoord->coord.t[1] - 0x800;
    coord->coord.t[2]  = playerCoord->coord.t[2] + 0x800;
    // The body borrows the contacts embedded in the work block for its whole lifetime.
    work->body.context.contacts = work->contacts;
    work->body.key              = 0x50000;
    work->body.radius           = 0xFA;
    work->body.coord            = coord;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PROPS, &work->body);
    worldCollisionInitContacts(work->body.context.contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
}

/// Dampens the loose prop's horizontal velocity and advances its motion phase at rest.
///
/// Requires initialized task work in the hopping or sliding phase. X/Z velocity
/// is in integer world units per tick: retain 6/7, truncating toward zero, then
/// snap magnitudes below 9 to zero. Both axes stopping increments the current
/// phase once, including during a hop; vertical velocity is preserved.
static inline void _acropolisCafeteriaDampLoosePropSlide(_AcropolisCafeteriaLoosePropWork* work)
{
    enum { SLIDE_STOP_SPEED = 9 }; // Exclusive magnitude threshold after damping, in world units per tick.

    work->velocity.vx = (work->velocity.vx * 6) / 7;
    if (ABS(work->velocity.vx) < SLIDE_STOP_SPEED) {
        work->velocity.vx = 0;
    }
    work->velocity.vz = (work->velocity.vz * 6) / 7;
    if (ABS(work->velocity.vz) < SLIDE_STOP_SPEED) {
        work->velocity.vz = 0;
    }
    if ((work->velocity.vx | work->velocity.vz) == 0) {
        work->phase++;
    }
}

/// Sinks the loose prop and handles its contact-driven hop, slide and leveling.
///
/// Requires initialized work and a linked sphere. Position increments are integer
/// world units per tick; Euler angles and the normalized kick direction use 4096
/// units per turn and per unit vector respectively. Every update clears contacts.
static void _acropolisCafeteriaLoosePropUpdate(Task* task)
{
    MATRIX*                           scratchEnd;
    _AcropolisCafeteriaLoosePropWork* work;
    GfxCoord*                         coord;
    SVECTOR*                          kickDirection;
    s32                               launchMagnitude;

    scratchEnd                   = SCRATCH_STACK_CURSOR(MATRIX);
    SCRATCH_STACK_CURSOR(MATRIX) = scratchEnd - 1;
    work                         = task->work;
    coord                        = task->extra.tmd->coords;
    work->kickStrength--;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += 0x80;
    actorRenderComposeCoord(coord);
    switch (work->phase) {
        case ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING:
            if (worldCollisionFindContactIndex(work->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY)) {
                work->phase++;
                // The matrix snapshot is unused; the discarded random draw still advances rand().
                scratchEnd[-1] = coord->coord;
                kickDirection  = &work->kickDirection;
                gfxReadMatrixZAxis(gPlayerStatus.coordMtx, kickDirection);
                VectorNormalSS(kickDirection, kickDirection);
                rand();
                launchMagnitude   = work->kickStrength;
                launchMagnitude >>= 1;
                launchMagnitude   = (launchMagnitude * launchMagnitude) >> 6;
                work->velocity.vy = -0x100;
                work->velocity.vx = (work->kickDirection.vx * launchMagnitude) >> 24;
                work->velocity.vz = (work->kickDirection.vz * launchMagnitude) >> 24;
            }
            break;
        case ACROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING:
            work->velocity.vy += 0x10;
            if (work->velocity.vy > 0) {
                work->velocity.vy = 0;
                work->phase++;
            } else {
                work->rotation.vx += (work->kickStrength >> 6) + (rand() & 0x7F);
                work->rotation.vy += (work->kickStrength >> 6) + (rand() & 0x7F);
                work->rotation.vz += (work->kickStrength >> 6) + (rand() & 0x7F);
            }
            // Fall through: the hop also loses horizontal speed and applies velocity.
        case ACROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING:
            _acropolisCafeteriaDampLoosePropSlide(work);
            coord->coord.t[0] += work->velocity.vx;
            coord->coord.t[1] += work->velocity.vy;
            coord->coord.t[2] += work->velocity.vz;
            break;
        case ACROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING:
            work->rotation.vx = (work->rotation.vx * 2) / 3;
            if (ABS(work->rotation.vx) < 9) {
                work->rotation.vx = 0;
            }
            work->rotation.vz = (work->rotation.vz * 2) / 3;
            if (ABS(work->rotation.vz) < 9) {
                work->rotation.vz = 0;
            }
            if (((u16)work->rotation.vx | (u16)work->rotation.vz) == 0) {
                work->phase = ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING;
            }
            break;
    }
    RotMatrix(&work->rotation, &coord->coord);
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Selects the loose-prop exit handler for the next task dispatch.
static void _acropolisCafeteriaLoosePropRequestExit(Task* task)
{
    enum { LOOSE_PROP_TASK_EXIT = 3 };
    task->state = LOOSE_PROP_TASK_EXIT;
}

/// Unlinks the initialized loose prop's sphere before teardown frees its work and model.
static void _acropolisCafeteriaLoosePropExit(Task* task)
{
    _AcropolisCafeteriaLoosePropWork* work;

    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// State handlers of the loose-prop task: set-up, the per-frame update, a
/// step that moves the task to state 3, and the exit that unlinks and kills it.
static const TaskFuncTable4 D_acropolis_cafeteria_8017D69C = { {
    _acropolisCafeteriaLoosePropInit,
    _acropolisCafeteriaLoosePropUpdate,
    _acropolisCafeteriaLoosePropRequestExit,
    _acropolisCafeteriaLoosePropExit,
} };

void acropolisCafeteriaLoosePropTask(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

void func_acropolis_cafeteria_801827C4(Task* task)
{
    Enemy*     enemy;
    TmdObject* tmd;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    if (Gp_GetCurBit2Flag((u8)enemy->placeKey) != 2) {
        tmd->lightMtx = &D_acropolis_cafeteria_8018D5C0;
        tmd->colorMtx = &D_acropolis_cafeteria_8018D5A0;
        tmd->flags    = 0;
    } else {
        tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    switch (viewGetMappedIndex() & 0xFF) {
        case 0xC:
            tmd->otOffset = 7;
            break;
        case 0x18:
            tmd->otOffset = 4;
            break;
        default:
            tmd->otOffset = -2;
            break;
    }
}
void func_acropolis_cafeteria_8018286C(Task* task)
{
    Enemy*     enemy;
    TmdObject* tmd;
    s32        flag;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    flag  = Gp_GetCurBit2Flag((u8)enemy->placeKey);
    if ((viewGetMappedIndex() & 0xFF) != 9) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    if ((u8)enemy->placeKey == 0xA) {
        gfxRotMatrixX(&task->extra.tmd->coords->coord, 0x400, GRAPHICS_ROTATION_REPLACE);
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D600;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
        taskCallExit(task);
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        tmdAllocPrimitiveBuffer(tmd);
    }
}
static void func_acropolis_cafeteria_80182954(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if ((viewGetMappedIndex() & 0xFF) != 9) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D640;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D620;
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        tmdAllocPrimitiveBuffer(tmd);
    }
    gfxRotMatrixX(&task->extra.tmd->coords->coord, 0x400, GRAPHICS_ROTATION_REPLACE);
}
static void func_acropolis_cafeteria_80182A08(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    switch (viewGetMappedIndex() & 0xFF) {
        case 6:
        case 7:
        case 0xA:
            tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
            tmd->lightMtx = &D_acropolis_cafeteria_8018D680;
            tmd->colorMtx = &D_acropolis_cafeteria_8018D660;
            break;
        default:
            tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    if (Gp_GetCurBit2Flag(0xB) == 2) {
        tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        tmdAllocPrimitiveBuffer(tmd);
    }
}
