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
#include "gameplay/direction_input.h"
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
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
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

#define DUMPING_HOLE_RAND() ((s32)((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16))

/// Spawns one debris task and gives it a work block seeded with `seed`.
#define DUMPING_HOLE_SPAWN_DEBRIS(seed)                                                              \
    {                                                                                                \
        Task*                  t = Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 0, 0, 0); \
        DumpingHoleDebrisSeed* w = Mem_Malloc(0x24, 0);                                              \
        t->work                  = w;                                                                \
        if (w == NULL) {                                                                             \
            taskKill(t);                                                                             \
        } else {                                                                                     \
            Mem_Set(w, 0, 0x24);                                                                     \
            *w = seed;                                                                               \
        }                                                                                            \
    }

extern SVECTOR D_shelter_b3_dumping_hole_8018B86C[44];

static void func_shelter_b3_dumping_hole_80181B64(Task* task, s32 arg1);

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4D4[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4D4_value __asm__("D_shelter_b3_dumping_hole_8018F4D4");

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b3_dumping_hole_8018F4D0[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b3_dumping_hole_8018F4D0_value __asm__("D_shelter_b3_dumping_hole_8018F4D0");

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4B0[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4B0_value __asm__("D_shelter_b3_dumping_hole_8018F4B0");

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[4];
        GpCopyArg  copy;
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
    s32 field_0;
    s32 field_4;
    s32 field_8;
    u8  pad_C[0x4];
    s16 field_10;
    s16 field_12;
    s16 field_14;
} DumpingHoleCoordCfg;

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

/// An entry of the spawn tables walked by message state 6; a 0xFFFF first
/// word ends the table.
typedef struct {
    s32 field_0;
    u8  pad_4[0x14];
} DumpingHoleSpawnEntry;

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
    SVECTOR  pos;
    SVECTOR  vel;
    GpCoord* parent;
    u16      size;
    u16      fall;
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
    MATRIX     lightMtx;
    MATRIX     colorMtx;
    GpXformArg pose;     // Sent to the task itself with message 0x7D4
    SVECTOR    field_58; // Spawn parameters handed by address to the table spawns
    SVECTOR    field_60;
    GpCoord*   field_68;
    s16        field_6C;
    s16        field_6E;
    VECTOR     scale; // Per-axis scale applied to the rotation of model part 3
    Task*      field_80;
    Task*      field_84;
    Task*      field_88;
    u16        state; // One-shot command, cleared once handled
    u16        step;  // Progress through the sequence the command started
    u16        timer; // Frames spent in the current step
    u8         pad_92[0x2];
    s16        field_94;
    s16        field_96;
    s16        field_98; // X rotation of model part 1 once the sequence reaches step 2
    s16        field_9A;
    u16        field_9C; // Latch: set once the state-F0 release has been issued
    u8         pad_9E[0x2];
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
extern GpXformArg             D_shelter_b3_dumping_hole_801881CC;
extern GpXformArg             D_shelter_b3_dumping_hole_801881E4;
extern DumpingHoleSpawnEntry  D_shelter_b3_dumping_hole_801881FC[];
extern DumpingHoleSpawnEntry  D_shelter_b3_dumping_hole_80188304[];
extern DumpingHoleDebrisEntry D_shelter_b3_dumping_hole_801884CC[];

extern GpEvsCmd   D_shelter_b3_dumping_hole_80188640[];
extern GpEvsCmd   D_shelter_b3_dumping_hole_80188A78[];
extern GpObj4C    D_shelter_b3_dumping_hole_8018ECA4[10];
extern Task*      D_shelter_b3_dumping_hole_8018F4AC;
extern GpXformArg D_shelter_b3_dumping_hole_8018966C;

extern s32 D_shelter_b3_dumping_hole_8018F4D8;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, GpCmdArg*);
        void (*call1)(Task*, s32, GpXformArg*);
        void (*call2)(Task*, s32, s32);
    } handler;
} ShelterB3DumpingHole2MessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3DumpingHole2MessageEntry, 8);

extern ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018965C[2];
extern GpEvsCmd                          D_shelter_b3_dumping_hole_8018968C[];
extern GpEvsCmd                          D_shelter_b3_dumping_hole_801899A4[];
extern TaskDesc                          D_shelter_b3_dumping_hole_8018AFBC;
extern GpEvt12*                          D_shelter_b3_dumping_hole_8018F4BC;
extern s16                               D_shelter_b3_dumping_hole_8018F4C6;
extern OverlayCapWindow                  D_shelter_b3_dumping_hole_8018B5A0[];
extern GlyphUvwh*                        D_shelter_b3_dumping_hole_8018F4B8;
extern GpCapEntry*                       D_shelter_b3_dumping_hole_8018F4B4;
extern s16                               D_shelter_b3_dumping_hole_8018F4C0;
extern s16                               D_shelter_b3_dumping_hole_8018F4C2;
extern s16                               D_shelter_b3_dumping_hole_8018F4C4;
extern s16                               D_shelter_b3_dumping_hole_8018F4C8;
extern s16                               D_shelter_b3_dumping_hole_8018F4CA;

extern s16      D_shelter_b3_dumping_hole_8018B578;
extern s16      D_shelter_b3_dumping_hole_8018B57A;
extern u16      D_shelter_b3_dumping_hole_8018F4CC;
extern u16      D_shelter_b3_dumping_hole_8018F4CE;
extern s32      D_shelter_b3_dumping_hole_8018B670;
extern s32      D_shelter_b3_dumping_hole_8018B674;
extern TaskDesc D_shelter_b3_dumping_hole_8018B588;
extern TaskDesc D_80142604;
extern TaskDesc D_801575F0;
extern TaskDesc D_shelter_b3_dumping_hole_8018B594;

