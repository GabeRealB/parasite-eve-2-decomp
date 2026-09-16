#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H

#include "common.h"

#include "main/task.h"

/// 4-byte payload of the 0x7DB message the loft's script task sends to the
/// slot-4 task (`func_dryfield_night_motel_loft_8017D808`). Only the halfword
/// at 0x2 is written -- raised to 1 before the send -- and it is the same
/// record the other rooms send under that message (`DwtwMsg7DB`,
/// `AcropolisBridgeMsg7DB`), which is why the field is not named here yet.
typedef struct DryfieldNightMotelLoftMsg7DB {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} DryfieldNightMotelLoftMsg7DB;
STATIC_ASSERT_SIZEOF(DryfieldNightMotelLoftMsg7DB, 0x4);

/// First state of the room's script task: publishes the room's message table at
/// `Task::field_24` and claims pointer slot 7, then -- once the slot-4 task
/// exists and the room's night flag (`GameFlag_GetNibble(0x96)`) is up --
/// raises the 0x7DB payload's halfword and hands it to that task. Every state
/// ends by running the shared flag-0xA body `func_dryfield_night_motel_loft_8017D9BC`
/// and advancing `Task::state`.
void func_dryfield_night_motel_loft_8017D808(Task* arg0);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
