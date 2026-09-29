#include "rooms/shelter_b1_sterilization_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_sterilization_room_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// The "%" suffix appended to a formatted percentage.
static u8 Telephone_Data_80181A78[];

/// UI descriptor of the help-line box the "Play Data" panels open beside their
/// lists.
static UiObjectDesc Telephone_Data_80181C90;

/// Task descriptor table used by the cutscene runner
/// `func_shelter_b1_sterilization_room_8017F550`, which spawns entry 1 and
/// waits on it while the cutscene plays. The room's event handler spawns
/// entry 0 with a `RoomCutsceneRec` as its argument.
extern TaskDesc D_shelter_b1_sterilization_room_80184E1C[];

/// A 0x18-byte message argument block passed to `Gp_DispatchMsg`; only its
/// stride is known.
typedef struct {
    u8 data[0x18];
} _ShelterB1SterilizationRoomMsg;

/// One destination: the view written to the session and the save, and the
/// index of the message block sent with it.
typedef struct {
    u8 view;
    s8 msg;
} _ShelterB1SterilizationRoomDest;

/// Row labels drawn by the statistics rows of `func_shelter_b1_sterilization_room_8017D794`.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Unit suffix appended to the counted rows.
static u8 Telephone_Data_80181A70[];

/// Help lines shown for each statistics row while it is selected.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Prompt lines drawn by the dialog handlers.
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// List state of the "Play Data" menu `func_shelter_b1_sterilization_room_8017EFE4` runs.
static UiList Telephone_Data_80181C44;

/// List state of the weapon- and PE-usage panels.
static UiList Telephone_Data_80181C6C;

/// UI descriptors the dialog handlers open on confirm.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// List state of the menu `func_shelter_b1_sterilization_room_8017EB2C` runs.
static UiList Telephone_Data_80181CF4;

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpMsg13EF*, s32);
        s32 (*call2)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call3)(s32, s32, s32);
    } handler;
} ShelterB1SterilizationRoomMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB1SterilizationRoomMessageEntry, 8);

extern ShelterB1SterilizationRoomMessageEntry D_shelter_b1_sterilization_room_80184E40[6];
extern TaskDesc                               D_shelter_b1_sterilization_room_80184E70;
extern s32                                    D_shelter_b1_sterilization_room_80184E7C;
extern s16                                    D_shelter_b1_sterilization_room_80184E80[3];
extern GpGridParams                           D_shelter_b1_sterilization_room_80184F28;

extern GpAnimArg                       D_shelter_b1_sterilization_room_80188624;
extern _ShelterB1SterilizationRoomMsg  D_shelter_b1_sterilization_room_80188668[];
extern _ShelterB1SterilizationRoomDest D_shelter_b1_sterilization_room_80188728[];

/// Area records `func_shelter_b1_sterilization_room_8017F550` applies when it
/// advances game flag nibble 0 from 2 to 3 in one particular view.

extern UiObjectDesc D_800611E4;

extern s32 D_80135AC0;
extern s32 D_80135D78;
extern s32 D_80136258;

#define TELEPHONE_TITLE_BYTES "Telephone\0\0 "
#include "../../shared/telephone.h"

static void func_shelter_b1_sterilization_room_8017FABC(Task* task);
static void func_shelter_b1_sterilization_room_80180340(s32 arg0);
static void func_shelter_b1_sterilization_room_80180464(Task* task);
static void func_shelter_b1_sterilization_room_8018049C(void);
static void func_shelter_b1_sterilization_room_80180570(GpCoord* coord, s16* arg1);
static void func_shelter_b1_sterilization_room_80180828(Task* task);
static void func_shelter_b1_sterilization_room_80181244(Task* task);
static void func_shelter_b1_sterilization_room_801812A0(Task* task);

static void func_shelter_b1_sterilization_room_80181308(s32 tpage, s16 arg1);

