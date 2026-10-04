#include "rooms/shelter_b1_sterilization_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "shelter_b1_sterilization_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"
/// Signed texture-frame index for the drifting effect's five-column sprite grid.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

#define D_shelter_b1_sterilization_room_80189334 (D_shelter_b1_sterilization_room_8018909C + 83)

extern EvsCommand D_shelter_b1_sterilization_room_80188C94[];
extern EvsCommand D_shelter_b1_sterilization_room_80188E14[];

static void func_shelter_b1_sterilization_room_80183B8C(SVECTOR* arg0, s32 arg1, s32 arg2);

// Indexed views below share one contiguous table.
extern WorldCoordRoomAmbientEntry D_shelter_b1_sterilization_room_8018C21C[25];
extern WorldCoordRoomLights       D_shelter_b1_sterilization_room_8018B630[1];

void func_shelter_b1_sterilization_room_801814B0(void);
void func_shelter_b1_sterilization_room_80181698(s32);

extern WorldCollisionOccluder D_shelter_b1_sterilization_room_8018C1A4[2];
extern WorldCollisionTrigger  D_shelter_b1_sterilization_room_8018B648[8];

void func_shelter_b1_sterilization_room_801815EC(void);
void func_shelter_b1_sterilization_room_80181658(void);

DamageAttack D_shelter_b1_sterilization_room_80188738 = { 25, 0 };

