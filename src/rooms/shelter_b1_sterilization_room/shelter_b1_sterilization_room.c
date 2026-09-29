#include "shelter_b1_sterilization_room_private.h"
#include "mapui/map_shelter.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "decomp/common.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181228.h"
#include "rooms/shelter_b1_sterilization_room.h"
#include "rooms/acropolis_square.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/area_transitions.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/item_menu.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "gameplay/attachment_state.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "gameplay/animation.h"

extern GpObj4C D_shelter_b1_sterilization_room_8018B8A8[28];

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
extern u8 D_shelter_b1_sterilization_room_8018453C[];
extern u8 D_shelter_b1_sterilization_room_8018456C[];
extern u8 D_shelter_b1_sterilization_room_80184544[];
extern u8 D_shelter_b1_sterilization_room_80184548[];
extern u8 D_shelter_b1_sterilization_room_80184550[];
extern u8 D_shelter_b1_sterilization_room_8018455C[];
extern u8 D_shelter_b1_sterilization_room_80184574[];
extern u8 D_shelter_b1_sterilization_room_8018457C[];
extern u8 D_shelter_b1_sterilization_room_80184584[];

/// Unit suffix appended to the counted rows.
extern u8 D_shelter_b1_sterilization_room_8018458C[];

/// Help lines shown for each statistics row while it is selected.
extern u8 D_shelter_b1_sterilization_room_80184598[];
extern u8 D_shelter_b1_sterilization_room_801845C4[];
extern u8 D_shelter_b1_sterilization_room_801845E8[];
extern u8 D_shelter_b1_sterilization_room_80184618[];
extern u8 D_shelter_b1_sterilization_room_8018464C[];
extern u8 D_shelter_b1_sterilization_room_80184680[];
extern u8 D_shelter_b1_sterilization_room_801846B8[];
extern u8 D_shelter_b1_sterilization_room_801846EC[];
extern u8 D_shelter_b1_sterilization_room_80184724[];

/// Prompt lines drawn by the dialog handlers.
extern u8 D_shelter_b1_sterilization_room_80184514[];
extern u8 D_shelter_b1_sterilization_room_8018451C[];
extern u8 D_shelter_b1_sterilization_room_80184528[];
extern u8 D_shelter_b1_sterilization_room_80184534[];

/// List state of the "Play Data" menu `func_shelter_b1_sterilization_room_8017EFE4` runs.
extern UiList D_shelter_b1_sterilization_room_80184760;

/// List state of the weapon- and PE-usage panels.
extern UiList D_shelter_b1_sterilization_room_80184788;

/// UI descriptors the dialog handlers open on confirm.
extern UiObjectDesc D_shelter_b1_sterilization_room_801847C8;
extern UiObjectDesc D_shelter_b1_sterilization_room_801847E4;

/// List state of the menu `func_shelter_b1_sterilization_room_8017EB2C` runs.
extern UiList D_shelter_b1_sterilization_room_80184810;

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task *, s32, GpMsg13EF *, s32);
        s32 (*call2)(s32, s32, RoomEventMsg *, RoomEventMsg *);
        s32 (*call3)(s32, s32, s32);
    } handler;
} ShelterB1SterilizationRoomMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB1SterilizationRoomMessageEntry, 8);

extern ShelterB1SterilizationRoomMessageEntry D_shelter_b1_sterilization_room_80184E40[6];
extern TaskDesc                        D_shelter_b1_sterilization_room_80184E70;
extern s32                             D_shelter_b1_sterilization_room_80184E7C;
extern s16 D_shelter_b1_sterilization_room_80184E80[3];
extern GpGridParams                    D_shelter_b1_sterilization_room_80184F28;
extern GpCopyArg D_shelter_b1_sterilization_room_80188590;
extern GpAnimArg D_shelter_b1_sterilization_room_80188624;
extern _ShelterB1SterilizationRoomMsg  D_shelter_b1_sterilization_room_80188668[];
extern _ShelterB1SterilizationRoomDest D_shelter_b1_sterilization_room_80188728[];
extern GpU16Pair                       D_shelter_b1_sterilization_room_80188738;
extern GpEvsCmd D_shelter_b1_sterilization_room_8018873C[];

