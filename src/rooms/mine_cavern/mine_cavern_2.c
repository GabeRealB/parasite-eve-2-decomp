#include "rooms/mine_cavern.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "mine_cavern_private.h"

#include "actors/task_tables.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pairsrc.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"

/// Work block a mine_cavern task parks at `Task::work`, allocated with
/// `memCalloc(0x14C, 0)` by the state-0 handler `func_mine_cavern_80182E34`
/// (and by `func_mine_cavern_801836D0`). It carries two `GpObj` display nodes:
/// the `+0x40` one is what `func_mine_cavern_80183890` is given away by - it
/// clears `obj40.flags` with `andi 0x7FFF` at +0x5E and then passes `&obj40` (a
/// `+0x40` on the same base pointer) to `Gp_UnlinkObj`, the way
/// `func_mine_cavern_80183860` does on its own exit path - and
/// `func_mine_cavern_801838F4` reaches the second by `+0xC0`, clearing its
/// flags at +0xDE the same way. `field_148` is the counter that function ticks
/// and switch-dispatches on (against 0x3C) and that `func_mine_cavern_80183890`
/// clears on its way out.
///
/// `coord` is the block's own display coordinate. `func_mine_cavern_80183AD4`
/// resets it - identity rotation, parked at (0, -0x320, 0) - and hangs the
/// model's own coordinate (`TmdObject::coords`) under it as `sub`, which is
/// what leaves the model's positions relative to that spot.
///
/// `recs` and `recE0` are contact tables the collision tests fill:
/// `func_mine_cavern_801830F0` looks through `recs` for a contact of class 2 and
/// releases both tables at the end of its tick.
///
/// `light` and `color` are the two matrices the block itself supplies to the
/// model: `func_mine_cavern_801836D0` publishes `&work->light` / `&work->color`
/// into `TmdObject::lightMtx` / `field_20`, which is what `Tmd_SetupDraw` loads
/// in place of `GsLIGHTWSMATRIX` and `D_80074080`.
typedef struct MineCavernWork {
    /* 0x000 */ MATRIX  light;
    /* 0x020 */ MATRIX  color;
    /* 0x040 */ GpObj   obj40;
    /* 0x060 */ GpRec18 recs[4];
    /* 0x0C0 */ GpObj   objC0;
    /* 0x0E0 */ GpRec18 recE0;
    /* 0x0F8 */ GpCoord coord;
    /* 0x148 */ u16     field_148;
    /* 0x14A */ byte    pad_14A[2];
} MineCavernWork;
STATIC_ASSERT_SIZEOF(MineCavernWork, 0x14C);

typedef struct {
    u8  center[3];
    u8  rim[3];
    u16 retained;
} MineCavernGlowPalette;
extern MineCavernGlowPalette D_mine_cavern_8018E350;
extern MineCavernGlowPalette D_mine_cavern_8018E358;

// Bounded byte views preserve the legacy per-channel loads.
extern u8 MineCavernGlowByte8018E350[1] __asm__("D_mine_cavern_8018E350");
extern u8 MineCavernGlowByte8018E351[1] __asm__("D_mine_cavern_8018E350+1");
extern u8 MineCavernGlowByte8018E352[1] __asm__("D_mine_cavern_8018E350+2");
extern u8 MineCavernGlowByte8018E353[1] __asm__("D_mine_cavern_8018E350+3");
extern u8 MineCavernGlowByte8018E354[1] __asm__("D_mine_cavern_8018E350+4");
extern u8 MineCavernGlowByte8018E355[1] __asm__("D_mine_cavern_8018E350+5");
extern u8 MineCavernGlowByte8018E358[1] __asm__("D_mine_cavern_8018E358");
extern u8 MineCavernGlowByte8018E359[1] __asm__("D_mine_cavern_8018E358+1");
extern u8 MineCavernGlowByte8018E35A[1] __asm__("D_mine_cavern_8018E358+2");
extern u8 MineCavernGlowByte8018E35B[1] __asm__("D_mine_cavern_8018E358+3");
extern u8 MineCavernGlowByte8018E35C[1] __asm__("D_mine_cavern_8018E358+4");
extern u8 MineCavernGlowByte8018E35D[1] __asm__("D_mine_cavern_8018E358+5");

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} MineCavernHaloStorage;
STATIC_ASSERT_SIZEOF(MineCavernHaloStorage, 20);
extern MineCavernHaloStorage D_mine_cavern_80188FCC;

extern SVECTOR D_mine_cavern_80188F64[];
extern SVECTOR D_mine_cavern_80188F7C[];
extern SVECTOR D_mine_cavern_80188F84[];
extern SVECTOR D_mine_cavern_80188F8C[];
extern SVECTOR D_mine_cavern_80188F94[];
extern SVECTOR D_mine_cavern_80188F9C[];
extern SVECTOR D_mine_cavern_80188FB4[];
extern SVECTOR D_mine_cavern_80188FBC;
extern SVECTOR D_mine_cavern_80188FC4[];

