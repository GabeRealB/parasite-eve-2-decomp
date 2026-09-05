#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/stream.h"
#include "main/unknown_syms.h"

#include "rooms/acropolis_plaza.h"

/// Per-frame service step for the plaza's streamed cutscene commands.
///
/// Only runs while the slot `CdCmd_Queue.readIdx` selects holds one of the
/// stream opcodes 0x71..0x73; the entry packs the stream slot in `idB0` and a
/// 16-bit argument in `idB1:idB2`. Step 0 waits for `CdCmd_PollStatus`: status
/// 0 keeps waiting, status 2 flushes the drive first, and status 1 (or 2)
/// promotes a 0x72 entry to 0x71 -- clearing the MDEC strip counters -- kicks
/// the decoder, primes `func_8001FAE0` and advances to step 1. Step 1 polls
/// `func_8001FAE0` every frame and retires the command once it reports done.
void func_acropolis_plaza_8017D6D4(void)
{
    CdCmdQueue* q;
    CdCmdEntry* e;
    s16         slot;
    s16         arg;
    s32         cmd;

    q    = &CdCmd_Queue;
    e    = &q->entries[q->readIdx];
    cmd  = e->cmd;
    slot = (s8)e->idB0;
    arg  = e->idB2 | (e->idB1 << 8);

    if (cmd != 0) {
        if (cmd >= 0) {
            if (cmd < 0x74) {
                if (cmd >= 0x71) {
                    switch (q->step) {
                        case 0:
                            switch ((s16)CdCmd_PollStatus(0, 0)) {
                                case 0:
                                    break;
                                case 2:
                                    CdFlush();
                                    /* fallthrough */
                                case 1:
                                    if (q->entries[q->readIdx].cmd == 0x72) {
                                        D_8005EAEC                 = 0;
                                        D_8005EAEE                 = 0;
                                        q->entries[q->readIdx].cmd = 0x71;
                                    }
                                    Stream_KickDecode(slot & 0xFFFF);
                                    if (q->entries[q->readIdx].cmd == 0x71) {
                                        func_8001FAE0(0, arg);
                                    } else if (q->entries[q->readIdx].cmd == 0x73) {
                                        func_8001FAE0(1, q->field_48);
                                    }
                                    q->step++;
                                    /* fallthrough */
                                default:
                                    goto poll;
                            }
                            break;
                        case 1:
                        poll:
                            if (q->entries[q->readIdx].cmd == 0x71) {
                                if (func_8001FAE0(0, arg) != 0) {
                                    CdCmd_AdvanceRead();
                                }
                            } else if (q->entries[q->readIdx].cmd == 0x73) {
                                if (func_8001FAE0(1, q->field_48) != 0) {
                                    CdCmd_AdvanceRead();
                                }
                            }
                            break;
                    }
                }
            }
        }
    }
}

/// Fade the plaza to white and tear the task down.
///
/// State 0 allocates the `AcropolisPlazaFadeWork` ramp at `Task::idMap` and
/// zeroes it; a failed allocation kills the task outright. State 1 runs every
/// frame: it links a semi-transparent full-screen `TILE` (`-0xA0,-0x78`,
/// `0x140x0xF0`) plus the `0xE1000240` `DR_TPAGE` into `Gpu_CurrentOt[-16]`,
/// tinting the tile `r`/`g`/`r`, then steps all three channels by
/// `Task::spawnArg1`. Once `r` saturates past 0xFF the screen is fully covered,
/// so the task blanks the display and kills itself.
void func_acropolis_plaza_8017D8AC(Task* arg0)
{
    AcropolisPlazaFadeWork* fade;
    AcropolisPlazaFadeWork* alloc;
    u8                      r;
    u8                      g;
    TILE*                   tile;
    DR_TPAGE*               dr;

    fade = (AcropolisPlazaFadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (AcropolisPlazaFadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(Gpu_CurrentOt - 16, tile);

            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(Gpu_CurrentOt - 16, dr);

            fade->r += (u16)arg0->spawnArg1;
            fade->g += (u16)arg0->spawnArg1;
            fade->b += (u16)arg0->spawnArg1;
            if (fade->r >= 0x100) {
                SetDispMask(0);
            kill:
                Task_Kill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza", func_acropolis_plaza_8017DA58);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza", D_acropolis_plaza_8017D5C0);
