/* Function body for the overlay's twin-trail task entry (Task* task).
 * GCC 2.8.1 changes argument spills when this particular body is inlined.
 * Including it in the entry preserves the matched code without extra calls or
 * register/volatile/assembly workarounds. Private drawing helpers remain in
 * the other room_visual_effects fragments. */

/// Two view-relative endpoint histories owned by the effect task's work pointer.
typedef struct {
    GfxCoord first[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT];  // Eight snapshots of the first endpoint
    GfxCoord second[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT]; // Eight snapshots of the second endpoint
} _RoomVisualEffectsTwinTrailHistory;

enum { TRAIL_INITIALIZE,
       TRAIL_RECORD,
       TRAIL_SLOT_MASK         = ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT - 1,
       TRAIL_COLOR_MULTIPLIERS = (1 << 8) | (2 << 4) | 3 }; // R:G:B = 1:2:3

GfxCoord                            secondEndpointCoord;
_RoomVisualEffectsTwinTrailHistory* history;
GfxCoord*                           firstEndpointCoord;
GfxCoord*                           historyFrame;
EffectWork*                         work;
SVECTOR*                            initialOffset;
s32                                 slotIndex;

history            = task->work;
work               = task->spawnArg2.pointer;
firstEndpointCoord = task->extra.coordBody->coord;

if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
    work->age++;
    switch (task->state) {
        case TRAIL_INITIALIZE:
            // Seed both histories from the current endpoints, so the beam starts collapsed.
            history = memCalloc(sizeof(*history), false);
            if (history == NULL) {
                work->age = 0;
                return;
            }
            task->work                       = history;
            firstEndpointCoord->parent       = work->parent;
            firstEndpointCoord->coord.t[0]   = RoomFx_TrailOffsets[0].vx;
            firstEndpointCoord->coord.t[1]   = RoomFx_TrailOffsets[0].vy;
            firstEndpointCoord->coord.t[2]   = RoomFx_TrailOffsets[0].vz;
            firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(firstEndpointCoord);
            task->state                      = TRAIL_RECORD;
            secondEndpointCoord.parent       = work->parent;
            initialOffset                    = &RoomFx_TrailOffsets[1];
            secondEndpointCoord.coord.t[0]   = initialOffset->vx;
            secondEndpointCoord.coord.t[1]   = initialOffset->vy;
            secondEndpointCoord.coord.t[2]   = initialOffset->vz;
            secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&secondEndpointCoord);
            for (slotIndex = 0; slotIndex < ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT; slotIndex++) {
                historyFrame = &history->first[slotIndex];
                _roomVisualEffectsStoreTrailFrame(historyFrame, firstEndpointCoord);
                historyFrame = &history->second[slotIndex];
                _roomVisualEffectsStoreTrailFrame(historyFrame, &secondEndpointCoord);
            }
            break;

        case TRAIL_RECORD:
            // View-relative snapshots keep old edges independent of later anchor movement.
            firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(firstEndpointCoord);
            secondEndpointCoord.parent = work->parent;
            {
                SVECTOR* updateOffset          = &RoomFx_TrailOffsets[1];
                secondEndpointCoord.coord.t[0] = updateOffset->vx;
                secondEndpointCoord.coord.t[1] = updateOffset->vy;
                secondEndpointCoord.coord.t[2] = updateOffset->vz;
            }
            secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&secondEndpointCoord);
            historyFrame = &history->first[work->age & TRAIL_SLOT_MASK];
            _roomVisualEffectsStoreTrailFrame(historyFrame, firstEndpointCoord);
            historyFrame = &history->second[work->age & TRAIL_SLOT_MASK];
            _roomVisualEffectsStoreTrailFrame(historyFrame, &secondEndpointCoord);
            for (slotIndex = 0; slotIndex < ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT; slotIndex++) {
                historyFrame               = &history->first[slotIndex];
                historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(historyFrame);
                historyFrame               = &history->second[slotIndex];
                historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(historyFrame);
            }
            _roomVisualEffectsDrawTwinTrail(history->first, history->second, work->age & TRAIL_SLOT_MASK, TRAIL_COLOR_MULTIPLIERS);
            if (work->age == task->spawnArg1.value && work->age != 0) {
                effectKillTask(work, task);
            }
            break;
    }
}
