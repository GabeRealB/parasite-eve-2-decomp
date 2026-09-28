#ifndef GAMEPLAY_PRIVATE_PAD_SCRIPT_H
#define GAMEPLAY_PRIVATE_PAD_SCRIPT_H

#include "types.h"

// Scripted pad commands and their task entry points.

/// Commands are stored as halfwords; the dispatcher selects their low opcode byte.
typedef union GpScriptOpcode {
    u16 command;
    struct {
        u8 opcode;
        u8 operand;
    } bytes;
} GpScriptOpcode;

/// Resume script, held-button and interpolated pad input.
void Gp_ClearPadHalt(void);

#endif // GAMEPLAY_PRIVATE_PAD_SCRIPT_H
