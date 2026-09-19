#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

#include "rooms/dryfield_dilapidated_house.h"
#include "rooms/room_common.h"

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

extern u8  D_80071075;
extern s16 D_8007107A;
extern s8  D_80114C12;
extern s16 D_80071076;
extern s8  D_801153F1;
extern u8  D_801156F9;

extern RECT D_dryfield_dilapidated_house_80183E7C;
extern RECT D_dryfield_dilapidated_house_80183E84;

extern s32            D_dryfield_dilapidated_house_80189B70;
extern s32            D_dryfield_dilapidated_house_80189B6C;
extern s32            D_dryfield_dilapidated_house_80183EFC;
extern s32            D_dryfield_dilapidated_house_80184408;
extern s32            D_dryfield_dilapidated_house_80184C60;
extern TaskDesc       D_dryfield_dilapidated_house_80183EB4;
extern s32            D_dryfield_dilapidated_house_80184EA0;
extern s32            D_dryfield_dilapidated_house_801855F0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189AA0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189B24;

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017D64C);

/// The room's capture task, the body the actor family also carries as
/// `func_actor_460200_80131E2C`: the whole image area is written into the
/// display buffer strip by strip and then desaturated in place.
///
/// It is spawned from entry 0 of `D_dryfield_dilapidated_house_80183E64` with
/// the `DdhCaptureArgs` block as its `spawnArg2`. State 0 seeds the countdown
/// from the block's duration, picks the strip origin's y out of `gDisplayState`
/// (`field_1f` non-zero selects 0, clear selects 0x110) and hands the twenty
/// 0x1E00-byte strips of `Fs_ImgBuffers` to `StoreImage` -- or, while the buffer
/// is being read back (`field_112` is negative), only the single flat
/// `D_dryfield_dilapidated_house_80183E7C` rectangle -- and then marks the
/// display busy in `field_104`. State 1 waits for the transfer with `DrawSync`
/// and runs the desaturating invert. State 2 counts the duration down in
/// `Task::killCountdown`, raises the block's `done` when it runs out, and on
/// `done` releases `field_104` and kills the task.
void func_dryfield_dilapidated_house_8017DE88(Task* task)
{
    DdhCaptureArgs* args;
    s32             i;
    u32*            strip;

    args = task->spawnArg2;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    D_dryfield_dilapidated_house_80183E84.y = 0;
                } else {
                    D_dryfield_dilapidated_house_80183E84.y = 0x110;
                }
                if (gDisplayState.field_112 < 0) {
                    StoreImage(&D_dryfield_dilapidated_house_80183E7C, Fs_ImgBuffers->buffers[0]);
                } else {
                    strip = Fs_ImgBuffers->buffers[0];
                    for (i = 0; i < 20; i++) {
                        D_dryfield_dilapidated_house_80183E84.x = i * 16;
                        StoreImage(&D_dryfield_dilapidated_house_80183E84, strip);
                        strip += 1920;
                    }
                }
                gDisplayState.skipDraw = 1;
                goto advance;
            case 1:
                DrawSync(0);
                func_dryfield_dilapidated_house_8017E48C();
            advance:
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D5C4);

/// The `"AUNT"` and `"Player"` name words `func_dryfield_dilapidated_house_8017E014`
/// hands to `func_80724608`, the two calls this room makes to it. The pair is
/// the 20-byte run that follows the room's task table at the head of the
/// overlay's leading rodata, and that run was carried by this function's own
/// assembly file until the body moved into C, so the bytes are written here -
/// between the `INCLUDE_RODATA` above and the unit's next emission below, which
/// is where the address order puts them. `Player` spans 12 bytes because it
/// also covers the four zero bytes that pad the run out to the switch table of
/// `func_dryfield_dilapidated_house_8017E144`.
///
/// The section attribute is load-bearing: at 8 and 12 bytes these fall under
/// the compiler's small-data threshold and would otherwise be emitted into
/// `.sdata`, which the overlay's linker script does not lay out.
const char D_dryfield_dilapidated_house_8017D5D0[8] __attribute__((section(".rodata")))  = "AUNT";
const char D_dryfield_dilapidated_house_8017D5D8[12] __attribute__((section(".rodata"))) = "Player";

