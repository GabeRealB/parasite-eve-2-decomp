#ifndef MINE_CAVERN_PRIVATE_H
#define MINE_CAVERN_PRIVATE_H

#include "gameplay/direction.h"

#include "gameplay/message.h"

#include "main/task_types.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_mine_cavern_8017DD6C(Task *);
void func_mine_cavern_8017DFAC(s32);
void func_mine_cavern_8017E088(s16);
void func_mine_cavern_8017E0B4(void);
void func_mine_cavern_8017E0F4(s32);
void func_mine_cavern_8017E150(s8);
void func_mine_cavern_8017E15C(void);
void func_mine_cavern_8017E180(u8);
void func_mine_cavern_8017E18C(Task *);
void func_mine_cavern_8017E2D8(void);
void func_mine_cavern_8017E2FC(void);
void func_mine_cavern_80182DC8(Task *);
void func_mine_cavern_80183A68(Task *);
void func_mine_cavern_80183C10(Task *);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task *, s32, GpMsg13EF *);
        s32 (*call2)(Task *, s32, s32, s32);
        s32 (*call3)(s32, s32, RoomEventMsg *, RoomEventMsg *);
        s32 (*call4)(s32, s32, s32);
    } handler;
} MineCavernMessageEntry;

extern MineCavernMessageEntry D_mine_cavern_80183C6C[7];

// Callbacks referenced by the overlay's shared data tables.
s32 func_mine_cavern_8017D908(s32, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_mine_cavern_8017DAA0(Task *, s32, s32, s32);
s32 func_mine_cavern_8017DC50(void);
s32 func_mine_cavern_8017DC58(Task *, s32, GpMsg13EF *);
s32 func_mine_cavern_8017DC9C(void);
s32 func_mine_cavern_8017DD38(s32, s32, s32);

#endif // MINE_CAVERN_PRIVATE_H