s32  func_shelter_b1_sterilization_room_8017FC78(Task*, s32, GpMsg13EF*, s32);
s32  func_shelter_b1_sterilization_room_8017FF80(s32, s32, s32);
s32  func_shelter_b1_sterilization_room_801803E4(void);
s32  func_shelter_b1_sterilization_room_801803EC(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b1_sterilization_room_80180430(s32, s32, s32);
void func_shelter_b1_sterilization_room_8017F550(Task*);
void func_shelter_b1_sterilization_room_80180188(Task*);
void func_shelter_b1_sterilization_room_801802B0(Task*);

extern GpAnimSet D_shelter_b1_sterilization_room_80185228;
extern GpAnimSet D_shelter_b1_sterilization_room_801853DC;
extern GpAnimSet D_shelter_b1_sterilization_room_80185AFC;
extern GpAnimSet D_shelter_b1_sterilization_room_80185C8C;
extern GpAnimSet D_shelter_b1_sterilization_room_80186910;
extern GpAnimSet D_shelter_b1_sterilization_room_80187E18;
extern GpAnimSet D_shelter_b1_sterilization_room_801884DC;
void             func_shelter_b1_sterilization_room_80180D74(Task*);
void             func_shelter_b1_sterilization_room_80180F74(Task*);
void             func_shelter_b1_sterilization_room_801811E0(Task*);

#include "../../shared/telephone_data.inc.c"

TmdBone D_shelter_b1_sterilization_room_80184834[3] = {
#include "assets/shelter_b1_sterilization_room_model_07838_skeleton.inc"
};

u32 D_shelter_b1_sterilization_room_801848A0[3] = {
#include "assets/shelter_b1_sterilization_room_model_07838_partVerts.inc"
};

SVECTOR D_shelter_b1_sterilization_room_801848AC[56] = {
#include "assets/shelter_b1_sterilization_room_model_07838_verts.inc"
};

SVECTOR D_shelter_b1_sterilization_room_80184A6C[6] = {
#include "assets/shelter_b1_sterilization_room_model_07838_normals.inc"
};

u32 D_shelter_b1_sterilization_room_80184A9C[215] = {
#include "assets/shelter_b1_sterilization_room_model_07838_stream.inc"
};

TmdSource D_shelter_b1_sterilization_room_80184DF8 = {
    0,
    1768,
    0,
    3,
    D_shelter_b1_sterilization_room_801848A0,
    D_shelter_b1_sterilization_room_801848AC,
    D_shelter_b1_sterilization_room_80184A6C,
    D_shelter_b1_sterilization_room_80184834,
    D_shelter_b1_sterilization_room_80184A9C,
};

TaskDesc D_shelter_b1_sterilization_room_80184E1C[3] = {
    { 0, 32, func_shelter_b1_sterilization_room_8017F550, { .model = NULL } },
    { 0, 32, func_shelter_b1_sterilization_room_801802B0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

ShelterB1SterilizationRoomMessageEntry D_shelter_b1_sterilization_room_80184E40[6] = {
    { 5102, { .call2 = func_shelter_b1_sterilization_room_801803EC } },
    { 5105, { .call0 = func_shelter_b1_sterilization_room_801803E4 } },
    { 5103, { .call1 = func_shelter_b1_sterilization_room_8017FC78 } },
    { 5104, { .call3 = func_shelter_b1_sterilization_room_8017FF80 } },
    { 5106, { .call3 = func_shelter_b1_sterilization_room_80180430 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_shelter_b1_sterilization_room_80184E70 = { 0, 192, func_shelter_b1_sterilization_room_80180188, { .model = NULL } };

s32 D_shelter_b1_sterilization_room_80184E7C = 0x11004;

s16 D_shelter_b1_sterilization_room_80184E80[3] = { 0 };

SVECTOR D_shelter_b1_sterilization_room_80184E88[4] = {
    { -3836, 0, 1436, 0 },
    { -1108, 0, -3943, 0 },
    { 4054, 0, 582, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_shelter_b1_sterilization_room_80184EA8[8] = {
    { -268, 500, 747, 0 },
    { -268, -932, 747, 0 },
    { -641, -932, -248, 0 },
    { -641, 500, -248, 0 },
    { 399, -932, -540, 0 },
    { 399, 500, -540, 0 },
    { 214, -932, 747, 0 },
    { 214, 500, 747, 0 },
};

GpGridFace D_shelter_b1_sterilization_room_80184EE8[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_shelter_b1_sterilization_room_80184F18[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16* D_shelter_b1_sterilization_room_80184F24[1] = {
    D_shelter_b1_sterilization_room_80184F18,
};

GpGridParams D_shelter_b1_sterilization_room_80184F28 = { NULL, D_shelter_b1_sterilization_room_80184E88, D_shelter_b1_sterilization_room_80184EA8, D_shelter_b1_sterilization_room_80184EE8, D_shelter_b1_sterilization_room_80184F24, 641, 540, 1, 1, 4000, 4 };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} ShelterB1SterilizationRoomPoseBank798C;

ShelterB1SterilizationRoomPoseBank798C D_shelter_b1_sterilization_room_80184F4C = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80184F94[46] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_8018504C[109] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_records.inc"
};

u16 D_shelter_b1_sterilization_room_80185200[20] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_80185228 = {
    D_shelter_b1_sterilization_room_8018504C,
    D_shelter_b1_sterilization_room_80185200,
    { NULL, D_shelter_b1_sterilization_room_80184F4C.words, NULL, NULL, D_shelter_b1_sterilization_room_80184F94, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} ShelterB1SterilizationRoomPoseBank7C90;

ShelterB1SterilizationRoomPoseBank7C90 D_shelter_b1_sterilization_room_80185250 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80185268[26] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_801852D0[57] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_records.inc"
};

u16 D_shelter_b1_sterilization_room_801853B4[20] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_801853DC = {
    D_shelter_b1_sterilization_room_801852D0,
    D_shelter_b1_sterilization_room_801853B4,
    { NULL, D_shelter_b1_sterilization_room_80185250.words, NULL, NULL, D_shelter_b1_sterilization_room_80185268, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} ShelterB1SterilizationRoomPoseBank7E44;

ShelterB1SterilizationRoomPoseBank7E44 D_shelter_b1_sterilization_room_80185404 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80185464[161] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_801856E8[251] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_records.inc"
};

u16 D_shelter_b1_sterilization_room_80185AD4[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_80185AFC = {
    D_shelter_b1_sterilization_room_801856E8,
    D_shelter_b1_sterilization_room_80185AD4,
    { NULL, D_shelter_b1_sterilization_room_80185404.words, NULL, NULL, D_shelter_b1_sterilization_room_80185464, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} ShelterB1SterilizationRoomPoseBank8564;

ShelterB1SterilizationRoomPoseBank8564 D_shelter_b1_sterilization_room_80185B24 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80185B3C[17] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_80185B80[57] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_records.inc"
};

u16 D_shelter_b1_sterilization_room_80185C64[20] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_80185C8C = {
    D_shelter_b1_sterilization_room_80185B80,
    D_shelter_b1_sterilization_room_80185C64,
    { NULL, D_shelter_b1_sterilization_room_80185B24.words, NULL, NULL, D_shelter_b1_sterilization_room_80185B3C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[31];
    GpPackedSvec words[93];
} ShelterB1SterilizationRoomPoseBank86F4;

ShelterB1SterilizationRoomPoseBank86F4 D_shelter_b1_sterilization_room_80185CB4 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_09350_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80185E28[308] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_801862F8[380] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_records.inc"
};

u16 D_shelter_b1_sterilization_room_801868E8[20] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_80186910 = {
    D_shelter_b1_sterilization_room_801862F8,
    D_shelter_b1_sterilization_room_801868E8,
    { NULL, D_shelter_b1_sterilization_room_80185CB4.words, NULL, NULL, D_shelter_b1_sterilization_room_80185E28, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[41];
    GpPackedSvec words[123];
} ShelterB1SterilizationRoomPoseBank9378;

ShelterB1SterilizationRoomPoseBank9378 D_shelter_b1_sterilization_room_80186938 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80186B24[549] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_801873B8[654] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_records.inc"
};

u16 D_shelter_b1_sterilization_room_80187DF0[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_80187E18 = {
    D_shelter_b1_sterilization_room_801873B8,
    D_shelter_b1_sterilization_room_80187DF0,
    { NULL, D_shelter_b1_sterilization_room_80186938.words, NULL, NULL, D_shelter_b1_sterilization_room_80186B24, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} ShelterB1SterilizationRoomPoseBankA880;

ShelterB1SterilizationRoomPoseBankA880 D_shelter_b1_sterilization_room_80187E40 = { .poses = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_bank1.inc"
                                                                                    } };

GpPackedSvec D_shelter_b1_sterilization_room_80187E70[151] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_bank4.inc"
};

GpAnimRec D_shelter_b1_sterilization_room_801880CC[250] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_records.inc"
};

u16 D_shelter_b1_sterilization_room_801884B4[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_indices.inc"
};

GpAnimSet D_shelter_b1_sterilization_room_801884DC = {
    D_shelter_b1_sterilization_room_801880CC,
    D_shelter_b1_sterilization_room_801884B4,
    { NULL, D_shelter_b1_sterilization_room_80187E40.words, NULL, NULL, D_shelter_b1_sterilization_room_80187E70, NULL, NULL, NULL },
};

TaskDesc D_shelter_b1_sterilization_room_80188504[9] = {
    { 0, 192, func_shelter_b1_sterilization_room_801811E0, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_801813A0, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_80180D74, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_801814FC, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_80181588, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_80180F74, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_80181634, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_801816E0, { .model = NULL } },
    { 0, 192, func_shelter_b1_sterilization_room_801817EC, { .model = NULL } },
};

GpAnimSet* D_shelter_b1_sterilization_room_80188570[8] = {
    NULL,
    &D_shelter_b1_sterilization_room_80185228,
    &D_shelter_b1_sterilization_room_801853DC,
    &D_shelter_b1_sterilization_room_80185AFC,
    &D_shelter_b1_sterilization_room_80185C8C,
    &D_shelter_b1_sterilization_room_80186910,
    &D_shelter_b1_sterilization_room_80187E18,
    &D_shelter_b1_sterilization_room_801884DC,
};

GpCopyArg D_shelter_b1_sterilization_room_80188590 = { { .sets = D_shelter_b1_sterilization_room_80188570 }, 8 };

GpAnimArg D_shelter_b1_sterilization_room_80188598 = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_shelter_b1_sterilization_room_801885AC = { { .index = 1 }, 48, 1, 30, 1 };

GpAnimArg D_shelter_b1_sterilization_room_801885C0 = { { .index = 1 }, 49, 1, 5, 1 };

GpAnimArg D_shelter_b1_sterilization_room_801885D4 = { { .index = 1 }, 50, 1, 5, 1 };

GpAnimArg D_shelter_b1_sterilization_room_801885E8 = { { .index = 1 }, 51, 0, 0, 1 };

GpAnimArg D_shelter_b1_sterilization_room_801885FC = { { .index = 1 }, 52, 0, 0, 0 };

GpAnimArg D_shelter_b1_sterilization_room_80188610 = { { .index = 1 }, 53, 0, 0, 0 };

GpAnimArg D_shelter_b1_sterilization_room_80188624 = { { .index = 1 }, 54, 1, 10, 0 };

GpXformArg D_shelter_b1_sterilization_room_80188638 = { { 5540, 0, 8600, 0 }, { 0, 2047, 0, 0 } };

GpXformArg D_shelter_b1_sterilization_room_80188650 = { { 5876, 0, 0x28D2, 0 }, { 0, 512, 0, 0 } };

_ShelterB1SterilizationRoomMsg D_shelter_b1_sterilization_room_80188668[8] = {
    { { 21, 19, 0, 0, 0, 0, 0, 0, 247, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 21, 19, 0, 0, 0, 0, 0, 0, 26, 41, 0, 0, 0, 0, 0, 0, 0, 0, 255, 7, 0, 0, 0, 0 } },
    { { 18, 8, 0, 0, 0, 0, 0, 0, 226, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 18, 8, 0, 0, 0, 0, 0, 0, 26, 41, 0, 0, 0, 0, 0, 0, 0, 0, 255, 7, 0, 0, 0, 0 } },
    { { 131, 19, 0, 0, 0, 0, 0, 0, 107, 20, 0, 0, 0, 0, 0, 0, 0, 0, 255, 7, 0, 0, 0, 0 } },
    { { 129, 8, 0, 0, 0, 0, 0, 0, 107, 20, 0, 0, 0, 0, 0, 0, 0, 0, 255, 7, 0, 0, 0, 0 } },
    { { 200, 7, 0, 0, 0, 0, 0, 0, 46, 45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 131, 19, 0, 0, 0, 0, 0, 0, 46, 45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

_ShelterB1SterilizationRoomDest D_shelter_b1_sterilization_room_80188728[8] = {
    { 9, 7 },
    { 5, 1 },
    { 8, 6 },
    { 7, 3 },
    { 4, 0 },
    { 3, 4 },
    { 6, 2 },
    { 3, 5 },
};

static void func_shelter_b1_sterilization_room_80180A2C(s32 shade);
static void func_shelter_b1_sterilization_room_80180BF0(s32 shade);

#include "../../shared/telephone.inc.c"

void func_shelter_b1_sterilization_room_8017EB2C(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

/// Runs a cutscene described by the `RoomCutsceneRec` in `spawnArg2`:
/// hides the HUD and holds the player, optionally switches the view and loads
/// a cap file, plays the cap slot while waiting on a spawned task that the
/// confirm or cancel button can cut short, then runs the follow-up cap
/// commands, restores the view and releases the player.
void func_shelter_b1_sterilization_room_8017F550(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_shelter_b1_sterilization_room_8018C33C = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694                        = Mc_SaveData[0].state.at4.loc.view;
                Mc_SaveData[0].state.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_shelter_b1_sterilization_room_8018C33C = Task_SpawnFromTable(D_shelter_b1_sterilization_room_80184E1C, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_shelter_b1_sterilization_room_8018C33C);
                task->state++;
            } else if (Task_PollKill(D_shelter_b1_sterilization_room_8018C33C, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// The three states of the room's main task, run by
/// `func_shelter_b1_sterilization_room_80180518`: set-up, the per-frame
/// handler and the kill.
static const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4 = {
    {
        func_shelter_b1_sterilization_room_8017FABC,
        func_shelter_b1_sterilization_room_80180464,
        taskKill,
    },
};

static void func_shelter_b1_sterilization_room_8017FABC(Task* task)
{
    Task* target;

    task->msgTable = D_shelter_b1_sterilization_room_80184E40;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 0) {
        GameFlag_SetNibble(0xF4, 3);
        Gp_ApplyAreaRecs(D_shelter_b1_sterilization_room_8018C334);
        if (gameGetPtrSlot(0xA) != NULL) {
            GameFlag_SetNibble(0x116, 1);
            GameFlag_SetNibble(0xEA, 2);
            GameFlag_SetNibble(0x4B, 8);
            func_800E8634(&D_80135D78, 0, &D_80136258);
            Gp_SetAreaObjId(&gGameSession->at4.loc, 6, 1);
        } else {
            GameFlag_SetNibble(0x116, 2);
            GameFlag_SetNibble(0xEA, 1);
            func_800E8634(&D_80135AC0, 0, &D_80136258);
        }
    }
    func_shelter_b1_sterilization_room_80180340(0);
    if (gGameSession->at4.loc.place == 5) {
        target = Gp_LookupSlot4(0);
        if (target != NULL) {
            Gp_DispatchMsgPtr(target, 0x7DB, &D_shelter_b1_sterilization_room_80184E7C, 0);
        }
    }
    if (GameFlag_GetNibble(0xEA) != 1) {
        (D_shelter_b1_sterilization_room_8018B8A8 + 22)[0].field_4A &= 0xBF;
    }
    if (gGameSession->at4.loc.place == 1) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 6, 0, 0);
    }
    task->state++;
}

s32 func_shelter_b1_sterilization_room_8017FC78(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    s32 cmd;
    s32 mask;
    s32 flags;

    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x76) == 0) {
                if (GameFlag_GetNibble(0x84) != 0) {
                    func_800E8634(D_shelter_b1_sterilization_room_8018873C, 0,
                                  D_shelter_b1_sterilization_room_80188AB4);
                    GameFlag_SetNibble(0x76, 1);
                }
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x76) == 1) {
                if (GameFlag_GetNibble(0x77) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 1, 0, 0);
                } else {
                    Gp_RunCapCmd1(0x17);
                }
            } else {
                Gp_RunCapCmd1(0x16);
            }
            break;
        case 5:
        case 8:
            switch (msg->field_2) {
                case 5:
                    cmd  = 5;
                    mask = 2;
                    break;
                case 8:
                    cmd  = 3;
                    mask = 8;
                    break;
                default:
                    cmd  = 0;
                    mask = 0xFF;
                    break;
            }
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                flags = GameFlag_GetNibble(0xF2);
                if (cmd != 0 && !(flags & mask)) {
                    Gp_RunCapCmd1(cmd);
                    GameFlag_SetNibble(0xF2, flags | mask);
                }
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 3:
        case 6:
        case 7:
        case 10:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 4, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                if (GameFlag_GetNibble(0x149) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 1, 0);
                } else {
                    if (GameFlag_GetNibble(0x150) == 0) {
                        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 2, 0);
                        GameFlag_SetNibble(0x150, 1);
                    }
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
                }
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (GameFlag_GetNibble(0x151) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 3, 0);
                    GameFlag_SetNibble(0x151, 1);
                }
            } else if (GameFlag_GetNibble(0x151) < 2) {
                if (GameFlag_GetNibble(0x14A) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 4, 0);
                } else {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 5, 0);
                }
                GameFlag_SetNibble(0x151, 2);
            }
            Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b1_sterilization_room_8017FF80(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x11) {
        if (GameFlag_GetNibble(0x161) == 0) {
            GameFlag_SetNibble(0x161, 1);
            Gp_SpawnIfCapIdle(0x18, 1);
            return;
        }
        D_shelter_b1_sterilization_room_8018C344.field_0  = 0x13;
        D_shelter_b1_sterilization_room_8018C344.field_1  = 1;
        D_shelter_b1_sterilization_room_8018C344.field_3  = 2;
        D_shelter_b1_sterilization_room_8018C344.field_2  = 0;
        D_shelter_b1_sterilization_room_8018C344.field_4  = 0x5410000C;
        D_shelter_b1_sterilization_room_8018C344.field_8  = 0x5410000F;
        D_shelter_b1_sterilization_room_8018C344.field_10 = 0x5410000D;
        D_shelter_b1_sterilization_room_8018C344.field_C  = 0x5410000E;
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80184E1C, 0, 6, &D_shelter_b1_sterilization_room_8018C344);
    }
    if (arg2 == 0x15) {
        Gp_RunCapCmd1(arg2);
    }
    if (arg2 == 4) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 0, 0);
    }
    if (arg2 == 0x13) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 1, 0);
    }
    if (arg2 == 0xE) {
        if (GameFlag_GetNibble(0x76) == 1 && GameFlag_GetNibble(0x77) == 0) {
            if (GameFlag_GetNibble(0x14F) == 0) {
                Gp_MsgPlayerWeapon(0);
                func_800E8634(D_shelter_b1_sterilization_room_80188ED4, 0, D_shelter_b1_sterilization_room_80188FDC);
                GameFlag_SetNibble(0x14F, 1);
            } else {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 0xB, 0);
                GameFlag_SetNibble(0x14F, 2);
            }
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xC || arg2 == 0xD) {
        if (gGameSession->at4.loc.room == 3) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E70, 0, arg2, 0);
        }
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180188(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 5:
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
            task->state++;
            break;
        case 6:
            task->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 8:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

/// Plays the sound event in `spawnArg2` in states 0 and 0x50, then kills the
/// task at state 0x78.
void func_shelter_b1_sterilization_room_801802B0(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

static void func_shelter_b1_sterilization_room_80180340(s32 arg0)
{
    Task* slot   = Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetPtrSlot(3);
    }
    if (slot != NULL) {
        if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 1) {
            D_shelter_b1_sterilization_room_80184E80[1] = 0;
        } else {
            D_shelter_b1_sterilization_room_80184E80[1] = 0x2710;
        }
    } else {
        D_shelter_b1_sterilization_room_80184E80[1] = 0x2710;
    }
    func_shelter_b1_sterilization_room_80180570(task->extra.tmd->coords, D_shelter_b1_sterilization_room_80184E80);
}

