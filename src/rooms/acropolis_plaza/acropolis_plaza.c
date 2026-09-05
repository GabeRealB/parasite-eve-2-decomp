#include "common.h"

#include "main/fs.h"
#include "main/stream.h"
#include "main/unknown_syms.h"

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza", func_acropolis_plaza_8017D8AC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza", func_acropolis_plaza_8017DA58);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza", D_acropolis_plaza_8017D5C0);