/// Area records `func_shelter_b1_sterilization_room_8017F550` applies when it
/// advances game flag nibble 0 from 2 to 3 in one particular view.

extern GpEvsCmd D_shelter_b1_sterilization_room_80188AB4[];
extern GpEvsCmd D_shelter_b1_sterilization_room_80188ED4[];
extern GpEvsCmd D_shelter_b1_sterilization_room_80188FDC[];
extern GpGridParams   D_shelter_b1_sterilization_room_80189E44;
extern GpAreaApplyRec D_shelter_b1_sterilization_room_8018C334[];

/// The task `func_shelter_b1_sterilization_room_8017F550` spawns from entry 1
/// of its table and waits on, or NULL while none runs.
extern Task* D_shelter_b1_sterilization_room_8018C33C;

extern RoomCutsceneRec D_shelter_b1_sterilization_room_8018C344;

extern UiObjectDesc D_800611E4;

extern s32 D_80135AC0;
extern s32 D_80135D78;
extern s32 D_80136258;

static void func_shelter_b1_sterilization_room_8017E35C(UiList* list, UiObject* obj);
static void func_shelter_b1_sterilization_room_8017E658(UiList* list, UiObject* obj);
static void func_shelter_b1_sterilization_room_8017FABC(Task* task);
static void func_shelter_b1_sterilization_room_80180340(s32 arg0);
static void func_shelter_b1_sterilization_room_80180464(Task* task);
static void func_shelter_b1_sterilization_room_8018049C(void);
static void func_shelter_b1_sterilization_room_80180570(GpCoord* coord, s16* arg1);
static void func_shelter_b1_sterilization_room_80180828(Task* task);
static void func_shelter_b1_sterilization_room_80181244(Task* task);
static void func_shelter_b1_sterilization_room_801812A0(Task* task);

static void func_shelter_b1_sterilization_room_8017F514(Task* task);
static void func_shelter_b1_sterilization_room_80181308(s32 tpage, s16 arg1);

void func_shelter_b1_sterilization_room_8017D794(UiList *, UiObject *);
void func_shelter_b1_sterilization_room_8017DF60(UiList *, UiObject *);
void func_shelter_b1_sterilization_room_8017E978(Task *);
void func_shelter_b1_sterilization_room_8017EE24(Task *);
void func_shelter_b1_sterilization_room_8017EFE4(Task *);
void func_shelter_b1_sterilization_room_8017F1D8(UiList *, UiObject *);
void func_shelter_b1_sterilization_room_8017F2BC(UiList *, UiObject *);
void func_shelter_b1_sterilization_room_8017F384(UiList *, UiObject *);
void func_shelter_b1_sterilization_room_8017F44C(UiList *, UiObject *);