s32 func_shelter_b1_sterilization_room_801803E4(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_b1_sterilization_room_801803EC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_sterilization_room_80180430(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54100016, 0, 0);
    }
    return 0;
}

static void func_shelter_b1_sterilization_room_80180464(Task* task)
{
    if (gGameSession->at4.loc.place == 5) {
        func_shelter_b1_sterilization_room_8018049C();
    }
}

static void func_shelter_b1_sterilization_room_8018049C(void)
{
    s32 view;

    view = gGameSession->at4.loc.view;
    if ((GameFlag_GetNibble(0xEA) == 1) && (gGameSession->eventState == 0)) {
        if (view == 2 || view == 3) {
            Gp_MsgSlot4Chain(0, 1);
        } else {
            Gp_MsgSlot4Chain(0, 0);
        }
    }
}

void func_shelter_b1_sterilization_room_80180518(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sterilization_room_8017D6A4;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_sterilization_room_80180570(GpCoord* coord, s16* arg1)
{
    MATRIX        m;
    long          flag;
    s32           i;
    SVECTOR*      d;
    SVECTOR*      s;
    GpGridParams* dst = &D_shelter_b1_sterilization_room_80189E44;
    GpGridParams* src = &D_shelter_b1_sterilization_room_80184F28;

    i = 0;
    do {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
        i++;
    } while (i < 4);

    i = 0;
    do {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
        i++;
    } while (i < 8);

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->field_4;
    s = src->field_4;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

/// Queues two VRAM copies of the frame buffer being drawn, a 0xC0-wide strip to
/// (0x340, 0) and the next 0x80 columns to (0x180, 0x100), bracketed by STP
/// commands that set the mask bit off before them and back on after. The
/// source row follows the live draw buffer. The task then advances a state.
static void func_shelter_b1_sterilization_room_80180828(Task* task)
{
    RECT     rect;
    DR_STP*  stp;
    DR_MOVE* mv;
    s16      x;
    s16      y;

    if (gDisplayState.drawBuffer == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + 8, stp);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(gGpuCurrentOt + 8, mv);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x180, 0x100);
    addPrim(gGpuCurrentOt + 8, mv);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 8, stp);

    task->state++;
}