extern ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018B7AC[2];

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0);
static s32  func_shelter_b3_dumping_hole_80181E70(s16 arg0, s16 arg1, s32 arg2);
static void func_shelter_b3_dumping_hole_80184638(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b3_dumping_hole_80184E7C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b3_dumping_hole_801866CC(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b3_dumping_hole_80186AB8(GpCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183218(u8 arg0);
static void func_shelter_b3_dumping_hole_8017FD9C(GpCoord* arg0, s32 arg1);
static void func_shelter_b3_dumping_hole_80181C8C(void);
static s32  func_shelter_b3_dumping_hole_80181F80(u16* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b3_dumping_hole_80182AA0(void);
static s32  func_shelter_b3_dumping_hole_80182FD0(s32 arg0);
static s16  func_shelter_b3_dumping_hole_80182C24(u16* arg0);
static s16  func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1);
static s32  func_shelter_b3_dumping_hole_80182F18(u16* arg0);
static s16  func_shelter_b3_dumping_hole_801829B4(u16* arg0);
static s32  func_shelter_b3_dumping_hole_80182E50(u16* arg0);
static void func_shelter_b3_dumping_hole_801833EC(Task* arg0);
static void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2);
static s32  func_shelter_b3_dumping_hole_80181D68(GpCapFileAddress base);
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
static void func_shelter_b3_dumping_hole_8018596C(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b3_dumping_hole_80185DCC(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

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
void func_shelter_b3_dumping_hole_801817D8(Task*, s32, s32);
void func_shelter_b3_dumping_hole_80181854(Task*, s32, GpXformArg*);
void func_shelter_b3_dumping_hole_801818E0(void);
void func_shelter_b3_dumping_hole_80181958(s32);
void func_shelter_b3_dumping_hole_80181990(s16);
void func_shelter_b3_dumping_hole_801819B0(void);
void func_shelter_b3_dumping_hole_801819D0(void);
void func_shelter_b3_dumping_hole_801819F0(void);

void func_shelter_b3_dumping_hole_80181A48(Task*);

extern GpAnimSet                           D_shelter_b3_dumping_hole_80189DD0;
extern GpAnimSet                           D_shelter_b3_dumping_hole_8018A274;
extern GpAnimSet                           D_shelter_b3_dumping_hole_8018AF84;
extern ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8;

extern GpAnimArg                           D_shelter_b3_dumping_hole_8018AFF4;
extern GpAnimArg                           D_shelter_b3_dumping_hole_8018B008;
extern GpAnimArg                           D_shelter_b3_dumping_hole_8018B01C;
extern GpCmdArg                            D_shelter_b3_dumping_hole_8018B078;
extern GpScriptCmd                         D_shelter_b3_dumping_hole_8018AFAC[2];
extern GpScriptRec                         D_shelter_b3_dumping_hole_8018AFB4[2];
extern GpXformArg                          D_shelter_b3_dumping_hole_8018B030;
extern GpXformArg                          D_shelter_b3_dumping_hole_8018B048;
extern GpXformArg                          D_shelter_b3_dumping_hole_8018B060;
extern ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8;
void                                       func_shelter_b3_dumping_hole_80181A18(void);
void                                       func_shelter_b3_dumping_hole_80181B04(s16);
void                                       func_shelter_b3_dumping_hole_80181B44(s32);

extern GpGridParams D_shelter_b3_dumping_hole_8018C3EC[1];
extern GpObj4C      D_shelter_b3_dumping_hole_8018E88C[8];
extern GpObj4C      D_shelter_b3_dumping_hole_8018EF9C[8];
void                func_shelter_b3_dumping_hole_80183024(Task*);
void                func_shelter_b3_dumping_hole_80183060(Task*);

void func_shelter_b3_dumping_hole_80183530(Task*, s32, GpCmdArg*);
void func_shelter_b3_dumping_hole_80183550(Task*);
void func_shelter_b3_dumping_hole_801835C8(Task*);
void func_shelter_b3_dumping_hole_80183620(Task*);
void func_shelter_b3_dumping_hole_80183678(Task*);

extern GpDrawAreaRec D_shelter_b3_dumping_hole_8018D3E0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018C944[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018C954[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018C964[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CAA0[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CAB8[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CAC8[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CAD8[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CC28[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CC40[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CC50[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CC60[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018CC70[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018D3B0[6];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018D7B4[4];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018D964[4];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018D984[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DB24[4];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DBE4[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DBFC[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DC0C[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DD34[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DD4C[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DD5C[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DD80[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DD98[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DF88[3];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFA0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFB0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFC0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFD0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFE0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018DFF0[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018E000[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018E010[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018E020[2];
extern GpSprtCmd     D_shelter_b3_dumping_hole_8018E030[2];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018C974[15];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018CAE8[16];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018CC80[92];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018D3F4[48];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018D7D4[20];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018D994[20];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018DB44[8];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018DC1C[14];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018DD6C[1];
extern GpSprtElem    D_shelter_b3_dumping_hole_8018DDA8[24];
extern TaskDesc      D_80164B78;
extern TaskDesc      D_80174D58;
extern TaskDesc      D_shelter_b3_dumping_hole_80188BC8[5];

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

GpXformArg D_shelter_b3_dumping_hole_801881CC = { { 0x290E, 0, -6000, 0 }, { 0, 3072, 0, 0 } };

GpXformArg D_shelter_b3_dumping_hole_801881E4 = { { 5000, 1800, -6000, 0 }, { 0, 0, 0, 0 } };

DumpingHoleSpawnEntry D_shelter_b3_dumping_hole_801881FC[11] = {
    { 9000, { 24, 252, 255, 255, 36, 250, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8900, { 192, 249, 255, 255, 20, 236, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8200, { 4, 247, 255, 255, 76, 235, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 248, 248, 255, 255, 88, 233, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8400, { 248, 248, 255, 255, 12, 229, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9100, { 80, 251, 255, 255, 88, 233, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9000, { 80, 251, 255, 255, 168, 228, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9000, { 224, 252, 255, 255, 188, 233, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8400, { 204, 247, 255, 255, 144, 232, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 7900, { 172, 244, 255, 255, 132, 234, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0xFFFF, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

DumpingHoleSpawnEntry D_shelter_b3_dumping_hole_80188304[19] = {
    { 8800, { 24, 252, 255, 255, 244, 232, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 160, 246, 255, 255, 200, 231, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 136, 250, 255, 255, 224, 227, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 36, 250, 255, 255, 212, 229, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9100, { 180, 251, 255, 255, 44, 232, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9000, { 136, 250, 255, 255, 88, 233, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8900, { 124, 252, 255, 255, 232, 234, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 180, 251, 255, 255, 220, 236, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 236, 250, 255, 255, 208, 238, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 92, 249, 255, 255, 164, 237, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8900, { 48, 248, 255, 255, 20, 236, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8600, { 4, 247, 255, 255, 208, 238, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8400, { 172, 244, 255, 255, 120, 236, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8400, { 48, 248, 255, 255, 168, 228, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 104, 247, 255, 255, 80, 226, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 8800, { 236, 250, 255, 255, 80, 226, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9100, { 224, 252, 255, 255, 224, 227, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 9000, { 68, 253, 255, 255, 8, 238, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0xFFFF, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
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

GpOverlayIds D_shelter_b3_dumping_hole_80188638 = { 4, 17, 11 };

GpEvsCmd D_shelter_b3_dumping_hole_80188640[45] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FE34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_8017FE64 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_shelter_b3_dumping_hole_80188638 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_80180014 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_8017FE64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_8017FE9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_80180034 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b3_dumping_hole_80188A78[14] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FED4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FEF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FF14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_8017FCA0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_shelter_b3_dumping_hole_80188BC8[5] = {
    { 0, 192, func_shelter_b3_dumping_hole_8017F820, { .model = NULL } },
    { 0, 192, func_shelter_b3_dumping_hole_8017FBA0, { .model = NULL } },
    { 257, 192, func_shelter_b3_dumping_hole_8017E94C, { .model = &D_shelter_b3_dumping_hole_801877F4 } },
    { 257, 192, func_shelter_b3_dumping_hole_8017E94C, { .model = &D_shelter_b3_dumping_hole_80187A70 } },
    { 257, 192, func_shelter_b3_dumping_hole_8017E94C, { .model = &D_shelter_b3_dumping_hole_80187D74 } },
};

TaskDesc D_shelter_b3_dumping_hole_80188C04[4] = {
    { 2, 192, func_shelter_b3_dumping_hole_8017DCFC, { .model = NULL } },
    { 2, 192, func_shelter_b3_dumping_hole_8017DF90, { .model = NULL } },
    { 2, 192, NULL, { .model = NULL } },
    { 2, 192, func_shelter_b3_dumping_hole_8017E440, { .model = NULL } },
};

TmdBone D_shelter_b3_dumping_hole_80188C34[4] = {
#include "assets/shelter_b3_dumping_hole_model_0C078_skeleton.inc"
};

u32 D_shelter_b3_dumping_hole_80188CC4[4] = {
#include "assets/shelter_b3_dumping_hole_model_0C078_partVerts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80188CD4[100] = {
#include "assets/shelter_b3_dumping_hole_model_0C078_verts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80188FF4[18] = {
#include "assets/shelter_b3_dumping_hole_model_0C078_normals.inc"
};

u32 D_shelter_b3_dumping_hole_80189084[365] = {
#include "assets/shelter_b3_dumping_hole_model_0C078_stream.inc"
};

TmdSource D_shelter_b3_dumping_hole_80189638 = {
    0, 2600, 0, 4,
    D_shelter_b3_dumping_hole_80188CC4, D_shelter_b3_dumping_hole_80188CD4, D_shelter_b3_dumping_hole_80188FF4, D_shelter_b3_dumping_hole_80188C34, D_shelter_b3_dumping_hole_80189084,
};

ShelterB3DumpingHole2MessageEntry D_shelter_b3_dumping_hole_8018965C[2] = {
    { 2005, { .call2 = func_shelter_b3_dumping_hole_801817D8 } },
    { 2004, { .call1 = func_shelter_b3_dumping_hole_80181854 } },
};

GpXformArg D_shelter_b3_dumping_hole_8018966C = { { 4500, -0x2CEC, -5450, 0 }, { 341, 0, 0, 0 } };

GpOverlayIds D_shelter_b3_dumping_hole_80189684 = { 4, 18, 11 };

GpEvsCmd D_shelter_b3_dumping_hole_8018968C[33] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_shelter_b3_dumping_hole_80189684 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_801819B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_801819D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_80181958 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_801819F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_80181958 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b3_dumping_hole_801899A4[13] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_80181430 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_shelter_b3_dumping_hole_80189ADC[2] = {
    { 257, 192, func_shelter_b3_dumping_hole_80181560, { .model = &D_shelter_b3_dumping_hole_80189638 } },
    { 2, 192, func_shelter_b3_dumping_hole_8018005C, { .model = NULL } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} ShelterB3DumpingHolePoseBankC534;

ShelterB3DumpingHolePoseBankC534 D_shelter_b3_dumping_hole_80189AF4 = { .poses = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank1.inc"
} };

GpPackedSvec D_shelter_b3_dumping_hole_80189B3C[46] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank4.inc"
};

GpAnimRec D_shelter_b3_dumping_hole_80189BF4[109] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_records.inc"
};

u16 D_shelter_b3_dumping_hole_80189DA8[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_indices.inc"
};

GpAnimSet D_shelter_b3_dumping_hole_80189DD0 = {
    D_shelter_b3_dumping_hole_80189BF4, D_shelter_b3_dumping_hole_80189DA8,
    { NULL, D_shelter_b3_dumping_hole_80189AF4.words, NULL, NULL, D_shelter_b3_dumping_hole_80189B3C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[9];
    GpPackedSvec words[27];
} ShelterB3DumpingHolePoseBankC838;

ShelterB3DumpingHolePoseBankC838 D_shelter_b3_dumping_hole_80189DF8 = { .poses = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank1.inc"
} };

GpPackedSvec D_shelter_b3_dumping_hole_80189E64[97] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank4.inc"
};

GpAnimRec D_shelter_b3_dumping_hole_80189FE8[153] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_records.inc"
};

u16 D_shelter_b3_dumping_hole_8018A24C[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_indices.inc"
};

GpAnimSet D_shelter_b3_dumping_hole_8018A274 = {
    D_shelter_b3_dumping_hole_80189FE8, D_shelter_b3_dumping_hole_8018A24C,
    { NULL, D_shelter_b3_dumping_hole_80189DF8.words, NULL, NULL, D_shelter_b3_dumping_hole_80189E64, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[28];
    GpPackedSvec words[84];
} ShelterB3DumpingHolePoseBankCCDC;

ShelterB3DumpingHolePoseBankCCDC D_shelter_b3_dumping_hole_8018A29C = { .poses = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank1.inc"
} };

GpPackedSvec D_shelter_b3_dumping_hole_8018A3EC[232] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank4.inc"
};

GpAnimRec D_shelter_b3_dumping_hole_8018A78C[500] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_records.inc"
};

u16 D_shelter_b3_dumping_hole_8018AF5C[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_indices.inc"
};

GpAnimSet D_shelter_b3_dumping_hole_8018AF84 = {
    D_shelter_b3_dumping_hole_8018A78C, D_shelter_b3_dumping_hole_8018AF5C,
    { NULL, D_shelter_b3_dumping_hole_8018A29C.words, NULL, NULL, D_shelter_b3_dumping_hole_8018A3EC, NULL, NULL, NULL },
};

GpScriptCmd D_shelter_b3_dumping_hole_8018AFAC[2] = {
    { 257, 1 },
    { 0, 0 },
};

GpScriptRec D_shelter_b3_dumping_hole_8018AFB4[2] = {
    { 255, 250, 9, 1 },
    { 0, 0, 9, 0 },
};

TaskDesc D_shelter_b3_dumping_hole_8018AFBC = { 0, 192, func_shelter_b3_dumping_hole_80181A48, { .model = NULL } };

ShelterB3DumpingHoleAnimStorageAFC8 D_shelter_b3_dumping_hole_8018AFC8 = { .data = { { NULL, &D_shelter_b3_dumping_hole_80189DD0, &D_shelter_b3_dumping_hole_8018A274, &D_shelter_b3_dumping_hole_8018AF84 }, { { .words = D_shelter_b3_dumping_hole_8018AFC8.words }, 5 } } };

GpAnimArg D_shelter_b3_dumping_hole_8018AFE0 = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_shelter_b3_dumping_hole_8018AFF4 = { { .index = 1 }, 48, 1, 30, 1 };

GpAnimArg D_shelter_b3_dumping_hole_8018B008 = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_shelter_b3_dumping_hole_8018B01C = { { .index = 1 }, 50, 0, 0, 0 };

GpXformArg D_shelter_b3_dumping_hole_8018B030 = { { 0x2904, 0, -3300, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_shelter_b3_dumping_hole_8018B048 = { { 0x2904, -100, -3300, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_shelter_b3_dumping_hole_8018B060 = { { 8800, 0, -6000, 0 }, { 0, 1024, 0, 0 } };

GpCmdArg D_shelter_b3_dumping_hole_8018B078 = { { .loc = { 4, 39 } }, 0 };

GpCmdArg D_shelter_b3_dumping_hole_8018B07C = { { .loc = { 4, 39 } }, 1 };

GpEvsCmd D_shelter_b3_dumping_hole_8018B080[39] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b3_dumping_hole_8018AFC8.data.copy }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b3_dumping_hole_8018B078 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018AFF4 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54270001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B008 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b3_dumping_hole_8018B030 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54270002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b3_dumping_hole_80181A18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_shelter_b3_dumping_hole_8018AFAC }, { .padRecords = D_shelter_b3_dumping_hole_8018AFB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_shelter_b3_dumping_hole_80181B04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_80181B44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B01C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b3_dumping_hole_8018B048 }, { .value = 0 } },
    { 4, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b3_dumping_hole_8018B060 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b3_dumping_hole_8018B428[14] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b3_dumping_hole_8018B078 }, { .value = 0 } },
    { 16, { .value = 0x54270001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x54270002 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b3_dumping_hole_8018B060 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b3_dumping_hole_80181B44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s16 D_shelter_b3_dumping_hole_8018B578 = 384;

s16 D_shelter_b3_dumping_hole_8018B57A = 0;

DumpingHoleCaptionTaskSeed D_shelter_b3_dumping_hole_8018B57C = { 0, 32, func_shelter_b3_dumping_hole_80181B64, NULL };

TaskDesc D_shelter_b3_dumping_hole_8018B588 = { 0, 32, func_shelter_b3_dumping_hole_80183024, { .model = NULL } };

TaskDesc D_shelter_b3_dumping_hole_8018B594 = { 0, 32, func_shelter_b3_dumping_hole_80183060, { .model = NULL } };

OverlayCapWindow D_shelter_b3_dumping_hole_8018B5A0[13] = {
    { 300, 295, 16, 5 },
    { 240, 235, 16, 4 },
    { 180, 175, 16, 3 },
    { 120, 115, 16, 2 },
    { 60, 55, 16, 1 },
    { 30, 25, 17, 30 },
    { 5, 4, 17, 5 },
    { 4, 3, 17, 4 },
    { 3, 2, 17, 3 },
    { 2, 1, 17, 2 },
    { 1, 0, 17, 1 },
    { 0, -3, 17, 0 },
    { -1, 0, 0, 0 },
};

s32 D_shelter_b3_dumping_hole_8018B670 = 8;

s32 D_shelter_b3_dumping_hole_8018B674 = 0;

GpRoomObjRec D_shelter_b3_dumping_hole_8018B678[2] = {
    { D_shelter_b3_dumping_hole_8018C3EC, NULL, D_shelter_b3_dumping_hole_8018ECA4, NULL },
    { D_shelter_b3_dumping_hole_8018C3EC, D_shelter_b3_dumping_hole_8018E88C, D_shelter_b3_dumping_hole_8018EF9C, NULL },
};

u8 * D_shelter_b3_dumping_hole_8018B698[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_shelter_b3_dumping_hole_8018B6A0[2] = {
    { { .bytes = { 37, 0 } } },
    { { .bytes = { 37, 0 } } },
};

GpWarpRec D_shelter_b3_dumping_hole_8018B6A4[3] = {
    { { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, 0x54270004, 0x54270003, 0, 30, 0, 0 },
    { { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, 0, 0, 0, 30, 0, 0 },
    { { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x4C2C, 0, -4450 } }, { 0, 0, 0, 0 }, 0, 0, 0, 15, 0, 0 },
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
    { 2011, { .call0 = func_shelter_b3_dumping_hole_80183530 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
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
    { 0, 32, func_shelter_b3_dumping_hole_80183550, { .model = NULL } },
    { 0, 97, func_shelter_b3_dumping_hole_801835C8, { .model = NULL } },
    { 0, 97, func_shelter_b3_dumping_hole_80183620, { .model = NULL } },
    { 0, 97, func_shelter_b3_dumping_hole_80183678, { .model = NULL } },
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

SVECTOR D_shelter_b3_dumping_hole_8018B9CC[35] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { -2896, -2896, 0, 0 },
    { 2666, -3110, 0, 0 },
    { 0, -3898, -1257, 0 },
    { 0, -3898, 1257, 0 },
    { -832, -3536, 1892, 0 },
    { -832, -3536, -1892, 0 },
    { 2740, -3045, 0, 0 },
    { -699, -3549, -1922, 0 },
    { -776, -3530, 1928, 0 },
    { -776, -3530, -1928, 0 },
    { -699, -3549, 1922, 0 },
    { 39, -3757, 1631, 0 },
    { 77, -3580, -1989, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 1513, 0, 3806, 0 },
    { 1992, 0, -3579, 0 },
    { -965, 0, 3981, 0 },
    { -1188, 0, -3920, 0 },
    { 3162, 0, 2604, 0 },
    { 2896, 0, -2896, 0 },
    { -533, 0, -4061, 0 },
    { 1928, 0, 3614, 0 },
    { 1363, 0, 3862, 0 },
    { 1328, 0, -3875, 0 },
    { 593, 0, 4053, 0 },
    { 1473, 0, -3822, 0 },
    { 488, 0, -4067, 0 },
    { 2554, 0, -3202, 0 },
};

SVECTOR D_shelter_b3_dumping_hole_8018BAE4[90] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0x46B4, 100, -8500, 0 },
    { 2300, -500, -8500, 0 },
    { 3000, 100, -8500, 0 },
    { 2300, -500, -3500, 0 },
    { 0x46B4, 100, -3500, 0 },
    { 3000, 100, -3500, 0 },
    { 1300, -1400, -6475, 0 },
    { -100, 0, -8550, 0 },
    { -100, 0, -3450, 0 },
    { 1300, -1400, -5525, 0 },
    { 0x46B4, -1900, 100, 0 },
    { 1300, -1400, -1450, 0 },
    { -100, -1900, 100, 0 },
    { 1300, -1400, -0x2936, 0 },
    { 0x46B4, -1900, -0x2F44, 0 },
    { -100, -1900, -0x2F44, 0 },
    { 0, -0x2AF8, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, -0x2EE0, 0 },
    { 0, -0x2AF8, -0x2EE0, 0 },
    { 0x4650, 0, -0x2EE0, 0 },
    { 0x4650, -0x2AF8, -0x2EE0, 0 },
    { 0x4650, -5000, -8500, 0 },
    { 0x4650, -5000, -3500, 0 },
    { 0x4650, -0x2AF8, 0, 0 },
    { 0x4650, 0, 0, 0 },
    { 0x4650, 0, -8500, 0 },
    { 0x4E20, -5000, -3500, 0 },
    { 0x4E20, -5000, -8500, 0 },
    { 0x4E20, 0, -8500, 0 },
    { 0x4E20, 0, -3500, 0 },
    { 0x4650, 0, -3500, 0 },
    { 0x4851, 0, -3800, 0 },
    { 0x4851, -1000, -3800, 0 },
    { 0x4E52, -1000, -3800, 0 },
    { 0x4E52, 0, -3800, 0 },
    { 3950, 0, -5100, 0 },
    { 3950, 0, -6600, 0 },
    { 3950, -1000, -6600, 0 },
    { 3950, -1000, -5100, 0 },
    { 0x4851, -1000, -8200, 0 },
    { 0x4851, 0, -8200, 0 },
    { 0x4E52, 0, -8200, 0 },
    { 0x4E52, -1000, -8200, 0 },
    { 0x4367, -1000, -4500, 0 },
    { 0x4367, 0, -4500, 0 },
    { 0x413C, 0, -4500, 0 },
    { 0x413C, -1000, -4500, 0 },
    { 0x4367, 0, -7700, 0 },
    { 0x4367, -1000, -7700, 0 },
    { 0x413C, -1000, -7700, 0 },
    { 0x413C, 0, -7700, 0 },
    { 0x3458, -1000, -8500, 0 },
    { 9150, -1000, -8500, 0 },
    { 9150, 0, -8500, 0 },
    { 0x3458, 0, -8500, 0 },
    { 0x3458, 0, -3500, 0 },
    { 0x300C, 0, -3500, 0 },
    { 0x300C, -1000, -3500, 0 },
    { 0x3458, -1000, -3500, 0 },
    { 8450, -1000, -7650, 0 },
    { 8450, 0, -7650, 0 },
    { 0x2EAE, 0, -3850, 0 },
    { 0x2EAE, -1000, -3850, 0 },
    { 6300, -1000, -6900, 0 },
    { 4800, -1000, -6900, 0 },
    { 4800, 0, -6900, 0 },
    { 6300, 0, -6900, 0 },
    { 0x2C24, 0, -3850, 0 },
    { 0x2C24, -1000, -3850, 0 },
    { 9395, 0, -3600, 0 },
    { 9395, -1000, -3600, 0 },
    { 7425, 0, -7500, 0 },
    { 7425, -1000, -7500, 0 },
    { 4825, -1000, -4800, 0 },
    { 4825, 0, -4800, 0 },
    { 6363, -1000, -4800, 0 },
    { 6363, 0, -4800, 0 },
    { 7400, -1000, -4400, 0 },
    { 7400, 0, -4400, 0 },
    { 8568, -1000, -4260, 0 },
    { 8568, 0, -4260, 0 },
};

GpGridFace D_shelter_b3_dumping_hole_8018BDB4[56] = {
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 8, 9, 10, 0xFFFF }, 2, 0 },
    { { 11, 12, 13, 0xFFFF }, 3, 0 },
    { { 15, 16, 14, 17 }, 4, 0 },
    { { 13, 10, 11, 9 }, 5, 0 },
    { { 18, 19, 20, 0xFFFF }, 6, 0 },
    { { 21, 22, 23, 0xFFFF }, 7, 0 },
    { { 15, 21, 23, 0xFFFF }, 8, 0 },
    { { 19, 16, 20, 0xFFFF }, 9, 0 },
    { { 14, 17, 9, 11 }, 10, 0 },
    { { 9, 15, 14, 0xFFFF }, 11, 0 },
    { { 15, 9, 21, 0xFFFF }, 12, 0 },
    { { 19, 11, 16, 0xFFFF }, 13, 0 },
    { { 11, 17, 16, 0xFFFF }, 14, 0 },
    { { 21, 9, 22, 8 }, 15, 0 },
    { { 18, 12, 19, 11 }, 16, 0 },
    { { 25, 26, 24, 27 }, 17, 0 },
    { { 26, 28, 27, 29 }, 2, 0 },
    { { 30, 31, 29, 32 }, 18, 0 },
    { { 33, 25, 32, 24 }, 3, 0 },
    { { 26, 34, 28, 0xFFFF }, 19, 0 },
    { { 24, 27, 32, 29 }, 20, 0 },
    { { 29, 28, 30, 34 }, 18, 0 },
    { { 36, 37, 35, 38 }, 18, 0 },
    { { 31, 39, 32, 33 }, 18, 0 },
    { { 35, 38, 31, 39 }, 3, 0 },
    { { 37, 36, 34, 30 }, 2, 0 },
    { { 38, 37, 39, 34 }, 19, 4 },
    { { 36, 35, 30, 31 }, 20, 0 },
    { { 39, 25, 33, 0xFFFF }, 19, 0 },
    { { 39, 34, 25, 26 }, 19, 4 },
    { { 41, 42, 40, 43 }, 3, 3 },
    { { 45, 46, 44, 47 }, 17, 3 },
    { { 49, 50, 48, 51 }, 2, 3 },
    { { 53, 54, 52, 55 }, 3, 3 },
    { { 57, 58, 56, 59 }, 2, 3 },
    { { 48, 57, 49, 56 }, 21, 3 },
    { { 40, 53, 41, 52 }, 22, 3 },
    { { 61, 62, 60, 63 }, 2, 3 },
    { { 65, 66, 64, 67 }, 3, 3 },
    { { 60, 63, 58, 59 }, 23, 3 },
    { { 64, 67, 54, 55 }, 24, 3 },
    { { 68, 69, 61, 62 }, 25, 3 },
    { { 70, 71, 65, 66 }, 26, 3 },
    { { 73, 74, 72, 75 }, 2, 3 },
    { { 76, 77, 70, 71 }, 3, 3 },
    { { 79, 77, 78, 76 }, 27, 3 },
    { { 81, 72, 80, 75 }, 28, 3 },
    { { 46, 45, 73, 74 }, 29, 3 },
    { { 83, 44, 82, 47 }, 30, 3 },
    { { 80, 69, 81, 68 }, 31, 3 },
    { { 85, 83, 84, 82 }, 3, 3 },
    { { 87, 85, 86, 84 }, 32, 3 },
    { { 89, 87, 88, 86 }, 33, 3 },
    { { 78, 89, 79, 88 }, 34, 3 },
};

s16 D_shelter_b3_dumping_hole_8018C054[19] = {
    0,
    1,
    2,
    4,
    5,
    7,
    8,
    10,
    11,
    12,
    15,
    17,
    18,
    21,
    22,
    31,
    33,
    49,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C07C[24] = {
    0,
    1,
    2,
    3,
    4,
    5,
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
    22,
    31,
    33,
    45,
    49,
    50,
    52,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C0AC[20] = {
    0,
    1,
    3,
    4,
    5,
    6,
    9,
    10,
    13,
    14,
    16,
    17,
    20,
    22,
    30,
    31,
    33,
    50,
    52,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C0D4[12] = {
    0,
    1,
    6,
    9,
    13,
    16,
    17,
    20,
    22,
    30,
    31,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C0EC[21] = {
    0,
    1,
    2,
    5,
    7,
    10,
    11,
    12,
    15,
    18,
    21,
    22,
    31,
    33,
    39,
    43,
    45,
    48,
    49,
    51,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C118[22] = {
    0,
    1,
    2,
    3,
    5,
    10,
    15,
    16,
    31,
    33,
    39,
    43,
    45,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C144[21] = {
    0,
    1,
    3,
    5,
    6,
    10,
    13,
    14,
    16,
    20,
    22,
    30,
    31,
    33,
    47,
    50,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C170[9] = {
    0,
    1,
    6,
    16,
    20,
    22,
    30,
    31,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C184[15] = {
    0,
    1,
    2,
    7,
    15,
    18,
    21,
    22,
    31,
    39,
    41,
    43,
    48,
    51,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C1A4[23] = {
    0,
    1,
    2,
    3,
    15,
    16,
    21,
    30,
    31,
    39,
    40,
    43,
    44,
    45,
    46,
    47,
    48,
    51,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C1D4[18] = {
    0,
    1,
    3,
    6,
    16,
    20,
    22,
    30,
    31,
    40,
    42,
    44,
    46,
    47,
    53,
    54,
    55,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C1F8[9] = {
    0,
    1,
    6,
    16,
    20,
    22,
    30,
    31,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C20C[15] = {
    0,
    1,
    2,
    7,
    15,
    18,
    19,
    21,
    22,
    23,
    31,
    36,
    39,
    41,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C22C[25] = {
    0,
    1,
    2,
    3,
    15,
    16,
    19,
    21,
    22,
    28,
    29,
    30,
    31,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    44,
    46,
    47,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C260[18] = {
    0,
    1,
    3,
    6,
    16,
    19,
    20,
    22,
    25,
    30,
    31,
    35,
    40,
    42,
    44,
    46,
    47,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C284[8] = {
    0,
    1,
    6,
    16,
    20,
    22,
    30,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C294[20] = {
    0,
    1,
    2,
    7,
    15,
    18,
    19,
    21,
    22,
    23,
    24,
    27,
    28,
    29,
    31,
    34,
    36,
    37,
    41,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C2BC[27] = {
    0,
    1,
    2,
    3,
    15,
    16,
    19,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    34,
    35,
    36,
    37,
    38,
    41,
    42,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C2F4[20] = {
    0,
    1,
    3,
    6,
    16,
    19,
    20,
    22,
    24,
    25,
    26,
    28,
    29,
    30,
    31,
    32,
    35,
    38,
    42,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C31C[10] = {
    0,
    1,
    6,
    16,
    19,
    20,
    22,
    25,
    30,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C330[14] = {
    0,
    1,
    15,
    19,
    21,
    22,
    23,
    24,
    27,
    28,
    29,
    34,
    37,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C34C[15] = {
    0,
    1,
    19,
    22,
    24,
    26,
    27,
    28,
    29,
    31,
    32,
    34,
    37,
    38,
    -1,
};

s16 D_shelter_b3_dumping_hole_8018C36C[15] = {
    0,
    1,
    3,
    16,
    19,
    22,
    24,
    25,
    26,
    28,
    29,
    30,
    32,
    38,
    -1,
};

s16 * D_shelter_b3_dumping_hole_8018C38C[24] = {
    D_shelter_b3_dumping_hole_8018C054,
    D_shelter_b3_dumping_hole_8018C07C,
    D_shelter_b3_dumping_hole_8018C0AC,
    D_shelter_b3_dumping_hole_8018C0D4,
    D_shelter_b3_dumping_hole_8018C0EC,
    D_shelter_b3_dumping_hole_8018C118,
    D_shelter_b3_dumping_hole_8018C144,
    D_shelter_b3_dumping_hole_8018C170,
    D_shelter_b3_dumping_hole_8018C184,
    D_shelter_b3_dumping_hole_8018C1A4,
    D_shelter_b3_dumping_hole_8018C1D4,
    D_shelter_b3_dumping_hole_8018C1F8,
    D_shelter_b3_dumping_hole_8018C20C,
    D_shelter_b3_dumping_hole_8018C22C,
    D_shelter_b3_dumping_hole_8018C260,
    D_shelter_b3_dumping_hole_8018C284,
    D_shelter_b3_dumping_hole_8018C294,
    D_shelter_b3_dumping_hole_8018C2BC,
    D_shelter_b3_dumping_hole_8018C2F4,
    D_shelter_b3_dumping_hole_8018C31C,
    D_shelter_b3_dumping_hole_8018C330,
    D_shelter_b3_dumping_hole_8018C34C,
    D_shelter_b3_dumping_hole_8018C36C,
    NULL,
};

GpGridParams D_shelter_b3_dumping_hole_8018C3EC[1] = {
    { NULL, D_shelter_b3_dumping_hole_8018B9CC, D_shelter_b3_dumping_hole_8018BAE4, D_shelter_b3_dumping_hole_8018BDB4, D_shelter_b3_dumping_hole_8018C38C, 100, 0x2F44, 6, 4, 4000, 56 },
};

GpViewRec D_shelter_b3_dumping_hole_8018C410[37] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018C944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018C954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018C964[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018C974[15] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018CAA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CAB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018CAE8[16] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018CC28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CC40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CC50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CC60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018CC70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018CC80[92] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018D3B0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 15, 0, 0, { 0, 0 } },
    { 27, 37, 0, 0, { 2, 0 } },
    { 64, 28, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_dumping_hole_8018D3E0[2] = {
    { { 1, 0, 318, 196 }, 1400 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b3_dumping_hole_8018D3F4[48] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018D7B4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 26, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018D7D4[20] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018D964[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018D984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018D994[20] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018DB24[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 16, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018DB44[8] = {
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -160, 24, 375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, 32, 375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -112, 40, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -88, 48, 375, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -40, 64, 375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 72, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 8, 96, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DBE4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DC0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018DC1C[14] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018DD34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DD4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DD5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018DD6C[1] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DD80[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DD98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_dumping_hole_8018DDA8[24] = {
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

GpSprtCmd D_shelter_b3_dumping_hole_8018DF88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018DFF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018E000[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018E010[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018E020[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018E030[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_dumping_hole_8018E040[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b3_dumping_hole_8018E050[37] = {
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

GpLight D_shelter_b3_dumping_hole_8018E20C[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9988, -0x2B02, -2510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2871, 2582, 2295, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9632, -0x2B02, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 925, 820, { 0, 0 } },
};

GpPointLight D_shelter_b3_dumping_hole_8018E2BC[3] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 819, 737, 655, { 0, 0 } }, 3561, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4914, 409, 0, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4914, 409, 0, { 0, 0 } }, 1500, 2000 },
};

GpRoomCoordSet D_shelter_b3_dumping_hole_8018E3DC = { 2, D_shelter_b3_dumping_hole_8018E20C, 3, D_shelter_b3_dumping_hole_8018E2BC, 0, NULL };

GpPointLight D_shelter_b3_dumping_hole_8018E3F4[12] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1230, 1110, 985, { 0, 0 } }, 4202, 7241 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1106, 987, { 0, 0 } }, 4339, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1230, 1109, 984, { 0, 0 } }, 4124, 7086 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1109, 987, { 0, 0 } }, 4239, 6981 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1106, 986, { 0, 0 } }, 4180, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1232, 1106, 985, { 0, 0 } }, 4275, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 1844, 1640, { 0, 0 } }, 3561, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4915, 409, 0, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4915, 409, 0, { 0, 0 } }, 1500, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4DF6, -2052, -4394 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 310, 3279, 1229, { 0, 0 } }, 1341, 1738 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A91, -2181, -3367 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 3690, 4096, { 0, 0 } }, 1400, 2022 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -9400, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2215, 1970, { 0, 0 } }, 3000, 4000 },
};

GpRoomCoordSet D_shelter_b3_dumping_hole_8018E874 = { 0, NULL, 12, D_shelter_b3_dumping_hole_8018E3F4, 0, NULL };

GpObj4C D_shelter_b3_dumping_hole_8018E88C[8] = {
    { NULL, NULL, NULL, { 0x3F30, -5696, -5696, 0 }, { { 0, -6336, -4624, 0 }, { 0, -6336, 4624, 0 }, { 0, 6336, -4624, 0 }, { 0, 6336, 4624, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 30, 29, 1, 0 },
    { NULL, NULL, NULL, { 0x3FE0, -5664, -5728, 0 }, { { 0, -6336, 4624, 0 }, { 0, -6336, -4624, 0 }, { 0, 6336, 4624, 0 }, { 0, 6336, -4624, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 30, 1, 0 },
    { NULL, NULL, NULL, { 0x327F, -5280, -5793, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 29, 1, 0 },
    { NULL, NULL, NULL, { 0x31DE, -5345, -5858, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 31, 1, 0 },
    { NULL, NULL, NULL, { 9152, -5376, -6017, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 35, 1, 0 },
    { NULL, NULL, NULL, { 9311, -5344, -5921, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 31, 1, 0 },
    { NULL, NULL, NULL, { 6016, -4993, -6048, 0 }, { { 0, -6336, -4623, 0 }, { 0, -6336, 4623, 0 }, { 0, 6336, -4623, 0 }, { 0, 6336, 4623, 0 } }, { 4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 36, 1, 0 },
    { NULL, NULL, NULL, { 6176, -5120, -6112, 0 }, { { 0, -6336, 4623, 0 }, { 0, -6336, -4623, 0 }, { 0, 6336, 4623, 0 }, { 0, 6336, -4623, 0 } }, { -4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 36, 35, 129, 0 },
};

GpAreaPlace D_shelter_b3_dumping_hole_8018EAEC[5] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { 252, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 44, 1, 0, 0x38C0, 0, -5888, 2048, 0, 4, 6, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b3_dumping_hole_8018EB3C[5] = {
    { 32, 32, 3, 0, { 0, 0 }, D_8015F8D0 },
    { 103, 417, 2, 0, { 0, 0 }, D_shelter_b3_dumping_hole_80188BC8 },
    { 252, 417, 5, 0, { 0, 0 }, D_80176354 },
    { 44, 44, 5, 2, { 0, 0 }, &D_80174D58 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b3_dumping_hole_8018EB78[2] = {
    { 32, 32, 3, 0, { 0, 0 }, D_8015F8D0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b3_dumping_hole_8018EB90[2] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b3_dumping_hole_8018EBB0[5] = {
    { 44, 44, 0, 1, { 0, 0 }, &D_80142604 },
    { 70, 70, 1, 2, { 0, 0 }, &D_801575F0 },
    { 71, 71, 1, 1, { 0, 0 }, &D_80151E60 },
    { 103, 421, 2, 1, { 0, 0 }, &D_80164B78 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b3_dumping_hole_8018EBEC[5] = {
    { 44, 1, 0, 0x38C0, 4000, -5888, 2048, 0, 0, 2, 0 },
    { 70, 1, 0, 0x2B5C, 0, -8200, 2048, 0, 2, 4, 0 },
    { 71, 1, 1, 5000, 0, -5900, 2048, 0, 3, 5, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b3_dumping_hole_8018EC3C[13] = {
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

GpObj4C D_shelter_b3_dumping_hole_8018ECA4[10] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, 0, 40, 19, 2, 0 },
    { NULL, NULL, NULL, { 0x3820, -64, -0x3A40, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2660, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, 2, 18, 255, 2, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, 2, 1, 1, 2, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, 2, 19, 1, 2, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, 2, 25, 1, 2, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, 2, 19, 1, 2, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, 2, 25, 1, 2, 0 },
    { NULL, NULL, NULL, { 8608, -64, -5888, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 2660, 0x4002, 20, 1, 2, 0 },
    { NULL, NULL, NULL, { -496, -64, -6080, 0 }, { { -1504, 0, -4336, 0 }, { 1504, 0, -4336, 0 }, { -1504, 0, 4336, 0 }, { 1504, 0, 4336, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 4579, 2, 21, 1, 130, 0 },
};

GpObj4C D_shelter_b3_dumping_hole_8018EF9C[8] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, 0, 40, 19, 2, 0 },
    { NULL, NULL, NULL, { 6334, -64, -5984, 0 }, { { -144, 0, -2576, 0 }, { 144, 0, -2576, 0 }, { -144, 0, 2576, 0 }, { 144, 0, 2576, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 2572, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, 2, 18, 1, 2, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, 2, 1, 1, 2, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, 2, 19, 1, 2, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, 2, 25, 1, 2, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, 2, 19, 1, 2, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, 2, 25, 1, 130, 0 },
};

GpRoomBoundVec D_shelter_b3_dumping_hole_8018F1FC[38] = {
    { 37, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 208, 208, 208, 208 },
    { 208, 208, 208, 208 },
    { 308, 311, 308, 309 },
    { 208, 208, 208, 208 },
    { 208, 208, 208, 208 },
    { 310, 310, 310, 310 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 537, 453, 359, 472 },
    { 16, 16, 16, 16 },
    { 516, 512, 516, 514 },
    { 246, 249, 246, 247 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 25, 288, 287, 189 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 205, 205, 205, 205 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

GpRoomBoundVec D_shelter_b3_dumping_hole_8018F32C[38] = {
    { 37, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 616, 618, 617, 617 },
    { 617, 618, 618, 617 },
    { 616, 617, 617, 616 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 615, 615, 615, 615 },
    { 615, 615, 615, 615 },
    { 16, 16, 16, 16 },
};

s32 D_shelter_b3_dumping_hole_8018F45C[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_b3_dumping_hole_8018F468[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b3_dumping_hole_8018F470[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b3_dumping_hole_8018F478[1] = {
    { 0, 0, 1, 0, D_shelter_b3_dumping_hole_8018F45C },
};

GpRoomParamRec * D_shelter_b3_dumping_hole_8018F480[9] = {
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

Task * D_shelter_b3_dumping_hole_8018F4A8 = NULL;

Task * D_shelter_b3_dumping_hole_8018F4AC = NULL;

u16 D_shelter_b3_dumping_hole_8018F4B0[2] = {
    0,
    0xDF0D,
};

GpCapEntry * D_shelter_b3_dumping_hole_8018F4B4 = NULL;

GlyphUvwh * D_shelter_b3_dumping_hole_8018F4B8 = NULL;

GpEvt12 * D_shelter_b3_dumping_hole_8018F4BC = NULL;

s16 D_shelter_b3_dumping_hole_8018F4C0 = 0;

s16 D_shelter_b3_dumping_hole_8018F4C2 = 0;

s16 D_shelter_b3_dumping_hole_8018F4C4 = 0;

s16 D_shelter_b3_dumping_hole_8018F4C6 = 0;

s16 D_shelter_b3_dumping_hole_8018F4C8 = 0;

s16 D_shelter_b3_dumping_hole_8018F4CA = 0;

u16 D_shelter_b3_dumping_hole_8018F4CC = 0;

u16 D_shelter_b3_dumping_hole_8018F4CE = 0;

u8 D_shelter_b3_dumping_hole_8018F4D0[4] = {
    0,
    19,
    111,
    0,
};

u16 D_shelter_b3_dumping_hole_8018F4D4[2] = {
    0,
    0xD086,
};

s32 D_shelter_b3_dumping_hole_8018F4D8 = 0;

static inline u16 _shelterB3DumpingHoleIsOffscreen(s16 x, s16 y);
static u16        func_shelter_b3_dumping_hole_8017DA00(GpCoord* coord, s16 w, s16 h, s16 u,
                                                        s16 v, s16 tpageX, s16 tpageY, s16 scale,
                                                        s16 clut, s32 otzOverride);
static void       func_shelter_b3_dumping_hole_8017E7DC(Task* arg0);
static void       func_shelter_b3_dumping_hole_8017FE10(s32 arg0);
static void       func_shelter_b3_dumping_hole_8018098C(Task* task);
static void       func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2);
static void       func_shelter_b3_dumping_hole_80183144(s16 arg0, s16 arg1, s16 arg2);

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
static u16 func_shelter_b3_dumping_hole_8017DA00(GpCoord* coord, s16 w, s16 h, s16 u,
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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
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
    GpCoord*             coord  = arg0->extra.tmd->coords;
    DumpingHoleEntity*   entity = D_shelter_b3_dumping_hole_8018F4A8->work;

    if (entity->field_42 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = W->field_0;
            coord->coord.t[1] = W->field_2;
            coord->coord.t[2] = W->field_4;
            arg0->state++;
            return;
        case 1:
            if (entity->field_42 != 2) {
                return;
            }
            W->field_1C = 5;
            W->field_14 = 0;
            W->field_18 = 0;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_16 = 0xFFF6 - ((Gp_LcgState >> 16) & 7);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_20 = (Gp_LcgState >> 16) & 7;
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
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_8017DF90(Task* arg0)
{
    DumpingHoleAnimWork* W     = (DumpingHoleAnimWork*)arg0->work;
    GpCoord*             coord = arg0->extra.tmd->coords;
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
            coord->sub = &gGfxViewCoord;
            Gp_ComposeParentWorld((GpCoord*)arg0->spawnArg2.pointer, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx + W->field_C;
            coord->coord.t[1] = vec.vy + W->field_E;
            coord->coord.t[2] = vec.vz + W->field_10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            W->field_16       = 0xFFF6 - ((Gp_LcgState >> 16) & 7);
            W->field_14       = 0;
            W->field_18       = 0;
            W->field_1C       = 0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            W->field_20       = ((Gp_LcgState >> 16) & 7) + 0x14;
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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
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
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_8017E440(Task* arg0)
{
    DumpingHoleAnimWork* work  = (DumpingHoleAnimWork*)arg0->work;
    GpCoord*             coord = arg0->extra.tmd->coords;
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
            coord->sub = &gGfxViewCoord;
            Gp_ComposeParentWorld((GpCoord*)arg0->spawnArg2.pointer, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx;
            coord->coord.t[1] = vec.vy;
            coord->coord.t[2] = vec.vz;
            arg0->work        = (TaskIdMap*)Mem_Malloc(0x24, 0);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            work = (DumpingHoleAnimWork*)arg0->work;
            Mem_Set(work, 0, 0x24);
            work->field_16 = -0xA;
            work->field_14 = 0;
            work->field_18 = 0;
            work->field_8  = 0x1000;
            work->field_14 = 0;
            roll1          = Gp_LcgState * 5 + 0x71357911;
            work->field_16 = 0xFFF1 - ((roll1 >> 16) & 7);
            sa1            = arg0->spawnArg1.value;
            Gp_LcgState    = roll1;
            if (sa1 == 0) {
                roll2       = roll1 * 5 + 0x71357911;
                base18      = work->field_18;
                Gp_LcgState = roll2;
                if ((roll2 >> 16) & 1) {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 + ((Gp_LcgState >> 16) & 1);
                } else {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 - ((Gp_LcgState >> 16) & 1);
                }
                work->field_18 = var0;
            } else {
                if (sa1 < 0) {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = (u16)work->field_18 + ((u16)arg0->spawnArg1.value - ((Gp_LcgState >> 16) & 1));
                } else {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = (u16)work->field_18 + ((u16)arg0->spawnArg1.value + ((Gp_LcgState >> 16) & 1));
                }
                work->field_18 = delta;
            }
            work->field_1C = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_20 = (Gp_LcgState >> 16) & 7;
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
            coord->flg = 0;
            return;
        }
    }
}

static void func_shelter_b3_dumping_hole_8017E7DC(Task* arg0)
{
    DumpingHoleCoordWork* work;
    TmdObject*            extra;
    GpCoord*              coord;
    DumpingHoleCoordCfg*  cfg;
    VECTOR                v;
    TmdObject*            e2;

    extra      = arg0->extra.tmd;
    cfg        = (DumpingHoleCoordCfg*)arg0->spawnArg2.pointer;
    coord      = extra->coords;
    work       = (DumpingHoleCoordWork*)Mem_Malloc(0x5C, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x5C);
    coord->sub             = &gGfxViewCoord;
    arg0->extra.tmd->flags = 0;
    Tmd_AllocBuffers(extra);
    extra->lightMtx   = &work->field_0;
    extra->colorMtx   = &work->field_20;
    coord->coord.t[0] = cfg->field_0;
    coord->coord.t[1] = cfg->field_4;
    coord->coord.t[2] = cfg->field_8;
    Gfx_RotMatrixY(&coord->coord, cfg->field_12, 1);
    Gfx_RotMatrixX(&coord->coord, cfg->field_10, 0);
    Gfx_RotMatrixZ(&coord->coord, cfg->field_14, 0);
    coord->flg = 0;
    Task_Reparent(D_shelter_b3_dumping_hole_8018F4A8, arg0);
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
    GpCoord*              coord = arg0->extra.tmd->coords;
    GpCoord*              c2;
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
            c2->sub         = &gGfxViewCoord;
            c2->coord.t[0] += work->velX;
            c2->coord.t[1] += work->velY + work->fall;
            c2->coord.t[2] += work->velZ;
            Gfx_RotMatrixY(&c2->coord, (s16)work->rotY, 1);
            Gfx_RotMatrixX(&c2->coord, (s16)work->rotX, 0);
            c2->flg = 0;
            return;
    }
}

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0)
{
    DumpingHoleEntity* work = (DumpingHoleEntity*)arg0->work;
    union {
        GpAnimArg anim;
        GpCmdArg  loc;
    } msg;
    u8 area;

    if (work->field_24 != NULL) {
        Gp_DispatchMsg(work->field_24, 0x3ED, 0, 0);
    }
    switch (work->field_30) {
        case 0:
            break;
        case 1:
            switch (work->field_32) {
                case 0:
                    Gp_DispatchMsgPtr(work->field_24, 0x3E9, D_shelter_b3_dumping_hole_8018819C, 0);
                    Gp_DispatchMsgPtr(work->field_24, 0x3F2, &D_shelter_b3_dumping_hole_8018819C[6], 0);
                    work->field_32++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_24, 0x3F0, 0, 0) == 0) {
                        work->field_34 = 0;
                        work->field_32++;
                    }
                    return;
                case 2:
                    if (++work->field_34 < 6) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = Player_Status.weapon;
                        msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.field_4            = 0x2F;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 2: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = Player_Status.weapon;
            msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x32;
            msg.anim.field_8            = 0;
            msg.anim.field_C            = 0;
            msg.anim.field_10           = 0;
            Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
        } break;
        case 3:
            Gp_DispatchMsg(work->field_24, 0x3F3, 2, 0);
            break;
        case 4:
            Gp_DispatchMsg(work->field_24, 0x3F3, 1, 0);
            Gp_DispatchMsgPtr(work->field_24, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
            {
                DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                s32                weaponId = Player_Status.weapon;
                msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.anim.field_4            = 9;
                msg.anim.field_8            = 0;
                msg.anim.field_C            = 0;
                msg.anim.field_10           = 0;
                Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
            }
            break;
        case 5:
            switch (work->field_32) {
                case 0:
                    msg.loc.from.loc.stage = gGameSession->at4.loc.stage;
                    area                   = gGameSession->at4.loc.area;
                    msg.loc.command        = 1;
                    msg.loc.from.loc.area  = area;
                    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
                    work->field_34 = 0;
                    work->field_32++;
                    return;
                case 1:
                    if (++work->field_34 < 0x10) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = Player_Status.weapon;
                        msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.field_4            = 0x30;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = Player_Status.weapon;
            msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x33;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
        }
            Gp_DispatchMsg(work->field_24, 0x3FD, 0x20, 0);
            break;
        case 7: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = Player_Status.weapon;
            msg.anim.animBlock.index    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x31;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsgPtr(w2->field_24, 0x3E8, &msg, 0);
        } break;
    }
    work->field_30 = 0;
}

static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0)
{
    DumpingHoleEntity*      work = (DumpingHoleEntity*)arg0->work;
    GpCmdArg                msg;
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
                    Gp_DispatchMsg(((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_28, 0x7D5, 1, 0);
                    msg.from.loc.stage = gGameSession->at4.loc.stage;
                    area               = gGameSession->at4.loc.area;
                    msg.command        = 2;
                    msg.from.loc.area  = area;
                    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            Gp_DispatchMsgPtr(work->field_28, 0x7D4, work, 0);
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            area               = gGameSession->at4.loc.area;
            msg.command        = 3;
            msg.from.loc.area  = area;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
            break;
        case 4:
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            area               = gGameSession->at4.loc.area;
            msg.command        = 5;
            msg.from.loc.area  = area;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
            break;
        case 5:
            Gp_DispatchMsgPtr(work->field_2C, 0x7D4, &D_shelter_b3_dumping_hole_801881E4, 0);
            break;
        case 6:
            switch (work->field_3A) {
                case 0:
                    work->field_44 = 1;
                    work->field_3A++;
                    return;
                case 1:
                    for (i = 0; D_shelter_b3_dumping_hole_801881FC[i].field_0 != 0xFFFF; i++) {
                        Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 2, 0, &D_shelter_b3_dumping_hole_801881FC[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_80188304[i].field_0 != 0xFFFF; i++) {
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
    DumpingHoleEntity* work;
    DumpingHoleEntity* w;
    Task*              t;
    DumpingHoleEntity* w2;
    GpCopyArg          msg;
    GpAnimArg          anim;
    GpAnimArg*         p;
    s32                n;
    s32                weaponId;

    switch (arg0->state) {
        case 0:
            work       = (DumpingHoleEntity*)Mem_Malloc(0x50, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x50);
                work->field_24                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4A8 = arg0;
                work->field_28                     = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                work->field_2C                     = Gp_FindWorkById((gGameSession->at4.loc.stage << 8) | (u16)(gGameSession->at4.loc.area | 0x1000))->field_0;
                work->field_42                     = 0;
                work->field_40                     = 0;
                work->field_4A                     = 0;
                work->field_48                     = 0;
                work->field_46                     = 0;
            }
            w                         = (DumpingHoleEntity*)arg0->work;
            w->field_0                = w->field_28->extra.tmd->coords->coord.t[0];
            t                         = w->field_28;
            w->field_4                = t->extra.tmd->coords->coord.t[1];
            w->field_8                = t->extra.tmd->coords->coord.t[2];
            w->field_12               = 0x400;
            w->field_10               = 0;
            w->field_14               = 0;
            Mc_SaveData[0].state.sceneEvent = 0xC;
            gStageSceneMusicEntry     = 3;
            arg0->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != 0 || Player_Status.coordMtx->t[0] < 0x36B1) {
                break;
            }
            w2 = (DumpingHoleEntity*)arg0->work;
            n  = 0;
            while (D_shelter_b3_dumping_hole_801880A0[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_shelter_b3_dumping_hole_801880A0[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsgPtr(w2->field_24, 0x3F7, &msg, 0);
            weaponId             = Player_Status.weapon;
            p                    = &anim;
            anim.animBlock.index = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            p->field_4           = 1;
            p->field_8           = 1;
            p->field_C           = 0xA;
            anim.field_10        = 0;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &anim, 0);
            arg0->state++;
            break;
        case 2:
            func_800E8634(D_shelter_b3_dumping_hole_80188640, 0, D_shelter_b3_dumping_hole_80188A78);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                GpObj4C* collision = &D_shelter_b3_dumping_hole_8018ECA4[8];
                collision->field_4A &= ~0x40;
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
    if (gGameSession->at4.loc.room == 2) {
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
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
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
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
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
    GpCmdArg msg;

    msg.from.loc.stage = gGameSession->at4.loc.stage;
    msg.from.loc.area  = gGameSession->at4.loc.area;
    msg.command        = arg0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
}

void func_shelter_b3_dumping_hole_8017FCF4(GpCoord* arg0, SVECTOR* arg1)
{
    Task*                 task;
    DumpingHoleSpawnWork* work;

    task       = Task_SpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 1, 0, arg0);
    work       = (DumpingHoleSpawnWork*)Mem_Malloc(0x24, 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    Mem_Set(work, 0, 0x24);
    work->field_C  = (u16)arg1->vx;
    work->field_E  = (u16)arg1->vy;
    work->field_10 = (u16)arg1->vz;
}

static void func_shelter_b3_dumping_hole_8017FD9C(GpCoord* arg0, s32 arg1)
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
    Gp_DispatchMsg(p->field_28, 0x7D5, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FE9C(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    Gp_DispatchMsg(p->field_2C, 0x7D5, arg0, 0);
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

    Gp_DispatchMsg(ent->field_2C, 0x7D5, 2, 0);
    Gp_DispatchMsg(ent->field_24, 0x3F3, 1, 0);
    Gp_DispatchMsgPtr(ent->field_24, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
    ent2    = st->work;
    desc[0] = Player_Status.weapon + (Mc_SaveData[0].state.characterId == 1 ? 1 : 0x22);
    desc[1] = 9;
    desc[2] = 0;
    desc[3] = 0;
    desc[4] = 0;
    Gp_DispatchMsgPtr(ent2->field_24, 0x3E8, desc, 0);
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
    GpCoord*             coord;
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
    coord = arg0->extra.tmd->coords;
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
            work       = (DumpingHoleShard*)arg0->work;
            coord->sub = &gGfxViewCoord;
            Mem_Set(arg0->work, 0, 0x34);
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
            prim           = (POLY_G3*)gGpuPrimCursor;
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
            Gfx_RotMatrixY(&coord->coord, work->rot.vy, 1);
            Gfx_RotMatrixX(&coord->coord, work->rot.vx, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rot.vz, 0);
            coord->flg = 0;
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
    OverlayMat*         ident;
    OverlayMat*         ident2;
    u16                 i;
    GpCmdArg*           loc3;
    GpCmdArg*           loc5;
    s32*                p;
    union {
        s32      words[5];
        GpCmdArg loc;
        SVECTOR  vec[4];
    } buf;
    union {
        SVECTOR  vec;
        GpCmdArg loc;
    } buf2;
    s32 words[5];

    work = task->work;
    switch (work->state) {
        case 1:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            buf.words[0]         = Player_Status.weapon + (Mc_SaveData[0].state.characterId == 1 ? 1 : 0x22);
            buf.words[1]         = 9;
            buf.words[2]         = 1;
            buf.words[3]         = 0xA;
            buf.words[4]         = 0;
            /* The message ABI carries this object address in one 32-bit word. */
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)buf.words, 0);
            buf.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf.loc.from.loc.area  = gGameSession->at4.loc.area;
            buf.loc.command        = 0xA;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.loc, 0x7DB);
            work->state = 0;
            return;
        case 2:
            switch (work->step) {
                case 0:
                    Gp_DispatchMsg(work->field_80, 0x3F3, 2, 0);
                    buf.vec[0].vx = 0x800;
                    buf.vec[0].vy = 0x800;
                    buf.vec[0].vz = 0x800;
                    Gp_SetOverrideVec(&buf.vec[0]);
                    Gp_DispatchMsg(work->field_84, 0x7D5, 2, 0);
                    Gp_DispatchMsg(task, 0x7D5, 1, 0);
                    work->pose.pos.vx = D_shelter_b3_dumping_hole_8018966C.pos.vx;
                    work->pose.pos.vy = D_shelter_b3_dumping_hole_8018966C.pos.vy;
                    work->pose.pos.vz = D_shelter_b3_dumping_hole_8018966C.pos.vz;
                    work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx;
                    work->pose.rot.vy = D_shelter_b3_dumping_hole_8018966C.rot.vy;
                    work->pose.rot.vz = D_shelter_b3_dumping_hole_8018966C.rot.vz;
                    Gp_DispatchMsgPtr(task, 0x7D4, &work->pose, 0);
                    work->field_58.vx                  = 0;
                    work->field_58.vy                  = 0;
                    work->field_58.vz                  = 0;
                    D_shelter_b3_dumping_hole_8018F4B0_value = 1;
                    work->field_68                     = &task->extra.tmd->coords[2];
                    work->field_6C                     = 0x14;
                    work->scale.vx                     = 0x1000;
                    work->scale.vy                     = 0x1000;
                    work->scale.vz                     = 0x1000;
                    work->timer                        = 0;
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
                        Gp_SpawnEff(0x60070, task->extra.tmd->coords, 0x608, &buf.vec[3]);
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0x320;
                        ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(0x60070, task->extra.tmd->coords, 0x608, &buf.vec[3]);
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
                            ident                                  = (OverlayMat*)&task->extra.tmd->coords[3].coord;

                            ident->ident.m00_m01 = 0x1000;
                            ident->ident.m02_m10 = 0;
                            ident->ident.m11_m12 = 0x1000;
                            ident->ident.m20_m21 = 0;
                            ident->ident.m22     = 0x1000;

                            gfxScaleMatrixColumns(&task->extra.tmd->coords[3].coord, &work->scale);
                        } else if (work->timer < 0x20) {
                            task->extra.tmd->coords[2].coord.t[1] += 0x20;
                            ident2                                 = (OverlayMat*)&task->extra.tmd->coords[3].coord;

                            ident2->ident.m00_m01 = 0x1000;
                            ident2->ident.m02_m10 = 0;
                            ident2->ident.m11_m12 = 0x1000;
                            ident2->ident.m20_m21 = 0;
                            ident2->ident.m22     = 0x1000;
                        }
                        if (!(gDisplayState.animFrame & 0xF)) {
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0xC8;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(0x600E0, task->extra.tmd->coords, 0x200, &buf2.vec);
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0x320;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(0x600E0, task->extra.tmd->coords, 0x200, &buf2.vec);
                        }
                    }
                    if (gDisplayState.animFrame & 1) {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx + 0x10;
                    } else {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx - 0x10;
                    }
                    Gp_DispatchMsgPtr(task, 0x7D4, &work->pose, 0);
                    break;
                case 2:
                    if (!(gDisplayState.animFrame & 0xF)) {
                        buf2.vec.vx = -0x190;
                        buf2.vec.vy = 0x190;
                        buf2.vec.vz = 0x190;
                        Gp_SpawnEff(0x600E0, &task->extra.tmd->coords[1], 0x200, &buf2.vec);
                    }
                    if (++work->timer >= 6) {
                        if (work->field_98 < -0x154) {
                            work->field_98 -= 1;
                        } else {
                            work->field_98 -= 8;
                        }
                        Gfx_RotMatrixX(&task->extra.tmd->coords[1].coord, work->field_98, 1);
                    }
                    task->extra.tmd->coords[2].coord.t[1] += 0x190;
                    Gp_DispatchMsgPtr(task, 0x7D4, &work->pose, 0);
                    break;
            }
            if (gDisplayState.animFrame & 1) {
                Display_ClampField126(1);
            } else {
                Display_ClampField126(-1);
            }
            return;
        case 3:
            Gp_SetOverrideVec(NULL);
            Gp_DispatchMsg(work->field_80, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_84, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D5, 2, 0);
            D_shelter_b3_dumping_hole_8018F4B0_value = 0;
            work->field_96                     = 1;
            Gp_PulseState1C();
            buf2.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf2.loc.from.loc.area  = gGameSession->at4.loc.area;
            loc3                    = &buf2.loc;
            loc3->command           = 0xB;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, loc3, 0x7DB);
            Display_ClampField126(0);
            work->state = 0;
            return;
        case 4:
            Gp_DispatchMsg(work->field_84, 0x7D5, 2, 0);
            break;
        case 5:
            Mc_SaveData[0].state.at4.loc.view = work->field_94;
            Gp_DispatchMsg(work->field_80, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_84, 0x7D5, 1, 0);
            work->field_96 = 1;
            Gp_PulseState1C();
            buf2.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf2.loc.from.loc.area  = gGameSession->at4.loc.area;
            loc5                    = &buf2.loc;
            loc5->command           = 0xC;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, loc5, 0x7DB);
            p        = words;
            words[0] = Player_Status.weapon + (Mc_SaveData[0].state.characterId == 1 ? 1 : 0x22);
            p[1]     = 1;
            p[2]     = 1;
            p[3]     = 0xA;
            words[4] = 0;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, words, 0);
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
    GpCmdArg            desc;
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
    desc.from.loc.stage                = gGameSession->at4.loc.stage;
    desc.from.loc.area                 = gGameSession->at4.loc.area;
    desc.command                       = 0x13;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &desc, 0x7DB);

    Display_ClampField126(0);
    Gp_DispatchMsg(ent->field_84, 0x7D5, 1, 0);
    Gp_DispatchMsg(ent->field_80, 0x3F3, 1, 0);

    p3       = desc3;
    desc3[0] = Player_Status.weapon + (Mc_SaveData[0].state.characterId == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, desc3, 0);
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
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != 0) {
                return;
            }
            obj        = task->extra.tmd;
            task->work = memCalloc(0xA0, false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                task->extra.tmd->coords->sub = &gGfxViewCoord;
                work                         = task->work;
                Mem_Set(work, 0, 0xA0);
                work->field_80                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4AC = task;
                work->field_84                     = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                obj->lightMtx                      = &work->lightMtx;
                obj->colorMtx                      = &work->colorMtx;
                task->msgTable                     = D_shelter_b3_dumping_hole_8018965C;
                func_shelter_b3_dumping_hole_80183218(0);
            }
            D_shelter_b3_dumping_hole_8018F4D8          = 0;
            ((DumpingHoleEntity4*)task->work)->field_94 = gGameSession->at4.loc.view;
            Gp_MsgPlayerWeapon(0);
            desc[0] = Player_Status.weapon + (Mc_SaveData[0].state.characterId == 1 ? 1 : 0x22);
            desc[1] = 9;
            desc[2] = 1;
            desc[3] = 0xA;
            desc[4] = 0;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, desc, 0);
            task->state++;
            break;
        case 1:
            D_shelter_b3_dumping_hole_8018809C = 0;
            func_800E8634(D_shelter_b3_dumping_hole_8018968C, 0, D_shelter_b3_dumping_hole_801899A4);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(0x11D, 1);
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

/// Sets how the task's model is treated from `arg2`: 0 hides it and leaves its
/// primitive buffer to be allocated on demand, 1 shows it with the same
/// allocation, 2 hides it and exempts it from that allocation.
void func_shelter_b3_dumping_hole_801817D8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Places the task's model at `placement`: the position becomes the
/// coordinate's translation, the rotation is applied in Y, X, Z order, and the
/// coordinate is marked for recomputation.
void func_shelter_b3_dumping_hole_80181854(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    MATRIX*  mtx;

    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_801818E0(void)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    if (p->field_9C == 0) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x20);
        Gp_StateF0.field_6       = 0;
        gGameSession->flowFlags |= 0x80;
        p->field_9C              = 1;
    }
}

void func_shelter_b3_dumping_hole_80181958(s32 arg0)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    Gp_DispatchMsg(p->field_80, 0x3F3, arg0, 0);
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
            arg0->spawnArg1.value     = 3;
            arg0->killCountdown = 8;
            arg0->state        += 1;
            break;
        case 1:
            if (--arg0->killCountdown < 0) {
                arg0->state += 1;
            }
            Display_ClampField126(arg0->spawnArg1.value);
            arg0->spawnArg1.value = -arg0->spawnArg1.value;
            break;
        default:
            Display_ClampField126(0);
            taskKill(arg0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80181B04(s16 arg0)
{
    func_shelter_b3_dumping_hole_8017FD9C(
        &gameGetPtrSlot(3)->extra.tmd->coords[1], arg0);
}

void func_shelter_b3_dumping_hole_80181B44(s32 arg0)
{
    func_shelter_b3_dumping_hole_8017FE10(arg0);
}

/// Drives the room's timed captions: state 0 arms the task, and each later
/// tick scans `D_shelter_b3_dumping_hole_8018B5A0` for the first window
/// holding `gGameSession->sceneClock` and, on a hit, starts that window's
/// caption at its line key, with the low half of the task's `spawnArg1` as
/// the line delay, and shows its current line. The clock then counts down one, unless a
/// caption is running or `Gp_StateF0.field_4` is set.
static void func_shelter_b3_dumping_hole_80181B64(Task* task, s32 arg1)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            script = 0;
            key    = arg1;
            for (i = 0; D_shelter_b3_dumping_hole_8018B5A0[i].upper != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_shelter_b3_dumping_hole_8018B5A0[i].upper * 30 >= time) &&
                    (D_shelter_b3_dumping_hole_8018B5A0[i].lower * 30 < time)) {
                    script = D_shelter_b3_dumping_hole_8018B5A0[i].script;
                    key    = D_shelter_b3_dumping_hole_8018B5A0[i].key;
                    break;
                }
            }
            if (script != 0) {
                func_shelter_b3_dumping_hole_80181E70(script, key, (s16)task->spawnArg1.value);
                func_shelter_b3_dumping_hole_80181C8C();
            }
            if ((Gp_CapBusy() == 0) && (Gp_StateF0.field_4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

static void func_shelter_b3_dumping_hole_80181C8C(void)
{
    if (D_shelter_b3_dumping_hole_8018F4BC == NULL) {
        return;
    }
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8.offset == -1) {
        return;
    }
    if (Gp_CapBusy() != 0) {
        return;
    }
    func_shelter_b3_dumping_hole_80181F80(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8.text, 0x80, 1,
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].prefix.bytes.field_0 |
            ((D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].prefix.bytes.field_1 & 0x10)
             << 4));
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_4 & 1) {
        return;
    }
    func_shelter_b3_dumping_hole_80182AA0();
}

static s32 func_shelter_b3_dumping_hole_80181D68(GpCapFileAddress base)
{
    GpEvt12*    r;
    GpCapEntry* q;
    s32         n1;
    s32         n2;
    s32         i;

    if (strncmp(base.file->magic, "CAP", 3) != 0) {
        return 0;
    }
    if (base.file->field_8.offset > 0) {
        base.file->field_8.offset  += base.address;
        base.file->field_C.offset  += base.address;
        base.file->field_10.offset += base.address;
        n1                          = base.file->field_C.ptr->count;
        r                           = &base.file->field_C.ptr->records[0];
        for (i = 0; i < n1; i++) {
            if (r->field_8.offset != -1) {
                r->field_8.offset += base.address;
            } else {
                r++;
            }
            r++;
        }
        n2 = base.file->field_10.ptr->count;
        q  = &base.file->field_10.ptr->entries[0];
        for (i = 0; i < n2; i++) {
            if (q->offset != 0) {
                q->offset += base.address;
            }
            q++;
        }
    }
    D_shelter_b3_dumping_hole_8018F4B8 = base.file->field_8.ptr;
    D_shelter_b3_dumping_hole_8018F4B4 = base.file->field_10.ptr->entries;
    return 1;
}

static s32 func_shelter_b3_dumping_hole_80181E70(s16 arg0, s16 arg1, s32 arg2)
{
    GpEvt12* entry;

    entry                              = D_shelter_b3_dumping_hole_8018F4B4[arg0].events;
    D_shelter_b3_dumping_hole_8018F4BC = entry;
    if (entry == NULL) {
        return 1;
    }
    D_shelter_b3_dumping_hole_8018F4CA = arg1;
    D_shelter_b3_dumping_hole_8018F4C6 = func_shelter_b3_dumping_hole_80182FD0(1);
    D_shelter_b3_dumping_hole_8018F4C4 = arg2;
    D_shelter_b3_dumping_hole_8018F4C0 = func_shelter_b3_dumping_hole_80182C24(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8.text);
    D_shelter_b3_dumping_hole_8018F4C2 = func_shelter_b3_dumping_hole_801829B4(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8.text);
    D_shelter_b3_dumping_hole_8018F4C8 = func_shelter_b3_dumping_hole_80182E50(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8.text);
    D_shelter_b3_dumping_hole_8018F4D0_value = 0x1E;
    return 0;
}

static s32 func_shelter_b3_dumping_hole_80181F80(u16* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16*       text;
    u16*       body;
    s32        title;
    s16        sc;
    u32        shifted;
    s32        titleWidth;
    s16        lineIdx;
    s16        x;
    s32        y;
    s16        i;
    u16        code;
    s16        centered;
    s32        palette;
    s16        t;
    s16        t2;
    s16        glyphY;
    s32        top;
    POLY_G4*   bg;
    POLY_G4*   bg2;
    DR_MODE*   dm;
    POLY_FT4*  ft;
    POLY_GT4*  gt;
    POLY_GT4*  gt2;
    GlyphUvwh* icon;

    lineIdx = 0;
    title   = arg3;
    text    = arg0;
    x       = func_shelter_b3_dumping_hole_80182D34(arg0, 0) - 0xA0;
    y       = (u16)D_shelter_b3_dumping_hole_8018F4C2 - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y0 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x1 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y1 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x2 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y2 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x3 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y3 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg2 + 1);
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dm + 1);
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(ft + 1);
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - (u16)D_shelter_b3_dumping_hole_8018F4C8;
        ft->x0     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->x2     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->u3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                                 = lineIdx + 1;
            lineIdx                            = t2;
            D_shelter_b3_dumping_hole_8018F4CE = y - 2;
            D_shelter_b3_dumping_hole_8018F4CC = x + 4;
            y                                 += func_shelter_b3_dumping_hole_80182F18(&body[i + 1]);
            if (centered != 0) {
                x = func_shelter_b3_dumping_hole_80182D34(arg0, t2) - 0xA0;
            } else {
                x = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(ft + 1);
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->h;
            ft->x1    = x + icon->w;
            ft->y1    = t - icon->h;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->w;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->w;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->h;
            ft->u3    = icon->u + icon->w;
            ft->v3    = icon->v + icon->h;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->w;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt + 1);
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            gt->y0    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x1    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y1    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->u3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_shelter_b3_dumping_hole_8018F4B8[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_shelter_b3_dumping_hole_8018F4C4`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0.
static s16 func_shelter_b3_dumping_hole_801829B4(u16* arg0)
{
    s16  lineH     = 0;
    s16  total     = 0;
    s16  i         = 0;
    s16  seenBreak = 0;
    u16* text      = arg0;
    s16  code      = text[0];

    while (code != -1) {
        if (code == -2) {
            if (seenBreak) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
            } else {
                seenBreak = 1;
            }
            lineH = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h + 2) {
                    lineH = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h + 2;
                }
            }
        }
        code = text[++i];
    }
    return D_shelter_b3_dumping_hole_8018F4C4 - total;
}

static void func_shelter_b3_dumping_hole_80182AA0(void)
{
    Prim82AA0* prim;
    s32        c1;
    s32        c2;

    if (D_shelter_b3_dumping_hole_8018F4D0_value != 0) {
        D_shelter_b3_dumping_hole_8018F4D0_value -= 1;
        return;
    }
    prim           = (Prim82AA0*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 6);
    prim->code     = 0x30;
    c1             = (D_shelter_b3_dumping_hole_8018B670 << 7) / 15;
    prim->r        = c1;
    prim->g        = c1;
    prim->b        = c1;
    c1             = (D_shelter_b3_dumping_hole_8018B670 * 192) / 15;
    c2             = c1;
    prim->field_C  = c2;
    prim->field_D  = c2;
    prim->field_E  = c2;
    prim->field_14 = c2;
    prim->field_15 = c2;
    prim->field_16 = c2;
    prim->field_8  = D_shelter_b3_dumping_hole_8018F4CC + 3;
    prim->field_A  = D_shelter_b3_dumping_hole_8018F4CE;
    prim->field_10 = D_shelter_b3_dumping_hole_8018F4CC;
    prim->field_18 = D_shelter_b3_dumping_hole_8018F4CC + 7;
    prim->field_12 = D_shelter_b3_dumping_hole_8018F4CE - 7;
    prim->field_1A = D_shelter_b3_dumping_hole_8018F4CE - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_shelter_b3_dumping_hole_8018B674 == 0) {
        D_shelter_b3_dumping_hole_8018B670 += 1;
        if (D_shelter_b3_dumping_hole_8018B670 >= 0xF) {
            D_shelter_b3_dumping_hole_8018B674 = 1;
        }
    } else {
        D_shelter_b3_dumping_hole_8018B670 -= 1;
        if (D_shelter_b3_dumping_hole_8018B670 < 9) {
            D_shelter_b3_dumping_hole_8018B674 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_shelter_b3_dumping_hole_8018F4B8`)
/// advances it by that glyph's `w - 1`.
static s16 func_shelter_b3_dumping_hole_80182C24(u16* text)
{
    s16 lineW = 0;
    s16 maxW  = 0;
    s16 i     = 0;
    s16 code  = text[0];

    while (code != -1) {
        if (code == -2) {
            if (lineW > maxW) {
                maxW = lineW;
            }
            lineW = 0;
            code  = text[++i];
        } else if (code == -3) {
            lineW += 3;
            code   = text[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = text[++i];
        } else if (code >= 0) {
            lineW += D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w - 1;
            code   = text[++i];
        } else {
            code = text[++i];
        }
    }
    return (0x140 - maxW) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes.
static s16 func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1)
{
    s16 lineW;
    s16 selectedW;
    s16 i;
    s16 lineIndex;
    s16 code;

    lineW     = 0;
    selectedW = 0;
    i         = 0;
    lineIndex = 0;
    code      = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineIndex == arg1) {
                selectedW = lineW;
            }
            lineW = 0;
            i++;
            lineIndex++;
            code = arg0[i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - selectedW) / 2 - 5;
}

static s32 func_shelter_b3_dumping_hole_80182E50(u16* arg0)
{
    u16*       p = arg0;
    short      acc;
    short      total;
    u16        i;
    u16        tok;
    s32        sh;
    s32        t;
    s32        ni;
    GlyphUvwh* e;
    union {
        GlyphUvwh* ptr;
        u32        address;
    } glyph;

    acc   = 0;
    total = acc;
    i     = total;
    tok   = *p;
    sh    = tok << 16;
    if ((sh >> 16) != -1) {
        do {
            t = sh >> 16;
            if (t == -2) {
                if (acc == 0) {
                    acc = 2;
                }
                total += acc;
                acc    = 0;
            } else if (t == -3) {
            } else if (t >= 0) {
                glyph.ptr     = D_shelter_b3_dumping_hole_8018F4B8;
                glyph.address = (tok & 0x3FF) * sizeof(GlyphUvwh) + glyph.address;
                e             = glyph.ptr;
                if (acc < e->h + 2) {
                    acc = e->h + 2;
                }
            }
            ni  = (i = i + 1);
            tok = p[(s16)ni];
            sh  = tok << 16;
        } while ((sh >> 16) != -1);
    }
    return (s16)total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_shelter_b3_dumping_hole_8018F4B8`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
static s32 func_shelter_b3_dumping_hole_80182F18(u16* arg0)
{
    s16 height = 0;
    s16 i      = 0;
    s16 cont   = 1;
    s16 code   = arg0[0];

    do {
        if (code == -2) {
            cont = 0;
        } else if (code == -1) {
            cont   = 0;
            height = 0xD;
        } else if (code >= 0) {
            if (height < D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h + 2) {
                height = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h + 2;
            }
            code = arg0[++i];
        } else {
            code = arg0[++i];
        }
    } while (cont);
    if (height == 0) {
        height = 2;
    }
    return height;
}

static s32 func_shelter_b3_dumping_hole_80182FD0(s32 arg0)
{
    s32      sentinel = -1;
    GpEvt12* base     = D_shelter_b3_dumping_hole_8018F4BC;
    s32      target   = D_shelter_b3_dumping_hole_8018F4CA;
    GpEvt12* e        = Gp_CapEventAt(base, arg0);

loop:
    if (e->field_8.offset != sentinel) {
        if (e->field_5 != target) {
            e++;
            arg0++;
            goto loop;
        }
    }
    return arg0;
}

void func_shelter_b3_dumping_hole_80183024(Task* arg0)
{
    if ((arg0->spawnArg1.value -= 1) <= 0) {
        taskKill(arg0);
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

void func_shelter_b3_dumping_hole_80183060(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state = 1;
            break;
        case 1:
            arg0->spawnArg1.value -= 1;
            if (arg0->spawnArg1.value <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                taskKill(arg0);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

/// Selects entry `arg0` through `func_shelter_b3_dumping_hole_80181E70` with a
/// fixed third argument of 0xD0, then spawns the task described by
/// `D_shelter_b3_dumping_hole_8018B588`, passing `arg2` to the spawn.
static void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_dumping_hole_80181E70(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B588, 0, (s32)(arg2), 0);
}

static void func_shelter_b3_dumping_hole_80183144(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_dumping_hole_80181E70(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_shelter_b3_dumping_hole_8018B594, arg2, 0, 0);
}

void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                              = 0;
    D_shelter_b3_dumping_hole_8018B578 = arg0;
    D_shelter_b3_dumping_hole_8018B57A = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_shelter_b3_dumping_hole_80181D68(D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

/// Hides or shows sprite commands 1 and 2 of the area's view 13 through their
/// `GpSprtCmd::field_4`: 0 hides both, 1 shows command 2 and 2 shows command 1.
static void func_shelter_b3_dumping_hole_80183218(u8 arg0)
{
    GpAreaKey* g4 = &gGameSession->at4.loc;
    GpSprtCmd* vs = Gp_SprtTables[g4->stage - 1]->field_0[g4->area - 1][13].field_4;

    if (arg0 == 0) {
        vs[1].field_4 = 1;
        vs[2].field_4 = 1;
    } else if (arg0 == 1) {
        vs[2].field_4 = 0;
    } else if (arg0 == 2) {
        vs[1].field_4 = 0;
    }
}

/// Spawns the two enemies of one slot from the `D_80151E60` table, numbering
/// them from the spawn counter, and marks the slot live. Actor 342400 carries
/// the same body.
static void func_shelter_b3_dumping_hole_80183298(Task* arg0)
{
    OverlayEncounterPairWork* work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(0xC, 0);
    if (work == NULL) {
        goto kill;
    }
    arg0->work   = (TaskIdMap*)work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
    kill:
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << 12;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task       = enemy->task;
        obj        = task->extra.tmd;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << 12;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task       = enemy->task;
        obj        = task->extra.tmd;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
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

void func_shelter_b3_dumping_hole_80183530(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    if (arg2->command == 4) {
        ent->stop = arg2->command;
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
    TaskFuncTable4 sp;

    sp = D_shelter_b3_dumping_hole_8017D654;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
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

    if ((u8)gGameSession->spawnPhase[0] == 2) {
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
    arg0->work                         = work;
    arg0->msgTable                     = D_shelter_b3_dumping_hole_8018B7AC;
    arg0->state                       += 1;
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
        gGameSession->spawnPhase[0] = 1;
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
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
            gGameSession->spawnPhase[0] = 2;
            taskKill(arg0);
        }
    }
}

static void func_shelter_b3_dumping_hole_80183950(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(8, 0);
    if (work != NULL) {
        GpEnemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_80142604, 1, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            idx                                                                     = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                             = enemy;
            enemy->placeKey                                                         = idx << 12;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                            += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183A00(Task* arg0)
{
    GpCmdArg                    desc;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x2E) {
        TmdObject* p        = t00->extra.tmd;
        p->clut             = 2;
        p->tpage            = 0;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2C;
        desc.command        = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(t00, 0x7DB, &desc, 0);
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
        GpEnemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = 1;
            idx                                                                     = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                             = enemy;
            enemy->placeKey                                                         = idx << 12;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                            += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183B9C(Task* arg0)
{
    GpCmdArg                    desc;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x3D) {
        TmdObject* p        = t00->extra.tmd;
        p->tpage            = 2;
        p->clut             = 4;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2A;
        desc.command        = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(t00, 0x7DB, &desc, 0);
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
    GpCmdArg                  desc;
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  t0  = ent->enemy0;

    if (t0 != NULL) {
        Task*      t00      = t0->task;
        TmdObject* p        = t00->extra.tmd;
        p->tpage            = 3;
        p->clut             = 5;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2E;
        desc.command        = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(t00, 0x7DB, &desc, 0);
    }
    ent->frames  = 0;
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183D34(Task* arg0)
{
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  t   = ent->enemy1;

    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->enemy1 != NULL) {
        if ((s16)(ent->frames += 1) < 0x3D) {
            return;
        }
        {
            Task*      t00 = ent->enemy1->task;
            TmdObject* p   = t00->extra.tmd;
            GpCmdArg   desc;
            p->tpage            = 3;
            p->clut             = 5;
            t->workType         = 0x900;
            desc.from.loc.stage = 0;
            desc.from.loc.area  = 0x2E;
            desc.command        = arg0->spawnArg1.value;
            Gp_DispatchMsgPtr(t00, 0x7DB, &desc, 0);
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
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[4], 0x300, 0x400);
            break;
        case 3:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x400);
            break;
        case 4:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[9], 0x300, 0x400);
            break;
        case 7:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 33)[0], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 33)[1], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 33)[2], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 33)[3], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 33)[4], 0x300, 0x400);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 30)[0], 0x280, 0x44);
                func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 30)[1], 0x280, 0x40);
            }
            break;
        case 15:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x21], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x22], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x23], 0x300, 0x400);
            break;
        case 17:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 36)[0], 0x300, 0x400);
            break;
        case 18:
            func_shelter_b3_dumping_hole_80184638(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x12], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x13], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x19], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x1A], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 10)[0x1B], 0x300, 0x200);
            break;
        case 19:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x400);
            break;
        case 21:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 24)[0], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 24)[1], 0x400, 0x444);
            break;
        case 22:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 30)[0], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 30)[1], 0x280, 0x40);
            break;
        case 23:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x400);
            break;
        case 26:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 26)[0], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 26)[1], 0x300, 0x400);
            break;
        case 29:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[2], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[3], 0x280, 0x40);
            break;
        case 30:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[2], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 28)[3], 0x280, 0x40);
            break;
        case 31:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x400);
            break;
        case 34:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[2], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[3], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[4], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[5], 0x300, 0x400);
            break;
        case 35:
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[1], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[6], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&(D_shelter_b3_dumping_hole_8018B86C + 32)[7], 0x300, 0x400);
            break;
        case 13:
        case 37:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[6], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[16], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[25], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[27], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[35], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x300);
            break;
    }
}

/// Draws a glowing capsule between the world points `arg0` and `arg0 + 1`,
/// projected through `gGfxViewCoord.workm`. Each end is a disc of radius
/// `(s16)arg1 * 64` over its depth; for each 0x400 step across half a turn
/// from the screen-space angle between the ends, one gouraud `POLY_G4` wedge
/// is queued at each end and one band joins them. The lit vertices, at the
/// centres, take the colour packed in `arg2` (one nibble per channel),
/// flickering with the animation frame. Nothing is drawn when either
/// projection flags an error.
static void func_shelter_b3_dumping_hole_80184638(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`: four gouraud `POLY_G4` wedges of radius
/// `(s16)arg1 * 64` over the depth, dark at the rim. The centre takes the
/// colour packed in `arg2` (one nibble per channel), flickering with the
/// animation frame. Nothing is drawn when the projection flags an error.
static void func_shelter_b3_dumping_hole_80184E7C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Per-frame update of a spark or debris effect task. State 0 seeds the work
/// from `spawnArg1`: the spin angle from its low 12 bits, the frame step from
/// bits 12-14, the palette bank from bits 28-30, and, unless the work already
/// carries a velocity, a random one of the kind bits 24-27 select, scaled to
/// the strength in bits 16-23 through the GTE. A negative `spawnArg1` picks
/// the second sprite set (state 2). Later ticks draw the frame, drift the
/// coordinate by the velocity under a small pull, and advance the frame every
/// `period` ticks, releasing the task after the set's last frame. While an
/// event is running the task only draws, and is released once the event state
/// reaches 4.
void func_shelter_b3_dumping_hole_8018521C(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        step;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->spawnArg1.value < 0) {
            func_shelter_b3_dumping_hole_80185DCC(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            func_shelter_b3_dumping_hole_8018596C(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
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
            break;
        case 1:
            func_shelter_b3_dumping_hole_8018596C(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b3_dumping_hole_80185DCC(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

static void func_shelter_b3_dumping_hole_8018596C(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u32              idx;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    idx                                       = arg1;
    idx                                      &= 0xFFF;
    bank                                      = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x68, u0 + 0x2F, v0 + 0x68, u0, v0 - 0x69, u0 + 0x2F, v0 - 0x69);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 47` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. The low 12 bits of `arg1` pick the
/// frame, a 48x48 cell in a five-wide grid of the texture page; the top bits
/// pick one of two palettes. Nothing is drawn when the projection flags an
/// error.
static void func_shelter_b3_dumping_hole_80185DCC(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u16              vz;

    bank                                      = arg1 >> 12;
    arg1                                     &= 0xFFF;
    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Per-frame update of an effect task drawn with
/// `func_shelter_b3_dumping_hole_801866CC` (state 1) or
/// `func_shelter_b3_dumping_hole_80186AB8` (state 2). State 0 seeds the work from
/// `spawnArg1` and, when `move` is zero, picks a random velocity scaled
/// through the GTE. Later ticks draw, drift the coordinate by that velocity
/// with `vy` growing by 6, and advance the frame every `period` ticks,
/// releasing the task after frame 7. While an event is running the task only
/// draws, and is released once the event state reaches 4.
void func_shelter_b3_dumping_hole_80186218(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b3_dumping_hole_801866CC(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b3_dumping_hole_80186AB8(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1.value)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
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
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1.value)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
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
            func_shelter_b3_dumping_hole_801866CC(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b3_dumping_hole_80186AB8(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 31` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. `arg1` picks the frame, a 32x32 cell
/// in a row of the texture page. Nothing is drawn when the projection flags
/// an error.
static void func_shelter_b3_dumping_hole_801866CC(GpCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              v;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        ang2      = ang + 0x400;
        prim->y3  = block->sy + (u16)block->dy;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws a textured billboard at the world position of `arg0`, projected
/// through `GsWSMATRIX`: one semi-transparent axis-aligned `POLY_FT4`, a
/// square of half-side `(s16)arg2 * 55` over the depth, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks the
/// frame, a 56x56 cell in a four-by-two grid of the texture page. Nothing is
/// drawn when the projection flags an error.
static void func_shelter_b3_dumping_hole_80186AB8(GpCoord* arg0, s32 arg1, s32 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            idx;
    u32            cell;
    s32            row;
    u8             u0;
    u8             u1;
    u8             v0;
    u8             v1;

    idx           = arg1;
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x4393;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row;
        v1          = row + 0x37;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->step = ((s16)arg2 * 55) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    MATRIX*    m;
    s32        i;

    mem   = (GpEffWork*)arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b3_dumping_hole_80186AB8(coord, (mem->age / 2) & 0xFFFF, 0x380);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                                = &coord->coord;
        coord->sub                       = mem->parent;
        MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
        MATRIX_PAIR(m, 0, 2)             = 0;
        MATRIX_PAIR(m, 1, 1)             = 0x1000;
        MATRIX_PAIR(m, 2, 0)             = 0;
        m->m[2][2]                       = 0x1000;
        coord->coord.t[2]                = 0;
        coord->coord.t[1]                = 0;
        coord->coord.t[0]                = 0;
        coord->flg                       = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->age += 1;
    switch (arg0->spawnArg1.value) {
        case 0:
            Gp_SpawnEff(0x6019A, coord, 0x14002400, NULL);
            arg0->spawnArg1.value = 1;
            return;
        case 1:
            func_shelter_b3_dumping_hole_80186AB8(coord, (mem->age / 2) & 0xFFFF, 0x380);
            if (!(mem->age & 1)) {
                Gp_SpawnEff(0x6019A, coord, 0x1001400, NULL);
            }
            mem->age += 1;
            return;
        case 2:
            Gp_SpawnEff(0x6019A, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x6019A, coord, 0x2002400, NULL);
                Gp_SpawnEff(0x60199, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1.value = 3;
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}