/// Room gate task. While the session is in the room (`gGameSession->eventState`
/// is 0) it walks `D_dryfield_dilapidated_house_80183EFC` from 1 to 2 and then
/// to 3: the 1 -> 2 step is unconditional, the 2 -> 3 step waits for the room's
/// message (0x7D6) to be dispatched and answered with 0 by the slot-0 object,
/// and for no sound to be playing; reaching 3 spawns entry 3 of the room's task
/// table. Independently, once the stream file is open it starts the named
/// sequences `"AUNT"` and `"Player"` on the two slot objects.
void func_dryfield_dilapidated_house_8017E014(void)
{
    if (gGameSession->eventState == 0) {
        if (D_dryfield_dilapidated_house_80183EFC == 1) {
            D_dryfield_dilapidated_house_80183EFC = 2;
        } else if ((D_dryfield_dilapidated_house_80183EFC == 2) &&
                   (Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0)) {
            if (D_80114C12 != 1) {
                if (D_80071075 == 0) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 3, 0, 0);
                }
            }
        }
    }
    if ((D_8007107A != 0) && (Gp_LookupSlot4(1) != 0)) {
        func_80724608(Gp_LookupSlot4(1), -0x8C, 0xA, &D_dryfield_dilapidated_house_8017D5D0);
        func_80724608(Game_GetPtrSlot(3), -0x8C, 0x14, &D_dryfield_dilapidated_house_8017D5D8);
    }
}

/// Screen-blackout timer of task-table entry 2: `func_dryfield_dilapidated_house_8017E970`
/// arms it by writing state 2 and a frame count into `spawnArg1` (a 0 arg resets
/// it to state 0 instead). State 2 copies that count into the shared countdown
/// `D_dryfield_dilapidated_house_80189B70` and falls through to state 3, whose
/// `var_s1` is the shared "paint the screen black" flag; state 4 runs the
/// countdown and at 0 calls `func_dryfield_dilapidated_house_8017E9A4(0xF)`,
/// which starts the room's captured-image scene, then raises the flag again once
/// the count is 15 frames past that hand-off, keeping the screen black over it.
/// The flag paints the whole frame with a zeroed `TILE` carved out of
/// `Gpu_PrimCursor` and links it into `gGpuCurrentOt`. When `D_801156F9` is set
/// the task does nothing at all.
void func_dryfield_dilapidated_house_8017E144(Task* task)
{
    TILE* tile;
    s32   var_s1;
    s32   temp_v0;
    s32   temp_v1;

    var_s1 = 0;
    if (D_801156F9 == 0) {
        temp_v1 = task->state;
        switch (temp_v1) {
            case 0:
                task->state = task->state + 1;
                break;
            case 1:
                break;
            case 2:
                D_dryfield_dilapidated_house_80189B70 = task->spawnArg1;
                task->state                           = task->state + 1;
                /* fallthrough */
            case 3:
                var_s1      = 1;
                task->state = task->state + var_s1;
                break;
            case 4:
                temp_v0                               = D_dryfield_dilapidated_house_80189B70 - 1;
                D_dryfield_dilapidated_house_80189B70 = temp_v0;
                if (temp_v0 == 0) {
                    func_dryfield_dilapidated_house_8017E9A4(0xF);
                }
                if (D_dryfield_dilapidated_house_80189B70 < -0xF) {
                    var_s1 = 1;
                }
                break;
        }
        if (var_s1 != 0) {
            tile           = (TILE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
            SetTile(tile);
            tile->x0 = -0xA0;
            tile->y0 = -0x80;
            tile->w  = 0x140;
            tile->h  = 0x100;
            tile->r0 = 0;
            tile->g0 = 0;
            tile->b0 = 0;
            addPrim(gGpuCurrentOt, tile);
        }
    }
}

/// Scene-clear task: the room's hand-off to the rest of the game. State 0
/// starts the streamed scene named by the two blocks `func_800E8634` takes,
/// state 1 fires when the session is back in play (`gGameSession->eventState`
/// is 2) and hands slot 0 the release event 0x1B, state 6 waits for the room
/// message (`gGameSession->field_126`), and state 7 -- reached once the save
/// has not already banked this clear (`Mc_SaveData.field_23`) -- applies the
/// room's two area records, raises the progression flags, refills the party
/// and hands off to the results screen with `Task_Spawn(0, 0x11, 0, 0)`.
/// States 0..6 share the `advance` tail that walks the task one state on;
/// `goto advance` from state 1 is the `acropolis_patio` idiom, and the
/// `do/while (0)` around the shared increment is this project's allocation
/// lever, not a loop: it weights the task pointer's references by loop depth
/// so it outranks the `Mc_SaveData` base and takes `$s0` instead of `$s1`.
void func_dryfield_dilapidated_house_8017E2B0(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634((s32)&D_dryfield_dilapidated_house_80184EA0, 0, (s32)&D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 2) {
                Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1B);
                D_801153F1 = 3;
                goto advance;
            }
            return;
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            return;
        case 6:
            if (gGameSession->field_126 == 0) {
                return;
            }
        advance:
            do {
                task->state += 1;
            } while (0);
            return;
        case 7:
            if (Mc_SaveData.field_23 != 9) {
                Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189AA0);
                if (GameFlag_GetNibble(0xCE) != 0) {
                    Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189B24);
                }
                GameFlag_SetNibble(0x4B, 6);
                GameFlag_SetNibble(0x4C, 1);
                GameFlag_SetNibble(0x45, 1);
                GameFlag_SetNibble(0x62, 1);
                GameFlag_SetNibble(0x59, 1);
                GameFlag_SetNibble(0x5A, 2);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0);
                Gp_FillPlayerHpMp();
                Gp_FillAllyHp();
                Mc_SaveData.field_5C5     = 1;
                Mc_SaveData.at4.loc.stage = 2;
                Mc_SaveData.at4.loc.warp  = 1;
                Mc_SaveData.at4.loc.room  = 1;
                Mc_SaveData.at4.loc.area  = 8;
                D_80071076                = 1;
                Task_Spawn(0, 0x11, 0, 0);
            }
            taskKill(task);
            return;
    }
}

