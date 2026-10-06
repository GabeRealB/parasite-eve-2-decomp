/* Part of the motel room 6 library; see motel_room_6.h. */

void MOTEL_ROOM_6_DRAW_GLOW_TASK(Task* unusedTask)
{
    enum {
        MOTEL_ROOM_6_GLOW_PULSE_RATE                = 0x60, // 4096 angle units per turn, per animation frame
        MOTEL_ROOM_6_GLOW_WIDE_DIAMOND_RADIUS_SCALE = 0x60, // Pixel half-extent is scale * 48 / (camera Z / 4)
        MOTEL_ROOM_6_GLOW_DISC_RADIUS_SCALE         = 0x80, // Outer/inner pixel radii are scale * 64/8 / (camera Z / 4)
    };
    u8 mappedViewIndex;

    // Select the glow shape by the current camera/image mapping, not the logical view ID.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 3:
        case 4:
            _glowDrawWideDiamond(gMotelRoom6GlowPos, MOTEL_ROOM_6_GLOW_PULSE_RATE, MOTEL_ROOM_6_GLOW_WIDE_DIAMOND_RADIUS_SCALE);
            break;
        case 12:
            _glowDrawPulsingDisc(gMotelRoom6GlowPos, MOTEL_ROOM_6_GLOW_PULSE_RATE, MOTEL_ROOM_6_GLOW_DISC_RADIUS_SCALE);
            break;
    }
}
