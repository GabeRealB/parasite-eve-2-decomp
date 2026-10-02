#include "rooms/shelter_b3_dumping_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/strings.h>

#include "common.h"
#include "gte.h"

#include "shelter_b3_dumping_hole_private.h"

#include "actors/actors_shared_801673f8.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// Selects this room's billboard declaration with `s32` frame and size arguments.
///
/// Presence-only configuration for the first inclusion of `effect_sprite.h`;
/// the replacement value is unused. Leave the halfword argument flag undefined
/// and undefine this flag after the header. The drawer narrows frame to `u16`
/// and size to `s16` internally; the call signature remains word-sized.
#define EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS
#include "../../shared/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS

#include "../../shared/actor_messages.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

#define DUMPING_HOLE_RAND() ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16))

/// Full sprite-work allocation and clear extent; the seed and work types are partial views.
enum { SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES = 0x24 };

/// Spawns one debris task and gives it a work block seeded with `seed`.
#define DUMPING_HOLE_SPAWN_DEBRIS(seed)                                                              \
    {                                                                                                \
        Task*                  t = Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 0, 0, 0); \
        DumpingHoleDebrisSeed* w = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);      \
        t->work                  = w;                                                                \
        if (w == NULL) {                                                                             \
            taskKill(t);                                                                             \
        } else {                                                                                     \
            memFillBytes(w, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);                           \
            *w = seed;                                                                               \
        }                                                                                            \
    }

extern SVECTOR D_shelter_b3_dumping_hole_8018B86C[44];

static void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2);
#include "../../shared/cap_captions.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4D4[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4D4_value __asm__("D_shelter_b3_dumping_hole_8018F4D4");

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
static u8 CapCaption_Data_8015E66C[4];
// Scalar symbol view preserves the original byte/halfword address formation.

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4B0[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4B0_value __asm__("D_shelter_b3_dumping_hole_8018F4B0");

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*            sets[4];
        AnimationBankCopyRequest copy;
    } data;
    s32 words[6];
} ShelterB3DumpingHoleAnimStorageAFC8;
STATIC_ASSERT_SIZEOF(ShelterB3DumpingHoleAnimStorageAFC8, 24);

extern ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8;

typedef struct {
    s32   field_0;
    s32   field_4;
    s32   field_8;
    u8    pad_0C[0x4];
    s16   field_10;
    s16   field_12;
    s16   field_14;
    u8    pad_16[0xE];
    Task* field_24;
    Task* field_28;
    Task* field_2C;
    u16   field_30;
    u16   field_32;
    u16   field_34;
    u8    pad_36[0x2];
    u16   field_38;
    u16   field_3A;
    u16   field_3C;
    u8    pad_3E[0x2];
    s16   field_40;
    u16   field_42;
    s16   field_44;
    s16   field_46;
    s16   field_48;
    s16   field_4A;
    u16   field_4C;
} DumpingHoleEntity;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    u8  pad_6[0x2];
    s16 field_8;
    u8  pad_A[0x2];
    s16 field_C;
    s16 field_E;
    s16 field_10;
    u8  pad_12[0x2];
    s16 field_14;
    s16 field_16;
    s16 field_18;
    u8  pad_1A[0x2];
    s16 field_1C;
    u16 field_1E;
    u16 field_20;
} DumpingHoleAnimWork;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    s16 field_6;
    s16 field_8;
    s16 field_A;
} DumpingHoleAnimFrame;

typedef struct {
    MATRIX field_0;
    MATRIX field_20;
    u16    rotX; // Accumulated rotation about X, advanced by `spinX` each frame
    u16    rotY; // Accumulated rotation about Y, advanced by `spinY` each frame
    u16    rotZ; // Accumulated rotation about Z; advanced but never applied
    u8     pad_46[0x2];
    s16    velX; // Per-frame translation added to the coordinate
    s16    velY;
    s16    velZ;
    u8     pad_4E[0x2];
    s16    spinX; // Per-frame rotation step, chosen at random at launch
    s16    spinY;
    s16    spinZ;
    u8     pad_56[0x2];
    u16    fall; // Downward speed added to `velY`, growing by 5 each frame
    u8     pad_5A[0x2];
} DumpingHoleCoordWork;

/// Stack block for projecting a point through `GsWSMATRIX`: the point, then
/// the screen position and depth the projection writes back.
typedef struct {
    SVECTOR pos;
    s16     sx;
    s16     sy;
    s32     otz;
} DumpingHoleProjection;

/// End marker in the X coordinate of the room's model placement tables.
enum { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END = 0xFFFF };

/// An entry of the third spawn table: a position, and a flag asking for a
/// ring of debris tasks around it. A 0xFFFF `x` ends the table.
typedef struct {
    s32 x;
    s32 y;
    s32 z;
    u8  pad_C[0xC];
    u16 field_18;
} DumpingHoleDebrisEntry;

/// The leading bytes of a debris task's work block, copied in whole.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 pad_6;
    s16 field_8;
    s16 field_A;
} DumpingHoleDebrisSeed;

typedef struct {
    u8  pad_00[0xC];
    s16 field_C;
    s16 field_E;
    s16 field_10;
} DumpingHoleSpawnWork;

/// Spawn record for a falling shard: where it starts relative to `parent`, its
/// base velocity, its size, and the downward speed it gains each frame.
typedef struct {
    SVECTOR   pos;
    SVECTOR   vel;
    GfxCoord* parent;
    u16       size;
    u16       fall;
} DumpingHoleShardCfg;

/// Work block of a falling shard: its current rotation and spin, its velocity,
/// the three corners of the triangle it draws, and the per-frame fall speed.
typedef struct {
    SVECTOR rot;
    SVECTOR rotSpeed;
    SVECTOR vel;
    SVECTOR verts[3];
    u16     fall;
} DumpingHoleShard;

typedef struct {
    MATRIX         lightMtx;
    MATRIX         colorMtx;
    ActorTransform pose;     // Sent to the task itself with message 0x7D4
    SVECTOR        field_58; // Spawn parameters handed by address to the table spawns
    SVECTOR        field_60;
    GfxCoord*      field_68;
    s16            field_6C;
    s16            field_6E;
    VECTOR         scale; // Per-axis scale applied to the rotation of model part 3
    Task*          field_80;
    Task*          field_84;
    Task*          field_88;
    u16            state; // One-shot command, cleared once handled
    u16            step;  // Progress through the sequence the command started
    u16            timer; // Frames spent in the current step
    u8             pad_92[0x2];
    s16            field_94;
    s16            field_96;
    s16            field_98; // X rotation of model part 1 once the sequence reaches step 2
    s16            field_9A;
    u16            field_9C; // Latch: set once the state-F0 release has been issued
    u8             pad_9E[0x2];
} DumpingHoleEntity4;

typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u8  r;
    /* 0x05 */ u8  g;
    /* 0x06 */ u8  b;
    /* 0x07 */ u8  code;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ u8  field_14;
    /* 0x15 */ u8  field_15;
    /* 0x16 */ u8  field_16;
    /* 0x17 */ u8  pad_17;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ s16 field_1A;
} Prim82AA0;

/// The encounter's enemy slots, in the order the controller starts them.
extern OverlayEncounterSlot D_shelter_b3_dumping_hole_8018B7BC[];

extern TaskDesc D_shelter_b3_dumping_hole_80188C04[];
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8[];

extern Task*                  D_shelter_b3_dumping_hole_8018F4A8;
extern s16                    D_shelter_b3_dumping_hole_80188154[];
extern DumpingHoleAnimFrame   D_shelter_b3_dumping_hole_801880B8[];
extern s16                    D_shelter_b3_dumping_hole_8018816C[];
extern s16                    D_shelter_b3_dumping_hole_80188184[];
extern s32                    D_shelter_b3_dumping_hole_8018819C[];
extern ActorTransform         D_shelter_b3_dumping_hole_801881CC;
extern ActorTransform         D_shelter_b3_dumping_hole_801881E4;
extern ActorTransform         D_shelter_b3_dumping_hole_801881FC[];
extern ActorTransform         D_shelter_b3_dumping_hole_80188304[];
extern DumpingHoleDebrisEntry D_shelter_b3_dumping_hole_801884CC[];

extern EvsCommand            D_shelter_b3_dumping_hole_80188640[];
extern EvsCommand            D_shelter_b3_dumping_hole_80188A78[];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018ECA4[10];
extern Task*                 D_shelter_b3_dumping_hole_8018F4AC;
extern ActorTransform        D_shelter_b3_dumping_hole_8018966C;

extern s32 D_shelter_b3_dumping_hole_8018F4D8;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, ActorCommand* request);
        void (*call1)(Task*, s32, ActorTransform*);
        void (*call2)(Task*, s32, s32);
    } handler;
} ShelterB3DumpingHole2MessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3DumpingHole2MessageEntry, 8);

extern ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018965C[2];
extern EvsCommand                        D_shelter_b3_dumping_hole_8018968C[];
extern EvsCommand                        D_shelter_b3_dumping_hole_801899A4[];
extern TaskDesc                          D_shelter_b3_dumping_hole_8018AFBC;
static CapSequenceRecord*                CapCaption_Data_8015E658;
static s16                               CapCaption_Data_8015E662;
static OverlayCapWindow                  CapCaption_Data_80154514[];
static TextGlyphCell*                    CapCaption_Data_8015E654;
static CapCommandRef*                    CapCaption_Data_8015E650;
static s16                               CapCaption_Data_8015E65C;
static s16                               CapCaption_Data_8015E65E;
static s16                               CapCaption_Data_8015E660;
static s16                               CapCaption_Data_8015E664;
static s16                               CapCaption_Data_8015E666;

static s16      CapCaption_Data_801544EC;
static s16      CapCaption_Data_801544EE;
static u16      CapCaption_Data_8015E668;
static u16      CapCaption_Data_8015E66A;
static s32      CapCaption_Data_801545E4;
static s32      CapCaption_Data_801545E8;
static TaskDesc CapCaption_Data_801544FC;
extern TaskDesc D_80142604;
extern TaskDesc D_801575F0;
static TaskDesc CapCaption_Data_80154508;

extern ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018B7AC[2];

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0);

static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183218(u8 arg0);
static void func_shelter_b3_dumping_hole_8017FD9C(GfxCoord* arg0, s32 arg1);

static void func_shelter_b3_dumping_hole_801833EC(Task* arg0);
static void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2);

static void func_shelter_b3_dumping_hole_80183298(Task* arg0);
static void func_shelter_b3_dumping_hole_801836E0(Task* arg0);
static void func_shelter_b3_dumping_hole_8018378C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183824(Task* arg0);
static void func_shelter_b3_dumping_hole_801838A0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183950(Task* arg0);
static void func_shelter_b3_dumping_hole_80183A00(Task* arg0);
static void func_shelter_b3_dumping_hole_80183A98(Task* arg0);
static void func_shelter_b3_dumping_hole_80183AEC(Task* arg0);
static void func_shelter_b3_dumping_hole_80183B9C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183C38(Task* arg0);
static void func_shelter_b3_dumping_hole_80183C8C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183CA0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183D34(Task* arg0);
static void func_shelter_b3_dumping_hole_80183E08(Task* arg0);
static void func_shelter_b3_dumping_hole_80183F04(Task* arg0);

void func_shelter_b3_dumping_hole_8017DCFC(Task*);
void func_shelter_b3_dumping_hole_8017DF90(Task*);
void func_shelter_b3_dumping_hole_8017E440(Task*);
void func_shelter_b3_dumping_hole_8017E94C(Task*);
void func_shelter_b3_dumping_hole_8017F820(Task*);
void func_shelter_b3_dumping_hole_8017FBA0(Task*);
void func_shelter_b3_dumping_hole_8017FCA0(s16);
void func_shelter_b3_dumping_hole_8017FE34(void);
void func_shelter_b3_dumping_hole_8017FE64(s32);
void func_shelter_b3_dumping_hole_8017FE9C(s32);
void func_shelter_b3_dumping_hole_8017FED4(s16);
void func_shelter_b3_dumping_hole_8017FEF4(s16);
void func_shelter_b3_dumping_hole_8017FF14(void);
void func_shelter_b3_dumping_hole_8017FFF4(void);
void func_shelter_b3_dumping_hole_80180014(void);
void func_shelter_b3_dumping_hole_80180034(void);

void func_shelter_b3_dumping_hole_8018005C(Task*);
void func_shelter_b3_dumping_hole_80181430(void);
void func_shelter_b3_dumping_hole_80181560(Task*);
void func_shelter_b3_dumping_hole_801818E0(void);
void func_shelter_b3_dumping_hole_80181958(s32);
void func_shelter_b3_dumping_hole_80181990(s16);
void func_shelter_b3_dumping_hole_801819B0(void);
void func_shelter_b3_dumping_hole_801819D0(void);
void func_shelter_b3_dumping_hole_801819F0(void);

void func_shelter_b3_dumping_hole_80181A48(Task*);

static AnimationSet                        _gShelterB3DumpingHoleAnimation0C810;
static AnimationSet                        _gShelterB3DumpingHoleAnimation0CCB4;
static AnimationSet                        _gShelterB3DumpingHoleAnimation0D9C4;
extern ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8;

extern AnimationPlayRequest                D_shelter_b3_dumping_hole_8018AFF4;
extern AnimationPlayRequest                D_shelter_b3_dumping_hole_8018B008;
extern AnimationPlayRequest                D_shelter_b3_dumping_hole_8018B01C;
extern ActorCommand                        D_shelter_b3_dumping_hole_8018B078;
extern PadScriptCmd                        D_shelter_b3_dumping_hole_8018AFAC[2];
extern PadScriptVibrationSegment           D_shelter_b3_dumping_hole_8018AFB4[2];
extern ActorTransform                      D_shelter_b3_dumping_hole_8018B030;
extern ActorTransform                      D_shelter_b3_dumping_hole_8018B048;
extern ActorTransform                      D_shelter_b3_dumping_hole_8018B060;
extern ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8;
void                                       func_shelter_b3_dumping_hole_80181A18(void);
void                                       func_shelter_b3_dumping_hole_80181B04(s16);
void                                       func_shelter_b3_dumping_hole_80181B44(s32);

