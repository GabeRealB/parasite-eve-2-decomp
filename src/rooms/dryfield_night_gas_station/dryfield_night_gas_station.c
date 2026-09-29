#include "rooms/dryfield_night_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
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
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// The block the room's effect task carries as its `spawnArg2`.
/// `func_dryfield_night_gas_station_80180E9C` keeps the spawn offset it hands
/// `Gp_SpawnEff` in `pos`, sets `active` once game flag nibble 0x63 has been
/// seen clear, and stores the per-anchor effect roll in `kind`. The bytes
/// around those fields are not reached here.
typedef struct DryfieldNightGasStationEffWork {
    byte    pad_0[0x10];
    SVECTOR pos;
    byte    pad_18[0xC];
    s16     active;
    s16     kind;
} DryfieldNightGasStationEffWork;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_dryfield_night_gas_station_801888A0[];

/// Handle of the task spawned from entry 0 of `D_dryfield_night_gas_station_801888A0`,
/// or NULL while none runs. `func_dryfield_night_gas_station_801807D4` either
/// passes it an argument or kills it.
extern Task* D_dryfield_night_gas_station_801907A4;

/// Handle of the task spawned from entry 1 or 3 of
/// `D_dryfield_night_gas_station_801888A0`, or NULL while none runs.
extern Task* D_dryfield_night_gas_station_801907A8;

/// UI descriptor of the help-line box (`func_dryfield_night_gas_station_8017ECF0`)
/// that the "Play Data" and usage panels open beside their lists.
static UiObjectDesc Telephone_Data_80181C90;

extern SVECTOR D_dryfield_night_gas_station_80189DA0[2];

extern UiObjectDesc D_800611E4;

/// Prompt texts: "Save", "Play Data", "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the "Play Data" statistics list, one per row index.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The " times" suffix appended to the statistics rows that count events.
static u8 Telephone_Data_80181A70[];

/// The "%" suffix appended to a formatted percentage.
static u8 Telephone_Data_80181A78[];

/// Help lines shown for the selected statistics row, one per row index.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The "Play Data" statistics list.
static UiList Telephone_Data_80181C44;

/// The usage list shown by `func_dryfield_night_gas_station_8017E844`.
static UiList Telephone_Data_80181C6C;

/// UI descriptors the "Play Data" and usage prompts open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The list shown by `func_dryfield_night_gas_station_8017E9F8`.
static UiList Telephone_Data_80181CF4;

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpMsg13EF*);
        s32 (*call2)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call3)(s32, s32, s32);
    } handler;
} DryfieldNightGasStationMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationMessageEntry, 8);

extern DryfieldNightGasStationMessageEntry D_dryfield_night_gas_station_80184034[7];
extern TaskDesc                            D_dryfield_night_gas_station_8018406C[];
extern AnimationPlayRequest                D_dryfield_night_gas_station_80184098;
extern GpEvsCmd                            D_dryfield_night_gas_station_801840AC[];
extern GpEvsCmd                            D_dryfield_night_gas_station_801841FC[];

/// The layout template and the live copy that
/// `func_dryfield_night_gas_station_8017FBD4` restores from it.
extern GpGridParams D_dryfield_night_gas_station_80184374;
extern GpGridParams D_dryfield_night_gas_station_8018ABBC;

extern SVECTOR    D_dryfield_night_gas_station_80188580[];
extern GpXformArg D_dryfield_night_gas_station_80188B0C;
extern GpEvsCmd   D_dryfield_night_gas_station_80188B64[];
extern GpEvsCmd   D_dryfield_night_gas_station_80188BF4[];
extern GpEvsCmd   D_dryfield_night_gas_station_80189014[];
extern GpEvsCmd   D_dryfield_night_gas_station_8018920C[];
extern GpEvsCmd   D_dryfield_night_gas_station_801892E4[];
extern GpEvsCmd   D_dryfield_night_gas_station_80189A7C[];

/// The room's effect anchors, 8 bytes apart. Entries 0-9 are drawn in pairs by
/// `func_dryfield_night_gas_station_801812B4`, 10-18 one at a time by
/// `func_dryfield_night_gas_station_80181AF8`, and 19-20 are where the spawned
/// effects are scattered around.
extern SVECTOR D_dryfield_night_gas_station_80189C8C[];

/// Entries 21-24 of the anchor list, reached by name: two
/// `func_dryfield_night_gas_station_801812B4` pairs drawn together whenever
/// one of views 2, 3, 13 or 14 is current.
extern SVECTOR D_dryfield_night_gas_station_80189D34[];

/// Per-anchor view masks, one word per anchor: bit `n` set draws the anchor
/// while view `n` is current.
extern s32 D_dryfield_night_gas_station_80189D54[];

/// The beam's two end points relative to the effect's parent coordinate;
/// the second is also read by its own name.

extern GpAreaApplyRec D_dryfield_night_gas_station_801907A0[];

/// Handle of the task spawned from entry 2 of
/// `D_dryfield_night_gas_station_801888A0`, or NULL while none runs.
extern Task* D_dryfield_night_gas_station_801907AC;

#define TELEPHONE_TITLE_BYTES "Telephone\0" \
                              "5\x96"
#include "../../shared/telephone.h"

void        func_dryfield_night_gas_station_8017FBD4(s32 arg0);
static void func_dryfield_night_gas_station_80180C20(void);
static void func_dryfield_night_gas_station_80180D1C(void);
static void func_dryfield_night_gas_station_80180DC8(s16 arg0);
static void func_dryfield_night_gas_station_80182CD4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);