s32 func_shelter_b1_sterilization_room_8017FC78(Task *, s32, GpMsg13EF *, s32);
s32 func_shelter_b1_sterilization_room_8017FF80(s32, s32, s32);
s32 func_shelter_b1_sterilization_room_801803E4(void);
s32 func_shelter_b1_sterilization_room_801803EC(s32, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_shelter_b1_sterilization_room_80180430(s32, s32, s32);
void func_shelter_b1_sterilization_room_8017F550(Task *);
void func_shelter_b1_sterilization_room_80180188(Task *);
void func_shelter_b1_sterilization_room_801802B0(Task *);

extern GpAnimSet D_shelter_b1_sterilization_room_80185228;
extern GpAnimSet D_shelter_b1_sterilization_room_801853DC;
extern GpAnimSet D_shelter_b1_sterilization_room_80185AFC;
extern GpAnimSet D_shelter_b1_sterilization_room_80185C8C;
extern GpAnimSet D_shelter_b1_sterilization_room_80186910;
extern GpAnimSet D_shelter_b1_sterilization_room_80187E18;
extern GpAnimSet D_shelter_b1_sterilization_room_801884DC;
void func_shelter_b1_sterilization_room_80180D74(Task *);
void func_shelter_b1_sterilization_room_80180F74(Task *);
void func_shelter_b1_sterilization_room_801811E0(Task *);

u8 D_shelter_b1_sterilization_room_80184514[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_8018451C[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184528[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_shelter_b1_sterilization_room_80184534[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_shelter_b1_sterilization_room_8018453C[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184544[4] = {
    87, 111, 110, 0,
};

u8 D_shelter_b1_sterilization_room_80184548[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_shelter_b1_sterilization_room_80184550[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_shelter_b1_sterilization_room_8018455C[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_8018456C[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184574[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_shelter_b1_sterilization_room_8018457C[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_shelter_b1_sterilization_room_80184584[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_shelter_b1_sterilization_room_8018458C[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184594[4] = {
    37, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184598[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_801845C4[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_shelter_b1_sterilization_room_801845E8[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184618[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_8018464C[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_shelter_b1_sterilization_room_80184680[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_shelter_b1_sterilization_room_801846B8[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_shelter_b1_sterilization_room_801846EC[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_shelter_b1_sterilization_room_80184724[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_shelter_b1_sterilization_room_8018475C[1] = {
    func_shelter_b1_sterilization_room_8017D794,
};

UiList D_shelter_b1_sterilization_room_80184760 = { D_shelter_b1_sterilization_room_8018475C, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_shelter_b1_sterilization_room_80184784[1] = {
    func_shelter_b1_sterilization_room_8017DF60,
};

UiList D_shelter_b1_sterilization_room_80184788 = { D_shelter_b1_sterilization_room_80184784, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_shelter_b1_sterilization_room_801847AC = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_shelter_b1_sterilization_room_8017EE24, 0 };

UiObjectDesc D_shelter_b1_sterilization_room_801847C8 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_shelter_b1_sterilization_room_8017EFE4, 0 };

UiObjectDesc D_shelter_b1_sterilization_room_801847E4 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_shelter_b1_sterilization_room_8017E978, 0 };

UiListItemFunc D_shelter_b1_sterilization_room_80184800[4] = {
    func_shelter_b1_sterilization_room_8017F1D8,
    func_shelter_b1_sterilization_room_8017F2BC,
    func_shelter_b1_sterilization_room_8017F384,
    func_shelter_b1_sterilization_room_8017F44C,
};

UiList D_shelter_b1_sterilization_room_80184810 = { D_shelter_b1_sterilization_room_80184800, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

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
    0, 1768, 0, 3,
    D_shelter_b1_sterilization_room_801848A0, D_shelter_b1_sterilization_room_801848AC, D_shelter_b1_sterilization_room_80184A6C, D_shelter_b1_sterilization_room_80184834, D_shelter_b1_sterilization_room_80184A9C,
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

s16 * D_shelter_b1_sterilization_room_80184F24[1] = {
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
    D_shelter_b1_sterilization_room_8018504C, D_shelter_b1_sterilization_room_80185200,
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
    D_shelter_b1_sterilization_room_801852D0, D_shelter_b1_sterilization_room_801853B4,
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
    D_shelter_b1_sterilization_room_801856E8, D_shelter_b1_sterilization_room_80185AD4,
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
    D_shelter_b1_sterilization_room_80185B80, D_shelter_b1_sterilization_room_80185C64,
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
    D_shelter_b1_sterilization_room_801862F8, D_shelter_b1_sterilization_room_801868E8,
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
    D_shelter_b1_sterilization_room_801873B8, D_shelter_b1_sterilization_room_80187DF0,
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
    D_shelter_b1_sterilization_room_801880CC, D_shelter_b1_sterilization_room_801884B4,
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

GpAnimSet * D_shelter_b1_sterilization_room_80188570[8] = {
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

void func_shelter_b1_sterilization_room_8017D794(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b1_sterilization_room_80184598,
                D_shelter_b1_sterilization_room_801845C4,
                D_shelter_b1_sterilization_room_801845E8,
                D_shelter_b1_sterilization_room_80184618,
                D_shelter_b1_sterilization_room_8018464C,
                D_shelter_b1_sterilization_room_80184680,
                D_shelter_b1_sterilization_room_801846B8,
                D_shelter_b1_sterilization_room_801846EC,
                D_shelter_b1_sterilization_room_80184724,
            };

            Ui_SetHolderParam(tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_8018453C);
            Text_FormatTime(p, Mc_SaveData[0].state.playTime);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_8018456C);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_80184544);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_80184548);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_80184550);
            if (Mc_SaveData[0].state.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData[0].state.field_6CC * 10000) / (Mc_SaveData[0].state.field_6CC + Mc_SaveData[0].state.field_6CE);
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = Mc_SaveData[0].state.field_6CC;
            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_8018455C);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar(&(arg1)->panel, arg1->panel.field_1C.s, (s16)arg1->panel.field_1E.u, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_80184574);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_8018457C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_92C), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_shelter_b1_sterilization_room_80184584);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel.
static const char D_shelter_b1_sterilization_room_8017D610[] = "Play Data";

/// Drawn in place of the percentage for a row holding every recorded use.
static const u8 D_shelter_b1_sterilization_room_8017D61C[] = "100.0%";

void func_shelter_b1_sterilization_room_8017DF60(UiList* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->panel.field_8 != 5) {
        req.x          = arg1->panel.field_20.u + 0x11 + x;
        textY          = arg1->panel.field_22.u - 6;
        req.y          = textY + y;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        Text_DrawString(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b1_sterilization_room_8017D61C, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            Text_ItoaPadded(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
        }
        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->panel.field_1C.s + 0x80;
    avail = (s16)arg1->panel.field_1E.u - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                     = (POLY_G4*)gGpuPrimCursor;
        tx                       = arg1->panel.field_20.u + barX + 1;
        prim->x2                 = tx;
        prim->x0                 = tx;
        ty                       = arg1->panel.field_22.u;
        gGpuPrimCursor           = prim + 1;
        ty                       = ty + rowY;
        ty                      += 1;
        PRIM_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        PRIM_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        tx                       = (u16)prim->x0 + barW - 1;
        prim->y1                 = ty;
        prim->y0                 = ty;
        ty                      += 8;
        prim->y3                 = ty;
        prim->y2                 = ty;
        prim->x3                 = tx;
        prim->x1                 = tx;
        addPrim(gGpuCurrentOt + arg1->panel.field_14.s + 1, prim);
    }
    one = 1;
    Ui_DrawBeveledRect(&(arg1)->panel, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->panel.field_0.w >> 16) == one) || (arg1->panel.field_0.w == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

/// Fills the weapon-usage panel's `RoomItemUsage` block from the save's
/// per-weapon use counters (ids 0x80-0x9F). Each id with a non-empty name and
/// a non-zero counter is marked seen and listed, most-used first; each row
/// then gets its share of all uses in hundredths of a percent and a bar width
/// as a 12-bit fraction of the top row, both scaled down until the top counter
/// fits in 17 bits.
static void func_shelter_b1_sterilization_room_8017E35C(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData[0].state.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData[0].state.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Fills the "Play Data" PE-usage panel's `RoomPeUsage` block from the
/// save's per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData[0].state.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData[0].state.attachUseCounts` is appended and its counter summed. Levels
/// are addressed by page and column, with three slots per page. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void func_shelter_b1_sterilization_room_8017E658(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;

    for (; i < 12; i++) {
        s32 useCount;

        useCount = Mc_SaveData[0].state.attachUseCounts[i];
        id       = i * 3 + 0xF;
        if (useCount > 0) {
            s32 page;
            s32 column;

            page   = i / 3;
            column = i % 3;
            *p     = id;
            if (Mc_SaveData[0].state.attachLevels[column + page * 3] != 0) {
                *p = id + (Mc_SaveData[0].state.attachLevels[column + page * 3] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData[0].state.attachUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData[0].state.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData[0].state.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData[0].state.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData[0].state.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData[0].state.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Titles of the weapon and PE usage panels.
static const char D_shelter_b1_sterilization_room_8017D624[] = "Weapon Data";
static const char D_shelter_b1_sterilization_room_8017D630[] = "PE Data";

void func_shelter_b1_sterilization_room_8017E978(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_shelter_b1_sterilization_room_80184788;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_shelter_b1_sterilization_room_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_shelter_b1_sterilization_room_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847AC, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_shelter_b1_sterilization_room_8017E35C(list, obj);
        } else {
            func_shelter_b1_sterilization_room_8017E658(list, obj);
        }
        Ui_InitList(list, &(obj)->panel);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->panel.field_0.w = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_shelter_b1_sterilization_room_8017D638[12] = "Telephone\0\0 ";

void func_shelter_b1_sterilization_room_8017EB2C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    ready         = Mc_SaveData[0].state.demoScene == 1;
    list          = &D_shelter_b1_sterilization_room_80184810;
    one           = 1;
    if (Mc_SaveData[0].state.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->panel.field_0.w = 0;
            obj->panel.field_4  |= 0x80000000;
            task->state          = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.field_0.w = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText(&(obj)->panel, D_shelter_b1_sterilization_room_8017D638);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4(obj, task);
        obj->panel.field_0.w = 0;
    }
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim(&(obj)->panel, task);
                    obj->panel.field_0.w = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}

void func_shelter_b1_sterilization_room_8017EE24(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_shelter_b1_sterilization_room_8017F514;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at
/// most the string's length) follow it. Does nothing when `decimals <= 0`.
static void func_shelter_b1_sterilization_room_8017EE80(u8* str, s32 decimals)
{
    s32 len = 0;
    s32 i;

    if (decimals > 0) {
        while (*str != 0) {
            str++;
            len++;
        }
        if (len < decimals) {
            decimals = len;
        }
        decimals++;
        for (i = 0; i < decimals; i++) {
            str[1] = str[0];
            str--;
        }
        str[1] = '.';
    }
}

/// Formats `value` into `buf` as a percentage with `decimals` digits after the
/// point, zero-padding a small value so that a digit precedes the point, then
/// appends "%" and returns `buf`.
static u8* func_shelter_b1_sterilization_room_8017EEF0(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        Text_ItoaPadded(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_shelter_b1_sterilization_room_80184594);
    return buf;
}

void func_shelter_b1_sterilization_room_8017EFE4(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b1_sterilization_room_80184760;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_shelter_b1_sterilization_room_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847AC, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.bounds.unsignedRect.h += 5;
        list->field_A                     = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud rectangle one OT slot past the panel's draw order, at
/// (`arg1`, `arg2`) from the panel origin and `arg3` by `arg4` in size, with
/// `arg5` on the left vertices and `arg6` on the right. Nothing is drawn for a
/// zero `arg5` or a width below 2.
static void func_shelter_b1_sterilization_room_8017F0D4(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s32      y;
    s16      bottom;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20.u + arg1 + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22.u;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                       += arg2;
        y++;
        x        = prim->x0 + arg3 - 1;
        prim->y1 = y;
        prim->y0 = y;
        prim->x3 = x;
        prim->x1 = x;
        bottom   = y + arg4 - 1;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14.u + 1, prim);
    }
}

void func_shelter_b1_sterilization_room_8017F1D8(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184514, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        gDisplayState.gameMode = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->panel.field_0.w = 0;
        obj->field_2E        = 6;
        obj->owner->state    = sel;
    }
}

void func_shelter_b1_sterilization_room_8017F2BC(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_8018451C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847C8, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_shelter_b1_sterilization_room_8017F384(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184528, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847E4, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_shelter_b1_sterilization_room_8017F44C(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184534, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847E4, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Exit callback of the help-line box task: releases `Wip_UiHolder` if the
/// task owns it, then frees the task's UI object and kills it.
static void func_shelter_b1_sterilization_room_8017F514(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

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
                D_80115694                  = Mc_SaveData[0].state.at4.loc.view;
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
            gGameSession->at4.loc.view  = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->viewDirty     = 1;
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
