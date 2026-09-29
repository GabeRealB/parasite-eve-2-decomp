#include "rooms/dryfield_night_saloon_g_r.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8018055c.h"

#define D_dryfield_night_saloon_g_r_801850DC (D_dryfield_night_saloon_g_r_80185074 + 13)
#define D_dryfield_night_saloon_g_r_801850E4 (D_dryfield_night_saloon_g_r_80185074[14])
#define D_dryfield_night_saloon_g_r_801850FC (D_dryfield_night_saloon_g_r_80185074[17])

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_dryfield_night_saloon_g_r_80188FB4[4];

extern GpObj4C D_dryfield_night_saloon_g_r_801887DC[21];

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_night_saloon_g_r_80188FAC;
extern RoomEventReq D_dryfield_night_saloon_g_r_80188FB8;

/// Descriptor of the event task `func_dryfield_night_saloon_g_r_8017DA04`.
extern TaskDesc D_dryfield_night_saloon_g_r_8017F90C;

/// Saved `Mc_SaveData[0].state.at4.loc.view` (area id), restored when the cutscene ends.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    u8 value;
    u8 retained[7];
} DryfieldNightSaloonGRStorage8FA4;
STATIC_ASSERT_SIZEOF(DryfieldNightSaloonGRStorage8FA4, 8);

extern DryfieldNightSaloonGRStorage8FA4 D_dryfield_night_saloon_g_r_80188FA4;

extern GpMsgEntry D_dryfield_night_saloon_g_r_8017F918[];
extern TaskDesc   D_dryfield_night_saloon_g_r_8017F940[];

/// Cutscene script blobs handed to `func_800E8614` / `func_800E8634`.
extern GpEvsCmd D_dryfield_night_saloon_g_r_801848DC[];
extern GpEvsCmd D_dryfield_night_saloon_g_r_80184B34[];
extern GpEvsCmd D_dryfield_night_saloon_g_r_80184D2C[];
extern GpEvsCmd D_dryfield_night_saloon_g_r_80183C94[];
extern GpEvsCmd D_dryfield_night_saloon_g_r_801847A4[];

