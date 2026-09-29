#include "actor_215100_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/mist_shooting_gallery.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[11];
        GpAnimArg  arguments[5];
    } data;
    s32 words[36];
} Actor215100AnimStorageE160;
STATIC_ASSERT_SIZEOF(Actor215100AnimStorageE160, 144);

extern Actor215100AnimStorageE160 D_actor_215100_8014E160;

// The engine copies words across the exported animation bank and its
// following argument records. Both views cover the complete backing object.
typedef union {
    struct {
        GpAnimSet* sets[20];
        GpAnimArg  arguments[3];
    } data;
    s32 words[35];
} Actor215100AnimCopy2EAC;
STATIC_ASSERT_SIZEOF(Actor215100AnimCopy2EAC, 140);

extern Actor215100AnimCopy2EAC D_actor_215100_80152EAC;

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_actor_215100_8015E66C[4];

// Retained task-shaped record; preserve the callback's actual ABI.
typedef struct {
    u16   flags;
    u16   priority;
    void  (*callback)(Task*, s32);
    void* arg;
} Actor215100RetainedTaskSeed;
STATIC_ASSERT_SIZEOF(Actor215100RetainedTaskSeed, 12);
extern Actor215100RetainedTaskSeed D_actor_215100_801544F0;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Eight-byte character appearance record `func_actor_215100_8014AA54` parks in
/// `D_actor_215100_8015E678` before it starts the actor's caption script.
///
/// That function copies its argument here whole and then only reads `field_5`:
/// non-zero means the character has already been committed, so it returns 2 and
/// leaves the record alone. The bytes are otherwise opaque to decompiled code
/// except through `func_actor_215100_8014A5C0`, which copies `field_0`,
/// `field_2` and `field_3` out one at a time into the task it spawns.
typedef struct Actor215100CharRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
    /* 0x4 */ u8 field_4;
    /* 0x5 */ u8 field_5; // non-zero: the character is already committed
    /* 0x6 */ u8 field_6;
    /* 0x7 */ u8 field_7;
} Actor215100CharRec;
STATIC_ASSERT_SIZEOF(Actor215100CharRec, 0x8);

static void func_actor_215100_8014C874(Task* task);
static void func_actor_215100_8014CA80(GpEnemy* enemy, Task* task);
static void func_actor_215100_8014CB04(Task* task);
static void func_actor_215100_8014CB2C(Task* task);
static void func_actor_215100_8014CBB8(Task* task);
static void func_actor_215100_8014CC04(Task* task);
static void func_actor_215100_8014CC7C(Task* task);

static s32 func_actor_215100_8014B3C8(u16* arg0, s32 arg1, s32 arg2, s32 arg3);
static s16 func_actor_215100_8014C17C(u16* arg0, s32 arg1);
static s32 func_actor_215100_8014C360(u16* arg0);

extern TaskDesc D_actor_215100_8014E13C[];
extern GpEvsCmd D_actor_215100_8014E370[];
extern GpEvsCmd D_actor_215100_8014E8F8[];
extern GpEvsCmd D_actor_215100_8014EA90[];
extern GpEvsCmd D_actor_215100_8014EB08[];

extern TaskDesc D_actor_215100_801544FC;
extern TaskDesc D_actor_215100_80154508;
extern Task*    D_actor_215100_8015E64C;

extern GpEvsCmd   D_actor_215100_80153ED4[];
extern GpEvsCmd   D_actor_215100_80153FDC[];
extern GpEvsCmd   D_actor_215100_801543E4[];
extern TaskDesc   D_actor_215100_8015E5D0[];
extern GpAnimSet* D_actor_215100_8015E5E8[25];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpAnimArg*);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor2151002MsgEntry;
STATIC_ASSERT_SIZEOF(Actor2151002MsgEntry, 8);

extern Actor2151002MsgEntry D_actor_215100_8015E5A0[];
/// Glyph metrics table this overlay's caption metrics are read out of, the
/// counterpart of gameplay's `Gp_CapGlyphs`. `func_actor_215100_8014B1B0`
/// stores it and `func_actor_215100_8014C360` indexes it with a text stream's
/// `code & 0x3FF`.
extern GlyphUvwh* D_actor_215100_8015E654;

/// Caption script table, and the script currently being played back with the
/// entry it is up to.
extern GpCapEntry* D_actor_215100_8015E650;
extern GpEvt12*    D_actor_215100_8015E658;
extern s16         D_actor_215100_8015E65C;
extern s16         D_actor_215100_8015E65E;
extern s16         D_actor_215100_8015E660;
extern s16         D_actor_215100_8015E662;
extern s16         D_actor_215100_8015E664;
extern s16         D_actor_215100_8015E666;
/// Frames left before the caret starts drawing.
/// Caret grey level (pulses between 9 and 15) and its direction flag.
extern s32 D_actor_215100_801545E4;
extern s32 D_actor_215100_801545E8;
/// Caret position.
extern u16                D_actor_215100_8015E668;
extern u16                D_actor_215100_8015E66A;
extern s16                D_actor_215100_801544EC;
extern s16                D_actor_215100_801544EE;
extern Actor215100CharRec D_actor_215100_8015E678;
/// Caption schedule `func_actor_215100_8014AFAC` scans, terminated by a -1
/// `field_0`.
extern OverlayCapWindow D_actor_215100_80154514[];

static void func_actor_215100_8014B0D4(void);
static s32  func_actor_215100_8014B1B0(GpCapFileAddress base);
static s32  func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
static s16  func_actor_215100_8014BDFC(u16* arg0);
static s16  func_actor_215100_8014C06C(u16* arg0);
static s16  func_actor_215100_8014C298(u16* arg0);
static s32  func_actor_215100_8014C418(s32 arg0);
static void func_actor_215100_8014BEE8(void);

void func_actor_215100_8014C46C(Task*);
void func_actor_215100_8014C4A8(Task*);

extern TmdSource D_actor_215100_8015A7E4;
extern TmdSource D_actor_215100_8015A9E0;
s32              func_actor_215100_8014CCE0(Task*, s32, GpAnimArg*);
s32              func_actor_215100_8014CD4C(Task*, s32, s32);
s32              func_actor_215100_8014CDB0(Task*, s32, GpXformArg*);
s32              func_actor_215100_8014CE28(void);
s32              func_actor_215100_8014CE30(Task*, s32, GpXformArg*);
void             func_actor_215100_8014CA2C(Task*);
void             func_actor_215100_8014CEF8(Task*);

extern GpAnimSet D_actor_215100_8015AC08;
extern GpAnimSet D_actor_215100_8015AFC0;
extern GpAnimSet D_actor_215100_8015B184;
extern GpAnimSet D_actor_215100_8015B414;
extern GpAnimSet D_actor_215100_8015B854;
extern GpAnimSet D_actor_215100_8015BA30;
extern GpAnimSet D_actor_215100_8015BC44;
extern GpAnimSet D_actor_215100_8015C150;
extern GpAnimSet D_actor_215100_8015C540;
extern GpAnimSet D_actor_215100_8015C8DC;
extern GpAnimSet D_actor_215100_8015CB84;
extern GpAnimSet D_actor_215100_8015CE3C;
extern GpAnimSet D_actor_215100_8015D130;
extern GpAnimSet D_actor_215100_8015D320;
extern GpAnimSet D_actor_215100_8015D638;
extern GpAnimSet D_actor_215100_8015D870;
extern GpAnimSet D_actor_215100_8015DAA4;
extern GpAnimSet D_actor_215100_8015DD04;
extern GpAnimSet D_actor_215100_8015DFC0;
extern GpAnimSet D_actor_215100_8015E188;
extern GpAnimSet D_actor_215100_8015E3B0;
extern GpAnimSet D_actor_215100_8015E578;

static void func_actor_215100_8014AFAC(Task*, s32);

void func_actor_215100_8014AEC4(s32);

void func_actor_215100_8014ABAC(Task*);
void func_actor_215100_8014AD50(Task*);
void func_actor_215100_8014ADD8(void);
void func_actor_215100_8014AE08(s32);
void func_actor_215100_8014AE2C(s32);
void func_actor_215100_8014AE90(s16);
void func_actor_215100_8014AEB4(s16);

TaskDesc D_actor_215100_8014E13C[3] = {
    { 0, 32, func_actor_215100_8014ABAC, { .model = NULL } },
    { 0, 32, func_actor_215100_80149F2C, { .model = NULL } },
    { 0, 32, func_actor_215100_8014AD50, { .model = NULL } },
};

Actor215100AnimStorageE160 D_actor_215100_8014E160 = { .data = { { &D_actor_215100_8014D304, &D_actor_215100_8014D574, &D_actor_215100_8014D7CC, &D_actor_215100_8014D968, &D_actor_215100_8014DBB8, &D_actor_215100_8014DE10, NULL, NULL, NULL, NULL, &D_actor_215100_8014E114 }, { { { .index = 1 }, 0, 0, 0, 0 }, { { .index = 1 }, 18, 0, 0, 0 }, { { .index = 1 }, 19, 0, 0, 0 }, { { .index = 1 }, 20, 0, 0, 0 }, { { .index = 1 }, 21, 0, 0, 0 } } } };

