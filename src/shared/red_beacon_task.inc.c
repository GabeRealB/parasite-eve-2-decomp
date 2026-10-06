/* Part of the red beacon library; see red_beacon.h. */

/// Lays out one Gouraud quad as the upper or lower half of the beacon's diamond.
///
/// `beaconQuad` borrows one writable packet; `glowScratch` supplies its projected
/// centre and nonnegative half extent in pixels. `halfIndex` selects 0 (upper)
/// or 1 (lower). Vertices 0 and 3 are the left and right tips, vertex 1 is the
/// selected vertical tip, and vertex 2 is the centre where the glow is brightest.
/// Coordinates narrow to signed 16-bit packet fields without clipping; the
/// caller supplies colours and queues the packet. Neither pointer is retained.
static inline void _redBeaconSetHalfBounds(POLY_G4* beaconQuad, const RoomGlowSpriteScratch* glowScratch, s32 halfIndex)
{
    beaconQuad->x0 = glowScratch->screenPos.vx - glowScratch->halfExtent;
    beaconQuad->x1 = beaconQuad->x2 = glowScratch->screenPos.vx;
    beaconQuad->x3                  = glowScratch->screenPos.vx + glowScratch->halfExtent;
    beaconQuad->y0 = beaconQuad->y2 = beaconQuad->y3 = glowScratch->screenPos.vy;
    beaconQuad->y1                                   = (glowScratch->screenPos.vy - glowScratch->halfExtent) + glowScratch->halfExtent * (halfIndex << 1);
}

void RED_BEACON_TASK(Task* task)
{
    enum {
        RED_BEACON_MIN_DEPTH         = 17,
        RED_BEACON_PULSE_FALLING_BIT = 0x80,
        RED_BEACON_PULSE_PHASE_MASK  = 0x7F,
        RED_BEACON_PULSE_PEAK        = 0x80,
        RED_BEACON_SIZE_UNIT         = 0x200,
        RED_BEACON_DIAMOND_HALVES    = 2,
        RED_BEACON_PULSE_RATE_BYTE   = 0,
        RED_BEACON_SIZE_BYTE         = 1
    };
    RoomGlowSpriteScratch* glowScratch;
    POLY_G4*               beaconQuad;
    GfxCoord*              effectCoord;
    EffectWork*            effectWork;
    s32                    halfIndex;
    s32                    redLevel;
    s32                    pulsePhase;
    s32                    pulseLevel;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    // Project the composed view-space centre, narrowing its translation to signed halfwords.
    glowScratch              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    glowScratch->worldPos.vx = effectCoord->workm.t[0];
    glowScratch->worldPos.vy = effectCoord->workm.t[1];
    glowScratch->worldPos.vz = effectCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&glowScratch->worldPos);
    gte_rtps();
    gte_stsxy(&glowScratch->screenPos);
    gte_stszotz(&glowScratch->otz);
    if (glowScratch->otz >= RED_BEACON_MIN_DEPTH) {
        // The low argument bytes set a shared triangle-wave phase and depth-scaled size.
        // Convert the task union's signed byte view to the beacon's unsigned 0..255 values.
        pulsePhase = gDisplayState.animFrame * (u8)task->spawnArg1.signedBytes[RED_BEACON_PULSE_RATE_BYTE];
        if (pulsePhase & RED_BEACON_PULSE_FALLING_BIT) {
            pulseLevel = RED_BEACON_PULSE_PEAK - (pulsePhase & RED_BEACON_PULSE_PHASE_MASK);
        } else {
            pulseLevel = pulsePhase & RED_BEACON_PULSE_PHASE_MASK;
        }
        redLevel                = pulseLevel;
        glowScratch->halfExtent = ((u8)task->spawnArg1.signedBytes[RED_BEACON_SIZE_BYTE] * RED_BEACON_SIZE_UNIT) / glowScratch->otz;
        // The centre vertex is red; black outer vertices fade both halves to their edges.
        for (halfIndex = 0; halfIndex < RED_BEACON_DIAMOND_HALVES; halfIndex++) {
            beaconQuad     = gGpuPrimCursor;
            gGpuPrimCursor = beaconQuad + 1;
            setPolyG4(beaconQuad);
            setRGB0(beaconQuad, 0, 0, 0);
            setRGB1(beaconQuad, 0, 0, 0);
            setRGB2(beaconQuad, redLevel, 0, 0);
            setRGB3(beaconQuad, 0, 0, 0);
            _redBeaconSetHalfBounds(beaconQuad, glowScratch, halfIndex);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)glowScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    beaconQuad);
            gpuSetPrimitiveBlendMode(beaconQuad, GPU_BLEND_ADD, glowScratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    // Scratch is released before effect teardown can dispatch child exit handlers.
    effectKillTask(effectWork, task);
}
