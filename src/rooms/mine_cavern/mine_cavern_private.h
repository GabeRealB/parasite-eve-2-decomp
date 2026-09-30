#ifndef SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H
#define SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"

#include "main/task_types.h"

/// One tint of the cavern's darkness overlay: a `u8` RGB triple plus a zero
/// fourth byte. `func_mine_cavern_80182454` picks the entry by the number of
/// `GameFlag_GetNibble(0xE2)` bits set, so the rows run light to dark and the
/// full-screen wash deepens as that nibble fills in.
typedef struct MineCavernTint {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 pad;
} MineCavernTint;
STATIC_ASSERT_SIZEOF(MineCavernTint, 0x4);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, DirectionActionRequest*);
        s32 (*call2)(Task*, s32, s32, s32);
        s32 (*call3)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call4)(s32, s32, s32);
    } handler;
} MineCavernMessageEntry;

/// The cavern's five tints: (0x1E,0x1E,0x1E), (0x19,0x19,0x19), (0x11,0x15,0x16),
/// (0x07,0x0F,0x10) and (0x00,0x09,0x0B).
extern MineCavernTint D_mine_cavern_8018E3E0[5];

/// How many of the two steps of game flag nibble 0xE6 (values 1 and 2)
/// `func_mine_cavern_8017DFAC` has already acted on; `func_mine_cavern_8017E394`
/// resets it.
extern s32 D_mine_cavern_8018EB54;

extern u16 D_mine_cavern_8018E360;

extern u16 D_mine_cavern_8018E362;

extern u16 D_mine_cavern_8018E364;

extern u16 D_mine_cavern_8018E366;

extern u16 D_mine_cavern_8018E368;

extern SVECTOR D_mine_cavern_8018E36C[6];

extern SVECTOR D_mine_cavern_8018E39C[4];

extern u8 D_mine_cavern_8018E3BC[4][8];

extern s16 D_mine_cavern_8018E3DC;

extern EnemyParams D_mine_cavern_8018EAE4;

extern u8 D_mine_cavern_8018EAF4[36];

extern SVECTOR D_mine_cavern_8018EB18[4];

extern TaskDesc D_mine_cavern_8018EB38[2];

extern s32 D_mine_cavern_8018EB58;

extern u16 D_mine_cavern_8018EB5C;

extern TaskDesc D_mine_cavern_80183CA4[2];

extern GpEvsCmd D_mine_cavern_80187C74[41];

extern GpEvsCmd D_mine_cavern_8018804C[19];

extern GpEvsCmd D_mine_cavern_80188214[60];

extern GpEvsCmd D_mine_cavern_801887B4[27];

extern GpEvsCmd D_mine_cavern_80188A3C[31];

extern GpEvsCmd D_mine_cavern_80188D24[24];

extern GpAreaApplyRec D_mine_cavern_8018E32C[9];

extern MineCavernMessageEntry D_mine_cavern_80183C6C[7];

void func_mine_cavern_8017E394(void);

/// Hides (`arg0` 1) or shows (0) five of the area's sprite commands by setting
/// their `SpriteBatch::hidden`, which keeps a command's sprites out of the
/// ordering table; any other value changes nothing.
void func_mine_cavern_8017E3A0(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_mine_cavern_8017DD6C(Task*);

void func_mine_cavern_8017DFAC(s32);

void func_mine_cavern_8017E088(s16);

void func_mine_cavern_8017E0B4(void);

void func_mine_cavern_8017E0F4(s32);

void func_mine_cavern_8017E150(s8);

void func_mine_cavern_8017E15C(void);

void func_mine_cavern_8017E180(u8);

void func_mine_cavern_8017E18C(Task*);

void func_mine_cavern_8017E2D8(void);

void func_mine_cavern_8017E2FC(void);

void func_mine_cavern_80182DC8(Task*);

void func_mine_cavern_80183A68(Task*);

void func_mine_cavern_80183C10(Task*);

// Callbacks referenced by the overlay's shared data tables.
s32 func_mine_cavern_8017D908(s32, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_mine_cavern_8017DAA0(Task*, s32, s32, s32);

s32 func_mine_cavern_8017DC50(void);

s32 func_mine_cavern_8017DC58(Task*, s32, DirectionActionRequest* request);

s32 func_mine_cavern_8017DC9C(void);

s32 func_mine_cavern_8017DD38(s32, s32, s32);

#endif // SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H