GpAnimArg D_actor_215100_8014E1F0 = { { .index = 1 }, 22, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E204[3] = {
    { { .index = 1 }, 23, 0, 0, 0 },
    { { .index = 1 }, 24, 0, 0, 0 },
    { { .index = 1 }, 47, 0, 0, 0 },
};

GpAnimArg D_actor_215100_8014E240 = { { .index = 1 }, 47, 1, 6, 0 };

GpAnimArg D_actor_215100_8014E254 = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E268 = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E27C = { { .index = 1 }, 50, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E290 = { { .index = 1 }, 51, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E2A4 = { { .index = 1 }, 52, 0, 0, 0 };

GpCopyArg D_actor_215100_8014E2B8 = { { .words = D_actor_215100_8014E160.words }, 32 };

GpAnimArg D_actor_215100_8014E2C0 = { { .index = 1 }, 57, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E2D4 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E2E8 = { { .index = 1 }, 9, 0, 0, 0 };

GpAnimArg D_actor_215100_8014E2FC = { { .index = 1 }, 7, 0, 0, 0 };

GpXformArg D_actor_215100_8014E310 = { { -0x27CE, 0, 4100, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_actor_215100_8014E328 = { { -0x279C, 0, 4720, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_actor_215100_8014E340 = { { -6550, 0, 2950, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_actor_215100_8014E358 = { { -6800, 0, 2950, 0 }, { 0, 1024, 0, 0 } };

GpEvsCmd D_actor_215100_8014E370[59] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_8014E2B8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_215100_8014ADD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2E8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E340 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E310 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5114000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AE08 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AE2C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5114000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AE2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E268 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[3] }, { .value = 0 } },
    { 4, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E27C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E1F0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E290 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E328 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[3] }, { .value = 0 } },
    { 4, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E310 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014E8F8[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_215100_8014AEB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AE2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E310 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E340 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EA90[5] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 32, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EB08[6] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_8014E2B8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E240 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EB98[3] = {
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EBE0[18] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_8014E2B8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E358 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014ED90[9] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2E8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E340 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E310 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EE68[13] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_8014E2B8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E340 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_8014E310 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014EFA0[8] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E358 }, { .value = 0 } },
    { 3, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_215100_8014AE90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014F060[9] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8014E358 }, { .value = 0 } },
    { 3, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_8014F138[6] = {
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[3] }, { .value = 0 } },
    { 4, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8014E160.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor215100PoseBank53A8;

Actor215100PoseBank53A8 D_actor_215100_8014F1C8 = { .poses = {
#include "assets/actor_215100_animation_05738_bank1.inc"
} };

GpPackedSvec D_actor_215100_8014F1EC[81] = {
#include "assets/actor_215100_animation_05738_bank4.inc"
};

GpAnimRec D_actor_215100_8014F330[128] = {
#include "assets/actor_215100_animation_05738_records.inc"
};

u16 D_actor_215100_8014F530[20] = {
#include "assets/actor_215100_animation_05738_indices.inc"
};

GpAnimSet D_actor_215100_8014F558 = {
    D_actor_215100_8014F330, D_actor_215100_8014F530,
    { NULL, D_actor_215100_8014F1C8.words, NULL, NULL, D_actor_215100_8014F1EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} Actor215100PoseBank5760;

Actor215100PoseBank5760 D_actor_215100_8014F580 = { .poses = {
#include "assets/actor_215100_animation_05B0C_bank1.inc"
} };

GpPackedSvec D_actor_215100_8014F5E0[84] = {
#include "assets/actor_215100_animation_05B0C_bank4.inc"
};

GpAnimRec D_actor_215100_8014F730[117] = {
#include "assets/actor_215100_animation_05B0C_records.inc"
};

u16 D_actor_215100_8014F904[20] = {
#include "assets/actor_215100_animation_05B0C_indices.inc"
};

GpAnimSet D_actor_215100_8014F92C = {
    D_actor_215100_8014F730, D_actor_215100_8014F904,
    { NULL, D_actor_215100_8014F580.words, NULL, NULL, D_actor_215100_8014F5E0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor215100PoseBank5B34;

Actor215100PoseBank5B34 D_actor_215100_8014F954 = { .poses = {
#include "assets/actor_215100_animation_05EFC_bank1.inc"
} };

GpPackedSvec D_actor_215100_8014F9A8[62] = {
#include "assets/actor_215100_animation_05EFC_bank4.inc"
};

GpAnimRec D_actor_215100_8014FAA0[149] = {
#include "assets/actor_215100_animation_05EFC_records.inc"
};

u16 D_actor_215100_8014FCF4[20] = {
#include "assets/actor_215100_animation_05EFC_indices.inc"
};

GpAnimSet D_actor_215100_8014FD1C = {
    D_actor_215100_8014FAA0, D_actor_215100_8014FCF4,
    { NULL, D_actor_215100_8014F954.words, NULL, NULL, D_actor_215100_8014F9A8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor215100PoseBank5F24;

Actor215100PoseBank5F24 D_actor_215100_8014FD44 = { .poses = {
#include "assets/actor_215100_animation_06268_bank1.inc"
} };

GpPackedSvec D_actor_215100_8014FD98[74] = {
#include "assets/actor_215100_animation_06268_bank4.inc"
};

GpAnimRec D_actor_215100_8014FEC0[104] = {
#include "assets/actor_215100_animation_06268_records.inc"
};

u16 D_actor_215100_80150060[20] = {
#include "assets/actor_215100_animation_06268_indices.inc"
};

GpAnimSet D_actor_215100_80150088 = {
    D_actor_215100_8014FEC0, D_actor_215100_80150060,
    { NULL, D_actor_215100_8014FD44.words, NULL, NULL, D_actor_215100_8014FD98, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} Actor215100PoseBank6290;

Actor215100PoseBank6290 D_actor_215100_801500B0 = { .poses = {
#include "assets/actor_215100_animation_066B4_bank1.inc"
} };

GpPackedSvec D_actor_215100_801500F8[78] = {
#include "assets/actor_215100_animation_066B4_bank4.inc"
};

GpAnimRec D_actor_215100_80150230[159] = {
#include "assets/actor_215100_animation_066B4_records.inc"
};

u16 D_actor_215100_801504AC[20] = {
#include "assets/actor_215100_animation_066B4_indices.inc"
};

GpAnimSet D_actor_215100_801504D4 = {
    D_actor_215100_80150230, D_actor_215100_801504AC,
    { NULL, D_actor_215100_801500B0.words, NULL, NULL, D_actor_215100_801500F8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor215100PoseBank66DC;

Actor215100PoseBank66DC D_actor_215100_801504FC = { .poses = {
#include "assets/actor_215100_animation_069DC_bank1.inc"
} };

GpPackedSvec D_actor_215100_80150538[69] = {
#include "assets/actor_215100_animation_069DC_bank4.inc"
};

GpAnimRec D_actor_215100_8015064C[98] = {
#include "assets/actor_215100_animation_069DC_records.inc"
};

u16 D_actor_215100_801507D4[20] = {
#include "assets/actor_215100_animation_069DC_indices.inc"
};

GpAnimSet D_actor_215100_801507FC = {
    D_actor_215100_8015064C, D_actor_215100_801507D4,
    { NULL, D_actor_215100_801504FC.words, NULL, NULL, D_actor_215100_80150538, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} Actor215100PoseBank6A04;

Actor215100PoseBank6A04 D_actor_215100_80150824 = { .poses = {
#include "assets/actor_215100_animation_06F5C_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015089C[104] = {
#include "assets/actor_215100_animation_06F5C_bank4.inc"
};

GpAnimRec D_actor_215100_80150A3C[198] = {
#include "assets/actor_215100_animation_06F5C_records.inc"
};

u16 D_actor_215100_80150D54[20] = {
#include "assets/actor_215100_animation_06F5C_indices.inc"
};

GpAnimSet D_actor_215100_80150D7C = {
    D_actor_215100_80150A3C, D_actor_215100_80150D54,
    { NULL, D_actor_215100_80150824.words, NULL, NULL, D_actor_215100_8015089C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} Actor215100PoseBank6F84;

Actor215100PoseBank6F84 D_actor_215100_80150DA4 = { .poses = {
#include "assets/actor_215100_animation_0733C_bank1.inc"
} };

GpPackedSvec D_actor_215100_80150E04[85] = {
#include "assets/actor_215100_animation_0733C_bank4.inc"
};

GpAnimRec D_actor_215100_80150F58[119] = {
#include "assets/actor_215100_animation_0733C_records.inc"
};

u16 D_actor_215100_80151134[20] = {
#include "assets/actor_215100_animation_0733C_indices.inc"
};

GpAnimSet D_actor_215100_8015115C = {
    D_actor_215100_80150F58, D_actor_215100_80151134,
    { NULL, D_actor_215100_80150DA4.words, NULL, NULL, D_actor_215100_80150E04, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor215100PoseBank7364;

Actor215100PoseBank7364 D_actor_215100_80151184 = { .poses = {
#include "assets/actor_215100_animation_075B4_bank1.inc"
} };

GpPackedSvec D_actor_215100_801511A8[27] = {
#include "assets/actor_215100_animation_075B4_bank4.inc"
};

GpAnimRec D_actor_215100_80151214[102] = {
#include "assets/actor_215100_animation_075B4_records.inc"
};

u16 D_actor_215100_801513AC[20] = {
#include "assets/actor_215100_animation_075B4_indices.inc"
};

GpAnimSet D_actor_215100_801513D4 = {
    D_actor_215100_80151214, D_actor_215100_801513AC,
    { NULL, D_actor_215100_80151184.words, NULL, NULL, D_actor_215100_801511A8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank75DC;

Actor215100PoseBank75DC D_actor_215100_801513FC = { .poses = {
#include "assets/actor_215100_animation_07794_bank1.inc"
} };

GpPackedSvec D_actor_215100_80151414[25] = {
#include "assets/actor_215100_animation_07794_bank4.inc"
};

GpAnimRec D_actor_215100_80151478[69] = {
#include "assets/actor_215100_animation_07794_records.inc"
};

u16 D_actor_215100_8015158C[20] = {
#include "assets/actor_215100_animation_07794_indices.inc"
};

GpAnimSet D_actor_215100_801515B4 = {
    D_actor_215100_80151478, D_actor_215100_8015158C,
    { NULL, D_actor_215100_801513FC.words, NULL, NULL, D_actor_215100_80151414, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank77BC;

Actor215100PoseBank77BC D_actor_215100_801515DC = { .poses = {
#include "assets/actor_215100_animation_07960_bank1.inc"
} };

GpPackedSvec D_actor_215100_801515F4[22] = {
#include "assets/actor_215100_animation_07960_bank4.inc"
};

GpAnimRec D_actor_215100_8015164C[67] = {
#include "assets/actor_215100_animation_07960_records.inc"
};

u16 D_actor_215100_80151758[20] = {
#include "assets/actor_215100_animation_07960_indices.inc"
};

GpAnimSet D_actor_215100_80151780 = {
    D_actor_215100_8015164C, D_actor_215100_80151758,
    { NULL, D_actor_215100_801515DC.words, NULL, NULL, D_actor_215100_801515F4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor215100PoseBank7988;

Actor215100PoseBank7988 D_actor_215100_801517A8 = { .poses = {
#include "assets/actor_215100_animation_07E28_bank1.inc"
} };

GpPackedSvec D_actor_215100_801517D8[97] = {
#include "assets/actor_215100_animation_07E28_bank4.inc"
};

GpAnimRec D_actor_215100_8015195C[177] = {
#include "assets/actor_215100_animation_07E28_records.inc"
};

u16 D_actor_215100_80151C20[20] = {
#include "assets/actor_215100_animation_07E28_indices.inc"
};

GpAnimSet D_actor_215100_80151C48 = {
    D_actor_215100_8015195C, D_actor_215100_80151C20,
    { NULL, D_actor_215100_801517A8.words, NULL, NULL, D_actor_215100_801517D8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank7E50;

Actor215100PoseBank7E50 D_actor_215100_80151C70 = { .poses = {
#include "assets/actor_215100_animation_07FFC_bank1.inc"
} };

GpPackedSvec D_actor_215100_80151C88[22] = {
#include "assets/actor_215100_animation_07FFC_bank4.inc"
};

GpAnimRec D_actor_215100_80151CE0[69] = {
#include "assets/actor_215100_animation_07FFC_records.inc"
};

u16 D_actor_215100_80151DF4[20] = {
#include "assets/actor_215100_animation_07FFC_indices.inc"
};

GpAnimSet D_actor_215100_80151E1C = {
    D_actor_215100_80151CE0, D_actor_215100_80151DF4,
    { NULL, D_actor_215100_80151C70.words, NULL, NULL, D_actor_215100_80151C88, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor215100PoseBank8024;

Actor215100PoseBank8024 D_actor_215100_80151E44 = { .poses = {
#include "assets/actor_215100_animation_08400_bank1.inc"
} };

GpPackedSvec D_actor_215100_80151E80[94] = {
#include "assets/actor_215100_animation_08400_bank4.inc"
};

GpAnimRec D_actor_215100_80151FF8[128] = {
#include "assets/actor_215100_animation_08400_records.inc"
};

u16 D_actor_215100_801521F8[20] = {
#include "assets/actor_215100_animation_08400_indices.inc"
};

GpAnimSet D_actor_215100_80152220 = {
    D_actor_215100_80151FF8, D_actor_215100_801521F8,
    { NULL, D_actor_215100_80151E44.words, NULL, NULL, D_actor_215100_80151E80, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor215100PoseBank8428;

Actor215100PoseBank8428 D_actor_215100_80152248 = { .poses = {
#include "assets/actor_215100_animation_08788_bank1.inc"
} };

GpPackedSvec D_actor_215100_80152278[84] = {
#include "assets/actor_215100_animation_08788_bank4.inc"
};

GpAnimRec D_actor_215100_801523C8[110] = {
#include "assets/actor_215100_animation_08788_records.inc"
};

u16 D_actor_215100_80152580[20] = {
#include "assets/actor_215100_animation_08788_indices.inc"
};

GpAnimSet D_actor_215100_801525A8 = {
    D_actor_215100_801523C8, D_actor_215100_80152580,
    { NULL, D_actor_215100_80152248.words, NULL, NULL, D_actor_215100_80152278, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor215100PoseBank87B0;

Actor215100PoseBank87B0 D_actor_215100_801525D0 = { .poses = {
#include "assets/actor_215100_animation_08BE0_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015260C[108] = {
#include "assets/actor_215100_animation_08BE0_bank4.inc"
};

GpAnimRec D_actor_215100_801527BC[135] = {
#include "assets/actor_215100_animation_08BE0_records.inc"
};

u16 D_actor_215100_801529D8[20] = {
#include "assets/actor_215100_animation_08BE0_indices.inc"
};

GpAnimSet D_actor_215100_80152A00 = {
    D_actor_215100_801527BC, D_actor_215100_801529D8,
    { NULL, D_actor_215100_801525D0.words, NULL, NULL, D_actor_215100_8015260C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank8C08;

Actor215100PoseBank8C08 D_actor_215100_80152A28 = { .poses = {
#include "assets/actor_215100_animation_08EBC_bank1.inc"
} };

GpPackedSvec D_actor_215100_80152A40[32] = {
#include "assets/actor_215100_animation_08EBC_bank4.inc"
};

GpAnimRec D_actor_215100_80152AC0[125] = {
#include "assets/actor_215100_animation_08EBC_records.inc"
};

u16 D_actor_215100_80152CB4[20] = {
#include "assets/actor_215100_animation_08EBC_indices.inc"
};

GpAnimSet D_actor_215100_80152CDC = {
    D_actor_215100_80152AC0, D_actor_215100_80152CB4,
    { NULL, D_actor_215100_80152A28.words, NULL, NULL, D_actor_215100_80152A40, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank8EE4;

Actor215100PoseBank8EE4 D_actor_215100_80152D04 = { .poses = {
#include "assets/actor_215100_animation_09064_bank1.inc"
} };

GpPackedSvec D_actor_215100_80152D1C[23] = {
#include "assets/actor_215100_animation_09064_bank4.inc"
};

GpAnimRec D_actor_215100_80152D78[57] = {
#include "assets/actor_215100_animation_09064_records.inc"
};

u16 D_actor_215100_80152E5C[20] = {
#include "assets/actor_215100_animation_09064_indices.inc"
};

GpAnimSet D_actor_215100_80152E84 = {
    D_actor_215100_80152D78, D_actor_215100_80152E5C,
    { NULL, D_actor_215100_80152D04.words, NULL, NULL, D_actor_215100_80152D1C, NULL, NULL, NULL },
};

Actor215100AnimCopy2EAC D_actor_215100_80152EAC = { .data = { { &D_actor_215100_801513D4, &D_actor_215100_801515B4, &D_actor_215100_80151780, &D_actor_215100_80151C48, &D_actor_215100_80151E1C, &D_actor_215100_80152220, &D_actor_215100_801525A8, &D_actor_215100_80152A00, &D_actor_215100_80152CDC, &D_actor_215100_80152E84, NULL, NULL, &D_actor_215100_8014F558, &D_actor_215100_8014F92C, &D_actor_215100_8014FD1C, &D_actor_215100_80150088, &D_actor_215100_801504D4, &D_actor_215100_801507FC, &D_actor_215100_80150D7C, &D_actor_215100_8015115C }, { { { .index = 1 }, 0, 0, 0, 0 }, { { .index = 1 }, 1, 0, 0, 0 }, { { .index = 1 }, 2, 0, 0, 0 } } } };

GpAnimArg D_actor_215100_80152F38 = { { .index = 1 }, 3, 0, 0, 0 };

GpAnimArg D_actor_215100_80152F4C = { { .index = 1 }, 4, 0, 0, 0 };

GpAnimArg D_actor_215100_80152F60 = { { .index = 1 }, 5, 0, 0, 0 };

GpAnimArg D_actor_215100_80152F74 = { { .index = 1 }, 6, 0, 0, 0 };

GpAnimArg D_actor_215100_80152F88 = { { .index = 1 }, 7, 0, 0, 0 };

GpAnimArg D_actor_215100_80152F9C = { { .index = 1 }, 8, 0, 0, 0 };

GpAnimArg D_actor_215100_80152FB0 = { { .index = 1 }, 9, 1, 8, 0 };

GpAnimArg D_actor_215100_80152FC4 = { { .index = 1 }, 10, 1, 8, 0 };

GpAnimArg D_actor_215100_80152FD8 = { { .index = 1 }, 11, 1, 8, 0 };

GpAnimArg D_actor_215100_80152FEC = { { .index = 1 }, 12, 0, 0, 0 };

GpAnimArg D_actor_215100_80153000 = { { .index = 1 }, 12, 1, 10, 0 };

GpAnimArg D_actor_215100_80153014 = { { .index = 1 }, 13, 0, 0, 0 };

GpAnimArg D_actor_215100_80153028 = { { .index = 1 }, 14, 0, 0, 0 };

GpAnimArg D_actor_215100_8015303C = { { .index = 1 }, 15, 0, 0, 0 };

GpAnimArg D_actor_215100_80153050 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_215100_80153064 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_215100_80153078 = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_actor_215100_8015308C = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_actor_215100_801530A0 = { { .index = 1 }, 50, 0, 0, 0 };

GpAnimArg D_actor_215100_801530B4 = { { .index = 1 }, 51, 0, 0, 0 };

GpAnimArg D_actor_215100_801530C8 = { { .index = 1 }, 52, 0, 0, 0 };

GpAnimArg D_actor_215100_801530DC = { { .index = 1 }, 53, 0, 0, 0 };

GpAnimArg D_actor_215100_801530F0 = { { .index = 1 }, 54, 0, 0, 0 };

GpAnimArg D_actor_215100_80153104 = { { .index = 1 }, 55, 0, 0, 0 };

GpAnimArg D_actor_215100_80153118 = { { .index = 1 }, 56, 0, 0, 0 };

GpAnimArg D_actor_215100_8015312C = { { .index = 1 }, 59, 1, 10, 0 };

GpAnimArg D_actor_215100_80153140 = { { .index = 1 }, 60, 1, 10, 0 };

GpAnimArg D_actor_215100_80153154 = { { .index = 1 }, 61, 1, 10, 0 };

GpAnimArg D_actor_215100_80153168 = { { .index = 1 }, 62, 1, 10, 0 };

GpAnimArg D_actor_215100_8015317C = { { .index = 1 }, 63, 1, 10, 0 };

GpAnimArg D_actor_215100_80153190 = { { .index = 1 }, 64, 1, 10, 0 };

GpAnimArg D_actor_215100_801531A4 = { { .index = 1 }, 65, 1, 10, 0 };

GpAnimArg D_actor_215100_801531B8 = { { .index = 1 }, 66, 1, 10, 0 };

GpCopyArg D_actor_215100_801531CC = { { .words = D_actor_215100_80152EAC.words }, 32 };

GpXformArg D_actor_215100_801531D4 = { { -0x27F6, 0, 4640, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_actor_215100_801531EC = { { -0x2BC0, 0, 3000, 0 }, { 0, -2218, 0, 0 } };

GpXformArg D_actor_215100_80153204 = { { -0x27F6, 0, 5000, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_actor_215100_8015321C = { { -0x295E, 0, 2100, 0 }, { 0, -56, 0, 0 } };

GpXformArg D_actor_215100_80153234 = { { -0x29FA, 0, 3972, 0 }, { 0, 0, 0, 0 } };

GpAnimArg D_actor_215100_8015324C = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_actor_215100_80153260 = { { .index = 1 }, 1, 1, 8, 0 };

GpEvsCmd D_actor_215100_80153274[117] = {
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_801531CC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_801531D4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152EAC.data.arguments[1] }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5114000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152EAC.data.arguments[2] }, { .value = 0 } },
    { 4, { .value = 112 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152EAC.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F74 }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5114000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F88 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_801531EC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_8015321C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015308C }, { .value = 0 } },
    { 4, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153078 }, { .value = 0 } },
    { 4, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F38 }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F9C }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FB0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F9C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FB0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153104 }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153118 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F4C }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FC4 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FD8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FB0 }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153000 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530A0 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530DC }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_8015303C }, { .value = 0 } },
    { 4, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153000 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015308C }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153104 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153118 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F9C }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FB0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F9C }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FB0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530B4 }, { .value = 0 } },
    { 4, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530F0 }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F60 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_80153204 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015324C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_80153234 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_80153D6C[15] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015324C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_215100_80153204 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_215100_80153234 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_80153ED4[11] = {
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153014 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153028 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_80153FDC[43] = {
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_215100_801531CC }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153014 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153028 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152F38 }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153140 }, { .value = 0 } },
    { 4, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153154 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153014 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153028 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153168 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015317C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153190 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153014 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153028 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801531A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801531B8 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_215100_801543E4[11] = {
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153014 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80153028 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_215100_80152FEC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s16 D_actor_215100_801544EC = 384;

s16 D_actor_215100_801544EE = 0;

Actor215100RetainedTaskSeed D_actor_215100_801544F0 = { 0, 32, func_actor_215100_8014AFAC, NULL };

TaskDesc D_actor_215100_801544FC = { 0, 32, func_actor_215100_8014C46C, { .model = NULL } };

TaskDesc D_actor_215100_80154508 = { 0, 32, func_actor_215100_8014C4A8, { .model = NULL } };

OverlayCapWindow D_actor_215100_80154514[13] = {
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

s32 D_actor_215100_801545E4 = 8;

s32 D_actor_215100_801545E8 = 0;

TmdBone D_actor_215100_801545EC[20] = {
#include "assets/actor_215100_model_109C4_skeleton.inc"
};

u32 D_actor_215100_801548BC[20] = {
#include "assets/actor_215100_model_109C4_partVerts.inc"
};

SVECTOR D_actor_215100_8015490C[390] = {
#include "assets/actor_215100_model_109C4_verts.inc"
};

SVECTOR D_actor_215100_8015553C[407] = {
#include "assets/actor_215100_model_109C4_normals.inc"
};

u32 D_actor_215100_801561F4[4476] = {
#include "assets/actor_215100_model_109C4_stream.inc"
};

TmdSource D_actor_215100_8015A7E4 = {
    0, 24444, 6776, 20,
    D_actor_215100_801548BC, D_actor_215100_8015490C, D_actor_215100_8015553C, D_actor_215100_801545EC, D_actor_215100_801561F4,
};

TmdBone D_actor_215100_8015A808[1] = {
#include "assets/actor_215100_model_10BC0_skeleton.inc"
};

u32 D_actor_215100_8015A82C[1] = {
#include "assets/actor_215100_model_10BC0_partVerts.inc"
};

SVECTOR D_actor_215100_8015A830[14] = {
#include "assets/actor_215100_model_10BC0_verts.inc"
};

SVECTOR D_actor_215100_8015A8A0[12] = {
#include "assets/actor_215100_model_10BC0_normals.inc"
};

u32 D_actor_215100_8015A900[56] = {
#include "assets/actor_215100_model_10BC0_stream.inc"
};

TmdSource D_actor_215100_8015A9E0 = {
    0, 340, 0, 1,
    D_actor_215100_8015A82C, D_actor_215100_8015A830, D_actor_215100_8015A8A0, D_actor_215100_8015A808, D_actor_215100_8015A900,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank10BE4;

Actor215100PoseBank10BE4 D_actor_215100_8015AA04 = { .poses = {
#include "assets/actor_215100_animation_10DE8_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015AA1C[25] = {
#include "assets/actor_215100_animation_10DE8_bank4.inc"
};

GpAnimRec D_actor_215100_8015AA80[88] = {
#include "assets/actor_215100_animation_10DE8_records.inc"
};

u16 D_actor_215100_8015ABE0[20] = {
#include "assets/actor_215100_animation_10DE8_indices.inc"
};

GpAnimSet D_actor_215100_8015AC08 = {
    D_actor_215100_8015AA80, D_actor_215100_8015ABE0,
    { NULL, D_actor_215100_8015AA04.words, NULL, NULL, D_actor_215100_8015AA1C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank10E10;

Actor215100PoseBank10E10 D_actor_215100_8015AC30 = { .poses = {
#include "assets/actor_215100_animation_111A0_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015AC48[77] = {
#include "assets/actor_215100_animation_111A0_bank4.inc"
};

GpAnimRec D_actor_215100_8015AD7C[135] = {
#include "assets/actor_215100_animation_111A0_records.inc"
};

u16 D_actor_215100_8015AF98[20] = {
#include "assets/actor_215100_animation_111A0_indices.inc"
};

GpAnimSet D_actor_215100_8015AFC0 = {
    D_actor_215100_8015AD7C, D_actor_215100_8015AF98,
    { NULL, D_actor_215100_8015AC30.words, NULL, NULL, D_actor_215100_8015AC48, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank111C8;

Actor215100PoseBank111C8 D_actor_215100_8015AFE8 = { .poses = {
#include "assets/actor_215100_animation_11364_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015B000[21] = {
#include "assets/actor_215100_animation_11364_bank4.inc"
};

GpAnimRec D_actor_215100_8015B054[66] = {
#include "assets/actor_215100_animation_11364_records.inc"
};

u16 D_actor_215100_8015B15C[20] = {
#include "assets/actor_215100_animation_11364_indices.inc"
};

GpAnimSet D_actor_215100_8015B184 = {
    D_actor_215100_8015B054, D_actor_215100_8015B15C,
    { NULL, D_actor_215100_8015AFE8.words, NULL, NULL, D_actor_215100_8015B000, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank1138C;

Actor215100PoseBank1138C D_actor_215100_8015B1AC = { .poses = {
#include "assets/actor_215100_animation_115F4_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015B1C4[32] = {
#include "assets/actor_215100_animation_115F4_bank4.inc"
};

GpAnimRec D_actor_215100_8015B244[106] = {
#include "assets/actor_215100_animation_115F4_records.inc"
};

u16 D_actor_215100_8015B3EC[20] = {
#include "assets/actor_215100_animation_115F4_indices.inc"
};

GpAnimSet D_actor_215100_8015B414 = {
    D_actor_215100_8015B244, D_actor_215100_8015B3EC,
    { NULL, D_actor_215100_8015B1AC.words, NULL, NULL, D_actor_215100_8015B1C4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor215100PoseBank1161C;

Actor215100PoseBank1161C D_actor_215100_8015B43C = { .poses = {
#include "assets/actor_215100_animation_11A34_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015B46C[92] = {
#include "assets/actor_215100_animation_11A34_bank4.inc"
};

GpAnimRec D_actor_215100_8015B5DC[148] = {
#include "assets/actor_215100_animation_11A34_records.inc"
};

u16 D_actor_215100_8015B82C[20] = {
#include "assets/actor_215100_animation_11A34_indices.inc"
};

GpAnimSet D_actor_215100_8015B854 = {
    D_actor_215100_8015B5DC, D_actor_215100_8015B82C,
    { NULL, D_actor_215100_8015B43C.words, NULL, NULL, D_actor_215100_8015B46C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank11A5C;

Actor215100PoseBank11A5C D_actor_215100_8015B87C = { .poses = {
#include "assets/actor_215100_animation_11C10_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015B894[27] = {
#include "assets/actor_215100_animation_11C10_bank4.inc"
};

GpAnimRec D_actor_215100_8015B900[66] = {
#include "assets/actor_215100_animation_11C10_records.inc"
};

u16 D_actor_215100_8015BA08[20] = {
#include "assets/actor_215100_animation_11C10_indices.inc"
};

GpAnimSet D_actor_215100_8015BA30 = {
    D_actor_215100_8015B900, D_actor_215100_8015BA08,
    { NULL, D_actor_215100_8015B87C.words, NULL, NULL, D_actor_215100_8015B894, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor215100PoseBank11C38;

Actor215100PoseBank11C38 D_actor_215100_8015BA58 = { .poses = {
#include "assets/actor_215100_animation_11E24_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015BA7C[32] = {
#include "assets/actor_215100_animation_11E24_bank4.inc"
};

GpAnimRec D_actor_215100_8015BAFC[72] = {
#include "assets/actor_215100_animation_11E24_records.inc"
};

u16 D_actor_215100_8015BC1C[20] = {
#include "assets/actor_215100_animation_11E24_indices.inc"
};

GpAnimSet D_actor_215100_8015BC44 = {
    D_actor_215100_8015BAFC, D_actor_215100_8015BC1C,
    { NULL, D_actor_215100_8015BA58.words, NULL, NULL, D_actor_215100_8015BA7C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor215100PoseBank11E4C;

Actor215100PoseBank11E4C D_actor_215100_8015BC6C = { .poses = {
#include "assets/actor_215100_animation_12330_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015BC9C[99] = {
#include "assets/actor_215100_animation_12330_bank4.inc"
};

GpAnimRec D_actor_215100_8015BE28[192] = {
#include "assets/actor_215100_animation_12330_records.inc"
};

u16 D_actor_215100_8015C128[20] = {
#include "assets/actor_215100_animation_12330_indices.inc"
};

GpAnimSet D_actor_215100_8015C150 = {
    D_actor_215100_8015BE28, D_actor_215100_8015C128,
    { NULL, D_actor_215100_8015BC6C.words, NULL, NULL, D_actor_215100_8015BC9C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor215100PoseBank12358;

Actor215100PoseBank12358 D_actor_215100_8015C178 = { .poses = {
#include "assets/actor_215100_animation_12720_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015C1B4[91] = {
#include "assets/actor_215100_animation_12720_bank4.inc"
};

GpAnimRec D_actor_215100_8015C320[126] = {
#include "assets/actor_215100_animation_12720_records.inc"
};

u16 D_actor_215100_8015C518[20] = {
#include "assets/actor_215100_animation_12720_indices.inc"
};

GpAnimSet D_actor_215100_8015C540 = {
    D_actor_215100_8015C320, D_actor_215100_8015C518,
    { NULL, D_actor_215100_8015C178.words, NULL, NULL, D_actor_215100_8015C1B4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} Actor215100PoseBank12748;

Actor215100PoseBank12748 D_actor_215100_8015C568 = { .poses = {
#include "assets/actor_215100_animation_12ABC_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015C5B0[74] = {
#include "assets/actor_215100_animation_12ABC_bank4.inc"
};

GpAnimRec D_actor_215100_8015C6D8[119] = {
#include "assets/actor_215100_animation_12ABC_records.inc"
};

u16 D_actor_215100_8015C8B4[20] = {
#include "assets/actor_215100_animation_12ABC_indices.inc"
};

GpAnimSet D_actor_215100_8015C8DC = {
    D_actor_215100_8015C6D8, D_actor_215100_8015C8B4,
    { NULL, D_actor_215100_8015C568.words, NULL, NULL, D_actor_215100_8015C5B0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank12AE4;

Actor215100PoseBank12AE4 D_actor_215100_8015C904 = { .poses = {
#include "assets/actor_215100_animation_12D64_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015C91C[33] = {
#include "assets/actor_215100_animation_12D64_bank4.inc"
};

GpAnimRec D_actor_215100_8015C9A0[111] = {
#include "assets/actor_215100_animation_12D64_records.inc"
};

u16 D_actor_215100_8015CB5C[20] = {
#include "assets/actor_215100_animation_12D64_indices.inc"
};

GpAnimSet D_actor_215100_8015CB84 = {
    D_actor_215100_8015C9A0, D_actor_215100_8015CB5C,
    { NULL, D_actor_215100_8015C904.words, NULL, NULL, D_actor_215100_8015C91C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor215100PoseBank12D8C;

Actor215100PoseBank12D8C D_actor_215100_8015CBAC = { .poses = {
#include "assets/actor_215100_animation_1301C_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015CBD0[29] = {
#include "assets/actor_215100_animation_1301C_bank4.inc"
};

GpAnimRec D_actor_215100_8015CC44[116] = {
#include "assets/actor_215100_animation_1301C_records.inc"
};

u16 D_actor_215100_8015CE14[20] = {
#include "assets/actor_215100_animation_1301C_indices.inc"
};

GpAnimSet D_actor_215100_8015CE3C = {
    D_actor_215100_8015CC44, D_actor_215100_8015CE14,
    { NULL, D_actor_215100_8015CBAC.words, NULL, NULL, D_actor_215100_8015CBD0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13044;

Actor215100PoseBank13044 D_actor_215100_8015CE64 = { .poses = {
#include "assets/actor_215100_animation_13310_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015CE7C[43] = {
#include "assets/actor_215100_animation_13310_bank4.inc"
};

GpAnimRec D_actor_215100_8015CF28[120] = {
#include "assets/actor_215100_animation_13310_records.inc"
};

u16 D_actor_215100_8015D108[20] = {
#include "assets/actor_215100_animation_13310_indices.inc"
};

GpAnimSet D_actor_215100_8015D130 = {
    D_actor_215100_8015CF28, D_actor_215100_8015D108,
    { NULL, D_actor_215100_8015CE64.words, NULL, NULL, D_actor_215100_8015CE7C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13338;

Actor215100PoseBank13338 D_actor_215100_8015D158 = { .poses = {
#include "assets/actor_215100_animation_13500_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015D170[30] = {
#include "assets/actor_215100_animation_13500_bank4.inc"
};

GpAnimRec D_actor_215100_8015D1E8[68] = {
#include "assets/actor_215100_animation_13500_records.inc"
};

u16 D_actor_215100_8015D2F8[20] = {
#include "assets/actor_215100_animation_13500_indices.inc"
};

GpAnimSet D_actor_215100_8015D320 = {
    D_actor_215100_8015D1E8, D_actor_215100_8015D2F8,
    { NULL, D_actor_215100_8015D158.words, NULL, NULL, D_actor_215100_8015D170, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13528;

Actor215100PoseBank13528 D_actor_215100_8015D348 = { .poses = {
#include "assets/actor_215100_animation_13818_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015D360[56] = {
#include "assets/actor_215100_animation_13818_bank4.inc"
};

GpAnimRec D_actor_215100_8015D440[116] = {
#include "assets/actor_215100_animation_13818_records.inc"
};

u16 D_actor_215100_8015D610[20] = {
#include "assets/actor_215100_animation_13818_indices.inc"
};

GpAnimSet D_actor_215100_8015D638 = {
    D_actor_215100_8015D440, D_actor_215100_8015D610,
    { NULL, D_actor_215100_8015D348.words, NULL, NULL, D_actor_215100_8015D360, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13840;

Actor215100PoseBank13840 D_actor_215100_8015D660 = { .poses = {
#include "assets/actor_215100_animation_13A50_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015D678[27] = {
#include "assets/actor_215100_animation_13A50_bank4.inc"
};

GpAnimRec D_actor_215100_8015D6E4[89] = {
#include "assets/actor_215100_animation_13A50_records.inc"
};

u16 D_actor_215100_8015D848[20] = {
#include "assets/actor_215100_animation_13A50_indices.inc"
};

GpAnimSet D_actor_215100_8015D870 = {
    D_actor_215100_8015D6E4, D_actor_215100_8015D848,
    { NULL, D_actor_215100_8015D660.words, NULL, NULL, D_actor_215100_8015D678, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor215100PoseBank13A78;

Actor215100PoseBank13A78 D_actor_215100_8015D898 = { .poses = {
#include "assets/actor_215100_animation_13C84_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015D8BC[30] = {
#include "assets/actor_215100_animation_13C84_bank4.inc"
};

GpAnimRec D_actor_215100_8015D934[82] = {
#include "assets/actor_215100_animation_13C84_records.inc"
};

u16 D_actor_215100_8015DA7C[20] = {
#include "assets/actor_215100_animation_13C84_indices.inc"
};

GpAnimSet D_actor_215100_8015DAA4 = {
    D_actor_215100_8015D934, D_actor_215100_8015DA7C,
    { NULL, D_actor_215100_8015D898.words, NULL, NULL, D_actor_215100_8015D8BC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13CAC;

Actor215100PoseBank13CAC D_actor_215100_8015DACC = { .poses = {
#include "assets/actor_215100_animation_13EE4_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015DAE4[34] = {
#include "assets/actor_215100_animation_13EE4_bank4.inc"
};

GpAnimRec D_actor_215100_8015DB6C[92] = {
#include "assets/actor_215100_animation_13EE4_records.inc"
};

u16 D_actor_215100_8015DCDC[20] = {
#include "assets/actor_215100_animation_13EE4_indices.inc"
};

GpAnimSet D_actor_215100_8015DD04 = {
    D_actor_215100_8015DB6C, D_actor_215100_8015DCDC,
    { NULL, D_actor_215100_8015DACC.words, NULL, NULL, D_actor_215100_8015DAE4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank13F0C;

Actor215100PoseBank13F0C D_actor_215100_8015DD2C = { .poses = {
#include "assets/actor_215100_animation_141A0_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015DD44[41] = {
#include "assets/actor_215100_animation_141A0_bank4.inc"
};

GpAnimRec D_actor_215100_8015DDE8[108] = {
#include "assets/actor_215100_animation_141A0_records.inc"
};

u16 D_actor_215100_8015DF98[20] = {
#include "assets/actor_215100_animation_141A0_indices.inc"
};

GpAnimSet D_actor_215100_8015DFC0 = {
    D_actor_215100_8015DDE8, D_actor_215100_8015DF98,
    { NULL, D_actor_215100_8015DD2C.words, NULL, NULL, D_actor_215100_8015DD44, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank141C8;

Actor215100PoseBank141C8 D_actor_215100_8015DFE8 = { .poses = {
#include "assets/actor_215100_animation_14368_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015E000[28] = {
#include "assets/actor_215100_animation_14368_bank4.inc"
};

GpAnimRec D_actor_215100_8015E070[60] = {
#include "assets/actor_215100_animation_14368_records.inc"
};

u16 D_actor_215100_8015E160[20] = {
#include "assets/actor_215100_animation_14368_indices.inc"
};

GpAnimSet D_actor_215100_8015E188 = {
    D_actor_215100_8015E070, D_actor_215100_8015E160,
    { NULL, D_actor_215100_8015DFE8.words, NULL, NULL, D_actor_215100_8015E000, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank14390;

Actor215100PoseBank14390 D_actor_215100_8015E1B0 = { .poses = {
#include "assets/actor_215100_animation_14590_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015E1C8[36] = {
#include "assets/actor_215100_animation_14590_bank4.inc"
};

GpAnimRec D_actor_215100_8015E258[76] = {
#include "assets/actor_215100_animation_14590_records.inc"
};

u16 D_actor_215100_8015E388[20] = {
#include "assets/actor_215100_animation_14590_indices.inc"
};

GpAnimSet D_actor_215100_8015E3B0 = {
    D_actor_215100_8015E258, D_actor_215100_8015E388,
    { NULL, D_actor_215100_8015E1B0.words, NULL, NULL, D_actor_215100_8015E1C8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor215100PoseBank145B8;

Actor215100PoseBank145B8 D_actor_215100_8015E3D8 = { .poses = {
#include "assets/actor_215100_animation_14758_bank1.inc"
} };

GpPackedSvec D_actor_215100_8015E3F0[28] = {
#include "assets/actor_215100_animation_14758_bank4.inc"
};

GpAnimRec D_actor_215100_8015E460[60] = {
#include "assets/actor_215100_animation_14758_records.inc"
};

u16 D_actor_215100_8015E550[20] = {
#include "assets/actor_215100_animation_14758_indices.inc"
};

GpAnimSet D_actor_215100_8015E578 = {
    D_actor_215100_8015E460, D_actor_215100_8015E550,
    { NULL, D_actor_215100_8015E3D8.words, NULL, NULL, D_actor_215100_8015E3F0, NULL, NULL, NULL },
};

Actor2151002MsgEntry D_actor_215100_8015E5A0[6] = {
    { 2003, { .call1 = func_actor_215100_8014CCE0 } },
    { 2005, { .call3 = func_actor_215100_8014CD4C } },
    { 2004, { .call2 = func_actor_215100_8014CDB0 } },
    { 2011, { .call0 = func_actor_215100_8014CE28 } },
    { 2013, { .call2 = func_actor_215100_8014CE30 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_215100_8015E5D0[2] = {
    { 1, 96, func_actor_215100_8014CA2C, { .model = &D_actor_215100_8015A7E4 } },
    { 1, 192, func_actor_215100_8014CEF8, { .model = &D_actor_215100_8015A9E0 } },
};

GpAnimSet * D_actor_215100_8015E5E8[25] = {
    NULL,
    &D_actor_215100_8015AC08,
    &D_actor_215100_8015AFC0,
    &D_actor_215100_8015B184,
    &D_actor_215100_8015B414,
    &D_actor_215100_8015B854,
    &D_actor_215100_8015BA30,
    &D_actor_215100_8015BC44,
    &D_actor_215100_8015C150,
    &D_actor_215100_8015C540,
    &D_actor_215100_8015C8DC,
    &D_actor_215100_8015CB84,
    &D_actor_215100_8015CE3C,
    &D_actor_215100_8015D130,
    &D_actor_215100_8015D320,
    &D_actor_215100_8015D638,
    NULL,
    NULL,
    &D_actor_215100_8015D870,
    &D_actor_215100_8015DAA4,
    &D_actor_215100_8015DD04,
    &D_actor_215100_8015DFC0,
    &D_actor_215100_8015E188,
    &D_actor_215100_8015E3B0,
    &D_actor_215100_8015E578,
};

Task * D_actor_215100_8015E64C = NULL;

GpCapEntry * D_actor_215100_8015E650 = NULL;

GlyphUvwh * D_actor_215100_8015E654 = NULL;

GpEvt12 * D_actor_215100_8015E658 = NULL;

s16 D_actor_215100_8015E65C = 0;

s16 D_actor_215100_8015E65E = 0;

s16 D_actor_215100_8015E660 = 0;

s16 D_actor_215100_8015E662 = 0;

s16 D_actor_215100_8015E664 = 0;

s16 D_actor_215100_8015E666 = 0;

u16 D_actor_215100_8015E668 = 0;

u16 D_actor_215100_8015E66A = 0;

u8 D_actor_215100_8015E66C[4] = {
    0,
    35,
    192,
    0,
};

Actor215100StorageE670 D_actor_215100_8015E670 = { 0 };

Actor215100CharRec D_actor_215100_8015E678 = { 0, 0, 0, 0, 0, 0, 0, 0 };

static void func_actor_215100_8014A398(void);
static void func_actor_215100_8014A908(void);
static void func_actor_215100_8014A9A0(void);
static s32  func_actor_215100_8014AA54(Actor215100CharRec* arg0);
static void func_actor_215100_8014AB6C(void);
static void func_actor_215100_8014AF0C(void);
static void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2);
static void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2);
static void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2);
static void func_actor_215100_8014C660(GpEnemy* enemy, Task* task);

/// Arms the weapon pickup at this actor's spot while the event flag
/// `D_actor_215100_8014D038` is up and the story step has reached 3. A session
/// leave (`gGameSession->at4.loc.view == 0x12`) drops the `func_mist_shooting_gallery_80180390` hold and
/// `D_actor_215100_8014D03C` with it, sub-states 2 and 3 of
/// `Gp_StateC08.field_A` start the 0x3C-frame cooldown in
/// `D_actor_215100_8014D044`, and while that cooldown runs the function only
/// ticks it down.
///
/// Otherwise the player has to be standing in the zone — its model root's X
/// below -0x1806 and its Z inside [0x10CD, 0x1644) — not aiming
/// (`GameActor.field_954 != 2`), with the caption system idle, `D_80115768`
/// and `gDisplayState.pendingMode` clear, its yaw inside one of the two 0x3FF-wide windows
/// opening at 0x201 and 0xA01, and one of the 0x1000 / 0x4000 pad masks held.
/// Either mask runs the handoff `func_actor_215100_8014AA54` uses: the weapon
/// message, caption command 0x14 and the scene task `D_actor_215100_8014CF6C`.
static void func_actor_215100_8014A398(void)
{
    Task*      task;
    GameActor* actor;
    GpCoord*   coord;
    s32        z;
    s32        facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if (D_actor_215100_8014D038 != 0) {
        if (D_actor_215100_8015E670.value >= 3) {
            if (gGameSession->at4.loc.view == 0x12) {
                func_mist_shooting_gallery_80180390(0);
                D_actor_215100_8014D03C = 0;
            }
            if ((u32)((u8)Gp_StateC08.field_A - 2) < 2U) {
                D_actor_215100_8014D044 = 0x3C;
            }
            if (D_actor_215100_8014D044 != 0) {
                D_actor_215100_8014D044 -= 1;
                return;
            }
            if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (D_actor_215100_8014D03C == 0) &&
                (D_80115768 == 0) && (coord->coord.t[0] < -0x1806)) {
                z = coord->coord.t[2];
                if (z < 0x1644) {
                    if ((z >= 0x10CD) && (Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                        facing = (u16)actor->field_52 & 0xFFF;
                        if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                            if ((u32)(facing - 0xA01) < 0x3FFU) {
                                Gp_MsgPlayerWeapon(0);
                                Gp_StateF0.field_4 = 1;
                                Gp_RunCapCmd(0x14, 0);
                                D_80115690 = 1;
                                Task_SpawnFromTable(D_actor_215100_8014CF6C, 0, 0, 0);
                            }
                        }
                        if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                            Gp_MsgPlayerWeapon(0);
                            Gp_StateF0.field_4 = 1;
                            Gp_RunCapCmd(0x14, 0);
                            D_80115690 = 1;
                            Task_SpawnFromTable(D_actor_215100_8014CF6C, 0, 0, 0);
                        }
                    }
                }
            }
        }
    }
}

/// Watches the caption system while the actor waits to be talked to.
///
/// State 0 first honours the spawn argument: `spawnArg1 == 2` means the actor
/// was placed already committed, so it just steps to state 1, and only
/// `spawnArg1 == 0` is the interactive case. Otherwise it waits for
/// `Gp_CapBusy` to drop and switches on the key `Gp_GetCapEventKey` returns.
/// Key 1 is the plain "talk to me" — it takes the player's weapon away and
/// clears `Gp_StateF0.field_4`; every other key ends the encounter, and which ending
/// depends on `spawnArg1`: non-zero plays caption command 0x17 behind story
/// flag 0xED and steps to state 1, while zero starts the full ending from here
/// (the caption system is stopped, the scene task `D_mist_shooting_gallery_8018E0C4` gets its exit,
/// the sound plays and the weapon is taken). All of those finish by killing
/// this task.
///
/// State 1 commits the character to the save slot once the caption system is
/// idle again: it copies `D_actor_215100_8015E678`'s appearance bytes into
/// `Mc_SaveData`, clears the inventory, then spawns task 0x11 and kills itself.
void func_actor_215100_8014A5C0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1.value == 2) {
                arg0->state = 1;
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 1) {
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                taskKill(arg0);
                break;
            }
            if (arg0->spawnArg1.value != 0) {
                if (GameFlag_GetNibble(0xED) != 0) {
                    Gp_RunCapCmd1(0x17);
                }
                gGameSession->field_126 = 1;
                arg0->state            += 1;
                break;
            }
            if (D_actor_215100_8015E670.value == 3) {
                Gp_StateC08.field_6 &= 0xFD;
            }
            D_actor_215100_8014D038 = 0;
            func_mist_shooting_gallery_80180390(1);
            D_actor_215100_8014D03C = 1;
            Task_CallExit(D_mist_shooting_gallery_8018E0C4);
            gGameSession->field_126  = 1;
            gGameSession->flowFlags |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            Gp_MsgPlayerWeapon(1);
            Gp_StateF0.field_4 = 0;
            taskKill(arg0);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                Player_Status.field_26    = 3;
                Mc_SaveData[0].state.sceneEvent = 1;
                Gp_ClearInventory();
                gGameSession->hideHud = 1;
                SndEvt_EnqueueType6(0x51140005, 0, 0);
                gDisplayState.roomVariant   = 1;
                Mc_SaveData[0].state.at4.loc.area = D_actor_215100_8015E678.field_0;
                Mc_SaveData[0].state.at4.loc.warp = D_actor_215100_8015E678.field_2;
                Mc_SaveData[0].state.at4.loc.room = D_actor_215100_8015E678.field_3;
                Task_Spawn(0, 0x11, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `Gp_CapBusy` / `Gp_GetCapEventKey`, and on key 2 hands the scene task
/// `D_mist_shooting_gallery_8018E0C4` its exit and steps to state 1, while any other key kills the
/// task outright. State 1 starts the caption playback and steps to state 2,
/// which commits the ending: it flags the save-slot session, plays the sound,
/// clears the actor's own 0x97B, drops the story flag the sibling
/// `func_actor_215100_8014A908` sets, and releases the display reference.
void func_actor_215100_8014A7C4(Task* arg0)
{
    GameActor* actor;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 2) {
                Task_CallExit(D_mist_shooting_gallery_8018E0C4);
                arg0->state++;
            } else {
                taskKill(arg0);
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            Mc_SaveData[0].state.at4.loc.view = 8;
            func_mist_shooting_gallery_801811C0(0);
            arg0->state++;
            break;
        case 2:
            gGameSession->field_126  = 1;
            gGameSession->flowFlags |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            actor->field_97B        = 0;
            D_actor_215100_8014D038 = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_StateC08.field_6 &= 0xFD;
            if (gDisplayState.holdCount != 0) {
                Display_ReleaseRef();
            }
            taskKill(arg0);
            break;
    }
}

static void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670.value < 3) {
        Mc_SaveData[0].state.at4.loc.view = 8;
        func_mist_shooting_gallery_801811C0(0);
    } else {
        func_mist_shooting_gallery_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670.value < 4) {
        Gp_StateC08.field_6 &= 0xFD;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

static void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670.value == 5) {
        D_actor_215100_8014D038 = 0;
        func_mist_shooting_gallery_80180390(1);
        D_actor_215100_8014D03C = 1;
        gGameSession->field_126 = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        gGameSession->flowFlags |= 0x80;
    }
    if (D_actor_215100_8015E670.value < 3) {
        Gp_RunCapCmd(0x1D, 3);
        Task_SpawnFromTable(D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

/// Hands the actor off to its caption script, or starts one, depending on
/// whether the script for the current story flag has already run.
///
/// The `else` arm is a `do { } while (0)` whose `break` is the "already
/// committed" exit. It is not vestigial: the loop notes it emits make `reorg`
/// mark that branch's label as leaving a loop, so the delay-slot pass predicts
/// it not-taken and fills its slot from the fall-through rather than from the
/// shared `return 2` tail. Without the loop the branch reaches the same label
/// by a copied `li v0,2`, one instruction longer.
static s32 func_actor_215100_8014AA54(Actor215100CharRec* arg0)
{
    if (D_actor_215100_8014D038 != 0) {
        if (arg0->field_5 != 0) {
            return 2;
        }
        D_actor_215100_8015E678 = *arg0;
        Gp_MsgPlayerWeapon(0);
        Gp_StateF0.field_4 = 1;
        Gp_RunCapCmd(0x14, 0);
        D_80115690 = 1;
        Task_SpawnFromTable(D_actor_215100_8014CF6C, 0, 1, 0);
    } else {
        do {
            if (GameFlag_GetNibble(0xED) == 0) {
                return 1;
            }
            if (arg0->field_5 != 0) {
                break;
            }
            D_actor_215100_8015E678 = *arg0;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(0x17);
            Task_SpawnFromTable(D_actor_215100_8014CF6C, 0, 2, 0);
        } while (0);
    }
    return 2;
}

static void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_mist_shooting_gallery_80184954();
        return;
    }
    Gp_SpawnIfCapIdle(0x11, 1);
}

void func_actor_215100_8014ABAC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            if (GameFlag_GetNibble(0x121) == 0) {
                GameFlag_SetNibble(0x121, 1);
                func_800E3FAC(0xA2, 0x3A);
                func_800E8634(D_actor_215100_8014E370, 1, D_actor_215100_8014E8F8);
                arg0->state++;
            } else {
                Task_SpawnFromTable(D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            break;
        case 2:
            func_800E8614(D_actor_215100_8014EA90, 1);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                if (Gp_GetCapEventKey() != 0) {
                    arg0->state = 10;
                } else {
                    arg0->state++;
                }
            }
            break;
        case 4:
            Gp_StartCapSlot(8, 0, 0);
            func_800E8614(D_actor_215100_8014EBE0, 1);
            taskKill(arg0);
            break;
        case 10:
            Gp_StartCapSlot(7, 0, 0);
            func_800E8614(D_actor_215100_8014EB08, 1);
            arg0->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                Task_SpawnFromTable(D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_215100_8014AD50(Task* arg0)
{
    if (arg0->killCountdown % 48 == 0) {
        Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
    }
    arg0->killCountdown = arg0->killCountdown + 1;
}

void func_actor_215100_8014ADD8(void)
{
    Task_SpawnFromTable(D_mist_shooting_gallery_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_mist_shooting_gallery_801848B4();
    }
}

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = Task_SpawnFromTable(D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        taskKill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_mist_shooting_gallery_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

static void func_actor_215100_8014AF0C(void)
{
    switch (GameFlag_GetNibble(0xF5)) {
        case 0:
            GameFlag_SetNibble(0xF5, 1);
            func_800E8614(D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614(D_actor_215100_80153FDC, 0);
            GameFlag_SetNibble(0xF5, 2);
            break;
        case 2:
            func_800E8614(D_actor_215100_801543E4, 0);
            break;
    }
}

/// Drives the caption schedule while the actor waits to be talked to: state 0
/// arms it, and state 1 scans `D_actor_215100_80154514` for the window
/// containing `gGameSession.sceneClock` - the first entry whose `upper * 30`
/// has not dropped below the clock and whose `lower * 30` has - and, when it
/// finds one, starts that entry's script at its own line key with the task's
/// `spawnArg1` as the line delay. It then ticks the clock down one, unless the
/// caption system is busy or `Gp_StateF0.field_4` is up.
static void func_actor_215100_8014AFAC(Task* task, s32 arg1)
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
            for (i = 0; D_actor_215100_80154514[i].upper != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_actor_215100_80154514[i].upper * 30 >= time) &&
                    (D_actor_215100_80154514[i].lower * 30 < time)) {
                    script = D_actor_215100_80154514[i].script;
                    key    = D_actor_215100_80154514[i].key;
                    break;
                }
            }
            if (script != 0) {
                func_actor_215100_8014B2B8(script, key, (s16)task->spawnArg1.value);
                func_actor_215100_8014B0D4();
            }
            if ((Gp_CapBusy() == 0) && (Gp_StateF0.field_4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

static void func_actor_215100_8014B0D4(void)
{
    if ((D_actor_215100_8015E658 != NULL) &&
        (D_actor_215100_8015E658[D_actor_215100_8015E662].field_8.offset != -1) &&
        (Gp_CapBusy() == 0)) {
        func_actor_215100_8014B3C8(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8.text, 0x80, 1,
                                   D_actor_215100_8015E658[D_actor_215100_8015E662].prefix.bytes.field_0 |
                                       ((D_actor_215100_8015E658[D_actor_215100_8015E662].prefix.bytes.field_1 & 0x10) * 0x10));
        if (!(D_actor_215100_8015E658[D_actor_215100_8015E662].field_4 & 1)) {
            func_actor_215100_8014BEE8();
        }
    }
}

/// Relocates a caption file in place, the counterpart of gameplay's
/// `Gp_RelocCapFile`, and publishes its glyph and script tables. Returns 0
/// when the "CAP" magic is missing.
static s32 func_actor_215100_8014B1B0(GpCapFileAddress base)
{
    s32            i;
    s32            count;
    s32            flag;
    GpEvt12*       rec;
    GpCapEntry*    ptr;
    GpCapEvtTable* evts;
    GpCapPtrTable* ptrs;

    if (strncmp(base.file->magic, "CAP", 3) != 0) {
        return 0;
    }

    i = 0;
    if (base.file->field_8.offset > 0) {
        base.file->field_8.offset  += base.address;
        base.file->field_C.offset  += base.address;
        base.file->field_10.offset += base.address;
        evts                        = base.file->field_C.ptr;
        rec                         = evts->records;
        count                       = evts->count;
        if (count > 0) {
            flag = -1;
            do {
                if (rec->field_8.offset != flag) {
                    rec->field_8.offset += base.address;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        ptrs  = base.file->field_10.ptr;
        i     = 0;
        count = ptrs->count;
        ptr   = ptrs->entries;
        if (count > 0) {
            do {
                if (ptr->offset != 0) {
                    ptr->offset += base.address;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    D_actor_215100_8015E654 = base.file->field_8.ptr;
    D_actor_215100_8015E650 = (base.file->field_10.ptr)->entries;
    return 1;
}

/// Starts playing the caption script `arg0` picks out of
/// `D_actor_215100_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `func_actor_215100_8014B0D4` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
static s32 func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2)
{
    GpEvt12* caption;
    s16      entry;

    caption                 = D_actor_215100_8015E650[arg0].events;
    D_actor_215100_8015E658 = caption;
    if (caption == NULL) {
        return 1;
    }
    D_actor_215100_8015E666 = arg1;
    entry                   = func_actor_215100_8014C418(1);
    D_actor_215100_8015E662 = entry;
    D_actor_215100_8015E660 = arg2;
    D_actor_215100_8015E65C = func_actor_215100_8014C06C(D_actor_215100_8015E658[entry].field_8.text);
    D_actor_215100_8015E65E = func_actor_215100_8014BDFC(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8.text);
    D_actor_215100_8015E664 = func_actor_215100_8014C298(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8.text);
    D_actor_215100_8015E66C[0] = 0x1E;
    return 0;
}

static s32 func_actor_215100_8014B3C8(u16* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    x       = func_actor_215100_8014C17C(arg0, 0) - 0xA0;
    y       = (u16)D_actor_215100_8015E65E - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_actor_215100_8015E65C - 0xA7;
    bg->y0 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664;
    bg->x1 = (u16)D_actor_215100_8015E65C - D_actor_215100_8015E65C * 2 + 0xAB;
    bg->y1 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664;
    bg->x2 = (u16)D_actor_215100_8015E65C - 0xA7;
    bg->y2 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664 + (u16)D_actor_215100_8015E664;
    bg->x3 = (u16)D_actor_215100_8015E65C - D_actor_215100_8015E65C * 2 + 0xAB;
    bg->y3 = ((u16)D_actor_215100_8015E660 - 0x77) - gDisplayState.vramYOffset - (u16)D_actor_215100_8015E664 + (u16)D_actor_215100_8015E664;
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
        top        = ((u16)D_actor_215100_8015E660 - 0x77) - (u16)D_actor_215100_8015E664;
        ft->x0     = (u16)D_actor_215100_8015E65C - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_actor_215100_8015E654[title & 0xFF].h;
        titleWidth = D_actor_215100_8015E654[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_actor_215100_8015E65C + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_actor_215100_8015E654[title & 0xFF].h;
        ft->x2     = (u16)D_actor_215100_8015E65C - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_actor_215100_8015E654[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_actor_215100_8015E65C + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_actor_215100_8015E654[title & 0xFF].u;
        ft->v0     = D_actor_215100_8015E654[title & 0xFF].v;
        ft->u1     = D_actor_215100_8015E654[title & 0xFF].u + D_actor_215100_8015E654[title & 0xFF].w;
        ft->v1     = D_actor_215100_8015E654[title & 0xFF].v;
        ft->u2     = D_actor_215100_8015E654[title & 0xFF].u;
        ft->v2     = D_actor_215100_8015E654[title & 0xFF].v + D_actor_215100_8015E654[title & 0xFF].h;
        ft->u3     = D_actor_215100_8015E654[title & 0xFF].u + D_actor_215100_8015E654[title & 0xFF].w;
        ft->v3     = D_actor_215100_8015E654[title & 0xFF].v + D_actor_215100_8015E654[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_actor_215100_801544EC, D_actor_215100_801544EE);
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
            t2                      = lineIdx + 1;
            lineIdx                 = t2;
            D_actor_215100_8015E66A = y - 2;
            D_actor_215100_8015E668 = x + 4;
            y                      += func_actor_215100_8014C360(&body[i + 1]);
            if (centered != 0) {
                x = func_actor_215100_8014C17C(arg0, t2) - 0xA0;
            } else {
                x = (u16)D_actor_215100_8015E65C - 0xA0;
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
            gt->tpage = getTPage(0, 1, D_actor_215100_801544EC, D_actor_215100_801544EE);
            gt->y0    = glyphY - D_actor_215100_8015E654[code & 0x3FF].h;
            gt->x1    = x + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->y1    = glyphY - D_actor_215100_8015E654[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_actor_215100_8015E654[code & 0x3FF].u;
            gt->v0    = D_actor_215100_8015E654[code & 0x3FF].v;
            gt->u1    = D_actor_215100_8015E654[code & 0x3FF].u + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->v1    = D_actor_215100_8015E654[code & 0x3FF].v;
            gt->u2    = D_actor_215100_8015E654[code & 0x3FF].u;
            gt->v2    = D_actor_215100_8015E654[code & 0x3FF].v + D_actor_215100_8015E654[code & 0x3FF].h;
            gt->u3    = D_actor_215100_8015E654[code & 0x3FF].u + D_actor_215100_8015E654[code & 0x3FF].w;
            gt->v3    = D_actor_215100_8015E654[code & 0x3FF].v + D_actor_215100_8015E654[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_actor_215100_801544EC, D_actor_215100_801544EE);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_actor_215100_8015E654[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_actor_215100_8015E660`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0.
static s16 func_actor_215100_8014BDFC(u16* arg0)
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
                if (lineH < D_actor_215100_8015E654[code & 0x3FF].h + 2) {
                    lineH = D_actor_215100_8015E654[code & 0x3FF].h + 2;
                }
            }
        }
        code = text[++i];
    }
    return D_actor_215100_8015E660 - total;
}

/// Draws the pulsing "more text" caret: a Gouraud triangle at
/// (`D_actor_215100_8015E668`, `D_actor_215100_8015E66A`) whose grey level
/// ramps up to 15 and back down to 9. Same body as gameplay's `Gp_DrawCapCaret`
/// without the VRAM Y offset.
static void func_actor_215100_8014BEE8(void)
{
    POLY_G3* prim;
    s32      c1;
    s32      c2;

    if (D_actor_215100_8015E66C[0] != 0) {
        D_actor_215100_8015E66C[0] -= 1;
        return;
    }
    prim           = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    c1 = (D_actor_215100_801545E4 << 7) / 15;
    setRGB0(prim, c1, c1, c1);
    c1 = (D_actor_215100_801545E4 * 192) / 15;
    c2 = c1;
    setRGB1(prim, c2, c2, c2);
    setRGB2(prim, c2, c2, c2);
    prim->x0 = D_actor_215100_8015E668 + 3;
    prim->y0 = D_actor_215100_8015E66A;
    prim->x1 = D_actor_215100_8015E668;
    prim->x2 = D_actor_215100_8015E668 + 7;
    prim->y1 = D_actor_215100_8015E66A - 7;
    prim->y2 = D_actor_215100_8015E66A - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_actor_215100_801545E8 == 0) {
        D_actor_215100_801545E4 += 1;
        if (D_actor_215100_801545E4 >= 0xF) {
            D_actor_215100_801545E8 = 1;
        }
    } else {
        D_actor_215100_801545E4 -= 1;
        if (D_actor_215100_801545E4 < 9) {
            D_actor_215100_801545E8 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_actor_215100_8015E654`)
/// advances it by that glyph's `w - 1`.
static s16 func_actor_215100_8014C06C(u16* arg0)
{
    s16 lineW;
    s16 maxW;
    s16 i;
    s16 code;

    lineW = 0;
    maxW  = 0;
    i     = 0;
    code  = arg0[0];
    while (code != -1) {
        if (code == -2) {
            if (lineW > maxW) {
                maxW = lineW;
            }
            lineW = 0;
            code  = arg0[++i];
        } else if (code == -3) {
            lineW += 3;
            code   = arg0[++i];
        } else if ((code & 0xFF00) == 0x8400) {
            lineW += 0x10;
            code   = arg0[++i];
        } else if (code >= 0) {
            lineW += D_actor_215100_8015E654[code & 0x3FF].w - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - maxW) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes.
static s16 func_actor_215100_8014C17C(u16* arg0, s32 arg1)
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
            lineW += D_actor_215100_8015E654[code & 0x3FF].w - 1;
            code   = arg0[++i];
        } else {
            code = arg0[++i];
        }
    }
    return (0x140 - selectedW) / 2 - 5;
}

/// Total height of the caption block the text stream `arg0` holds: every `-2`
/// line break adds the line's height (the tallest glyph's `h + 2`, or 2 when
/// the line is empty). Gameplay's `Gp_CapTextHeight` is the same walk plus a
/// final `2 -> 0` clamp.
static s16 func_actor_215100_8014C298(u16* arg0)
{
    s16 lineH = 0;
    s16 total = 0;
    s16 i     = 0;
    s16 code  = arg0[0];

    while (code != -1) {
        if (code == -2) {
            if (lineH == 0) {
                lineH = 2;
            }
            total += lineH;
            lineH  = 0;
        } else if (code != -3) {
            if (code >= 0) {
                if (lineH < D_actor_215100_8015E654[code & 0x3FF].h + 2) {
                    lineH = D_actor_215100_8015E654[code & 0x3FF].h + 2;
                }
            }
        }
        code = arg0[++i];
    }
    return total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_actor_215100_8015E654`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
static s32 func_actor_215100_8014C360(u16* arg0)
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
            if (height < D_actor_215100_8015E654[code & 0x3FF].h + 2) {
                height = D_actor_215100_8015E654[code & 0x3FF].h + 2;
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

static s32 func_actor_215100_8014C418(s32 arg0)
{
    s32      flag;
    s32      id;
    GpEvt12* base;
    GpEvt12* p;

    flag = -1;
    id   = D_actor_215100_8015E666;
    base = D_actor_215100_8015E658;
    p    = Gp_CapEventAt(base, arg0);
loop:
    if (p->field_8.offset == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_actor_215100_8014C46C(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1.value - 1;
    task->spawnArg1.value = remaining;
    if (remaining <= 0) {
        taskKill(task);
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C4A8(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1.value - 1;
            task->spawnArg1.value = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                taskKill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_actor_215100_8014B0D4();
}

static void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_actor_215100_801544FC, 0, (s32)(arg2), 0);
}

static void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_actor_215100_80154508, arg2, 0, 0);
}

static void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                   = 0;
    D_actor_215100_801544EC = arg0;
    D_actor_215100_801544EE = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_actor_215100_8014B1B0(D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 0xC, installs the message table whose handlers
/// are the actor's script opcodes, and starts the animation.
static void func_actor_215100_8014C660(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor160600Work* work;
    Actor160600Work* mem;
    GpCoord*         coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor160600Work*)memCalloc(0x4F8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_215100_8014CB04;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_215100_8015E5D0, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask  = spawned->task;
    work->st.animId = 0xC;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_215100_8015E5E8, obj,
                  work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_215100_8015E5A0;
    func_actor_215100_8014C874(task);
    task->state++;
}

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
static void func_actor_215100_8014C874(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        func_actor_215100_8014CC7C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_215100_8014CC04(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_215100_8014CBB8(task);
        return;
    }
}

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_215100_8014CA2C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_215100_8014C660,
        func_actor_215100_8014CA80,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
static void func_actor_215100_8014CA80(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_215100_8014C874(task);
    func_actor_215100_8014CB2C(task);
}

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
static void func_actor_215100_8014CB04(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the actor's ground shadow under its root part, unless the model's
/// `flags` bit 0x80 is set or it has no buffer. The position is the root
/// part's world translation, staged on the scratchpad stack.
static void func_actor_215100_8014CB2C(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13.
static void func_actor_215100_8014CBB8(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
static void func_actor_215100_8014CC04(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
static void func_actor_215100_8014CC7C(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: plays clip `args->field_4` (ids from 0x19 up are refused
/// with -1). With `args->field_8` set the slots are reseeded with
/// `args->field_C`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->field_4 < 0x19) {
        work->st.animId = args->field_4;
        if (args->field_8 != 0) {
            work->st.state = 1;
            work->animArg  = args->field_C;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_215100_8014C874(task);
        return 0;
    }
    return -1;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 hides both
/// (`TmdObject::flags` 0) and its absence restores 0x80; bit 1 also sets 0x4.
/// The middle argument is the one every opcode of the table receives.
s32 func_actor_215100_8014CD4C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor160600Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode: yaws the actor's root coordinate to `placement->rot.vy`,
/// caching the yaw in the work block, and moves it to `placement->pos`.
s32 func_actor_215100_8014CDB0(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    Actor160600Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode that does nothing.
s32 func_actor_215100_8014CE28(void)
{
    return 0;
}

/// Script opcode "walk to": turns the actor's root coordinate to face
/// `target` horizontally, caching the yaw, and stores the horizontal distance
/// in steps of 12 as `travel` for the step body to walk off.
s32 func_actor_215100_8014CE30(Task* task, s32 arg1, GpXformArg* target)
{
    GpCoord*         coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `flg` so it is recomputed from that part.
void func_actor_215100_8014CEF8(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GpCoord*         coord  = obj->coords;
    GpCoord*         sub    = &parent->extra.tmd->coords[4];
    Actor160600Work* work   = (Actor160600Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->flags    = 0;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