/// Draws the room's two backdrop halves as opaque `SPRT`s in OT slot 8, tinted
/// by `shade`. The source rows, both the sprites' `v` and the tpage row, follow
/// the display buffer being drawn.
static void func_shelter_b1_sterilization_room_80180A2C(s32 shade)
{
    SPRT* p;
    s16   tpageY;
    u8    u;
    u8    v;

    if (gDisplayState.drawBuffer == 0) {
        tpageY = 0;
        u      = 0;
        v      = 0;
    } else {
        tpageY = 0x100;
        u      = 0;
        v      = 0x10;
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = u;
    p->v0   = v;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0, tpageY);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->x0   = 0x20;
    p->u0   = u;
    p->v0   = v;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0xC0, tpageY);
}

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
static void func_shelter_b1_sterilization_room_80180BF0(s32 shade)
{
    SPRT* p;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0x180, 0x100);
}

void func_shelter_b1_sterilization_room_80180D74(Task* task)
{
    s32 c;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            SndEvt_EnqueueType6(0x5410000A, 0, 0);
            task->state++;
            break;
        case 1:
            if (++task->killCountdown >= 30) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            Fade_DrawOverlay(c, c, c, 2);
            break;
        case 2:
            Mc_SaveData[0].state.at4.loc.view = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->at4.loc.view        = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->viewDirty           = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E9,
                              &D_shelter_b1_sterilization_room_80188668[D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].msg],
                              0);
            SndEvt_EnqueueType6(0x5410000B, 0, 0);
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            task->state++;
            break;
        case 3:
            if (--task->killCountdown <= 0) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            Fade_DrawOverlay(c, c, c, 2);
            break;
        default:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_80180F74(Task* task)
{
    Task*       player;
    GpCoord*    coord;
    s32         pan;
    GpStateC08* st;

    switch (task->state) {
        case 0:
            gGameSession->restartMode = 2;
            task->state++;
            return;
        case 1:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (gGameSession->eventState == 0 && Gp_StateF0.field_4 == 0) {
                    player = gameGetPtrSlot(3);
                    task->killCountdown++;
                    if (task->killCountdown == 0x78) {
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_shelter_b1_sterilization_room_80188738, 0), 0);
                    } else if (task->killCountdown >= 0x79) {
                        if (Player_Status.hp > 0) {
                            coord = player->extra.tmd->coords;
                            Gp_DispatchMsgPtr(player, 0x3F7, &D_shelter_b1_sterilization_room_80188590, 0);
                            Gp_PlayerWeaponId(&D_shelter_b1_sterilization_room_80188624.animBlock.index);
                            Gp_DispatchMsgPtr(player, 0x3E8, &D_shelter_b1_sterilization_room_80188624, 0);
                            pan = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(0x54100011, pan, (s8)gpGetObjDepth(coord));
                            task->killCountdown = 0;
                        }
                        st           = &Gp_StateC08;
                        st->field_6 |= 1;
                    } else if (task->killCountdown == 0x49) {
                        Gp_MsgPlayerWeapon(1);
                    }
                    task->spawnArg1.value = 0;
                    return;
                }
                if (task->spawnArg1.value == 0) {
                    SndEvt_EnqueueType7(0x54100011, 1);
                    task->spawnArg1.value = 1;
                }
                return;
            }
            SndEvt_EnqueueType7(0x54100011, 1);
        default:
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_8018118C(s32 arg0)
{
    if (!(((s32)D_shelter_b1_sterilization_room_8018C340 >> arg0) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << arg0;
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, arg0, 0, 0);
    }
}