/// The jukebox's track lists, one per game mode, each a run of track id and
/// name pairs.
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F0C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F24[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F3C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F54[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F6C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F84[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FA4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FC4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FE4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80185004[];

/// The jukebox menu's title, "SELECT". A stray 0x0D byte follows its
/// terminator, so the block stays in assembly.
static const char D_dryfield_night_saloon_g_r_8017D898[];

/// The jukebox's track list.
extern UiList D_dryfield_night_saloon_g_r_80185028;

/// Descriptor of the jukebox menu panel, whose task is
/// `func_dryfield_night_saloon_g_r_8017E28C`.
extern UiObjectDesc D_dryfield_night_saloon_g_r_8018504C;

/// Descriptor of the jukebox task `func_dryfield_night_saloon_g_r_8017E564`.
extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

/// The room's effect positions in the model's local space. The frame hook
/// draws a quad at each of 0-10 and 20-27; 12 and 13 are the two ends
/// `func_dryfield_night_saloon_g_r_8017F0A4` is handed; 14-19 are the two
/// light shafts of `func_dryfield_night_saloon_g_r_8017EB38`.

/// Entry 13 of `D_dryfield_night_saloon_g_r_80185074`, reached under a label
/// of its own.

/// Entries 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`, the two
/// shaft roots, which the code also reaches under labels of their own.

/// One view bitmask per effect, tested against `1 << view`. Entries 0-10 gate
/// positions 0-10, entries 11 and 12 the two helper effects, and entries 13-20
/// gate positions 20-27.
extern s16 D_dryfield_night_saloon_g_r_80185154[];

static void func_dryfield_night_saloon_g_r_8017DF90(Task* task);
static void func_dryfield_night_saloon_g_r_8017E040(Task* task);
static s32  func_dryfield_night_saloon_g_r_8017E698(s32 arg0);
static void func_dryfield_night_saloon_g_r_8017E8B0(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_saloon_g_r_8017EB38(GpCoord* coord);
static void func_dryfield_night_saloon_g_r_8017F0A4(GpCoord* coord, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

// Indexed views below share one contiguous table.
void func_dryfield_night_saloon_g_r_8017E28C(Task*);
void func_dryfield_night_saloon_g_r_8017E564(Task*);

extern GpRoomCoordSet D_dryfield_night_saloon_g_r_80188304[1];
void                  func_dryfield_night_saloon_g_r_8017E0C0(UiList*, UiObject*);

extern GpGridParams D_dryfield_night_saloon_g_r_80185B50[1];
extern GpObj3A      D_dryfield_night_saloon_g_r_80188E18[2];
extern GpObj4C      D_dryfield_night_saloon_g_r_8018831C[16];

extern GpSprtCmd  D_dryfield_night_saloon_g_r_80185D48[2];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_80185EAC[3];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_80185FDC[4];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_80186574[6];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_80186A90[5];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_801871D4[5];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_801873C8[5];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_801873F0[2];
extern GpSprtCmd  D_dryfield_night_saloon_g_r_80187504[5];
extern GpSprtElem D_dryfield_night_saloon_g_r_80185D58[17];
extern GpSprtElem D_dryfield_night_saloon_g_r_80185EC4[14];
extern GpSprtElem D_dryfield_night_saloon_g_r_80185FFC[70];
extern GpSprtElem D_dryfield_night_saloon_g_r_801865A4[63];
extern GpSprtElem D_dryfield_night_saloon_g_r_80186AB8[91];
extern GpSprtElem D_dryfield_night_saloon_g_r_801871FC[23];
extern GpSprtElem D_dryfield_night_saloon_g_r_80187400[13];

extern GpAnimArg     D_dryfield_night_saloon_g_r_80183968;
extern GpAnimArg     D_dryfield_night_saloon_g_r_8018397C;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183990;
extern GpAnimArg     D_dryfield_night_saloon_g_r_801839B8;
extern GpAnimArg     D_dryfield_night_saloon_g_r_801839CC;
extern GpAnimArg     D_dryfield_night_saloon_g_r_801839F4;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183A08;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183A1C;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183A6C;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183A94;
extern GpAnimArg     D_dryfield_night_saloon_g_r_80183AA8;
extern GpCopyArg     D_dryfield_night_saloon_g_r_8018394C;
extern GpOverrideArg D_dryfield_night_saloon_g_r_80183B34;
extern GpXformArg    D_dryfield_night_saloon_g_r_80183ABC;
extern GpXformArg    D_dryfield_night_saloon_g_r_80183AD4;
extern GpXformArg    D_dryfield_night_saloon_g_r_80183AEC;
extern GpXformArg    D_dryfield_night_saloon_g_r_80183B04;
extern GpXformArg    D_dryfield_night_saloon_g_r_80183B1C;
extern const char    D_dryfield_night_saloon_g_r_8017D600[22];
extern const char    D_dryfield_night_saloon_g_r_8017D618[19];
extern const char    D_dryfield_night_saloon_g_r_8017D62C[14];
extern const char    D_dryfield_night_saloon_g_r_8017D63C[26];
extern const char    D_dryfield_night_saloon_g_r_8017D658[24];
extern const char    D_dryfield_night_saloon_g_r_8017D670[17];
extern const char    D_dryfield_night_saloon_g_r_8017D684[23];
extern const char    D_dryfield_night_saloon_g_r_8017D69C[17];
extern const char    D_dryfield_night_saloon_g_r_8017D6B0[16];
extern const char    D_dryfield_night_saloon_g_r_8017D6C0[15];
extern const char    D_dryfield_night_saloon_g_r_8017D6D0[24];
extern const char    D_dryfield_night_saloon_g_r_8017D6E8[11];
extern const char    D_dryfield_night_saloon_g_r_8017D6F4[27];
extern const char    D_dryfield_night_saloon_g_r_8017D710[11];
extern const char    D_dryfield_night_saloon_g_r_8017D71C[17];
extern const char    D_dryfield_night_saloon_g_r_8017D730[14];
extern const char    D_dryfield_night_saloon_g_r_8017D740[11];
extern const char    D_dryfield_night_saloon_g_r_8017D74C[13];
extern const char    D_dryfield_night_saloon_g_r_8017D75C[23];
extern const char    D_dryfield_night_saloon_g_r_8017D774[13];
extern const char    D_dryfield_night_saloon_g_r_8017D784[17];
extern const char    D_dryfield_night_saloon_g_r_8017D798[16];
extern const char    D_dryfield_night_saloon_g_r_8017D7A8[12];
extern const char    D_dryfield_night_saloon_g_r_8017D7B4[20];
extern const char    D_dryfield_night_saloon_g_r_8017D7C8[17];
extern const char    D_dryfield_night_saloon_g_r_8017D7DC[19];
extern const char    D_dryfield_night_saloon_g_r_8017D7F0[24];
extern const char    D_dryfield_night_saloon_g_r_8017D808[19];
extern const char    D_dryfield_night_saloon_g_r_8017D81C[27];
extern const char    D_dryfield_night_saloon_g_r_8017D838[18];
extern const char    D_dryfield_night_saloon_g_r_8017D84C[16];
extern const char    D_dryfield_night_saloon_g_r_8017D85C[20];
void                 func_dryfield_night_saloon_g_r_8017E0A8(u8);

s32  func_dryfield_night_saloon_g_r_8017DCA4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_saloon_g_r_8017DD7C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_night_saloon_g_r_8017DD84(Task*, s32, s32, s32);
s32  func_dryfield_night_saloon_g_r_8017DE68(Task*, s32, GpMsg13EF*, GpMessageArg);
void func_dryfield_night_saloon_g_r_8017DA04(Task*);
void func_dryfield_night_saloon_g_r_8017DB74(Task*);

TaskDesc D_dryfield_night_saloon_g_r_8017F90C = { 0, 32, func_dryfield_night_saloon_g_r_8017DA04, { .model = NULL } };

GpMsgEntry D_dryfield_night_saloon_g_r_8017F918[5] = {
    { 5102, func_dryfield_night_saloon_g_r_8017DCA4 },
    { 5105, func_dryfield_night_saloon_g_r_8017DD7C },
    { 5103, func_dryfield_night_saloon_g_r_8017DE68 },
    { 5104, func_dryfield_night_saloon_g_r_8017DD84 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_saloon_g_r_8017F940[1] = {
    { 0, 192, func_dryfield_night_saloon_g_r_8017DB74, { .model = NULL } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} DryfieldNightSaloonGRPoseBank238C;

DryfieldNightSaloonGRPoseBank238C D_dryfield_night_saloon_g_r_8017F94C = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_8017F994[46] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_8017FA4C[109] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_records.inc"
};

u16 D_dryfield_night_saloon_g_r_8017FC00[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_8017FC28 = {
    D_dryfield_night_saloon_g_r_8017FA4C,
    D_dryfield_night_saloon_g_r_8017FC00,
    { NULL, D_dryfield_night_saloon_g_r_8017F94C.words, NULL, NULL, D_dryfield_night_saloon_g_r_8017F994, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} DryfieldNightSaloonGRPoseBank2690;

DryfieldNightSaloonGRPoseBank2690 D_dryfield_night_saloon_g_r_8017FC50 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_8017FCB0[84] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_8017FE00[117] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_8017FFD4[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_8017FFFC = {
    D_dryfield_night_saloon_g_r_8017FE00,
    D_dryfield_night_saloon_g_r_8017FFD4,
    { NULL, D_dryfield_night_saloon_g_r_8017FC50.words, NULL, NULL, D_dryfield_night_saloon_g_r_8017FCB0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldNightSaloonGRPoseBank2A64;

DryfieldNightSaloonGRPoseBank2A64 D_dryfield_night_saloon_g_r_80180024 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80180078[62] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80180170[149] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801803C4[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_801803EC = {
    D_dryfield_night_saloon_g_r_80180170,
    D_dryfield_night_saloon_g_r_801803C4,
    { NULL, D_dryfield_night_saloon_g_r_80180024.words, NULL, NULL, D_dryfield_night_saloon_g_r_80180078, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldNightSaloonGRPoseBank2E54;

DryfieldNightSaloonGRPoseBank2E54 D_dryfield_night_saloon_g_r_80180414 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80180468[74] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80180590[104] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80180730[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80180758 = {
    D_dryfield_night_saloon_g_r_80180590,
    D_dryfield_night_saloon_g_r_80180730,
    { NULL, D_dryfield_night_saloon_g_r_80180414.words, NULL, NULL, D_dryfield_night_saloon_g_r_80180468, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} DryfieldNightSaloonGRPoseBank31C0;

DryfieldNightSaloonGRPoseBank31C0 D_dryfield_night_saloon_g_r_80180780 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_801807F8[100] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80180988[178] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80180C50[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80180C78 = {
    D_dryfield_night_saloon_g_r_80180988,
    D_dryfield_night_saloon_g_r_80180C50,
    { NULL, D_dryfield_night_saloon_g_r_80180780.words, NULL, NULL, D_dryfield_night_saloon_g_r_801807F8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} DryfieldNightSaloonGRPoseBank36E0;

DryfieldNightSaloonGRPoseBank36E0 D_dryfield_night_saloon_g_r_80180CA0 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80180CDC[59] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80180DC8[90] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80180F30[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80180F58 = {
    D_dryfield_night_saloon_g_r_80180DC8,
    D_dryfield_night_saloon_g_r_80180F30,
    { NULL, D_dryfield_night_saloon_g_r_80180CA0.words, NULL, NULL, D_dryfield_night_saloon_g_r_80180CDC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} DryfieldNightSaloonGRPoseBank39C0;

DryfieldNightSaloonGRPoseBank39C0 D_dryfield_night_saloon_g_r_80180F80 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80180FC8[110] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80181180[166] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80181418[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80181440 = {
    D_dryfield_night_saloon_g_r_80181180,
    D_dryfield_night_saloon_g_r_80181418,
    { NULL, D_dryfield_night_saloon_g_r_80180F80.words, NULL, NULL, D_dryfield_night_saloon_g_r_80180FC8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} DryfieldNightSaloonGRPoseBank3EA8;

DryfieldNightSaloonGRPoseBank3EA8 D_dryfield_night_saloon_g_r_80181468 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_8018148C[81] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_801815D0[128] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801817D0[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_801817F8 = {
    D_dryfield_night_saloon_g_r_801815D0,
    D_dryfield_night_saloon_g_r_801817D0,
    { NULL, D_dryfield_night_saloon_g_r_80181468.words, NULL, NULL, D_dryfield_night_saloon_g_r_8018148C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} DryfieldNightSaloonGRPoseBank4260;

DryfieldNightSaloonGRPoseBank4260 D_dryfield_night_saloon_g_r_80181820 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80181844[34] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_801818CC[63] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801819C8[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_801819F0 = {
    D_dryfield_night_saloon_g_r_801818CC,
    D_dryfield_night_saloon_g_r_801819C8,
    { NULL, D_dryfield_night_saloon_g_r_80181820.words, NULL, NULL, D_dryfield_night_saloon_g_r_80181844, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} DryfieldNightSaloonGRPoseBank4458;

DryfieldNightSaloonGRPoseBank4458 D_dryfield_night_saloon_g_r_80181A18 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80181A54[58] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80181B3C[120] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80181D1C[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80181D44 = {
    D_dryfield_night_saloon_g_r_80181B3C,
    D_dryfield_night_saloon_g_r_80181D1C,
    { NULL, D_dryfield_night_saloon_g_r_80181A18.words, NULL, NULL, D_dryfield_night_saloon_g_r_80181A54, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} DryfieldNightSaloonGRPoseBank47AC;

DryfieldNightSaloonGRPoseBank47AC D_dryfield_night_saloon_g_r_80181D6C = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80181D90[34] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80181E18[63] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80181F14[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80181F3C = {
    D_dryfield_night_saloon_g_r_80181E18,
    D_dryfield_night_saloon_g_r_80181F14,
    { NULL, D_dryfield_night_saloon_g_r_80181D6C.words, NULL, NULL, D_dryfield_night_saloon_g_r_80181D90, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} DryfieldNightSaloonGRPoseBank49A4;

DryfieldNightSaloonGRPoseBank49A4 D_dryfield_night_saloon_g_r_80181F64 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80181F94[46] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_8018204C[92] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801821BC[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_801821E4 = {
    D_dryfield_night_saloon_g_r_8018204C,
    D_dryfield_night_saloon_g_r_801821BC,
    { NULL, D_dryfield_night_saloon_g_r_80181F64.words, NULL, NULL, D_dryfield_night_saloon_g_r_80181F94, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} DryfieldNightSaloonGRPoseBank4C4C;

DryfieldNightSaloonGRPoseBank4C4C D_dryfield_night_saloon_g_r_8018220C = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80182254[68] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80182364[99] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801824F0[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80182518 = {
    D_dryfield_night_saloon_g_r_80182364,
    D_dryfield_night_saloon_g_r_801824F0,
    { NULL, D_dryfield_night_saloon_g_r_8018220C.words, NULL, NULL, D_dryfield_night_saloon_g_r_80182254, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} DryfieldNightSaloonGRPoseBank4F80;

DryfieldNightSaloonGRPoseBank4F80 D_dryfield_night_saloon_g_r_80182540 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_801825B8[96] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80182738[136] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80182958[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80182980 = {
    D_dryfield_night_saloon_g_r_80182738,
    D_dryfield_night_saloon_g_r_80182958,
    { NULL, D_dryfield_night_saloon_g_r_80182540.words, NULL, NULL, D_dryfield_night_saloon_g_r_801825B8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} DryfieldNightSaloonGRPoseBank53E8;

DryfieldNightSaloonGRPoseBank53E8 D_dryfield_night_saloon_g_r_801829A8 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_801829F0[71] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80182B0C[102] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80182CA4[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_80182CCC = {
    D_dryfield_night_saloon_g_r_80182B0C,
    D_dryfield_night_saloon_g_r_80182CA4,
    { NULL, D_dryfield_night_saloon_g_r_801829A8.words, NULL, NULL, D_dryfield_night_saloon_g_r_801829F0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} DryfieldNightSaloonGRPoseBank5734;

DryfieldNightSaloonGRPoseBank5734 D_dryfield_night_saloon_g_r_80182CF4 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80182D30[99] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_80182EBC[150] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_80183114[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_8018313C = {
    D_dryfield_night_saloon_g_r_80182EBC,
    D_dryfield_night_saloon_g_r_80183114,
    { NULL, D_dryfield_night_saloon_g_r_80182CF4.words, NULL, NULL, D_dryfield_night_saloon_g_r_80182D30, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[13];
    GpPackedSvec words[39];
} DryfieldNightSaloonGRPoseBank5BA4;

DryfieldNightSaloonGRPoseBank5BA4 D_dryfield_night_saloon_g_r_80183164 = { .poses = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_bank1.inc"
                                                                           } };

GpPackedSvec D_dryfield_night_saloon_g_r_80183200[179] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_bank4.inc"
};

GpAnimRec D_dryfield_night_saloon_g_r_801834CC[250] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_records.inc"
};

u16 D_dryfield_night_saloon_g_r_801838B4[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_indices.inc"
};

GpAnimSet D_dryfield_night_saloon_g_r_801838DC = {
    D_dryfield_night_saloon_g_r_801834CC,
    D_dryfield_night_saloon_g_r_801838B4,
    { NULL, D_dryfield_night_saloon_g_r_80183164.words, NULL, NULL, D_dryfield_night_saloon_g_r_80183200, NULL, NULL, NULL },
};

GpAnimSet* D_dryfield_night_saloon_g_r_80183904[18] = {
    NULL,
    &D_dryfield_night_saloon_g_r_8017FC28,
    &D_dryfield_night_saloon_g_r_801821E4,
    &D_dryfield_night_saloon_g_r_80182518,
    &D_dryfield_night_saloon_g_r_80182980,
    &D_dryfield_night_saloon_g_r_8017FFFC,
    &D_dryfield_night_saloon_g_r_801803EC,
    &D_dryfield_night_saloon_g_r_80180758,
    &D_dryfield_night_saloon_g_r_80181440,
    &D_dryfield_night_saloon_g_r_80180C78,
    &D_dryfield_night_saloon_g_r_80180F58,
    &D_dryfield_night_saloon_g_r_801819F0,
    &D_dryfield_night_saloon_g_r_80181D44,
    &D_dryfield_night_saloon_g_r_80181F3C,
    &D_dryfield_night_saloon_g_r_8018313C,
    &D_dryfield_night_saloon_g_r_801838DC,
    &D_dryfield_night_saloon_g_r_80182CCC,
    &D_dryfield_night_saloon_g_r_801817F8,
};

GpCopyArg D_dryfield_night_saloon_g_r_8018394C = { { .sets = D_dryfield_night_saloon_g_r_80183904 }, 18 };

GpAnimArg D_dryfield_night_saloon_g_r_80183954 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183968 = { { .index = 1 }, 48, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_8018397C = { { .index = 1 }, 49, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183990 = { { .index = 1 }, 50, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_801839A4 = { { .index = 1 }, 51, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_801839B8 = { { .index = 1 }, 52, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_801839CC = { { .index = 1 }, 53, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_801839E0 = { { .index = 1 }, 54, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_801839F4 = { { .index = 1 }, 55, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183A08 = { { .index = 1 }, 56, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183A1C = { { .index = 1 }, 57, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183A30[3] = {
    { { .index = 1 }, 58, 1, 15, 0 },
    { { .index = 1 }, 59, 1, 15, 0 },
    { { .index = 1 }, 60, 1, 15, 0 },
};

GpAnimArg D_dryfield_night_saloon_g_r_80183A6C = { { .index = 1 }, 61, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183A80 = { { .index = 1 }, 62, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183A94 = { { .index = 1 }, 63, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183AA8 = { { .index = 1 }, 64, 1, 15, 0 };

GpXformArg D_dryfield_night_saloon_g_r_80183ABC = { { 950, 0, -180, 0 }, { 0, 530, 0, 0 } };

GpXformArg D_dryfield_night_saloon_g_r_80183AD4 = { { 1640, 0, 510, 0 }, { 0, 530, 0, 0 } };

GpXformArg D_dryfield_night_saloon_g_r_80183AEC = { { 2150, 0, 545, 0 }, { 0, 470, 0, 0 } };

GpXformArg D_dryfield_night_saloon_g_r_80183B04 = { { 2150, 0, 545, 0 }, { 0, 470, 0, 0 } };

GpXformArg D_dryfield_night_saloon_g_r_80183B1C = { { 4110, 0, 1730, 0 }, { 0, -1024, 0, 0 } };

GpOverrideArg D_dryfield_night_saloon_g_r_80183B34 = { 62, 48 };

GpAnimArg D_dryfield_night_saloon_g_r_80183B3C[2] = {
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 1, 15, 0 },
};

GpAnimArg D_dryfield_night_saloon_g_r_80183B64 = { { .index = 0 }, 2, 0, 0, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183B78 = { { .index = 0 }, 3, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183B8C = { { .index = 0 }, 4, 1, 10, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183BA0 = { { .index = 0 }, 5, 1, 10, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183BB4 = { { .index = 0 }, 6, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183BC8 = { { .index = 0 }, 7, 1, 25, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183BDC = { { .index = 0 }, 8, 1, 15, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_night_saloon_g_r_80183BF0 = { { .index = 0 }, 9, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C04 = { { .index = 0 }, 10, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C18 = { { .index = 0 }, 11, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C2C = { { .index = 0 }, 12, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C40 = { { .index = 0 }, 13, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C54 = { { .index = 0 }, 14, 1, 15, 0 };

GpAnimArg D_dryfield_night_saloon_g_r_80183C68 = { { .index = 0 }, 15, 1, 15, 0 };

GpXformArg D_dryfield_night_saloon_g_r_80183C7C = { { 2670, 0, 1520, 0 }, { 0, 1024, 0, 0 } };

GpEvsCmd D_dryfield_night_saloon_g_r_80183C94[118] = {
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_saloon_g_r_8018394C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_saloon_g_r_80183ABC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_night_saloon_g_r_80183C7C }, { .value = 0 } },
    { 3, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_dryfield_night_saloon_g_r_80183AD4 }, { .storage = &D_dryfield_night_saloon_g_r_80183B34 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x53120006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C68 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B78 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C40 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x53120007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B8C }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BA0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_8018397C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C54 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183990 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BB4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x53120008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839CC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BC8 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C18 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BDC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BC8 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C18 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BDC }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A08 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BC8 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C18 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A1C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BC8 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C18 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x53120009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839F4 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C04 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BC8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C18 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183BDC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A94 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C2C }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_dryfield_night_saloon_g_r_8017E0A8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_saloon_g_r_801847A4[13] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_saloon_g_r_80183ABC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_dryfield_night_saloon_g_r_8017E0A8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_saloon_g_r_801848DC[25] = {
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_saloon_g_r_8018394C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_saloon_g_r_80183AEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_night_saloon_g_r_80183C7C }, { .value = 0 } },
    { 3, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5312000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C68 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B78 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A6C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_saloon_g_r_80184B34[21] = {
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_saloon_g_r_8018394C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_saloon_g_r_80183B04 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_night_saloon_g_r_80183C7C }, { .value = 0 } },
    { 3, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183C68 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_saloon_g_r_80184D2C[20] = {
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_saloon_g_r_8018394C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_saloon_g_r_80183B1C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_night_saloon_g_r_80183C7C }, { .value = 0 } },
    { 3, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5312000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_night_saloon_g_r_80183B64 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F0C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 23, D_dryfield_night_saloon_g_r_8017D618 },
    { 49, D_dryfield_night_saloon_g_r_8017D600 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F24[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 60, D_dryfield_night_saloon_g_r_8017D658 },
    { 66, D_dryfield_night_saloon_g_r_8017D63C },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F3C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 22, D_dryfield_night_saloon_g_r_8017D684 },
    { 74, D_dryfield_night_saloon_g_r_8017D670 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F54[3] = {
    { 67, D_dryfield_night_saloon_g_r_8017D6C0 },
    { 82, D_dryfield_night_saloon_g_r_8017D6B0 },
    { 93, D_dryfield_night_saloon_g_r_8017D69C },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F6C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 21, D_dryfield_night_saloon_g_r_8017D6E8 },
    { 60, D_dryfield_night_saloon_g_r_8017D6D0 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F84[4] = {
    { 41, D_dryfield_night_saloon_g_r_8017D730 },
    { 45, D_dryfield_night_saloon_g_r_8017D71C },
    { 58, D_dryfield_night_saloon_g_r_8017D710 },
    { 61, D_dryfield_night_saloon_g_r_8017D6F4 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FA4[4] = {
    { 35, D_dryfield_night_saloon_g_r_8017D774 },
    { 36, D_dryfield_night_saloon_g_r_8017D75C },
    { 42, D_dryfield_night_saloon_g_r_8017D74C },
    { 44, D_dryfield_night_saloon_g_r_8017D740 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FC4[4] = {
    { 31, D_dryfield_night_saloon_g_r_8017D7B4 },
    { 59, D_dryfield_night_saloon_g_r_8017D7A8 },
    { 89, D_dryfield_night_saloon_g_r_8017D798 },
    { 93, D_dryfield_night_saloon_g_r_8017D784 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FE4[4] = {
    { 76, D_dryfield_night_saloon_g_r_8017D808 },
    { 77, D_dryfield_night_saloon_g_r_8017D7F0 },
    { 78, D_dryfield_night_saloon_g_r_8017D7DC },
    { 83, D_dryfield_night_saloon_g_r_8017D7C8 },
};

RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80185004[4] = {
    { 9, D_dryfield_night_saloon_g_r_8017D85C },
    { 43, D_dryfield_night_saloon_g_r_8017D84C },
    { 17, D_dryfield_night_saloon_g_r_8017D838 },
    { 37, D_dryfield_night_saloon_g_r_8017D81C },
};

UiListItemFunc D_dryfield_night_saloon_g_r_80185024[1] = {
    func_dryfield_night_saloon_g_r_8017E0C0,
};

UiList D_dryfield_night_saloon_g_r_80185028 = { D_dryfield_night_saloon_g_r_80185024, 1, { .u = 1 }, 0, 17, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_night_saloon_g_r_8018504C = { 2, 0xFF90, 0xFFC0, 224, 128, 48, 0, 0, 192, func_dryfield_night_saloon_g_r_8017E28C, 0 };

TaskDesc D_dryfield_night_saloon_g_r_80185068 = { 0, 192, func_dryfield_night_saloon_g_r_8017E564, { .model = NULL } };

SVECTOR D_dryfield_night_saloon_g_r_80185074[28] = {
    { 4915, -1870, -727, 0 },
    { 4915, -1870, -2339, 0 },
    { 4915, -1870, -4188, 0 },
    { -3905, -1870, -4460, 0 },
    { -100, -1870, 4785, 0 },
    { 1100, -1870, 4785, 0 },
    { 3500, -2097, 3000, 0 },
    { 3500, -2097, 2000, 0 },
    { 3500, -2097, 1000, 0 },
    { 3500, -2097, 0, 0 },
    { 4200, -2097, -500, 0 },
    { 4983, -2211, 4240, 0 },
    { 4983, -2211, 4467, 0 },
    { 4983, -2211, 4013, 0 },
    { 700, -1640, 1680, 0 },
    { 700, -1520, 1520, 0 },
    { 700, -1520, 1840, 0 },
    { -750, -1640, 1680, 0 },
    { -750, -1520, 1520, 0 },
    { -750, -1520, 1840, 0 },
    { -3900, -1840, -2430, 0 },
    { -3900, -1840, -420, 0 },
    { -3900, -1840, 1630, 0 },
    { -2600, -2570, -2500, 0 },
    { -2600, -2570, 1900, 0 },
    { -300, -2570, -300, 0 },
    { 2000, -2570, -2500, 0 },
    { 2000, -2570, 1900, 0 },
};

s16 D_dryfield_night_saloon_g_r_80185154[22] = {
    2176,
    2060,
    4,
    1032,
    560,
    560,
    656,
    2704,
    2688,
    2176,
    2176,
    656,
    560,
    1024,
    1024,
    1024,
    1024,
    1024,
    1024,
    0,
    512,
    -3540,
};

GpRoomCoordRec D_dryfield_night_saloon_g_r_80185180[2] = {
    { D_dryfield_night_saloon_g_r_80188304, NULL },
    { D_dryfield_night_saloon_g_r_80188304, NULL },
};

GpRoomObjRec D_dryfield_night_saloon_g_r_80185190[2] = {
    { D_dryfield_night_saloon_g_r_80185B50, D_dryfield_night_saloon_g_r_8018831C, &D_dryfield_night_saloon_g_r_8018831C[16], D_dryfield_night_saloon_g_r_80188E18 },
    { D_dryfield_night_saloon_g_r_80185B50, D_dryfield_night_saloon_g_r_8018831C, &D_dryfield_night_saloon_g_r_8018831C[16], D_dryfield_night_saloon_g_r_80188E18 },
};

u8* D_dryfield_night_saloon_g_r_801851B0[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_saloon_g_r_801851B8[2] = {
    { { .bytes = { 13, 0 } } },
    { { .bytes = { 13, 0 } } },
};

GpWarpRec D_dryfield_night_saloon_g_r_801851BC[2] = {
    { { .words = { 0, 2386, 0, -4661 } }, { 0, 0, 0, 0 }, { .words = { 0, 2386, 0, -4661 } }, { 0, 0, 0, 0 }, 0x53120002, 0x53120001, 0, 2, 0, 479 },
    { { .words = { 3072, 4665, 0, 4567 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4665, 0, 4567 } }, { 0, 0, 0, 0 }, 0x53120004, 0x53120003, 0, 7, 0, 478 },
};

SVECTOR D_dryfield_night_saloon_g_r_8018522C[9] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_normals.inc"
};

SVECTOR D_dryfield_night_saloon_g_r_80185274[116] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_verts.inc"
};

GpGridFace D_dryfield_night_saloon_g_r_80185614[61] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_faces.inc"
};

s16 D_dryfield_night_saloon_g_r_801858F0[280] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_saloon_g_r_801858F0[i])
s16* D_dryfield_night_saloon_g_r_80185B20[12] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_saloon_g_r_80185B50[1] = {
    { NULL, D_dryfield_night_saloon_g_r_8018522C, D_dryfield_night_saloon_g_r_80185274, D_dryfield_night_saloon_g_r_80185614, D_dryfield_night_saloon_g_r_80185B20, 4500, 5400, 3, 4, 4000, 61 },
};

GpViewRec D_dryfield_night_saloon_g_r_80185B74[13] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, -680 } }, 519 },
    { { { { -3183, 0, -2576 }, { -1148, 3666, 1418 }, { 2306, 1824, -2850 } }, { 315, 2637, 16 } }, 257 },
    { { { { -4045, 0, -638 }, { -348, 3430, 2210 }, { 534, 2237, -3388 } }, { 724, 3602, -2788 } }, 230 },
    { { { { 4063, 0, -511 }, { -261, 3522, -2074 }, { 439, 2090, 3494 } }, { 628, 3274, 3211 } }, 230 },
    { { { { 3790, 0, -1551 }, { -715, 3635, -1746 }, { 1377, 1887, 3364 } }, { 1033, 2561, -668 } }, 230 },
    { { { { -171, 0, 4092 }, { 3010, 2774, 126 }, { -2771, 3013, -116 } }, { -1881, 2483, -5950 } }, 188 },
    { { { { 3128, 0, -2643 }, { -887, 3858, -1050 }, { 2490, 1375, 2946 } }, { -1421, 2204, 1801 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -620, 1520, -5560 } }, 230 },
    { { { { 4089, 0, -236 }, { 96, 3738, 1671 }, { 215, -1674, 3731 } }, { -1891, 732, 608 } }, 230 },
    { { { { -627, 0, 4047 }, { 323, 4082, 50 }, { -4034, 326, -625 } }, { -4164, 1341, -1329 } }, 230 },
    { { { { -110, 0, -4094 }, { -247, 4088, 6 }, { 4087, 247, -109 } }, { -415, 1531, -768 } }, 230 },
    { { { { 702, 0, 4035 }, { 1935, 3594, -337 }, { -3540, 1964, 616 } }, { 2150, 1500, -3200 } }, 289 },
    { { { { 2919, 0, 2873 }, { 1429, 3553, -1451 }, { -2492, 2037, 2532 } }, { 1395, 2500, -2735 } }, 257 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80185D48[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80185D58[17] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 48, 550, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 88, 550, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 104, 500, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 16, 96, 500, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 16, 64, 525, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 72, 104, 500, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, 80, 500, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -8, 850, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 850, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 40, 875, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1050, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1050, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 8, 1075, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -8, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 1075, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -16, 1075, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80185EAC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80185EC4[14] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 1200, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, 8, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, 16, 975, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -32, -16, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -32, 16, 975, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 48, 112, 719, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 16, 104, 730, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, 104, 516, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, 96, 522, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 96, 521, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 529, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -128, 88, 529, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -136, 96, 532, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, 112, 527, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80185FDC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80185FFC[70] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -112, 1250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -112, 1250, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -56, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 1325, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -32, 1325, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -32, -32, 1375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, -16, 1325, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1375, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 1325, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1325, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 0, 1375, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -16, 1375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, 16, 1375, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, -32, 1350, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -16, 1325, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 1325, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -16, 1350, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 0, 1325, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 16, 1250, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -152, 24, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 40 } }, -8, 80, 705, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -8, 48, 798, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 8, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 32 } }, -24, 16, 1000, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, 48, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 64, 925, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 32, 925, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -112, -120, 1775, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -104, -112, 1843, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -96, -72, 1758, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 48 } }, -80, -80, 1981, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 40 } }, -80, -120, 1847, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -8, -120, 1873, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -96, 2291, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, -120, 1855, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -96, 1921, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -72, 2025, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, -48, 2105, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -40, -48, 1930, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -72, 1985, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -96, 1900, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -32, 1833, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -48, 1615, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 88, -64, 1862, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, -64, 2213, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 80, -40, 1611, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 96, -64, 1775, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -40, 1588, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -24, 1724, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 104, -16, 1623, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -40, 1414, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -64, 1727, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -56, 1608, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -32, 1377, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 112, -8, 1540, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, 0, 1455, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -24, 1509, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -48, 1480, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -48, 1424, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 128, -16, 1229, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 8, 1378, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, 8, 1327, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -16, 1213, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -40, 1371, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, -40, 1323, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 144, -8, 1292, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 144, 16, 1059, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 152, 24, 997, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 0, 1065, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 152, -32, 1251, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80186574[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 7, 0, 0, { 1, 0 } },
    { 27, 14, 0, 0, { 2, 0 } },
    { 41, 29, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_801865A4[63] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 250, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 16, -24, 250, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -88, 250, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 128, -48, 250, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 32, 250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -32, 40, 250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -48, 1143, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 8, 1253, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -40, 1150, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -8, 1175, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -88, 934, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -40, 999, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, 8, 932, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -40, 1044, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, 8, 1011, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 8, 1116, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -40, 1208, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -40, 1046, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -40, 1090, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -24, 1183, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -80, -120, 940, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -128, -120, 917, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, -88, 932, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -88, 921, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -112, -72, 1008, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, -88, 1024, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -72, 1083, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -88, 1250, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -72, 1312, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -56, 1325, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -40, 1375, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -24, 1500, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -16, 1500, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -8, 1500, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -88, 1250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -72, 1250, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -56, 1312, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -40, 1325, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -24, 1337, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -16, 1375, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, -8, 1375, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 0, 1500, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -88, 1250, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1250, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -56, 1250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -40, 1325, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 1375, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -8, 1375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -96, 88, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 80, 500, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 72, 500, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, 64, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 24, 56, 500, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 56, 48, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 40, 500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 32, 500, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 40, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 80, 88, 500, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 500, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80186A90[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 42, 0, 0, { 2, 0 } },
    { 52, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80186AB8[91] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 375, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 72, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, 80, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 80, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 80, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 72, 250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 250, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 80, 250, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 250, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 156, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 742, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -40, 750, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 689, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 843, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -64, 547, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -64, 541, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -64, 589, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 598, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -40, 645, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -40, 723, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -40, 686, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 751, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -40, 576, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 56, 549, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 24, 460, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 384, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 0, 386, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -8, 354, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -24, 301, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -40, 295, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 293, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -64, 276, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 261, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -104, 247, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 299, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -120, 258, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -104, 267, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, -120, 325, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -120, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -24, 641, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -24, 748, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -32, 698, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -48, 649, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -64, 866, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -88, 919, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -120, 827, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, -120, 492, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -88, 564, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -96, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -120, 423, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -120, 453, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -120, 490, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 246, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -48, 259, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -32, 271, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -16, 272, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -8, 307, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 8, 328, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -16, 239, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 252, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 32, 324, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 260, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 64, 262, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 625, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 615, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -24, 625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -48, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 625, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -80, 625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, -96, 625, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 731, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -64, 625, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -72, 625, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 358, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 32, 386, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 48, 420, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 448, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 492, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 64, 613, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 290, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_801871D4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 13, 0, 0, { 2, 0 } },
    { 32, 59, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_801871FC[23] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 16, 625, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -8, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 72, 40, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 8, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 56, 500, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, -8, 500, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, 40, 500, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 0, 40, 500, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, -16, 500, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -32, 16, 625, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -32, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -64, -40, 975, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, 0, 975, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -128, -32, 1175, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -40, 1125, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -8, 1125, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -80, 1550, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -32, 1550, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -48, -80, 1550, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -32, 1550, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, 8, 950, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 8, 987, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 40, 1050, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_801873C8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_saloon_g_r_801873F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80187400[13] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 72, 525, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 88, 500, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 64, 525, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 80, 500, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 525, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 500, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 0, 350, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, -24, 362, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -56, 375, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 387, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -88, 387, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80187504[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 1, 0, 0, { 2, 0 } },
    { 8, 5, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_8018752C[119] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 130, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 88, 130, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 72, 130, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 88, 130, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 88, 135, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 80, 149, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 137, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 80, 135, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 32, 152, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 144, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 56, 141, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 40, 132, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 56, 132, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 40, 129, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 129, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 56, 139, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -8, 140, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 144, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 140, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 142, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 24, 140, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 737, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 140, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 40, 140, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 40, 142, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 139, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 72, 140, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 140, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 72, 141, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 88, 141, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 88, 142, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 154, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 64, 150, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 151, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 292, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 159, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 72, 156, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 56, 236, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 32, 152, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 48, 146, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 147, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 148, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 96, 148, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 72, 191, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 152, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 48, 146, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 64, 146, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 80, 148, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 147, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 150, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 151, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 169, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 152, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 24, 152, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 152, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 48, 156, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 64, 157, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 148, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 64, 146, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 146, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 149, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 149, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 40, 149, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 56, 144, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 72, 144, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 926, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 32, 1258, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 149, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 64, 146, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 56, 153, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 155, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, 40, 300, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, 40, 300, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -16, 40, 300, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, 40, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 48, 40, 300, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 40, 250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 40, 250, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 129, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 96, 130, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 112, 131, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 80, 132, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 96, 133, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 133, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 141, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 96, 141, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 112, 141, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 80, 183, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 96, 156, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 112, 144, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 139, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 140, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 144, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 80, 150, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 96, 150, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 112, 144, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 152, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 152, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 112, 144, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 153, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 96, 154, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 112, 144, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 80, 147, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 96, 148, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 112, 144, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 80, 183, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 96, 159, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 112, 141, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 80, 150, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 96, 151, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 145, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 80, 145, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 96, 146, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 112, 145, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 148, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 96, 150, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 112, 145, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 80, 177, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 104, 150, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80187E78[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 1, 0 } },
    { 71, 48, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_saloon_g_r_80187E98[12] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -96, 56, 612, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 56, 800, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 8, 800, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, 0, 800, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, 56, 800, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 8, 800, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 56, 800, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 8, 800, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 56, 800, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 800, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 40, 800, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 72, 800, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80187F88[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80187FA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_saloon_g_r_80187FB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_saloon_g_r_80187FC8[13] = {
    { { .empty = D_dryfield_night_saloon_g_r_80185D48 }, D_dryfield_night_saloon_g_r_80185D48, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185D58 }, D_dryfield_night_saloon_g_r_80185EAC, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185EC4 }, D_dryfield_night_saloon_g_r_80185FDC, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185FFC }, D_dryfield_night_saloon_g_r_80186574, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_801865A4 }, D_dryfield_night_saloon_g_r_80186A90, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80186AB8 }, D_dryfield_night_saloon_g_r_801871D4, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_801871FC }, D_dryfield_night_saloon_g_r_801873C8, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_801873F0 }, D_dryfield_night_saloon_g_r_801873F0, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80187400 }, D_dryfield_night_saloon_g_r_80187504, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_8018752C }, D_dryfield_night_saloon_g_r_80187E78, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80187E98 }, D_dryfield_night_saloon_g_r_80187F88, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_80187FA8 }, D_dryfield_night_saloon_g_r_80187FA8, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_80187FB8 }, D_dryfield_night_saloon_g_r_80187FB8, NULL },
};

GpPointLight D_dryfield_night_saloon_g_r_80188064[7] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1392, 1556, 1720, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 940, -1500, 5040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3768, 3522, 2703, { 0, 0 } }, 1000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1850, -1500, -3210 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 2621, { 0, 0 } }, 1500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3480, -1500, -3530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3932, 4096, { 0, 0 } }, 1500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3780, -1600, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3112, 2457, { 0, 0 } }, 2500, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4750, -1400, 4320 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3686, 3686, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2490, -1000, 3100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3276, 2375, { 0, 0 } }, 1000, 3000 },
};

GpRoomCoordSet D_dryfield_night_saloon_g_r_80188304[1] = {
    { 0, NULL, 7, D_dryfield_night_saloon_g_r_80188064, 0, NULL },
};

GpObj4C D_dryfield_night_saloon_g_r_8018831C[16] = {
    { NULL, NULL, NULL, { 2633, -1376, -1224, 0 }, { { -2531, -2400, -372, 0 }, { 2524, -2400, 366, 0 }, { -2531, 2400, -372, 0 }, { 2524, 2400, 366, 0 } }, { 592, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 3500, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 2782, -1248, -1346, 0 }, { { 2519, -2272, 364, 0 }, { -2534, -2272, -375, 0 }, { 2519, 2272, 364, 0 }, { -2534, 2272, -375, 0 } }, { -594, 0, 4058, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1685, -1152, -59, 0 }, { { 1038, -2176, 153, 0 }, { -1037, -2176, -153, 0 }, { 1038, 2176, 153, 0 }, { -1037, 2176, -153, 0 } }, { -603, 0, 4071, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1659, -1280, 60, 0 }, { { -1102, -2304, -152, 0 }, { 1103, -2304, 152, 0 }, { -1102, 2304, -152, 0 }, { 1103, 2304, 152, 0 } }, { 561, 0, -4072, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 1801, -1520, 2996, 0 }, { { -878, -2544, -853, 0 }, { 878, -2544, 853, 0 }, { -878, 2544, -853, 0 }, { 878, 2544, 853, 0 } }, { 2854, 0, -2940, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -1185, -1632, 3294, 0 }, { { -299, -2656, 1167, 0 }, { 295, -2656, -1172, 0 }, { -299, 2656, 1167, 0 }, { 295, 2656, -1172, 0 } }, { -3977, 0, -1011, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1824, -1456, 2896, 0 }, { { 698, -2480, 685, 0 }, { -711, -2480, -703, 0 }, { 698, 2480, 685, 0 }, { -711, 2480, -703, 0 } }, { -2884, 0, 2926, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1377, -1344, 3391, 0 }, { { 300, -2368, -1106, 0 }, { -309, -2368, 1096, 0 }, { 300, 2368, -1106, 0 }, { -309, 2368, 1096, 0 } }, { 3955, 0, 1093, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 431, -1552, 5183, 0 }, { { -1068, -2576, -9, 0 }, { 1069, -2576, 10, 0 }, { -1068, 2576, -9, 0 }, { 1069, 2576, 10, 0 } }, { 35, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 383, -1376, 5055, 0 }, { { 1069, -2400, 0, 0 }, { -1069, -2400, 0, 0 }, { 1069, 2400, 0, 0 }, { -1069, 2400, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 2906, -1200, 4312, 0 }, { { 1, -2224, -1068, 0 }, { -1, -2224, 1068, 0 }, { 1, 2224, -1068, 0 }, { -1, 2224, 1068, 0 } }, { 4096, 0, 3, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 5, 1, 0 },
    { NULL, NULL, NULL, { 3066, -1216, 4376, 0 }, { { -1, -2240, 1068, 0 }, { 1, -2240, -1068, 0 }, { -1, 2240, 1068, 0 }, { 1, 2240, -1068, 0 } }, { -4104, 0, -6, 0 }, { 0, 0, 4096, 0 }, 2468, 0, 5, 7, 1, 0 },
    { NULL, NULL, NULL, { 158, -1216, -3715, 0 }, { { 4, -2272, -2047, 0 }, { -13, -2272, 2033, 0 }, { 4, 2272, -2047, 0 }, { -13, 2272, 2033, 0 } }, { 4099, 0, 16, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 303, -1216, -3729, 0 }, { { 11, -2272, 1935, 0 }, { -21, -2272, -1950, 0 }, { 11, 2272, 1935, 0 }, { -21, 2272, -1950, 0 } }, { -4121, 0, 33, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -2128, -1248, -193, 0 }, { { -1157, -2304, -193, 0 }, { 1157, -2304, 194, 0 }, { -1157, 2304, -193, 0 }, { 1157, 2304, 194, 0 } }, { 675, 0, -4046, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2144, -1280, -353, 0 }, { { 1157, -2304, 194, 0 }, { -1157, -2304, -193, 0 }, { 1157, 2304, 194, 0 }, { -1157, 2304, -193, 0 } }, { -678, 0, 4044, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 4, 3, 129, 0 },
};

GpObj4C D_dryfield_night_saloon_g_r_801887DC[21] = {
    { NULL, NULL, NULL, { 2464, -48, -4720, 0 }, { { 624, 0, -272, 0 }, { 624, 0, 272, 0 }, { -624, 0, -272, 0 }, { -624, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 680, 0, 15, 20, 2, 0 },
    { NULL, NULL, NULL, { 4480, -50, 4368, 0 }, { { -448, 0, -624, 0 }, { 448, 0, -624, 0 }, { -448, 0, 624, 0 }, { 448, 0, 624, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 768, 0, 19, 33, 2, 0 },
    { NULL, NULL, NULL, { -3232, -64, 3392, 0 }, { { 624, 0, -880, 0 }, { 624, 0, 880, 0 }, { -624, 0, -880, 0 }, { -624, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1078, 0x4002, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { -32, -64, 1728, 0 }, { { 2032, 0, -1392, 0 }, { 2032, 0, 1392, 0 }, { -2032, 0, -1392, 0 }, { -2032, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4076, 0, 401, 0 }, 2455, 2, 5, 0, 4, 0 },
    { NULL, NULL, NULL, { 4048, -64, -4672, 0 }, { { 896, 0, -848, 0 }, { 896, 0, 848, 0 }, { -896, 0, -848, 0 }, { -896, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { -1380, 0, 3856, 0 }, 1227, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 544, -64, 6432, 0 }, { { 464, 0, -272, 0 }, { 464, 0, 272, 0 }, { -464, 0, -272, 0 }, { -464, 0, 272, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 535, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1472, -64, -1152, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, 0x8005, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { -1632, -64, 288, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, 0x8005, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { 4560, -64, 3456, 0 }, { { 1088, 0, -272, 0 }, { 1088, 0, 272, 0 }, { -1088, 0, -272, 0 }, { -1088, 0, 272, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1115, 0x8005, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { 3424, -64, 4384, 0 }, { { 384, 0, -976, 0 }, { 384, 0, 976, 0 }, { -384, 0, -976, 0 }, { -384, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1047, 0x8005, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { 4688, -64, 5040, 0 }, { { 928, 0, -320, 0 }, { 928, 0, 320, 0 }, { -928, 0, -320, 0 }, { -928, 0, 320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 981, 0x8005, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { 2080, -64, 1328, 0 }, { { 880, 0, -1056, 0 }, { 880, 0, 1056, 0 }, { -880, 0, -1056, 0 }, { -880, 0, 1056, 0 } }, { 0, 4104, 0, 0 }, { -4092, 0, -202, 0 }, 1372, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 3968, -64, 1408, 0 }, { { 704, 0, -991, 0 }, { 704, 0, 992, 0 }, { -704, 0, -991, 0 }, { -704, 0, 992, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, 600, 0 }, 1214, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 224, -64, 4176, 0 }, { { 2768, 0, -1024, 0 }, { 2768, 0, 1024, 0 }, { -2768, 0, -1024, 0 }, { -2768, 0, 1024, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 2941, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 320, -64, 5648, 0 }, { { 1232, 0, -464, 0 }, { 1232, 0, 464, 0 }, { -1232, 0, -464, 0 }, { -1232, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1311, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { -2336, -64, 3552, 0 }, { { 1296, 0, -336, 0 }, { 1296, 0, 688, 0 }, { -1296, 0, -688, 0 }, { -1296, 0, 336, 0 } }, { 0, 4105, 0, 0 }, { 601, 0, -4052, 0 }, 1465, 2, 18, 0, 2, 0 },
    { NULL, NULL, NULL, { 4912, -64, 560, 0 }, { { 848, 0, -1216, 0 }, { 848, 0, 1216, 0 }, { -848, 0, -1216, 0 }, { -848, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1481, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 4352, -64, 192, 0 }, { { 848, 0, -528, 0 }, { 848, 0, 528, 0 }, { -848, 0, -528, 0 }, { -848, 0, 528, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 997, 2, 14, 0, 2, 0 },
    { NULL, NULL, NULL, { 3936, -64, -2144, 0 }, { { 624, 0, -720, 0 }, { 624, 0, 720, 0 }, { -624, 0, -720, 0 }, { -624, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 951, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 4896, -64, 2544, 0 }, { { 848, 0, -848, 0 }, { 848, 0, 848, 0 }, { -848, 0, -848, 0 }, { -848, 0, 848, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1193, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 3904, -64, 1664, 0 }, { { 704, 0, -1983, 0 }, { 704, 0, 1984, 0 }, { -704, 0, -1983, 0 }, { -704, 0, 1984, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 2095, 2, 14, 0, 130, 0 },
};

GpObj3A D_dryfield_night_saloon_g_r_80188E18[2] = {
    { NULL, NULL, { -1056, -1392, 5216, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, { -118, 9 }, 1, 0 },
    { NULL, NULL, { 2048, -1376, 5215, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, { -118, 9 }, 129, 0 },
};

GpAreaTmdRec D_dryfield_night_saloon_g_r_80188E90[2] = {
    { 140, 356, 0, 0, { 0, 0 }, D_8013B0C4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_saloon_g_r_80188EA8[2] = {
    { 40, 40, 0, 0, { 0, 0 }, D_8013E500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_saloon_g_r_80188EC0[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 40, 40, 1, 0, { 0, 0 }, D_80156500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_saloon_g_r_80188EE4[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017BE58, D_dryfield_night_saloon_g_r_80188E90 },
    { D_map_dryfield_full_8017BE78, D_dryfield_night_saloon_g_r_80188EA8 },
    { D_map_dryfield_full_8017BEA8, D_dryfield_night_saloon_g_r_80188EC0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_night_saloon_g_r_80188F4C[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_night_saloon_g_r_80188F58[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_night_saloon_g_r_80188F64[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_saloon_g_r_80188F6C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_saloon_g_r_80188F4C },
};

GpRoomParamRec D_dryfield_night_saloon_g_r_80188F74[1] = {
    { 0, 0, 1, 0, D_dryfield_night_saloon_g_r_80188F58 },
};

GpRoomParamRec D_dryfield_night_saloon_g_r_80188F7C[1] = {
    { 0, 1, 0, 0, D_dryfield_night_saloon_g_r_80188F58 },
};

GpRoomParamRec* D_dryfield_night_saloon_g_r_80188F84[8] = {
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F6C,
    D_dryfield_night_saloon_g_r_80188F74,
    D_dryfield_night_saloon_g_r_80188F7C,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
};

DryfieldNightSaloonGRStorage8FA4 D_dryfield_night_saloon_g_r_80188FA4 = { 0 };

RoomEventMsg D_dryfield_night_saloon_g_r_80188FAC = { 0 };

u8 D_dryfield_night_saloon_g_r_80188FB4[4] = {
    0,
    16,
    15,
    0,
};

RoomEventReq D_dryfield_night_saloon_g_r_80188FB8;

static s32 func_dryfield_night_saloon_g_r_8017D8A0(RoomEventReq* req, RoomEventMsg* msg);

/// Event gate for the room's exit. Returns 1 when game-flag nibble
/// `req->flagId` already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`.
static s32 func_dryfield_night_saloon_g_r_8017D8A0(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                    = req->flagId;
    D_dryfield_night_saloon_g_r_80188FB4[0] = 0;
    neg                                     = flag < 0;
    got                                     = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_night_saloon_g_r_80188FAC = *msg;
                D_dryfield_night_saloon_g_r_80188FB8 = *req;
                id                                   = req->flagId;
                mode                                 = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_saloon_g_r_8017F90C, 0, 0, 0);
                D_dryfield_night_saloon_g_r_80188FB4[0] = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_night_saloon_g_r_8017DA04(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_saloon_g_r_80188FB8.field_0);
            if (D_dryfield_night_saloon_g_r_80188FB8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_saloon_g_r_80188FB8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_saloon_g_r_80188FB8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_saloon_g_r_80188FB8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_saloon_g_r_80188FB8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_saloon_g_r_80188FB8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant         = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_saloon_g_r_80188FAC.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_saloon_g_r_80188FAC.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_night_saloon_g_r_80188FAC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_night_saloon_g_r_8017E050`: the entry tick
/// `func_dryfield_night_saloon_g_r_8017DF90`, the idle state
/// `func_dryfield_night_saloon_g_r_8017E040`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_saloon_g_r_8017D5DC = {
    { func_dryfield_night_saloon_g_r_8017DF90, func_dryfield_night_saloon_g_r_8017E040, taskKill },
};
/// Room cutscene task: case 0 saves the area id, forces `Mc_SaveData[0].state.at4.loc.view`
/// to 0xC, raises the script halt flags and starts cap command 0x13; the
/// following states wait for the cap to go idle, then start the jukebox task,
/// and case 4 restores the area id and kills the task.
void func_dryfield_night_saloon_g_r_8017DB74(Task* task)
{
    McSaveData* save;
    u8          temp;

    switch (task->state) {
        case 0:
            gGameSession->eventState                   = 1;
            gGameSession->hideHud                      = 1;
            Gp_StateF0.field_4                         = 2;
            save                                       = &Mc_SaveData[0];
            temp                                       = save->state.at4.loc.view;
            save->state.at4.loc.view                   = 0xC;
            D_dryfield_night_saloon_g_r_80188FA4.value = temp;
            Gp_MsgPlayer3F3(0);
            Gp_RunCapCmd(0x13, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            func_dryfield_night_saloon_g_r_8017E698(0);
            task->state = task->state + 1;
            return;
        case 3:
            task->state = task->state + 1;
            return;
        case 4:
            gGameSession->eventState          = 0;
            gGameSession->hideHud             = 0;
            D_80114D08                        = 0xA;
            Gp_StateF0.field_4                = 0;
            Mc_SaveData[0].state.at4.loc.view = D_dryfield_night_saloon_g_r_80188FA4.value;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Numbered names the room lists. Nothing in the code refers to them: the
/// room's data holds them in records that pair each with a small number.
const char D_dryfield_night_saloon_g_r_8017D600[] = "3. Heaven-sent Killer";
const char D_dryfield_night_saloon_g_r_8017D618[] = "2. Eager For Blood";
const char D_dryfield_night_saloon_g_r_8017D62C[] = "1. Crazy King";
const char D_dryfield_night_saloon_g_r_8017D63C[] = "3. Crawling Waste Emperor";
const char D_dryfield_night_saloon_g_r_8017D658[] = "2. Pick Up The Gauntlet";
const char D_dryfield_night_saloon_g_r_8017D670[] = "3. Hunter's Moon";
const char D_dryfield_night_saloon_g_r_8017D684[] = "2. Quadrumanous Leader";
const char D_dryfield_night_saloon_g_r_8017D69C[] = "3. Genic Reactor";
const char D_dryfield_night_saloon_g_r_8017D6B0[] = "2. Rebel Forces";
const char D_dryfield_night_saloon_g_r_8017D6C0[] = "1. Yellow Rain";
const char D_dryfield_night_saloon_g_r_8017D6D0[] = "3. Pick Up The Gauntlet";
const char D_dryfield_night_saloon_g_r_8017D6E8[] = "2. Ambush!";
const char D_dryfield_night_saloon_g_r_8017D6F4[] = "4. Dark Voice Of The Heart";
const char D_dryfield_night_saloon_g_r_8017D710[] = "3. Requiem";
const char D_dryfield_night_saloon_g_r_8017D71C[] = "2. Rock And Fire";
const char D_dryfield_night_saloon_g_r_8017D730[] = "1. Ghost Town";
const char D_dryfield_night_saloon_g_r_8017D740[] = "4. Snooper";
const char D_dryfield_night_saloon_g_r_8017D74C[] = "3. Wild Hunt";
const char D_dryfield_night_saloon_g_r_8017D75C[] = "2. Lightning Operation";
const char D_dryfield_night_saloon_g_r_8017D774[] = "1. L.A. Maze";
const char D_dryfield_night_saloon_g_r_8017D784[] = "4. Genic Reactor";
const char D_dryfield_night_saloon_g_r_8017D798[] = "3. Mental Agony";
const char D_dryfield_night_saloon_g_r_8017D7A8[] = "2. Wishwash";
const char D_dryfield_night_saloon_g_r_8017D7B4[] = "1. Curse Your Fate!";
const char D_dryfield_night_saloon_g_r_8017D7C8[] = "4. Killing Field";
const char D_dryfield_night_saloon_g_r_8017D7DC[] = "3. Fool's Paradise";
const char D_dryfield_night_saloon_g_r_8017D7F0[] = "2. Gazing into The Void";
const char D_dryfield_night_saloon_g_r_8017D808[] = "1. Man Made Nature";
const char D_dryfield_night_saloon_g_r_8017D81C[] = "4. Lazing Away the Morning";
const char D_dryfield_night_saloon_g_r_8017D838[] = "3. Out Of Phase 2";
const char D_dryfield_night_saloon_g_r_8017D84C[] = "2. The Vagrants";
const char D_dryfield_night_saloon_g_r_8017D85C[] = "1. Tower Rendezvous";

/// The jukebox's ten track lists: one per game mode, with list 4 standing in
/// before the first clear, and the second five used outside the debug attach
/// room.
static const RoomsShared8018055cMenu D_dryfield_night_saloon_g_r_8017D870 = {
    {
        D_dryfield_night_saloon_g_r_80184F0C,
        D_dryfield_night_saloon_g_r_80184F24,
        D_dryfield_night_saloon_g_r_80184F3C,
        D_dryfield_night_saloon_g_r_80184F54,
        D_dryfield_night_saloon_g_r_80184F6C,
        D_dryfield_night_saloon_g_r_80184F84,
        D_dryfield_night_saloon_g_r_80184FA4,
        D_dryfield_night_saloon_g_r_80184FC4,
        D_dryfield_night_saloon_g_r_80184FE4,
        D_dryfield_night_saloon_g_r_80185004,
    },
};

/// "SELECT", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_night_saloon_g_r_8017D898[8] = "SELECT\0\x0D";
/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0xF picks the destination
/// room from game-flag nibble 0x61 (unless `in->field_5` asks for a dry run),
/// then passes the warp through the event gate with the room's own request -
/// nibble 0x35, no item, cap command 2 and two stage sound ids. Any other area
/// answers 1.
s32 func_dryfield_night_saloon_g_r_8017DCA4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;

    *out  = *in;
    msgId = in->prefix.packed;
    if (msgId == 0xF) {
        if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
        if (in->prefix.packed == msgId) {
            req.field_0 = 2;
            req.field_4 = 2;
            req.field_8 = Gp_PackStageSndId(0x52120005);
            req.field_C = Gp_PackStageSndId(0x52120003);
            req.flagId  = 0x35;
            req.itemId  = 0;
            return func_dryfield_night_saloon_g_r_8017D8A0(&req, in);
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_night_saloon_g_r_8017DD7C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_saloon_g_r_8017DD84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case 8:
            if (GameFlag_GetNibble(0x5A) == 0) {
                func_800E8614(D_dryfield_night_saloon_g_r_801848DC, 0);
                GameFlag_SetNibble(0x5A, 1);
            } else if (GameFlag_GetNibble(0x5A) == 1) {
                func_800E8614(D_dryfield_night_saloon_g_r_80184B34, 0);
            }
            break;
        case 10:
            if (GameFlag_GetNibble(0x5A) < 2) {
                func_800E8614(D_dryfield_night_saloon_g_r_80184D2C, 0);
            }
            break;
    }
    return 0;
}

/// Handler for this room's script entry 0x13EF, whose `GpMsg13EF` payload
/// arrives as `arg2`. `field_2 == 7` plays the room's first-visit cutscene
/// once (nibble 0x59). Then, in session phase 2 with nibble 0xB0 still clear,
/// `field_2 == 1` unlinks the room's 4A object and queues sound 0x5312000C,
/// while the room's own phase (`field_2 == 2`) announces the visit to the
/// slot-4 task with message 0x7DA carrying the session's two id bytes and a
/// non-zero action halfword, and sets nibble 0xB0. Always returns 0.
s32 func_dryfield_night_saloon_g_r_8017DE68(Task* task, s32 msgId, GpMsg13EF* arg2, GpMessageArg arg3)
{
    GpCmdArg msg;
    u8       temp_s0;

    if (arg2->field_2 == 7 && GameFlag_GetNibble(0x59) == 0) {
        func_800E8634(D_dryfield_night_saloon_g_r_80183C94, 0, D_dryfield_night_saloon_g_r_801847A4);
        GameFlag_SetNibble(0x59, 1);
    }
    temp_s0 = gGameSession->at4.loc.place;
    if (temp_s0 == 2 && GameFlag_GetNibble(0xB0) == 0) {
        if (arg2->field_2 == 1) {
            Gp_UnlinkObj4A(0, &D_dryfield_night_saloon_g_r_801887DC[13]);
            SndEvt_EnqueueType6(0x5312000C, 0, 0);
        } else if (arg2->field_2 == temp_s0) {
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
            GameFlag_SetNibble(0xB0, 1);
        }
    }
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::msgTable` -
/// the table whose 0x13EE entry is the room's own script task - register the
/// task in pointer slot 7, then, on the phase-2 visit whose nibble 0xB0 is
/// still clear, announce the room to the slot-4 task with message 0x7DA
/// carrying the session's two id bytes and a zero halfword. Then advance state.
static void func_dryfield_night_saloon_g_r_8017DF90(Task* task)
{
    GpCmdArg msg;

    task->msgTable = D_dryfield_night_saloon_g_r_8017F918;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 2 && GameFlag_GetNibble(0xB0) == 0) {
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 0;
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
    }
    task->state = task->state + 1;
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
static void func_dryfield_night_saloon_g_r_8017E040(Task* task)
{
    char pad[0x10];
}

/// The room task: copies the three-state table
/// `D_dryfield_night_saloon_g_r_8017D5DC` onto the stack and runs the entry
/// for the task's current state - the entry tick, the idle state, then
/// `taskKill`.
void func_dryfield_night_saloon_g_r_8017E050(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}

/// Cutscene script callback: stores `arg0` as the session's room and in the
/// main-executable byte `Mc_SaveData[0].state.at4.loc.room`.
void func_dryfield_night_saloon_g_r_8017E0A8(u8 arg0)
{
    Mc_SaveData[0].state.at4.loc.room = arg0;
    gGameSession->at4.loc.room        = arg0;
}

/// Row callback of the jukebox list: draws the row's track name, and on
/// confirm, when the row is not the one already chosen, plays the select
/// sound and, when the track differs from the one playing, fades the music
/// out and hands the track id to the menu task to load.
void func_dryfield_night_saloon_g_r_8017E0C0(UiList* prompt, UiObject* obj)
{
    RoomsShared8018055cMenu    menu;
    RoomsShared8018055cCourse* course;
    s32                        row;
    s32                        list;
    s32                        mode;

    row  = prompt->field_8;
    menu = D_dryfield_night_saloon_g_r_8017D870;

    list = 4;
    if (Mc_SaveData[0].state.clearCount != 0) {
        list = Mc_SaveData[0].state.gameMode;
    }
    if (Gp_IsDebugAttachRoom() == 0) {
        list += 5;
    }

    course              = &menu.lists[list][row];
    menu.req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    menu.req.y          = (prompt->field_1A - 3) + obj->panel.field_22.u;
    menu.req.otIndex    = obj->panel.field_14.s + 1;
    menu.req.field_8    = prompt->field_1C;
    menu.req.glyphTable = 4;
    menu.req.field_E    = 1;
    menu.req.centerMode = 0;
    Text_DrawString(&menu.req, course->name);

    mode = prompt->field_C;
    if (mode == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1.value != prompt->field_8) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                if (obj->owner->status != course->id) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state  = mode;
                    obj->owner->status = course->id;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1.value = prompt->field_8;
            }
        }
    }
}

/// The jukebox menu task. Draws the title and, on its first tick, lays out the
/// track list (four rows, three in the debug attach room). While a chosen
/// track is pending it waits for the MIDI player to go idle, queues the
/// track's CD load, then starts it once the CD is idle and records it as the
/// current track. The menu or cancel button plays the back sound and closes
/// the panel.
void func_dryfield_night_saloon_g_r_8017E28C(Task* task)
{
    u8        param1[8];
    u8        param2[8];
    UiObject* obj;
    UiList*   menu;
    u8        flags;
    s32       sent;
    s32       state;
    u8        ready;

    obj  = task->spawnArg2.pointer;
    menu = &D_dryfield_night_saloon_g_r_80185028;

    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_dryfield_night_saloon_g_r_8017D898);
    if (task->state == 0) {
        task->spawnArg1.value = -1;
        if (Gp_IsDebugAttachRoom() == 0) {
            menu->field_4 = 4;
        } else {
            menu->field_4 = 3;
        }
        if (menu->field_4 >= 0xB) {
            menu->field_5.u = 0xA;
        } else {
            menu->field_5.u = menu->field_4;
        }
        menu->field_10  = 0;
        menu->field_9.u = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->field_A = 1;
        Ui_SetListScrollFlag(menu, 1);
        obj->panel.bounds.unsignedRect.x = -((s16)obj->panel.bounds.unsignedRect.w / 2);
        obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
        if (Gp_IsDebugAttachRoom() == 0) {
            task->status = 0xFF;
        } else {
            task->status = 0xFE;
        }
        task->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    flags = task->status;
    if (flags < 0xF1) {
        state = task->state;
        if (state == 1) {
            if (Midi_IsBusy(0) == 0) {
                param1[3] = 0;
                param1[2] = 4;
                param1[0] = flags;
                param2[0] = state;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                sent = 1;
            } else {
                sent = 0;
            }
            if (sent == 1) {
                task->state += 1;
            }
        } else {
            if (CdCmd_IsIdle() & 0xFFFF) {
                SndEvt_EnqueueType1(flags, 0);
                SndEvt_EnqueueType5(flags, (u8)D_8007A396);
                ready          = 1;
                gStageRoomSong = flags;
            } else {
                ready = 0;
            }
            if (ready == 1) {
                task->state  = 1;
                task->status = 0xFF;
                if (Gp_IsDebugAttachRoom() == 0) {
                    gGameSession->flowFlags |= 3;
                }
                if (obj->panel.field_0.w != 1) {
                    obj->field_2E = 6;
                }
            }
        }
    }
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            if (task->status != 0xFE) {
                if (task->status == 0xFF) {
                    obj->field_2E = 6;
                } else {
                    Ui_SetState4(obj, obj->owner);
                    obj->panel.field_0.w = 0;
                }
            }
        }
    }
}

/// The jukebox task: opens the menu panel with the session's UI flag raised
/// and frame timing switched, waits for the panel to close, tears it down and,
/// ten frames later, restores timing, releases the primitive buffer and kills
/// itself.
void func_dryfield_night_saloon_g_r_8017E564(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_dryfield_night_saloon_g_r_8018504C, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

/// Starts the jukebox task and reports success. Its argument is unused;
/// `func_dryfield_night_saloon_g_r_8017DB74` (state 2) still passes one.
static s32 func_dryfield_night_saloon_g_r_8017E698(s32 arg0)
{
    Display_InitModeObj(&D_dryfield_night_saloon_g_r_80185068, 0, 0, 0);
    return 1;
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix, then draws every effect whose view mask includes the current view
/// `gGameSession->at4.loc.view`. Positions 0-5 and 20-22 are drawn with UV
/// column 0 and half-extent 0x200, 6-10 with column 1 and 0x1C0, and 23-27
/// with column 0 and 0x300; the two helpers in between take the model's coord.
void func_dryfield_night_saloon_g_r_8017E6C8(Task* arg0)
{
    GpCoord* coord;
    s32      mask;
    s32      i;

    coord = arg0->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    for (i = 0; i < 6; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 6; i < 11; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 1, 0x1C0);
        }
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[12]) {
        func_dryfield_night_saloon_g_r_8017EB38(coord);
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[11]) {
        func_dryfield_night_saloon_g_r_8017F0A4(coord, D_dryfield_night_saloon_g_r_801850DC,
                                                D_dryfield_night_saloon_g_r_801850DC - 1, 0x100);
    }
    for (i = 20; i < 23; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 23; i < 28; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x300);
        }
    }
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// it projects, queues one semi-transparent `POLY_FT4` centred on it: tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380` and the 40-texel texture column
/// `arg1`. `arg2` is a signed half-extent scaled by depth; the grey level
/// follows the frame counter's low bit.
static void func_dryfield_night_saloon_g_r_8017E8B0(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`;
/// each root's tip lies at four times its offset to a later entry (15 and 18
/// for the first shaft, 16 and 19 for the second). All four corners are
/// moved to world space through `coord->workm` and projected through
/// `GsWSMATRIX`. The roots take a grey of 0x20 or 0x30 depending on the
/// parity of `gDisplayState.animFrame` and the tips are black, so the shaft
/// fades outward. The quad is sorted by `tipB`'s `otz` and skipped when that
/// is below 0x11.
static void func_dryfield_night_saloon_g_r_8017EB38(GpCoord* coord)
{
    u8*                    head;
    RoomLightShaftScratch* block;
    POLY_G4*               prim;
    SVECTOR*               dirA;
    SVECTOR*               dirB;
    s32                    i;
    s32                    j;
    s32                    rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (RoomLightShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850E4);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootA);
    (u16) block->rootA.vx = (u16)block->rootA.vx + (u16)coord->workm.t[0];
    (u16) block->rootA.vy = (u16)block->rootA.vy + (u16)coord->workm.t[1];
    (u16) block->rootA.vz = (u16)block->rootA.vz + (u16)coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850FC);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootB);
    (u16) block->rootB.vx = (u16)block->rootB.vx + (u16)coord->workm.t[0];
    (u16) block->rootB.vy = (u16)block->rootB.vy + (u16)coord->workm.t[1];
    (u16) block->rootB.vz = (u16)block->rootB.vz + (u16)coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                    = i + 15;
        dirA                 = &D_dryfield_night_saloon_g_r_80185074[j];
        (u16) block->tipA.vx = (u16)D_dryfield_night_saloon_g_r_80185074[14].vx +
                               ((u16)dirA->vx - (u16)D_dryfield_night_saloon_g_r_80185074[14].vx) * 4;
        (u16) block->tipA.vy = (u16)D_dryfield_night_saloon_g_r_80185074[14].vy +
                               ((u16)dirA->vy - (u16)D_dryfield_night_saloon_g_r_80185074[14].vy) * 4;
        (u16) block->tipA.vz = (u16)D_dryfield_night_saloon_g_r_80185074[14].vz +
                               ((u16)dirA->vz - (u16)D_dryfield_night_saloon_g_r_80185074[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        (u16) block->tipA.vx = (u16)block->tipA.vx + (u16)coord->workm.t[0];
        (u16) block->tipA.vy = (u16)block->tipA.vy + (u16)coord->workm.t[1];
        (u16) block->tipA.vz = (u16)block->tipA.vz + (u16)coord->workm.t[2];

        j                    = i + 18;
        dirB                 = &D_dryfield_night_saloon_g_r_80185074[j];
        (u16) block->tipB.vx = (u16)D_dryfield_night_saloon_g_r_80185074[17].vx +
                               ((u16)dirB->vx - (u16)D_dryfield_night_saloon_g_r_80185074[17].vx) * 4;
        (u16) block->tipB.vy = (u16)D_dryfield_night_saloon_g_r_80185074[17].vy +
                               ((u16)dirB->vy - (u16)D_dryfield_night_saloon_g_r_80185074[17].vy) * 4;
        (u16) block->tipB.vz = (u16)D_dryfield_night_saloon_g_r_80185074[17].vz +
                               ((u16)dirB->vz - (u16)D_dryfield_night_saloon_g_r_80185074[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        (u16) block->tipB.vx = (u16)block->tipB.vx + (u16)coord->workm.t[0];
        (u16) block->tipB.vy = (u16)block->tipB.vy + (u16)coord->workm.t[1];
        (u16) block->tipB.vz = (u16)block->tipB.vz + (u16)coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->rootA);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->rootB, &((RoomLightShaftScratch*)(head - 0x24))->tipA,
                 &((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x24);
}

/// Draws a tapered beam between two points of `coord`'s local space. `arg1`
/// and `arg2` are rotated by `coord->workm` and offset by its translation,
/// then projected through `GsWSMATRIX`; nothing is drawn unless the far end's
/// `otz` is at least 0x11; the near end's `otz` is not clamped. The two ends
/// get screen radii `(s16)arg3 * 64 / otz`.
///
/// Each quarter-turn step of an angle running 0..0x800 queues three
/// `POLY_G4`s: a wedge around the near end, a quad joining the two ends, and a
/// wedge around the far end walked backwards from 0x1000. The centre vertices
/// take a grey of 0x20 or 0x30 depending on the parity of
/// `gDisplayState.animFrame`, the rim vertices are black. Each primitive goes
/// into the OT bucket of its own end's `otz` with a `Gp_AddTpageShift` tpage.
static void func_dryfield_night_saloon_g_r_8017F0A4(GpCoord* coord, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    (u16) block->vec0.vx = (u16)block->vec0.vx + (u16)coord->workm.t[0];
    (u16) block->vec0.vy = (u16)block->vec0.vy + (u16)coord->workm.t[1];
    (u16) block->vec0.vz = (u16)block->vec0.vz + (u16)coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    (u16) block->vec1.vx = (u16)block->vec1.vx + (u16)coord->workm.t[0];
    (u16) block->vec1.vy = (u16)block->vec1.vy + (u16)coord->workm.t[1];
    (u16) block->vec1.vz = (u16)block->vec1.vz + (u16)coord->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        extent    = (s16)arg3 * 64;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        block->r1 = extent / block->otz1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
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

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x28);
}