static void func_dryfield_night_gas_station_801812B4(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_gas_station_80181AF8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_gas_station_80182024(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_dryfield_night_gas_station_80182450(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_dryfield_night_gas_station_80183354(GfxCoord* arg0, s16 arg1, u8* arg2);

void func_dryfield_night_gas_station_80180828(Task*);

extern GpAnimSet D_dryfield_night_gas_station_80184674;
extern GpAnimSet D_dryfield_night_gas_station_80184964;
extern GpAnimSet D_dryfield_night_gas_station_80184D64;
extern GpAnimSet D_dryfield_night_gas_station_80185018;
extern GpAnimSet D_dryfield_night_gas_station_801852BC;

extern GpAnimSet D_dryfield_night_gas_station_80185538;
extern GpAnimSet D_dryfield_night_gas_station_80185708;
extern GpAnimSet D_dryfield_night_gas_station_801858D8;
extern GpAnimSet D_dryfield_night_gas_station_80185E1C;
extern GpAnimSet D_dryfield_night_gas_station_80186904;
extern GpAnimSet D_dryfield_night_gas_station_80187340;
extern GpAnimSet D_dryfield_night_gas_station_801879BC;
extern GpAnimSet D_dryfield_night_gas_station_80187C58;
extern GpAnimSet D_dryfield_night_gas_station_80187FFC;
extern GpAnimSet D_dryfield_night_gas_station_80188558;

extern AnimationPlayRequest D_dryfield_night_gas_station_80188904;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188918;
extern AnimationPlayRequest D_dryfield_night_gas_station_8018892C;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188954;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188968;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A18;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A2C;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A40;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A54;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A68;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188A7C;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188AA4;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188AB8;
extern AnimationPlayRequest D_dryfield_night_gas_station_80188AE0;
extern GpCopyArg            D_dryfield_night_gas_station_801888E8;
extern GpCopyArg            D_dryfield_night_gas_station_80188A10;
extern GpGridParams         D_dryfield_night_gas_station_8018ABBC;
extern GpGridParams         D_dryfield_night_gas_station_8018B75C[1];
extern GpObj4C              D_dryfield_night_gas_station_8018FD90[11];
extern GpObj4C              D_dryfield_night_gas_station_801900D4[16];
extern GpOverrideArg        D_dryfield_night_gas_station_801889DC;
extern GpRoomBoundVec       D_dryfield_night_gas_station_80190684[22];
extern GpRoomCoordSet       D_dryfield_night_gas_station_8018FAC0[1];
extern GpRoomCoordSet       D_dryfield_night_gas_station_8018FD78[1];
extern GpXformArg           D_dryfield_night_gas_station_8018897C;
extern GpXformArg           D_dryfield_night_gas_station_80188994;
extern GpXformArg           D_dryfield_night_gas_station_801889AC;
extern GpXformArg           D_dryfield_night_gas_station_801889C4;
extern GpXformArg           D_dryfield_night_gas_station_80188AF4;
extern GpXformArg           D_dryfield_night_gas_station_80188B0C;
extern TaskDesc             D_8014D8A4;
void                        func_dryfield_night_gas_station_8017FBD4(s32);
void                        func_dryfield_night_gas_station_80180604(s32);
void                        func_dryfield_night_gas_station_80180720(void);
void                        func_dryfield_night_gas_station_80180740(void);
void                        func_dryfield_night_gas_station_80180760(void);
void                        func_dryfield_night_gas_station_80180780(void);
void                        func_dryfield_night_gas_station_801807A0(void);
void                        func_dryfield_night_gas_station_801807D4(s32);
void                        func_dryfield_night_gas_station_80180920(s32);
void                        func_dryfield_night_gas_station_80180940(void);
void                        func_dryfield_night_gas_station_80180974(void);
void                        func_dryfield_night_gas_station_80180A00(void);
void                        func_dryfield_night_gas_station_80180A34(void);
void                        func_dryfield_night_gas_station_80180B04(void);
void                        func_dryfield_night_gas_station_80180B38(void);
void                        func_dryfield_night_gas_station_80180BEC(void);
void                        func_dryfield_night_gas_station_80180C3C(s32);

extern AnimationPlayRequest D_dryfield_night_gas_station_80184084;
void                        func_dryfield_night_gas_station_8017FB64(u8);

s32 func_dryfield_night_gas_station_8017F544(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_gas_station_8017F6B8(s32, s32, s32);
s32 func_dryfield_night_gas_station_8017F7E0(s32, s32, s32);
s32 func_dryfield_night_gas_station_8017F89C(s32, s32, s32);
s32 func_dryfield_night_gas_station_8017F990(Task*, s32, GpMsg13EF*);
s32 func_dryfield_night_gas_station_8017F9E8(void);

void func_dryfield_night_gas_station_8017FA6C(Task*);

#include "../../shared/telephone_data.inc.c"

DryfieldNightGasStationMessageEntry D_dryfield_night_gas_station_80184034[7] = {
    { 5102, { .call2 = func_dryfield_night_gas_station_8017F544 } },
    { 5105, { .call3 = func_dryfield_night_gas_station_8017F7E0 } },
    { 5103, { .call1 = func_dryfield_night_gas_station_8017F990 } },
    { 5104, { .call3 = func_dryfield_night_gas_station_8017F89C } },
    { 5106, { .call3 = func_dryfield_night_gas_station_8017F6B8 } },
    { 5108, { .call0 = func_dryfield_night_gas_station_8017F9E8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_dryfield_night_gas_station_8018406C[2] = {
    { 0, 32, func_dryfield_night_gas_station_8017FA6C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

AnimationPlayRequest D_dryfield_night_gas_station_80184084 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80184098 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_dryfield_night_gas_station_801840AC[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 19 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80184084 }, { .value = 0 } },
    { 13, { .callbackU8 = func_dryfield_night_gas_station_8017FB64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5301000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_801841FC[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 20 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_night_gas_station_801842D4[4] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_normals.inc"
};

SVECTOR D_dryfield_night_gas_station_801842F4[8] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_verts.inc"
};

GpGridFace D_dryfield_night_gas_station_80184334[4] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_faces.inc"
};

s16 D_dryfield_night_gas_station_80184364[6] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_gas_station_80184364[i])
s16* D_dryfield_night_gas_station_80184370[1] = {
#include "assets/dryfield_night_gas_station_collision_06DB4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_gas_station_80184374 = { NULL, D_dryfield_night_gas_station_801842D4, D_dryfield_night_gas_station_801842F4, D_dryfield_night_gas_station_80184334, D_dryfield_night_gas_station_80184370, -0x3534, 312, 1, 1, 4000, 4 };

AnimationPackedPose D_dryfield_night_gas_station_80184398[6] = {
#include "assets/dryfield_night_gas_station_animation_070B4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_801843E0[46] = {
#include "assets/dryfield_night_gas_station_animation_070B4_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80184498[109] = {
#include "assets/dryfield_night_gas_station_animation_070B4_records.inc"
};

u16 D_dryfield_night_gas_station_8018464C[20] = {
#include "assets/dryfield_night_gas_station_animation_070B4_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80184674 = {
    D_dryfield_night_gas_station_80184498,
    D_dryfield_night_gas_station_8018464C,
    { NULL, D_dryfield_night_gas_station_80184398, NULL, NULL, D_dryfield_night_gas_station_801843E0, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_8018469C[5] = {
#include "assets/dryfield_night_gas_station_animation_073A4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_801846D8[48] = {
#include "assets/dryfield_night_gas_station_animation_073A4_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80184798[105] = {
#include "assets/dryfield_night_gas_station_animation_073A4_records.inc"
};

u16 D_dryfield_night_gas_station_8018493C[20] = {
#include "assets/dryfield_night_gas_station_animation_073A4_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80184964 = {
    D_dryfield_night_gas_station_80184798,
    D_dryfield_night_gas_station_8018493C,
    { NULL, D_dryfield_night_gas_station_8018469C, NULL, NULL, D_dryfield_night_gas_station_801846D8, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_8018498C[10] = {
#include "assets/dryfield_night_gas_station_animation_077A4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80184A04[85] = {
#include "assets/dryfield_night_gas_station_animation_077A4_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80184B58[121] = {
#include "assets/dryfield_night_gas_station_animation_077A4_records.inc"
};

u16 D_dryfield_night_gas_station_80184D3C[20] = {
#include "assets/dryfield_night_gas_station_animation_077A4_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80184D64 = {
    D_dryfield_night_gas_station_80184B58,
    D_dryfield_night_gas_station_80184D3C,
    { NULL, D_dryfield_night_gas_station_8018498C, NULL, NULL, D_dryfield_night_gas_station_80184A04, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80184D8C[4] = {
#include "assets/dryfield_night_gas_station_animation_07A58_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80184DBC[49] = {
#include "assets/dryfield_night_gas_station_animation_07A58_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80184E80[92] = {
#include "assets/dryfield_night_gas_station_animation_07A58_records.inc"
};

u16 D_dryfield_night_gas_station_80184FF0[20] = {
#include "assets/dryfield_night_gas_station_animation_07A58_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80185018 = {
    D_dryfield_night_gas_station_80184E80,
    D_dryfield_night_gas_station_80184FF0,
    { NULL, D_dryfield_night_gas_station_80184D8C, NULL, NULL, D_dryfield_night_gas_station_80184DBC, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80185040[5] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_8018507C[32] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_801850FC[102] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_records.inc"
};

u16 D_dryfield_night_gas_station_80185294[20] = {
#include "assets/dryfield_night_gas_station_animation_07CFC_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_801852BC = {
    D_dryfield_night_gas_station_801850FC,
    D_dryfield_night_gas_station_80185294,
    { NULL, D_dryfield_night_gas_station_80185040, NULL, NULL, D_dryfield_night_gas_station_8018507C, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_801852E4[2] = {
#include "assets/dryfield_night_gas_station_animation_07F78_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_801852FC[32] = {
#include "assets/dryfield_night_gas_station_animation_07F78_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_8018537C[101] = {
#include "assets/dryfield_night_gas_station_animation_07F78_records.inc"
};

u16 D_dryfield_night_gas_station_80185510[20] = {
#include "assets/dryfield_night_gas_station_animation_07F78_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80185538 = {
    D_dryfield_night_gas_station_8018537C,
    D_dryfield_night_gas_station_80185510,
    { NULL, D_dryfield_night_gas_station_801852E4, NULL, NULL, D_dryfield_night_gas_station_801852FC, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80185560[2] = {
#include "assets/dryfield_night_gas_station_animation_08148_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80185578[30] = {
#include "assets/dryfield_night_gas_station_animation_08148_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_801855F0[60] = {
#include "assets/dryfield_night_gas_station_animation_08148_records.inc"
};

u16 D_dryfield_night_gas_station_801856E0[20] = {
#include "assets/dryfield_night_gas_station_animation_08148_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80185708 = {
    D_dryfield_night_gas_station_801855F0,
    D_dryfield_night_gas_station_801856E0,
    { NULL, D_dryfield_night_gas_station_80185560, NULL, NULL, D_dryfield_night_gas_station_80185578, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80185730[2] = {
#include "assets/dryfield_night_gas_station_animation_08318_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80185748[30] = {
#include "assets/dryfield_night_gas_station_animation_08318_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_801857C0[60] = {
#include "assets/dryfield_night_gas_station_animation_08318_records.inc"
};

u16 D_dryfield_night_gas_station_801858B0[20] = {
#include "assets/dryfield_night_gas_station_animation_08318_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_801858D8 = {
    D_dryfield_night_gas_station_801857C0,
    D_dryfield_night_gas_station_801858B0,
    { NULL, D_dryfield_night_gas_station_80185730, NULL, NULL, D_dryfield_night_gas_station_80185748, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80185900[9] = {
#include "assets/dryfield_night_gas_station_animation_0885C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_8018596C[127] = {
#include "assets/dryfield_night_gas_station_animation_0885C_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80185B68[163] = {
#include "assets/dryfield_night_gas_station_animation_0885C_records.inc"
};

u16 D_dryfield_night_gas_station_80185DF4[20] = {
#include "assets/dryfield_night_gas_station_animation_0885C_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80185E1C = {
    D_dryfield_night_gas_station_80185B68,
    D_dryfield_night_gas_station_80185DF4,
    { NULL, D_dryfield_night_gas_station_80185900, NULL, NULL, D_dryfield_night_gas_station_8018596C, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80185E44[24] = {
#include "assets/dryfield_night_gas_station_animation_09344_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80185F64[263] = {
#include "assets/dryfield_night_gas_station_animation_09344_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80186380[343] = {
#include "assets/dryfield_night_gas_station_animation_09344_records.inc"
};

u16 D_dryfield_night_gas_station_801868DC[20] = {
#include "assets/dryfield_night_gas_station_animation_09344_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80186904 = {
    D_dryfield_night_gas_station_80186380,
    D_dryfield_night_gas_station_801868DC,
    { NULL, D_dryfield_night_gas_station_80185E44, NULL, NULL, D_dryfield_night_gas_station_80185F64, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_8018692C[23] = {
#include "assets/dryfield_night_gas_station_animation_09D80_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80186A40[243] = {
#include "assets/dryfield_night_gas_station_animation_09D80_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80186E0C[323] = {
#include "assets/dryfield_night_gas_station_animation_09D80_records.inc"
};

u16 D_dryfield_night_gas_station_80187318[20] = {
#include "assets/dryfield_night_gas_station_animation_09D80_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80187340 = {
    D_dryfield_night_gas_station_80186E0C,
    D_dryfield_night_gas_station_80187318,
    { NULL, D_dryfield_night_gas_station_8018692C, NULL, NULL, D_dryfield_night_gas_station_80186A40, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80187368[10] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_801873E0[142] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80187618[223] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_records.inc"
};

u16 D_dryfield_night_gas_station_80187994[20] = {
#include "assets/dryfield_night_gas_station_animation_0A3FC_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_801879BC = {
    D_dryfield_night_gas_station_80187618,
    D_dryfield_night_gas_station_80187994,
    { NULL, D_dryfield_night_gas_station_80187368, NULL, NULL, D_dryfield_night_gas_station_801873E0, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_801879E4[2] = {
#include "assets/dryfield_night_gas_station_animation_0A698_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_801879FC[31] = {
#include "assets/dryfield_night_gas_station_animation_0A698_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80187A78[110] = {
#include "assets/dryfield_night_gas_station_animation_0A698_records.inc"
};

u16 D_dryfield_night_gas_station_80187C30[20] = {
#include "assets/dryfield_night_gas_station_animation_0A698_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80187C58 = {
    D_dryfield_night_gas_station_80187A78,
    D_dryfield_night_gas_station_80187C30,
    { NULL, D_dryfield_night_gas_station_801879E4, NULL, NULL, D_dryfield_night_gas_station_801879FC, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80187C80[2] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80187C98[72] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80187DB8[135] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_records.inc"
};

u16 D_dryfield_night_gas_station_80187FD4[20] = {
#include "assets/dryfield_night_gas_station_animation_0AA3C_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80187FFC = {
    D_dryfield_night_gas_station_80187DB8,
    D_dryfield_night_gas_station_80187FD4,
    { NULL, D_dryfield_night_gas_station_80187C80, NULL, NULL, D_dryfield_night_gas_station_80187C98, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_gas_station_80188024[9] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_gas_station_80188090[130] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_bank4.inc"
};

AnimationRecord D_dryfield_night_gas_station_80188298[166] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_records.inc"
};

u16 D_dryfield_night_gas_station_80188530[20] = {
#include "assets/dryfield_night_gas_station_animation_0AF98_indices.inc"
};

GpAnimSet D_dryfield_night_gas_station_80188558 = {
    D_dryfield_night_gas_station_80188298,
    D_dryfield_night_gas_station_80188530,
    { NULL, D_dryfield_night_gas_station_80188024, NULL, NULL, D_dryfield_night_gas_station_80188090, NULL, NULL, NULL },
};

SVECTOR D_dryfield_night_gas_station_80188580[100] = {
    { -58, -397, 0, 0 },
    { -71, -397, 0, 0 },
    { -85, -403, 0, 0 },
    { -98, -408, 0, 0 },
    { -110, -419, 0, 0 },
    { -111, -428, 0, 0 },
    { -112, -433, 0, 0 },
    { -114, -447, 0, 0 },
    { -117, -463, 0, 0 },
    { -121, -472, 0, 0 },
    { -124, -480, 0, 0 },
    { -119, -488, 0, 0 },
    { -109, -497, 0, 0 },
    { -100, -510, 0, 0 },
    { -90, -519, 0, 0 },
    { -79, -530, 0, 0 },
    { -68, -532, 0, 0 },
    { -53, -524, 0, 0 },
    { -37, -516, 0, 0 },
    { -23, -513, 0, 0 },
    { -9, -505, 0, 0 },
    { -1, -513, 0, 0 },
    { 6, -519, 0, 0 },
    { 16, -521, 0, 0 },
    { 26, -532, 0, 0 },
    { 29, -541, 0, 0 },
    { 30, -549, 0, 0 },
    { 25, -557, 0, 0 },
    { 16, -571, 0, 0 },
    { 1, -579, 0, 0 },
    { 11, -588, 0, 0 },
    { 6, -596, 0, 0 },
    { -5, -607, 0, 0 },
    { -14, -621, 0, 0 },
    { -10, -646, 0, 0 },
    { 0, -622, 0, 0 },
    { 8, -645, 0, 0 },
    { 20, -659, 0, 0 },
    { 34, -630, 0, 0 },
    { 45, -648, 0, 0 },
    { 56, -629, 0, 0 },
    { 67, -621, 0, 0 },
    { 78, -607, 0, 0 },
    { 86, -593, 0, 0 },
    { 77, -579, 0, 0 },
    { 68, -574, 0, 0 },
    { 59, -560, 0, 0 },
    { 46, -566, 0, 0 },
    { 32, -582, 0, 0 },
    { 20, -593, 0, 0 },
    { 9, -610, 0, 0 },
    { 10, -598, 0, 0 },
    { 16, -609, 0, 0 },
    { 31, -613, 0, 0 },
    { 52, -627, 0, 0 },
    { 74, -645, 0, 0 },
    { 86, -644, 0, 0 },
    { 97, -639, 0, 0 },
    { 110, -627, 0, 0 },
    { 122, -631, 0, 0 },
    { 113, -637, 0, 0 },
    { 95, -621, 0, 0 },
    { 73, -612, 0, 0 },
    { 51, -604, 0, 0 },
    { 31, -610, 0, 0 },
    { 19, -610, 0, 0 },
    { 5, -623, 0, 0 },
    { -8, -618, 0, 0 },
    { -20, -599, 0, 0 },
    { -18, -590, 0, 0 },
    { 1, -563, 0, 0 },
    { 19, -541, 0, 0 },
    { 34, -521, 0, 0 },
    { 26, -502, 0, 0 },
    { 9, -505, 0, 0 },
    { -5, -491, 0, 0 },
    { -14, -477, 0, 0 },
    { -20, -458, 0, 0 },
    { -29, -477, 0, 0 },
    { -36, -497, 0, 0 },
    { -41, -519, 0, 0 },
    { -38, -543, 0, 0 },
    { -30, -563, 0, 0 },
    { -22, -582, 0, 0 },
    { -6, -593, 0, 0 },
    { 12, -599, 0, 0 },
    { 31, -610, 0, 0 },
    { 44, -593, 0, 0 },
    { 53, -593, 0, 0 },
    { 43, -582, 0, 0 },
    { 24, -582, 0, 0 },
    { 4, -579, 0, 0 },
    { -18, -585, 0, 0 },
    { -40, -590, 0, 0 },
    { -60, -590, 0, 0 },
    { -79, -585, 0, 0 },
    { -91, -563, 0, 0 },
    { -91, -560, 0, 0 },
    { -87, -538, 0, 0 },
    { -83, -538, 0, 0 },
};

void func_dryfield_night_gas_station_80180828(Task*);
void func_dryfield_night_gas_station_80180998(Task*);
void func_dryfield_night_gas_station_80180A60(Task*);
void func_dryfield_night_gas_station_80180B5C(Task*);

TaskDesc D_dryfield_night_gas_station_801888A0[4] = {
    { 0, 192, func_dryfield_night_gas_station_80180828, { .model = NULL } },
    { 2, 192, func_dryfield_night_gas_station_80180998, { .model = NULL } },
    { 0, 192, func_dryfield_night_gas_station_80180A60, { .model = NULL } },
    { 2, 192, func_dryfield_night_gas_station_80180B5C, { .model = NULL } },
};

GpAnimSet* D_dryfield_night_gas_station_801888D0[6] = {
    NULL,
    &D_dryfield_night_gas_station_80184674,
    &D_dryfield_night_gas_station_80184964,
    &D_dryfield_night_gas_station_80184D64,
    &D_dryfield_night_gas_station_80185018,
    &D_dryfield_night_gas_station_801852BC,
};

GpCopyArg D_dryfield_night_gas_station_801888E8 = { { .sets = D_dryfield_night_gas_station_801888D0 }, 6 };

// Retained data: Same five-field layout as the following animation arguments; retained unreferenced entry.
AnimationPlayRequest D_dryfield_night_gas_station_801888F0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188904 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188918 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_8018892C = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_night_gas_station_80188940 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188954 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188968 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpXformArg D_dryfield_night_gas_station_8018897C = { { 0x2F44, 0, -3180, 0 }, { 0, 1444, 0, 0 } };

GpXformArg D_dryfield_night_gas_station_80188994 = { { 1630, 0, -4332, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_dryfield_night_gas_station_801889AC = { { 2670, 0, -4332, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_dryfield_night_gas_station_801889C4 = { { 7767, 0, -2200, 0 }, { 0, 1376, 0, 0 } };

GpOverrideArg D_dryfield_night_gas_station_801889DC = { 2, 51 };

GpAnimSet* D_dryfield_night_gas_station_801889E4[11] = {
    NULL,
    &D_dryfield_night_gas_station_80185538,
    &D_dryfield_night_gas_station_80185708,
    &D_dryfield_night_gas_station_801858D8,
    &D_dryfield_night_gas_station_80185E1C,
    &D_dryfield_night_gas_station_80186904,
    &D_dryfield_night_gas_station_80187340,
    &D_dryfield_night_gas_station_801879BC,
    &D_dryfield_night_gas_station_80187C58,
    &D_dryfield_night_gas_station_80187FFC,
    &D_dryfield_night_gas_station_80188558,
};

GpCopyArg D_dryfield_night_gas_station_80188A10 = { { .sets = D_dryfield_night_gas_station_801889E4 }, 11 };

AnimationPlayRequest D_dryfield_night_gas_station_80188A18 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188A2C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188A40 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188A54 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188A68 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188A7C = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_night_gas_station_80188A90 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188AA4 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188AB8 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_night_gas_station_80188ACC = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_gas_station_80188AE0 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpXformArg D_dryfield_night_gas_station_80188AF4 = { { 0x319C, 0, -2450, 0 }, { 0, 1480, 0, 0 } };

GpXformArg D_dryfield_night_gas_station_80188B0C = { { 0x35CA, 0, -298, 0 }, { 0, 2292, 0, 0 } };

GpXformArg D_dryfield_night_gas_station_80188B24 = { { 7335, 0, -3520, 0 }, { 0, 1024, 0, 0 } };

GpCmdArg D_dryfield_night_gas_station_80188B3C = { { .loc = { 3, 1 } }, 1 };

GpCmdArg D_dryfield_night_gas_station_80188B40 = { { .loc = { 3, 1 } }, 2 };

GpCmdArg D_dryfield_night_gas_station_80188B44 = { { .loc = { 3, 1 } }, 3 };

GpCmdArg D_dryfield_night_gas_station_80188B48 = { { .loc = { 3, 1 } }, 4 };

GpCmdArg D_dryfield_night_gas_station_80188B4C = { { .loc = { 3, 1 } }, 5 };

GpCmdArg D_dryfield_night_gas_station_80188B50 = { { .loc = { 3, 1 } }, 6 };

GpCmdArg D_dryfield_night_gas_station_80188B54 = { { .loc = { 3, 1 } }, 7 };

GpCmdArg D_dryfield_night_gas_station_80188B58 = { { .loc = { 3, 1 } }, 8 };

GpOverlayIds D_dryfield_night_gas_station_80188B5C = { 3, 51, 11 };

GpEvsCmd D_dryfield_night_gas_station_80188B64[6] = {
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_801888E8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_80188A10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188904 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A2C }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_80188BF4[44] = {
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_801888E8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_80188A10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188904 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188AB8 }, { .value = 0 } },
    { 32, { .value = 10 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_8018897C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188AF4 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180BEC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188918 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A54 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A68 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_8018892C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A7C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188B0C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188904 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A2C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B58 }, { .value = 2011 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_8017FBD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_80189014[21] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_8018897C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188B0C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_801888E8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_80188A10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188968 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A18 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_8017FBD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180BEC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B58 }, { .value = 2011 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_8018920C[9] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_801888E8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_80188A10 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188904 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188AA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_801892E4[81] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_801888E8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_gas_station_80188A10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188904 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188A2C }, { .value = 0 } },
    { 32, { .value = 10 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188994 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_dryfield_night_gas_station_801889AC }, { .storage = &D_dryfield_night_gas_station_801889DC } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_801807A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180A00 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180C3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_dryfield_night_gas_station_80188B5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180720 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B3C }, { .value = 2011 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180740 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180C3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180A34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B40 }, { .value = 2011 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180604 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_801807D4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_801889C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188954 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188B24 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_gas_station_80188AE0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B54 }, { .value = 2011 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180B04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_801807D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180B38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B44 }, { .value = 2011 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B48 }, { .value = 2011 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180974 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B4C }, { .value = 2011 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180760 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180920 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_801807D4 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B50 }, { .value = 2011 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_gas_station_80189A7C[22] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180780 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_801889C4 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_gas_station_80188B24 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_gas_station_80188B50 }, { .value = 2011 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_801807D4 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180974 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_gas_station_80180A34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_night_gas_station_80180920 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_night_gas_station_80189C8C[21] = {
    { 0x32F0, -3360, 310, 0 },
    { 0x2D50, -3360, 310, 0 },
    { 0x32F0, -3500, 130, 0 },
    { 0x2D50, -3500, 130, 0 },
    { 3370, -2310, -170, 0 },
    { 2660, -2310, -170, 0 },
    { -300, -3250, -4100, 0 },
    { -300, -3190, -4610, 0 },
    { -6010, -2870, -2940, 0 },
    { -6010, -2870, -4150, 0 },
    { 9000, -4360, -400, 0 },
    { 6300, -4360, -400, 0 },
    { 0x2850, -4110, -5150, 0 },
    { -6930, -4850, -3150, 0 },
    { -6910, -1700, -3150, 0 },
    { 0x45E2, -1800, 6000, 0 },
    { 0x4AEC, -1800, 6000, 0 },
    { 0x70C6, -1800, 6000, 0 },
    { 0x75C6, -1800, 6000, 0 },
    { 0x409C, -250, -2000, 0 },
    { 0x3DC2, -250, -3200, 0 },
};

SVECTOR D_dryfield_night_gas_station_80189D34[4] = {
    { 0x3412, -3360, 320, 0 },
    { 0x3926, -3360, 320, 0 },
    { 0x3412, -3500, 140, 0 },
    { 0x3926, -3500, 140, 0 },
};

s32 D_dryfield_night_gas_station_80189D54[19] = {
    0x600C,
    0x600C,
    0x600C,
    0x600C,
    0x110850,
    0x110850,
    0x100810,
    0x100810,
    0x100810,
    0x100810,
    0x4008,
    0x4008,
    0x4008,
    0x100810,
    0x100810,
    8196,
    8196,
    8196,
    8196,
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_dryfield_night_gas_station_80189DA0[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomCoordRec D_dryfield_night_gas_station_80189DB0[4] = {
    { D_dryfield_night_gas_station_8018FAC0, D_dryfield_night_gas_station_80190684 },
    { D_dryfield_night_gas_station_8018FD78, D_dryfield_night_gas_station_80190684 },
    { D_dryfield_night_gas_station_8018FD78, D_dryfield_night_gas_station_80190684 },
    { D_dryfield_night_gas_station_8018FD78, D_dryfield_night_gas_station_80190684 },
};

GpRoomObjRec D_dryfield_night_gas_station_80189DD0[4] = {
    { &D_dryfield_night_gas_station_8018ABBC, D_dryfield_night_gas_station_8018FD90, D_dryfield_night_gas_station_801900D4, NULL },
    { &D_dryfield_night_gas_station_8018ABBC, D_dryfield_night_gas_station_8018FD90, D_dryfield_night_gas_station_801900D4, NULL },
    { &D_dryfield_night_gas_station_8018ABBC, D_dryfield_night_gas_station_8018FD90, D_dryfield_night_gas_station_801900D4, NULL },
    { D_dryfield_night_gas_station_8018B75C, D_dryfield_night_gas_station_8018FD90, D_dryfield_night_gas_station_801900D4, NULL },
};

u8 D_dryfield_night_gas_station_80189E10[24] = {
    1,
    13,
    14,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    19,
    19,
    18,
    15,
    16,
    17,
    10,
    19,
    12,
    21,
    0,
    0,
    0,
};

u8 D_dryfield_night_gas_station_80189E28[24] = {
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
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

u8 D_dryfield_night_gas_station_80189E40[24] = {
    1,
    2,
    3,
    11,
    5,
    6,
    7,
    8,
    9,
    10,
    4,
    12,
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

u8 D_dryfield_night_gas_station_80189E58[24] = {
    1,
    2,
    3,
    20,
    5,
    6,
    7,
    8,
    9,
    10,
    4,
    12,
    12,
    10,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

u8* D_dryfield_night_gas_station_80189E70[4] = {
    D_dryfield_night_gas_station_80189E10,
    D_dryfield_night_gas_station_80189E28,
    D_dryfield_night_gas_station_80189E40,
    D_dryfield_night_gas_station_80189E58,
};

GpViewCountRec D_dryfield_night_gas_station_80189E80[4] = {
    { { .bytes = { 21, 0 } } },
    { { .bytes = { 21, 0 } } },
    { { .bytes = { 21, 0 } } },
    { { .bytes = { 21, 0 } } },
};

GpWarpRec D_dryfield_night_gas_station_80189E88[3] = {
    { { .words = { 2816, 0x3848, 0, -2630 } }, { 0, 0, 0, 0 }, { .words = { 2816, 0x3848, 0, -1440 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 1024, 294, -5, -3482 } }, { 0, 0, 0, 0 }, { .words = { 1024, 294, -5, -4466 } }, { 0, 0, 0, 0 }, 0x53010002, 0x53010001, 0, 4, 0, 488 },
    { { .words = { 2048, 3039, 0, -433 } }, { 0, 0, 0, 0 }, { .words = { 2048, 4316, 0, -535 } }, { 0, 0, 0, 0 }, 0x53010004, 0x53010003, 0x5301000F, 6, 0, 489 },
};

SVECTOR D_dryfield_night_gas_station_80189F30[34] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_normals.inc"
};

SVECTOR D_dryfield_night_gas_station_8018A040[144] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_verts.inc"
};

GpGridFace D_dryfield_night_gas_station_8018A4C0[65] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_faces.inc"
};

s16 D_dryfield_night_gas_station_8018A7CC[456] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_gas_station_8018A7CC[i])
s16* D_dryfield_night_gas_station_8018AB5C[24] = {
#include "assets/dryfield_night_gas_station_collision_0D5FC_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_gas_station_8018ABBC = { NULL, D_dryfield_night_gas_station_80189F30, D_dryfield_night_gas_station_8018A040, D_dryfield_night_gas_station_8018A4C0, D_dryfield_night_gas_station_8018AB5C, 5043, 6369, 6, 4, 4000, 65 };

SVECTOR D_dryfield_night_gas_station_8018ABE0[32] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_normals.inc"
};

SVECTOR D_dryfield_night_gas_station_8018ACE0[135] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_verts.inc"
};

GpGridFace D_dryfield_night_gas_station_8018B118[64] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_faces.inc"
};

s16 D_dryfield_night_gas_station_8018B418[370] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_gas_station_8018B418[i])
s16* D_dryfield_night_gas_station_8018B6FC[24] = {
#include "assets/dryfield_night_gas_station_collision_0E19C_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_gas_station_8018B75C[1] = {
    { NULL, D_dryfield_night_gas_station_8018ABE0, D_dryfield_night_gas_station_8018ACE0, D_dryfield_night_gas_station_8018B118, D_dryfield_night_gas_station_8018B6FC, 5043, 6369, 6, 4, 4000, 64 },
};

GpViewRec D_dryfield_night_gas_station_8018B780[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { -1914, 0, -3621 }, { -313, 4080, 165 }, { 3607, 354, -1907 } }, { -0x2C6B, 987, 1522 } }, 230 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1185, 0, -3920 }, { -2756, 2912, -833 }, { 2787, 2879, 843 } }, { -0x30B0, 2176, 5231 } }, 312 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1718, 0, 3718 }, { 463, 4064, -214 }, { -3688, 510, 1705 } }, { -0x4844, 1820, 5790 } }, 789 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { -752, 0, -4026 }, { 331, 4082, -61 }, { 4012, -337, -750 } }, { -0x2ED6, 720, 1750 } }, 329 },
    { { { { 3629, 0, 1898 }, { -87, 4091, 166 }, { -1896, -188, 3625 } }, { -9070, 1190, 6110 } }, 541 },
    { { { { -915, 0, -3992 }, { 584, 4051, -134 }, { 3949, -600, -905 } }, { -0x2E7C, 820, 2240 } }, 380 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 1015, 0, 3968 }, { 1689, 3706, -432 }, { -3590, 1743, 919 } }, { -6015, 2185, 5635 } }, 312 },
};

GpSprtCmd D_dryfield_night_gas_station_8018BA74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018BA84[70] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -48, 2385, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -48, 2386, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -48, 2382, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2350, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2291, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 2293, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -40, 2303, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 2322, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -40, 1992, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -40, 1949, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -40, 1880, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -40, 1832, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2275, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2208, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -32, 2200, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -32, 2164, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -32, 2153, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -32, 1741, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -32, 1717, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -32, 1718, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -32, 1875, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 1828, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -32, 1747, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2235, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -24, 1917, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -24, 2082, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -24, 1986, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 1891, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -24, 1700, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 1695, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -24, 1584, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 1563, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -16, 1615, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -16, 1609, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -16, 1578, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 1529, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1485, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -16, 1523, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -16, 1503, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -8, 1528, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 1537, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -8, 1478, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -8, 1484, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 2415, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -24, 2381, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 2141, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -24, 2172, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -24, 2059, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -24, 1710, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -24, 1645, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -8, 1582, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1538, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 0, 1517, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 1519, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 1492, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -8, 1505, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -8, 1550, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 1583, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -16, 1613, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 8, 1061, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 40, 1063, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -56, 2975, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 2950, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -48, 2934, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -48, 2933, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 2823, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 2743, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, -48, 2646, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -48, 2528, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, -40, 2645, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018BFFC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 61, 0, 0, { 1, 0 } },
    { 61, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018C01C[22] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 8, 2387, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 0, 2334, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 0, 2362, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3258, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3126, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 3124, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 3080, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3133, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, 0, 4750, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 0, 4250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3937, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 0, 4125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 2109, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 8, 1954, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 8, 1899, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 16, 1933, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 1952, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018C1D4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 4, 0, 0, { 2, 0 } },
    { 17, 5, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C1FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018C20C[86] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 16, 998, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 16, 1075, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 8, 1012, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 979, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -8, 941, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -8, 924, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -8, 912, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -16, 1000, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -16, 937, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -24, 1062, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, -24, 1125, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -40, -32, 1250, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -24, -40, 1212, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -40, 750, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 40, -40, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -24, 759, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 80, -32, 734, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 120, -32, 708, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -32, 1031, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -32, 1082, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -32, 1094, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -32, 1126, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -32, 1156, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 1191, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 1241, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 1274, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -24, 1000, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -24, 995, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -24, 1018, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -24, 1042, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -24, 1067, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 1098, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 1132, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -24, 1175, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -24, 1226, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -24, 1291, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -24, 1264, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -16, 979, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -16, 979, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 1000, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -16, 1023, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -16, 1047, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 1078, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 1112, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -16, 1154, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -16, 1250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -16, 1250, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -8, 962, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -8, 965, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -8, 986, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -8, 1010, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -8, 1032, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 0, 1115, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 930, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 0, 930, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 0, 888, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -8, 857, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -16, 826, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -24, 800, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, -24, 775, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -40, -32, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -32, -32, 775, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -32, 800, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -72, -8, 917, { .fields = { 80, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -64, -16, 847, { .fields = { 16, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -56, -24, 875, { .fields = { 24, 224 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -48, -24, 846, { .fields = { 24, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -40, -32, 825, { .fields = { 32, 112 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -32, -32, 797, { .fields = { 72, 216 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -24, -32, 803, { .fields = { 96, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 8, 8 } }, -80, -8, 975, { .fields = { 32, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 8, 8 } }, -72, -8, 983, { .fields = { 40, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -64, -16, 927, { .fields = { 24, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -56, -24, 912, { .fields = { 16, 160 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 32, 24 } }, -48, -32, 1225, { .fields = { 120, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 32 } }, -16, -40, 1233, { .fields = { 16, 104 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 48, 24 } }, 0, -40, 1225, { .fields = { 104, 24 } }, 128, 128, 128, 2 },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, -24, 1750, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, -16, 1750, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 72, 8 } }, -160, -8, 1750, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 80, 8 } }, -160, 0, 1750, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 8, 1750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 24, 8 } }, -160, 16, 1750, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, 24, 1750, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 32, 1750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 40, 1750, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018C8C4[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 37, 0, 0, { 0, 0 } },
    { 55, 8, 0, 0, { 5, 0 } },
    { 63, 7, 0, 0, { 1, 0 } },
    { 70, 7, 0, 0, { 4, 0 } },
    { 77, 9, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C904[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C914[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C924[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C934[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018C944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018C954[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 2553, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2276, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 2617, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -96, 0x61A7, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 2422, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -104, 2423, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -80, 2672, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -48, 2664, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 32, 2662, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 48, 2603, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -104, 2681, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -24, 2535, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, -24, 2657, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -120, 2750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 2540, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, -120, 2626, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -40, -120, 2785, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -120, 2981, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 16, 2526, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -160, -32, 1084, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -24, 1200, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -24, 1313, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -16, 1577, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -8, 1831, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 0, 2075, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 24, 2440, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -80, 24, 2698, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 40 } }, -16, 24, 2725, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -16, 1550, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -56, 2601, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -48, 2598, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 2550, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, -32, 2505, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -24, 2648, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, -16, 2666, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, -8, 2675, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 0, 2673, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 8, 2646, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 16, 2558, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 2749, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, 32, 2718, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 40, 2740, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 48, 2676, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 56, 2655, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -16, 4287, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -40, -16, 0xA881, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 64, 16 } }, -88, -8, 4112, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018CD00[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 11, 0, 0, { 0, 0 } },
    { 29, 15, 0, 0, { 5, 0 } },
    { 44, 1, 0, 0, { 1, 0 } },
    { 45, 1, 0, 0, { 4, 0 } },
    { 46, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018CD40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018CD50[90] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -56, 3250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 0x61A7, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -48, 3184, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -48, 3183, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 3000, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 2875, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -48, 2750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -48, 2750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -48, 2750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 8, 1111, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 40, 1163, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -40, 1925, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -40, 1834, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1784, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -32, 1988, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -32, 1978, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 1722, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -24, 1965, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -24, 1956, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -24, 1720, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 1828, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -16, 1725, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -8, 1677, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -8, 1657, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 1625, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 1610, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 1576, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -8, 1566, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -8, 1525, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -8, 1525, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 1558, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -16, 1588, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -16, 1615, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -16, 1555, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -16, 1523, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -48, 2357, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -48, 2380, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -48, 2384, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2296, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2291, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 2293, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -40, 2303, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 2320, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2286, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2208, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -32, 2200, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -32, 2263, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2235, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -40, 1836, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, -40, 1992, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -32, 1751, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -32, 2191, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1759, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -24, 2016, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -24, 2055, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 1747, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -16, 2015, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -16, 1809, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -16, 1736, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 2415, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -24, 2381, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -24, 2141, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -64, -56, 3125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -56, -48, 3000, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -32, -48, 2809, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -16, -48, 2808, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -8, -48, 2875, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 8, -48, 2750, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 56, -48, 2625, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 16, -48, 2625, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, -48, 2625, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -48, 2770, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -40, 2750, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -40, 2925, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -32, 2531, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -24, 2526, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, -16, 2497, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -32, 2964, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -32, -24, 2816, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -32, -16, 2506, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -32, 3108, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -24, 2801, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -48, -8, 2471, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -24, -8, 2329, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 8 } }, 48, -32, 1907, { .fields = { 72, 104 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 8 } }, 64, -32, 1950, { .fields = { 72, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 8 } }, 40, -24, 1909, { .fields = { 72, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 8 } }, 56, -24, 1784, { .fields = { 72, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 8 } }, 72, -24, 1848, { .fields = { 72, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 24, 8 } }, 48, -16, 2027, { .fields = { 64, 224 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_dryfield_night_gas_station_8018D458[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 6, 0 } },
    { 9, 26, 0, 0, { 1, 0 } },
    { 35, 13, 0, 0, { 4, 0 } },
    { 48, 11, 0, 0, { 0, 0 } },
    { 59, 3, 0, 0, { 5, 0 } },
    { 62, 9, 0, 0, { 3, 0 } },
    { 71, 13, 0, 0, { 7, 0 } },
    { 84, 6, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018D4A8[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 8, 2387, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 0, 2334, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 0, 2362, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3113, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3126, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -8, 3124, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 3080, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3133, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, 0, 4250, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 0, 4250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 3937, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 0, 4125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 2109, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 8, 1954, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 8, 1899, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 16, 1933, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 1952, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 3500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 3394, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 3133, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -8, 3101, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 2925, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 2955, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 0, 3008, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 0, 2949, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 64, -8, 3202, { .fields = { 88, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, -8, 3225, { .fields = { 88, 72 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 56, 0, 3083, { .fields = { 88, 56 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 64, 0, 3070, { .fields = { 88, 40 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, 0, 3133, { .fields = { 88, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -8, 0, 4305, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 24 } }, 0, 0, 4378, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 8 } }, 8, 8, 4500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 8 } }, 8, 16, 3566, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 16 } }, -24, 0, 4225, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 16 } }, 8, 0, 4225, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 24 } }, 32, 0, 4175, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 16 } }, 40, 0, 3850, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 24 } }, 24, 0, 3735, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 24 } }, 32, 0, 3749, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 24 } }, 40, 0, 3581, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 24 } }, 48, 0, 3369, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 32 } }, 56, -8, 3100, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 16 } }, 64, 8, 2879, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 32 } }, -16, -16, 3427, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 32 } }, -8, -16, 3470, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 96, 8 } }, -32, 16, 3684, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 80, 8 } }, -24, 24, 2781, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 48, 8 } }, 8, 32, 2449, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018D8E0[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 4, 0, 0, { 5, 0 } },
    { 17, 5, 0, 0, { 4, 0 } },
    { 22, 8, 0, 0, { 6, 0 } },
    { 30, 5, 0, 0, { 1, 0 } },
    { 35, 4, 0, 0, { 7, 0 } },
    { 39, 4, 0, 0, { 3, 0 } },
    { 43, 6, 0, 0, { 8, 0 } },
    { 49, 5, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018D938[143] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, 56, 951, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 48, 945, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -48, 24, 797, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 24, 801, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, 8, 850, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 16, 925, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 16, 700, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 16, 975, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, 16, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 1025, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, 8, 1050, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 0, 1087, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -24, 1076, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, -24, 850, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -32, 675, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, 8, 1007, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 0, 675, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 104, -8, 675, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -32, 675, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -88, 40, 950, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 32, 954, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 56, 750, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 56, 48, 749, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 40, 752, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 24, 750, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, 0, 675, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 104, -8, 675, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, -16, 688, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -16, 691, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -8, 675, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1000, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 801, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 650, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 96, 72, 633, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, 40, 635, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 112, 24, 610, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, 16, 598, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, 8, 580, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -32, 628, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -24, 628, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 1036, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -8, 1075, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -8, 1109, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1137, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1192, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 0, 992, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 0, 968, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 0, 993, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 0, 1028, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 0, 1073, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 0, 1138, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 0, 1194, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 8, 950, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 8, 936, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 8, 959, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 8, 992, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 8, 1027, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 8, 1094, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 8, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 16, 923, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 913, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 16, 930, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 963, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 16, 1000, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 24, 864, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 24, 884, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 24, 908, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 24, 919, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 32, 843, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 32, 815, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 633, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 32, 765, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -8, 24, 726, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 0, 0, 687, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 16, -8, 748, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 737, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -16, 707, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 679, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -16, 633, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 632, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 32, 647, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 48, 24, 611, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 56, 614, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, 24, 610, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -8, 8, 776, { .fields = { 72, 136 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 56 } }, 8, -16, 775, { .fields = { 120, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 40, -16, 803, { .fields = { 32, 208 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 64, -24, 807, { .fields = { 88, 40 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 16 } }, -8, 16, 776, { .fields = { 56, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 24 } }, 8, 0, 1000, { .fields = { 56, 104 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 48, 40 } }, 32, -16, 1025, { .fields = { 88, 208 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 32 } }, 80, -16, 1000, { .fields = { 72, 200 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 32, 40 } }, 96, -24, 715, { .fields = { 88, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, 128, -24, 722, { .fields = { 96, 40 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 8, 32 } }, 152, -24, 721, { .fields = { 72, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4080, { .fields = { 48, 48 } }, -160, 8, 1583, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 16, 64 } }, -112, 0, 1582, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 56, 1861, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 56, 8 } }, -160, 64, 1587, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 64, 8 } }, -160, 72, 1321, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 32, 16 } }, -88, 8, 1544, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 48, 16 } }, -88, 24, 1532, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 16 } }, -96, 40, 1381, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 88, 1030, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 80, 1155, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 32 } }, -64, 40, 838, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 24 } }, -8, 32, 725, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 32, 40 } }, -48, 32, 765, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x40C0, { .fields = { 8, 16 } }, -88, 48, 932, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 24 } }, -80, 48, 867, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 32 } }, -16, 32, 756, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 0, 8, 1094, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 8, 8, 760, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -8, 16, 972, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, 0, 16, 769, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -24, 24, 762, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -16, 24, 762, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -8, 24, 787, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -40, 32, 761, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -32, 32, 838, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 8, 8 } }, -24, 32, 794, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -96, -8, 1000, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -80, -8, 1038, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, -8, 1084, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -48, -8, 1128, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -32, -8, 1171, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -88, 0, 950, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -72, 0, 976, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 0, 1019, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 0, 1060, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -24, 0, 1120, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -8, 0, 1192, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -80, 8, 920, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, 8, 925, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -48, 8, 937, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -32, 8, 975, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -16, 8, 1025, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -72, 16, 897, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 16, 903, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 16, 924, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -24, 16, 954, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -56, 24, 775, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -40, 24, 787, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018E464[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 6, 0 } },
    { 21, 9, 0, 0, { 1, 0 } },
    { 30, 40, 0, 0, { 8, 0 } },
    { 70, 14, 0, 0, { 0, 0 } },
    { 84, 4, 0, 0, { 5, 0 } },
    { 88, 7, 0, 0, { 2, 0 } },
    { 95, 10, 0, 0, { 9, 0 } },
    { 105, 6, 0, 0, { 4, 0 } },
    { 111, 32, 0, 0, { 7, 0 } },
    { 143, 0, 0, 0, { 3, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018E4C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018E4D4[161] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 16, 933, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 824, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 789, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 64, 768, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 64, 768, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 64, 781, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 1137, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 56, 696, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, 48, 713, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 740, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, 48, 741, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 692, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, 48, 671, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 651, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -24, 48, 952, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -16, 48, 956, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -8, 48, 971, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, 16, 960, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, 8, 973, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 16, 48, 952, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, 40, 972, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 8, 681, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 32, 40, 959, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 72, 0, 632, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -8, 582, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 588, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 104, 0, 605, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 0, 570, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 32, 951, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 48, 8, 632, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 16, 625, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 24, 625, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 104, 747, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 96, 741, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 8, 80, 819, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 64, 705, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 64, 697, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 32, 40, 671, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, 24, 645, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 24, 640, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, 24, 648, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 64, 24, 645, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 637, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 24, 649, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 608, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 24, 1027, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 24, 1074, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 32, 977, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 929, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 890, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 1010, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 40, 958, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 48, 918, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 24, 1120, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 32, 1050, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 40, 991, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 48, 942, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 24, 1142, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 32, 1102, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 32, 1195, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 40, 1025, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 40, 992, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 936, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 902, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 975, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -128, 56, 888, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 846, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 1170, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 808, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 80, 776, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 871, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, 64, 944, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 917, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 984, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 48, 923, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 48, 881, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 56, 1005, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 1108, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1153, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 921, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 603, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 48, 56, 588, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 72, 48, 559, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 88, 534, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 40, 578, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 64, 8, 609, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 8, 605, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 651, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, 64, 683, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, 56, 666, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, 48, 653, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 657, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 24, 681, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 645, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 624, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, 16, 599, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -16, 16, 601, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 16, 606, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 72, 589, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 72, 570, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 72, 578, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, 56, 614, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 779, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -72, 48, 663, { .fields = { 64, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -64, 32, 676, { .fields = { 24, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -56, 24, 687, { .fields = { 40, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -32, 16, 646, { .fields = { 72, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, 16, 631, { .fields = { 32, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -88, 56, 768, { .fields = { 80, 136 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -16, 16, 631, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -48, 24, 673, { .fields = { 32, 56 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 16 } }, -88, 56, 697, { .fields = { 64, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 16 } }, -64, 48, 715, { .fields = { 96, 160 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, -48, 24, 923, { .fields = { 96, 80 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 32 } }, -24, 24, 947, { .fields = { 80, 80 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 32, 40 } }, -8, 16, 933, { .fields = { 96, 176 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, 24, 16, 650, { .fields = { 88, 120 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 32, 48 } }, 48, 8, 625, { .fields = { 0, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, 80, 8, 625, { .fields = { 96, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4080, { .fields = { 8, 8 } }, -136, 56, 2317, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 8 } }, -160, 96, 1058, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 48, 1373, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 56, 1372, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 8 } }, -160, 64, 1365, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 48, 8 } }, -160, 72, 1021, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 16, 8 } }, -160, 80, 1340, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 24, 8 } }, -160, 88, 1094, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 40 } }, -160, 80, 799, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x40C0, { .fields = { 16, 48 } }, -104, 72, 744, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 40 } }, -136, 80, 760, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x40C0, { .fields = { 8, 40 } }, -112, 80, 762, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 24, 1020, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 32, 965, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 40, 925, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -160, 48, 875, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -152, 56, 877, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 24, 1065, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 32, 1005, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 40, 950, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -136, 48, 907, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 24, 1116, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 32, 1028, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 40, 973, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -112, 48, 937, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -128, 56, 876, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -104, 56, 911, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -136, 64, 843, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -120, 64, 762, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -128, 72, 770, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -104, 64, 762, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x40C0, { .fields = { 16, 16 } }, -112, 72, 745, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -96, 72, 778, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 64, 694, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 56, 912, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 48, 940, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 40, 976, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 8 } }, -88, 32, 1002, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 16, 8 } }, -64, 32, 740, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 32, 8 } }, -64, 40, 711, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 32, 8 } }, -64, 48, 696, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 24, 24 } }, -104, 80, 662, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_gas_station_8018F168[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 6, 0 } },
    { 32, 12, 0, 0, { 1, 0 } },
    { 44, 43, 0, 0, { 8, 0 } },
    { 87, 16, 0, 0, { 0, 0 } },
    { 103, 8, 0, 0, { 5, 0 } },
    { 111, 8, 0, 0, { 2, 0 } },
    { 119, 8, 0, 0, { 9, 0 } },
    { 127, 4, 0, 0, { 4, 0 } },
    { 131, 29, 0, 0, { 7, 0 } },
    { 160, 1, 0, 0, { 3, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018F1C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018F1D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_gas_station_8018F1E8[57] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 2553, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2526, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 2617, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 2422, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -104, 2423, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -96, 0x61A7, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -80, 2672, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -48, 2664, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 32, 2662, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 48, 2603, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -120, 2750, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -104, 2681, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, -24, 2657, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -24, 2535, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 2540, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, -120, 2626, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -40, -120, 2785, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 8, -120, 2981, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 16, 2526, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -160, -32, 1084, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -24, 1200, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -24, 1313, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -16, 1577, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -8, 1831, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 0, 2075, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 24, 2440, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -80, 24, 2691, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 40 } }, -16, 24, 2725, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, -16, 1550, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -56, 2601, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -48, 2598, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 2550, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, -32, 2505, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -24, 2648, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, -16, 2666, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -8, 2675, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 0, 2673, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 8, 2646, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 16, 2558, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 2749, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, 32, 2718, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 40, 2740, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -24, 48, 2676, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 56, 2675, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -16, 4287, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -40, -16, 0xD08F, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 64, 16 } }, -88, -8, 4112, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 8 } }, -120, 64, 1430, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 40 } }, -56, 16, 1392, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 16, 72 } }, -72, 0, 1396, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 8, 64 } }, -80, 0, 1384, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 24, 56 } }, -104, 0, 1424, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 16, 64 } }, -120, 0, 1425, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4080, { .fields = { 16, 64 } }, -136, 8, 1425, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 72, 32 } }, -128, 16, 1401, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 72, 16 } }, -128, 0, 1450, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x40C0, { .fields = { 72, 24 } }, -128, 0, 1407, { .fields = { 80, 40 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_dryfield_night_gas_station_8018F65C[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { 18, 11, 0, 0, { 5, 0 } },
    { 29, 15, 0, 0, { 4, 0 } },
    { 44, 1, 0, 0, { 6, 0 } },
    { 45, 1, 0, 0, { 1, 0 } },
    { 46, 1, 0, 0, { 7, 0 } },
    { 47, 7, 0, 0, { 3, 0 } },
    { 54, 2, 0, 0, { 8, 0 } },
    { 56, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_gas_station_8018F6B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_gas_station_8018F6C4[21] = {
    { { .empty = D_dryfield_night_gas_station_8018BA74 }, D_dryfield_night_gas_station_8018BA74, NULL },
    { { .elements = D_dryfield_night_gas_station_8018BA84 }, D_dryfield_night_gas_station_8018BFFC, NULL },
    { { .elements = D_dryfield_night_gas_station_8018C01C }, D_dryfield_night_gas_station_8018C1D4, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C1FC }, D_dryfield_night_gas_station_8018C1FC, NULL },
    { { .elements = D_dryfield_night_gas_station_8018C20C }, D_dryfield_night_gas_station_8018C8C4, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C904 }, D_dryfield_night_gas_station_8018C904, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C914 }, D_dryfield_night_gas_station_8018C914, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C924 }, D_dryfield_night_gas_station_8018C924, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C934 }, D_dryfield_night_gas_station_8018C934, NULL },
    { { .empty = D_dryfield_night_gas_station_8018C944 }, D_dryfield_night_gas_station_8018C944, NULL },
    { { .elements = D_dryfield_night_gas_station_8018C954 }, D_dryfield_night_gas_station_8018CD00, NULL },
    { { .empty = D_dryfield_night_gas_station_8018CD40 }, D_dryfield_night_gas_station_8018CD40, NULL },
    { { .elements = D_dryfield_night_gas_station_8018CD50 }, D_dryfield_night_gas_station_8018D458, NULL },
    { { .elements = D_dryfield_night_gas_station_8018D4A8 }, D_dryfield_night_gas_station_8018D8E0, NULL },
    { { .elements = D_dryfield_night_gas_station_8018D938 }, D_dryfield_night_gas_station_8018E464, NULL },
    { { .empty = D_dryfield_night_gas_station_8018E4C4 }, D_dryfield_night_gas_station_8018E4C4, NULL },
    { { .elements = D_dryfield_night_gas_station_8018E4D4 }, D_dryfield_night_gas_station_8018F168, NULL },
    { { .empty = D_dryfield_night_gas_station_8018F1C8 }, D_dryfield_night_gas_station_8018F1C8, NULL },
    { { .empty = D_dryfield_night_gas_station_8018F1D8 }, D_dryfield_night_gas_station_8018F1D8, NULL },
    { { .elements = D_dryfield_night_gas_station_8018F1E8 }, D_dryfield_night_gas_station_8018F65C, NULL },
    { { .empty = D_dryfield_night_gas_station_8018F6B4 }, D_dryfield_night_gas_station_8018F6B4, NULL },
};