static void func_mine_cavern_8017F50C(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_mine_cavern_801804CC(GpCoord* coord, s16 size);
static void func_mine_cavern_80181864(void);
static void func_mine_cavern_80182184(void);
static void func_mine_cavern_80182454(void);
static void func_mine_cavern_801825C8(s16 arg0);
static void func_mine_cavern_80182CEC(Task* arg0);
static void func_mine_cavern_80182DA8(Task* task);
static void func_mine_cavern_801838F4(GpEnemy* arg0, Task* arg1);
static void func_mine_cavern_80183AD4(GpEnemy* enemy, Task* task);

/// Current screen id at 0x8007218B.

static void func_mine_cavern_80183860(Task* arg0);

/// Colour of the glow fan's centre vertex, one channel per symbol.
///
/// Each channel is its own symbol, reloaded on every use, and is declared as an
/// array because the fan's position stores are only ordered against loads from
/// aggregate memory: the scheduler treats a halfword store into the primitive
/// and a load from a plain scalar global as independent, but not a load from an
/// array element.

/// Colour of the glow fan's two rim vertices, declared as the centre colour is.

/// Colour of the point glow fans' centre vertex, one channel per symbol and
/// declared as arrays for the same reason as the cavern glow's colours.

/// Colour of the point glow fans' two rim vertices.

/// Mode byte the cavern enemy's hit check switches on: 1 skips the check and 2
/// hides the model and skips it. Its wider role is unproven.

/// Scratch block the enemy's hit check works in: the model's world position
/// (then the offset to the player's model), the offset to the player or to a
/// contact, and the values derived from that contact.
typedef struct _MineCavernHitScratch {
    VECTOR3 pos;
    s32     pad_C;
    SVECTOR d;
    s32     dist;
    u32     key;
    s32     bits;
    s16     angle;
    s16     damage;
} _MineCavernHitScratch;

static void func_mine_cavern_8017E774(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_mine_cavern_8017EFB8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_mine_cavern_801809F8(GpCoord* arg0, s32 arg1);
static void func_mine_cavern_80180D70(GpCoord* arg0, s16 arg1, u8* arg2);

extern GpGridParams   D_mine_cavern_8018981C[1];
extern GpObj3A        D_mine_cavern_8018E078[2];
extern GpObj4C        D_mine_cavern_8018D154[20];
extern GpObj4C        D_mine_cavern_8018D744[18];
extern GpObj4C        D_mine_cavern_8018DC9C[13];
extern GpRoomCoordSet D_mine_cavern_8018D13C[1];

extern GpSprtCmd  D_mine_cavern_80189BC4[2];
extern GpSprtCmd  D_mine_cavern_80189FA8[4];
extern GpSprtCmd  D_mine_cavern_8018A4C8[7];
extern GpSprtCmd  D_mine_cavern_8018A8D4[7];
extern GpSprtCmd  D_mine_cavern_8018AD94[8];
extern GpSprtCmd  D_mine_cavern_8018B108[3];
extern GpSprtCmd  D_mine_cavern_8018B42C[3];
extern GpSprtCmd  D_mine_cavern_8018B660[3];
extern GpSprtCmd  D_mine_cavern_8018B754[3];
extern GpSprtCmd  D_mine_cavern_8018B7A8[3];
extern GpSprtCmd  D_mine_cavern_8018B7C0[2];
extern GpSprtCmd  D_mine_cavern_8018B7D0[2];
extern GpSprtCmd  D_mine_cavern_8018B7E0[2];
extern GpSprtCmd  D_mine_cavern_8018B7F0[2];
extern GpSprtCmd  D_mine_cavern_8018B800[2];
extern GpSprtCmd  D_mine_cavern_8018B810[2];
extern GpSprtCmd  D_mine_cavern_8018B820[2];
extern GpSprtCmd  D_mine_cavern_8018B830[2];
extern GpSprtCmd  D_mine_cavern_8018B840[2];
extern GpSprtCmd  D_mine_cavern_8018BC10[5];
extern GpSprtCmd  D_mine_cavern_8018C048[4];
extern GpSprtCmd  D_mine_cavern_8018C504[7];
extern GpSprtCmd  D_mine_cavern_8018C8E8[5];
extern GpSprtCmd  D_mine_cavern_8018CCD0[6];
extern GpSprtElem D_mine_cavern_80189BD4[49];
extern GpSprtElem D_mine_cavern_80189FC8[64];
extern GpSprtElem D_mine_cavern_8018A500[49];
extern GpSprtElem D_mine_cavern_8018A90C[58];
extern GpSprtElem D_mine_cavern_8018ADD4[41];
extern GpSprtElem D_mine_cavern_8018B120[39];
extern GpSprtElem D_mine_cavern_8018B444[27];
extern GpSprtElem D_mine_cavern_8018B678[11];
extern GpSprtElem D_mine_cavern_8018B76C[3];
extern GpSprtElem D_mine_cavern_8018B850[48];
extern GpSprtElem D_mine_cavern_8018BC38[52];
extern GpSprtElem D_mine_cavern_8018C068[59];
extern GpSprtElem D_mine_cavern_8018C53C[47];
extern GpSprtElem D_mine_cavern_8018C910[48];

void func_mine_cavern_8017E330(void);
void func_mine_cavern_8017E358(void);
void func_mine_cavern_8017E360(void);

MineCavernMessageEntry D_mine_cavern_80183C6C[7] = {
    { 5102, { .call3 = func_mine_cavern_8017D908 } },
    { 5105, { .call0 = func_mine_cavern_8017DC50 } },
    { 5103, { .call1 = func_mine_cavern_8017DC58 } },
    { 5104, { .call2 = func_mine_cavern_8017DAA0 } },
    { 5108, { .call0 = func_mine_cavern_8017DC9C } },
    { 5106, { .call4 = func_mine_cavern_8017DD38 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_mine_cavern_80183CA4[2] = {
    { 0, 32, func_mine_cavern_8017DD6C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MineCavernPoseBank66FC;

MineCavernPoseBank66FC D_mine_cavern_80183CBC = { .poses = {
#include "assets/mine_cavern_animation_069D8_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80183D04[46] = {
#include "assets/mine_cavern_animation_069D8_bank4.inc"
};

GpAnimRec D_mine_cavern_80183DBC[109] = {
#include "assets/mine_cavern_animation_069D8_records.inc"
};

u16 D_mine_cavern_80183F70[20] = {
#include "assets/mine_cavern_animation_069D8_indices.inc"
};

GpAnimSet D_mine_cavern_80183F98 = {
    D_mine_cavern_80183DBC,
    D_mine_cavern_80183F70,
    { NULL, D_mine_cavern_80183CBC.words, NULL, NULL, D_mine_cavern_80183D04, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[13];
    GpPackedSvec words[39];
} MineCavernPoseBank6A00;

MineCavernPoseBank6A00 D_mine_cavern_80183FC0 = { .poses = {
#include "assets/mine_cavern_animation_07178_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_8018405C[179] = {
#include "assets/mine_cavern_animation_07178_bank4.inc"
};

GpAnimRec D_mine_cavern_80184328[250] = {
#include "assets/mine_cavern_animation_07178_records.inc"
};

u16 D_mine_cavern_80184710[20] = {
#include "assets/mine_cavern_animation_07178_indices.inc"
};

GpAnimSet D_mine_cavern_80184738 = {
    D_mine_cavern_80184328,
    D_mine_cavern_80184710,
    { NULL, D_mine_cavern_80183FC0.words, NULL, NULL, D_mine_cavern_8018405C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MineCavernPoseBank71A0;

MineCavernPoseBank71A0 D_mine_cavern_80184760 = { .poses = {
#include "assets/mine_cavern_animation_07544_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_801847A8[64] = {
#include "assets/mine_cavern_animation_07544_bank4.inc"
};

GpAnimRec D_mine_cavern_801848A8[141] = {
#include "assets/mine_cavern_animation_07544_records.inc"
};

u16 D_mine_cavern_80184ADC[20] = {
#include "assets/mine_cavern_animation_07544_indices.inc"
};

GpAnimSet D_mine_cavern_80184B04 = {
    D_mine_cavern_801848A8,
    D_mine_cavern_80184ADC,
    { NULL, D_mine_cavern_80184760.words, NULL, NULL, D_mine_cavern_801847A8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} MineCavernPoseBank756C;

MineCavernPoseBank756C D_mine_cavern_80184B2C = { .poses = {
#include "assets/mine_cavern_animation_07784_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80184B5C[42] = {
#include "assets/mine_cavern_animation_07784_bank4.inc"
};

GpAnimRec D_mine_cavern_80184C04[70] = {
#include "assets/mine_cavern_animation_07784_records.inc"
};

u16 D_mine_cavern_80184D1C[20] = {
#include "assets/mine_cavern_animation_07784_indices.inc"
};

GpAnimSet D_mine_cavern_80184D44 = {
    D_mine_cavern_80184C04,
    D_mine_cavern_80184D1C,
    { NULL, D_mine_cavern_80184B2C.words, NULL, NULL, D_mine_cavern_80184B5C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} MineCavernPoseBank77AC;

MineCavernPoseBank77AC D_mine_cavern_80184D6C = { .poses = {
#include "assets/mine_cavern_animation_07958_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80184D90[31] = {
#include "assets/mine_cavern_animation_07958_bank4.inc"
};

GpAnimRec D_mine_cavern_80184E0C[57] = {
#include "assets/mine_cavern_animation_07958_records.inc"
};

u16 D_mine_cavern_80184EF0[20] = {
#include "assets/mine_cavern_animation_07958_indices.inc"
};

GpAnimSet D_mine_cavern_80184F18 = {
    D_mine_cavern_80184E0C,
    D_mine_cavern_80184EF0,
    { NULL, D_mine_cavern_80184D6C.words, NULL, NULL, D_mine_cavern_80184D90, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} MineCavernPoseBank7980;

MineCavernPoseBank7980 D_mine_cavern_80184F40 = { .poses = {
#include "assets/mine_cavern_animation_07BA4_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80184F64[29] = {
#include "assets/mine_cavern_animation_07BA4_bank4.inc"
};

GpAnimRec D_mine_cavern_80184FD8[89] = {
#include "assets/mine_cavern_animation_07BA4_records.inc"
};

u16 D_mine_cavern_8018513C[20] = {
#include "assets/mine_cavern_animation_07BA4_indices.inc"
};

GpAnimSet D_mine_cavern_80185164 = {
    D_mine_cavern_80184FD8,
    D_mine_cavern_8018513C,
    { NULL, D_mine_cavern_80184F40.words, NULL, NULL, D_mine_cavern_80184F64, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} MineCavernPoseBank7BCC;

MineCavernPoseBank7BCC D_mine_cavern_8018518C = { .poses = {
#include "assets/mine_cavern_animation_08178_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_801851EC[117] = {
#include "assets/mine_cavern_animation_08178_bank4.inc"
};

GpAnimRec D_mine_cavern_801853C0[212] = {
#include "assets/mine_cavern_animation_08178_records.inc"
};

u16 D_mine_cavern_80185710[20] = {
#include "assets/mine_cavern_animation_08178_indices.inc"
};

GpAnimSet D_mine_cavern_80185738 = {
    D_mine_cavern_801853C0,
    D_mine_cavern_80185710,
    { NULL, D_mine_cavern_8018518C.words, NULL, NULL, D_mine_cavern_801851EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} MineCavernPoseBank81A0;

MineCavernPoseBank81A0 D_mine_cavern_80185760 = { .poses = {
#include "assets/mine_cavern_animation_08420_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80185790[46] = {
#include "assets/mine_cavern_animation_08420_bank4.inc"
};

GpAnimRec D_mine_cavern_80185848[92] = {
#include "assets/mine_cavern_animation_08420_records.inc"
};

u16 D_mine_cavern_801859B8[20] = {
#include "assets/mine_cavern_animation_08420_indices.inc"
};

GpAnimSet D_mine_cavern_801859E0 = {
    D_mine_cavern_80185848,
    D_mine_cavern_801859B8,
    { NULL, D_mine_cavern_80185760.words, NULL, NULL, D_mine_cavern_80185790, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[12];
    GpPackedSvec words[36];
} MineCavernPoseBank8448;

MineCavernPoseBank8448 D_mine_cavern_80185A08 = { .poses = {
#include "assets/mine_cavern_animation_08AC0_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80185A98[164] = {
#include "assets/mine_cavern_animation_08AC0_bank4.inc"
};

GpAnimRec D_mine_cavern_80185D28[204] = {
#include "assets/mine_cavern_animation_08AC0_records.inc"
};

u16 D_mine_cavern_80186058[20] = {
#include "assets/mine_cavern_animation_08AC0_indices.inc"
};

GpAnimSet D_mine_cavern_80186080 = {
    D_mine_cavern_80185D28,
    D_mine_cavern_80186058,
    { NULL, D_mine_cavern_80185A08.words, NULL, NULL, D_mine_cavern_80185A98, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} MineCavernPoseBank8AE8;

MineCavernPoseBank8AE8 D_mine_cavern_801860A8 = { .poses = {
#include "assets/mine_cavern_animation_08CB4_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_801860C0[20] = {
#include "assets/mine_cavern_animation_08CB4_bank4.inc"
};

GpAnimRec D_mine_cavern_80186110[79] = {
#include "assets/mine_cavern_animation_08CB4_records.inc"
};

u16 D_mine_cavern_8018624C[20] = {
#include "assets/mine_cavern_animation_08CB4_indices.inc"
};

GpAnimSet D_mine_cavern_80186274 = {
    D_mine_cavern_80186110,
    D_mine_cavern_8018624C,
    { NULL, D_mine_cavern_801860A8.words, NULL, NULL, D_mine_cavern_801860C0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[11];
    GpPackedSvec words[33];
} MineCavernPoseBank8CDC;

MineCavernPoseBank8CDC D_mine_cavern_8018629C = { .poses = {
#include "assets/mine_cavern_animation_092C4_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80186320[150] = {
#include "assets/mine_cavern_animation_092C4_bank4.inc"
};

GpAnimRec D_mine_cavern_80186578[185] = {
#include "assets/mine_cavern_animation_092C4_records.inc"
};

u16 D_mine_cavern_8018685C[20] = {
#include "assets/mine_cavern_animation_092C4_indices.inc"
};

GpAnimSet D_mine_cavern_80186884 = {
    D_mine_cavern_80186578,
    D_mine_cavern_8018685C,
    { NULL, D_mine_cavern_8018629C.words, NULL, NULL, D_mine_cavern_80186320, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} MineCavernPoseBank92EC;

MineCavernPoseBank92EC D_mine_cavern_801868AC = { .poses = {
#include "assets/mine_cavern_animation_096E4_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_8018690C[88] = {
#include "assets/mine_cavern_animation_096E4_bank4.inc"
};

GpAnimRec D_mine_cavern_80186A6C[132] = {
#include "assets/mine_cavern_animation_096E4_records.inc"
};

u16 D_mine_cavern_80186C7C[20] = {
#include "assets/mine_cavern_animation_096E4_indices.inc"
};

GpAnimSet D_mine_cavern_80186CA4 = {
    D_mine_cavern_80186A6C,
    D_mine_cavern_80186C7C,
    { NULL, D_mine_cavern_801868AC.words, NULL, NULL, D_mine_cavern_8018690C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} MineCavernPoseBank970C;

MineCavernPoseBank970C D_mine_cavern_80186CCC = { .poses = {
#include "assets/mine_cavern_animation_099C8_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80186D08[58] = {
#include "assets/mine_cavern_animation_099C8_bank4.inc"
};

GpAnimRec D_mine_cavern_80186DF0[92] = {
#include "assets/mine_cavern_animation_099C8_records.inc"
};

u16 D_mine_cavern_80186F60[20] = {
#include "assets/mine_cavern_animation_099C8_indices.inc"
};

GpAnimSet D_mine_cavern_80186F88 = {
    D_mine_cavern_80186DF0,
    D_mine_cavern_80186F60,
    { NULL, D_mine_cavern_80186CCC.words, NULL, NULL, D_mine_cavern_80186D08, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} MineCavernPoseBank99F0;

MineCavernPoseBank99F0 D_mine_cavern_80186FB0 = { .poses = {
#include "assets/mine_cavern_animation_09C2C_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80186FC8[22] = {
#include "assets/mine_cavern_animation_09C2C_bank4.inc"
};

GpAnimRec D_mine_cavern_80187020[105] = {
#include "assets/mine_cavern_animation_09C2C_records.inc"
};

u16 D_mine_cavern_801871C4[20] = {
#include "assets/mine_cavern_animation_09C2C_indices.inc"
};

GpAnimSet D_mine_cavern_801871EC = {
    D_mine_cavern_80187020,
    D_mine_cavern_801871C4,
    { NULL, D_mine_cavern_80186FB0.words, NULL, NULL, D_mine_cavern_80186FC8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} MineCavernPoseBank9C54;

MineCavernPoseBank9C54 D_mine_cavern_80187214 = { .poses = {
#include "assets/mine_cavern_animation_09F1C_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_80187244[40] = {
#include "assets/mine_cavern_animation_09F1C_bank4.inc"
};

GpAnimRec D_mine_cavern_801872E4[116] = {
#include "assets/mine_cavern_animation_09F1C_records.inc"
};

u16 D_mine_cavern_801874B4[20] = {
#include "assets/mine_cavern_animation_09F1C_indices.inc"
};

GpAnimSet D_mine_cavern_801874DC = {
    D_mine_cavern_801872E4,
    D_mine_cavern_801874B4,
    { NULL, D_mine_cavern_80187214.words, NULL, NULL, D_mine_cavern_80187244, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} MineCavernPoseBank9F44;

MineCavernPoseBank9F44 D_mine_cavern_80187504 = { .poses = {
#include "assets/mine_cavern_animation_0A38C_bank1.inc"
                                                  } };

GpPackedSvec D_mine_cavern_8018757C[95] = {
#include "assets/mine_cavern_animation_0A38C_bank4.inc"
};

GpAnimRec D_mine_cavern_801876F8[139] = {
#include "assets/mine_cavern_animation_0A38C_records.inc"
};

u16 D_mine_cavern_80187924[20] = {
#include "assets/mine_cavern_animation_0A38C_indices.inc"
};

GpAnimSet D_mine_cavern_8018794C = {
    D_mine_cavern_801876F8,
    D_mine_cavern_80187924,
    { NULL, D_mine_cavern_80187504.words, NULL, NULL, D_mine_cavern_8018757C, NULL, NULL, NULL },
};

TaskDesc D_mine_cavern_80187974 = { 0, 192, func_mine_cavern_8017E18C, { .model = NULL } };

GpAnimSet* D_mine_cavern_80187980[17] = {
    NULL,
    &D_mine_cavern_80183F98,
    &D_mine_cavern_80184B04,
    &D_mine_cavern_80184D44,
    &D_mine_cavern_80184F18,
    &D_mine_cavern_80185164,
    &D_mine_cavern_80185738,
    &D_mine_cavern_801859E0,
    &D_mine_cavern_80184738,
    &D_mine_cavern_80186080,
    &D_mine_cavern_80186274,
    &D_mine_cavern_80186884,
    &D_mine_cavern_80186CA4,
    &D_mine_cavern_80186F88,
    &D_mine_cavern_801871EC,
    &D_mine_cavern_801874DC,
    &D_mine_cavern_8018794C,
};

GpCopyArg D_mine_cavern_801879C4 = { { .sets = D_mine_cavern_80187980 }, 17 };

GpAnimArg D_mine_cavern_801879CC = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_mine_cavern_801879E0 = { { .index = 1 }, 48, 1, 30, 0 };

GpAnimArg D_mine_cavern_801879F4 = { { .index = 1 }, 49, 1, 30, 0 };

GpAnimArg D_mine_cavern_80187A08 = { { .index = 1 }, 50, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187A1C = { { .index = 1 }, 51, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187A30 = { { .index = 1 }, 52, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187A44 = { { .index = 1 }, 53, 1, 20, 0 };

GpAnimArg D_mine_cavern_80187A58 = { { .index = 1 }, 54, 1, 20, 0 };

GpAnimArg D_mine_cavern_80187A6C = { { .index = 1 }, 55, 1, 30, 0 };

GpAnimArg D_mine_cavern_80187A80 = { { .index = 1 }, 56, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187A94 = { { .index = 1 }, 57, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187AA8 = { { .index = 1 }, 58, 1, 5, 0 };

GpAnimArg D_mine_cavern_80187ABC[3] = {
    { { .index = 1 }, 59, 1, 5, 0 },
    { { .index = 1 }, 60, 1, 2, 0 },
    { { .index = 1 }, 61, 0, 0, 0 },
};

GpAnimArg D_mine_cavern_80187AF8 = { { .index = 1 }, 62, 0, 0, 0 };

GpAnimArg D_mine_cavern_80187B0C = { { .index = 1 }, 63, 0, 0, 0 };

GpAnimArg D_mine_cavern_80187B20 = { { .index = 1 }, 9, 1, 30, 1 };

GpXformArg D_mine_cavern_80187B34 = { { 0x4402, 0, 1680, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_mine_cavern_80187B4C = { { 0x31A6, 0, 1680, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_mine_cavern_80187B64 = { { 6660, 0, 7200, 0 }, { 0, 569, 0, 0 } };

GpXformArg D_mine_cavern_80187B7C = { { 6550, 0, 7100, 0 }, { 0, 910, 0, 0 } };

GpXformArg D_mine_cavern_80187B94 = { { 3880, 0, 6678, 0 }, { 0, 910, 0, 0 } };

GpXformArg D_mine_cavern_80187BAC = { { 1900, 0, 160, 0 }, { 0, -114, 0, 0 } };

GpOverrideArg D_mine_cavern_80187BC4 = { 55, 48 };

GpXformArg D_mine_cavern_80187BCC = { { 7400, 0, 1750, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_mine_cavern_80187BE4 = { { 9250, 0, 1300, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_mine_cavern_80187BFC = { { 9250, 0, 1800, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_mine_cavern_80187C14 = { { 7550, 0, 7360, 0 }, { 0, -455, 0, 0 } };

GpXformArg D_mine_cavern_80187C2C = { { 1410, 0, 3000, 0 }, { 0, 910, 0, 0 } };

GpCmdArg D_mine_cavern_80187C44 = { { .loc = { 4, 2 } }, 0 };

GpCmdArg D_mine_cavern_80187C48 = { { .loc = { 4, 2 } }, 1 };

GpCmdArg D_mine_cavern_80187C4C = { { .loc = { 4, 2 } }, 2 };

GpCmdArg D_mine_cavern_80187C50 = { { .loc = { 4, 2 } }, 3 };

GpCmdArg D_mine_cavern_80187C54 = { { .loc = { 4, 2 } }, 4 };

GpCmdArg D_mine_cavern_80187C58 = { { .loc = { 4, 2 } }, 5 };

GpCmdArg D_mine_cavern_80187C5C = { { .loc = { 4, 2 } }, 6 };

GpCmdArg D_mine_cavern_80187C60 = { { .loc = { 4, 2 } }, 7 };

GpCmdArg D_mine_cavern_80187C64 = { { .loc = { 4, 2 } }, 10 };

GpCmdArg D_mine_cavern_80187C68 = { { .loc = { 4, 2 } }, 11 };

GpCmdArg D_mine_cavern_80187C6C = { { .loc = { 4, 2 } }, 12 };

GpCmdArg D_mine_cavern_80187C70 = { { .loc = { 4, 2 } }, 13 };

GpEvsCmd D_mine_cavern_80187C74[41] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B34 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_cavern_801879C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_801879E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C44 }, { .value = 0 } },
    { 3, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_mine_cavern_80187B4C }, { .storage = &D_mine_cavern_80187BC4 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_801879F4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A58 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A08 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A1C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C64 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187BCC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A30 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C48 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187BE4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A44 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C4C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187BFC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_mine_cavern_8017E180 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E0B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C68 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187BFC }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_cavern_8018804C[19] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B4C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_cavern_801879C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A44 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C4C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187BFC }, { .value = 0 } },
    { 13, { .callbackU8 = func_mine_cavern_8017E180 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E0B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C68 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_cavern_80188214[60] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mine_cavern_8017DFAC }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B64 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_cavern_801879C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_801879E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187C14 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C58 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187A80 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C50 }, { .value = 0 } },
    { 15, { .value = 0x401E0013 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187AA8 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187B0C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B7C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54020013 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B94 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187B20 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C54 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401E0014 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401E0015 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401E0015 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401E0014 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401E0016 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_mine_cavern_8017E150 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_mine_cavern_8017E180 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E360 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E0B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C68 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_cavern_801887B4[27] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187B94 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187B20 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187C14 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C58 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C50 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C54 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C68 }, { .value = 0 } },
    { 13, { .callback = func_mine_cavern_8017DFAC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_mine_cavern_8017E088 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_mine_cavern_8017E150 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_mine_cavern_8017E180 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E360 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E0B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_cavern_80188A3C[31] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187BAC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_cavern_801879C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_801879E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187C2C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C70 }, { .value = 0 } },
    { 3, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_80187AF8 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E330 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E358 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mine_cavern_8017DFAC }, { .value = 95 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E15C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C60 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_cavern_80188D24[24] = {
    { 13, { .callback = func_mine_cavern_8017E0F4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E2FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mine_cavern_80187BAC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_cavern_801879C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_cavern_801879E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mine_cavern_80187C2C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C70 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mine_cavern_80187C60 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E330 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E358 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mine_cavern_8017DFAC }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mine_cavern_8017E15C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_mine_cavern_80188F64[3] = {
    { 970, -2060, -950, 0 },
    { 830, -1100, -1260, 0 },
    { 1080, -70, -710, 0 },
};

SVECTOR D_mine_cavern_80188F7C[1] = {
    { 4550, -2090, 280, 0 },
};

SVECTOR D_mine_cavern_80188F84[1] = {
    { 0x36B0, -2060, 290, 0 },
};

SVECTOR D_mine_cavern_80188F8C[1] = {
    { 200, -2060, 5160, 0 },
};

SVECTOR D_mine_cavern_80188F94[1] = {
    { 0x4560, -2060, 5000, 0 },
};

SVECTOR D_mine_cavern_80188F9C[3] = {
    { 9000, -2040, 8710, 0 },
    { 6910, -2780, 3700, 0 },
    { 6890, -1500, 4690, 0 },
};

SVECTOR D_mine_cavern_80188FB4[1] = {
    { 0x2E86, -1890, 5620, 0 },
};

SVECTOR D_mine_cavern_80188FBC = { 5990, -1460, -330, 0 };

SVECTOR D_mine_cavern_80188FC4[1] = {
    { 900, -1730, -0x36CE, 0 },
};

MineCavernHaloStorage D_mine_cavern_80188FCC = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x3C95 };

GpRoomObjRec D_mine_cavern_80188FE0[3] = {
    { D_mine_cavern_8018981C, D_mine_cavern_8018D154, D_mine_cavern_8018DC9C, D_mine_cavern_8018E078 },
    { D_mine_cavern_8018981C, D_mine_cavern_8018D744, D_mine_cavern_8018DC9C, D_mine_cavern_8018E078 },
    { D_mine_cavern_8018981C, D_mine_cavern_8018D744, D_mine_cavern_8018DC9C, D_mine_cavern_8018E078 },
};

GpRoomCoordRec D_mine_cavern_80189010[3] = {
    { D_mine_cavern_8018D13C, NULL },
    { D_mine_cavern_8018D13C, NULL },
    { D_mine_cavern_8018D13C, NULL },
};

u8 D_mine_cavern_80189028[28] = {
    1,
    3,
    2,
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
    0,
    0,
    0,
};

u8 D_mine_cavern_80189044[28] = {
    1,
    3,
    2,
    4,
    25,
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
    24,
    23,
    5,
    0,
    0,
    0,
};

u8* D_mine_cavern_80189060[3] = {
    D_8010CAF8,
    D_mine_cavern_80189028,
    D_mine_cavern_80189044,
};

GpViewCountRec D_mine_cavern_8018906C[3] = {
    { { .bytes = { 25, 0 } } },
    { { .bytes = { 25, 0 } } },
    { { .bytes = { 25, 0 } } },
};

GpWarpRec D_mine_cavern_80189074[2] = {
    { { .words = { 3072, 0x4524, 0, 2500 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x4524, 0, 2500 } }, { 0, 0, 0, 0 }, 0x54020002, 0x54020001, 0, 2, 0, 448 },
    { { .words = { 0, 2840, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 0, 2840, 0, 0 } }, { 0, 0, 0, 0 }, 0x54020004, 0x54020003, 0x54020012, 23, 0, 441 },
};

SVECTOR D_mine_cavern_801890E4[30] = {
    { 3109, 0, 2667, 0 },
    { 4070, 0, 459, 0 },
    { 3774, 0, 1592, 0 },
    { -3949, 0, 1089, 0 },
    { -479, 0, 4068, 0 },
    { 3369, 0, 2329, 0 },
    { 0, 0, 4096, 0 },
    { -3904, 0, 1240, 0 },
    { -3901, 0, -1250, 0 },
    { 0, 0, -4096, 0 },
    { 3843, 0, -1418, 0 },
    { 3958, 0, -1053, 0 },
    { -3824, 0, -1467, 0 },
    { 85, 0, -4095, 0 },
    { 2955, 0, -2837, 0 },
    { 2272, 0, 3408, 0 },
    { -4096, 0, 0, 0 },
    { 4092, 0, 176, 0 },
    { -2762, 0, -3025, 0 },
    { -27, 0, -4096, 0 },
    { 4077, 0, -390, 0 },
    { 743, 0, -4028, 0 },
    { 4096, 0, 0, 0 },
    { 205, 0, 4091, 0 },
    { -1197, 0, 3917, 0 },
    { 0, -4096, 0, 0 },
    { -135, 0, -4094, 0 },
    { -2644, 0, 3128, 0 },
    { -3539, 0, -2062, 0 },
    { -4096, 0, 15, 0 },
};

SVECTOR D_mine_cavern_801891D4[88] = {
    { -580, -1330, 2510, 0 },
    { -580, 100, 2510, 0 },
    { 930, 100, 750, 0 },
    { 930, -1330, 750, 0 },
    { 1150, 100, -1200, 0 },
    { 1150, -1330, -1200, 0 },
    { 0x3656, -1050, 690, 0 },
    { 0x3656, 100, 690, 0 },
    { 0x37B4, 100, -140, 0 },
    { 0x37B4, -1050, -140, 0 },
    { 3930, -1050, -530, 0 },
    { 3930, 100, -530, 0 },
    { 4250, 100, 630, 0 },
    { 4250, -1050, 630, 0 },
    { 4760, 100, 690, 0 },
    { 4760, -1050, 690, 0 },
    { 5320, 100, -120, 0 },
    { 5320, -1050, -120, 0 },
    { 0x3458, -1050, 690, 0 },
    { 0x3458, 100, 690, 0 },
    { 0x334A, -1050, -160, 0 },
    { 0x334A, 100, -160, 0 },
    { 3730, 100, 9380, 0 },
    { 3730, -1050, 9380, 0 },
    { 4060, -1050, 8350, 0 },
    { 4060, 100, 8350, 0 },
    { 4600, -1050, 8350, 0 },
    { 4600, 100, 8350, 0 },
    { 4980, -1050, 9380, 0 },
    { 4980, 100, 9380, 0 },
    { 0x366A, 100, 8320, 0 },
    { 0x366A, -1050, 8320, 0 },
    { 0x3764, -1050, 9260, 0 },
    { 0x3764, 100, 9260, 0 },
    { 0x3304, 100, 9180, 0 },
    { 0x3304, -1050, 9180, 0 },
    { 0x344E, -1050, 8320, 0 },
    { 0x344E, 100, 8320, 0 },
    { 6920, 100, 2690, 0 },
    { 6920, -2500, 2690, 0 },
    { 0x3B2E, -2500, 2860, 0 },
    { 0x3B2E, 100, 2860, 0 },
    { 0x3D0E, -2500, 3360, 0 },
    { 0x3D0E, 100, 3360, 0 },
    { 0x3D0E, 100, 5390, 0 },
    { 0x3D0E, -2500, 5390, 0 },
    { 0x39A8, -2500, 5970, 0 },
    { 0x39A8, 100, 5970, 0 },
    { 0x2B20, -2500, 5970, 0 },
    { 0x2B20, 100, 5970, 0 },
    { 0x2B20, -2500, 5070, 0 },
    { 0x2B20, 100, 5070, 0 },
    { 6640, -2500, 5070, 0 },
    { 6640, 100, 5070, 0 },
    { 6600, -2500, 6000, 0 },
    { 6600, 100, 6000, 0 },
    { 2970, -2500, 6000, 0 },
    { 2970, 100, 6000, 0 },
    { 2970, -2500, 2820, 0 },
    { 2970, 100, 2820, 0 },
    { 4730, -5000, 110, 0 },
    { 4730, 100, 110, 0 },
    { 0x44C0, 100, 110, 0 },
    { 0x44C0, -5000, 110, 0 },
    { 0x45BA, -5000, 4570, 0 },
    { 0x45BA, 100, 4570, 0 },
    { 0x45BA, 100, 7410, 0 },
    { 0x45BA, -5000, 7410, 0 },
    { 0x3F70, 100, 8880, 0 },
    { 0x3F70, -5000, 8880, 0 },
    { 1220, -5000, 8980, 0 },
    { 1220, 100, 8980, 0 },
    { 1070, 100, 7410, 0 },
    { 1070, -5000, 7410, 0 },
    { 40, 100, 7220, 0 },
    { 40, -5000, 7220, 0 },
    { 40, 100, -470, 0 },
    { 40, -5000, -470, 0 },
    { 2440, -5000, -590, 0 },
    { 2440, 100, -590, 0 },
    { -1000, 0, 0x2710, 0 },
    { 0x4A38, 0, 0x2710, 0 },
    { 0x4A38, 0, -1000, 0 },
    { -1000, 0, -1000, 0 },
    { 0x4808, 100, 820, 0 },
    { 0x4808, -5000, 820, 0 },
    { 0x4812, -5000, 3540, 0 },
    { 0x4812, 100, 3540, 0 },
};

GpGridFace D_mine_cavern_80189494[38] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 7, 8, 6, 9 }, 2, 2 },
    { { 11, 12, 10, 13 }, 3, 2 },
    { { 12, 14, 13, 15 }, 4, 2 },
    { { 14, 16, 15, 17 }, 5, 2 },
    { { 19, 7, 18, 6 }, 6, 2 },
    { { 21, 19, 20, 18 }, 7, 2 },
    { { 23, 24, 22, 25 }, 8, 2 },
    { { 24, 26, 25, 27 }, 9, 2 },
    { { 26, 28, 27, 29 }, 10, 2 },
    { { 31, 32, 30, 33 }, 11, 2 },
    { { 35, 36, 34, 37 }, 12, 2 },
    { { 36, 31, 37, 30 }, 9, 2 },
    { { 39, 40, 38, 41 }, 13, 0 },
    { { 40, 42, 41, 43 }, 14, 0 },
    { { 45, 46, 44, 47 }, 15, 0 },
    { { 46, 48, 47, 49 }, 6, 0 },
    { { 48, 50, 49, 51 }, 16, 0 },
    { { 50, 52, 51, 53 }, 6, 0 },
    { { 52, 54, 53, 55 }, 17, 0 },
    { { 54, 56, 55, 57 }, 6, 0 },
    { { 56, 58, 57, 59 }, 16, 0 },
    { { 61, 62, 60, 63 }, 6, 0 },
    { { 65, 66, 64, 67 }, 16, 0 },
    { { 66, 68, 67, 69 }, 18, 0 },
    { { 69, 68, 70, 71 }, 19, 0 },
    { { 71, 72, 70, 73 }, 20, 0 },
    { { 72, 74, 73, 75 }, 21, 0 },
    { { 74, 76, 75, 77 }, 22, 0 },
    { { 77, 76, 78, 79 }, 23, 0 },
    { { 78, 79, 60, 61 }, 24, 0 },
    { { 81, 82, 80, 83 }, 25, 1 },
    { { 38, 59, 39, 58 }, 26, 0 },
    { { 44, 43, 45, 42 }, 22, 0 },
    { { 85, 63, 84, 62 }, 27, 0 },
    { { 87, 65, 86, 64 }, 28, 0 },
    { { 84, 87, 85, 86 }, 29, 0 },
};

s16 D_mine_cavern_8018965C[13] = {
    0,
    1,
    3,
    4,
    5,
    22,
    23,
    29,
    30,
    31,
    32,
    33,
    -1,
};

s16 D_mine_cavern_80189678[10] = {
    0,
    1,
    21,
    22,
    27,
    28,
    29,
    32,
    33,
    -1,
};

s16 D_mine_cavern_8018968C[11] = {
    8,
    9,
    10,
    21,
    22,
    26,
    27,
    28,
    29,
    32,
    -1,
};

s16 D_mine_cavern_801896A4[13] = {
    0,
    1,
    3,
    4,
    5,
    14,
    22,
    23,
    30,
    31,
    32,
    33,
    -1,
};

s16 D_mine_cavern_801896C0[11] = {
    8,
    9,
    10,
    14,
    19,
    20,
    21,
    22,
    32,
    33,
    -1,
};

s16 D_mine_cavern_801896D8[11] = {
    8,
    9,
    10,
    19,
    20,
    21,
    22,
    26,
    27,
    32,
    -1,
};

s16 D_mine_cavern_801896F0[6] = {
    5,
    14,
    23,
    32,
    33,
    -1,
};

s16 D_mine_cavern_801896FC[9] = {
    14,
    17,
    18,
    19,
    20,
    21,
    32,
    33,
    -1,
};

s16 D_mine_cavern_80189710[10] = {
    10,
    12,
    17,
    18,
    19,
    20,
    21,
    26,
    32,
    -1,
};

s16 D_mine_cavern_80189724[9] = {
    2,
    6,
    7,
    14,
    15,
    23,
    32,
    34,
    -1,
};

s16 D_mine_cavern_80189738[13] = {
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    26,
    32,
    34,
    -1,
};

s16 D_mine_cavern_80189754[10] = {
    11,
    12,
    13,
    16,
    17,
    18,
    25,
    26,
    32,
    -1,
};

s16 D_mine_cavern_80189768[13] = {
    2,
    6,
    7,
    14,
    15,
    23,
    24,
    32,
    34,
    35,
    36,
    37,
    -1,
};

s16 D_mine_cavern_80189784[11] = {
    14,
    15,
    16,
    17,
    24,
    25,
    32,
    34,
    36,
    37,
    -1,
};

s16 D_mine_cavern_8018979C[11] = {
    11,
    12,
    13,
    16,
    17,
    24,
    25,
    26,
    32,
    34,
    -1,
};

s16 D_mine_cavern_801897B4[6] = {
    23,
    32,
    35,
    36,
    37,
    -1,
};

s16 D_mine_cavern_801897C0[6] = {
    24,
    25,
    32,
    36,
    37,
    -1,
};

s16 D_mine_cavern_801897CC[4] = {
    24,
    25,
    32,
    -1,
};

s16* D_mine_cavern_801897D4[18] = {
    D_mine_cavern_8018965C,
    D_mine_cavern_80189678,
    D_mine_cavern_8018968C,
    D_mine_cavern_801896A4,
    D_mine_cavern_801896C0,
    D_mine_cavern_801896D8,
    D_mine_cavern_801896F0,
    D_mine_cavern_801896FC,
    D_mine_cavern_80189710,
    D_mine_cavern_80189724,
    D_mine_cavern_80189738,
    D_mine_cavern_80189754,
    D_mine_cavern_80189768,
    D_mine_cavern_80189784,
    D_mine_cavern_8018979C,
    D_mine_cavern_801897B4,
    D_mine_cavern_801897C0,
    D_mine_cavern_801897CC,
};

GpGridParams D_mine_cavern_8018981C[1] = {
    { NULL, D_mine_cavern_801890E4, D_mine_cavern_801891D4, D_mine_cavern_80189494, D_mine_cavern_801897D4, 1000, 1200, 6, 3, 4000, 38 },
};

GpViewRec D_mine_cavern_80189840[25] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -9000, 0x541A, -4500 } }, 269 },
    { { { { -4078, 0, -376 }, { -147, 3767, 1601 }, { 346, 1607, -3751 } }, { -0x3C2B, 3769, -9847 } }, 282 },
    { { { { 760, 0, -4024 }, { -1190, 3912, -224 }, { 3844, 1211, 726 } }, { -7741, 2840, -961 } }, 257 },
    { { { { 1072, 0, 3953 }, { 1176, 3910, -319 }, { -3774, 1219, 1023 } }, { -0x3909, 2740, -561 } }, 257 },
    { { { { -4077, 0, 384 }, { 137, 3822, 1464 }, { -358, 1470, -3806 } }, { -2391, 3550, -9951 } }, 257 },
    { { { { -688, 0, 4037 }, { 1013, 3964, 172 }, { -3908, 1028, -666 } }, { -0x3002, 3145, -8160 } }, 289 },
    { { { { -1126, 0, -3938 }, { -1263, 3879, 361 }, { 3729, 1314, -1066 } }, { -3683, 3259, -8447 } }, 282 },
    { { { { -945, 0, -3985 }, { -1292, 3874, 306 }, { 3770, 1328, -894 } }, { -8013, 3259, -8447 } }, 282 },
    { { { { 3340, 0, -2370 }, { -15, 4095, -21 }, { 2370, 26, 3339 } }, { -0x30EC, 1602, 1567 } }, 257 },
    { { { { 3539, 0, 2061 }, { 130, 4087, -223 }, { -2057, 258, 3532 } }, { -0x3825, 1489, 1491 } }, 551 },
    { { { { -6, 0, 4095 }, { 313, 4083, 0 }, { -4083, 313, -6 } }, { -0x30DE, 1598, -1721 } }, 257 },
    { { { { 2476, 0, -3262 }, { -2253, 2962, -1710 }, { 2359, 2828, 1791 } }, { -0x28A8, 3082, -638 } }, 197 },
    { { { { 1289, 0, 3887 }, { -518, 4059, 171 }, { -3853, -545, 1278 } }, { -0x3974, 602, -808 } }, 246 },
    { { { { -1648, 0, 3749 }, { 215, 4089, 94 }, { -3743, 235, -1645 } }, { -9183, 1059, -8459 } }, 269 },
    { { { { 3593, 0, -1965 }, { -1404, 2863, -2569 }, { 1373, 2928, 2512 } }, { -6222, 3044, -5668 } }, 254 },
    { { { { 841, 0, 4008 }, { -456, 4069, 95 }, { -3982, -466, 835 } }, { -7616, 1152, -6716 } }, 282 },
    { { { { 1753, 0, -3701 }, { 533, 4053, 252 }, { 3663, -590, 1735 } }, { -2707, 799, -5551 } }, 278 },
    { { { { 2645, 0, -3126 }, { -2616, 2242, -2214 }, { 1711, 3427, 1448 } }, { -12, 4448, 151 } }, 282 },
    { { { { -2728, 0, -3055 }, { -283, 4078, 253 }, { 3042, 380, -2716 } }, { -1023, 1439, -1057 } }, 285 },
    { { { { 4090, 0, -212 }, { -77, 3810, -1499 }, { 197, 1501, 3805 } }, { -0x3F71, 3033, 838 } }, 257 },
    { { { { 4068, 0, 476 }, { 189, 3756, -1621 }, { -436, 1632, 3731 } }, { -2741, 3275, 638 } }, 257 },
    { { { { 1072, 0, 3953 }, { 1176, 3910, -319 }, { -3774, 1219, 1023 } }, { -9591, 2740, -561 } }, 257 },
    { { { { -4084, 0, -300 }, { -69, 3985, 942 }, { 292, 944, -3974 } }, { -1641, 2934, -7401 } }, 257 },
    { { { { -4084, 0, -300 }, { -69, 3985, 942 }, { 292, 944, -3974 } }, { -1641, 2934, -7401 } }, 257 },
    { { { { -4077, 0, 384 }, { 137, 3822, 1464 }, { -358, 1470, -3806 } }, { -2391, 3550, -9951 } }, 257 },
};

GpSprtCmd D_mine_cavern_80189BC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_80189BD4[49] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 1376, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -152, -104, 1264, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -72, 1343, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, -32, 1240, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -32, 1207, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 8, 1422, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -16, 1421, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 32, 1485, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 56, 1483, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 56, 1516, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 56, 1486, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 32, 1483, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -16, 1404, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, -48, 1344, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -48, 1307, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, -104, 1221, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -120, 1171, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -120, 1187, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -40, 1278, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, -40, 1258, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 1271, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 80, -48, 1317, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 8, 1516, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1533, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 8, 1375, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -24, 1220, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -8, 1166, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 16, 1149, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 48, 1190, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 80, 1247, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 1223, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -24, 1660, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 1355, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, 8, 1357, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 96, 1241, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 72, 1399, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -32, 1317, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 16, 64, 1372, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 88, 1361, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 16, 48, 1350, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 16, 32, 1366, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 32, 1407, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 56, 1393, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 1319, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, 88, -24, 1317, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, 88, -8, 1224, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, 88, 16, 1197, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 64, 1316, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, 88, 40, 1223, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_80189FA8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 28, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_80189FC8[64] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -24, 519, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 32, 591, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 72, 599, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 80, -120, 519, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, -120, 506, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 48, -120, 531, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 48, -104, 564, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -104, 584, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, -88, 569, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, -80, 554, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -72, 553, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -64, 549, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 152, -80, 519, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -48, 533, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 48, -96, 1121, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 80, -96, 1116, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 104, -120, 1058, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -64, 1123, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 88, -16, 1195, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 40, 1294, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, 40, 1176, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, -120, 1611, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 48, -96, 1705, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 80, -96, 1688, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 72, -32, 1767, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -88, 1462, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -88, 791, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, -40, 803, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 0, 1917, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -40, 1950, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -16, 1924, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 0, 1693, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -16, 1706, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -64, 1699, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -40, 1465, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, -48, 1504, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, -48, 1654, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, -48, 1673, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, -32, 1791, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 0, 1697, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 32, 1467, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, -32, 799, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, -16, 785, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -160, 48, 866, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -136, -16, 810, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, 48, 864, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -16, 845, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 48, 861, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, -32, 1436, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, -32, 1514, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -32, 1722, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -112, 40, 969, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 80, 1084, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 104, 956, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -112, -16, 1446, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 16, 1197, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -16, 1515, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 1379, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 56, 1095, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -16, 1689, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, 16, 1409, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 1245, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, -56, 1691, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, -56, 1631, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018A4C8[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 7, 0, 0, { 3, 0 } },
    { 21, 4, 0, 0, { 2, 0 } },
    { 25, 1, 0, 0, { 4, 0 } },
    { 26, 38, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018A500[49] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -24, 2965, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -40, 1045, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, -88, 1093, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, -72, 1107, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -88, 2170, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, -96, 2174, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, -96, 2137, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, -96, 2093, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, -96, 2239, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 16, -64, 2297, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -96, 2037, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -40, 2003, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, -48, 2384, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -48, 2936, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -8, -88, 2379, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, 48, 913, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, -24, 982, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 104, -56, 1274, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 104, 32, 997, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, 24, 1051, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, -56, 1089, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 72, -64, 1116, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 72, 8, 1193, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, -64, 2661, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 24, -16, 1737, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, -16, 1554, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 40, -64, 2681, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -64, 2178, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 56, 0, 1301, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, -88, 1250, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, -120, 2187, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -96, 2175, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -104, -96, 2175, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -104, -48, 1919, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -64, -120, 453, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -96, -104, 447, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -120, -88, 445, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -72, 445, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -88, 444, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -120, 424, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -160, -32, 454, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -160, 24, 460, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, 72, 488, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -120, 1546, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -120, -96, 1575, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, -96, 1598, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -104, -16, 1713, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -112, -72, 1600, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 32 } }, -88, -56, 4500, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018A8D4[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { 30, 4, 0, 0, { 3, 0 } },
    { 34, 9, 0, 0, { 2, 0 } },
    { 43, 5, 0, 0, { 4, 0 } },
    { 48, 1, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018A90C[58] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -72, 2490, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -120, 1149, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -80, 1208, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -48, 1213, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -40, 963, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -32, 877, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -80, 1231, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -56, 1200, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -56, 1370, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 56, -120, 1764, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 1567, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 1726, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -72, 2111, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -16, 1314, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -24, 1302, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 80, 8, 1368, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 40, 1427, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 64, 1427, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 48, -40, 2005, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 0, 650, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 686, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 0, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 16, 552, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 16, 621, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 40, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 803, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 1070, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, 104, 836, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -120, 1134, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, -96, 1185, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 32, -120, 1736, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -96, 1800, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -40, 1300, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -80, 1200, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -80, 1432, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -32, 1464, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -32, 1325, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 8, 1243, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 8, 1213, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -120, -72, 1677, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 24, 1337, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, -24, 1737, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, -8, 1613, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 8, 1387, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -8, 1804, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 8, 1904, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 24, 1717, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 40, 1563, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 56, 1450, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, 72, 1293, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, 88, 1235, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -104, 24, 1292, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -112, 56, 1255, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -112, 80, 1209, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 56, 1237, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 80, 1188, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 56 } }, 8, -48, 3000, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 64, -16, 2500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018AD94[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 8, 0, 0, { 0, 0 } },
    { 27, 1, 0, 0, { 5, 0 } },
    { 28, 5, 0, 0, { 1, 0 } },
    { 33, 23, 0, 0, { 4, 0 } },
    { 56, 2, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018ADD4[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 0, 2417, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 1195, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -72, 1599, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -80, 1569, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -80, 1466, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, -32, 1502, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -8, 1601, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -8, 1299, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -8, 1469, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -32, 1641, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -32, 1698, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 96, 1187, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 88, 1235, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 80, 1287, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 72, 1343, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 64, 1406, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 56, 1471, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 80, 1255, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 72, 1320, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 64, 1394, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 56, 1458, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 16, 1445, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, 16, 1453, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 16, 1639, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 56, 1546, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -104, -80, 1535, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -104, -32, 1620, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -104, 24, 1731, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 40, 1654, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 32, 1696, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 24, 1829, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 16, 1985, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 8, 2198, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 8, 2416, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -80, 1694, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -72, -16, 1712, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -80, 1878, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -72, 1977, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -24, 2052, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -24, 2065, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -8, 2300, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018B108[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018B120[39] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -56, 2855, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -8, -64, 2755, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, -72, 2761, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, -72, 2137, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 96, 993, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 48, 995, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 8, 949, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -64, 896, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 64, 1307, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 56, 1408, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 48, 1520, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 32, 1690, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 16, -24, 2109, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, -24, 2185, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, -56, 2470, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 16, -56, 2041, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 16, 1921, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -8, 2127, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -56, 2105, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -24, 1939, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 24, 1573, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 32, 1484, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 40, 1404, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -16, 1225, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, -32, 1495, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 88, -32, 1835, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, -24, 1396, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, -16, 1314, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, -8, 1266, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -56, 2121, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -32, 1853, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -56, 2146, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -80, 1792, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -80, 1969, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -56, 1972, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, -56, 1943, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 2046, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -16, 1734, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 1555, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018B42C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 39, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018B444[27] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 16, 1874, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -32, 1803, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, -48, 1732, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, -56, 1739, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, -48, 1761, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -56, 1760, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -56, 1645, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -56, 1495, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -32, 1809, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 1896, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 40, 1407, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 120, 32, 1084, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 120, 88, 1119, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 1146, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 120, -32, 1091, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -32, 1018, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 32, 1109, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, -40, 1645, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -40, 1535, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -32, 1579, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -16, 1478, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 40, 1259, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -32, 1480, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -32, 1436, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -16, 1445, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -16, 1148, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 96, 40, 1116, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018B660[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018B678[11] = {
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -160, -120, 276, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -160, 0, 275, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -120, 72, 296, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, -64, 80, 340, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 0, 72, 375, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 40, 64, 434, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 72, 56, 487, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 48 } }, 88, -120, 638, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 96 } }, 104, -72, 630, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 96 } }, 104, 24, 623, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 0, -120, 558, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018B754[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018B76C[3] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, 80, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -112, 104, 375, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 112, 375, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018B7A8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B7C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B7D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B7E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B7F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B800[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B810[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B820[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B830[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018B840[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018B850[48] = {
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, -120, 250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, -104, 250, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -72, -120, 250, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -120, 250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -16, -120, 250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 72, -120, 250, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 1549, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -72, 1458, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -72, 1446, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 56, -40, 1541, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 16, 1549, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 16, 1532, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 32, 1460, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -120, 1339, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -104, 1383, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -64, 1157, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -24, 1399, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 0, 1529, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 1458, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -24, 1402, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 1375, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 0, 1470, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1296, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 0, 1379, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 24, 1280, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 56, 1105, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 56, 1096, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 48, 1131, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 32, 1361, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 48, 1275, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 32, 1113, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 32, 1127, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 32, 1105, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 32, 1101, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -48, 1189, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, -48, 1266, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, 8, 1076, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -24, 1170, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 8, 1077, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -104, 8, 1089, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -24, 1380, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -8, 1396, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 8, 1374, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1225, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -64, 1289, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -64, 1369, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -64, 1470, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -48, 1579, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018BC10[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 9, 0, 0, { 2, 0 } },
    { 15, 33, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018BC38[52] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -8, 1006, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, 80, 1067, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 1062, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 48, 1030, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 56, 1099, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -120, 1412, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -120, 1401, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 16, 1621, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 24, 1582, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 32, 1525, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -96, 1459, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -56, 1575, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -40, 1559, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -16, 1674, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -16, 1669, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -64, 1505, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -48, 1574, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -72, 1472, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -72, 1472, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 56, 96, 1071, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 96, 1052, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 48, 56, 1115, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, 48, 8, 1089, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, -8, 1282, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, -16, 1435, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, -24, 1480, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -8, 1537, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -24, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -104, 1394, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -104, 1180, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -104, 989, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -64, 1492, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -64, 1353, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -64, 1031, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 48, 1278, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 64, 1179, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 40, 1206, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 48, 1049, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 104, 32, 1217, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 104, 16, 1299, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 104, 0, 1341, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 104, -16, 1443, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 0, 1749, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 1557, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 32, 1408, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 1285, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 64, 1181, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 1114, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 48, 80, 1089, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 96, 80, 1085, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 0, 954, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 1074, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018C048[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 33, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018C068[59] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 421, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -80, 430, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -72, 437, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -120, -88, 440, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -96, -104, 441, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -64, -120, 449, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -32, 468, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 32, 459, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 485, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -144, -120, 937, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -144, -112, 966, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -96, -112, 982, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, -96, 998, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -144, -96, 966, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -136, -80, 982, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -64, 986, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, -40, 1039, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, -24, 1028, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, -8, 1045, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 1080, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, 24, 1108, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, 40, 1116, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 72, 1150, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 56, 1149, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, -112, 1187, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -88, 1187, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 1336, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -112, 910, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 8, 1795, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, -16, 1748, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 8, 1699, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 24, 1601, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -32, 1486, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -88, 1330, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -8, 1451, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 56, -8, 1217, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 72, -8, 1097, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 88, -8, 950, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -8, 1194, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 32, 1173, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 872, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 88, 80, 1048, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 80, 957, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, 64, 825, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 64, 933, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, 0, 1027, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, 0, 1140, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -64, 951, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -64, 1098, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -104, 995, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -56, 1183, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -56, 888, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -64, 1187, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -56, 1032, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, -56, 930, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 72, -96, 1342, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -104, 1241, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -104, 853, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 48 } }, -96, -40, 2750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018C504[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 15, 0, 0, { 3, 0 } },
    { 24, 2, 0, 0, { 2, 0 } },
    { 26, 32, 0, 0, { 4, 0 } },
    { 58, 1, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018C53C[47] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1099, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -104, 1100, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 112, -120, 1125, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 104, -48, 1173, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, 24, 1242, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 1271, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 72, 1274, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, 80, 1250, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 1195, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 80, 1266, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 96, 669, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 755, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 144 } }, -152, -64, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -160, -64, 1145, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 16, 1125, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 24, 1125, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -120, 40, 1049, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 40, 1060, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 56, 902, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 56, 943, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 56, 1004, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 56, 1062, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 56, 1173, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, 72, 855, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, 88, 759, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 88, 829, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 104, 1075, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 104, 1075, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 104, 1015, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 104, 1056, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 104, 1106, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 72, 872, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 930, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 88, 991, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 88, 991, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 88, 1012, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 88, 1089, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 88, 1118, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 88, 1162, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 88, 1225, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 72, 1083, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1080, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 72, 1105, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 1191, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 72, 1244, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 72, 80 } }, 32, -8, 2250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 120, 40, 1750, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018C8E8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 37, 0, 0, { 2, 0 } },
    { 45, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_mine_cavern_8018C910[48] = {
    { 143, 0x3FC0, { .fields = { 72, 80 } }, -8, -96, 2250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 1195, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 80, 1266, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 96, 669, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 755, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -160, -64, 1145, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 144 } }, -152, -64, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 16, 1125, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 24, 1125, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 40, 1060, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 56, 902, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, 72, 855, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, 88, 759, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 88, 829, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -120, 40, 1049, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 56, 943, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 56, 1004, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 56, 1062, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 56, 1173, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 72, 872, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 104, 1075, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 104, 1075, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 104, 1015, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 104, 1056, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 104, 1106, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 930, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 88, 991, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 72, 1083, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1080, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 72, 1105, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 1191, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 72, 1244, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 88, 991, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 88, 1012, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 88, 1089, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 88, 1118, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 88, 1162, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 88, 1225, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1099, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -104, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 112, -120, 1125, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 104, -48, 1173, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, 24, 1242, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 1271, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 72, 1274, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, 80, 1250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 72, 80 } }, 32, -8, 2250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 120, 40, 1750, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_mine_cavern_8018CCD0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 37, 0, 0, { 0, 0 } },
    { 38, 8, 0, 0, { 2, 0 } },
    { 46, 2, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_mine_cavern_8018CD00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_mine_cavern_8018CD10[25] = {
    { { .empty = D_mine_cavern_80189BC4 }, D_mine_cavern_80189BC4, NULL },
    { { .elements = D_mine_cavern_80189BD4 }, D_mine_cavern_80189FA8, NULL },
    { { .elements = D_mine_cavern_80189FC8 }, D_mine_cavern_8018A4C8, NULL },
    { { .elements = D_mine_cavern_8018A500 }, D_mine_cavern_8018A8D4, NULL },
    { { .elements = D_mine_cavern_8018A90C }, D_mine_cavern_8018AD94, NULL },
    { { .elements = D_mine_cavern_8018ADD4 }, D_mine_cavern_8018B108, NULL },
    { { .elements = D_mine_cavern_8018B120 }, D_mine_cavern_8018B42C, NULL },
    { { .elements = D_mine_cavern_8018B444 }, D_mine_cavern_8018B660, NULL },
    { { .elements = D_mine_cavern_8018B678 }, D_mine_cavern_8018B754, NULL },
    { { .elements = D_mine_cavern_8018B76C }, D_mine_cavern_8018B7A8, NULL },
    { { .empty = D_mine_cavern_8018B7C0 }, D_mine_cavern_8018B7C0, NULL },
    { { .empty = D_mine_cavern_8018B7D0 }, D_mine_cavern_8018B7D0, NULL },
    { { .empty = D_mine_cavern_8018B7E0 }, D_mine_cavern_8018B7E0, NULL },
    { { .empty = D_mine_cavern_8018B7F0 }, D_mine_cavern_8018B7F0, NULL },
    { { .empty = D_mine_cavern_8018B800 }, D_mine_cavern_8018B800, NULL },
    { { .empty = D_mine_cavern_8018B810 }, D_mine_cavern_8018B810, NULL },
    { { .empty = D_mine_cavern_8018B820 }, D_mine_cavern_8018B820, NULL },
    { { .empty = D_mine_cavern_8018B830 }, D_mine_cavern_8018B830, NULL },
    { { .empty = D_mine_cavern_8018B840 }, D_mine_cavern_8018B840, NULL },
    { { .elements = D_mine_cavern_8018B850 }, D_mine_cavern_8018BC10, NULL },
    { { .elements = D_mine_cavern_8018BC38 }, D_mine_cavern_8018C048, NULL },
    { { .elements = D_mine_cavern_8018C068 }, D_mine_cavern_8018C504, NULL },
    { { .elements = D_mine_cavern_8018C53C }, D_mine_cavern_8018C8E8, NULL },
    { { .elements = D_mine_cavern_8018C910 }, D_mine_cavern_8018CCD0, NULL },
    { { .elements = D_mine_cavern_8018A90C }, D_mine_cavern_8018AD94, NULL },
};

GpPointLight D_mine_cavern_8018CE3C[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8770, -2001, 8760 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3799, 3112, { 0, 0 } }, 0, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 240, -2001, 4380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3799, 3112, { 0, 0 } }, 0, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4560, -2001, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3799, 3112, { 0, 0 } }, 0, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36E2, -2001, 270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3799, 3112, { 0, 0 } }, 0, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4530, -2001, 270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3799, 3112, { 0, 0 } }, 0, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9080, -2001, 270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1324, 1594, 1648, { 0, 0 } }, 0, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3581, -2001, 8980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1324, 1495, 1416, { 0, 0 } }, 0, 5261 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3C5B, -2001, 8800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1324, 1676, 1723, { 0, 0 } }, 0, 4299 },
};

GpRoomCoordSet D_mine_cavern_8018D13C[1] = {
    { 0, NULL, 8, D_mine_cavern_8018CE3C, 0, NULL },
};

GpObj4C D_mine_cavern_8018D154[20] = {
    { NULL, NULL, NULL, { 1520, -2688, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 1377, -2496, 6754, 0 }, { { 2656, -3456, -1408, 0 }, { -2656, -3456, 1408, 0 }, { 2656, 3456, -1408, 0 }, { -2656, 3456, 1408, 0 } }, { 1920, 0, 3621, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 7890, -2528, 7376, 0 }, { { 144, -3456, 2947, 0 }, { -147, -3456, -2950, 0 }, { 144, 3456, 2947, 0 }, { -147, 3456, -2950, 0 } }, { -4105, 0, 202, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 7376, -2528, 7407, 0 }, { { -560, -3456, -2880, 0 }, { 560, -3456, 2880, 0 }, { -560, 3456, -2880, 0 }, { 560, 3456, 2880, 0 } }, { 4030, 0, -784, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 0x41B0, -2464, 6368, 0 }, { { -2640, -3456, -393, 0 }, { 2605, -3456, 379, 0 }, { -2640, 3456, -393, 0 }, { 2605, 3456, 379, 0 } }, { 597, 0, -4061, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x4210, -2529, 6176, 0 }, { { 3083, -3456, 279, 0 }, { -3125, -3456, -286, 0 }, { 3083, 3456, 279, 0 }, { -3125, 3456, -286, 0 } }, { -373, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 8, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x41D1, -2528, 2944, 0 }, { { -2555, -3456, 655, 0 }, { 2554, -3456, -720, 0 }, { -2555, 3456, 655, 0 }, { 2554, 3456, -720, 0 } }, { -1065, 0, -3956, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 20, 1, 0 },
    { NULL, NULL, NULL, { 0x40A1, -2817, 2720, 0 }, { { 2333, -3456, -816, 0 }, { -2346, -3456, 754, 0 }, { 2333, 3456, -816, 0 }, { -2346, 3456, 754, 0 } }, { 1304, 0, 3886, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 1568, -2656, 3184, 0 }, { { -2723, -3456, -593, 0 }, { 2711, -3456, 583, 0 }, { -2723, 3456, -593, 0 }, { 2711, 3456, 583, 0 } }, { 870, 0, -4026, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 22, 21, 1, 0 },
    { NULL, NULL, NULL, { 1520, -2656, 2896, 0 }, { { 2712, -3456, 458, 0 }, { -2732, -3456, -469, 0 }, { 2712, 3456, 458, 0 }, { -2732, 3456, -469, 0 } }, { -692, 0, 4057, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 22, 1, 0 },
    { NULL, NULL, NULL, { 0x2CC0, -2561, 1184, 0 }, { { -16, -3456, 2640, 0 }, { 16, -3456, -2640, 0 }, { -16, 3456, 2640, 0 }, { 16, 3456, -2640, 0 } }, { -4115, 0, -25, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2A81, -2497, 1072, 0 }, { { 0, -3456, -2672, 0 }, { 1, -3456, 2672, 0 }, { 0, 3456, -2672, 0 }, { 1, 3456, 2672, 0 } }, { 4110, 0, -1, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x40BE, -2656, 2653, 0 }, { { 2335, -3456, -804, 0 }, { -2352, -3456, 764, 0 }, { 2335, 3456, -804, 0 }, { -2352, 3456, 764, 0 } }, { 1302, 0, 3893, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 20, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x4240, -2464, 6463, 0 }, { { -2470, -3456, -299, 0 }, { 2437, -3456, 288, 0 }, { -2470, 3456, -299, 0 }, { 2437, 3456, 288, 0 } }, { 487, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 20, 8, 1, 0 },
    { NULL, NULL, NULL, { 2176, -2432, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 21, 6, 1, 0 },
    { NULL, NULL, NULL, { 1535, -2400, 2847, 0 }, { { 2824, -3456, 537, 0 }, { -2853, -3456, -561, 0 }, { 2824, 3456, 537, 0 }, { -2853, 3456, -561, 0 } }, { -782, 0, 4038, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 21, 22, 1, 0 },
    { NULL, NULL, NULL, { 0x2FDF, -2496, 7423, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x2DFE, -2560, 7486, 0 }, { { -441, -3456, -2860, 0 }, { 436, -3456, 2855, 0 }, { -441, 3456, -2860, 0 }, { 436, 3456, 2855, 0 } }, { 4066, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 6079, -2464, 1343, 0 }, { { -439, -3456, -2859, 0 }, { 438, -3456, 2858, 0 }, { -439, 3456, -2859, 0 }, { 438, 3456, 2858, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 22, 1, 0 },
    { NULL, NULL, NULL, { 6559, -2560, 1343, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 4, 129, 0 },
};

GpObj4C D_mine_cavern_8018D744[18] = {
    { NULL, NULL, NULL, { 1520, -2688, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 1377, -2496, 6754, 0 }, { { 2656, -3456, -1408, 0 }, { -2656, -3456, 1408, 0 }, { 2656, 3456, -1408, 0 }, { -2656, 3456, 1408, 0 } }, { 1920, 0, 3621, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 7890, -2528, 7376, 0 }, { { 144, -3456, 2947, 0 }, { -147, -3456, -2950, 0 }, { 144, 3456, 2947, 0 }, { -147, 3456, -2950, 0 } }, { -4105, 0, 202, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 7376, -2528, 7407, 0 }, { { -560, -3456, -2880, 0 }, { 560, -3456, 2880, 0 }, { -560, 3456, -2880, 0 }, { 560, 3456, 2880, 0 } }, { 4030, 0, -784, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 0x4210, -2529, 6176, 0 }, { { 3083, -3456, 279, 0 }, { -3125, -3456, -286, 0 }, { 3083, 3456, 279, 0 }, { -3125, 3456, -286, 0 } }, { -373, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 8, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x41D1, -2528, 2944, 0 }, { { -2555, -3456, 655, 0 }, { 2554, -3456, -720, 0 }, { -2555, 3456, 655, 0 }, { 2554, 3456, -720, 0 } }, { -1065, 0, -3956, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x40A1, -2817, 2720, 0 }, { { 2333, -3456, -816, 0 }, { -2346, -3456, 754, 0 }, { 2333, 3456, -816, 0 }, { -2346, 3456, 754, 0 } }, { 1304, 0, 3886, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1568, -2656, 3184, 0 }, { { -2723, -3456, -593, 0 }, { 2711, -3456, 583, 0 }, { -2723, 3456, -593, 0 }, { 2711, 3456, 583, 0 } }, { 870, 0, -4026, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 23, 5, 1, 0 },
    { NULL, NULL, NULL, { 1520, -2656, 2896, 0 }, { { 2712, -3456, 458, 0 }, { -2732, -3456, -469, 0 }, { 2712, 3456, 458, 0 }, { -2732, 3456, -469, 0 } }, { -692, 0, 4057, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 23, 1, 0 },
    { NULL, NULL, NULL, { 0x2CC0, -2561, 1184, 0 }, { { -16, -3456, 2640, 0 }, { 16, -3456, -2640, 0 }, { -16, 3456, 2640, 0 }, { 16, 3456, -2640, 0 } }, { -4115, 0, -25, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x2A81, -2497, 1072, 0 }, { { 0, -3456, -2672, 0 }, { 1, -3456, 2672, 0 }, { 0, 3456, -2672, 0 }, { 1, 3456, 2672, 0 } }, { 4110, 0, -1, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x4240, -2464, 6463, 0 }, { { -2470, -3456, -299, 0 }, { 2437, -3456, 288, 0 }, { -2470, 3456, -299, 0 }, { 2437, 3456, 288, 0 } }, { 487, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x2FDF, -2496, 7423, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x2DFE, -2560, 7486, 0 }, { { -441, -3456, -2860, 0 }, { 436, -3456, 2855, 0 }, { -441, 3456, -2860, 0 }, { 436, 3456, 2855, 0 } }, { 4066, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 6079, -2464, 1343, 0 }, { { -439, -3456, -2859, 0 }, { 438, -3456, 2858, 0 }, { -439, 3456, -2859, 0 }, { 438, 3456, 2858, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 22, 1, 0 },
    { NULL, NULL, NULL, { 6559, -2560, 1343, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 4, 1, 0 },
    { NULL, NULL, NULL, { 2815, -2368, 1055, 0 }, { { -438, -3456, -2858, 0 }, { 439, -3456, 2859, 0 }, { -438, 3456, -2858, 0 }, { 439, 3456, 2859, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 23, 1, 0 },
    { NULL, NULL, NULL, { 3263, -2464, 1151, 0 }, { { 298, -3456, 2877, 0 }, { -298, -3456, -2877, 0 }, { 298, 3456, 2877, 0 }, { -298, 3456, -2877, 0 } }, { -4095, 0, 423, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 23, 22, 129, 0 },
};

GpObj4C D_mine_cavern_8018DC9C[13] = {
    { NULL, NULL, NULL, { 0x4740, -48, 1888, 0 }, { { -544, 0, -1824, 0 }, { 544, 0, -1824, 0 }, { -544, 0, 1824, 0 }, { 544, 0, 1824, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1902, 0, 5, 19, 2, 0 },
    { NULL, NULL, NULL, { 1792, -48, -352, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1159, 0, 8, 33, 2, 0 },
    { NULL, NULL, NULL, { 896, -64, 1440, 0 }, { { -1056, 0, -864, 0 }, { 1088, 0, -864, 0 }, { -1056, 0, 1920, 0 }, { 1024, 0, -192, 0 } }, { 0, 4104, 0, 0 }, { 2275, 0, 3405, 0 }, 2187, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 6880, -64, 832, 0 }, { { -1344, 0, -864, 0 }, { 1344, 0, -864, 0 }, { -1344, 0, 864, 0 }, { 1344, 0, 864, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1593, 0x4005, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 1360, -64, 32, 0 }, { { -560, 0, -1216, 0 }, { 592, 0, -1216, 0 }, { -560, 0, 1216, 0 }, { 528, 0, 1216, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1348, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 4512, -64, 224, 0 }, { { -1514, 0, -997, 0 }, { 166, 0, -99, 0 }, { -455, 0, 1954, 0 }, { 2505, 0, -316, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 2521, 2, 8, 255, 4, 0 },
    { NULL, NULL, NULL, { 4320, -64, 8864, 0 }, { { 1906, 0, 290, 0 }, { -66, 0, 293, 0 }, { -19, 0, -1958, 0 }, { -1910, 0, 269, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1958, 2, 15, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x3580, -64, 8832, 0 }, { { 1906, 0, 226, 0 }, { -66, 0, 229, 0 }, { -19, 0, -1670, 0 }, { -1846, 0, 173, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1915, 2, 16, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x3546, -64, 356, 0 }, { { -1912, 0, -262, 0 }, { 60, 0, -265, 0 }, { 13, 0, 1570, 0 }, { 1840, 0, -305, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1928, 2, 14, 255, 4, 0 },
    { NULL, NULL, NULL, { 8848, -64, 5312, 0 }, { { -2576, 0, -448, 0 }, { 2576, 0, -448, 0 }, { -2576, 0, 448, 0 }, { 2576, 0, 448, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 2610, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 9200, -64, 2432, 0 }, { { -5504, 0, -448, 0 }, { 5504, 0, -448, 0 }, { -5504, 0, 448, 0 }, { 5504, 0, 448, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 5514, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 4784, -64, 6048, 0 }, { { -1472, 0, -448, 0 }, { 1472, 0, -448, 0 }, { -1472, 0, 448, 0 }, { 1472, 0, 448, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1536, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x3300, -64, 6112, 0 }, { { -1472, 0, -448, 0 }, { 1472, 0, -448, 0 }, { -1472, 0, 448, 0 }, { 1472, 0, 448, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1536, 2, 12, 0, 130, 0 },
};

GpObj3A D_mine_cavern_8018E078[2] = {
    { NULL, NULL, { 9216, -2384, 4448, 0 }, { { -5856, -3408, -1088, 0 }, { 5856, -3408, 1088, 0 }, { -5856, 3408, -1088, 0 }, { 5856, 3408, 1088, 0 } }, { 750, 0, -4041, 0 }, { -62, 26 }, 1, 0 },
    { NULL, NULL, { 9183, -2320, 4399, 0 }, { { -5890, -3344, 1060, 0 }, { 5891, -3344, -1059, 0 }, { -5890, 3344, 1060, 0 }, { 5891, 3344, -1059, 0 } }, { -726, 0, -4034, 0 }, { -62, 26 }, 129, 0 },
};

GpAreaTmdRec D_mine_cavern_8018E0F0[2] = {
    { 30, 30, 3, 0, { 0, 0 }, D_80158D58 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_mine_cavern_8018E108[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_mine_cavern_8018E120[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_mine_cavern_8018E138[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_mine_cavern_8018E150[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_mine_cavern_8018E168[2] = {
    { 30, 0, 0, 2000, 0, 3960, 3072, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_mine_cavern_8018E188[3] = {
    { 6, 0, 1, 0x2C24, 0, 1750, 900, 0, 0, 2, 0 },
    { 6, 0, 1, 6700, 0, 1500, 3000, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_mine_cavern_8018E1B8[3] = {
    { 6, 0, 1, 2272, 0, 1792, 1536, 0, 0, 2, 0 },
    { 6, 0, 1, 0x2920, 0, 7296, 0, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_mine_cavern_8018E1E8[3] = {
    { 3, 0, 0, 5500, 0, 1500, 1024, 0, 0, 2, 0 },
    { 3, 0, 1, 8500, 0, 7500, 2048, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_mine_cavern_8018E218[2] = {
    { 22, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_mine_cavern_8018E238[22] = {
    { NULL, NULL },
    { D_mine_cavern_8018E168, D_mine_cavern_8018E0F0 },
    { D_mine_cavern_8018E188, D_mine_cavern_8018E108 },
    { D_mine_cavern_8018E1B8, D_mine_cavern_8018E120 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_cavern_8018E1E8, D_mine_cavern_8018E138 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_cavern_8018E218, D_mine_cavern_8018E150 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_mine_cavern_8018E2E8[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

GpRoomParamRec D_mine_cavern_8018E2F4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_mine_cavern_8018E2FC[1] = {
    { 0, 0, 1, 0, D_mine_cavern_8018E2E8 },
};

GpRoomParamRec D_mine_cavern_8018E304[1] = {
    { 0, 1, 1, 0, D_mine_cavern_8018E2E8 },
};

GpRoomParamRec* D_mine_cavern_8018E30C[8] = {
    D_mine_cavern_8018E2F4,
    D_mine_cavern_8018E2FC,
    D_mine_cavern_8018E304,
    D_mine_cavern_8018E2F4,
    D_mine_cavern_8018E2F4,
    D_mine_cavern_8018E2F4,
    D_mine_cavern_8018E2F4,
    D_mine_cavern_8018E2F4,
};

GpAreaApplyRec D_mine_cavern_8018E32C[9] = {
    { 4, 1, 2, 1 },
    { 4, 2, 4, 0 },
    { 4, 3, 4, 1 },
    { 4, 4, 4, 1 },
    { 4, 5, 2, 1 },
    { 4, 7, 2, 1 },
    { 4, 11, 11, 33 },
    { 4, 15, 11, 33 },
    { 255, 0, 0, 0 },
};

MineCavernGlowPalette D_mine_cavern_8018E350 = { { 48, 32, 0 }, { 0, 0, 0 }, 1128 };

MineCavernGlowPalette D_mine_cavern_8018E358 = { { 42, 25, 0 }, { 0, 0, 0 }, 1960 };

static void func_mine_cavern_8017F7D0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_mine_cavern_8017FBF4(GpCoord* arg0, s16 arg1, u8* rgb);
static void func_mine_cavern_80181CAC(s16 point);
static void func_mine_cavern_80181D80(s16 point);
static void func_mine_cavern_80182E34(GpEnemy* arg0, Task* arg1);
static void func_mine_cavern_801830F0(GpEnemy* arg0, Task* arg1);
static void func_mine_cavern_801836D0(GpEnemy* arg0, Task* arg1);
static void func_mine_cavern_80183890(GpEnemy* enemy, Task* task);

void func_mine_cavern_8017E330(void)
{
    Mc_SaveData[0].state.at4.loc.room = 2;
    gGameSession->at4.loc.room        = 2;
    gGameSession->roomObjsDirty       = 1;
}

void func_mine_cavern_8017E358(void)
{
}

void func_mine_cavern_8017E360(void)
{
    gGameSession->at4.loc.place     = 4;
    Gp_StateF0.prefix.bytes.field_0 = 0;
    Gp_StateF0.field_5              = 0;
    Gp_StateF0.field_6              = 0;
    Gp_StateF0.field_8              = 0;
    Gp_StateF0.field_C              = 0;
    Gp_StateF0.field_10             = 0;
}

void func_mine_cavern_8017E394(void)
{
    D_mine_cavern_8018EB54 = 0;
}

void func_mine_cavern_8017E3A0(s32 arg0)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    s32        v;

    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    v    = arg0 & 0xFF;

    if (v == 1) {
        rec[3].field_4[5].field_4  = v;
        rec[4].field_4[6].field_4  = v;
        rec[21].field_4[5].field_4 = v;
        rec[22].field_4[3].field_4 = v;
        rec[23].field_4[4].field_4 = v;
        return;
    }
    if (v == 0) {
        rec[3].field_4[5].field_4  = 0;
        rec[4].field_4[6].field_4  = 0;
        rec[21].field_4[5].field_4 = 0;
        rec[22].field_4[3].field_4 = 0;
        rec[23].field_4[4].field_4 = 0;
    }
}

void func_mine_cavern_8017E474(Task* arg0)
{
    u32 rnd;

    if (arg0->state == 0) {
        D_80115728                 = 0x60244;
        D_80115744                 = 0x60250;
        D_8011573C                 = 0x6023F;
        D_80115720                 = 0x60267;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }

    if (GameFlag_GetNibble(0xC4) == 1) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 7) == 0) {
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x1766;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vy = ((Gp_LcgState >> 16) & 0x3F) - 0x5B4;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vz = ((Gp_LcgState >> 16) & 0x3F) - 0x14A;
            Gp_SpawnEff(0x600E0, NULL, 0x300, &D_mine_cavern_80188FBC);
        }
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3:
        case 9: {
            SVECTOR* p = D_mine_cavern_80188F84;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[5], 1, 0x300);
            break;
        }
        case 5:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
        case 23: {
            SVECTOR* p = D_mine_cavern_80188F64;
            func_mine_cavern_8017E774(&p[0], 0x180, 0x222);
            func_mine_cavern_8017E774(&p[1], 0x180, 0x222);
            func_mine_cavern_8017EFB8(&p[3], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_cavern_80188F8C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[4], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_mine_cavern_80188F84;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[3], 1, 0x300);
            break;
        }
        case 8:
        case 20:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F94, 1, 0x300);
            break;
        case 10:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188FB4, 1, 0x300);
            break;
        case 11:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
        case 13: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[5], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[6], 1, 0x300);
            break;
        }
        case 14:
        case 16:
        case 21:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F8C, 1, 0x300);
            break;
        case 17:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F9C, 1, 0x300);
            break;
        case 24:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188FC4, 1, 0x300);
        case 22:
            func_mine_cavern_8017EFB8(D_mine_cavern_80188F7C, 1, 0x300);
            break;
        case 25: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            func_mine_cavern_8017EFB8(&p[0], 1, 0x300);
            func_mine_cavern_8017EFB8(&p[2], 1, 0x300);
            break;
        }
    }
}

/// Draws a glow joining the two points `arg0[0]` and `arg0[1]`, projected
/// through `gGfxViewCoord.workm`; nothing is drawn unless both project. `arg1` is
/// the half-extent, scaled by each end's depth. Starting from the screen-space
/// angle between the ends, for each 0x400 step over half a turn it queues three
/// gouraud `POLY_G4`s: a wedge of each end and the band between them. The lit
/// vertices take the colour packed in `arg2`, four bits per channel (R, G, B
/// from high to low nibble), with the frame counter's low bit as a flicker.
static void func_mine_cavern_8017E774(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels.
static void func_mine_cavern_8017EFB8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Frame callback of a drifting mote. Setup reads speed, lifetime and drawing
/// flags out of `Task::spawnArg1`; the mote then rises or falls one step a
/// frame and is drawn with `func_mine_cavern_8017F50C` every other tick. State
/// 1 brightens while young, state 2 holds its brightness; both fade over their
/// last eight ticks of lifetime and release the work block once dark, or as
/// soon as the room's event state reaches 4.
void func_mine_cavern_8017F240(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_mine_cavern_8017F50C(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_mine_cavern_8017F50C(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_mine_cavern_8017F50C(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
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
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
static void func_mine_cavern_8017F7D0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   outer;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
static void func_mine_cavern_8017FBF4(GpCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
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
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Frame callback of an expanding halo. State 0 parks the coordinate on its
/// parent at the spawn position and derives the ramp step from the spawn
/// argument; state 1 grows the halo's brightness and size, drawing the
/// `func_mine_cavern_8017FBF4` wedge ring (with a half-bright echo on odd
/// ticks) and a `func_mine_cavern_8017F7D0` ring; state 2 fades it out through
/// `func_mine_cavern_80180D70` and then releases the work block. The shade row
/// `D_mine_cavern_80188FCC.entries[index]` tints each channel.
void func_mine_cavern_8017FF88(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift                 = ((GpEffSpawnArg*)&arg0->spawnArg1.value)->field_2;
                mem->index            = shift;
                arg0->spawnArg1.value = ((GpEffSpawnArg*)&arg0->spawnArg1.value)->field_0;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].r;
                rgb[1]                 = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].g;
                rgb[2]                 = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].b;
                func_mine_cavern_8017FBF4(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_mine_cavern_8017FBF4(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_mine_cavern_8017F7D0(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].r;
                    rgb[1] = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].g;
                    rgb[2] = mem->scale >> D_mine_cavern_80188FCC.entries[mem->index].b;
                    func_mine_cavern_80180D70(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Frame callback for one of the cavern's expanding-ring effects. `Gp_State1C`'s
/// `field_4` gates the whole room-effect family: 1-3 park the effect for the
/// frame and 4 or more tear its work block down, so a task that sees them either
/// returns or releases. Otherwise the effect ticks its lifetime counter, stages
/// `scale` into an RGB triple, advances the coordinate, and draws the
/// eight-wedge `func_mine_cavern_8017FBF4` ring at twice `angle` plus the cavern's own
/// glow quads at half-extent `angle`. Once `period` reaches 0x19 the
/// two ramps swap roles - a `func_mine_cavern_8017F7D0` ring is drawn at `step * 3 / 2` and
/// then `period` shrinks by 0x18 and `step` grows by 0x30 - and the effect
/// otherwise fades `scale` by 0x18 a frame until it drops under 0x18 and the
/// work block is handed back with `Gp_ReleaseState1CMem`.
void func_mine_cavern_80180320(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         sp10[3];
    u16        temp;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age    = 1;
            work->scale  = 0xE0;
            work->angle  = 0x80;
            work->period = 0xE0;
            work->step   = 0x80;
            task->state  = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]     = (u8)work->scale;
        sp10[1]     = (u8)(work->scale >> 1);
        sp10[2]     = (u8)(work->scale >> 2);
        temp        = work->angle + 0x10;
        work->angle = temp;
        func_mine_cavern_8017FBF4(coord, (s16)(temp * 2), sp10);
        func_mine_cavern_801804CC(coord, work->angle);
        if (work->period >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->period;
            sp10[1] = (u8)(work->period >> 1);
            sp10[2] = (u8)(work->period >> 2);
            temp_a1 = work->step * 3;
            func_mine_cavern_8017F7D0(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->period -= 0x18;
            work->step   += 0x30;
            return;
        }
        temp        = work->scale - 0x18;
        work->scale = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_mine_cavern_801804CC(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = shifted >> 0x11;
    light->head.b               = shifted >> 0x12;
    light->head.u.at.local.t[0] = coord->coord.t[0];
    light->head.u.at.local.t[1] = coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    Gp_LcgState                 = random;
    block                       = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx               = coord->workm.t[0];
    block->vec.vy               = coord->workm.t[1];
    block->vec.vz               = coord->workm.t[2];
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
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_mine_cavern_801809F8(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_mine_cavern_801809F8(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
static void func_mine_cavern_80180D70(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
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
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Frame callback of a rising spark: each tick walks its angle on by a random
/// 0x200..0x3FF, sets its velocity to a 3/16-scaled unit circle in X and Z and
/// an upward Y that grows with age, and spawns the `D_80115728` effect at the
/// task's coordinate. Releases the work block after 0x15 ticks, or as soon as
/// the room's event state reaches 4.
void func_mine_cavern_80181730(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}

/// Draws a glow at each of the six points of `D_mine_cavern_8018E36C`, the
/// fourth skipped while view 4 is active: per point, a fan of eight
/// semi-transparent Gouraud triangles around its projected position, each
/// followed by a drawing-mode packet, both linked at the point's depth. The
/// radius is scaled by depth and jittered by the shared LCG, and its base
/// shrinks as more `GameFlag_GetNibble(0xE2)` bits are set. A point whose
/// projection flags an error is skipped.
static void func_mine_cavern_80181864(void)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    s32       flags;
    u8        count;
    s32       j;
    s32       base;
    u16       view;
    s32       size;
    s32       shift;
    u16       x;
    u16       y;

    flags = GameFlag_GetNibble(0xE2);
    view  = Gp_GetViewIndex() & 0xFF;
    count = 0;
    for (j = 0; j < 4; j++) {
        if ((flags >> j) & 1) {
            count++;
        }
    }
    switch (count) {
        case 0:
        case 1:
            base = 0x428;
            break;
        case 2:
            base = 0x3C0;
            break;
        case 3:
            base = 0xC8;
            break;
        case 4:
        default:
            base = 0x80;
            break;
    }
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    size = base;
    for (j = 0; j < 6; j++) {
        shift = 12; // fraction bits of rsin/rcos
        if (j == 3 && view == 4) {
            continue;
        }
        gte_ldv0(&D_mine_cavern_8018E36C[j]);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        if (flag < 0) {
            continue;
        }
        x           = sxy;
        y           = sxy >> 16;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        radius      = (s32)(size + ((Gp_LcgState >> 16) & 0xF)) * 0x160 / (otz * 4);
        /* Each packet is written as a POLY_G3 and a DR_TPAGE, but the original
           reserves a POLY_GT3 and a DR_MODE for them, so the cursor steps by
           the larger types. */
        for (i = 0; i < 8; i++) {
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = (POLY_GT3*)prim + 1;
            setPolyG3(prim);
            prim->r0 = MineCavernGlowByte8018E350[0];
            prim->g0 = MineCavernGlowByte8018E351[0];
            prim->b0 = MineCavernGlowByte8018E352[0];
            prim->x0 = x;
            prim->y0 = y;
            prim->r1 = MineCavernGlowByte8018E353[0];
            prim->g1 = MineCavernGlowByte8018E354[0];
            prim->b1 = MineCavernGlowByte8018E355[0];
            prim->r2 = MineCavernGlowByte8018E353[0];
            prim->g2 = MineCavernGlowByte8018E354[0];
            prim->b2 = MineCavernGlowByte8018E355[0];
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> shift);
            prim->y1 = y + ((rcos(i << 9) * radius) >> shift);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> shift);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> shift);
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = (DR_MODE*)dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[otz >> 4], dr);
        }
    }
}

/// Switches on transient light slot `4 + point` for cavern point `point`: fills
/// it from the cavern's light parameters and the point's position in
/// `D_mine_cavern_8018E39C`, with the outer radius jittered by a draw from the
/// shared LCG.
static void func_mine_cavern_80181CAC(s16 point)
{
    GpCoord64*    light = &Gp_RoomCoords[4 + point];
    GpPointLight* work  = &light->data.light;

    light->framesLeft          = 2;
    work->inner                = D_mine_cavern_8018E366;
    work->outer                = D_mine_cavern_8018E368 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x7FF);
    work->head.r               = D_mine_cavern_8018E360;
    work->head.g               = D_mine_cavern_8018E362;
    work->head.b               = D_mine_cavern_8018E364;
    work->head.u.at.local.t[0] = D_mine_cavern_8018E39C[point].vx;
    work->head.u.at.local.t[1] = D_mine_cavern_8018E39C[point].vy;
    work->head.u.at.local.t[2] = D_mine_cavern_8018E39C[point].vz;
    light->data.coord.flg      = 0;
}

/// Draws a glow at cavern point `point` of `D_mine_cavern_8018E39C`: a fan of
/// eight semi-transparent Gouraud triangles around the point's projected
/// position, each followed by a drawing-mode packet, both linked at the point's
/// depth. The radius is scaled by depth and jittered by the shared LCG, and its
/// base shrinks as more `GameFlag_GetNibble(0xE2)` bits are set. Nothing is
/// drawn when the projection flags an error.
static void func_mine_cavern_80181D80(s16 point)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    s32       flags;
    u8        count;
    s32       j;
    s32       base;
    u16       x;
    u16       y;

    flags = GameFlag_GetNibble(0xE2);
    count = 0;
    for (j = 0; j < 4; j++) {
        if ((flags >> j) & 1) {
            count++;
        }
    }
    switch (count) {
        case 0:
        case 1:
            base = 0x780;
            break;
        case 2:
            base = 0x500;
            break;
        case 3:
            base = 0x280;
            break;
        case 4:
        default:
            base = 0x200;
            break;
    }
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&D_mine_cavern_8018E39C[point]);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stflg(&flag);
    gte_stszotz(&otz);
    if (flag >= 0) {
        x           = sxy;
        y           = sxy >> 16;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        radius      = (s32)(base | ((Gp_LcgState >> 16) & 0x7F)) * 0x160 / (otz * 4);
        /* Each packet is written as a POLY_G3 and a DR_TPAGE, but the original
           reserves a POLY_GT3 and a DR_MODE for them, so the cursor steps by
           the larger types. */
        for (i = 0; i < 8; i++) {
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = (POLY_GT3*)prim + 1;
            setPolyG3(prim);
            prim->r0 = MineCavernGlowByte8018E358[0];
            prim->g0 = MineCavernGlowByte8018E359[0];
            prim->b0 = MineCavernGlowByte8018E35A[0];
            prim->x0 = x;
            prim->y0 = y;
            prim->r1 = MineCavernGlowByte8018E35B[0];
            prim->g1 = MineCavernGlowByte8018E35C[0];
            prim->b1 = MineCavernGlowByte8018E35D[0];
            prim->r2 = MineCavernGlowByte8018E35B[0];
            prim->g2 = MineCavernGlowByte8018E35C[0];
            prim->b2 = MineCavernGlowByte8018E35D[0];
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> 12);
            prim->y1 = y + ((rcos(i << 9) * radius) >> 12);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> 12);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> 12);
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = (DR_MODE*)dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[otz >> 4], dr);
        }
    }
}

/// Runs the cavern's four emitter points while `GameFlag_GetNibble(0x7A)` is
/// below 5. Each point whose bit is set in `GameFlag_GetNibble(0xE2)` has its
/// light refreshed, and its sound restarted when the view has just been set up
/// or the enabled set changed since the last run. A point listed for the
/// current view in `D_mine_cavern_8018E3BC` also runs
/// `func_mine_cavern_80181D80`, and on every ninth tick or on entering the view
/// spawns effect `0x60080` within 64 units of the point on each axis, unless
/// `Gp_StateF0.field_4` is set.
static void func_mine_cavern_80182184(void)
{
    VECTOR   unused;
    GpCoord  coord;
    MATRIX*  m;
    SVECTOR* pos;
    s32      view;
    s32      flags;
    s16      i;
    s16      j;
    s16      k;

    view  = Gp_GetViewIndex() & 0xFF;
    flags = GameFlag_GetNibble(0xE2);
    for (i = 0; i < 4 && GameFlag_GetNibble(0x7A) < 5; i++) {
        if (!((flags >> i) & 1)) {
            continue;
        }
        func_mine_cavern_80181CAC(i);
        if (gGameSession->viewReady == 1 || D_mine_cavern_8018EB58 != flags) {
            func_mine_cavern_801825C8(i);
        }
        for (j = 0; j < 8 && D_mine_cavern_8018E3BC[i][j] != 0; j++) {
            k = D_mine_cavern_8018E3BC[i][j];
            if (k != (u8)view) {
                continue;
            }
            func_mine_cavern_80181D80(i);
            if ((s16)((s16)D_mine_cavern_8018EB5C % 9) != 0 && D_mine_cavern_8018E3DC == k) {
                continue;
            }
            if (Gp_StateF0.field_4 != 0) {
                continue;
            }
            m                               = &coord.coord;
            MATRIX_PAIR(&coord.coord, 0, 0) = 0x1000;
            MATRIX_PAIR(&coord.coord, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1)            = 0x1000;
            MATRIX_PAIR(&coord.coord, 2, 0) = 0;
            m->m[2][2]                      = 0x1000;
            coord.sub                       = &gGfxViewCoord;
            pos                             = &D_mine_cavern_8018E39C[i];
            coord.coord.t[0]                = pos->vx + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.coord.t[1]                = pos->vy + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.coord.t[2]                = pos->vz + ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 0x7F) - 0x40;
            coord.flg                       = 0;
            Gp_SpawnEff(0x60080, &coord, 0x800004FF, NULL);
        }
    }
    if (Gp_StateF0.field_4 == 0) {
        D_mine_cavern_8018EB5C++;
    }
    D_mine_cavern_8018E3DC = view;
    D_mine_cavern_8018EB58 = flags;
}

/// Queues the cavern's darkness overlay: a semi-transparent flat quad filling
/// the screen with the tint `D_mine_cavern_8018E3E0` holds for the number of
/// `GameFlag_GetNibble(0xE2)` bits set, followed by the drawing-mode packet
/// that restores the room's texture page (`0xE100004A`). Both go into the head
/// of the current OT, and the cavern's own two passes are run afterwards.
static void func_mine_cavern_80182454(void)
{
    POLY_F4* poly;
    DR_MODE* dr;
    s32      flags;
    s16      i;
    s16      count;

    flags = GameFlag_GetNibble(0xE2);
    count = 0;

    poly           = (POLY_F4*)gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 5);
    setcode(poly, 0x2A);

    for (i = 0; i < 4; i++) {
        if ((flags >> i) & 1) {
            count++;
        }
    }

    poly->r0 = D_mine_cavern_8018E3E0[count].r;
    poly->g0 = D_mine_cavern_8018E3E0[count].g;
    poly->b0 = D_mine_cavern_8018E3E0[count].b;

    poly->x0 = -0xA0;
    poly->y0 = -0x78;
    poly->x1 = 0xA0;
    poly->y1 = -0x78;
    poly->x2 = -0xA0;
    poly->y2 = 0x78;
    poly->x3 = 0xA0;
    poly->y3 = 0x78;
    addPrim(gGpuCurrentOt, poly);

    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100004A;
    addPrim(gGpuCurrentOt, dr);

    func_mine_cavern_80181864();
    func_mine_cavern_80182184();
}

/// The mine task's state handlers, run by `func_mine_cavern_80182DC8`.
static const TaskFuncTable3 D_mine_cavern_8017D65C = {
    { func_mine_cavern_80182CEC, func_mine_cavern_80182DA8, taskKill },
};

static void func_mine_cavern_801825C8(s16 arg0)
{
    GpCoord coord;
    s32     view;

    view             = Gp_GetViewIndex() & 0xFF;
    coord.sub        = &gGfxViewCoord;
    coord.coord.t[0] = D_mine_cavern_8018E39C[arg0].vx;
    coord.coord.t[1] = D_mine_cavern_8018E39C[arg0].vy;
    coord.coord.t[2] = D_mine_cavern_8018E39C[arg0].vz;
    coord.flg        = 0;
    Gp_UpdateCoord(&coord);

    switch (arg0) {
        case 0:
            switch (view) {
                case 2:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 20:
                    SndEvt_EnqueueType7(0x5402000F, 1);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 18:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 19:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0xD);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0xD);
                    break;
                case 21:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
            }
            break;
        case 1:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 20:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 23:
                case 24:
                case 25:
                    SndEvt_EnqueueType7(0x5402000E, 1);
                    break;
            }
            break;
        case 2:
            switch (view) {
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 6:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 14:
                case 15:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 16:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 17:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 8:
                case 21:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x59);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 2:
                case 3:
                case 4:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 18:
                case 19:
                case 20:
                case 22:
                default:
                    SndEvt_EnqueueType7(0x54020010, 1);
                    break;
            }
            break;
        case 3:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x40);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x33);
                    break;
                case 8:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x20);
                    break;
                case 6:
                case 20:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan(&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan(&coord), 0x46);
                    break;
                case 3:
                case 4:
                case 5:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 22:
                case 23:
                case 24:
                case 25:
                default:
                    SndEvt_EnqueueType7(0x54020011, 1);
                    break;
            }
            break;
    }
}

static void func_mine_cavern_80182CEC(Task* arg0)
{
    s16 i;
    s32 flags;

    flags = GameFlag_GetNibble(0xE2);
    for (i = 0; i < 4; i++) {
        if (!((flags >> i) & 1)) {
            Gp_SpawnEnemyFromTable(D_mine_cavern_8018EB38, 0, i, NULL);
        }
        Gp_SpawnEnemyFromTable(D_mine_cavern_8018EB38, 1, i, NULL);
    }
    arg0->state++;
}

static void func_mine_cavern_80182DA8(Task* task)
{
    func_mine_cavern_80182454();
}

/// Mine task dispatcher: runs the state handler this task's `state` selects,
/// unless the screen id says the room is being left.
void func_mine_cavern_80182DC8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D65C;
    if (Mc_SaveData[0].state.demoScene != 3) {
        sp.funcs[arg0->state](arg0);
    }
}

/// Spawn state of the cavern enemy: allocates its work block, parks it in
/// `Task::work` and installs `func_mine_cavern_80183860` as the exit callback,
/// or destroys the enemy when the allocation fails. The model is hung under the
/// view coordinate, given the block's two matrices and seated on the spawn spot
/// `Task::spawnArg1` names. Two collision bodies are then linked through
/// `Gp_LinkObj`: a small one (kind 2) with four contact records and flag 0x8000
/// set, and a wide one (kind 1) with a single record and flag 0x8000 cleared.
/// The enemy takes its hit points and parameters from `D_mine_cavern_8018EAE4`,
/// the model is republished through `func_800D7A9C`, and the enemy's node is
/// linked with its flags set to 1.
///
/// The wide body's x and y offset are read from a structure at address 0. The
/// read has to be a structure member: the scheduler lets a load from a plain
/// scalar at a fixed address pass the stores into the body before it, and the
/// original keeps it behind them.
static void func_mine_cavern_80182E34(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* mem;
    MineCavernWork* work;
    GpObj*          obj40;
    GpObj*          objC0;
    u16             temp;
    VECTOR          vec;

    mem        = (MineCavernWork*)memCalloc(0x14C, false);
    work       = mem;
    arg1->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->exitCallback                  = func_mine_cavern_80183860;
    arg1->extra.tmd->coords->sub        = &gGfxViewCoord;
    arg1->extra.tmd->flags              = 0;
    arg1->extra.tmd->lightMtx           = &work->light;
    arg1->extra.tmd->colorMtx           = &work->color;
    arg1->extra.tmd->coords->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vx;
    arg1->extra.tmd->coords->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vy;
    arg1->extra.tmd->coords->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vz;
    arg1->extra.tmd->coords->flg        = 0;
    obj40                               = &work->obj40;
    obj40->coord                        = arg1->extra.tmd->coords;
    obj40->ctx.recs                     = work->recs;
    obj40->pos.vx                       = 0;
    obj40->pos.vy                       = -0x320;
    obj40->pos.vz                       = 0;
    obj40->key                          = 0x50000;
    obj40->radius                       = 0x100;
    obj40->flags                        = 1;
    Gp_LinkObj(2, obj40);
    obj40->flags |= 0x8000;
    Gp_InitRec18Table(obj40->ctx.recs, 4, 0);
    work->obj40.flags |= 0x8000;
    objC0              = &work->objC0;
    objC0->coord       = arg1->extra.tmd->coords;
    objC0->ctx.recs    = &work->recE0;
    temp               = ((SVECTOR*)NULL)->vy;
    objC0->pos.vz      = 0;
    objC0->radius      = 0xBB8;
    objC0->flags       = 1;
    objC0->pos.vy      = temp;
    objC0->pos.vx      = temp;
    Gp_LinkObj(1, objC0);
    Gp_InitRec18Table(objC0->ctx.recs, 1, 0);
    work->objC0.key    = 0x22121;
    work->objC0.flags &= 0x7FFF;
    arg0->hp           = D_mine_cavern_8018EAE4.hpMax;
    arg0->param        = &D_mine_cavern_8018EAE4;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    vec.vx = arg1->extra.tmd->coords->workm.t[0];
    vec.vy = arg1->extra.tmd->coords->workm.t[1];
    vec.vz = arg1->extra.tmd->coords->workm.t[2];
    func_800D7A9C(arg1->extra.tmd, &vec, 0, 3);
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = -0x320;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->extra.tmd->coords;
    Gp_LinkNode(&arg0->node);
    arg0->node.state.b.flags = 1;
    arg1->state++;
}

/// Second state handler of `D_mine_cavern_8017D7F8`: the cavern enemy's
/// per-frame hit check. Unless gameplay is suspended, it marks the enemy
/// lockable only while the player is within 0x1770 on the XZ plane and in place
/// 1 or 4, republishes the model's world position, and looks through the work
/// block's contacts for one of class 2. A contact taken in place 1 or 4 without
/// key bit 0x8000 costs the enemy the damage `D_mine_cavern_8018EAF4` gives its
/// key; when that empties `GpEnemy::hp` the enemy's `Task::spawnArg1` bit is
/// set in flag nibble 0xE2, the model is hidden, a sound is played at it and
/// the task advances.
static void func_mine_cavern_801830F0(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork*        work;
    Task*                  player;
    u8*                    head;
    _MineCavernHitScratch* blk;
    GpCoord*               coords;
    GpRec18*               recs;
    SVECTOR*               d;
    SVECTOR*               dst;
    s16                    i;
    s16                    angle;
    u32                    key;
    s32                    id;
    s32                    pan;

    work   = arg1->work;
    player = gameGetPtrSlot(3);
    switch (Gp_StateF0.field_4) {
        case 2:
            arg1->extra.tmd->flags = 0x80;
            return;
        case 0:
        default:
            break;
        case 1:
            return;
    }

    coords                              = arg1->extra.tmd->coords;
    head                                = SCRATCH_HEAD(u8);
    ((SVECTOR*)(head - 0x18))->vx       = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    d                                   = (SVECTOR*)(head - 0x18);
    d->vy                               = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    SCRATCH_HEAD(_MineCavernHitScratch) = (_MineCavernHitScratch*)(head - 0x28);
    d->vz                               = Player_Status.coordMtx->t[2] - coords->coord.t[2];
    blk                                 = (_MineCavernHitScratch*)(head - 0x28);

    if (overlayOutOfRange(d, 0x1770) || Gp_StateF0.prefix.bytes.field_0 != 1 ||
        (gGameSession->at4.loc.place != Gp_StateF0.prefix.bytes.field_0 && gGameSession->at4.loc.place != 4)) {
        arg0->node.state.b.flags = 1;
    } else {
        arg0->node.state.b.flags = 0;
    }

    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    blk->pos.vx = arg1->extra.tmd->coords->workm.t[0];
    blk->pos.vy = arg1->extra.tmd->coords->workm.t[1];
    blk->pos.vz = arg1->extra.tmd->coords->workm.t[2];
    func_800D7A9C(arg1->extra.tmd, &blk->pos, 0, 3);
    arg1->extra.tmd->flags = 0;

    dst  = &blk->d;
    recs = work->recs;
    for (i = 0; i < 4; i++) {
        if (recs[i].key == 0) {
            break;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            dst->vx = recs[i].point.vx;
            dst->vy = recs[i].point.vy;
            dst->vz = recs[i].point.vz;
            key     = recs[i].key;
            goto found;
        }
    }
    key = 0;
found:
    blk->key = key;
    if (key & 0x8000) {
        blk->key = 0;
    }
    if (gGameSession->at4.loc.place != 1 && gGameSession->at4.loc.place != 4) {
        blk->key = 0;
    }

    if (blk->key != 0) {
        blk->d.vx -= arg1->extra.tmd->coords->workm.t[0];
        blk->d.vy -= arg1->extra.tmd->coords->workm.t[1];
        blk->d.vz -= arg1->extra.tmd->coords->workm.t[2];
        angle = blk->angle = ratan2(blk->d.vx, blk->d.vz) - ratan2(-arg1->extra.tmd->coords->workm.m[2][0],
                                                                   arg1->extra.tmd->coords->workm.m[2][2]);
        if (angle < 0) {
        neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto neg;
            }
        } else {
        pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto pos;
            }
        }
        blk->angle  = angle;
        blk->pos.vx = player->extra.tmd->coords->coord.t[0] - arg1->extra.tmd->coords->coord.t[0];
        blk->pos.vy = player->extra.tmd->coords->coord.t[1] - arg1->extra.tmd->coords->coord.t[1];
        blk->pos.vz = player->extra.tmd->coords->coord.t[2] - arg1->extra.tmd->coords->coord.t[2];
        blk->dist   = SquareRoot0(blk->pos.vx * blk->pos.vx + blk->pos.vy * blk->pos.vy + blk->pos.vz * blk->pos.vz);
        blk->damage = Gp_ComputeDamage(blk->key, blk->dist, 0, 0);
        blk->damage = D_mine_cavern_8018EAF4[blk->key & 0x7F];
        arg0->hp   -= blk->damage;
        func_800DA6E8(&arg0->node, blk->damage, 0);
        if (arg0->hp <= 0) {
            blk->bits = GameFlag_GetNibble(0xE2);
            if (!((blk->bits >> (u16)arg1->spawnArg1.value) & 1)) {
                blk->bits |= 1 << (u16)arg1->spawnArg1.value;
                GameFlag_SetNibble(0xE2, blk->bits);
                arg1->extra.tmd->flags = 0x80;
            }
            id  = ((arg0->placeKey >> 12) << 8) | 0x54020014;
            pan = (s8)Gp_GetObjPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)(gpGetObjDepth(arg1->extra.tmd->coords) / 2));
            arg1->state++;
        }
    }
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->recE0);
    SCRATCH_POP_BYTES(0x28);
}

/// Second state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It allocates the work block, parks it at `Task::work` and
/// hands its two matrices to the model, then seats the model on the spawn spot
/// `Task::spawnArg1` names: the block's own coordinate adopts that spot with the
/// model's coordinate hung under it, and the model is republished through
/// `func_800D7A9C`.
///
/// `mem` and `work` are the same block: the original build tests and parks the
/// allocation through `mem` and reaches the block through `work` afterwards,
/// which is what keeps the two live ranges - and so `$v0` / `$a0` - apart.
static void func_mine_cavern_801836D0(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* mem;
    MineCavernWork* work;
    VECTOR          vec;

    mem        = (MineCavernWork*)memCalloc(0x14C, false);
    work       = mem;
    arg1->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->extra.tmd->coords->sub        = &gGfxViewCoord;
    arg1->extra.tmd->flags              = 0;
    arg1->extra.tmd->lightMtx           = &work->light;
    arg1->extra.tmd->colorMtx           = &work->color;
    arg1->extra.tmd->coords->coord.t[0] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vx;
    arg1->extra.tmd->coords->coord.t[1] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vy;
    arg1->extra.tmd->coords->coord.t[2] = D_mine_cavern_8018EB18[(u16)arg1->spawnArg1.value].vz;
    arg1->extra.tmd->coords->flg        = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    vec.vx = arg1->extra.tmd->coords->workm.t[0];
    vec.vy = arg1->extra.tmd->coords->workm.t[1];
    vec.vz = arg1->extra.tmd->coords->workm.t[2];
    func_800D7A9C(arg1->extra.tmd, &vec, 0, 3);
    arg1->state++;
}

static void func_mine_cavern_80183860(Task* arg0)
{
    MineCavernWork* work;

    work = (MineCavernWork*)arg0->work;
    if (work != NULL) {
        Gp_UnlinkObj(&work->obj40);
    }
}

static void func_mine_cavern_80183890(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;

    work                      = (MineCavernWork*)task->work;
    work->obj40.flags        &= 0x7FFF;
    enemy->node.state.b.flags = 1;
    Gp_UnlinkObj(&work->obj40);
    work->field_148 = 0;
    task->state++;
}

/// The two cue lines `func_mine_cavern_801838F4` prints on its first two ticks.
static const char D_mine_cavern_8017D7E8[] = "BOMB1\n";
static const char D_mine_cavern_8017D7F0[] = "BOMB2\n";

/// The cavern enemy's state handlers, run by `func_mine_cavern_80183A68`.
static const GpEnemyTaskFuncTable5 D_mine_cavern_8017D7F8 = {
    {
        func_mine_cavern_80182E34,
        func_mine_cavern_801830F0,
        func_mine_cavern_80183890,
        func_mine_cavern_801838F4,
        Gp_DestroyEnemy,
    },
};

/// The second enemy's state handlers, run by `func_mine_cavern_80183C10`.
static const GpEnemyTaskFuncTable3 D_mine_cavern_8017D80C = {
    { func_mine_cavern_801836D0, func_mine_cavern_80183AD4, Gp_DestroyEnemy },
};

/// Fourth state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It parks the model hidden (`field_C = 0x80`) and walks
/// `work->field_148` down its 0x3C-step countdown, one case per tick: 0 prints
/// "BOMB1", drops the model to y = -0x258 and spawns effect 0x01001200; 1
/// prints "BOMB2" and spawns 0x01000580, parking that effect's own first three
/// halfwords; 2 and 4 spawn 0x01002500; 3 and 5 clear the hidden bit on the
/// work block's second object (`objC0`); 9 hands `objC0` to `Gp_UnlinkObj`;
/// 0x3B advances `Task::state`.
static void func_mine_cavern_801838F4(GpEnemy* arg0, Task* arg1)
{
    MineCavernWork* work;
    GpEffWork*      eff;
    u16             state;

    work = (MineCavernWork*)arg1->work;

    arg1->extra.tmd->flags = 0x80;

    state           = work->field_148;
    work->field_148 = state + 1;

    switch ((s16)state) {
        case 0:
            printf(D_mine_cavern_8017D7E8);
            arg1->extra.tmd->coords->coord.t[1] = -0x258;
            arg1->extra.tmd->coords->flg        = 0;
            Gp_UpdateCoord(arg1->extra.tmd->coords);
            Gp_SpawnEff(0x6005C, arg1->extra.tmd->coords, 0x01001200, NULL);
            return;

        case 1:
            printf(D_mine_cavern_8017D7F0);
            eff = Gp_SpawnEff(0x6005C, arg1->extra.tmd->coords, 0x01000580, NULL);
            if (eff != NULL) {
                eff->move.vx = 0;
                eff->move.vy = -0xA;
                eff->move.vz = 0;
            }
            return;

        case 2:
        case 4:
            Gp_SpawnEff(0x6005C, arg1->extra.tmd->coords, 0x01002500, NULL);
            return;

        case 3:
        case 5:
            work->objC0.flags &= 0x7FFF;
            return;

        case 9:
            Gp_UnlinkObj(&work->objC0);
            return;

        case 0x3B:
            arg1->state++;
            break;

        default:
            return;
    }
}

void func_mine_cavern_80183A68(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_mine_cavern_8017D7F8;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Third state handler of `D_mine_cavern_8017D7F8` (`func_mine_cavern_80183A68`
/// dispatches it). It republishes the model's world position through
/// `func_800D7A9C`, then settles the work block's own coordinate: when the
/// `GameFlag_GetNibble(0xE2)` bit selected by `Task::spawnArg1` is set the
/// coordinate is reset to an identity rotation parked at (0, -0x320, 0) under
/// the model's own coordinate, `field_148` ticks, and the model's `field_C` is
/// cleared; otherwise the model is flagged hidden with `field_C = 0x80`.
///
/// `ang` is declared and never read - the original build's frame reserved 8
/// bytes for it ahead of nothing, so dropping it shrinks the frame from 0x38 to
/// 0x30 and moves every spill.
static void func_mine_cavern_80183AD4(GpEnemy* enemy, Task* task)
{
    MineCavernWork* work;
    MATRIX*         m;
    VECTOR          vec;
    SVECTOR         ang;

    work = (MineCavernWork*)task->work;

    task->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(task->extra.tmd->coords);
    vec.vx = task->extra.tmd->coords->workm.t[0];
    vec.vy = task->extra.tmd->coords->workm.t[1];
    vec.vz = task->extra.tmd->coords->workm.t[2];
    func_800D7A9C(task->extra.tmd, &vec, 0, 3);

    if (!((GameFlag_GetNibble(0xE2) >> (u16)task->spawnArg1.value) & 1)) {
        task->extra.tmd->flags = 0x80;
    } else {
        m                         = &work->coord.coord;
        *(s32*)&work->coord.coord = 0x1000;
        MATRIX_PAIR(m, 0, 2)      = 0;
        MATRIX_PAIR(m, 1, 1)      = 0x1000;
        MATRIX_PAIR(m, 2, 0)      = 0;
        m->m[2][2]                = 0x1000;
        work->coord.sub           = task->extra.tmd->coords;
        work->coord.coord.t[2]    = 0;
        work->coord.coord.t[0]    = 0;
        work->coord.coord.t[1]    = -0x320;
        work->coord.flg           = 0;
        Gp_UpdateCoord(&work->coord);
        work->field_148++;
        task->extra.tmd->flags = 0;
    }
}

/// Runs the current state handler of one of the room's enemies from its
/// three-entry table - setup (`func_mine_cavern_801836D0`), per-frame tick
/// (`func_mine_cavern_80183AD4`) or teardown (`Gp_DestroyEnemy`) - copying the
/// table onto the stack before the call.
void func_mine_cavern_80183C10(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_mine_cavern_8017D80C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