/// The four states of the backdrop task run by
/// `func_shelter_b1_sterilization_room_801811E0`: copy the frame buffer into
/// the backdrop, wait for the view, fade the copy out and kill.
static const TaskFuncTable4 D_shelter_b1_sterilization_room_8017D700 = {
    {
        func_shelter_b1_sterilization_room_80180828,
        func_shelter_b1_sterilization_room_80181244,
        func_shelter_b1_sterilization_room_801812A0,
        taskKill,
    },
};

void func_shelter_b1_sterilization_room_801811E0(Task* task)
{
    TaskFuncTable4 states;

    states = D_shelter_b1_sterilization_room_8017D700;
    states.funcs[task->state](task);
}

static void func_shelter_b1_sterilization_room_80181244(Task* task)
{
    func_shelter_b1_sterilization_room_80180BF0(0x80);
    func_shelter_b1_sterilization_room_80180A2C(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

/// Steps `killCountdown` down by 8 each frame, advancing the task once it
/// reaches zero, and redraws the backdrop with the semi-transparent copy at
/// that level and the opaque copy at the rest.
static void func_shelter_b1_sterilization_room_801812A0(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_shelter_b1_sterilization_room_80180BF0(task->killCountdown);
    func_shelter_b1_sterilization_room_80180A2C(0x80 - task->killCountdown);
}

/// Appends a semi-transparent 15-bit `DR_TPAGE` for VRAM origin (`x`, `y`)
/// to OT slot 8.
static void func_shelter_b1_sterilization_room_80181308(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}
