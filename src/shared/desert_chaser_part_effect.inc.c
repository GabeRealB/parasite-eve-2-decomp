/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Spawn effect 0x60054 on model part `part` when the room's effect mode is 2.
/// Only parts 0, 1, 7, 9, 14 and 17 emit; each carries its own vertical offset
/// in the effect's position argument. `flags` goes to `effectSpawn` with the
/// top bit set.
void desertChaserPartEffect(Task* arg0, s16 part, s16 flags)
{
    SVECTOR sp10;
    s32     spawn;

    switch (part) {
        case 0:
        case 1:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 7:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && spawn == 1) {
        effectSpawn(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[part], flags | 0x80000000, &sp10);
    }
}
