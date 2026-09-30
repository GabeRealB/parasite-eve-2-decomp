#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "actors/actor_511000.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/actor_contacts.h"

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern AnimationSet* D_acropolis_helicopter_landing_pad_801838F4[3];

/// 0x20 scratch block `func_acropolis_helicopter_landing_pad_80180A64` takes
/// from the scratch stack for one spark line. `a` / `b` are the two random
/// endpoints, rotated by the coord's `workm` and offset by its translation;
/// `otz` is `SZ3 >> 2` of the second `RTPS`, `flag` the GTE flag word (bit 31
/// rejects the line), and `x0..y1` the two projected screen points.
typedef struct AhlpSparkScratch {
    /* 0x00 */ SVECTOR a;
    /* 0x08 */ SVECTOR b;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ u16     x0;
    /* 0x1A */ u16     y0;
    /* 0x1C */ u16     x1;
    /* 0x1E */ u16     y1;
} AhlpSparkScratch;
STATIC_ASSERT_SIZEOF(AhlpSparkScratch, 0x20);

/// 0x1C scratch block `func_acropolis_helicopter_landing_pad_80181064` takes
/// from the scratch stack for one lens-flare sprite. `pos` is the coord's
/// world translation, `otz` is `SZ3 >> 2` of the `RTPS`, `flag` the GTE flag
/// word (bit 31 rejects the sprite), `sx` / `sy` the projected centre and
/// `dx` / `dy` the rotated half-extents of the quad's two diagonals.
typedef struct AhlpFlareScratch {
    /* 0x00 */ SVECTOR pos;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} AhlpFlareScratch;
STATIC_ASSERT_SIZEOF(AhlpFlareScratch, 0x1C);

extern SVECTOR D_acropolis_helicopter_landing_pad_80184E80[12];
extern s32     D_acropolis_helicopter_landing_pad_80184EE0[12];

static void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0);
static void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0);
static void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level);
static void func_acropolis_helicopter_landing_pad_80180664(GfxCoord* coord);

void func_acropolis_helicopter_landing_pad_8017EB58(Task*);
void func_acropolis_helicopter_landing_pad_8017ED00(Task*);

extern GpGridParams D_acropolis_helicopter_landing_pad_80185998[1];

extern GpObj4C D_acropolis_helicopter_landing_pad_801859BC[16];

GpMsgEntry D_acropolis_helicopter_landing_pad_80183710[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_helicopter_landing_pad_8017E3F0 },
    { 5105, func_acropolis_helicopter_landing_pad_8017E49C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_helicopter_landing_pad_8017E4A4 },
    { 5104, func_acropolis_helicopter_landing_pad_8017E570 },
    { 0x7FFFFFFF, NULL },
};

PadScriptCmd D_acropolis_helicopter_landing_pad_80183738[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 3), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 4), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80183748[2] = {
    { 0, 0, 1, 0 },
    { 255, 255, 4, 1 },
};

