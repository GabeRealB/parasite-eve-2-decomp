#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_motel_balcony.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
extern void RoomsShared8017e890Draw(GsCOORDINATE2* arg0, s16 arg1);
extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
extern void RoomsShared8017e4f8Halo(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
extern void RoomsShared8017e4f8Fade(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony_6", RoomsShared8017e890Draw);