extern WorldCollisionGrid    D_shelter_b3_dumping_hole_8018C3EC[1];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018E88C[8];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018EF9C[8];

void func_shelter_b3_dumping_hole_80183530(Task*, s32, ActorCommand* request);
void func_shelter_b3_dumping_hole_80183550(Task*);
void func_shelter_b3_dumping_hole_801835C8(Task*);
void func_shelter_b3_dumping_hole_80183620(Task*);
void func_shelter_b3_dumping_hole_80183678(Task*);

extern SpriteDrawArea D_shelter_b3_dumping_hole_8018D3E0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C944[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C954[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C964[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAA0[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAB8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAC8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAD8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC28[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC40[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC50[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC60[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC70[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D3B0[6];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D7B4[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D964[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D984[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DB24[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DBE4[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DBFC[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DC0C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD34[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD4C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD5C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD80[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD98[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DF88[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFA0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFB0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFC0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFD0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFE0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFF0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E000[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E010[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E020[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E030[2];
extern SpriteSource   D_shelter_b3_dumping_hole_8018C974[15];
extern SpriteSource   D_shelter_b3_dumping_hole_8018CAE8[16];
extern SpriteSource   D_shelter_b3_dumping_hole_8018CC80[92];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D3F4[48];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D7D4[20];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D994[20];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DB44[8];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DC1C[14];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DD6C[1];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DDA8[24];
extern TaskDesc       D_80164B78;
extern TaskDesc       D_80174D58;
extern TaskDesc       D_shelter_b3_dumping_hole_80188BC8[5];

DumpingHoleAnimFrame D_shelter_b3_dumping_hole_801880B8[13] = {
    { 704, 0, 112, 112, 48, 48 },
    { 716, 48, 112, 112, 48, 48 },
    { 728, 96, 112, 112, 48, 48 },
    { 740, 144, 112, 112, 48, 48 },
    { 752, 192, 112, 112, 48, 48 },
    { 704, 0, 160, 160, 48, 48 },
    { 716, 48, 160, 160, 48, 48 },
    { 728, 96, 160, 160, 48, 48 },
    { 740, 144, 160, 160, 48, 48 },
    { 752, 192, 160, 160, 48, 48 },
    { 704, 0, 208, 208, 48, 48 },
    { 716, 48, 208, 208, 48, 48 },
    { -1, 0, 0, 0, 0, 0 },
};

s16 D_shelter_b3_dumping_hole_80188154[12] = {
    2,
    2,
    2,
    2,
    2,
    60,
    2,
    2,
    2,
    2,
    2,
    2,
};

s16 D_shelter_b3_dumping_hole_8018816C[12] = {
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

s16 D_shelter_b3_dumping_hole_80188184[12] = {
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

s32 D_shelter_b3_dumping_hole_8018819C[12] = {
    8000,
    0,
    -6000,
    0,
    0x4000000,
    0,
    0x290E,
    0,
    -6000,
    0,
    0x4000000,
    0,
};

ActorTransform D_shelter_b3_dumping_hole_801881CC = { { 0x290E, 0, -6000, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_801881E4 = { { 5000, 1800, -6000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_801881FC[11] = {
    { { 9000, -1000, -1500, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -1600, -5100, 0 }, { 0, 0, 0, 0 } },
    { { 8200, -2300, -5300, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1800, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -1800, -6900, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -1200, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -1200, -7000, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -800, -5700, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2100, -6000, 0 }, { 0, 0, 0, 0 } },
    { { 7900, -2900, -5500, 0 }, { 0, 0, 0, 0 } },
    { { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END, 0, 0, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_shelter_b3_dumping_hole_80188304[19] = {
    { { 8800, -1000, -5900, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -2400, -6200, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1400, -7200, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1500, -6700, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -1100, -6100, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -1400, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -900, -5400, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1100, -4900, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1300, -4400, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1700, -4700, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -2000, -5100, 0 }, { 0, 0, 0, 0 } },
    { { 8600, -2300, -4400, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2900, -5000, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2000, -7000, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -2200, -7600, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1300, -7600, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -800, -7200, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -700, -4600, 0 }, { 0, 0, 0, 0 } },
    { { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END, 0, 0, 0 }, { 0, 0, 0, 0 } },
};

DumpingHoleDebrisEntry D_shelter_b3_dumping_hole_801884CC[13] = {
    { 8300, -2900, -5600, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 9000, -1200, -6700, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8600, -2700, -7000, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8600, -2300, -6700, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8600, -1800, -5500, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8800, -1500, -5400, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8900, -1000, -7700, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
    { 8800, -1900, -6300, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8300, -2300, -4800, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8700, -1700, -4400, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8500, -1800, -7700, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 8700, -2300, -5600, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    { 0xFFFF, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
};

EvsSceneKey D_shelter_b3_dumping_hole_80188638 = { 4, 17, 11 };

EvsCommand D_shelter_b3_dumping_hole_80188640[45] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FE34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_8017FE64 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_shelter_b3_dumping_hole_80188638 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80180014 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_8017FE64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80180034 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_80188A78[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FF14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_8017FCA0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_dumping_hole_80188BC8[5] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_dumping_hole_8017F820, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_dumping_hole_8017FBA0, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_dumping_hole_8017E94C, { .model = &gShelterB3DumpingHoleModel0A0CC } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_dumping_hole_8017E94C, { .model = &gShelterB3DumpingHoleModel0A348 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_dumping_hole_8017E94C, { .model = &gShelterB3DumpingHoleModel0A5EC } },
};

TaskDesc D_shelter_b3_dumping_hole_80188C04[4] = {
    { { { TASK_BODY_COORD, 192 } }, func_shelter_b3_dumping_hole_8017DCFC, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_shelter_b3_dumping_hole_8017DF90, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_shelter_b3_dumping_hole_8017E440, { .value = 0 } },
};

static TmdBone _gShelterB3DumpingHoleModel0BAC8Skeleton[4] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleModel0BAC8PartVerts[4] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0BAC8Verts[100] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0BAC8Normals[18] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_normals.inc"
};

static u32 _gShelterB3DumpingHoleModel0BAC8Stream[365] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_stream.inc"
};

static TmdSource _gShelterB3DumpingHoleModel0BAC8 = {
    0,
    2600,
    0,
    4,
    _gShelterB3DumpingHoleModel0BAC8PartVerts,
    _gShelterB3DumpingHoleModel0BAC8Verts,
    _gShelterB3DumpingHoleModel0BAC8Normals,
    _gShelterB3DumpingHoleModel0BAC8Skeleton,
    _gShelterB3DumpingHoleModel0BAC8Stream,
};

ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018965C[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .call2 = actorMsgSetDrawMode } },
    { ACTOR_MESSAGE_PLACE, { .call1 = actorMsgPlaceYawPitchRoll } },
};

ActorTransform D_shelter_b3_dumping_hole_8018966C = { { 4500, -0x2CEC, -5450, 0 }, { 341, 0, 0, 0 } };

EvsSceneKey D_shelter_b3_dumping_hole_80189684 = { 4, 18, 11 };

EvsCommand D_shelter_b3_dumping_hole_8018968C[33] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_shelter_b3_dumping_hole_80189684 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801819B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801819D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_80181958 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801819F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_80181958 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_801899A4[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80181430 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_dumping_hole_80189ADC[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_dumping_hole_80181560, { .model = &_gShelterB3DumpingHoleModel0BAC8 } },
    { { { TASK_BODY_COORD, 192 } }, func_shelter_b3_dumping_hole_8018005C, { .value = 0 } },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0C810Bank1[6] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0C810Bank4[46] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0C810Records[109] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0C810Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0C810 = {
    _gShelterB3DumpingHoleAnimation0C810Records,
    _gShelterB3DumpingHoleAnimation0C810Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0C810Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0C810Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0CCB4Bank1[9] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0CCB4Bank4[97] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0CCB4Records[153] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0CCB4Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0CCB4 = {
    _gShelterB3DumpingHoleAnimation0CCB4Records,
    _gShelterB3DumpingHoleAnimation0CCB4Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0CCB4Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0CCB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0D9C4Bank1[28] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0D9C4Bank4[232] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0D9C4Records[500] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0D9C4Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0D9C4 = {
    _gShelterB3DumpingHoleAnimation0D9C4Records,
    _gShelterB3DumpingHoleAnimation0D9C4Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0D9C4Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0D9C4Bank4, NULL, NULL, NULL },
};

PadScriptCmd D_shelter_b3_dumping_hole_8018AFAC[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_shelter_b3_dumping_hole_8018AFB4[2] = {
    { 255, 250, 9, 1 },
    { 0, 0, 9, 0 },
};

TaskDesc D_shelter_b3_dumping_hole_8018AFBC = { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_dumping_hole_80181A48, { .value = 0 } };

ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8 = { .data = { { NULL, &_gShelterB3DumpingHoleAnimation0C810, &_gShelterB3DumpingHoleAnimation0CCB4, &_gShelterB3DumpingHoleAnimation0D9C4 }, { { .words = D_shelter_b3_dumping_hole_8018AFC8.words }, 5 } } };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018AFE0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018AFF4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018B008 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018B01C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_shelter_b3_dumping_hole_8018B030 = { { 0x2904, 0, -3300, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_8018B048 = { { 0x2904, -100, -3300, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_8018B060 = { { 8800, 0, -6000, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b3_dumping_hole_8018B078 = { { .loc = { 4, 39 } }, 0 };

ActorCommand D_shelter_b3_dumping_hole_8018B07C = { { .loc = { 4, 39 } }, 1 };

EvsCommand D_shelter_b3_dumping_hole_8018B080[39] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018AFC8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b3_dumping_hole_8018B078 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018AFF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54270001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B008 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B030 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54270002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80181A18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_shelter_b3_dumping_hole_8018AFAC }, { .vibrationSegments = D_shelter_b3_dumping_hole_8018AFB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_shelter_b3_dumping_hole_80181B04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_80181B44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B01C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B060 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_8018B428[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b3_dumping_hole_8018B078 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54270001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54270002 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B060 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b3_dumping_hole_80181B44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/cap_captions_settings.inc.c"

static void CapCaption_RunSchedule(Task* task);

TaskDesc D_shelter_b3_dumping_hole_8018B57C[1] = {
    { { { TASK_BODY_NONE, 32 } }, CapCaption_RunSchedule, { .value = 0 } }
};

#include "../../shared/cap_captions_schedule.inc.c"

WorldCollisionRoomResources D_shelter_b3_dumping_hole_8018B678[2] = {
    { D_shelter_b3_dumping_hole_8018C3EC, NULL, D_shelter_b3_dumping_hole_8018ECA4, NULL },
    { D_shelter_b3_dumping_hole_8018C3EC, D_shelter_b3_dumping_hole_8018E88C, D_shelter_b3_dumping_hole_8018EF9C, NULL },
};

u8* D_shelter_b3_dumping_hole_8018B698[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_shelter_b3_dumping_hole_8018B6A0[2] = { 37, 37 };

DirectionWarpEntry D_shelter_b3_dumping_hole_8018B6A4[3] = {
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, 0x54270004, 0x54270003, DIRECTION_WARP_SOUND_NONE, 30, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 30, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 15, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

ActorsShared801673f8Spot D_shelter_b3_dumping_hole_8018B74C[12] = {
    { 1500, -3950, -550, 2048 },
    { 4500, -3950, -550, 2048 },
    { 7500, -3950, -550, 2048 },
    { 0x2904, -3950, -550, 2048 },
    { 0x34BC, -3950, -550, 2048 },
    { 0x4074, -3950, -550, 2048 },
    { 1500, -3950, -0x30A2, 0 },
    { 4500, -3950, -0x30A2, 0 },
    { 7500, -3950, -0x30A2, 0 },
    { 0x2904, -3950, -0x30A2, 0 },
    { 0x34BC, -3950, -0x30A2, 0 },
    { 0x4074, -3950, -0x30A2, 0 },
};

ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018B7AC[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_shelter_b3_dumping_hole_80183530 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

OverlayEncounterSlot D_shelter_b3_dumping_hole_8018B7BC[16] = {
    { 0, 801, { 0, 0 }, 0 },
    { 1, 2305, { 0, 0 }, 0 },
    { 2, 513, { 0, 0 }, 0 },
    { 0, 1041, { 0, 0 }, 0 },
    { 0, 2049, { 0, 0 }, 0 },
    { 2, 2305, { 0, 0 }, 0 },
    { 1, 769, { 0, 0 }, 0 },
    { 1, 2561, { 0, 0 }, 0 },
    { 2, 1025, { 0, 0 }, 0 },
    { 0, 2561, { 0, 0 }, 0 },
    { 1, 1025, { 0, 0 }, 0 },
    { 1, 2561, { 0, 0 }, 0 },
    { 2, 1281, { 0, 0 }, 0 },
    { 0, 2817, { 0, 0 }, 0 },
    { 1, 2817, { 0, 0 }, 0 },
    { 0, 769, { 0, 0 }, 0 },
};

TaskDesc D_shelter_b3_dumping_hole_8018B83C[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b3_dumping_hole_80183550, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_801835C8, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_80183620, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_80183678, { .value = 0 } },
};

// Lighting task indexes a shared pool through entry 40; interior bases also address entries in the same pool. The final existing eight-point view ends at entry 43.
SVECTOR D_shelter_b3_dumping_hole_8018B86C[44] = {
    { 1170, -7750, 150, 0 },
    { 1830, -7750, 150, 0 },
    { 4170, -7750, 150, 0 },
    { 4830, -7750, 150, 0 },
    { 7170, -7750, 150, 0 },
    { 7830, -7750, 150, 0 },
    { 10170, -7750, 150, 0 },
    { 10830, -7750, 150, 0 },
    { 13170, -7750, 150, 0 },
    { 13830, -7750, 150, 0 },
    { 16170, -7750, 150, 0 },
    { 16830, -7750, 150, 0 },
    { 1170, -7750, -12150, 0 },
    { 1830, -7750, -12150, 0 },
    { 4170, -7750, -12150, 0 },
    { 4830, -7750, -12150, 0 },
    { 7170, -7750, -12150, 0 },
    { 7830, -7750, -12150, 0 },
    { 10170, -7750, -12150, 0 },
    { 10830, -7750, -12150, 0 },
    { 13170, -7750, -12150, 0 },
    { 13830, -7750, -12150, 0 },
    { 16170, -7750, -12150, 0 },
    { 16830, -7750, -12150, 0 },
    { 1500, -10440, -3500, 0 },
    { 4500, -10440, -3500, 0 },
    { 1500, -10440, -8500, 0 },
    { 4500, -10440, -8500, 0 },
    { 19090, -5050, -4610, 0 },
    { 19090, -5050, -7300, 0 },
    { 19090, -1044, -3620, 0 },
    { 20150, -2050, -4400, 0 },
    { 1500, -4380, 550, 0 },
    { 4500, -4380, 550, 0 },
    { 7500, -4380, 550, 0 },
    { 10500, -4380, 550, 0 },
    { 13500, -4380, 550, 0 },
    { 16500, -4380, 550, 0 },
    { 1500, -4380, -12550, 0 },
    { 4500, -4380, -12550, 0 },
    { 7500, -4380, -12550, 0 },
    { 10500, -4380, -12550, 0 },
    { 13500, -4380, -12550, 0 },
    { 16500, -4380, -12550, 0 },
};

static SVECTOR _gShelterB3DumpingHoleCollision0EE2CNormals[35] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_normals.inc"
};

static SVECTOR _gShelterB3DumpingHoleCollision0EE2CVerts[90] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_verts.inc"
};

static WorldCollisionGridFace _gShelterB3DumpingHoleCollision0EE2CFaces[56] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_faces.inc"
};

static s16 _gShelterB3DumpingHoleCollision0EE2CCells[412] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB3DumpingHoleCollision0EE2CCells[i])
static s16* _gShelterB3DumpingHoleCollision0EE2CTable[24] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b3_dumping_hole_8018C3EC[1] = {
    { NULL, _gShelterB3DumpingHoleCollision0EE2CNormals, _gShelterB3DumpingHoleCollision0EE2CVerts, _gShelterB3DumpingHoleCollision0EE2CFaces, _gShelterB3DumpingHoleCollision0EE2CTable, 100, 0x2F44, 6, 4, 4000, 56 },
};

ViewCamera D_shelter_b3_dumping_hole_8018C410[37] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x61A8, 6000 } }, 257 },
    { { { { 3054, 0, 2728 }, { 510, 4023, -571 }, { -2680, 766, 3000 } }, { -0x452F, 2991, 0x2DB7 } }, 257 },
    { { { { 3126, 0, 2645 }, { 600, 3989, -709 }, { -2576, 929, 3045 } }, { -0x3779, 3121, 0x2A83 } }, 257 },
    { { { { 2261, 0, -3415 }, { -290, 4081, -192 }, { 3403, 348, 2252 } }, { -0x2919, 2411, 9983 } }, 257 },
    { { { { 4028, 0, -739 }, { -727, 736, -3963 }, { 132, 4029, 724 } }, { -8741, 9891, 7423 } }, 289 },
    { { { { 4028, 0, -739 }, { -727, 736, -3963 }, { 132, 4029, 724 } }, { -0x3CD3, 9891, 7423 } }, 289 },
    { { { { 4095, 0, 19 }, { 2, 4057, -556 }, { -19, 556, 4057 } }, { -0x33FE, 2784, 0x3158 } }, 230 },
    { { { { 2223, 0, -3439 }, { -1144, 3862, -739 }, { 3243, 1362, 2096 } }, { -0x31C5, 2794, 9228 } }, 257 },
    { { { { 1013, 0, -3968 }, { -3267, 2324, -834 }, { 2252, 3372, 574 } }, { -8292, 7543, 6554 } }, 289 },
    { { { { 1061, 0, -3956 }, { -2152, 3436, -577 }, { 3319, 2228, 890 } }, { -0x333C, 4161, 7069 } }, 289 },
    { { { { 4028, 0, -739 }, { -723, 834, -3944 }, { 150, 4010, 821 } }, { -0x2E19, 8861, 7423 } }, 289 },
    { { { { 4037, 0, -690 }, { -663, 1141, -3877 }, { 192, 3933, 1125 } }, { -0x41F1, 7141, 7933 } }, 257 },
    { { { { 618, 0, 4049 }, { -574, 4054, 87 }, { -4008, -580, 611 } }, { -0x511E, 690, 6915 } }, 257 },
    { { { { 2533, 0, -3218 }, { -725, 3990, -570 }, { 3135, 922, 2468 } }, { -0x410B, 1800, 6163 } }, 329 },
    { { { { 2538, 0, 3214 }, { -729, 3989, 575 }, { -3130, -929, 2472 } }, { -0x3A3F, 581, 4683 } }, 289 },
    { { { { 3865, 0, -1353 }, { 0, 4096, 0 }, { 1353, 0, 3865 } }, { -9755, 1115, 5205 } }, 447 },
    { { { { 3700, 0, -1756 }, { 511, 3918, 1077 }, { 1680, -1193, 3539 } }, { -0x2FE5, 2981, 1883 } }, 257 },
    { { { { 2435, 0, -3293 }, { 228, 4086, 168 }, { 3285, -283, 2429 } }, { -6561, 901, 8903 } }, 257 },
    { { { { 547, 0, 4059 }, { -348, 4080, 47 }, { -4044, -351, 545 } }, { -0x3071, 1141, 6693 } }, 257 },
    { { { { 2366, 0, 3342 }, { 2987, 1837, -2115 }, { -1499, 3660, 1061 } }, { -0x28F1, 5771, 7663 } }, 289 },
    { { { { 2828, 0, 2962 }, { -1276, 3696, 1218 }, { -2673, -1764, 2552 } }, { -6451, 9662, 7109 } }, 257 },
    { { { { 2533, 0, -3218 }, { -725, 3990, -570 }, { 3135, 922, 2468 } }, { -0x410B, 1800, 6163 } }, 329 },
    { { { { 449, 0, 4071 }, { -401, 4075, 44 }, { -4051, -404, 447 } }, { -0x2B7A, 1770, 6430 } }, 230 },
    { { { { 3412, 0, 2265 }, { 1845, 2373, -2781 }, { -1312, 3337, 1977 } }, { -0x3C29, 0x2879, 0x2BFF } }, 275 },
    { { { { 449, 0, -4071 }, { 286, 4085, 31 }, { 4061, -287, 448 } }, { -0x376A, 1090, 6535 } }, 230 },
    { { { { 3508, 0, -2113 }, { -876, 3727, -1454 }, { 1923, 1697, 3192 } }, { -0x31C5, 4261, 0x297F } }, 230 },
    { { { { 941, 0, -3986 }, { -3234, 2393, -764 }, { 2329, 3323, 550 } }, { -0x2AB4, 7743, 6554 } }, 289 },
    { { { { 3954, 0, -1065 }, { -1039, 900, -3858 }, { 234, 3995, 869 } }, { -0x3CA1, 8931, 7643 } }, 257 },
    { { { { 473, 0, -4068 }, { 308, 4084, 35 }, { 4056, -310, 472 } }, { -8800, 1401, 6883 } }, 257 },
    { { { { 698, 0, -4035 }, { 449, 4070, 77 }, { 4010, -456, 694 } }, { -0x2F6C, 1401, 6883 } }, 257 },
    { { { { 354, 0, 4080 }, { -290, 4085, 25 }, { -4070, -291, 353 } }, { -0x4589, 1641, 7013 } }, 230 },
    { { { { 627, 0, -4047 }, { -2564, 3169, -397 }, { 3131, 2594, 485 } }, { -3752, 7543, 6554 } }, 289 },
    { { { { 639, 0, 4045 }, { 1942, 3592, -307 }, { -3548, 1967, 560 } }, { -0x40D0, 5263, 7144 } }, 289 },
    { { { { 3310, 0, 2412 }, { 400, 4039, -549 }, { -2378, 680, 3264 } }, { -0x452F, 2991, 0x2DB7 } }, 257 },
    { { { { 344, 0, 4081 }, { 160, 4092, -13 }, { -4078, 160, 344 } }, { -0x3517, 1731, 6823 } }, 230 },
    { { { { 1289, 0, 3887 }, { 2700, 2946, -895 }, { -2797, 2844, 927 } }, { -7481, 2971, 7013 } }, 257 },
    { { { { 618, 0, 4049 }, { -574, 4054, 87 }, { -4008, -580, 611 } }, { -0x511E, 690, 6915 } }, 257 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C964[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018C974[15] = {
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 32, 1637, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 80, 0, 1612, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 40, 1575, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 32, 1587, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -16, 1587, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 112, -120, 1537, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 120, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 128, -120, 1462, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 136, -120, 1425, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 144, -120, 1412, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 152, -120, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 104, -120, 1575, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 104, -8, 1500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -16, 1600, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 96, -80, 1575, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018CAE8[16] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 16, 1025, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, 32, 1075, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 104, -40, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, 112, -40, 975, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -80, 950, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, 120, -120, 950, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 240 } }, 136, -120, 900, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, 24, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, 0, 1025, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -24, 1000, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -8, 986, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 1087, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 72, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 24, 1075, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018CC80[92] = {
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 16, 950, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, -64, 987, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, -64, 987, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, 16, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -136, -64, 987, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 16, 950, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, -64, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, 16, 937, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -152, -64, 1025, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, 16, 937, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -64, 1025, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 16, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -88, 48, 2700, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 48, 2700, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 48, 2700, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -104, 56, 2700, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -40, 56, 2700, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 24, 56, 2700, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 56, 2700, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 64, 2700, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 2700, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 2700, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 2700, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 64, 2700, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 2700, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 64, 2700, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 64, 2700, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 56, 2125, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 2125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 56, 2125, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -32, 56, 2125, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 32, 56, 2125, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -120, 64, 2125, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 64, 2125, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 64, 2125, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 64, 2125, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 2125, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 80, 2125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 2125, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 2125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 2125, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 80, 2125, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 80, 2125, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 80, 2125, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 80, 2125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 80, 2125, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 72, 2125, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 2125, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 72, 2125, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 2125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 72, 2125, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 72, 2125, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 72, 2125, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 72, 2125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 72, 2125, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 72, 2125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 2125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 72, 2125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 2125, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 72, 2125, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 72, 2125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 72, 2125, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 2125, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 72, 2125, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 1400, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 56, 1425, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 64, 1425, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 72, 1400, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 1400, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 1400, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 1400, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 80, 1400, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 80, 1400, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 1400, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 80, 1400, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 80, 1400, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 80, 1400, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 80, 1400, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 72, 1400, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1400, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 72, 1400, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1400, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 72, 1400, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 72, 1400, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 72, 1400, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 1400, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 64, 1400, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 1400, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 1400, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 1400, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 64, 1400, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1400, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D3B0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 15, 0, 0, { 0, 0 } },
    { 27, 37, 0, 0, { 2, 0 } },
    { 64, 28, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_dumping_hole_8018D3E0[2] = {
    { { 1, 0, 318, 196 }, 1400 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_dumping_hole_8018D3F4[48] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, -120, 1050, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -120, 1050, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -120, 1050, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1050, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -120, 1050, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 1000, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -120, 1000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -120, 912, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 1025, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -80, 975, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -80, 900, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, -80, 1075, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, -80, 1050, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -120, 887, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -120, 887, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -120, 875, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -120, 925, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -120, 925, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -120, 950, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -120, 950, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -120, 1000, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -120, 1000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -56, 750, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -16, 750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -96, 800, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -56, 800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -16, 800, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -96, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -56, 875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -16, 900, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -96, 925, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -56, 925, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -16, 837, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -96, 950, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -56, 975, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -16, 875, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 120 } }, -48, -96, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -96, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -56, 1000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -16, 900, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, 24, 1125, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 1025, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 24, 775, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 24, 800, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 24, 912, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 24, 962, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 962, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D7B4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018D7D4[20] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 625, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 8, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 0, 32, 625, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -104, 48, 625, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -128, 72, 625, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -88, 72, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -48, 72, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -8, 72, 625, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 32, 72, 625, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 72, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 112, 72, 625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 72, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 80, 32, 625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 80, -16, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, -40, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 32, 625, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -64, 875, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 56, -112, 875, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 56, -88, 875, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 56, -64, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D964[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018D994[20] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -56, 768, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -56, 769, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -56, 769, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -56, 789, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, -80, 595, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, 0, 573, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 40, 691, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -80, 634, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -40, 598, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, 0, 590, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 622, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -80, 490, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -40, 465, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, 0, 456, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 40, 450, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, -80, 392, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, -40, 383, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 0, 371, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 40, 359, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, -40, 568, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DB24[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DB44[8] = {
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -160, 24, 375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, 32, 375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -112, 40, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -88, 48, 375, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -40, 64, 375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 72, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 8, 96, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DBE4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DC0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DC1C[14] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 375, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, -120, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, -120, 375, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -24, -104, 375, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 32, -104, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -40, -88, 375, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 24, -88, 375, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, -88, 375, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, -72, 375, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 16, -72, 375, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -72, 375, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -56, -56, 375, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 8, -56, 375, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -40, -40, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DD6C[1] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD80[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DDA8[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 562, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 1300, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 72, 1250, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 80, 1200, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 16, 750, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 104, 24, 712, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 32, 675, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 637, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 600, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 88, 1150, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 96, 1100, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 104, 1050, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 32, 1200, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 32, 1150, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 40, 1100, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 40, 1050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 48, 1000, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, 48, 950, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 900, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 0, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -24, 750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 48, 1225, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 64, 1300, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 64, 850, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DF88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E000[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E010[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E020[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E030[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E040[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b3_dumping_hole_8018E050[37] = {
    { { .empty = D_shelter_b3_dumping_hole_8018C944 }, D_shelter_b3_dumping_hole_8018C944, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018C954 }, D_shelter_b3_dumping_hole_8018C954, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018C964 }, D_shelter_b3_dumping_hole_8018C964, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018C974 }, D_shelter_b3_dumping_hole_8018CAA0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAB8 }, D_shelter_b3_dumping_hole_8018CAB8, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAC8 }, D_shelter_b3_dumping_hole_8018CAC8, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAD8 }, D_shelter_b3_dumping_hole_8018CAD8, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CAE8 }, D_shelter_b3_dumping_hole_8018CC28, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC40 }, D_shelter_b3_dumping_hole_8018CC40, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC50 }, D_shelter_b3_dumping_hole_8018CC50, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC60 }, D_shelter_b3_dumping_hole_8018CC60, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC70 }, D_shelter_b3_dumping_hole_8018CC70, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CC80 }, D_shelter_b3_dumping_hole_8018D3B0, D_shelter_b3_dumping_hole_8018D3E0 },
    { { .elements = D_shelter_b3_dumping_hole_8018D3F4 }, D_shelter_b3_dumping_hole_8018D7B4, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018D7D4 }, D_shelter_b3_dumping_hole_8018D964, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018D984 }, D_shelter_b3_dumping_hole_8018D984, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018D994 }, D_shelter_b3_dumping_hole_8018DB24, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DB44 }, D_shelter_b3_dumping_hole_8018DBE4, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DBFC }, D_shelter_b3_dumping_hole_8018DBFC, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DC0C }, D_shelter_b3_dumping_hole_8018DC0C, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DC1C }, D_shelter_b3_dumping_hole_8018DD34, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD4C }, D_shelter_b3_dumping_hole_8018DD4C, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD5C }, D_shelter_b3_dumping_hole_8018DD5C, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DD6C }, D_shelter_b3_dumping_hole_8018DD80, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD98 }, D_shelter_b3_dumping_hole_8018DD98, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DDA8 }, D_shelter_b3_dumping_hole_8018DF88, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFA0 }, D_shelter_b3_dumping_hole_8018DFA0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFB0 }, D_shelter_b3_dumping_hole_8018DFB0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFC0 }, D_shelter_b3_dumping_hole_8018DFC0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFD0 }, D_shelter_b3_dumping_hole_8018DFD0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFE0 }, D_shelter_b3_dumping_hole_8018DFE0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFF0 }, D_shelter_b3_dumping_hole_8018DFF0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E000 }, D_shelter_b3_dumping_hole_8018E000, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E010 }, D_shelter_b3_dumping_hole_8018E010, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E020 }, D_shelter_b3_dumping_hole_8018E020, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E030 }, D_shelter_b3_dumping_hole_8018E030, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CC80 }, D_shelter_b3_dumping_hole_8018D3B0, NULL },
};

WorldCoordLight D_shelter_b3_dumping_hole_8018E20C[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9988, -0x2B02, -2510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2582, 2295 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9632, -0x2B02, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 925, 820 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b3_dumping_hole_8018E2BC[3] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 819, 737, 655 }, { 0, 0 } }, 3561, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4914, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4914, 409, 0 }, { 0, 0 } }, 1500, 2000 },
};

WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E3DC = { ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E20C), D_shelter_b3_dumping_hole_8018E20C, ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E2BC), D_shelter_b3_dumping_hole_8018E2BC, 0, NULL };

WorldCoordPointLight D_shelter_b3_dumping_hole_8018E3F4[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1230, 1110, 985 }, { 0, 0 } }, 4202, 7241 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1106, 987 }, { 0, 0 } }, 4339, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1230, 1109, 984 }, { 0, 0 } }, 4124, 7086 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1109, 987 }, { 0, 0 } }, 4239, 6981 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1106, 986 }, { 0, 0 } }, 4180, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1232, 1106, 985 }, { 0, 0 } }, 4275, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 1844, 1640 }, { 0, 0 } }, 3561, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4DF6, -2052, -4394 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 310, 3279, 1229 }, { 0, 0 } }, 1341, 1738 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A91, -2181, -3367 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 3690, 4096 }, { 0, 0 } }, 1400, 2022 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -9400, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2215, 1970 }, { 0, 0 } }, 3000, 4000 },
};

WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E874 = { 0, NULL, ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E3F4), D_shelter_b3_dumping_hole_8018E3F4, 0, NULL };

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018E88C[8] = {
    { NULL, NULL, NULL, { 0x3F30, -5696, -5696, 0 }, { { 0, -6336, -4624, 0 }, { 0, -6336, 4624, 0 }, { 0, 6336, -4624, 0 }, { 0, 6336, 4624, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 30, 29, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3FE0, -5664, -5728, 0 }, { { 0, -6336, 4624, 0 }, { 0, -6336, -4624, 0 }, { 0, 6336, 4624, 0 }, { 0, 6336, -4624, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 30, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x327F, -5280, -5793, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 29, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x31DE, -5345, -5858, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 31, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9152, -5376, -6017, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 35, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9311, -5344, -5921, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 31, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6016, -4993, -6048, 0 }, { { 0, -6336, -4623, 0 }, { 0, -6336, 4623, 0 }, { 0, 6336, -4623, 0 }, { 0, 6336, 4623, 0 } }, { 4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 36, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6176, -5120, -6112, 0 }, { { 0, -6336, 4623, 0 }, { 0, -6336, -4623, 0 }, { 0, 6336, 4623, 0 }, { 0, 6336, -4623, 0 } }, { -4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 36, 35, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EAEC[5] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { 252, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 44, 1, 0, 0x38C0, 0, -5888, 2048, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_dumping_hole_8018EB3C[5] = {
    { 32, 32, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_8015F8D0 },
    { 103, 417, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_shelter_b3_dumping_hole_80188BC8 },
    { 252, 417, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, D_80176354 },
    { 44, 44, AREA_RESOURCE_FILE_GROUP_BASE_60, 2, { 0, 0 }, &D_80174D58 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b3_dumping_hole_8018EB78[2] = {
    { 32, 32, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_8015F8D0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EB90[2] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_dumping_hole_8018EBB0[5] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 1, { 0, 0 }, &D_80142604 },
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 2, { 0, 0 }, &D_801575F0 },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 1, { 0, 0 }, &D_80151E60 },
    { 103, 421, AREA_RESOURCE_FILE_GROUP_BASE_30, 1, { 0, 0 }, &D_80164B78 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EBEC[5] = {
    { 44, 1, 0, 0x38C0, 4000, -5888, 2048, 0, 0, 2, 0 },
    { 70, 1, 0, 0x2B5C, 0, -8200, 2048, 0, 2, 4, 0 },
    { 71, 1, 1, 5000, 0, -5900, 2048, 0, 3, 5, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b3_dumping_hole_8018EC3C[13] = {
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EAEC, D_shelter_b3_dumping_hole_8018EB3C },
    { D_shelter_b3_dumping_hole_8018EBEC, D_shelter_b3_dumping_hole_8018EBB0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EB90, D_shelter_b3_dumping_hole_8018EB78 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018ECA4[10] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, WORLD_COLLISION_TRIGGER_ACTION_WARP, 40, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3820, -64, -0x3A40, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2660, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8608, -64, -5888, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 2660, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 20, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -496, -64, -6080, 0 }, { { -1504, 0, -4336, 0 }, { 1504, 0, -4336, 0 }, { -1504, 0, 4336, 0 }, { 1504, 0, 4336, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 4579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018EF9C[8] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, WORLD_COLLISION_TRIGGER_ACTION_WARP, 40, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6334, -64, -5984, 0 }, { { -144, 0, -2576, 0 }, { 144, 0, -2576, 0 }, { -144, 0, 2576, 0 }, { 144, 0, 2576, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 2572, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F1FC[38] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b3_dumping_hole_8018F1FC) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 308, 311, 308, 309 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 310, 310, 310, 310 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 537, 453, 359, 472 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 516, 512, 516, 514 } },
    { .color = { 246, 249, 246, 247 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 25, 288, 287, 189 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F32C[38] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b3_dumping_hole_8018F32C) - 1 },
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
    { .color = { 616, 618, 617, 617 } },
    { .color = { 617, 618, 618, 617 } },
    { .color = { 616, 617, 617, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 615, 615, 615 } },
    { .color = { 615, 615, 615, 615 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b3_dumping_hole_8018F45C = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F468[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F470[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F478[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_dumping_hole_8018F45C },
};

WorldCollisionSurfaceProperties* D_shelter_b3_dumping_hole_8018F480[9] = {
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F470,
    D_shelter_b3_dumping_hole_8018F478,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    NULL,
};

u8 D_shelter_b3_dumping_hole_8018F4A4[4] = {
    0,
    74,
    201,
    8,
};

Task* D_shelter_b3_dumping_hole_8018F4A8 = NULL;

Task* D_shelter_b3_dumping_hole_8018F4AC = NULL;

u16 D_shelter_b3_dumping_hole_8018F4B0[2] = {
    0,
    0xDF0D,
};

static CapCommandRef* CapCaption_Data_8015E650 = NULL;

static TextGlyphCell* CapCaption_Data_8015E654 = NULL;

static CapSequenceRecord* CapCaption_Data_8015E658 = NULL;

static s16 CapCaption_Data_8015E65C = 0;

static s16 CapCaption_Data_8015E65E = 0;

static s16 CapCaption_Data_8015E660 = 0;

static s16 CapCaption_Data_8015E662 = 0;

static s16 CapCaption_Data_8015E664 = 0;

static s16 CapCaption_Data_8015E666 = 0;

static u16 CapCaption_Data_8015E668 = 0;

static u16 CapCaption_Data_8015E66A = 0;

static u8 CapCaption_Data_8015E66C[4] = {
    0,
    19,
    111,
    0,
};

u16 D_shelter_b3_dumping_hole_8018F4D4[2] = {
    0,
    0xD086,
};

s32 D_shelter_b3_dumping_hole_8018F4D8;

static inline u16 _shelterB3DumpingHoleIsOffscreen(s16 x, s16 y);
static u16        func_shelter_b3_dumping_hole_8017DA00(GfxCoord* coord, s16 w, s16 h, s16 u,
                                                        s16 v, s16 tpageX, s16 tpageY, s16 scale,
                                                        s16 clut, s32 otzOverride);
static void       func_shelter_b3_dumping_hole_8017E7DC(Task* arg0);
static void       func_shelter_b3_dumping_hole_8017FE10(s32 arg0);
static void       func_shelter_b3_dumping_hole_8018098C(Task* task);

/// Returns 1 when the screen position (`x`, `y`) lies outside the 320x240
/// screen centred on the origin, 0 when it is on screen.
static inline u16 _shelterB3DumpingHoleIsOffscreen(s16 x, s16 y)
{
    if (x < -0xA0) {
        return 1;
    }
    if (x > 0xA0) {
        return 1;
    }
    if (y < -0x78) {
        return 1;
    }
    if (y > 0x78) {
        return 1;
    }
    return 0;
}

/// Draws a camera-facing textured quad centred on `coord`'s origin. The origin
/// is projected with the coordinate's world-screen matrix; if it lands outside
/// the 320x240 screen or behind the camera nothing is drawn and 1 is returned.
/// Otherwise a semi-transparent `POLY_FT4` of `w` x `h` texels at (`u`, `v`),
/// scaled by `scale` (4096 = 1.0), is linked into the ordering table at the
/// projected depth, or at `otzOverride` when that is non-zero, and 0 is returned.
static u16 func_shelter_b3_dumping_hole_8017DA00(GfxCoord* coord, s16 w, s16 h, s16 u,
                                                 s16 v, s16 tpageX, s16 tpageY, s16 scale,
                                                 s16 clut, s32 otzOverride)
{
    SVECTOR   origin;
    s32       sxy;
    s32       z;
    s32       otz;
    u16       off;
    POLY_FT4* prim;
    u16       hw;
    u16       hh;
    s16       sx;
    s16       sy;

    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vx = origin.vy = origin.vz = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stszotz(&z);
    sy = sxy >> 16;
    sx = sxy;
    if (otzOverride != 0) {
        z = otzOverride;
    }
    otz = z;
    if (sx < -0xA0) {
        off = 1;
    } else if (sx > 0xA0 || sy < -0x78 || sy > 0x78 || otz < 0) {
        off = 1;
    } else {
        off = 0;
    }
    if (off) {
        return 1;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2F);
    hw = w * scale / 4096;
    hh = h * scale / 4096;
    setXY4(prim, sx - hw / 2, sy - hh / 2, sx + hw / 2, sy - hh / 2, sx - hw / 2, sy + hh / 2,
           sx + hw / 2, sy + hh / 2);
    setUV4(prim, u, v, u + w - 1, v, u, v + h - 1, u + w - 1, v + h - 1);
    prim->clut  = clut;
    prim->tpage = getTPage(0, 1, tpageX / 64 * 64, tpageY / 256 * 256);
    addPrim(gGpuCurrentOt + (z >> 4), prim);
    return 0;
}

void func_shelter_b3_dumping_hole_8017DCFC(Task* arg0)
{
    DumpingHoleAnimWork* W      = (DumpingHoleAnimWork*)arg0->work;
    GfxCoord*            coord  = arg0->extra.coordBody->coord;
    DumpingHoleEntity*   entity = D_shelter_b3_dumping_hole_8018F4A8->work;

    if (entity->field_42 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->parent     = &gGfxViewCoord;
            coord->coord.t[0] = W->field_0;
            coord->coord.t[1] = W->field_2;
            coord->coord.t[2] = W->field_4;
            arg0->state++;
            return;
        case 1:
            if (entity->field_42 != 2) {
                return;
            }
            W->field_1C     = 5;
            W->field_14     = 0;
            W->field_18     = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            W->field_16     = 0xFFF6 - ((gRandomLcgState >> 16) & 7);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            W->field_20     = (gRandomLcgState >> 16) & 7;
            arg0->state++;
            return;
        case 2:
            if (W->field_20 == 0) {
                arg0->state = 3;
            } else {
                W->field_20--;
            }
            return;
        case 3: {
            s32 t1e     = W->field_1E + 1;
            W->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188154[W->field_1C] < (s16)t1e) {
                (u16) W->field_1C = (u16)W->field_1C + 1;
                W->field_1E       = 0;
                if ((u16)D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += W->field_16;
    if (func_shelter_b3_dumping_hole_8017DA00(
            coord,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_8,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_A,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_2,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_6,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_4,
            W->field_8, 0x43C0, 0) != 0) {
        taskKill(arg0);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void func_shelter_b3_dumping_hole_8017DF90(Task* arg0)
{
    DumpingHoleAnimWork* W     = (DumpingHoleAnimWork*)arg0->work;
    GfxCoord*            coord = arg0->extra.coordBody->coord;
    SVECTOR              vec;
    SVECTOR              pos;
    DVECTOR              sxy;
    POLY_FT4*            prim;
    u16                  offscreen;
    u16                  hw;
    u16                  hh;
    s16                  sx;
    s16                  sy;
    s32                  y;
    s16                  w;
    s16                  h;
    s16                  u;
    s16                  v;
    s16                  tx;
    s16                  ty;
    s16                  scale = 0x1000;
    s32                  otz   = 0x3E8;

    if ((u16)((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_44 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->parent = &gGfxViewCoord;
            Gp_ComposeParentWorld(arg0->spawnArg2.pointer, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx + W->field_C;
            coord->coord.t[1] = vec.vy + W->field_E;
            coord->coord.t[2] = vec.vz + W->field_10;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            W->field_16       = 0xFFF6 - ((gRandomLcgState >> 16) & 7);
            W->field_14       = 0;
            W->field_18       = 0;
            W->field_1C       = 0;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            W->field_20       = ((gRandomLcgState >> 16) & 7) + 0x14;
            arg0->state++;
            return;
        case 1:
            if (W->field_20 == 0) {
                arg0->state = 2;
            } else {
                W->field_20--;
            }
            return;
        case 2: {
            s32 t1e     = W->field_1E + 1;
            W->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_8018816C[W->field_1C] < (s16)t1e) {
                (u16) W->field_1C = (u16)W->field_1C + 1;
                W->field_1E       = 0;
                if ((u16)D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += W->field_16;
    w                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_8;
    h                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_A;
    u                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_2;
    v                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_6;
    tx                 = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0;
    ty                 = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_4;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    pos.vx = pos.vy = pos.vz = 0;
    gte_ldv0(&pos);
    gte_rtps();
    gte_stsxy(&sxy);
    sx        = sxy.vx;
    y         = sxy.vy;
    sy        = y;
    offscreen = _shelterB3DumpingHoleIsOffscreen(sx, y);
    if (offscreen) {
        taskKill(arg0);
        return;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    setSemiTrans(prim, 1);
    prim->r0 = prim->g0 = prim->b0 = 0x20;
    hw                             = w * scale / 4096;
    hh                             = h * scale / 4096;
    setXY4(prim, sx - hw / 2, sy - hh / 2, sx + hw / 2, sy - hh / 2, sx - hw / 2, sy + hh / 2, sx + hw / 2, sy + hh / 2);
    setUV4(prim, u, v, u + w - 1, v, u, v + h - 1, u + w - 1, v + h - 1);
    prim->clut  = 0x43C0;
    prim->tpage = getTPage(0, 1, (tx / 64) * 64, (ty / 256) * 256);
    addPrim(&gGpuCurrentOt[otz >> 4], prim);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void func_shelter_b3_dumping_hole_8017E440(Task* arg0)
{
    DumpingHoleAnimWork* work  = (DumpingHoleAnimWork*)arg0->work;
    GfxCoord*            coord = arg0->extra.coordBody->coord;
    SVECTOR              vec;
    s32                  sa1;
    u32                  roll1;
    u32                  roll2;
    s32                  base18;
    s16                  var0;
    s16                  delta;

    if ((u16)((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_48 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->parent = &gGfxViewCoord;
            Gp_ComposeParentWorld(arg0->spawnArg2.pointer, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx;
            coord->coord.t[1] = vec.vy;
            coord->coord.t[2] = vec.vz;
            arg0->work        = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            work = (DumpingHoleAnimWork*)arg0->work;
            memFillBytes(work, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);
            work->field_16  = -0xA;
            work->field_14  = 0;
            work->field_18  = 0;
            work->field_8   = 0x1000;
            work->field_14  = 0;
            roll1           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_16  = 0xFFF1 - ((roll1 >> 16) & 7);
            sa1             = arg0->spawnArg1.value;
            gRandomLcgState = roll1;
            if (sa1 == 0) {
                roll2           = roll1 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                base18          = work->field_18;
                gRandomLcgState = roll2;
                if ((roll2 >> 16) & 1) {
                    gRandomLcgState = roll2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    var0            = base18 + ((gRandomLcgState >> 16) & 1);
                } else {
                    gRandomLcgState = roll2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    var0            = base18 - ((gRandomLcgState >> 16) & 1);
                }
                work->field_18 = var0;
            } else {
                if (sa1 < 0) {
                    gRandomLcgState = roll1 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    delta           = (u16)work->field_18 + ((u16)arg0->spawnArg1.value - ((gRandomLcgState >> 16) & 1));
                } else {
                    gRandomLcgState = roll1 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    delta           = (u16)work->field_18 + ((u16)arg0->spawnArg1.value + ((gRandomLcgState >> 16) & 1));
                }
                work->field_18 = delta;
            }
            work->field_1C  = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_20  = (gRandomLcgState >> 16) & 7;
            arg0->state++;
            return;
        case 1:
            if (work->field_20 == 0) {
                arg0->state = 2;
            } else {
                work->field_20--;
            }
            return;
        case 2: {
            s32 t1e        = work->field_1E + 1;
            work->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188184[work->field_1C] < (s16)t1e) {
                (u16) work->field_1C = (u16)work->field_1C + 1;
                work->field_1E       = 0;
                if ((u16)D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            coord->coord.t[1] += work->field_16;
            coord->coord.t[2] += work->field_18;
            func_shelter_b3_dumping_hole_8017DA00(
                coord,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_8,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_A,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_2,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_6,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_4,
                work->field_8, 0x43C0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

static void func_shelter_b3_dumping_hole_8017E7DC(Task* arg0)
{
    DumpingHoleCoordWork* work;
    TmdObject*            extra;
    GfxCoord*             coord;
    ActorTransform*       placement;
    VECTOR                v;
    TmdObject*            e2;

    extra      = arg0->extra.tmd;
    placement  = arg0->spawnArg2.pointer;
    coord      = extra->coords;
    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    coord->parent          = &gGfxViewCoord;
    arg0->extra.tmd->flags = 0;
    Tmd_AllocBuffers(extra);
    extra->lightMtx   = &work->field_0;
    extra->colorMtx   = &work->field_20;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&coord->coord, placement->rot.vy, 1);
    gfxRotMatrixX(&coord->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&coord->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    taskReparent(D_shelter_b3_dumping_hole_8018F4A8, arg0);
    Gp_UpdateCoord(coord);
    e2   = arg0->extra.tmd;
    v.vx = e2->coords->workm.t[0];
    v.vy = arg0->extra.tmd->coords->workm.t[1];
    v.vz = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(e2, &v, 0, 3);
}

/// Debris thrown from the hole. Once the room signals, the piece is projected
/// to the screen: off-screen or behind the camera it is dropped, otherwise it
/// is launched away from the screen centre with a random speed and spin, and
/// then falls under a growing downward speed.
void func_shelter_b3_dumping_hole_8017E94C(Task* arg0)
{
    DumpingHoleCoordWork* work  = (DumpingHoleCoordWork*)arg0->work;
    u16                   flag  = ((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_40;
    GfxCoord*             coord = arg0->extra.tmd->coords;
    GfxCoord*             c2;
    DumpingHoleProjection p;
    s32                   sx;
    s32                   sy;
    s16                   angle;
    s32                   x;
    s32                   y;
    s32                   z;

    if (flag == 2) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            func_shelter_b3_dumping_hole_8017E7DC(arg0);
            arg0->state++;
            return;
        case 1:
            if (flag == 1) {
                arg0->state = 2;
            }
            return;
        case 2:
            p.pos.vx = coord->workm.t[0];
            p.pos.vy = coord->workm.t[1];
            p.pos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&p.pos);
            gte_rtps();
            gte_stsxy(&p.sx);
            gte_stszotz(&p.otz);
            sx = p.sx;
            sy = p.sy;
            if (sx < -0xA0) {
                taskKill(arg0);
                return;
            }
            if (sx > 0xA0) {
                taskKill(arg0);
                return;
            }
            if (sy < -0x78) {
                taskKill(arg0);
                return;
            }
            if (sy > 0x78) {
                taskKill(arg0);
                return;
            }
            if (p.otz < 0) {
                taskKill(arg0);
                return;
            }
            angle      = ratan2(sy, sx);
            work->velZ = rcos(angle) * ((DUMPING_HOLE_RAND() & 7) + 0x11) / 4096;
            work->velY = rsin(angle) * ((DUMPING_HOLE_RAND() & 7) + 5) / 4096;
            switch (arg0->spawnArg1.value) {
                case 0:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x32;
                    break;
                case 1:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x28;
                    break;
                case 2:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x1E;
                    break;
            }
            x = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                x = -x;
            }
            work->spinX = x;
            y           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                y = -y;
            }
            work->spinY = y;
            z           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                z = -z;
            }
            work->spinZ = z;
            work->fall  = 0;
            arg0->state++;
            return;
        case 3:
            work->rotX     += work->spinX;
            work->rotY     += work->spinY;
            work->rotZ     += work->spinZ;
            work->fall     += 5;
            c2              = arg0->extra.tmd->coords;
            c2->parent      = &gGfxViewCoord;
            c2->coord.t[0] += work->velX;
            c2->coord.t[1] += work->velY + work->fall;
            c2->coord.t[2] += work->velZ;
            gfxRotMatrixY(&c2->coord, (s16)work->rotY, 1);
            gfxRotMatrixX(&c2->coord, (s16)work->rotX, GRAPHICS_ROTATION_COMPOSE);
            c2->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
    }
}

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0)
{
    DumpingHoleEntity* work = (DumpingHoleEntity*)arg0->work;
    union {
        AnimationPlayRequest anim;
        ActorCommand         loc;
    } msg;
    u8 area;

    if (work->field_24 != NULL) {
        taskMessageDispatch(work->field_24, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->field_30) {
        case 0:
            break;
        case 1:
            switch (work->field_32) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_24, 0x3E9, D_shelter_b3_dumping_hole_8018819C, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_24, 0x3F2, &D_shelter_b3_dumping_hole_8018819C[6], 0);
                    work->field_32++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->field_24, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->field_34 = 0;
                        work->field_32++;
                    }
                    return;
                case 2:
                    if (++work->field_34 < 6) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId   = gPlayerStatus.weapon;
                        msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.animationId          = 0x2F;
                        msg.anim.blend                = ANIMATION_BLEND_INTERPOLATE;
                        msg.anim.blendFrames          = 0xA;
                        msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 2: {
            DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
            s32                weaponId   = gPlayerStatus.weapon;
            msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId          = 0x32;
            msg.anim.blend                = ANIMATION_BLEND_RESET;
            msg.anim.blendFrames          = 0;
            msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
        } break;
        case 3:
            taskMessageDispatch(work->field_24, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 4:
            taskMessageDispatch(work->field_24, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->field_24, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
            {
                DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
                s32                weaponId   = gPlayerStatus.weapon;
                msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.anim.animationId          = 9;
                msg.anim.blend                = ANIMATION_BLEND_RESET;
                msg.anim.blendFrames          = 0;
                msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
            }
            break;
        case 5:
            switch (work->field_32) {
                case 0:
                    msg.loc.context.loc.stage = gGameSession->location.loc.stage;
                    area                      = gGameSession->location.loc.area;
                    msg.loc.command           = 1;
                    msg.loc.context.loc.area  = area;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                    work->field_34 = 0;
                    work->field_32++;
                    return;
                case 1:
                    if (++work->field_34 < 0x10) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId   = gPlayerStatus.weapon;
                        msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.animationId          = 0x30;
                        msg.anim.blend                = ANIMATION_BLEND_INTERPOLATE;
                        msg.anim.blendFrames          = 0xA;
                        msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
            s32                weaponId   = gPlayerStatus.weapon;
            msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId          = 0x33;
            msg.anim.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.anim.blendFrames          = 0xA;
            msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
        }
            taskMessageDispatch(work->field_24, ANIMATION_MESSAGE_SET_RATE, 0x20, 0);
            break;
        case 7: {
            DumpingHoleEntity* w2         = (DumpingHoleEntity*)arg0->work;
            s32                weaponId   = gPlayerStatus.weapon;
            msg.anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId          = 0x31;
            msg.anim.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.anim.blendFrames          = 0xA;
            msg.anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_PLAY, &msg, 0);
        } break;
    }
    work->field_30 = 0;
}

static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0)
{
    DumpingHoleEntity*      work = (DumpingHoleEntity*)arg0->work;
    ActorCommand            msg;
    DumpingHoleDebrisSeed   seed;
    DumpingHoleDebrisEntry* e;
    DumpingHoleDebrisEntry* p;
    u16                     i;
    u8                      area;

    switch (work->field_38) {
        case 0:
            break;
        case 1:
            switch (work->field_3A) {
                case 0:
                    work->field_40 = 1;
                    work->field_42 = 2;
                    work->field_3C = 0;
                    work->field_3A++;
                    return;
                case 1:
                    if (++work->field_3C < 3) {
                        return;
                    }
                    taskMessageDispatch(((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_28, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    msg.context.loc.stage = gGameSession->location.loc.stage;
                    area                  = gGameSession->location.loc.area;
                    msg.command           = 2;
                    msg.context.loc.area  = area;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_28, 0x7D4, work, 0);
            msg.context.loc.stage = gGameSession->location.loc.stage;
            area                  = gGameSession->location.loc.area;
            msg.command           = 3;
            msg.context.loc.area  = area;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            break;
        case 4:
            msg.context.loc.stage = gGameSession->location.loc.stage;
            area                  = gGameSession->location.loc.area;
            msg.command           = 5;
            msg.context.loc.area  = area;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            break;
        case 5:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_2C, 0x7D4, &D_shelter_b3_dumping_hole_801881E4, 0);
            break;
        case 6:
            switch (work->field_3A) {
                case 0:
                    work->field_44 = 1;
                    work->field_3A++;
                    return;
                case 1:
                    for (i = 0; D_shelter_b3_dumping_hole_801881FC[i].pos.vx != SHELTER_B3_DUMPING_HOLE_TRANSFORM_END; i++) {
                        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 2, 0, &D_shelter_b3_dumping_hole_801881FC[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_80188304[i].pos.vx != SHELTER_B3_DUMPING_HOLE_TRANSFORM_END; i++) {
                        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 3, 1, &D_shelter_b3_dumping_hole_80188304[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_801884CC[i].x != 0xFFFF; i++) {
                        e = &D_shelter_b3_dumping_hole_801884CC[i];
                        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 4, 2, e);
                        if (e->field_18 != 0) {
                            p            = e;
                            seed.x       = p->x;
                            seed.y       = p->y;
                            seed.z       = p->z;
                            seed.field_8 = 0x1000;
                            seed.field_A = 1;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y + 200;
                            seed.z = p->z + 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y + 200;
                            seed.z = p->z - 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y - 200;
                            seed.z = p->z + 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y - 200;
                            seed.z = p->z - 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                        }
                    }
                    break;
                default:
                    return;
            }
            break;
        case 7:
            work->field_42 = 1;
            work->field_40 = 2;
            break;
    }
    work->field_38 = 0;
}

void func_shelter_b3_dumping_hole_8017F820(Task* arg0)
{
    DumpingHoleEntity*       work;
    DumpingHoleEntity*       w;
    Task*                    t;
    DumpingHoleEntity*       w2;
    AnimationBankCopyRequest msg;
    AnimationPlayRequest     anim;
    AnimationPlayRequest*    p;
    s32                      n;
    s32                      weaponId;

    switch (arg0->state) {
        case 0:
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->field_24                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_dumping_hole_8018F4A8 = arg0;
                work->field_28                     = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->field_0;
                work->field_2C                     = Gp_FindWorkById((gGameSession->location.loc.stage << 8) | (u16)(gGameSession->location.loc.area | 0x1000))->field_0;
                work->field_42                     = 0;
                work->field_40                     = 0;
                work->field_4A                     = 0;
                work->field_48                     = 0;
                work->field_46                     = 0;
            }
            w                                                   = (DumpingHoleEntity*)arg0->work;
            w->field_0                                          = w->field_28->extra.tmd->coords->coord.t[0];
            t                                                   = w->field_28;
            w->field_4                                          = t->extra.tmd->coords->coord.t[1];
            w->field_8                                          = t->extra.tmd->coords->coord.t[2];
            w->field_12                                         = 0x400;
            w->field_10                                         = 0;
            w->field_14                                         = 0;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xC;
            gStageSceneMusicEntry                               = 3;
            arg0->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE || gPlayerStatus.coordMtx->t[0] < 0x36B1) {
                break;
            }
            w2 = (DumpingHoleEntity*)arg0->work;
            n  = 0;
            while (D_shelter_b3_dumping_hole_801880A0[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_shelter_b3_dumping_hole_801880A0[0];
            msg.wordCount   = n & 0xFFFF;
            TASK_MESSAGE_DISPATCH_POINTER(w2->field_24, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            weaponId                  = gPlayerStatus.weapon;
            p                         = &anim;
            anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            p->animationId            = 1;
            p->blend                  = ANIMATION_BLEND_INTERPOLATE;
            p->blendFrames            = 0xA;
            anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &anim, 0);
            arg0->state++;
            break;
        case 2:
            func_800E8634(D_shelter_b3_dumping_hole_80188640, 0, D_shelter_b3_dumping_hole_80188A78);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                WorldCollisionTrigger* collision = &D_shelter_b3_dumping_hole_8018ECA4[8];
                collision->flags                &= ~WORLD_COLLISION_TRIGGER_ENABLED;
                taskKill(arg0);
                return;
            }
            func_shelter_b3_dumping_hole_8017EDB8(arg0);
            func_shelter_b3_dumping_hole_8017F1B0(arg0);
            break;
    }
}

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (gGameSession->location.loc.room == 2) {
        return 0;
    }
    return D_shelter_b3_dumping_hole_8018809C;
}

void func_shelter_b3_dumping_hole_8017FBA0(Task* arg0)
{
    OverlayFadeWork*   fade;
    OverlayFadeWork*   alloc;
    DumpingHoleEntity* ent;

    ent  = D_shelter_b3_dumping_hole_8018F4A8->work;
    fade = (OverlayFadeWork*)arg0->work;
    if (ent->field_4C == 1) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Sends message 0x7DA to the slot-4 task, tagged with the current session's
/// stage and area and the caller's selector. The actors' shared library carries
/// the same body.
void func_shelter_b3_dumping_hole_8017FCA0(s16 arg0)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

void func_shelter_b3_dumping_hole_8017FCF4(GfxCoord* arg0, SVECTOR* arg1)
{
    Task*                 task;
    DumpingHoleSpawnWork* work;

    task       = Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 1, 0, arg0);
    work       = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);
    work->field_C  = (u16)arg1->vx;
    work->field_E  = (u16)arg1->vy;
    work->field_10 = (u16)arg1->vz;
}

static void func_shelter_b3_dumping_hole_8017FD9C(GfxCoord* arg0, s32 arg1)
{
    if ((arg1 << 0x10) == 0) {
        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 3, 0, arg0);
        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 3, -0xA, arg0);
        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 3, 0xA, arg0);
    }
}

static void func_shelter_b3_dumping_hole_8017FE10(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    if (arg0 == 0) {
        p->field_48 = 1;
    }
}

void func_shelter_b3_dumping_hole_8017FE34(void)
{
    Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 1, 9, 0);
}

void func_shelter_b3_dumping_hole_8017FE64(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    taskMessageDispatch(p->field_28, ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FE9C(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    taskMessageDispatch(p->field_2C, ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FED4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    p->field_30          = arg0;
    p->field_32          = 0;
}

void func_shelter_b3_dumping_hole_8017FEF4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    p->field_38          = arg0;
    p->field_3A          = 0;
}

void func_shelter_b3_dumping_hole_8017FF14(void)
{
    Task*              st  = D_shelter_b3_dumping_hole_8018F4A8;
    DumpingHoleEntity* ent = st->work;
    DumpingHoleEntity* ent2;
    s32                desc[5];

    taskMessageDispatch(ent->field_2C, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
    taskMessageDispatch(ent->field_24, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    TASK_MESSAGE_DISPATCH_POINTER(ent->field_24, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
    ent2    = st->work;
    desc[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
    desc[1] = 9;
    desc[2] = 0;
    desc[3] = 0;
    desc[4] = 0;
    TASK_MESSAGE_DISPATCH_POINTER(ent2->field_24, ANIMATION_MESSAGE_PLAY, desc, 0);
    ent->field_40 = 2;
    ent->field_46 = 1;
    ent->field_42 = 1;
    ent->field_4C = 1;
    ent->field_44 = 1;
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_8017FFF4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_80180014(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_80180034(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// A shard thrown out of the hole, alive only while the room flag is set. On
/// its first frame it places itself from the spawn record and randomises its
/// velocity, spin and triangle shape; afterwards it falls and spins, is dropped
/// once its origin leaves the screen or passes behind the camera, and otherwise
/// draws itself as a shaded triangle.
void func_shelter_b3_dumping_hole_8018005C(Task* arg0)
{
    DumpingHoleShard*    work;
    GfxCoord*            coord;
    DumpingHoleShardCfg* cfg;
    POLY_G3*             prim;
    SVECTOR              ofs;
    s16                  x[3];
    s16                  y[3];
    SVECTOR              origin;
    s32                  sxy;
    s32                  otz;
    s16                  i;
    s16                  sx;
    s16                  sy;

    work  = (DumpingHoleShard*)arg0->work;
    coord = arg0->extra.coordBody->coord;
    cfg   = (DumpingHoleShardCfg*)arg0->spawnArg2.pointer;
    if (D_shelter_b3_dumping_hole_8018F4B0_value == 0) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(0x34, 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work          = (DumpingHoleShard*)arg0->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(arg0->work, 0, 0x34);
            Gp_ComposeParentWorld(cfg->parent, &coord->coord, &ofs);
            coord->coord.t[0] = ofs.vx + cfg->pos.vx;
            coord->coord.t[1] = ofs.vy + cfg->pos.vy;
            coord->coord.t[2] = ofs.vz + cfg->pos.vz;
            work->vel.vx      = cfg->vel.vx + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x1F) : -(DUMPING_HOLE_RAND() & 0x1F));
            work->vel.vy      = cfg->vel.vy + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x1F) : -(DUMPING_HOLE_RAND() & 0x1F));
            work->vel.vz      = cfg->vel.vz + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x1F) : -(DUMPING_HOLE_RAND() & 0x1F));
            work->fall        = cfg->fall;
            work->rotSpeed.vx = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            work->rotSpeed.vy = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            work->rotSpeed.vz = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            if (work->rotSpeed.vx > 0) {
                work->rotSpeed.vx += 100;
            } else {
                work->rotSpeed.vx -= 100;
            }
            if (work->rotSpeed.vy > 0) {
                work->rotSpeed.vy += 100;
            } else {
                work->rotSpeed.vy -= 100;
            }
            if (work->rotSpeed.vz > 0) {
                work->rotSpeed.vz += 100;
            } else {
                work->rotSpeed.vz -= 100;
            }
            work->verts[0].vx = 0;
            work->verts[0].vy = cfg->size + ((DUMPING_HOLE_RAND() & 1) ? cfg->size / 10 : 0);
            work->verts[0].vz = 0;
            work->verts[1].vx = cfg->size * rsin(0x2AA) / 4096 + ((DUMPING_HOLE_RAND() & 1) ? cfg->size / 10 : 0);
            work->verts[1].vy = -(cfg->size * rsin(0x155) / 4096) - ((DUMPING_HOLE_RAND() & 1) ? cfg->size / 10 : 0);
            work->verts[1].vz = 0;
            work->verts[2].vx = -(cfg->size * rsin(0x2AA) / 4096) - ((DUMPING_HOLE_RAND() & 1) ? cfg->size / 10 : 0);
            work->verts[2].vy = -(cfg->size * rsin(0x155) / 4096) - ((DUMPING_HOLE_RAND() & 1) ? cfg->size / 10 : 0);
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            work->vel.vy      += work->fall;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            Gp_UpdateCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            origin.vz = 0;
            origin.vy = 0;
            origin.vx = 0;
            gte_ldv0(&origin);
            gte_rtps();
            gte_stsxy(&sxy);
            gte_stszotz(&otz);
            sy = sxy >> 16;
            sx = sxy;
            if (sx < -0xA0) {
                goto kill;
            }
            if (sx > 0xA0 || sy < -0x78 || sy > 0x78 || otz < 0) {
            kill:
                taskKill(arg0);
                break;
            }
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, 0x10, 0x10, 0x10);
            setRGB1(prim, 0x40, 0x40, 0x40);
            setRGB2(prim, 0x80, 0x80, 0x80);
            prim->x0 = x[0];
            prim->y0 = y[0];
            prim->x1 = x[1];
            prim->y1 = y[1];
            prim->x2 = x[2];
            prim->y2 = y[2];
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            work->rot.vx += work->rotSpeed.vx;
            work->rot.vy += work->rotSpeed.vy;
            work->rot.vz += work->rotSpeed.vz;
            gfxRotMatrixY(&coord->coord, work->rot.vy, 1);
            gfxRotMatrixX(&coord->coord, work->rot.vx, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->rot.vz, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

/// Per-frame handler of the event task: runs the command in `state`, which is
/// cleared once handled unless it is a multi-frame sequence.
///
/// - 1 sends the slot-3 task its animation and the slot-4 task message 0x7DA
///   for the current stage and area.
/// - 2 is a sequence stepped by `step`. Step 0 sends the opening messages,
///   overrides the view vector and places the task at its starting pose.
///   Step 1 spawns a burst of table tasks every 16 frames and moves model
///   part 2; from frame 0x3D it also shrinks part 3 along X and Z, rebuilding
///   its rotation from identity each frame, and at frame 0x5B it spawns the
///   closing effects and advances. Until then the pose rotation alternates
///   either side of its starting value each frame. Step 2 keeps moving part 2
///   and, after six frames, tips part 1 about X. Every frame of the sequence
///   also shakes the screen by one unit.
/// - 3 and 5 undo the sequence's overrides and send the slot-4 task message
///   0x7DA; 5 also restores the saved view, resets the slot-3 animation and
///   ends the task in `field_88`.
/// - 4 sends the task in `field_84` message 0x7D5.
/// - 6 is a second sequence that alternates `func_shelter_b3_dumping_hole_80183218`
///   calls, then spawns the task kept in `field_88`.
///
/// The message buffers are unions because the cases share their stack slots.
static void func_shelter_b3_dumping_hole_8018098C(Task* task)
{
    DumpingHoleEntity4* work;
    GfxMatrix*          ident;
    GfxMatrix*          ident2;
    u16                 i;
    ActorCommand*       command3;
    ActorCommand*       command5;
    s32*                p;
    union {
        s32          words[5];
        ActorCommand loc;
        SVECTOR      vec[4];
    } buf;
    union {
        SVECTOR      vec;
        ActorCommand loc;
    } buf2;
    s32 words[5];

    work = task->work;
    switch (work->state) {
        case 1:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            buf.words[0]         = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            buf.words[1]         = 9;
            buf.words[2]         = 1;
            buf.words[3]         = 0xA;
            buf.words[4]         = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, buf.words, 0);
            buf.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf.loc.context.loc.area  = gGameSession->location.loc.area;
            buf.loc.command           = 0xA;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &buf.loc, ACTOR_COMMAND_MESSAGE_APPLY);
            work->state = 0;
            return;
        case 2:
            switch (work->step) {
                case 0:
                    taskMessageDispatch(work->field_80, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                    buf.vec[0].vx = 0x800;
                    buf.vec[0].vy = 0x800;
                    buf.vec[0].vz = 0x800;
                    Gp_SetOverrideVec(&buf.vec[0]);
                    taskMessageDispatch(work->field_84, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                    taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    work->pose.pos.vx = D_shelter_b3_dumping_hole_8018966C.pos.vx;
                    work->pose.pos.vy = D_shelter_b3_dumping_hole_8018966C.pos.vy;
                    work->pose.pos.vz = D_shelter_b3_dumping_hole_8018966C.pos.vz;
                    work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx;
                    work->pose.rot.vy = D_shelter_b3_dumping_hole_8018966C.rot.vy;
                    work->pose.rot.vz = D_shelter_b3_dumping_hole_8018966C.rot.vz;
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &work->pose, 0);
                    work->field_58.vx                        = 0;
                    work->field_58.vy                        = 0;
                    work->field_58.vz                        = 0;
                    D_shelter_b3_dumping_hole_8018F4B0_value = 1;
                    work->field_68                           = &task->extra.tmd->coords[2];
                    work->field_6C                           = 0x14;
                    work->scale.vx                           = 0x1000;
                    work->scale.vy                           = 0x1000;
                    work->scale.vz                           = 0x1000;
                    work->timer                              = 0;
                    work->step++;
                    break;
                case 1:
                    work->timer++;
                    if (!(gDisplayState.animFrame & 0xF)) {
                        work->field_60.vx = 0;
                        work->field_60.vy = 0;
                        work->field_60.vz = 0;
                        work->field_6E    = 4;
                        for (i = 0; i < 10; i++) {
                            Task_SpawnFromTable(D_shelter_b3_dumping_hole_80189ADC, 1, 0, &work->field_58);
                        }
                    }
                    if (work->timer >= 0x5B) {
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0xC8;
                        ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, task->extra.tmd->coords, 0x608, &buf.vec[3]);
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0x320;
                        ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, task->extra.tmd->coords, 0x608, &buf.vec[3]);
                        work->timer    = 0;
                        work->field_98 = 0;
                        work->field_9A = 0x80;
                        work->step++;
                        break;
                    }
                    if (work->timer >= 0x1F) {
                        if (work->timer >= 0x3D) {
                            task->extra.tmd->coords[2].coord.t[1] += 8;
                            work->scale.vx                        -= 10;
                            work->scale.vz                        -= 10;
                            ident                                  = (GfxMatrix*)&task->extra.tmd->coords[3].coord;

                            ident->rotationWords.m00M01 = ONE;
                            ident->rotationWords.m02M10 = 0;
                            ident->rotationWords.m11M12 = ONE;
                            ident->rotationWords.m20M21 = 0;
                            ident->rotationWords.m22    = ONE;

                            gfxScaleMatrixColumns(&task->extra.tmd->coords[3].coord, &work->scale);
                        } else if (work->timer < 0x20) {
                            task->extra.tmd->coords[2].coord.t[1] += 0x20;
                            ident2                                 = (GfxMatrix*)&task->extra.tmd->coords[3].coord;

                            ident2->rotationWords.m00M01 = ONE;
                            ident2->rotationWords.m02M10 = 0;
                            ident2->rotationWords.m11M12 = ONE;
                            ident2->rotationWords.m20M21 = 0;
                            ident2->rotationWords.m22    = ONE;
                        }
                        if (!(gDisplayState.animFrame & 0xF)) {
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0xC8;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(EFFECT_FLASH_BURST, task->extra.tmd->coords, 0x200, &buf2.vec);
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0x320;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(EFFECT_FLASH_BURST, task->extra.tmd->coords, 0x200, &buf2.vec);
                        }
                    }
                    if (gDisplayState.animFrame & 1) {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx + 0x10;
                    } else {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx - 0x10;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &work->pose, 0);
                    break;
                case 2:
                    if (!(gDisplayState.animFrame & 0xF)) {
                        buf2.vec.vx = -0x190;
                        buf2.vec.vy = 0x190;
                        buf2.vec.vz = 0x190;
                        Gp_SpawnEff(EFFECT_FLASH_BURST, &task->extra.tmd->coords[1], 0x200, &buf2.vec);
                    }
                    if (++work->timer >= 6) {
                        if (work->field_98 < -0x154) {
                            work->field_98 -= 1;
                        } else {
                            work->field_98 -= 8;
                        }
                        gfxRotMatrixX(&task->extra.tmd->coords[1].coord, work->field_98, GRAPHICS_ROTATION_REPLACE);
                    }
                    task->extra.tmd->coords[2].coord.t[1] += 0x190;
                    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &work->pose, 0);
                    break;
            }
            if (gDisplayState.animFrame & 1) {
                displaySetShakeY(1);
            } else {
                displaySetShakeY(-1);
            }
            return;
        case 3:
            Gp_SetOverrideVec(NULL);
            taskMessageDispatch(work->field_80, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->field_84, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            D_shelter_b3_dumping_hole_8018F4B0_value = 0;
            work->field_96                           = 1;
            Gp_PulseState1C();
            buf2.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf2.loc.context.loc.area  = gGameSession->location.loc.area;
            command3                   = &buf2.loc;
            command3->command          = 0xB;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, command3, ACTOR_COMMAND_MESSAGE_APPLY);
            displaySetShakeY(0);
            work->state = 0;
            return;
        case 4:
            taskMessageDispatch(work->field_84, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 5:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->field_94;
            taskMessageDispatch(work->field_80, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->field_84, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            work->field_96 = 1;
            Gp_PulseState1C();
            buf2.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf2.loc.context.loc.area  = gGameSession->location.loc.area;
            command5                   = &buf2.loc;
            command5->command          = 0xC;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, command5, ACTOR_COMMAND_MESSAGE_APPLY);
            p        = words;
            words[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            p[1]     = 1;
            p[2]     = 1;
            p[3]     = 0xA;
            words[4] = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, words, 0);
            if (work->field_88 != NULL) {
                Task_CallExit(work->field_88);
                work->field_88 = NULL;
            }
            break;
        case 0:
            break;
        case 6:
            switch (work->step) {
                case 1:
                case 3:
                case 5:
                    func_shelter_b3_dumping_hole_80183218(0);
                    work->timer = 0;
                    work->step++;
                    return;
                case 0:
                case 2:
                case 4:
                    func_shelter_b3_dumping_hole_80183218(1);
                    work->timer = 0;
                    work->step++;
                    return;
                case 6:
                    if (++work->timer >= 0xB) {
                        work->field_88 = Task_Spawn(1, 0x2D, 0x10, 0);
                        work->timer    = 0;
                        work->step++;
                    }
                    return;
                case 7:
                    if (++work->timer >= 6) {
                        func_shelter_b3_dumping_hole_80183218(1);
                        func_shelter_b3_dumping_hole_80183218(2);
                        break;
                    }
                    return;
                default:
                    return;
            }
            break;
    }
    work->state = 0;
}

void func_shelter_b3_dumping_hole_80181430(void)
{
    DumpingHoleEntity4* ent;
    ActorCommand        request;
    s32                 desc3[5];
    s32*                p3;

    ent = D_shelter_b3_dumping_hole_8018F4AC->work;
    Gp_SetOverrideVec(NULL);
    if (ent->field_88 != NULL) {
        Task_CallExit(ent->field_88);
        ent->field_88 = NULL;
    }
    ent->field_96 = 1;
    Gp_PulseState1C();

    D_shelter_b3_dumping_hole_8018F4B0_value = 0;
    request.context.loc.stage                = gGameSession->location.loc.stage;
    request.context.loc.area                 = gGameSession->location.loc.area;
    request.command                          = 0x13;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);

    displaySetShakeY(0);
    taskMessageDispatch(ent->field_84, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    taskMessageDispatch(ent->field_80, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);

    p3       = desc3;
    desc3[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, desc3, 0);
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_80181560(Task* task)
{
    s32                 desc[5];
    TmdObject*          obj;
    TmdObject*          tail;
    DumpingHoleEntity4* work;

    switch (task->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            obj        = task->extra.tmd;
            task->work = memCalloc(0xA0, false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                task->extra.tmd->coords->parent = &gGfxViewCoord;
                work                            = task->work;
                memFillBytes(work, 0, sizeof(*work));
                work->field_80                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_dumping_hole_8018F4AC = task;
                work->field_84                     = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->field_0;
                obj->lightMtx                      = &work->lightMtx;
                obj->colorMtx                      = &work->colorMtx;
                task->msgTable                     = D_shelter_b3_dumping_hole_8018965C;
                func_shelter_b3_dumping_hole_80183218(0);
            }
            D_shelter_b3_dumping_hole_8018F4D8          = 0;
            ((DumpingHoleEntity4*)task->work)->field_94 = gGameSession->location.loc.view;
            Gp_MsgPlayerWeapon(0);
            desc[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            desc[1] = 9;
            desc[2] = 1;
            desc[3] = 0xA;
            desc[4] = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, desc, 0);
            task->state++;
            break;
        case 1:
            D_shelter_b3_dumping_hole_8018809C = 0;
            func_800E8634(D_shelter_b3_dumping_hole_8018968C, 0, D_shelter_b3_dumping_hole_801899A4);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(GAME_FLAG_11D, 1);
                taskKill(task);
            } else {
                func_shelter_b3_dumping_hole_8018098C(task);
            }
            break;
    }
    tail    = task->extra.tmd;
    desc[0] = tail->coords->workm.t[0];
    desc[1] = task->extra.tmd->coords->workm.t[1];
    desc[2] = task->extra.tmd->coords->workm.t[2];
    func_800D7A9C(tail, (VECTOR*)desc, 0, 3);
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

void func_shelter_b3_dumping_hole_801818E0(void)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    if (p->field_9C == 0) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x20);
        gSceneCombatState.battleRefs = 0;
        gGameSession->flowFlags     |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        p->field_9C                  = 1;
    }
}

void func_shelter_b3_dumping_hole_80181958(s32 arg0)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    taskMessageDispatch(p->field_80, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_shelter_b3_dumping_hole_80181990(s16 arg0)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    p->state              = arg0;
    p->step               = 0;
}

void func_shelter_b3_dumping_hole_801819B0(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_801819D0(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_801819F0(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns the task described by `D_shelter_b3_dumping_hole_8018AFBC`.
void func_shelter_b3_dumping_hole_80181A18(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018AFBC, 0, 0, 0);
}

void func_shelter_b3_dumping_hole_80181A48(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->spawnArg1.value = 3;
            arg0->killCountdown   = 8;
            arg0->state          += 1;
            break;
        case 1:
            if (--arg0->killCountdown < 0) {
                arg0->state += 1;
            }
            displaySetShakeY(arg0->spawnArg1.value);
            arg0->spawnArg1.value = -arg0->spawnArg1.value;
            break;
        default:
            displaySetShakeY(0);
            taskKill(arg0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80181B04(s16 arg0)
{
    func_shelter_b3_dumping_hole_8017FD9C(
        &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[1], arg0);
}

void func_shelter_b3_dumping_hole_80181B44(s32 arg0)
{
    func_shelter_b3_dumping_hole_8017FE10(arg0);
}

#include "../../shared/cap_captions.inc.c"

static void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_ShowTimed(arg0, arg1, arg2);
}

#include "../../shared/cap_captions_resource.inc.c"

void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_LoadResource(arg0, arg1, arg2);
}

/// Hides or shows sprite commands 1 and 2 of the area's view 13 through their
/// `SpriteBatch::hidden`: 0 hides both, 1 shows command 2 and 2 shows command 1.
static void func_shelter_b3_dumping_hole_80183218(u8 arg0)
{
    GameLocationKey* g4      = &gGameSession->location.loc;
    SpriteBatch*     batches = Gp_SprtTables[g4->stage - 1]->areaViews[g4->area - 1][13].batches;

    if (arg0 == 0) {
        batches[1].hidden = 1;
        batches[2].hidden = 1;
    } else if (arg0 == 1) {
        batches[2].hidden = 0;
    } else if (arg0 == 2) {
        batches[1].hidden = 0;
    }
}

/// Spawns the two enemies of one slot from the `D_80151E60` table, numbering
/// them from the spawn counter, and marks the slot live. Actor 342400 carries
/// the same body.
static void func_shelter_b3_dumping_hole_80183298(Task* arg0)
{
    OverlayEncounterPairWork* work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(0xC, 0);
    if (work == NULL) {
        goto kill;
    }
    arg0->work   = work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
    kill:
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << ENEMY_PLACE_INDEX_SHIFT;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << ENEMY_PLACE_INDEX_SHIFT;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
    arg0->state++;
}

static void func_shelter_b3_dumping_hole_801833EC(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s16                       i;
    s16                       idx;
    s16                       type;
    s16                       arg;

    count = 0;
    for (i = 0; i < 16; i++) {
        if (D_shelter_b3_dumping_hole_8018B7BC[i].status == 1) {
            count++;
        }
    }
    if (count < 3) {
        idx = ent->nextSlot;
        if (idx < 16 && gGameSession->sceneClock >= 0x3D) {
            type = D_shelter_b3_dumping_hole_8018B7BC[idx].kind;
            arg  = D_shelter_b3_dumping_hole_8018B7BC[idx].command;
            switch (type) {
                case 0:
                    Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 1, (idx << 16) + arg, 0);
                    break;
                case 1:
                    Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 2, (idx << 16) + arg, 0);
                    break;
                case 2:
                    Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 3, (idx << 16) + arg, 0);
                    break;
            }
            ent->nextSlot++;
        }
    }
}

void func_shelter_b3_dumping_hole_80183530(Task* arg0, s32 arg1, ActorCommand* request)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    if (request->command == 4) {
        ent->stop = request->command;
    }
}

/// States of the task that works through the room's 16 enemy slots: set-up,
/// the first three slot spawns, a 15-frame wait before `Gp_ArmStateF0`, and a
/// loop that starts the next slot while fewer than three are live and ends
/// once all 16 have been cleared.
static const TaskFuncTable4 D_shelter_b3_dumping_hole_8017D654 = { {
    func_shelter_b3_dumping_hole_801836E0,
    func_shelter_b3_dumping_hole_8018378C,
    func_shelter_b3_dumping_hole_80183824,
    func_shelter_b3_dumping_hole_801838A0,
} };

/// States of a slot task holding one enemy from `D_80142604`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D664 = { {
    func_shelter_b3_dumping_hole_80183950,
    func_shelter_b3_dumping_hole_80183A00,
    func_shelter_b3_dumping_hole_80183A98,
} };

/// States of a slot task holding one enemy from `D_801575F0`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D670 = { {
    func_shelter_b3_dumping_hole_80183AEC,
    func_shelter_b3_dumping_hole_80183B9C,
    func_shelter_b3_dumping_hole_80183C38,
} };

/// States of a slot task holding a pair of enemies from `D_80151E60`: spawn
/// them, one idle frame, send the first message 0x7DB, send the second the
/// same after a delay, then wait until both are gone.
static const TaskFuncTable5 D_shelter_b3_dumping_hole_8017D67C = { {
    func_shelter_b3_dumping_hole_80183298,
    func_shelter_b3_dumping_hole_80183C8C,
    func_shelter_b3_dumping_hole_80183CA0,
    func_shelter_b3_dumping_hole_80183D34,
    func_shelter_b3_dumping_hole_80183E08,
} };

void func_shelter_b3_dumping_hole_80183550(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_shelter_b3_dumping_hole_8017D654;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

void func_shelter_b3_dumping_hole_801835C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D664;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183620(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D670;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183678(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_shelter_b3_dumping_hole_8017D67C;
    sp.funcs[task->state](task);
}

static void func_shelter_b3_dumping_hole_801836E0(Task* arg0)
{
    OverlayEncounterCtrlWork* work;
    s32                       i;

    if (gGameSession->spawnPhase[0] == GAME_SESSION_SPAWN_COMPLETE) {
        taskKill(arg0);
        return;
    }
    work = memCalloc(6, 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 15; i >= 0; i--) {
        D_shelter_b3_dumping_hole_8018B7BC[i].status = 0;
    }
    D_shelter_b3_dumping_hole_8018F4D4_value = 0;
    arg0->work                               = work;
    arg0->msgTable                           = D_shelter_b3_dumping_hole_8018B7AC;
    arg0->state                             += 1;
}

static void func_shelter_b3_dumping_hole_8018378C(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    s32                       i;

    for (i = 0; i < 3; i++) {
        s16 idx = ent->nextSlot;
        func_shelter_b3_dumping_hole_80183E6C(idx, D_shelter_b3_dumping_hole_8018B7BC[idx].kind,
                                              D_shelter_b3_dumping_hole_8018B7BC[idx].command);
        ent->nextSlot += 1;
    }
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183824(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    if ((s16)(ent->frames += 1) == 0xF) {
        (Gp_IncStateF0Ref)(0);
        gGameSession->spawnPhase[0] = GAME_SESSION_SPAWN_ARMED;
        Gp_ArmStateF0(1);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_801838A0(Task* arg0)
{
    s16 count;
    s32 i;

    count = 0;
    if (((OverlayEncounterCtrlWork*)arg0->work)->stop != 4) {
        func_shelter_b3_dumping_hole_801833EC(arg0);
        for (i = 0; i < 0x10; i++) {
            if (D_shelter_b3_dumping_hole_8018B7BC[i].status == 2) {
                count++;
            }
        }
        if (count == 0x10) {
            Gp_ReleaseStateF0Clear(arg0, 0);
            gGameSession->spawnPhase[0] = GAME_SESSION_SPAWN_COMPLETE;
            taskKill(arg0);
        }
    }
}

static void func_shelter_b3_dumping_hole_80183950(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(8, 0);
    if (work != NULL) {
        Enemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_80142604, 1, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            idx                                                                           = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                                   = enemy;
            enemy->placeKey                                                               = idx << ENEMY_PLACE_INDEX_SHIFT;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                                  += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183A00(Task* arg0)
{
    ActorCommand                request;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    Enemy*                      t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x2E) {
        TmdObject* p              = t00->extra.tmd;
        p->clutRowOffset          = 2;
        p->texturePageOffset      = 0;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2C;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_80183A98(Task* arg0)
{
    if (((OverlayEncounterSingleWork*)arg0->work)->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}

static void func_shelter_b3_dumping_hole_80183AEC(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(8, 0);
    if (work != NULL) {
        Enemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            idx                                                                           = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                                   = enemy;
            enemy->placeKey                                                               = idx << ENEMY_PLACE_INDEX_SHIFT;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                                  += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183B9C(Task* arg0)
{
    ActorCommand                request;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    Enemy*                      t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x3D) {
        TmdObject* p              = t00->extra.tmd;
        p->texturePageOffset      = 2;
        p->clutRowOffset          = 4;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2A;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_80183C38(Task* arg0)
{
    if (((OverlayEncounterSingleWork*)arg0->work)->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}

/// Advances the task to its next state.
static void func_shelter_b3_dumping_hole_80183C8C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_shelter_b3_dumping_hole_80183CA0(Task* arg0)
{
    ActorCommand              request;
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    Enemy*                    t0  = ent->enemy0;

    if (t0 != NULL) {
        Task*      t00            = t0->task;
        TmdObject* p              = t00->extra.tmd;
        p->texturePageOffset      = 3;
        p->clutRowOffset          = 5;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2E;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
    }
    ent->frames  = 0;
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183D34(Task* arg0)
{
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    Enemy*                    t   = ent->enemy1;

    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->enemy1 != NULL) {
        if ((s16)(ent->frames += 1) < 0x3D) {
            return;
        }
        {
            Task*        t00 = ent->enemy1->task;
            TmdObject*   p   = t00->extra.tmd;
            ActorCommand request;
            p->texturePageOffset      = 3;
            p->clutRowOffset          = 5;
            t->workType               = ENEMY_WORK_PLAIN;
            request.context.loc.stage = 0;
            request.context.loc.area  = 0x2E;
            request.command           = arg0->spawnArg1.value;
            TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        }
    }
    ent->frames  = 0;
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183E08(Task* arg0)
{
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->goneMask == 3) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}

static void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 1, (arg0 << 16) + arg2, 0);
            break;
        case 1:
            Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 2, (arg0 << 16) + arg2, 0);
            break;
        case 2:
            Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 3, (arg0 << 16) + arg2, 0);
            break;
    }
}

static void func_shelter_b3_dumping_hole_80183F04(Task* arg0)
{
    OverlayEncounterPairWork* p = (OverlayEncounterPairWork*)arg0->work;

    if (p->enemy0 != NULL) {
        if (p->enemy0->hp <= 0) {
            p->enemy0 = NULL;
        }
    } else {
        p->goneMask |= 1;
    }
    if (p->enemy1 != NULL) {
        if (p->enemy1->hp <= 0) {
            p->enemy1 = NULL;
        }
    } else {
        p->goneMask |= 2;
    }
}

void func_shelter_b3_dumping_hole_80183F84(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b3_dumping_hole_8018F4D8 = 0;
        task->state                        = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x100);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x300);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[4], 0x300, 0x400);
            break;
        case 3:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x300);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x400);
            break;
        case 4:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[9], 0x300, 0x400);
            break;
        case 7:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 33)[0], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 33)[1], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 33)[2], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 33)[3], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 33)[4], 0x300, 0x400);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 30)[0], 0x280, 0x44);
                glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 30)[1], 0x280, 0x40);
            }
            break;
        case 15:
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x100);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[0x21], 0x300, 0x200);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[0x22], 0x300, 0x300);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[0x23], 0x300, 0x400);
            break;
        case 17:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 36)[0], 0x300, 0x400);
            break;
        case 18:
            glowDrawCapsule(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0], 0x200, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x12], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x13], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x19], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x1A], 0x300, 0x300);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x1B], 0x300, 0x200);
            break;
        case 19:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x400);
            break;
        case 21:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 24)[0], 0x400, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 24)[1], 0x400, 0x444);
            break;
        case 22:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 30)[0], 0x280, 0x44);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 30)[1], 0x280, 0x40);
            break;
        case 23:
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x400);
            break;
        case 26:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 26)[0], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 26)[1], 0x300, 0x400);
            break;
        case 29:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[2], 0x280, 0x44);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[3], 0x280, 0x40);
            break;
        case 30:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[2], 0x280, 0x44);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 28)[3], 0x280, 0x40);
            break;
        case 31:
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x200);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x300);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x400);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x200);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x300);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x400);
            break;
        case 34:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x300);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x300);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[4], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[5], 0x300, 0x400);
            break;
        case 35:
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x400);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[6], 0x300, 0x200);
            glowDrawDisc(&(D_shelter_b3_dumping_hole_8018B86C + 32)[7], 0x300, 0x400);
            break;
        case 13:
        case 37:
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[6], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[16], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[25], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[27], 0x400, 0x444);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x100);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x200);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x300);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], 0x300, 0x400);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x100);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x200);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x300);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/effect_sprite_drift_aimed.inc.c"

