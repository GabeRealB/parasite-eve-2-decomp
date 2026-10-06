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

/// Colours of the cavern's darkness, one per count of destroyed targets (0-4).
///
/// The colour is what a full-screen quad subtracts from the picture, so a
/// larger channel is a darker cavern. The rows run from an even grey with all
/// four targets intact to a weaker wash that leaves red untouched once all are
/// destroyed. Only the three colour channels are used; `cd` is zero.
extern CVECTOR D_mine_cavern_8018E3E0[5];

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

extern EvsCommand D_mine_cavern_80187C74[41];

extern EvsCommand D_mine_cavern_8018804C[19];

extern EvsCommand D_mine_cavern_80188214[60];

extern EvsCommand D_mine_cavern_801887B4[27];

extern EvsCommand D_mine_cavern_80188A3C[31];

extern EvsCommand D_mine_cavern_80188D24[24];

extern AreaApplyRec D_mine_cavern_8018E32C[9];

extern TaskMessageEntry D_mine_cavern_80183C6C[7];

void func_mine_cavern_8017E394(void);

/// Hides or shows the cavern's sprite batches controlled by nursery progress.
///
/// Only the low byte of `hiddenValue` is used: 0 shows, 1 hides, and other
/// byte values leave visibility unchanged. Requires the cavern's stage/area
/// sprite directory and its mutable view/batch arrays to remain loaded.
void mineCavernSetProgressSpritesHidden(s32 hiddenValue);

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
s32 func_mine_cavern_8017D908(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_mine_cavern_8017DAA0(Task*, s32, s32, s32);

s32 func_mine_cavern_8017DC50(Task*, s32, s32, s32);

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3);

s32 func_mine_cavern_8017DC9C(Task*, s32, s32, s32);

s32 func_mine_cavern_8017DD38(Task*, s32, s32, s32);

#endif // SRC_ROOMS_MINE_CAVERN_MINE_CAVERN_PRIVATE_H