EvsCommand D_shelter_b1_sterilization_room_8018873C[37] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188590 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_801885AC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_8018118C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_801885C0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188638 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_801885E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_801885D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100010 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_8018118C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_801815EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_80181658 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_sterilization_room_80188AB4[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_80181658 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54100010 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_8018118C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_801815EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188590 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188638 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_sterilization_room_80188C94[16] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188590 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_801885FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188650 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_sterilization_room_80188610 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_801814B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_sterilization_room_80188E14[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_sterilization_room_80188650 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b1_sterilization_room_801814B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_sterilization_room_80188ED4[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_80181698 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100014 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100015 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_80181698 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_sterilization_room_80188FDC[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b1_sterilization_room_80181698 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_shelter_b1_sterilization_room_8018909C[87] = {
    { 4020, -2640, 6625, 0 },
    { 4020, -2640, 6775, 0 },
    { 4020, -2640, 6975, 0 },
    { 4020, -2640, 7125, 0 },
    { 4020, -2640, 7725, 0 },
    { 4020, -2640, 7875, 0 },
    { 4020, -2640, 8075, 0 },
    { 4020, -2640, 8225, 0 },
    { 4020, -2640, 8775, 0 },
    { 4020, -2640, 8925, 0 },
    { 4020, -2640, 9125, 0 },
    { 4020, -2640, 9275, 0 },
    { 4020, -2640, 9875, 0 },
    { 4020, -2640, 10025, 0 },
    { 4020, -2640, 10225, 0 },
    { 4020, -2640, 10375, 0 },
    { 6730, -2640, 6625, 0 },
    { 6730, -2640, 6775, 0 },
    { 6730, -2640, 6975, 0 },
    { 6730, -2640, 7125, 0 },
    { 6730, -2640, 7725, 0 },
    { 6730, -2640, 7875, 0 },
    { 6730, -2640, 8075, 0 },
    { 6730, -2640, 8225, 0 },
    { 6730, -2640, 8775, 0 },
    { 6730, -2640, 8925, 0 },
    { 6730, -2640, 9125, 0 },
    { 6730, -2640, 9275, 0 },
    { 6730, -2640, 9875, 0 },
    { 6730, -2640, 10025, 0 },
    { 6730, -2640, 10225, 0 },
    { 6730, -2640, 10375, 0 },
    { 4090, -150, 6625, 0 },
    { 4090, -150, 6775, 0 },
    { 4090, -150, 6975, 0 },
    { 4090, -150, 7125, 0 },
    { 4090, -150, 7725, 0 },
    { 4090, -150, 7875, 0 },
    { 4090, -150, 8075, 0 },
    { 4090, -150, 8225, 0 },
    { 4090, -150, 8775, 0 },
    { 4090, -150, 8925, 0 },
    { 4090, -150, 9125, 0 },
    { 4090, -150, 9275, 0 },
    { 4090, -150, 9875, 0 },
    { 4090, -150, 10025, 0 },
    { 4090, -150, 10225, 0 },
    { 4090, -150, 10375, 0 },
    { 6660, -150, 6625, 0 },
    { 6660, -150, 6775, 0 },
    { 6660, -150, 6975, 0 },
    { 6660, -150, 7125, 0 },
    { 6660, -150, 7725, 0 },
    { 6660, -150, 7875, 0 },
    { 6660, -150, 8075, 0 },
    { 6660, -150, 8225, 0 },
    { 6660, -150, 8775, 0 },
    { 6660, -150, 8925, 0 },
    { 6660, -150, 9125, 0 },
    { 6660, -150, 9275, 0 },
    { 6660, -150, 9875, 0 },
    { 6660, -150, 10025, 0 },
    { 6660, -150, 10225, 0 },
    { 6660, -150, 10375, 0 },
    { 4575, -2910, 10590, 0 },
    { 4575, -2910, 6390, 0 },
    { 6175, -2910, 10590, 0 },
    { 6175, -2910, 6390, 0 },
    { 910, -2150, 1270, 0 },
    { 910, -2150, 1720, 0 },
    { 910, -2150, 4270, 0 },
    { 910, -2150, 4720, 0 },
    { 910, -2150, 2270, 0 },
    { 910, -2150, 2730, 0 },
    { 910, -2150, 3270, 0 },
    { 910, -2150, 3730, 0 },
    { 5000, -2310, 10650, 0 },
    { 5000, -2310, 6350, 0 },
    { 6250, -1660, 6350, 0 },
    { 1200, -1320, 12490, 0 },
    { 1200, -1570, 12470, 0 },
    { 1500, 0, 2500, 0 },
    { 5250, 0, 2000, 0 },
    { 4096, 4096, 0, 0 },
    { -4096, 4096, 0, 0 },
    { 4096, -4096, 0, 0 },
    { -4096, -4096, 0, 0 },
};

WorldCoordRoomLighting D_shelter_b1_sterilization_room_80189354[3] = {
    { D_shelter_b1_sterilization_room_8018B630, D_shelter_b1_sterilization_room_8018C21C },
    { D_shelter_b1_sterilization_room_8018B630, D_shelter_b1_sterilization_room_8018C21C },
    { D_shelter_b1_sterilization_room_8018B630, D_shelter_b1_sterilization_room_8018C21C },
};

WorldCollisionRoomResources D_shelter_b1_sterilization_room_8018936C[3] = {
    { &D_shelter_b1_sterilization_room_80189E44, D_shelter_b1_sterilization_room_8018B648, D_shelter_b1_sterilization_room_8018B8A8, D_shelter_b1_sterilization_room_8018C1A4 },
    { &D_shelter_b1_sterilization_room_80189E44, D_shelter_b1_sterilization_room_8018B648, D_shelter_b1_sterilization_room_8018B8A8, D_shelter_b1_sterilization_room_8018C1A4 },
    { &D_shelter_b1_sterilization_room_80189E44, D_shelter_b1_sterilization_room_8018B648, D_shelter_b1_sterilization_room_8018B8A8, D_shelter_b1_sterilization_room_8018C1A4 },
};

u8 D_shelter_b1_sterilization_room_8018939C[24] = {
    1,
    2,
    3,
    10,
    11,
    12,
    13,
    8,
    9,
    4,
    5,
    6,
    7,
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
};

u8 D_shelter_b1_sterilization_room_801893B4[24] = {
    1,
    23,
    24,
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
};

u8* D_shelter_b1_sterilization_room_801893CC[3] = {
    gViewIdentityMap,
    D_shelter_b1_sterilization_room_8018939C,
    D_shelter_b1_sterilization_room_801893B4,
};

ViewCount D_shelter_b1_sterilization_room_801893D8[3] = { 24, 24, 24 };

DirectionWarpEntry D_shelter_b1_sterilization_room_801893E0[3] = {
    { { { .word = 0 }, 3456, 0, 960 }, { 0, 0, 0, 0 }, { { .word = 0 }, 3456, 0, 960 }, { 0, 0, 0, 0 }, 0x54100002, 0x54100001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 3614, 0, 0x32C8 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 3614, 0, 0x32C8 }, { 0, 0, 0, 0 }, 0x54100004, 0x54100003, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 6184, 0, 0x27FB }, { 0, 0, 0, 0 }, { { .word = 2048 }, 6184, 0, 0x27FB }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1SterilizationRoomCollision0C884Normals[14] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_normals.inc"
};

static SVECTOR _gShelterB1SterilizationRoomCollision0C884Verts[124] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_verts.inc"
};

static WorldCollisionGridFace _gShelterB1SterilizationRoomCollision0C884Faces[68] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_faces.inc"
};

static s16 _gShelterB1SterilizationRoomCollision0C884Cells[270] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1SterilizationRoomCollision0C884Cells[i])
static s16* _gShelterB1SterilizationRoomCollision0C884Table[8] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_sterilization_room_80189E44 = { NULL, _gShelterB1SterilizationRoomCollision0C884Normals, _gShelterB1SterilizationRoomCollision0C884Verts, _gShelterB1SterilizationRoomCollision0C884Faces, _gShelterB1SterilizationRoomCollision0C884Table, 0, -500, 2, 4, 4000, 68 };