GpPointLight D_dryfield_night_gas_station_8018F7C0[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 656, -2500, -4504 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3353, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6775, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8271, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2E46, -2500, -534 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x35FC, -2500, -544 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9661, -2500, -5043 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1861, 6306 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x40BE, -1000, -1697 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 2688, 1778, { 0, 0 } }, 4242, 6423 },
};

GpRoomCoordSet D_dryfield_night_gas_station_8018FAC0[1] = {
    { 0, NULL, 8, D_dryfield_night_gas_station_8018F7C0, 0, NULL },
};

GpPointLight D_dryfield_night_gas_station_8018FAD8[7] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 656, -2500, -4504 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3353, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6775, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8271, -2000, 174 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2E46, -2500, -534 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x35FC, -2500, -544 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9661, -2500, -5043 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1861, 5080 },
};

GpRoomCoordSet D_dryfield_night_gas_station_8018FD78[1] = {
    { 0, NULL, 7, D_dryfield_night_gas_station_8018FAD8, 0, NULL },
};

GpObj4C D_dryfield_night_gas_station_8018FD90[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_night_gas_station_801900D4[16] = {
    { NULL, NULL, NULL, { 2528, -108, -332, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, 0, 3, 49, 2, 0 },
    { NULL, NULL, NULL, { 192, -80, -3616, 0 }, { { 384, 0, -1024, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1024, 0 }, { -384, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1093, 0, 2, 33, 2, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, 2, 5, 255, 2, 0 },
    { NULL, NULL, NULL, { 0x33D2, -96, -5400, 0 }, { { -1173, 0, 517, 0 }, { 466, 0, -1005, 0 }, { -594, 0, 1118, 0 }, { 1301, 0, -628, 0 } }, { 0, 4096, 0, 0 }, { -3290, 0, -2441, 0 }, 1442, 2, 30, 0, 2, 0 },
    { NULL, NULL, NULL, { 7552, -64, -1296, 0 }, { { -1776, 0, -624, 0 }, { 1776, 0, -624, 0 }, { -1776, 0, 624, 0 }, { 1776, 0, 624, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -4096, 0 }, 1881, 2, 25, 0, 2, 0 },
    { NULL, NULL, NULL, { 4400, -96, 240, 0 }, { { -736, 0, -1392, 0 }, { 736, 0, -1392, 0 }, { -736, 0, -272, 0 }, { 736, 0, -272, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 1572, 2, 1, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x367F, -64, -3585, 0 }, { { -756, 0, -780, 0 }, { 1209, 0, 1140, 0 }, { -1432, 0, 333, 0 }, { -331, 0, 1261, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, 2, 15, 0, 4, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x366F, -64, 191, 0 }, { { 1312, 0, -2172, 0 }, { 1289, 0, 271, 0 }, { -1368, 0, -2173, 0 }, { -1365, 0, 272, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 2560, 5, 14, 0, 4, 0 },
    { NULL, NULL, NULL, { 0x31E9, -64, -4760, 0 }, { { -685, 0, -131, 0 }, { -70, 0, -693, 0 }, { 54, 0, 694, 0 }, { 701, 0, 132, 0 } }, { 0, 4102, 0, 0 }, { -2752, 0, 3034, 0 }, 712, 2, 30, 0, 2, 0 },
    { NULL, NULL, NULL, { 4608, -96, -4960, 0 }, { { -1024, 0, -1120, 0 }, { 1024, 0, -1120, 0 }, { -1024, 0, 1120, 0 }, { 1024, 0, 1120, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1514, 2, 23, 255, 3, 0 },
    { NULL, NULL, NULL, { 0x3C4F, -64, -1537, 0 }, { { -365, 0, -1008, 0 }, { 960, 0, 522, 0 }, { -959, 0, -521, 0 }, { 366, 0, 1009, 0 } }, { 0, 4099, 0, 0 }, { -3290, 0, 2440, 0 }, 1086, 2, 24, 0, 2, 0 },
    { NULL, NULL, NULL, { 543, -64, -1793, 0 }, { { -1401, 0, -1255, 0 }, { 1880, 0, 103, 0 }, { -1879, 0, -102, 0 }, { 1402, 0, 1256, 0 } }, { 0, 4108, 0, 0 }, { 2751, 0, -3035, 0 }, 1881, 2, 27, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x31C0, -64, -384, 0 }, { { -2416, 0, -704, 0 }, { 2416, 0, -704, 0 }, { -2416, 0, 704, 0 }, { 2416, 0, 704, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2508, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x3660, -64, -1152, 0 }, { { -1024, 0, -624, 0 }, { 1088, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 1254, 5, 14, 0, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -608, 0 }, { { -480, 0, -560, 0 }, { 480, 0, -560, 0 }, { -480, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 735, 2, 1, 255, 130, 0 },
};

GpAreaTmdRec D_dryfield_night_gas_station_80190594[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_gas_station_801905AC[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_gas_station_801905C4[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_gas_station_801905DC[3] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_gas_station_80190600[3] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 15, 15, 2, 0, { 0, 0 }, D_8016BE28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_gas_station_80190624[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017AC48, D_dryfield_night_gas_station_80190594 },
    { D_map_dryfield_full_8017AD18, D_dryfield_night_gas_station_801905AC },
    { D_map_dryfield_full_8017AD48, D_dryfield_night_gas_station_801905C4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017AD78, D_dryfield_night_gas_station_801905DC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017ADA8, D_dryfield_night_gas_station_80190600 },
};

GpRoomBoundVec D_dryfield_night_gas_station_80190684[22] = {
    { 21, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 618, 618, 618, 618 },
    { 618, 615, 618, 616 },
    { 16, 16, 16, 16 },
    { 618, 618, 618, 618 },
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
};

s32 D_dryfield_night_gas_station_80190734[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

s32 D_dryfield_night_gas_station_80190740[3] = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

s32 D_dryfield_night_gas_station_8019074C[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_dryfield_night_gas_station_80190758[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_gas_station_80190760[1] = {
    { 0, 1, 0, 0, D_dryfield_night_gas_station_80190734 },
};

GpRoomParamRec D_dryfield_night_gas_station_80190768[1] = {
    { 0, 0, 1, 0, D_dryfield_night_gas_station_80190740 },
};

GpRoomParamRec D_dryfield_night_gas_station_80190770[1] = {
    { 0, 0, 1, 0, D_dryfield_night_gas_station_8019074C },
};

GpRoomParamRec D_dryfield_night_gas_station_80190778[1] = {
    { 0, 0, 1, 0, D_dryfield_night_gas_station_80190734 },
};

GpRoomParamRec* D_dryfield_night_gas_station_80190780[8] = {
    D_dryfield_night_gas_station_80190758,
    D_dryfield_night_gas_station_80190760,
    D_dryfield_night_gas_station_80190768,
    D_dryfield_night_gas_station_80190770,
    D_dryfield_night_gas_station_80190778,
    D_dryfield_night_gas_station_80190758,
    D_dryfield_night_gas_station_80190758,
    D_dryfield_night_gas_station_80190758,
};

GpAreaApplyRec D_dryfield_night_gas_station_801907A0[1] = {
    { 255, 0, 0, 0 },
};

Task* D_dryfield_night_gas_station_801907A4 = NULL;

Task* D_dryfield_night_gas_station_801907A8;

Task* D_dryfield_night_gas_station_801907AC;

static void func_dryfield_night_gas_station_8017F41C(Task* arg0);
static void func_dryfield_night_gas_station_8017FAEC(Task* task);
static void func_dryfield_night_gas_station_8017FD80(s32 arg0);
static void func_dryfield_night_gas_station_801802EC(s32 arg0);

#include "../../shared/telephone.inc.c"

void func_dryfield_night_gas_station_8017E9F8(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

static void func_dryfield_night_gas_station_8017F41C(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_gas_station_80184034;
    Game_SetPtrSlot(arg0, 7);
    if ((GameFlag_GetNibble(0x63) >= 2) && (gameGetPtrSlot(0xA) != 0)) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), 0x3E9, &D_dryfield_night_gas_station_80188B0C, 0);
        Gp_AllyAnimId(&D_dryfield_night_gas_station_80184098.source.index);
        Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), ANIMATION_MESSAGE_PLAY, &D_dryfield_night_gas_station_80184098, 0);
        func_dryfield_night_gas_station_8017FBD4(0);
    }
    if (GameFlag_GetNibble(0xA0) == 0) {
        GameFlag_SetNibble(0xA0, 1);
        func_800E3FAC(0xA2, 0x12);
        GameFlag_SetNibble(0x4C, 2);
        func_dryfield_night_gas_station_80180C20();
        if (gameGetPtrSlot(0xA) != 0) {
            func_800E8634(D_dryfield_night_gas_station_801892E4, 0, D_dryfield_night_gas_station_80189A7C);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Answers the room message `in`, copying it to `out` first. For message 2 it
/// reports in `out->field_3` how far nibble 0x61 has advanced (3 once nibble
/// 0x7A reaches 4). Message 3 returns 2 when the session sits at stage 3,
/// place 1 with `Gp_StateF0` agreeing, and 0 while nibble 0x3B is clear;
/// message 2 returns 0 while nibble 0x45 reads 1. The cap commands and nibble
/// write that go with those answers run only when `in->field_5` is clear.
/// Every other case returns 1.
s32 func_dryfield_night_gas_station_8017F544(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 n;
    s32 val;

    *out = *in;
    if (in->prefix.packed == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->field_3 = val;
    }
    if (in->prefix.packed == 3) {
        if ((gGameSession->at4.loc.stage == in->prefix.packed) && (gGameSession->at4.loc.variant == 1) &&
            (Gp_StateF0.prefix.bytes.field_0 == gGameSession->at4.loc.variant)) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x15);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x3B) == 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(7);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
    }
    if (in->prefix.packed == 2) {
        if (GameFlag_GetNibble(0x45) == 1) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 0;
        }
    }
    return 1;
}

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the night gas station, then enqueues it as a type-6 sound event. Event key
/// 0x83 only plays if a cap script is still reporting an event key. Keys with
/// no sound are ignored. Always returns 0.
s32 func_dryfield_night_gas_station_8017F6B8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}

/// Message handler for msg 0x117: walks the `Gp_PendingObj4C` list looking for
/// an object in mode 5 whose `field_48` is 0xFF and which is still pending, and
/// on a hit flips `gGameSession->eventState` / `field_68` and spawns the night gas
/// station cutscene task. Answers 1 only when it found one.
s32 func_dryfield_night_gas_station_8017F7E0(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x117) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            Task_SpawnOnDefaultList(D_dryfield_night_gas_station_8018406C, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_dryfield_night_gas_station_8017F89C(s32 arg0, s32 arg1, s32 arg2)
{
    s16 var_a2;

    if (arg2 == 1) {
        Gp_RunCapCmd1(0x11);
    }
    if (arg2 == 5) {
        if (Gp_HasCollectedBit(0x118) == 0) {
            Gp_HasCollectedBit(0x117);
            var_a2 = 0;
        } else {
            var_a2 = 1;
        }
        Gp_StartCapSlot(0x12, 1, var_a2);
    }
    if ((arg2 == 0x17) && (gGameSession->at4.loc.room == 4)) {
        if (Gp_HasCollectedBit(0x11E) != 0) {
            if (GameFlag_GetNibble(0xD4) == 0) {
                GameFlag_SetNibble(0xD4, 1);
            } else {
                GameFlag_SetNibble(0xD4, 2);
            }
        }
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xD4) != 0 ? (GameFlag_GetNibble(0xD4) == 1 ? 0x20 : 0x1F) : arg2, 0);
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_dryfield_night_gas_station_80184034`:
/// the directed action selected by `field_2` 0xE runs the room's cutscene script
/// blob at `D_dryfield_night_gas_station_8018920C`, but only once nibble 0x63 has
/// reached 2 and pointer slot 0xA is live.
s32 func_dryfield_night_gas_station_8017F990(Task* task, s32 msgId, GpMsg13EF* msg)
{
    if ((msg->field_2 == 0xE) && (gameGetPtrSlot(0xA) != NULL) && (GameFlag_GetNibble(0x63) >= 2)) {
        func_800E8614(D_dryfield_night_gas_station_8018920C, 0);
    }
    return 0;
}

/// Arms the room's night sequence, once: while nibble 0x63 is still clear it
/// sets that nibble, plays the script blob at
/// `D_dryfield_night_gas_station_80188B64`, raises `gGameSession->flowFlags`
/// bit 0x80, applies the room's area records, clears nibbles 0x62 and 0x45 and
/// queues sound event 0x64.
s32 func_dryfield_night_gas_station_8017F9E8(void)
{
    if (GameFlag_GetNibble(0x63) == 0) {
        GameFlag_SetNibble(0x63, 1);
        func_800E8614(D_dryfield_night_gas_station_80188B64, 1);
        gGameSession->flowFlags |= 0x80;
        Gp_ApplyAreaRecs(D_dryfield_night_gas_station_801907A0);
        GameFlag_SetNibble(0x62, 0);
        GameFlag_SetNibble(0x45, 0);
        SndEvt_EnqueueType2(0, 0x64);
    }
    return 0;
}

/// Tears the room's scripted sequence down: raises `gGameSession->hideHud`
/// and `D_80115768`, hides the display, clears collection bit 0x117, installs
/// the room's two cap files, runs the 0xA2/0x16 event and kills its own task.
void func_dryfield_night_gas_station_8017FA6C(Task* arg0)
{
    gGameSession->hideHud = 1;
    D_80115768            = 1;
    SetDispMask(0);
    Gp_ClearCollectedBit(0x117);
    func_800E8634(D_dryfield_night_gas_station_801840AC, 0, D_dryfield_night_gas_station_801841FC);
    func_800E3FAC(0xA2, 0x16);
    Mc_SaveData[0].state.sceneEvent = 4;
    taskKill(arg0);
}

/// Runs the room's one-shot post-sequence event: with the session still on its
/// first mode and nibble 0x63 reading 1 — and the cutscene flag agreeing — it
/// advances the nibble to 2 and plays the cap pair
/// `D_dryfield_night_gas_station_80188BF4` / `_80189014`.
static void func_dryfield_night_gas_station_8017FAEC(Task* task)
{
    s32 temp_v0;

    if (gGameSession->eventState == 0) {
        temp_v0 = GameFlag_GetNibble(0x63);
        if ((temp_v0 == 1) && (Gp_StateC08.field_A != temp_v0)) {
            GameFlag_SetNibble(0x63, 2);
            func_800E8634(D_dryfield_night_gas_station_80188BF4, 0, D_dryfield_night_gas_station_80189014);
        }
    }
}

/// Stores `arg0` in `D_80115768`.
void func_dryfield_night_gas_station_8017FB64(u8 arg0)
{
    D_80115768 = arg0;
}

/// The three states of the room's main task, run by
/// `func_dryfield_night_gas_station_8017FB70`: set-up, the per-frame handler
/// and the kill.
static const TaskFuncTable3 D_dryfield_night_gas_station_8017D644 = {
    {
        func_dryfield_night_gas_station_8017F41C,
        func_dryfield_night_gas_station_8017FAEC,
        taskKill,
    },
};

/// Offset added to a marker's position before it is projected.
static const SVECTOR D_dryfield_night_gas_station_8017D650 = { 0x3B23, -0x498, -0xD76, 0 };

/// The lamp beam's direction vector in the lamp's model space.
static const SVECTOR D_dryfield_night_gas_station_8017D658 = { -0x1E, 0x122, 0x28, 0 };

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

/// Resets the live layout lists from the template: the four-entry vector list
/// and its 12-byte records, then the eight-entry list, which is afterwards
/// raised by 0xBB8 on y when `arg0` is nonzero.
void func_dryfield_night_gas_station_8017FBD4(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_gas_station_8018ABBC;
    src = &D_dryfield_night_gas_station_80184374;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0xBB8;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Draws a marker for entry `arg0` of `D_dryfield_night_gas_station_80188580`:
/// the vector is turned by a fixed -0x262 yaw, offset by
/// `D_dryfield_night_gas_station_8017D650` and projected through
/// `gGfxViewCoord.workm`. When the projection passes, a semi-transparent 3x3 dark
/// red `TILE` with its draw-mode `DR_TPAGE` and a bright red `TILE_1` mark the
/// point. The point is then projected again and once more displaced by
/// (-0x3E8, +0x1F4, +0x1F4), and a semi-transparent `LINE_G2` runs from the
/// displaced point's x, 10 pixels below the point, back to the point, shading
/// from black to a red that flickers with `rand()`. Everything is linked into
/// OT slot 0xA.
static void func_dryfield_night_gas_station_8017FD80(s32 arg0)
{
    SVECTOR     off;
    OverlayMat  mtx;
    SVECTOR     pos;
    long        sxy;
    long        p;
    long        flag;
    u16         x0;
    u16         y0;
    u16         x1;
    u16         y1;
    u16         x2;
    SVECTOR*    vec;
    s32         val;
    s32         one;
    OverlayMat* m;
    TILE*       tile;
    TILE_1*     tile1;
    LINE_G2*    line;
    DR_TPAGE*   dr;

    off               = D_dryfield_night_gas_station_8017D650;
    one               = ONE;
    m                 = &mtx;
    mtx.ident.m00_m01 = one;
    mtx.ident.m02_m10 = 0;
    m->ident.m11_m12  = one;
    mtx.ident.m20_m21 = 0;
    m->ident.m22      = one;
    RotMatrixY((s16)(-0x262), &mtx.mat);
    vec = &D_dryfield_night_gas_station_80188580[arg0];
    ApplyMatrixSV(&mtx.mat, vec, &pos);
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    pos.vx += off.vx;
    pos.vy += off.vy;
    pos.vz += off.vz;
    RotTransPers(&pos, &sxy, &p, &flag);
    if (flag >= 0) {
        val            = 0xA;
        x0             = sxy;
        y0             = sxy >> 16;
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setTile(tile);
        setSemiTrans(tile, 1);
        tile->x0 = x0 - 1;
        tile->y0 = y0 - 1;
        setRGB0(tile, 0x80, 0, 0);
        tile->w = 3;
        tile->h = 3;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)val << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), tile);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)val << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), dr);
        tile1          = (TILE_1*)gGpuPrimCursor;
        gGpuPrimCursor = tile1 + 1;
        setTile1(tile1);
        setSemiTrans(tile1, 1);
        tile1->x0 = x0;
        tile1->y0 = y0;
        setRGB0(tile1, 0xFF, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)val << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), tile1);

        mtx.ident.m00_m01 = one;
        mtx.ident.m02_m10 = 0;
        m->ident.m11_m12  = one;
        mtx.ident.m20_m21 = 0;
        m->ident.m22      = one;
        val               = -0x262;
        RotMatrixY((s16)(val), &mtx.mat);
        ApplyMatrixSV(&mtx.mat, vec, &pos);
        SetRotMatrix(&gGfxViewCoord.workm);
        SetTransMatrix(&gGfxViewCoord.workm);
        pos.vx += off.vx;
        pos.vy += off.vy;
        pos.vz += off.vz;
        RotTransPers(&pos, &sxy, &p, &flag);
        x1      = sxy;
        y1      = sxy >> 16;
        pos.vx -= 0x3E8;
        pos.vy += 0x1F4;
        pos.vz += 0x1F4;
        RotTransPers(&pos, &sxy, &p, &flag);
        x2             = sxy;
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x2;
        line->y0 = y1 + 0xA;
        line->x1 = x1;
        line->y1 = y1;
        setRGB1(line, rand() % 60 + 0x50, 0, 0);
        val = 0xA;
        setRGB0(line, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)val << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)val << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), dr);
    }
}

/// Draws the room's lamp beam. The matrix comes from the slot-0xA model's
/// per-part coordinate array, 0x280 (part 8) in, composed into an identity
/// rotation on the stack; the beacon vector `D_dryfield_night_gas_station_8017D658`
/// is rotated by it twice - as-is and 0x190 further up - and both points are
/// added to that world position and projected through `gGfxViewCoord.workm`. Once
/// both `RotTransPers` FLAG words pass, a semi-transparent `LINE_G2` between
/// them, whose red channel flickers with `rand() % 100 - 0x7E`, and its
/// draw-mode `DR_TPAGE` are linked into OT slot 0xA.
static void func_dryfield_night_gas_station_801802EC(s32 arg0)
{
    MATRIX    mtx;
    SVECTOR   pos;
    SVECTOR   p0;
    SVECTOR   p1;
    SVECTOR   off;
    long      sxy;
    long      p;
    long      flag0;
    long      flag1;
    u16       x0;
    u16       y0;
    u16       x1;
    u16       y1;
    s32       one;
    MATRIX*   m;
    GfxCoord* coord;
    LINE_G2*  line;
    DR_TPAGE* dr;

    off                  = D_dryfield_night_gas_station_8017D658;
    one                  = ONE;
    m                    = &mtx;
    *(s32*)&mtx          = one;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = one;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = one;
    coord                = gameGetPtrSlot(0xA)->extra.tmd->coords;
    Gp_ComposeParentWorld(&coord[8], m, &pos);
    ApplyMatrixSV(&mtx, &off, &p0);
    p0.vx  += pos.vx;
    p0.vy  += pos.vy;
    p0.vz  += pos.vz;
    off.vy += 0x190;
    ApplyMatrixSV(&mtx, &off, &p1);
    p1.vx += pos.vx;
    p1.vy += pos.vy;
    p1.vz += pos.vz;
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    RotTransPers(&p0, &sxy, &p, &flag0);
    x0 = sxy;
    y0 = sxy >> 16;
    RotTransPers(&p1, &sxy, &p, &flag1);
    x1 = sxy;
    y1 = sxy >> 16;
    if (flag0 >= 0 && flag1 >= 0) {
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        setRGB0(line, rand() % 100 - 0x7E, 0, 0);
        setRGB1(line, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), dr);
    }
}

/// Bursts the room's lamp effect: `arg0` picks one of three spawn-argument
/// triples and the effect's `arg2`, the spark is spawned at the lamp task's own
/// coordinate, and the lamp is then told to light up. Any other `arg0` only
/// switches the lamp back to dark.
void func_dryfield_night_gas_station_80180604(s32 arg0)
{
    GpWorkObj* work;
    GfxCoord*  coord;
    SVECTOR    offset;

    work = Gp_FindWorkById(gGameSession->at4.loc.area | ((gGameSession->at4.loc.stage << 8) | 0x2000));
    if (work != NULL) {
        coord = (work->field_0)->extra.tmd->coords;
        switch (arg0) {
            case 0:
                offset.vx = 0;
                offset.vy = -0x64;
                offset.vz = -0x12C;
                Gp_SpawnEff(0x600E0, coord, 0x300, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 1:
                offset.vx = 0xC8;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 2:
                offset.vx = -0x64;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            default:
                func_dryfield_night_gas_station_80180DC8(0);
                break;
        }
    }
}

void func_dryfield_night_gas_station_80180720(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_dryfield_night_gas_station_80180740(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_dryfield_night_gas_station_80180760(void)
{
    Gp_RestoreStreamRng();
}

void func_dryfield_night_gas_station_80180780(void)
{
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns entry 0 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A4`.
void func_dryfield_night_gas_station_801807A0(void)
{
    D_dryfield_night_gas_station_801907A4 = Task_SpawnFromTable(D_dryfield_night_gas_station_801888A0, 0, 0, 0);
}

/// Passes `arg0` to the task tracked in `D_dryfield_night_gas_station_801907A4`
/// as its `spawnArg1` when it is 0 or 1; any other value kills the task and
/// clears the handle.
void func_dryfield_night_gas_station_801807D4(s32 arg0)
{
    Task* t = D_dryfield_night_gas_station_801907A4;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1.value = arg0;
    return;
kill:
    taskKill(D_dryfield_night_gas_station_801907A4);
    D_dryfield_night_gas_station_801907A4 = NULL;
}

/// The room's tracked-task timer, run once a frame while `D_801156F9` is
/// clear. The slot-3 pointer going away forces `state` to -1, which retires
/// the task on the following test; a live one moves `killCountdown` a step of
/// 0x100 towards 0x1000 (or zero), then passes it with the slot-3 and slot-0xA
/// objects to `func_800B0928`.
void func_dryfield_night_gas_station_80180828(Task* task)
{
    Task* owner;
    u16   tick;

    owner = gameGetPtrSlot(3);
    if (D_801156F9 == 0) {
        if (owner == 0) {
            task->state = -1;
        }
        if (task->state == 0) {
            if (task->spawnArg1.value != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            func_800B0928(owner, gameGetPtrSlot(0xA), 0x300, 0x10, task->killCountdown);
            return;
        }
        taskKill(task);
        D_dryfield_night_gas_station_801907A4 = 0;
    }
}

void func_dryfield_night_gas_station_80180920(s32 arg0)
{
    Gp_ArmStateF0(arg0);
}

/// Spawns entry 1 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A8`.
void func_dryfield_night_gas_station_80180940(void)
{
    D_dryfield_night_gas_station_801907A8 = Task_SpawnFromTable(D_dryfield_night_gas_station_801888A0, 1, 0, 0);
}

/// Retires the room's third tracked task and drops the room's reference to it.
/// The `-1` state is the task's own exit request, so the task frees itself on
/// its next tick.
void func_dryfield_night_gas_station_80180974(void)
{
    if (D_dryfield_night_gas_station_801907A8 != NULL) {
        D_dryfield_night_gas_station_801907A8->state = -1;
        D_dryfield_night_gas_station_801907A8        = NULL;
    }
}

/// Runs the room's countdown timer task: for its first 100 ticks it pulses
/// `func_dryfield_night_gas_station_8017FD80` and counts up, then kills itself.
void func_dryfield_night_gas_station_80180998(Task* arg0)
{
    s16 temp_a0;

    if (arg0->state == 0) {
        temp_a0 = arg0->killCountdown;
        if (temp_a0 < 0x64) {
            func_dryfield_night_gas_station_8017FD80(temp_a0);
            arg0->killCountdown = (u16)arg0->killCountdown + 1;
            return;
        }
    }
    taskKill(arg0);
}

/// Spawns the room's second tracked task (entry 2 of the room's task table) and
/// stores it beside `D_dryfield_night_gas_station_801907A8`.
void func_dryfield_night_gas_station_80180A00(void)
{
    D_dryfield_night_gas_station_801907AC = Task_SpawnFromTable(D_dryfield_night_gas_station_801888A0, 2, 0, 0);
}

/// Advances the room's second tracked task by one state and drops the room's
/// reference to it: the task carries on with its own schedule, untracked.
void func_dryfield_night_gas_station_80180A34(void)
{
    if (D_dryfield_night_gas_station_801907AC != NULL) {
        D_dryfield_night_gas_station_801907AC->state++;
        D_dryfield_night_gas_station_801907AC = NULL;
    }
}

/// Steps the room's blinking-light table: each tick it re-derives whether the
/// current entry's `vx` is odd, and when that flag flips it republishes it to
/// `func_dryfield_night_gas_station_80180DC8` (which switches the lamp effect
/// between its on and off appearance, or back to dark for the -1 state). The
/// index runs to 100 and then wraps.
void func_dryfield_night_gas_station_80180A60(Task* arg0)
{
    s16 temp_v0_2;
    s32 temp_v0;

    if (arg0->state == 0) {
        temp_v0 = (D_dryfield_night_gas_station_80188580[arg0->killCountdown].vx & 1) ^ 1;
        if (arg0->spawnArg1.value != temp_v0) {
            arg0->spawnArg1.value = temp_v0;
            func_dryfield_night_gas_station_80180DC8((s16)arg0->spawnArg1.value);
        }
        temp_v0_2           = (u16)arg0->killCountdown + 1;
        arg0->killCountdown = temp_v0_2;
        if (temp_v0_2 >= 0x64) {
            arg0->killCountdown = 0;
        }
    } else {
        func_dryfield_night_gas_station_80180DC8(0);
        taskKill(arg0);
    }
}

/// Spawns the room's third tracked task (entry 3 of the room's task table) and stores
/// it in `D_dryfield_night_gas_station_801907A8`, the slot the room's teardown clears.
void func_dryfield_night_gas_station_80180B04(void)
{
    D_dryfield_night_gas_station_801907A8 = Task_SpawnFromTable(D_dryfield_night_gas_station_801888A0, 3, 0, 0);
}

/// Second teardown entry point for `D_dryfield_night_gas_station_801907A8`: requests the
/// task's exit and drops the room's reference to it, exactly as
/// `func_dryfield_night_gas_station_80180974` does.
void func_dryfield_night_gas_station_80180B38(void)
{
    if (D_dryfield_night_gas_station_801907A8 != NULL) {
        D_dryfield_night_gas_station_801907A8->state = -1;
        D_dryfield_night_gas_station_801907A8        = NULL;
    }
}

/// Runs the room's countdown task: seeds the RNG on its first tick, then for
/// 100 ticks pulses `func_dryfield_night_gas_station_801802EC` and counts up,
/// then kills itself.
void func_dryfield_night_gas_station_80180B5C(Task* arg0)
{
    s16 temp_a0;

    switch (arg0->state) {
        case 0:
            srand(1);
            arg0->state += 1;
            /* fallthrough */
        case 1:
            temp_a0 = arg0->killCountdown;
            if (temp_a0 < 0x64) {
                func_dryfield_night_gas_station_801802EC(temp_a0);
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            taskKill(arg0);
            return;
    }
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_dryfield_night_gas_station_80180BEC(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

static void func_dryfield_night_gas_station_80180C20(void)
{
    D_dryfield_night_gas_station_801907A4 = 0;
    D_dryfield_night_gas_station_801907A8 = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

/// Hides or shows sprite commands 6 and 7 of five of the area's views on its
/// own argument, through their `GpSprtCmd::field_4`: cleared for a 0 argument,
/// set for a 1, and any other argument changes nothing. The first view takes
/// only command 6 and the last two only command 7;
/// `func_dryfield_night_gas_station_80180DC8` drives commands 8 to 10 of the
/// last three views instead.
void func_dryfield_night_gas_station_80180C3C(s32 arg0)
{
    GameLocationKey* sess;
    GpSprtRec*       rec;
    GpSprtCmd*       view;
    s32              flag;

    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    flag = arg0 & 0xFF;

    switch (flag) {
        case 0:
            view            = rec[4].field_4;
            view[6].field_4 = 0;
            view            = rec[12].field_4;
            view[6].field_4 = 0;
            view[7].field_4 = 0;
            view            = rec[13].field_4;
            view[6].field_4 = 0;
            view[7].field_4 = 0;
            view            = rec[14].field_4;
            view[7].field_4 = 0;
            view            = rec[16].field_4;
            view[7].field_4 = 0;
            break;
        case 1:
            view            = rec[4].field_4;
            view[6].field_4 = flag;
            view            = rec[12].field_4;
            view[6].field_4 = flag;
            view[7].field_4 = flag;
            view            = rec[13].field_4;
            view[6].field_4 = flag;
            view[7].field_4 = flag;
            view            = rec[14].field_4;
            view[7].field_4 = flag;
            view            = rec[16].field_4;
            view[7].field_4 = flag;
            break;
    }
}

/// Gates the room's two sprite command records on game flag nibble 0x8D: a
/// zero nibble clears both commands' skip-link flag, a one sets it. The two
/// records are views 10 and 19 of the current room's sprite record array, and
/// the flag both write is command 6's.
static void func_dryfield_night_gas_station_80180D1C(void)
{
    GameLocationKey* sess = &gGameSession->at4.loc;
    GpSprtRec*       view = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1];
    s32              flag = GameFlag_GetNibble(0x8D);

    switch (flag) {
        case 0:
            view[10].field_4[6].field_4 = 0;
            view[19].field_4[6].field_4 = 0;
            break;
        case 1:
            view[10].field_4[6].field_4 = flag;
            view[19].field_4[6].field_4 = flag;
            break;
    }
}

/// Switches the room's lamp effect between its lit and dark appearance: the
/// current room's three lamp views have their three flags written to 1 for the
/// 0 argument and to 0 for the 1 argument, and any other argument changes
/// nothing. `func_dryfield_night_gas_station_80180A60` drives it from the
/// blinking-light table, whose own exit passes 0.
static void func_dryfield_night_gas_station_80180DC8(s16 arg0)
{
    GameLocationKey* sess = &gGameSession->at4.loc;
    GpSprtRec*       rec =
        Gp_SprtTables[sess->stage - 1][0]
            .field_0[sess->area - 1];
    GpSprtCmd* view;

    switch (arg0) {
        case 0:
            view             = rec[13].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view             = rec[14].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view[10].field_4 = 1;
            view             = rec[16].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view[10].field_4 = 1;
            break;
        case 1:
            view             = rec[13].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view             = rec[14].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view[10].field_4 = 0;
            view             = rec[16].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view[10].field_4 = 0;
            break;
    }
}

/// Room effect task tick. The first tick installs the room's three effect ids
/// and sets `roomEffectMode` to 2. Every tick it draws the anchors whose view
/// mask includes the current view. While game flag nibble 0x63 is clear, and
/// outside battles and events, each of the two scatter anchors rolls a jittered
/// spawn position and one of three effects (0x60080, 0x6008D, or 0x60070 on a
/// further 1-in-3). Once the nibble is set, and only if it was seen clear
/// before, the anchors keep spawning 0x60070 alone on a 1-in-3.
void func_dryfield_night_gas_station_80180E9C(Task* task)
{
    DryfieldNightGasStationEffWork* work;
    GfxCoord*                       coord;
    s32                             mask;
    s32                             i;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    mask  = 1 << Gp_GetViewIndex();
    if (task->state == 0) {
        D_80115758                 = 0x60006;
        D_8011572C                 = 0x60008;
        D_80115750                 = 0x60009;
        Gp_State1C->roomEffectMode = 2;
    }
    for (i = 0; i < 10; i += 2) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189C8C[i], 0x180, 0x222);
        }
    }
    if (mask & 0x600C) {
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[0], 0x180, 0x444);
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[2], 0x180, 0x444);
    }
    for (i = 10; i < 19; i++) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_80181AF8(&D_dryfield_night_gas_station_80189C8C[i], 0, 0x380);
        }
    }
    if (GameFlag_GetNibble(0x63) == 0) {
        work->active = 1;
        if (Gp_State1C->battleState != 1 && Gp_State1C->eventState == 0) {
            for (i = 19; i < 21; i++) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->kind   = (Gp_LcgState >> 16) % 3;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                if (work->kind == 0) {
                    Gp_SpawnEff(0x60080, coord, 0x10300, &work->pos);
                } else if (work->kind == 1) {
                    Gp_SpawnEff(0x6008D, coord, 0x300, &work->pos);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                        Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
                    }
                }
            }
        }
    } else if (work->active != 0 && Gp_State1C->eventState == 0) {
        for (i = 19; i < 21; i++) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
            }
        }
    }
}

/// Projects the world-space points `arg0[0]` and `arg0[1]` through
/// `gGfxViewCoord.workm` and, when both project, joins them with a capsule of
/// gouraud `POLY_G4`s: a half-disc wedge fan around each point and a quad
/// strip between them, three quads per 0x400 step of the angle between the
/// two centres. `arg1` is a signed half-extent scaled by depth; the lit
/// vertices take `arg2` as three 4-bit channels, blended with the frame
/// counter's low bit, and the rim is black.
static void func_dryfield_night_gas_station_801812B4(SVECTOR* arg0, s32 arg1, s32 arg2)
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
/// it projects, queues one semi-transparent `POLY_FT4` centred on it: tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380` and the 40-texel texture column
/// `arg1`. `arg2` is a signed half-extent scaled by depth; the grey level
/// follows the frame counter's low bit.
static void func_dryfield_night_gas_station_80181AF8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

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
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Effect task of a growing glow: over `spawnArg1` ticks it widens and
/// brightens a disc and ring around the task's object, then flashes the
/// screen and fades a two-ring glow out before releasing itself.
void func_dryfield_night_gas_station_80181D80(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_dryfield_night_gas_station_80182450(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_dryfield_night_gas_station_80182450(coord, (s16)((u16)work->angle * 2), rgb);
                func_dryfield_night_gas_station_80182024(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_dryfield_night_gas_station_80183354(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, when it projects. The ring runs from radius
/// `(s16)arg1 * 64 / (otz + 1)`, which is black, to
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`, which takes the colour `rgb`.
static void func_dryfield_night_gas_station_80182024(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
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
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues eight gouraud `POLY_G4` wedges around it, lit by `rgb` at
/// the centre and black at the rim; `arg1` is a signed half-extent scaled by
/// depth.
static void func_dryfield_night_gas_station_80182450(GfxCoord* arg0, s16 arg1, u8* rgb)
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Effect task of a beam trail: allocates two eight-slot coordinate rings,
/// then each tick records the beam's two end points into the next slot and
/// draws the trail, releasing itself after `spawnArg1` ticks.
void func_dryfield_night_gas_station_801827E4(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_dryfield_night_gas_station_80189DA0[0].vx;
                objCoord->coord.t[1]   = D_dryfield_night_gas_station_80189DA0[0].vy;
                objCoord->coord.t[2]   = D_dryfield_night_gas_station_80189DA0[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_dryfield_night_gas_station_80189DA0[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_dryfield_night_gas_station_80189DA0[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_night_gas_station_80182CD4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the trail between the coordinate rings `arg0` and `arg1` as seven
/// gouraud `POLY_G4` quads, walking back from slot `arg2` and fading with
/// age; `arg3` packs three 2-bit colour channels at bits 8, 4 and 0.
static void func_dryfield_night_gas_station_80182CD4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Effect task of a burst: spawns its particle effects, then either scatters
/// sparks for a few ticks (`spawnArg1` non-zero) or expands two rings, and
/// releases itself.
void func_dryfield_night_gas_station_801830CC(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_dryfield_night_gas_station_80182024(objCoord, 0x100, 0x100, rgb);
            func_dryfield_night_gas_station_80182024(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues two rings of gouraud `POLY_G4` wedges: an inner one
/// lit by `arg2` at full strength and an outer one at half, both black at the
/// rim. `arg1` is a signed half-extent scaled by depth.
static void func_dryfield_night_gas_station_80183354(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
