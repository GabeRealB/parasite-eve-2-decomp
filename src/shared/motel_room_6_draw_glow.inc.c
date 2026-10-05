/* Part of the motel room 6 library; see motel_room_6.h. */

void motelRoom6DrawGlow(Task* unused)
{
    u8 view;

    view = viewGetMappedIndex();
    switch (view) {
        case 3:
        case 4:
            glowDrawWideDiamond(&gMotelRoom6GlowPos[0], 0x60, 0x60);
            break;
        case 12:
            glowDrawPulsingDisc(&gMotelRoom6GlowPos[0], 0x60, 0x80);
            break;
    }
}