#include "../../shared/effect_sprite_draw_banked.inc.c"

#include "../../shared/effect_sprite_draw_rotated.inc.c"

/// Per-frame update of an effect task drawn with
/// `effectSpriteDrawChip` (state 1) or
/// `effectSpriteDrawBillboard` (state 2). State 0 seeds the work from
/// `spawnArg1` and, when `move` is zero, picks a random velocity scaled
/// through the GTE. Later ticks draw, drift the coordinate by that velocity
/// with `vy` growing by 6, and advance the frame every `period` ticks,
/// releasing the task after frame 7. While an event is running the task only
/// draws, and is released once the event state reaches 4.
void func_shelter_b3_dumping_hole_80186218(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         kind;
    s32         step;
    s32         state;
    s32         level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < 2) {
                effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            } else {
                effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            }
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale     = task->spawnArg1.halves.low & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0xFFC0 - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            effectKillTask(work, task);
        }
    }
}

#include "../../shared/effect_sprite_draw_chip.inc.c"

#include "../../shared/effect_sprite_draw_billboard.inc.c"

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    MATRIX*     m;
    s32         i;

    mem   = (EffectWork*)arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectSpriteDrawBillboard(coord, (mem->age / 2) & 0xFFFF, 0x380);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                                = &coord->coord;
        coord->parent                    = mem->parent;
        MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
        MATRIX_PAIR(m, 0, 2)             = 0;
        MATRIX_PAIR(m, 1, 1)             = 0x1000;
        MATRIX_PAIR(m, 2, 0)             = 0;
        m->m[2][2]                       = 0x1000;
        coord->coord.t[2]                = 0;
        coord->coord.t[1]                = 0;
        coord->coord.t[0]                = 0;
        coord->composeStamp              = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->age += 1;
    switch (arg0->spawnArg1.value) {
        case 0:
            Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x14002400, NULL);
            arg0->spawnArg1.value = 1;
            return;
        case 1:
            effectSpriteDrawBillboard(coord, (mem->age / 2) & 0xFFFF, 0x380);
            if (!(mem->age & 1)) {
                Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x1001400, NULL);
            }
            mem->age += 1;
            return;
        case 2:
            Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x2002400, NULL);
                Gp_SpawnEff(EFFECT_SHELTER_B3_DUMPING_HOLE_DRIFT_SPRITE, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1.value = 3;
            return;
        case 3:
            effectKillTask(mem, arg0);
            return;
    }
}