ViewCamera D_shelter_b1_sterilization_room_80189E68[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x61A8, -7000 } }, 329 },
    { { { { -4014, 0, 815 }, { 651, 2458, 3210 }, { -489, 3276, -2409 } }, { -4078, 4407, -4494 } }, 230 },
    { { { { 4009, 0, 835 }, { 674, 2419, -3235 }, { -493, 3305, 2368 } }, { -4078, 4407, -1394 } }, 230 },
    { { { { -3830, 0, 1450 }, { -422, 3918, -1115 }, { -1387, -1192, -3664 } }, { -6453, 407, -0x29C6 } }, 230 },
    { { { { 3845, 0, 1410 }, { -375, 3948, 1023 }, { -1359, -1090, 3706 } }, { -6453, 457, -6319 } }, 230 },
    { { { { -4068, 0, 473 }, { 206, 3686, 1773 }, { -425, 1785, -3661 } }, { -2059, 2735, -0x294C } }, 230 },
    { { { { 4060, 0, 536 }, { 258, 3590, -1954 }, { -470, 1971, 3559 } }, { -1865, 2682, -6335 } }, 230 },
    { { { { 916, 0, 3992 }, { 953, 3977, -219 }, { -3876, 978, 890 } }, { -6259, 1516, -0x2E4D } }, 230 },
    { { { { 1007, 0, 3970 }, { 3716, 1439, -943 }, { -1394, 3834, 354 } }, { -5758, 3397, -0x2F43 } }, 230 },
    { { { { -3830, 0, 1450 }, { -422, 3918, -1115 }, { -1387, -1192, -3664 } }, { -6453, 407, -0x29C6 } }, 230 },
    { { { { 3845, 0, 1410 }, { -375, 3948, 1023 }, { -1359, -1090, 3706 } }, { -6453, 457, -6319 } }, 230 },
    { { { { -4068, 0, 473 }, { 206, 3686, 1773 }, { -425, 1785, -3661 } }, { -2059, 2735, -0x294C } }, 230 },
    { { { { 4060, 0, 536 }, { 258, 3590, -1954 }, { -470, 1971, 3559 } }, { -1865, 2682, -6335 } }, 230 },
    { { { { 3420, 0, 2253 }, { -604, 3945, 917 }, { -2170, -1098, 3295 } }, { -6170, 740, -7020 } }, 289 },
    { { { { 3451, 0, 2205 }, { -85, 4092, 134 }, { -2203, -159, 3449 } }, { -4630, 2480, -7460 } }, 289 },
    { { { { 3448, 0, 2209 }, { 1712, 2588, -2672 }, { -1396, 3174, 2179 } }, { -5940, 2880, -7540 } }, 257 },
    { { { { 612, 0, 4049 }, { 869, 4000, -131 }, { -3955, 879, 598 } }, { -8900, 1805, -9990 } }, 680 },
    { { { { 3423, 0, 2248 }, { 495, 3995, -753 }, { -2193, 901, 3339 } }, { -7680, 1760, -8010 } }, 447 },
    { { { { 1256, 0, 3898 }, { 1644, 3713, -529 }, { -3534, 1728, 1138 } }, { -2000, 1830, -0x2F26 } }, 329 },
    { { { { 180, 0, 4092 }, { 115, 4094, -5 }, { -4090, 115, 180 } }, { -7570, 830, -3330 } }, 380 },
    { { { { 488, 0, -4066 }, { -1621, 3756, -194 }, { 3729, 1632, 448 } }, { -1500, 2630, -3240 } }, 541 },
    { { { { 3577, 0, 1993 }, { 149, 4084, -267 }, { -1988, 306, 3567 } }, { -6270, 1130, -1200 } }, 289 },
    { { { { -4014, 0, 815 }, { 651, 2458, 3210 }, { -489, 3276, -2409 } }, { -4078, 4407, -4494 } }, 230 },
    { { { { 4009, 0, 835 }, { 674, 2419, -3235 }, { -493, 3305, 2368 } }, { -4078, 4407, -1394 } }, 230 },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A1C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A1D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A1E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A1F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A208[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A218[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A228[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_sterilization_room_8018A238[89] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -120, 800, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -88, 825, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 850, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -40, 875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -16, 900, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 8, 925, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 24, 925, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 48, 925, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 24, 925, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 48, 925, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -56, 825, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -32, 825, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -8, 825, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 16, 825, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 40, 825, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 56, 825, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -120, 675, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -88, 675, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -56, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -24, 675, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 8, 675, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 40, 675, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -152, -120, 625, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -152, -88, 625, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -152, -56, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -24, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 625, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 32, 625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -16, 994, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -8, 1025, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 8, 1042, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 8, 1046, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 1043, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -16, 1050, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -16, 1015, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -40, 992, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -40, 992, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -64, 967, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -64, 962, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -88, 949, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -88, 932, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 72, 587, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 88, 568, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 104, 549, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 88, 544, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 72, 575, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 48, 575, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 72, 522, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 72, 468, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 48, 523, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 48, 528, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 486, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 32, 552, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 547, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 16, 510, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 16, 492, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 0, 535, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -16, 526, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -32, 523, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -32, 482, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -8, 497, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -32, 476, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -8, 479, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -48, 510, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -64, 501, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -80, 494, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -96, 492, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -120, 476, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -120, 443, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, -96, 453, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, -72, 464, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, -48, 475, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -96, 457, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -72, 462, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -48, 464, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -40, 598, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -40, 644, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -40, 650, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -32, 616, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -32, 626, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 669, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -24, 621, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -24, 625, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 637, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 662, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -16, 620, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 679, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -8, 624, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -8, 657, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A92C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 47, 0, 0, { 2, 0 } },
    { 75, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A964[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A974[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A994[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A9A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A9B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A9C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018A9D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_sterilization_room_8018A9E4[42] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -32, 687, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 875, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -56, 691, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -40, 696, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -56, 690, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -40, 695, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -56, 675, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -32, 681, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -8, 688, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 16, 695, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 702, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 64, 712, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 88, 732, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -24, 700, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -24, 703, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -24, 669, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -8, 706, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 16, 713, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 40, 720, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 64, 728, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 88, 734, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -8, 712, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 8, 721, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 24, 729, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 40, 734, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 56, 739, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 72, 746, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -8, 677, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 8, 685, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 694, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 56, 755, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 756, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 771, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 767, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 40, 766, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 24, 675, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 8, 675, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 32, 675, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 48, 780, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 64, 781, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -8, 675, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AD2C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 42, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AD44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_sterilization_room_8018AD54[30] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 645, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 658, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 96, 481, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 96, 482, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 72, 551, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 48, 603, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 622, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, 80, 621, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, 64, 625, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 48, 630, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 104, 475, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 104, 475, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 477, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 88, 652, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 72, 655, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 676, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 80, 669, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -120, 431, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -120, 378, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, -96, 416, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -96, 418, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -72, 433, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -72, 436, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -48, 433, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -48, 452, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -24, 427, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -16, 428, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 0, 426, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 424, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 32, 424, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AFAC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AFCC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AFDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AFEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_sterilization_room_8018AFFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_sterilization_room_8018B00C[24] = {
    { { .empty = D_shelter_b1_sterilization_room_8018A1C8 }, D_shelter_b1_sterilization_room_8018A1C8, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A1D8 }, D_shelter_b1_sterilization_room_8018A1D8, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A1E8 }, D_shelter_b1_sterilization_room_8018A1E8, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A1F8 }, D_shelter_b1_sterilization_room_8018A1F8, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A208 }, D_shelter_b1_sterilization_room_8018A208, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A218 }, D_shelter_b1_sterilization_room_8018A218, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A228 }, D_shelter_b1_sterilization_room_8018A228, NULL },
    { { .elements = D_shelter_b1_sterilization_room_8018A238 }, D_shelter_b1_sterilization_room_8018A92C, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A954 }, D_shelter_b1_sterilization_room_8018A954, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A964 }, D_shelter_b1_sterilization_room_8018A964, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A974 }, D_shelter_b1_sterilization_room_8018A974, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A984 }, D_shelter_b1_sterilization_room_8018A984, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A994 }, D_shelter_b1_sterilization_room_8018A994, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A9A4 }, D_shelter_b1_sterilization_room_8018A9A4, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A9B4 }, D_shelter_b1_sterilization_room_8018A9B4, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A9C4 }, D_shelter_b1_sterilization_room_8018A9C4, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018A9D4 }, D_shelter_b1_sterilization_room_8018A9D4, NULL },
    { { .elements = D_shelter_b1_sterilization_room_8018A9E4 }, D_shelter_b1_sterilization_room_8018AD2C, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018AD44 }, D_shelter_b1_sterilization_room_8018AD44, NULL },
    { { .elements = D_shelter_b1_sterilization_room_8018AD54 }, D_shelter_b1_sterilization_room_8018AFAC, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018AFCC }, D_shelter_b1_sterilization_room_8018AFCC, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018AFDC }, D_shelter_b1_sterilization_room_8018AFDC, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018AFEC }, D_shelter_b1_sterilization_room_8018AFEC, NULL },
    { { .empty = D_shelter_b1_sterilization_room_8018AFFC }, D_shelter_b1_sterilization_room_8018AFFC, NULL },
};