ActorTransform D_acropolis_helicopter_landing_pad_80183750 = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183768 = { { 3000, 0, -6692, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183780 = { { 3000, 0, -6692, 0 }, { 0, -1204, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183798 = { { 3000, 0, -6692, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_801837B0 = { { 0, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_801837C8 = { { -3975, -2871, -1454, 0 }, { 0, 808, 0, 0 } };

s32 D_acropolis_helicopter_landing_pad_801837E0[18] = {
    -3496,
    -2871,
    -1283,
    0,
    0x3280000,
    0,
    -360,
    -120,
    480,
    0,
    0,
    0,
    -3188,
    -2871,
    -694,
    0,
    0x9C40000,
    0,
};

ActorTransform D_acropolis_helicopter_landing_pad_80183828 = { { -2176, 0, -6845, 0 }, { 0, 1024, 0, 0 } };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183840 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183854 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183868 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018387C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183890 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838A4 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838B8 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838CC = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838E0 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_acropolis_helicopter_landing_pad_801838F4[3] = {
    NULL,
    &D_actor_511000_80136AE8,
    &D_actor_511000_801398FC,
};

GpCopyArg D_acropolis_helicopter_landing_pad_80183900 = { { .sets = D_acropolis_helicopter_landing_pad_801838F4 }, 3 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183908 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018391C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183930 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183944 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183958 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018396C = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183980 = { 1, 9, 11 };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183988 = { 1, 9, 21 };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183990 = { 1, 10, 11 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183998 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839AC = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839C0 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_acropolis_helicopter_landing_pad_801839D4 = { { .loc = { 1, 16 } }, 0 };

ActorCommand D_acropolis_helicopter_landing_pad_801839D8 = { { .loc = { 1, 16 } }, 1 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839DC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839F0 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_acropolis_helicopter_landing_pad_80183A04[2] = {
    { 15, { .value = 0x51100005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_80183A34[58] = {
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x51100005 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_acropolis_helicopter_landing_pad_80183980 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_80183840 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_acropolis_helicopter_landing_pad_80183738 }, { .padRecords = D_acropolis_helicopter_landing_pad_80183748 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51100006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_helicopter_landing_pad_8017E6C0 }, { .value = 3072 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 43 }, { .value = 43 }, { .value = 45 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_80183854 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_helicopter_landing_pad_80183780 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_80183868 }, { .value = 0 } },
    { 4, { .value = 360 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_8018387C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_80183890 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_helicopter_landing_pad_80183768 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 1 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_80183FA4[16] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_helicopter_landing_pad_80183768 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 1 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_80184124[38] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 49, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 12, { .overlays = &D_acropolis_helicopter_landing_pad_80183988 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183828 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801838A4 }, { .value = 0 } },
    { 4, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E67C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 40 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801839DC }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 20, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51100003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 21, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 2 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E6F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_801844B4[19] = {
    { 20, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 21, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { 1, { .value = -1 }, { .value = 40 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801839F0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E67C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 2 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E6F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_8018467C[69] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_acropolis_helicopter_landing_pad_80183900 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 12, { .overlays = &D_acropolis_helicopter_landing_pad_80183990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1013 }, { .storage = &gGfxViewCoord }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_helicopter_landing_pad_801837C8 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E5E8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_80183998 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 26, { .value = 32 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_helicopter_landing_pad_8017E75C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_helicopter_landing_pad_8017E75C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 26, { .value = 64 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801839AC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801839C0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 1 }, { .value = 1001 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_8018391C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_helicopter_landing_pad_801839D8 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_helicopter_landing_pad_80183750 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801838CC }, { .value = 0 } },
    { 4, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183930 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_helicopter_landing_pad_801838E0 }, { .value = 0 } },
    { 4, { .value = 350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E5B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x313A0003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x313A0003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x313A0004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 1 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x313A0004 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E64C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_helicopter_landing_pad_80184CF4[7] = {
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_helicopter_landing_pad_8017E64C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_acropolis_helicopter_landing_pad_80184D9C = 0;

TaskDesc D_acropolis_helicopter_landing_pad_80184DA0[9] = {
    { 0, 32, func_acropolis_helicopter_landing_pad_8017DA9C, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017E76C, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017E81C, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017DE78, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017E974, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017DFCC, { .model = NULL } },
    { 0, 32, func_acropolis_helicopter_landing_pad_8017E0F8, { .model = NULL } },
    { 0, 97, func_acropolis_helicopter_landing_pad_8017E270, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_acropolis_helicopter_landing_pad_80184E0C = 0;

s32 D_acropolis_helicopter_landing_pad_80184E10[6] = {
    0,
    1,
    0,
    0,
    0,
    0,
};

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E28 = { 0 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E3C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_helicopter_landing_pad_80184E50 = { { -6801, 0, -1998, 0 }, { 0, 1024, 0, 0 } };

TaskDesc D_acropolis_helicopter_landing_pad_80184E68[2] = {
    { 0, 192, func_acropolis_helicopter_landing_pad_8017ED00, { .model = NULL } },
    { 0, 192, func_acropolis_helicopter_landing_pad_8017EB58, { .model = NULL } },
};

SVECTOR D_acropolis_helicopter_landing_pad_80184E80[12] = {
    { -7050, -2800, -7030, 0 },
    { -2800, -3000, -7760, 0 },
    { 2800, -3000, -7760, 0 },
    { 7050, -2800, -7030, 0 },
    { 7760, -3000, -2800, 0 },
    { 7760, -3000, 2800, 0 },
    { 7050, -2800, 7030, 0 },
    { 2800, -3000, 7760, 0 },
    { -2800, -3000, 7760, 0 },
    { -7050, -2800, 7030, 0 },
    { -7760, -3000, 2800, 0 },
    { -7760, -3000, -2900, 0 },
};

s32 D_acropolis_helicopter_landing_pad_80184EE0[12] = {
    0x4014404,
    0x4014000,
    8,
    0x8818,
    2064,
    0xC2830,
    0xC2910,
    0x2C2980,
    0x200580,
    0x200700,
    0x200600,
    0x20400,
};

GpRoomObjRec D_acropolis_helicopter_landing_pad_80184F10[1] = {
    { D_acropolis_helicopter_landing_pad_80185998, D_acropolis_helicopter_landing_pad_801859BC, D_acropolis_helicopter_landing_pad_80185E7C, D_acropolis_helicopter_landing_pad_80186128 },
};

u8 D_acropolis_helicopter_landing_pad_80184F20[28] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    0,
};

u8* D_acropolis_helicopter_landing_pad_80184F3C[1] = {
    D_acropolis_helicopter_landing_pad_80184F20,
};

GpViewCountRec D_acropolis_helicopter_landing_pad_80184F40[1] = {
    { { .bytes = { 27, 0 } } },
};

GpRoomCoordRec D_acropolis_helicopter_landing_pad_80184F44[1] = {
    { D_acropolis_helicopter_landing_pad_80186AE8, NULL },
};

GpWarpRec D_acropolis_helicopter_landing_pad_80184F4C[3] = {
    { { .words = { 2048, -6387, 1, -6306 } }, { 0, 0, 0, 0 }, { .words = { 2048, -6387, 1, -6306 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 1024, -5340, -3001, -1921 } }, { 0, 0, 0, 0 }, { .words = { 1024, -5340, -3001, -1921 } }, { 0, 0, 0, 0 }, 0, 0, 0, 11, 0, 0 },
    { { .words = { 2048, -6400, 600, -5280 } }, { 0, 0, 0, 0 }, { .words = { 2048, -6387, 451, -5400 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 1, 0 },
};

SVECTOR D_acropolis_helicopter_landing_pad_80184FF4[15] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_normals.inc"
};

SVECTOR D_acropolis_helicopter_landing_pad_8018506C[92] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_verts.inc"
};

GpGridFace D_acropolis_helicopter_landing_pad_8018534C[37] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_faces.inc"
};

s16 D_acropolis_helicopter_landing_pad_80185508[512] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_helicopter_landing_pad_80185508[i])
s16* D_acropolis_helicopter_landing_pad_80185908[36] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_table.inc"
};
#undef GRID_CELL

GpGridParams D_acropolis_helicopter_landing_pad_80185998[1] = {
    { NULL, D_acropolis_helicopter_landing_pad_80184FF4, D_acropolis_helicopter_landing_pad_8018506C, D_acropolis_helicopter_landing_pad_8018534C, D_acropolis_helicopter_landing_pad_80185908, 0x2710, 0x2710, 6, 6, 4000, 37 },
};

GpObj4C D_acropolis_helicopter_landing_pad_801859BC[16] = {
    { NULL, NULL, NULL, { -5505, 0, -5857, 0 }, { { -473, 6080, -1561, 0 }, { 474, 6080, 1562, 0 }, { -473, -6080, -1561, 0 }, { 474, -6080, 1562, 0 } }, { -3931, 0, 1191, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -6016, 0, -5888, 0 }, { { 395, 6080, 1582, 0 }, { -397, 6080, -1584, 0 }, { 395, -6080, 1582, 0 }, { -397, -6080, -1584, 0 } }, { 3984, 0, -998, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -1985, 0, -5921, 0 }, { { 155, 6080, 1619, 0 }, { -165, 6080, -1627, 0 }, { 155, -6080, 1619, 0 }, { -165, -6080, -1627, 0 } }, { 4085, 0, -403, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1536, 0, -6112, 0 }, { { -161, 6080, -1626, 0 }, { 157, 6080, 1621, 0 }, { -161, -6080, -1626, 0 }, { 157, -6080, 1621, 0 } }, { -4087, 0, 400, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 3550, 0, -5857, 0 }, { { 1224, 6080, -1401, 0 }, { -1223, 6080, 1401, 0 }, { 1224, -6080, -1401, 0 }, { -1223, -6080, 1401, 0 } }, { -3090, 0, -2698, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 3007, 0, -6113, 0 }, { { -1515, 6080, 1692, 0 }, { 1515, 6080, -1691, 0 }, { -1515, -6080, 1692, 0 }, { 1515, -6080, -1691, 0 } }, { 3053, 0, 2735, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 6048, 0, -1152, 0 }, { { 1860, 6080, -35, 0 }, { -1860, 6080, 34, 0 }, { 1860, -6080, -35, 0 }, { -1860, -6080, 34, 0 } }, { -77, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 6144, 0, -1729, 0 }, { { -1860, 6080, 34, 0 }, { 1860, 6080, -35, 0 }, { -1860, -6080, 34, 0 }, { 1860, -6080, -35, 0 } }, { 75, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 5760, 0, 3359, 0 }, { { -1624, 6080, 907, 0 }, { 1624, 6080, -907, 0 }, { -1624, -6080, 907, 0 }, { 1624, -6080, -907, 0 } }, { 1999, 0, 3580, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 5952, 0, 3840, 0 }, { { 1666, 6080, -827, 0 }, { -1667, 6080, 826, 0 }, { 1666, -6080, -827, 0 }, { -1667, -6080, 826, 0 } }, { -1823, 0, -3675, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 2655, 0, 6143, 0 }, { { 304, 6080, 1833, 0 }, { -310, 6080, -1836, 0 }, { 304, -6080, 1833, 0 }, { -310, -6080, -1836, 0 } }, { 4044, 0, -677, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 3231, 0, 6047, 0 }, { { -310, 6080, -1836, 0 }, { 304, 6080, 1832, 0 }, { -310, -6080, -1836, 0 }, { 304, -6080, 1832, 0 } }, { -4044, 0, 676, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { -4193, 0, 6111, 0 }, { { 124, 6080, 1855, 0 }, { -126, 6080, -1856, 0 }, { 124, -6080, 1855, 0 }, { -126, -6080, -1856, 0 } }, { 4091, 0, -276, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { -3939, 0, 6079, 0 }, { { -216, 6080, -1848, 0 }, { 215, 6080, 1846, 0 }, { -216, -6080, -1848, 0 }, { 215, -6080, 1846, 0 } }, { -4073, 0, 474, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { -6098, 0, 3342, 0 }, { { -1437, 6080, 1741, 0 }, { 1437, 6080, -1740, 0 }, { -1437, -6080, 1741, 0 }, { 1437, -6080, -1740, 0 } }, { 3159, 0, 2608, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 9, 10, 1, 0 },
    { NULL, NULL, NULL, { -5856, 0, 4064, 0 }, { { 1437, 6080, -1741, 0 }, { -1437, 6080, 1740, 0 }, { 1437, -6080, -1741, 0 }, { -1437, -6080, 1740, 0 } }, { -3161, 0, -2610, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 10, 9, 129, 0 },
};

static void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0);
static void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0);
static void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017EB58(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->location;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot((u8*)&key, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    SetDispMask(1);
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_acropolis_helicopter_landing_pad_8017ED00(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_helicopter_landing_pad_80184E68, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Asks the slot-7 task to warp to stage 0xF, room 3 (message 0x13EE with the
/// room's `RoomEventMsg`); advances on success, otherwise kills the task.
static void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0)
{
    Task* slot = gameGetPtrSlot(7);

    D_acropolis_helicopter_landing_pad_80187F90.field_4   = 1;
    D_acropolis_helicopter_landing_pad_80187F90.room      = 1;
    D_acropolis_helicopter_landing_pad_80187F90.areaId    = 0xF;
    D_acropolis_helicopter_landing_pad_80187F90.warp      = 3;
    D_acropolis_helicopter_landing_pad_80187F90.queryOnly = ROOM_EVENT_EXECUTE;
    if (Gp_DispatchMsgPtrs(slot, ROOM_EVENT_MESSAGE_RESOLVE, &D_acropolis_helicopter_landing_pad_80187F90,
                           &D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        arg0->state += 1;
    } else {
        taskKill(arg0);
    }
}

static void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0)
{
    ActorTransform msg;
    Task*          slot;

    slot       = gameGetPtrSlot(3);
    msg.rot.vx = 0;
    msg.rot.vy = 0;
    msg.rot.vz = 0;
    Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
    arg0->state = arg0->state + 1;
}

/// Task state step: advances the state once msg 0x3F0 to slot 3 returns 0.
static void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

static void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0)
{
    GpFacingArg args;
    Task*       slot;

    slot         = gameGetPtrSlot(3);
    args.field_0 = 1;
    args.field_4 = 3;
    Gp_DispatchMsgPtr(slot, 0x3EF, &args, 0);
    arg0->state = arg0->state + 1;
}

static void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = (u8)D_acropolis_helicopter_landing_pad_80187F90.areaId;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_acropolis_helicopter_landing_pad_80187F90.warp;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_acropolis_helicopter_landing_pad_80187F90.room;
        Task_Spawn(0, 0x11, 0, 0);
        taskKill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EF60(s32 unused0, s32 unused1)
{
    Task_Spawn(2, 0xF, 0, 0);
}

/// Five-state dispatcher of the room's intro task; the handler table is built
/// on the stack. Marks the player actor's `field_930` as 2 before every step.
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0)
{
    GameActor* actor     = (GameActor*)(gameGetPtrSlot(3))->work;
    TaskFunc   states[5] = {
        func_acropolis_helicopter_landing_pad_8017ED50,
        func_acropolis_helicopter_landing_pad_8017EDD4,
        func_acropolis_helicopter_landing_pad_8017EE2C,
        func_acropolis_helicopter_landing_pad_8017EE80,
        func_acropolis_helicopter_landing_pad_8017EEDC,
    };

    actor->field_930 = 2;
    states[arg0->state](arg0);
}

/// Draws one helipad floodlight glow. Light `index` owns transient light slot
/// `6 + (index & 1)`; the light is skipped while `gRoomEffectState->effectControl` is
/// non-zero (switching the slot off once it reaches 4) and unless the
/// current view's bit is set in the light's
/// `D_acropolis_helicopter_landing_pad_80184EE0` mask. Otherwise `pos` is
/// projected through `gGfxViewCoord.workm` into a scratch stack block and,
/// when the GTE flag word is clean, the record is refreshed and two rings of
/// flat-shaded `POLY_G4` fans are linked into the OT at the light's `otz`: 16
/// wedges of the outer radius (a dim `level >> 1` layer under a `level` one)
/// and four inner-radius blades whose intensity is `level >> 1`.
static void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level)
{
    GpCoord64*            light;
    WorldCoordPointLight* work;
    void**                scratch;
    u8*                   head;
    RoomDraw05Scratch*    blk;
    POLY_G4*              prim;
    s32                   a;
    s32                   b;
    s32                   c;
    s32                   d;
    s16                   lvl;
    s32                   half;
    s32                   mask;

    lvl   = level;
    light = &Gp_RoomCoords[6 + (index & 1)];
    work  = &light->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            light->framesLeft = 0;
        }
    } else {
        mask = D_acropolis_helicopter_landing_pad_80184EE0[index] & (1 << ((Gp_GetViewIndex() & 0xFF) - 1));
        if (mask == 0) {
            return;
        }
        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        *scratch = head - 0x14;
        blk      = (RoomDraw05Scratch*)(head - 0x14);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(pos);
        gte_rtps();
        gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
        gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            light->framesLeft                              = 2;
            work->inner                                    = 0x640;
            work->outer                                    = 0x3200;
            work->head.color.r                             = level * 16;
            work->head.color.g                             = 0;
            work->head.color.b                             = 0;
            work->head.transform.lighting.local.t[0]       = pos->vx;
            work->head.transform.lighting.local.t[1]       = pos->vy;
            work->head.transform.lighting.local.t[2]       = pos->vz;
            light->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            blk->rOuter                                    = 0xC000 / ((RoomDraw05Scratch*)(head - 0x14))->otz;
            blk->rInner                                    = 0x1800 / ((RoomDraw05Scratch*)(head - 0x14))->otz;

            for (a = 0; a < 0x1000; a += 0x200) {
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                half = lvl >> 1;
                setRGB2(prim, half, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * rcos(a)) >> 12);
                b        = a + 0x100;
                prim->x1 = blk->sx + ((blk->rOuter * rsin(b)) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(b)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                c        = a + 0x200;
                prim->x3 = blk->sx + ((blk->rOuter * rsin(c)) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * rcos(c)) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * rsin(a)) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * rcos(a)) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(b)) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(b)) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * rsin(c)) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * rcos(c)) >> 13);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }

            lvl = half;
            for (a = 0x200; a < 0x1000; a += 0x800) {
                d              = a - 0x400;
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * rsin(d)) >> 13);
                prim->y0 = blk->sy + ((blk->rInner * rcos(d)) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(a)) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(a)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x400;
                prim->x3 = blk->sx + ((blk->rInner * rsin(d)) >> 13);
                prim->y3 = blk->sy + ((blk->rInner * rcos(d)) >> 13);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->rInner * rcos(a)) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(d)) >> 11);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(d)) >> 11);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x800;
                prim->x3 = blk->sx + ((blk->rInner * rsin(d)) >> 12);
                prim->y3 = blk->sy + ((blk->rInner * rcos(d)) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x14);
    }
}

/// Draws one helipad ember / spark sprite. `spawnArg1` non-zero spawns the
/// bright variant (`scale` 0x300..0x3FF, no drift beyond a fixed -0x18 on
/// Y, `step` 2..5 with `period` up to 0x3F); zero spawns the dim one
/// (`scale` 0x100..0x1FF, random 3D drift, `step` / `period` 1..4).
/// The sprite lives `step * 6` frames counted in `age`. Each frame
/// the coord's translation is projected through `GsWSMATRIX` into a
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x4383, one of the 32x32
/// cells on row 0x28) whose corners are the projected centre plus / minus
/// `scale * 31 / otz` rotated by `angle` and `angle + 0x400`. A
/// bright sprite (`spawnArg1 == 1`) flickers a random green / blue-white tint
/// on 1-in-4 LCG rolls and, before its last two frames, fires a 0x600E0
/// effect on 1-in-16. While `gRoomEffectState->effectControl` is 0 the coord drifts,
/// `scale` grows by `period` and the frame counter advances until it
/// expires, which releases the state-1C memory; `field_4 >= 4` releases it at
/// once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    u32               tmp;
    s16               n;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            if (arg0->spawnArg1.value != 0) {
                mem->move.vx = 0;
                mem->move.vy = -0x18;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x300;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->period  = ((u32)Gp_LcgState >> 16) & 0x3F;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 2;
            } else {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x100;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 7) - 4;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = ~(((u32)Gp_LcgState >> 16) & 0xF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = (((u32)Gp_LcgState >> 16) & 7) - 4;
            }
            arg0->state++;
        }
        scratch     = SCRATCH_STACK_CURSOR_SLOT;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            if (arg0->spawnArg1.value == 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
                    setRGB0(prim, tmp >> 1, tmp, 0xFF);
                } else {
                    prim->code |= 1;
                }
                if (mem->age < mem->step * 6 - 2) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                }
            } else {
                prim->code = 0x2D;
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 2;
            prim->u0    = (mem->age / mem->step + 1) * 0x20;
            prim->v0    = 0x28;
            prim->u1    = (mem->age / mem->step + 1) * 0x20 + 0x1F;
            prim->v1    = 0x28;
            prim->u2    = (mem->age / mem->step + 1) * 0x20;
            prim->v2    = 0x47;
            prim->u3    = (mem->age / mem->step + 1) * 0x20 + 0x1F;
            prim->v3    = 0x47;
            blk->dx     = ((mem->scale * 0x1F / blk->otz) * rsin(mem->angle)) >> 12;
            blk->dy     = ((mem->scale * 0x1F / blk->otz) * rcos(mem->angle)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->scale * 0x1F / blk->otz) * rsin(mem->angle + 0x400)) >> 12;
            blk->dy     = ((mem->scale * 0x1F / blk->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
        SCRATCH_STACK_RELEASE_BYTES(0x1C);
        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            mem->scale         += mem->period;
            n                   = mem->age + 1;
            mem->age            = n;
            if (n > mem->step * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Effect task for the helipad floodlights anchored to `Gp_RoomCoords[4]` and
/// `[5]`. On first run it parents the coord to the work's `parent` and
/// positions it from `pos`. State 0 rolls 0-3 spawns of
/// `func_acropolis_helicopter_landing_pad_80180664`, a 1-in-4 roll of
/// `func_acropolis_helicopter_landing_pad_80180A64`, and claims slot 4 as a
/// light (refcount 4). State 1 (also reached by fallthrough) rearms
/// `scale` on a 1-in-4 roll every 8th frame; when armed it plays sound
/// `0x51100001` panned at the coord, spawns one 0x6003B and six 0x600A4
/// effects reparented under this task, and claims slot 5 as a light. State 2
/// releases the state-1C memory, the only step taken while
/// `gRoomEffectState->effectControl` is set.
void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0)
{
    EffectWork*           mem;
    GfxCoord*             coord;
    GpCoord64*            base;
    WorldCoordPointLight* slot;
    EffectWork*           eff;
    s32                   i;
    s32                   n;
    s32                   pan;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (arg0->state == 2) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (mem->index == 0) {
        coord->parent       = mem->parent;
        coord->coord.t[0]   = mem->pos.vx;
        coord->coord.t[1]   = mem->pos.vy;
        coord->coord.t[2]   = mem->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        mem->scale = 1;
        mem->index++;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            n           = ((u32)Gp_LcgState >> 16) & 3;
            for (i = 0; i < n; i++) {
                func_acropolis_helicopter_landing_pad_80180664(coord);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    func_acropolis_helicopter_landing_pad_80180A64(coord);
                }
            }
            base               = &Gp_RoomCoords[4];
            slot               = &base->light;
            base->framesLeft   = 4;
            slot->inner        = 0x15E0;
            slot->outer        = 0x1900;
            slot->head.color.r = 0x800;
            slot->head.color.g = 0x800;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            slot->head.color.b = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &base->light.head.transform.coord.coord);
            base->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            /* fallthrough */
        case 1:
            if ((gDisplayState.animFrame & 7) == 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    mem->scale = 1;
                }
            }
            if (mem->scale != 0) {
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x51100001, pan, (s8)gpGetObjDepth(coord));
                mem->scale = 0;
                eff        = Gp_SpawnEff(0x6003B, coord, 0x200, NULL);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
                for (i = 0; i < 6; i++) {
                    eff = Gp_SpawnEff(0x600A4, coord, 1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                base               = &Gp_RoomCoords[5];
                slot               = &base->light;
                base->framesLeft   = 4;
                slot->inner        = 0xFA0;
                slot->outer        = 0x12C0;
                slot->head.color.r = 0xC00;
                slot->head.color.g = 0xC00;
                slot->head.color.b = 0x600;
                Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &base->light.head.transform.coord.coord);
                base->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            }
            break;
        case 2:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Draws one random spark line off the floodlight coord, the same shape as
/// `func_acropolis_helicopter_landing_pad_80180A64` with a different box:
/// `a` is rolled 64 wide and 128 tall hanging 0xC0..0x41 below the coord,
/// `b` is centred (128 wide, 255 tall via an LCG modulo). Both are rotated by
/// the coord's `workm`, offset by its translation and projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
static void func_acropolis_helicopter_landing_pad_80180664(GfxCoord* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = SCRATCH_STACK_CURSOR_SLOT;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0xC0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = (((u32)Gp_LcgState >> 16) % 0xFF) - 0x80;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = (u16)blk->b.vx + (u16)coord->workm.t[0];
    blk->b.vy = (u16)blk->b.vy + (u16)coord->workm.t[1];
    blk->b.vz = (u16)blk->b.vz + (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Draws one random spark line off the floodlight coord: two endpoints are
/// rolled from the LCG (`a` in a 64x128x64 box, `b` in 64x256x64), rotated by
/// the coord's `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
void func_acropolis_helicopter_landing_pad_80180A64(GfxCoord* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = SCRATCH_STACK_CURSOR_SLOT;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = ((u32)Gp_LcgState >> 16) & 0x7F;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = ((u32)Gp_LcgState >> 16) & 0xFF;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = (u16)blk->b.vx + (u16)coord->workm.t[0];
    blk->b.vy = (u16)blk->b.vy + (u16)coord->workm.t[1];
    blk->b.vz = (u16)blk->b.vz + (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Effect task for the helipad beacon anchored to `Gp_RoomCoords[4]`. State 0
/// spawns two 0x6005E effects, takes the slot (refcount 4) and seeds its
/// light parameters from the coord and an LCG draw; state 1 spawns two more
/// with arg 0; state 2 fires a 0x6005A effect on 1-in-16 LCG rolls every
/// 64th frame; state 3 releases the state-1C memory. Idle while
/// `gRoomEffectState->effectControl` is set.
void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0)
{
    EffectWork*           mem;
    GfxCoord*             coord;
    GpCoord64*            base;
    WorldCoordPointLight* slot;

    base  = &Gp_RoomCoords[4];
    slot  = &base->light;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (arg0->state == 3) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING && arg0->state < 3) {
        return;
    }
    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            base->framesLeft                              = 4;
            slot->inner                                   = 0x1900;
            slot->outer                                   = 0x1C20;
            slot->head.color.r                            = 0x800;
            slot->head.color.g                            = 0x800;
            Gp_LcgState                                   = Gp_LcgState * 5 + 0x71357911;
            slot->head.color.b                            = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            slot->head.transform.coord.coord.t[0]         = coord->coord.t[0];
            slot->head.transform.coord.coord.t[1]         = coord->coord.t[1];
            slot->head.transform.coord.coord.t[2]         = coord->coord.t[2];
            base->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 1:
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            break;
        case 2:
            if (gDisplayState.animFrame & 0x40) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2, NULL);
                }
            }
            break;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task for one helipad lens flare. State 0 seeds the `EffectWork`
/// from the LCG: a 0x200..0x3FF radius (`scale`), a 12-bit angle
/// (`angle`), a 1..4 lifetime scale (`step`, the flare lives
/// `step * 6` frames counted in `age`) and a per-frame drift
/// (`move` / `move.vy` / `move.vz`). Each frame the coord's translation
/// is projected through `GsWSMATRIX` into a semi-transparent `POLY_FT4`
/// (tpage 0x2B, clut 0x4384, one of `step` 40x40 cells on row 0x48)
/// whose four corners are the projected centre plus / minus
/// `scale * 39 / otz` rotated by `angle` and `angle + 0x400`. The
/// last eight frames fade to grey; before that a spawned flare
/// (`spawnArg1`) flickers a random green / blue-white tint on 1-in-4 LCG rolls
/// and fires a 0x600E0 effect on 1-in-16, and every flare fires 0x6005A on
/// 1-in-16. While `gRoomEffectState->effectControl` is 0 the coord drifts and the frame
/// counter advances until it expires, which releases the state-1C memory;
/// `field_4 >= 4` releases it at once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_80181064(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    s32               span;
    s32               n;
    s32               lvl;
    u8                tmp;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = -(((u32)Gp_LcgState >> 16) & 0x1F) - 0x40;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            arg0->state++;
        }
        scratch     = SCRATCH_STACK_CURSOR_SLOT;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            span = mem->step * 6;
            n    = mem->age;
            if (span - 8 < n) {
                lvl = (span - n + 1) * 16;
                setRGB0(prim, lvl, lvl, lvl);
            } else {
                if (arg0->spawnArg1.value != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        tmp         = (u32)Gp_LcgState >> 16;
                        setRGB0(prim, tmp >> 1, tmp, 0xFF);
                    } else {
                        prim->code |= 1;
                    }
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                } else {
                    prim->code = 0x2D;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    Gp_SpawnEff(0x6005A, coord, 2 - arg0->spawnArg1.value, NULL);
                }
            }
            prim->tpage = 0x2B;
            prim->code |= 2;
            prim->clut  = 0x4384;
            prim->u0    = (mem->age / mem->step) * 0x28;
            prim->v0    = 0x48;
            prim->u1    = (mem->age / mem->step) * 0x28 + 0x27;
            prim->v1    = 0x48;
            prim->u2    = (mem->age / mem->step) * 0x28;
            prim->v2    = 0x6F;
            prim->u3    = (mem->age / mem->step) * 0x28 + 0x27;
            prim->v3    = 0x6F;
            blk->dx     = ((mem->scale * 0x27 / blk->otz) * rsin(mem->angle)) >> 12;
            blk->dy     = ((mem->scale * 0x27 / blk->otz) * rcos(mem->angle)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->scale * 0x27 / blk->otz) * rsin(mem->angle + 0x400)) >> 12;
            blk->dy     = ((mem->scale * 0x27 / blk->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
        SCRATCH_STACK_RELEASE_BYTES(0x1C);
        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            mem->age++;
            if (mem->age > mem->step * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Per-frame driver of the twelve helipad lights. Flags `gRoomEffectState->groundShadowShade`
/// while view 0x12 is active, folds the frame counter `gDisplayState.animFrame * 4` into a
/// 0..0xFE triangle wave kept in the effect work's `scale` (the low two bits
/// are dropped on the rising half so the ramp steps in fours), then runs
/// `func_acropolis_helicopter_landing_pad_8017F010` once per light position.
void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0)
{
    EffectWork* work = (EffectWork*)arg0->spawnArg2.pointer;
    SVECTOR*    pos;
    s32         i;
    s32         v;
    s32         level;

    if ((Gp_GetViewIndex() & 0xFF) == 0x12) {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_DISABLED;
    } else {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    }

    v           = gDisplayState.animFrame << 2;
    work->scale = v;
    if (v & 0x80) {
        level = 0x7F - (v & 0x7F);
    } else {
        level = v & 0x7C;
    }
    work->scale = level * 2;

    i   = 0;
    pos = D_acropolis_helicopter_landing_pad_80184E80;
    for (; i < 12; i++) {
        func_acropolis_helicopter_landing_pad_8017F010(pos++, i, work->scale);
    }
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Task step of an item-pickup model: when the item's 2-bit flag reads 2 it
/// sets `TMD_OBJECT_SKIP_AUTO_BUFFER`, otherwise it selects the flagged draw
/// pass, clears the draw offset and allocates the buffers. The view index is
/// fetched and ignored.
void func_acropolis_helicopter_landing_pad_801822B0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