/// Inverts the grey of the whole image buffer in place, two 16-bit texels per
/// step. Each packed pair is averaged with weights 3:4:1 over its R, G and B
/// fields, the average is complemented against the 5-bit field mask, and the
/// result is spread back over 15 bits. The 0x4B00 passes cover the buffer's
/// 38400 words exactly.
void func_dryfield_dilapidated_house_8017E48C(void)
{
    s32          i;
    u32          maskR;
    u32          maskG;
    u32          maskB;
    u32          maskAll;
    u32*         p0;
    u32*         p1;
    u32          a0;
    u32          a2;
    u32          a1;
    register u32 v0 asm("v0");
    register u32 v1 asm("v1");

    p0      = Fs_ImgBuffers->buffers[0];
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    p1      = p0 + 1;

    do {
        i += 1;
        a0 = *p1;
        a2 = *p0;

        v1 = (a0 & maskR) << 8;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 1;
        a1 = v0 + v1;

        v1 = (a0 & maskG) << 3;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 2;
        a1 = a1 + v0;

        a0 = a0 >> 2;
        v1 = a0 & maskB;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        a1 = a1 + v1;

        v0 = a1 >> 3;
        a1 = v0 & maskAll;
        a1 = maskAll - a1;

        a2 = a1 & maskR;
        v0 = a2 << 10;
        v1 = a2 << 5;
        v0 = v0 | v1;
        a2 = a2 | v0;

        a0 = a1 & maskB;
        a0 = a0 >> 8;
        v0 = a0 << 10;
        v1 = a0 << 5;
        v0 = v0 | v1;
        a0 = a0 | v0;

        *p0 = a2;
        *p1 = a0;
        p1 += 2;
        p0 += 2;
    } while (i < 0x4B00);
}

s32 func_dryfield_dilapidated_house_8017E56C(void)
{
    return 0;
}

/// Message gate for the room's second hotspot. It copies the incoming record to
/// the outgoing one and then writes the answer the caller acts on to the copy's
/// `field_3`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`gGameSession->at4.loc.stage` is 2), a
/// type-7 record with no sub-id answers 1, or the session's own value when flag
/// nibble 0x3C is set. A type-7 record in play (`Gp_StateF0.field_0` is 1) runs
/// CAP command 0x14 and a type-5 record runs 0x13, each only when the sub-id is
/// clear; everything else is left to the caller and answers 1.
s32 func_dryfield_dilapidated_house_8017E574(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->at4.loc.stage;
    if (s1 == 2) {
        if (in->msgId == 7) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = s1;
                }
            }
        }
    }
    if ((in->msgId == 7) && (Gp_StateF0.field_0 == 1)) {
        if (in->field_5 == 0) {
            Gp_SpawnIfCapIdle(0x14, 0);
        }
        return 0;
    }
    if (in->msgId == 5) {
        if (in->field_5 == 0) {
            Gp_SpawnIfCapIdle(0x13, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_dryfield_dilapidated_house_8017E684(void)
{
    return 0;
}

s32 func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_v1;

    temp_v1 = arg2->field_2;
    if ((temp_v1 == 1) && (D_dryfield_dilapidated_house_80183EFC == 0)) {
        D_dryfield_dilapidated_house_80183EFC = (s32)temp_v1;
        func_800E8634((s32)&D_dryfield_dilapidated_house_80184408, 0, (s32)&D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    s32   temp_a1;
    s32   temp_v1;

    temp_s1 = Game_GetPtrSlot(3);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1 = 0;
            arg0->state    += 1;
            return;
        case 2:
            func_800B0928(temp_s1, (Task*)temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            Display_ClampField126((s8)var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_dilapidated_house_8017E858(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D61C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D628);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D634);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D640);