WorldCoordPointLight D_shelter_b1_sterilization_room_8018B12C[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3500, -2000, 0x30D4 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2129, 3686, 3276 }, { 0, 0 } }, 3500, 3750 },
};

WorldCoordSpotLight D_shelter_b1_sterilization_room_8018B18C[11] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 865, -2750, 1490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1625, -4500, 8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3522 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 4500, 5250, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5375, -4500, 8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3522 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 4500, 5250, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 865, -2750, 2490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 865, -2750, 3490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 865, -2750, 4490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6140, -2750, 1490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6140, -2750, 2490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6140, -2750, 3490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6140, -2750, 4490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3500, -4750, 3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 3686, 3276 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 5500, 6000, 853 },
};

WorldCoordRoomLights D_shelter_b1_sterilization_room_8018B630[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_b1_sterilization_room_8018B12C), D_shelter_b1_sterilization_room_8018B12C, ARRAY_SIZE(D_shelter_b1_sterilization_room_8018B18C), D_shelter_b1_sterilization_room_8018B18C },
};

WorldCollisionTrigger D_shelter_b1_sterilization_room_8018B648[8] = {
    { NULL, NULL, NULL, { 3633, -1968, 2960, 0 }, { { -3680, -2736, 32, 0 }, { 3680, -2736, -32, 0 }, { -3680, 2736, 32, 0 }, { 3680, 2736, -32, 0 } }, { -36, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3505, -1984, 2752, 0 }, { { 3088, -2736, -32, 0 }, { -3088, -2736, 32, 0 }, { 3088, 2736, -32, 0 }, { -3088, 2736, 32, 0 } }, { 42, 0, 4125, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5456, -1856, 8240, 0 }, { { 1904, -2736, -16, 0 }, { -1904, -2736, 16, 0 }, { 1904, 2736, -16, 0 }, { -1904, 2736, 16, 0 } }, { 33, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5455, -1792, 8384, 0 }, { { -1920, -2736, 0, 0 }, { 1920, -2736, 0, 0 }, { -1920, 2736, 0, 0 }, { 1920, 2736, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3337, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -1888, 8256, 0 }, { { 1904, -2736, -16, 0 }, { -1904, -2736, 16, 0 }, { 1904, 2736, -16, 0 }, { -1904, 2736, 16, 0 } }, { 33, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1536, -1760, 8448, 0 }, { { -1920, -2736, 0, 0 }, { 1920, -2736, 0, 0 }, { -1920, 2736, 0, 0 }, { 1920, 2736, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3337, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4433, -1601, 0x3081, 0 }, { { -416, -2736, -1536, 0 }, { 416, -2736, 1536, 0 }, { -416, 2736, -1536, 0 }, { 416, 2736, 1536, 0 } }, { 3980, 0, -1079, 0 }, { 0, 0, 4096, 0 }, 3156, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4591, -1472, 0x30D0, 0 }, { { 416, -2736, 1488, 0 }, { -416, -2736, -1488, 0 }, { 416, 2736, 1488, 0 }, { -416, 2736, -1488, 0 } }, { -3945, 0, 1102, 0 }, { 0, 0, 4096, 0 }, 3135, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_sterilization_room_8018B8A8[28] = {
    { NULL, NULL, NULL, { 5536, -64, 9200, 0 }, { { -1792, 0, -496, 0 }, { 1792, 0, -496, 0 }, { -1792, 0, 496, 0 }, { 1792, 0, 496, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 1859, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6256, -64, 0x2840, 0 }, { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4992, -64, 0x2980, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4992, -64, 0x2D60, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2016, -64, 0x2900, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1984, -64, 0x2D00, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5056, -64, 5536, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5024, -64, 6432, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1984, -64, 5472, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1920, -64, 6336, 0 }, { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3440, -80, 976, 0 }, { { -912, 0, -464, 0 }, { 912, 0, -464, 0 }, { -912, 0, 464, 0 }, { 912, 0, 464, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1021, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 22, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3344, -80, 0x3380, 0 }, { { -912, 0, -320, 0 }, { 912, 0, -320, 0 }, { -912, 0, 320, 0 }, { 912, 0, 320, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 966, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1088, -64, 3120, 0 }, { { -512, 0, -2624, 0 }, { 512, 0, -2624, 0 }, { -512, 0, 2624, 0 }, { 512, 0, 2624, 0 } }, { 0, 4096, 0, 0 }, { 4076, 0, -401, 0 }, 2672, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -64, 6624, 0 }, { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, { 0, 4095, 0, 0 }, { -202, 0, 4090, 0 }, 586, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, 0x2900, 0 }, { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, -4093, 0 }, 586, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6336, -64, 6560, 0 }, { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, { 0, 4095, 0, 0 }, { -402, 0, 4078, 0 }, 586, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3472, -64, 0x2D20, 0 }, { { -688, 0, -288, 0 }, { 688, 0, -288, 0 }, { -688, 0, 288, 0 }, { 688, 0, 288, 0 } }, { 0, 4102, 0, 0 }, { -1, 0, 4096, 0 }, 743, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1280, -64, 0x3100, 0 }, { { -368, 0, -640, 0 }, { 368, 0, -640, 0 }, { -368, 0, 640, 0 }, { 368, 0, 640, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 738, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5712, -64, 0x30E0, 0 }, { { -1248, 0, -448, 0 }, { 1248, 0, -448, 0 }, { -1248, 0, 448, 0 }, { 1248, 0, 448, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 1324, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3536, -64, 5424, 0 }, { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4098, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 0x3380, 0 }, { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4098, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5184, -64, 832, 0 }, { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4098, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6208, -64, 3936, 0 }, { { -1472, 0, -864, 0 }, { 352, 0, -864, 0 }, { -1472, 0, 864, 0 }, { 352, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { -4052, 0, 600, 0 }, 1702, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 21, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, -64, 3200, 0 }, { { -512, 0, -2624, 0 }, { 512, 0, -2624, 0 }, { -512, 0, 2624, 0 }, { 512, 0, 2624, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, -202, 0 }, 2672, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4576, -64, 0x3300, 0 }, { { -352, 0, -448, 0 }, { 352, 0, -448, 0 }, { -352, 0, 448, 0 }, { 352, 0, 448, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 568, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5248, -64, 1632, 0 }, { { -896, 0, -1072, 0 }, { 896, 0, -1072, 0 }, { -896, 0, 1072, 0 }, { 896, 0, 1072, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1396, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1952, -64, 2992, 0 }, { { -896, 0, -1408, 0 }, { 896, 0, -1408, 0 }, { -896, 0, 1408, 0 }, { 896, 0, 1408, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1668, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6304, -64, 0x2840, 0 }, { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, { 0, 4101, 0, 0 }, { -2276, 0, -3406, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_sterilization_room_8018C0F8[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_sterilization_room_8018C104[2] = {
    { 117, 606, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_160600_8013DFA0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_sterilization_room_8018C11C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_sterilization_room_8018C12C[2] = {
    { 117, 0, 0, 6080, 0, 4000, -967, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_sterilization_room_8018C14C[11] = {
    { NULL, NULL },
    { D_shelter_b1_sterilization_room_8018C11C, D_shelter_b1_sterilization_room_8018C0F8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_sterilization_room_8018C12C, D_shelter_b1_sterilization_room_8018C104 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionOccluder D_shelter_b1_sterilization_room_8018C1A4[2] = {
    { NULL, NULL, { 3536, -1424, 6048, 0 }, { { -3888, 2448, 0, 0 }, { 3888, 2448, 0, 0 }, { -3888, -2448, 0, 0 }, { 3888, -2448, 0, 0 } }, { 0, 0, 4104, 0 }, 4579, 1, 0 },
    { NULL, NULL, { 3488, -1360, 0x2B00, 0 }, { { -3888, 2384, 0, 0 }, { 3888, 2384, 0, 0 }, { -3888, -2384, 0, 0 }, { 3888, -2384, 0, 0 } }, { 0, 0, 4099, 0 }, 4550, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b1_sterilization_room_8018C21C[25] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b1_sterilization_room_8018C21C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1006, 1603, 1284, 1339 } },
    { .color = { 925, 1526, 1344, 1277 } },
    { .color = { 870, 965, 955, 928 } },
    { .color = { 866, 959, 962, 924 } },
    { .color = { 864, 966, 957, 926 } },
    { .color = { 862, 980, 978, 935 } },
    { .color = { 1300, 1806, 1517, 1580 } },
    { .color = { 1523, 2045, 1610, 1794 } },
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

WorldCollisionFootstepSounds D_shelter_b1_sterilization_room_8018C2E4 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b1_sterilization_room_8018C2F0 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_sterilization_room_8018C2FC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_sterilization_room_8018C304[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_sterilization_room_8018C2E4 },
};

WorldCollisionSurfaceProperties D_shelter_b1_sterilization_room_8018C30C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_sterilization_room_8018C2F0 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_sterilization_room_8018C314[8] = {
    D_shelter_b1_sterilization_room_8018C2FC,
    D_shelter_b1_sterilization_room_8018C304,
    D_shelter_b1_sterilization_room_8018C30C,
    D_shelter_b1_sterilization_room_8018C2FC,
    D_shelter_b1_sterilization_room_8018C2FC,
    D_shelter_b1_sterilization_room_8018C2FC,
    D_shelter_b1_sterilization_room_8018C2FC,
    D_shelter_b1_sterilization_room_8018C2FC,
};

AreaApplyRec D_shelter_b1_sterilization_room_8018C334[2] = {
    { 4, 20, 21, 0 },
    { 255, 0, 0, 0 },
};

Task* gRoomCutsceneSoundTask = NULL;

s32 D_shelter_b1_sterilization_room_8018C340 = 0;

RoomCutsceneRec D_shelter_b1_sterilization_room_8018C344 = { 0 };

void func_shelter_b1_sterilization_room_801813A0(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(8);
            gGameSession->eventState = 1;
            arg0->state              = arg0->state + 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                func_800E8634(D_shelter_b1_sterilization_room_80188C94, 0, D_shelter_b1_sterilization_room_80188E14);
                GameFlag_SetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED, 1);
                gGameSession->restartMode = GAME_SESSION_RESTART_NORMAL;
            } else {
                gGameSession->eventState = 0;
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            return;
        default:
            gGameSession->eventState = 0;
            taskKill(arg0);
            return;
    }
}

void func_shelter_b1_sterilization_room_801814B0(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B3_DUMPING_HOLE;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b1_sterilization_room_801814FC(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            gGameSession->viewDirty = 1;
            arg0->state            += 1;
            break;
        case 1:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
            gGameSession->location.loc.room                            = 2;
            gGameSession->roomObjsDirty                                = state;
            arg0->state                                               += 1;
            break;
        default:
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_sterilization_room_80181588(Task* arg0)
{
    if (arg0->state == 0) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(9);
        arg0->state += 1;
        return;
    }
    Gp_MsgPlayerWeapon(1);
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_801815EC(void)
{
    if (!(D_shelter_b1_sterilization_room_8018C340 & 0x20)) {
        D_shelter_b1_sterilization_room_8018C340 |= 0x20;
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 5, 0, 0);
    }
}

void func_shelter_b1_sterilization_room_80181634(Task* arg0)
{
    D_shelter_b1_sterilization_room_8018C340 = 0;
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_80181658(void)
{
    SndEvt_EnqueueTypeA(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 6), 0, 0x24);
    SndEvt_EnqueueTypeA(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 7), 0, 0x24);
}

void func_shelter_b1_sterilization_room_80181698(s32 arg0)
{
    Gp_ResetCap();
    if (arg0 == 1) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x2C0, 0x100);
    }
}

void func_shelter_b1_sterilization_room_801816E0(Task* task)
{
    s32 cmd;
    s32 flag;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            if (task->spawnArg1.value != 0) {
                flag = GameFlag_GetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED);
                cmd  = 8;
                if (flag == 0) {
                    cmd = 7;
                }
                Gp_RunCapCmd1(cmd);
                GameFlag_SetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_149, 1);
            } else {
                flag = GameFlag_GetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED);
                cmd  = 6;
                if (flag != 0) {
                    GameFlag_SetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14A, 1);
                    GameFlag_SetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, 1);
                    cmd = 9;
                }
                Gp_RunCapCmd1(cmd);
            }
            task->state++;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b1_sterilization_room_801817EC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_RunCapCmd1(task->spawnArg1.value);
            task->state = task->state + 1;
            /* fallthrough */
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            break;
    }
}

/// Per-frame task for the room's view-dependent effects. In state 0 it switches
/// on the camera view: some views draw glows at fixed points of the position
/// table, view 6 requests all-effect cancellation on `gRoomEffectState` once, view 14 moves the task to state 1,
/// and views 20-24 set `spawnArg1` and, while no event runs, place one or two
/// points on a random circle (12-bit angle, radius 0x100-0x2FF) around fixed
/// centres and spawn effect 0x60070 at each. In state 1 it spawns effect
/// 0x6017D at random entries of the position table, the entries and the
/// argument depending on the view.
///
/// Three constructs exist only to reproduce the original code generation: the
/// `do { } while (0)` around the view cases, the `(s16)` cast on the `rsin`
/// argument, and the high-half round trip through `hi` / `hiShift` in the
/// angle draw. The last gives `hi` a first life that combine folds away after
/// recording a use of it, so its reuse for the radius draw is a value combine
/// cannot bound and the radius keeps its `s16` sign extension.
void func_shelter_b1_sterilization_room_8018188C(Task* task)
{
    GfxCoord* coord;

    s32 angle;
    s32 i;
    s32 j;
    s32 idx;

    coord = task->extra.coordBody->coord;

    if (task->state == 0) {
        switch (Gp_GetViewIndex() & 0xFF) {
            case 2:
                glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                break;
            case 3:
                glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                break;
                do {
                    case 6:
                        if (task->spawnArg1.value != 0) {
                            Gp_PulseState1C();
                            task->spawnArg1.value = 0;
                        }
                        break;
                    case 8:
                        glowDrawDisc(&D_shelter_b1_sterilization_room_8018909C[0x50], 0x100, 0x440);
                        glowDrawDiamond(&D_shelter_b1_sterilization_room_8018909C[0x4F], 0x60, 0x80);
                        break;
                    case 14:
                        task->state = 1;
                        break;
                    case 19:
                        glowDrawDisc(&D_shelter_b1_sterilization_room_8018909C[0x50], 0x100, 0x440);
                        func_shelter_b1_sterilization_room_80183B8C(&D_shelter_b1_sterilization_room_8018909C[0x4F], 0x60, 0x80);
                        break;
                    case 20:
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x48], 0x200, 0x222);
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x4A], 0x200, 0x222);
                        task->spawnArg1.value = 1;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi              = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
                            hiShift         = hi << 16;
                            angle           = hiShift >> 16;
                            angle          &= 0xFFF;
                            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rnd;
                            hi              = (rnd >> 16) & 0x1FF;
                            radius          = hi + 0x100;
                            vec0->vx        = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy        = 0;
                            vec0->vz        = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx        = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy        = 0;
                            vec1->vz        = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 21:
                        task->spawnArg1.value = 1;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec1;
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi              = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
                            hiShift         = hi << 16;
                            angle           = hiShift >> 16;
                            angle          &= 0xFFF;
                            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rnd;
                            hi              = (rnd >> 16) & 0x1FF;
                            radius          = hi + 0x100;
                            vec1->vx        = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy        = 0;
                            vec1->vz        = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 22:
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        task->spawnArg1.value = 1;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec1;
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi              = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
                            hiShift         = hi << 16;
                            angle           = hiShift >> 16;
                            angle          &= 0xFFF;
                            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rnd;
                            hi              = (rnd >> 16) & 0x1FF;
                            radius          = hi + 0x100;
                            vec1->vx        = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy        = 0;
                            vec1->vz        = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 23:
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x44], 0x200, 0x222);
                        task->spawnArg1.value = 1;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi              = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
                            hiShift         = hi << 16;
                            angle           = hiShift >> 16;
                            angle          &= 0xFFF;
                            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rnd;
                            hi              = (rnd >> 16) & 0x1FF;
                            radius          = hi + 0x100;
                            vec0->vx        = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy        = 0;
                            vec0->vz        = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx        = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy        = 0;
                            vec1->vz        = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                    case 24:
                        glowDrawCapsule(&D_shelter_b1_sterilization_room_8018909C[0x46], 0x200, 0x222);
                        task->spawnArg1.value = 1;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            u32      rnd;
                            u32      hi;
                            u32      hiShift;
                            s16      radius;
                            SVECTOR* vec0;
                            SVECTOR* vec1;
                            vec0 = &D_shelter_b1_sterilization_room_8018909C[0x51];
                            vec1 = &D_shelter_b1_sterilization_room_8018909C[0x52];

                            hi              = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
                            hiShift         = hi << 16;
                            angle           = hiShift >> 16;
                            angle          &= 0xFFF;
                            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = rnd;
                            hi              = (rnd >> 16) & 0x1FF;
                            radius          = hi + 0x100;
                            vec0->vx        = ((radius * rcos(angle)) >> 12) + 0x5DC;
                            vec0->vy        = 0;
                            vec0->vz        = ((radius * rsin((s16)angle)) >> 12) + 0xBB8;
                            vec1->vx        = ((radius * rcos(angle)) >> 12) + 0x157C;
                            vec1->vy        = 0;
                            vec1->vz        = ((radius * rsin((s16)angle)) >> 12) + 0x7D0;
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec0);
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) << 30) | 0x80023400, vec1);
                        }
                        break;
                } while (0);
        }
    } else {
        switch (Gp_GetViewIndex() & 0xFF) {
            case 14:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (i = 8; i < 0x10; i += 4) {
                        idx = i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                        Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
            case 15:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (i = 4; i < 0x10; i += 4) {
                        if (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1) {
                            idx = i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                            Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx - 0x800000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                        }
                    }
                }
                break;
            case 16:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (i = 0; i < 0x40; i += 4) {
                        if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3)) {
                            idx = i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                            Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx, &D_shelter_b1_sterilization_room_8018909C[idx]);
                        }
                    }
                }
                break;
            case 11:
                glowDrawDisc(&D_shelter_b1_sterilization_room_8018909C[0x4C], 0x300, 0x800);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (j = 0; j < 0x40; j += 0x10) {
                        for (i = 4; i < 0x10; i += 4) {
                            if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3)) {
                                idx = j + i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                                Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx + 0x600000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                            }
                        }
                    }
                }
                break;
            case 10:
                glowDrawDisc(&D_shelter_b1_sterilization_room_8018909C[0x4D], 0x300, 0x800);
                glowDrawDisc(&D_shelter_b1_sterilization_room_8018909C[0x4E], 0x300, 0x800);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (j = 0; j < 0x40; j += 0x10) {
                        for (i = 0; i < 0xC; i += 4) {
                            if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3)) {
                                idx = j + i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                                Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx + 0x600000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                            }
                        }
                    }
                }
                break;
            case 17:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (i = 0xC; i < 0x40; i += 0x10) {
                        idx = i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                        Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx + 0x1800000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
            case 18:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (i = 0xC; i < 0x40; i += 0x10) {
                        idx = i + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3);
                        Gp_SpawnEff(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, idx + 0x1000000, &D_shelter_b1_sterilization_room_8018909C[idx]);
                    }
                }
                break;
        }
    }
}

