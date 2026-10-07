/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Emits recursive dust at a supported model part while room effects are enabled.
///
/// Parts 0, 1, 7, 9, 14 and 17 select fixed local vertical offsets; other
/// parts do nothing. spawnArg is a signed halfword of dust size/period options,
/// promoted to a word with the recursive-child bit set. The effect spawner
/// consumes the stack offset during the call. Requires a live model.
static void _desertChaserSpawnPartDust(Task* task, s16 part, s16 spawnArg)
{
    SVECTOR offset;
    s32     supportedPart;

    switch (part) {
        case 0:
        case 1:
            supportedPart = 1;
            offset.vz     = 0;
            offset.vx     = 0;
            offset.vy     = 0;
            break;
        case 9:
            supportedPart = 1;
            offset.vz     = 0;
            offset.vx     = 0;
            offset.vy     = 0x2BC;
            break;
        case 7:
            supportedPart = 1;
            offset.vz     = 0;
            offset.vx     = 0;
            offset.vy     = 0x2BC;
            break;
        case 14:
        case 17:
            supportedPart = 1;
            offset.vz     = 0;
            offset.vx     = 0;
            offset.vy     = 0x258;
            break;
        default:
            supportedPart = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && supportedPart == 1) {
        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[part], spawnArg | DESERT_CHASER_CUE_DUST_RECURSIVE, &offset);
    }
}
