/* Part of the room variants library; see room_variants.h. */

/// Resolves a departure's initialized destination selectors through a stage resolver.
///
/// The resolver must accept one aliased request/reply and read only area,
/// arrival, room and execution choice. Other message bytes remain uninitialized.
/// Borrows both pointers for this call and preserves stage, facing and sound.
static inline void _roomVariantResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolveVariant)
{
    RoomEventMsg request;

    request.areaId    = departure->area;
    request.warp      = departure->warp;
    request.room      = departure->room;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    resolveVariant(&request, &request);
    departure->area = request.areaId;
    departure->warp = request.warp;
    departure->room = request.room;
}