/// Per-frame update of a drifting effect drawn by
/// `spriteQuadDraw`. State 0 seeds the work block
/// from the LCG and takes a direction from a table indexed by the 12-bit angle
/// in `spawnArg1`, scaled through the GTE by `period` and jittered into the
/// velocity `move`. Each tick then moves the coordinate by that velocity
/// and adds `step` to `scale`; while an event is running the tick
/// counter is held instead. The drawn frame advances every `index` ticks
/// and the task is released once ten frames have passed.
void func_shelter_b1_sterilization_room_801823D8(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         base;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case 0:
            base                   = task->spawnArg1.halves.high;
            work->scale            = ((((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF) + 0x180) + base;
            work->angle            = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFFF;
            task->spawnArg1.value &= 0xFFF;
            work->index            = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 1;
            work->period           = (work->scale >> 5) + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF);
            work->step             = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF;
            gte_lddp(work->period);
            gte_ldsv(&D_shelter_b1_sterilization_room_80189334[task->spawnArg1.value / 16]);
            gte_gpf12();
            vec = &work->move;
            gte_stsv(vec);
            work->move.vx -= (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) - 8;
            work->move.vy -= (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) - 8;
            work->move.vz -= (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) - 8;
            task->state    = 1;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale        += work->step;
            } else {
                work->age--;
            }
            spriteQuadDraw(coord, (work->age - 1) / work->index,
                           work->scale, work->angle);
            if (work->index * 10 - 1 < work->age) {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Packed additive drifting-sprite page: 4-bit indexed texels at VRAM X=704 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 704, 0)
/// Drifting-sprite palette: VRAM X=256 words, Y=271 scanlines.
#define SPRITE_QUAD_CLUT getClut(256, 271)
/// Texel width and horizontal stride of each cell in the drifting sprite's five-column grid.
#define SPRITE_QUAD_CELL_WIDTH 48
/// Number of columns in the drifting sprite's grid of ten frames in two rows.
#define SPRITE_QUAD_CELLS_PER_ROW 5
#define SPRITE_QUAD_CELL_H        0x30
/// Inclusive top texel row of the grid's first row, relative to its texture page.
///
/// Keep this signed: adding the frame's row stride precedes narrowing to the
/// GPU byte. Frames 0..4 start at V=0x80; frames 5..9 start at V=0xB0.
/// The next drawer inclusion undefines this binding; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V (-0x80)
#define SPRITE_QUAD_V1    -0x51
/// Perspective-sizing multiplier for the sterilization room's drifting sprite.
///
/// Uses the cell's inclusive 47-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE    (SPRITE_QUAD_CELL_WIDTH - 1)
#define SPRITE_QUAD_OTZ_BIAS 0
#include "../../shared/sprite_quad_draw.inc.c"

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/glow_draw_diamond.inc.c"

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-step disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then
/// wedges reaching between that radius and an inner one of
/// `(s16)arg2 * 8 / otz`. Only the centre vertex is lit, on green and blue, at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses.
static void func_shelter_b1_sterilization_room_80183B8C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDiscScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomDiscScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse              = rsin(gDisplayState.animFrame * (s16)arg1);
        ang                = 0;
        size               = (s16)arg2;
        block->outerRadius = (size * 64) / block->otz;
        color              = pulse / 34 + 0x78;
        block->innerRadius = (size * 8) / block->otz;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->outerRadius * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->outerRadius * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->outerRadius * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->outerRadius * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->outerRadius * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->outerRadius * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->innerRadius * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->innerRadius * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->innerRadius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->innerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->outerRadius * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->innerRadius * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDiscScratch);
}
