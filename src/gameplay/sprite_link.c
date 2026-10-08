#include "gameplay/loading.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "hud_sprites.h"
#include "loading.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

/// Texture-page bits retained from a source when building the GPU draw-mode word.
enum { SPRITE_SOURCE_TEXTURE_PAGE_MASK = 0x9FF };

/// GPU draw-mode command preceding each merged view sprite.
enum { SPRITE_DRAW_MODE_COMMAND = 0xE1000000 };

SpriteDrawModePacket* Gp_SprtCursor;

/// Dual-buffer primitive list heads, indexed by `gDisplayState.drawBuffer`.
/// Allocated by `spriteAllocateViewCachedPackets`; `Gp_SprtLists[1]` is the second half of
/// the same block.
extern SpriteDrawModePacket* Gp_SprtLists[];

static void _spriteEmitBatch(const SpriteSource* sourceElements, const SpriteBatch* batch);

static void _spriteSetViewRawTexture(s32 rawTexture);

static void _worldCollisionBindCurrentRoomResources(Task* unusedTask);

static s32 _spriteViewUsesImageStrips(void);

static void _spriteQueueViewDrawAreas(void);

static void _spriteLinkCachedBatch(const SpriteSource* sources, const SpriteBatch* batch);

static void _spriteInitViewBackgroundTask(Task* task);

static void _spriteLinkViewCachedPacketsTask(Task* task);

SpriteDrawModePacket* Gp_SprtLists[2] = {
    NULL,
    NULL,
};

/// Links a room's contiguous trigger records to the current view and enables them.
///
/// The non-NULL writable array includes a LAST-marked final record; every record,
/// including that final one, is linked in order. listIndex selects ACTION or
/// VIEW_BOUNDARIES. Records must be unlinked or already in that same list and
/// remain live until unlinking or clearing. Geometry and hit latches survive;
/// each coordinate is replaced by the live view coordinate before linking.
static inline void _worldCollisionBindViewTriggerArray(WorldCollisionTrigger* triggers, s32 listIndex)
{
    s32 triggerIndex;

    for (triggerIndex = 0;; triggerIndex++) {
        triggers[triggerIndex].coord = &gGfxViewCoord;
        worldCollisionLinkTrigger(listIndex, &triggers[triggerIndex]);
        triggers[triggerIndex].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
        if (triggers[triggerIndex].flags & WORLD_COLLISION_TRIGGER_LAST) {
            break;
        }
    }
}

/// Initializes the headers of one merged draw-mode and sprite packet.
///
/// Requires a word-aligned, writable `SpriteDrawModePacket` and a readable
/// texture-page halfword outside that packet. Page and blend bits are retained;
/// dithering and drawing into the display area are disabled. `rawTexture` selects
/// ignored RGB (true) or colour modulation (false). Preserves RGB, texture
/// coordinates, CLUT and geometry; callers apply source code flags afterwards.
/// The merge clears the sprite tag and makes it a no-op in the six-word payload.
/// Leaves the draw-mode tag's link address intact for subsequent OT linking.
static inline void _spriteInitDrawModePacket(SpriteDrawModePacket* drawPacket, const u16* texturePageAddress, bool rawTexture)
{
    SpritePacket* spritePacket = &drawPacket->sprite;
    u32           texturePageBits;

    // Retain each emitter's source-read and header-write order for matching.
    if (rawTexture != 0) {
        setlen(&drawPacket->drawMode, ARRAY_SIZE(drawPacket->drawMode.code));
        texturePageBits = *texturePageAddress;
        setSprt(&drawPacket->sprite.sprt);
        drawPacket->sprite.sprt.code |= SPRITE_SOURCE_RAW_TEXTURE;
    } else {
        texturePageBits = *texturePageAddress;
        setlen(&drawPacket->drawMode, ARRAY_SIZE(drawPacket->drawMode.code));
        setSprt(&spritePacket->sprt);
    }
    drawPacket->drawMode.code[0] = SPRITE_DRAW_MODE_COMMAND | (texturePageBits & SPRITE_SOURCE_TEXTURE_PAGE_MASK);
    MargePrim(&drawPacket->drawMode, &spritePacket->sprt);
}

/// Borrows the sprite descriptor selected by the current logical room view.
///
/// Requires loaded stage/area/room directories and a valid nonzero mapped byte
/// within the selected area's view array. The returned descriptor borrows the
/// room overlay's lifetime; the map is read again on each call.
static inline const SpriteView* _spriteGetCurrentView(void)
{
    const GameSession*     session;
    const GameLocationKey* location;
    ViewIndexTable*        viewIndexTable;
    u8***                  areaViewMaps;
    u8**                   roomViewMaps;
    const u8*              viewMap;
    u8                     mappedViewIndex;
    const SpriteAreaTable* spriteTable;
    SpriteView**           spriteAreaViews;
    const SpriteView*      areaViews;

    session         = gGameSession;
    location        = &session->location.loc;
    viewIndexTable  = Gp_ViewIndexTables[location->stage - 1];
    areaViewMaps    = viewIndexTable->viewMaps;
    roomViewMaps    = areaViewMaps[location->area - 1];
    viewMap         = roomViewMaps[location->room - 1];
    mappedViewIndex = viewMap[location->view - 1];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    spriteAreaViews = spriteTable->areaViews;
    areaViews       = spriteAreaViews[location->area - 1];
    return &areaViews[mappedViewIndex - 1];
}

/// Appends a GPU clipping command to the current frame's packet arena.
///
/// `rect` supplies a rectangle in absolute VRAM pixels, including the selected
/// buffer's origin. Requires a word-aligned arena with `sizeof(DR_AREA)` bytes
/// available. Advances `gGpuPrimCursor` and returns an unlinked packet that
/// remains live until the frame arena is reused; does not queue it in an OT.
static inline DR_AREA* _spriteCreateDrawAreaPacket(RECT* rect)
{
    DR_AREA* packet;

    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    SetDrawArea(packet, rect);
    return packet;
}

void spriteLinkViewCachedPackets(void)
{
    GameLocationKey*       location;
    s32                    mappedViewIndex;
    DisplayState*          display;
    SpriteDrawModePacket** packetBuffers;
    SpriteAreaTable*       spriteTable;
    SpriteView*            areaViews;
    SpriteBatch*           batch;
    SpriteSource*          sources;

    location        = &gGameSession->location.loc;
    mappedViewIndex = viewGetMappedIndex();
    packetBuffers   = Gp_SprtLists;
    display         = &gDisplayState;
    Gp_SprtCursor   = packetBuffers[display->drawBuffer];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    areaViews       = spriteTable->areaViews[location->area - 1];
    batch           = areaViews[(u8)mappedViewIndex - 1].batches;
    sources         = areaViews[(u8)mappedViewIndex - 1].sources.elements;
    // A zero first count keeps image strips and skips the first batch before testing its end marker.
    if (batch->spriteCount == 0) {
        batch++;
    } else {
        display->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                _spriteLinkCachedBatch(sources, batch);
            }
            batch++;
        } while (batch->firstSprite != SPRITE_BATCH_END);
    }
}

/// Builds and depth-links one source range in the current frame's packet arena.
///
/// `firstSprite` and `spriteCount` select source elements; the complete range
/// must remain live. Reserves one `SpriteDrawModePacket` per element, ignoring
/// batch visibility and cached-packet exclusion. Raw texture skips the RGB copy;
/// otherwise RGB modulates the texture. Requires a word-aligned arena with space
/// for the whole range and a 1024-tag OT. Source depths use the display's shift
/// and masked byte-offset conversion. A zero count consumes no packet storage.
static void _spriteEmitBatch(const SpriteSource* sourceElements, const SpriteBatch* batch)
{
    u32                   spriteIndex;
    SpriteDrawModePacket* drawPacket;
    const SpriteSource*   texturePageSource;
    const SpriteSource*   source;
    DisplayState*         display;
    u32                   packetLengthMask;
    u32                   linkAddressMask;
    SpritePacket*         spritePacket;

    spriteIndex       = 0;
    drawPacket        = gGpuPrimCursor;
    texturePageSource = sourceElements + batch->firstSprite;
    gGpuPrimCursor    = drawPacket + batch->spriteCount;
    if (batch->spriteCount != 0) {
        display          = &gDisplayState;
        linkAddressMask  = GPU_DMA_LINK_ADDRESS_MASK;
        packetLengthMask = GPU_DMA_PACKET_LENGTH_MASK;
        source           = texturePageSource;
        do {
            spritePacket = &drawPacket->sprite;
            // Copy source geometry and texture state, then link the merged packet by depth.
            if ((source->codeFlags & SPRITE_SOURCE_RAW_TEXTURE) == 0) {
                spritePacket->packed.color = GPU_PRIMITIVE_COLOR_WORD(source, 0);
            }
            _spriteInitDrawModePacket(drawPacket, &texturePageSource->tpage, false);
            spritePacket->sprt.code      |= source->codeFlags;
            spritePacket->packed.uv       = source->uv.packed;
            spritePacket->sprt.clut       = source->clut;
            spritePacket->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
            spriteIndex++;
            spritePacket->packed.size = source->size.packed;
            texturePageSource++;
            drawPacket->drawMode.tag = (drawPacket->drawMode.tag & packetLengthMask) | (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & linkAddressMask);
            *GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) =
                (*GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)source->depth << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) & packetLengthMask) | ((u32)drawPacket & linkAddressMask);
            drawPacket++;
            source++;
        } while (spriteIndex < batch->spriteCount);
    }
}

/// Enables or disables raw texture on the current buffer's cached view sprites.
///
/// Nonzero `rawTexture` ignores packet RGB; zero enables colour modulation.
/// Requires live view resources and initialized cached packets. Excluded batches
/// consume no packets, while hidden batches are updated too. Only the selected
/// draw buffer changes; the other buffer and source flags retain their values.
/// Leaves the published cached-packet cursor at the selected buffer's start.
static void _spriteSetViewRawTexture(s32 rawTexture)
{
    GameLocationKey*      location;
    s32                   mappedViewIndex;
    SpriteDrawModePacket* packet;
    SpriteAreaTable*      spriteTable;
    SpriteView*           areaViews;
    SpriteBatch*          batch;
    u32                   spriteIndex;

    location        = &gGameSession->location.loc;
    mappedViewIndex = viewGetMappedIndex();
    Gp_SprtCursor   = Gp_SprtLists[gDisplayState.drawBuffer];
    spriteTable     = Gp_SprtTables[location->stage - 1];
    areaViews       = spriteTable->areaViews[location->area - 1];
    batch           = areaViews[(u8)mappedViewIndex - 1].batches;
    packet          = Gp_SprtCursor;
    if (batch->firstSprite != SPRITE_BATCH_END) {
        do {
            if (batch->skipCachedPackets == 0) {
                if (Gp_SprtLists[0] != NULL) {
                    for (spriteIndex = 0; spriteIndex < batch->spriteCount; spriteIndex++) {
                        if (rawTexture != 0) {
                            packet->sprite.sprt.code |= SPRITE_SOURCE_RAW_TEXTURE;
                        } else {
                            packet->sprite.sprt.code &= ~SPRITE_SOURCE_RAW_TEXTURE;
                        }
                        packet++;
                    }
                }
            }
            batch++;
        } while (batch->firstSprite != SPRITE_BATCH_END);
    }
}

/// Views the start of a cached-packet buffer through its allocation's word view.
///
/// Converts the address without reading, initializing or allocating storage.
/// NULL remains NULL. A non-NULL address must be word-aligned and have room for
/// the caller's packet count; the returned packets share the allocation's life.
/// The inline call preserves word-offset expansion when locating buffer two.
static inline SpriteDrawModePacket* _spriteCachedPacketsAtWord(u32* bufferWords)
{
    return (SpriteDrawModePacket*)bufferWords;
}

void spriteAllocateViewCachedPackets(void)
{
    enum {
        SPRITE_CACHED_INITIAL_COLOR_WORD = GPU_PACK_COLOR_WORD(0, 0x80, 0, 0),
        /// GPU words in one cached packet.
        SPRITE_CACHED_PACKET_WORDS = sizeof(SpriteDrawModePacket) / sizeof(u32),
        /// log2 of the bytes the allocation spends per word of one buffer: two
        /// buffers of four-byte words.
        SPRITE_CACHED_BYTES_PER_BUFFER_WORD_SHIFT = 3
    };

    const GameLocationKey* location;
    u8                     mappedViewIndex;
    u32                    spriteCount;
    u32                    allocationBytes;
    u32*                   words;
    s32                    spriteIndex;
    const SpriteView*      areaViews;
    const SpriteBatch*     batch;
    const SpriteSource*    sourceElements;
    const SpriteSource*    source;
    s32                    drawBuffer;
    SpriteDrawModePacket*  bufferCursors[2];
    SpriteDrawModePacket*  drawPacket;
    SpritePacket*          spritePacket;

    location        = &gGameSession->location.loc;
    spriteCount     = 0;
    mappedViewIndex = viewGetMappedIndex();
    areaViews       = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];
    batch           = areaViews[mappedViewIndex - 1].batches;
    sourceElements  = areaViews[mappedViewIndex - 1].sources.elements;
    while (batch->firstSprite != SPRITE_BATCH_END) {
        spriteCount += batch->spriteCount;
        batch++;
    }
    // Reserve both buffers, including unused capacity for excluded batches.
    allocationBytes = (spriteCount * SPRITE_CACHED_PACKET_WORDS) << SPRITE_CACHED_BYTES_PER_BUFFER_WORD_SHIFT;
    if (allocationBytes == 0) {
        Gp_SprtLists[0] = NULL;
        return;
    }
    words           = memCalloc(allocationBytes, true);
    Gp_SprtLists[0] = _spriteCachedPacketsAtWord(words);
    if (words == NULL) {
        return;
    }
    // The second buffer starts halfway through the words. Its address is an
    // inline's argument so that the word offset is the sum's first operand.
    {
        register SpriteDrawModePacket* secondBuffer asm("s1");

        secondBuffer     = _spriteCachedPacketsAtWord(&words[allocationBytes >> SPRITE_CACHED_BYTES_PER_BUFFER_WORD_SHIFT]);
        Gp_SprtLists[1]  = secondBuffer;
        bufferCursors[0] = Gp_SprtLists[0];
        bufferCursors[1] = secondBuffer;
    }
    for (batch = areaViews[mappedViewIndex - 1].batches; batch->firstSprite != SPRITE_BATCH_END; batch++) {
        if (batch->skipCachedPackets != 0) {
            continue;
        }
        for (drawBuffer = 0; drawBuffer < ARRAY_SIZE(bufferCursors); drawBuffer++) {
            // Snapshot the source range into both buffers with raw texture enabled.
            source = sourceElements + batch->firstSprite;
            for (spriteIndex = 0; spriteIndex < batch->spriteCount; spriteIndex++) {
                drawPacket                 = bufferCursors[drawBuffer];
                spritePacket               = &drawPacket->sprite;
                spritePacket->packed.color = SPRITE_CACHED_INITIAL_COLOR_WORD;
                _spriteInitDrawModePacket(drawPacket, &source->tpage, true);
                spritePacket->sprt.code      |= source->codeFlags;
                spritePacket->packed.uv       = source->uv.packed;
                spritePacket->sprt.clut       = source->clut;
                spritePacket->packed.position = GPU_PRIMITIVE_XY_WORD(source, 0);
                spritePacket->packed.size     = source->size.packed;
                source++;
                bufferCursors[drawBuffer]++;
            }
        }
    }
}

/// Replaces the active collision resources with those of the current room.
///
/// Applies the current mapped camera, clears the old trigger/occluder lists and
/// grid, then borrows and enables each available room resource. Non-NULL arrays
/// include their LAST-marked record. The stage/area/room directories and camera
/// must be loaded and valid; old list storage must stay live until cleared, and
/// new resource storage must remain live until it is cleared or replaced.
/// Rebuilds the view coordinate's composition cache and changes GTE state.
/// `unusedTask` is not inspected.
static void _worldCollisionBindCurrentRoomResources(Task* unusedTask)
{
    enum { WORLD_COLLISION_OCCLUDER_LIST_ACTIVE = 0 };

    const GameLocationKey*             location;
    const WorldCollisionRoomResources* areaRooms;
    WorldCollisionGrid*                grid;
    WorldCollisionTrigger*             viewBoundaryTriggers;
    WorldCollisionTrigger*             actionTriggers;
    WorldCollisionOccluder*            occluders;
    s32                                occluderIndex;

    location = &gGameSession->location.loc;
    // Retire borrowed list links before binding resources to the current camera.
    viewApplyCurrentCamera();
    Gp_GridParams = NULL;
    worldCollisionClearTriggerList(WORLD_COLLISION_TRIGGER_LIST_VIEW_BOUNDARIES);
    worldCollisionClearTriggerList(WORLD_COLLISION_TRIGGER_LIST_ACTION);
    worldCollisionClearOccluderList(WORLD_COLLISION_OCCLUDER_LIST_ACTIVE);
    areaRooms = Gp_RoomObjTables[location->stage - 1]->areaRooms[location->area - 1];
    if (areaRooms != NULL) {
        grid                 = areaRooms[location->room - 1].grid;
        viewBoundaryTriggers = areaRooms[location->room - 1].viewBoundaryTriggers;
        actionTriggers       = areaRooms[location->room - 1].actionTriggers;
        occluders            = areaRooms[location->room - 1].occluders;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord = &gGfxViewCoord;
            Gp_GridParams   = grid;
        }
        if (viewBoundaryTriggers != NULL) {
            // The LAST record is part of each array and is linked before stopping.
            _worldCollisionBindViewTriggerArray(viewBoundaryTriggers, WORLD_COLLISION_TRIGGER_LIST_VIEW_BOUNDARIES);
        }
        if (actionTriggers != NULL) {
            _worldCollisionBindViewTriggerArray(actionTriggers, WORLD_COLLISION_TRIGGER_LIST_ACTION);
        }
        if (occluders != NULL) {
            for (occluderIndex = 0;; occluderIndex++) {
                worldCollisionLinkOccluder(WORLD_COLLISION_OCCLUDER_LIST_ACTIVE, &occluders[occluderIndex]);
                occluders[occluderIndex].flags |= WORLD_COLLISION_OCCLUDER_ENABLED;
                if (occluders[occluderIndex].flags & WORLD_COLLISION_OCCLUDER_LAST) {
                    break;
                }
            }
        }
    }
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
}

s8 viewFindLogicalIndex(s32 mappedViewIndex)
{
    enum { VIEW_LOGICAL_INDEX_NOT_FOUND = 0 };

    s16                    logicalViewOffset;
    const GameLocationKey* location;
    ViewCount              viewCount;
    const u8*              viewMap;

    logicalViewOffset = 0;
    location          = &gGameSession->location.loc;
    viewCount         = Gp_ViewCountTables[location->stage - 1]->viewCounts[location->area - 1][location->room - 1];
    viewMap           = Gp_ViewIndexTables[location->stage - 1]->viewMaps[location->area - 1][location->room - 1];
    // Search logical slots in order: duplicate mapped indices select the first.
    if (viewCount > 0) {
        do {
            if (viewMap[logicalViewOffset] == (u8)mappedViewIndex) {
                return logicalViewOffset + 1;
            }
            logicalViewOffset++;
        } while (logicalViewOffset < viewCount);
    }
    return VIEW_LOGICAL_INDEX_NOT_FOUND;
}

/// Returns 1 to select decoded-image strips, or 0 to suppress that background.
///
/// Tests only the mapped view's first batch count, including a terminal first
/// record; it does not count sprites or test the whole list for emptiness.
/// Requires valid loaded stage/area/room/view indices and a first batch record.
static s32 _spriteViewUsesImageStrips(void)
{
    GameSession*      session;
    GameLocationKey*  location;
    SpriteAreaTable** stageSpriteEntry;
    s32               stageIndex;
    ViewIndexTable*   viewIndexTable;
    u8***             areaViewMaps;
    u8**              roomViewMaps;
    u8*               viewMap;
    u8                mappedViewIndex;
    SpriteAreaTable*  spriteTable;
    SpriteView**      spriteAreaViews;
    SpriteView*       areaViews;

    session          = gGameSession;
    stageSpriteEntry = Gp_SprtTables;
    location         = &session->location.loc;
    stageIndex       = location->stage - 1;
    stageSpriteEntry = &stageSpriteEntry[stageIndex];
    viewIndexTable   = Gp_ViewIndexTables[stageIndex];
    areaViewMaps     = viewIndexTable->viewMaps;
    roomViewMaps     = areaViewMaps[location->area - 1];
    viewMap          = roomViewMaps[location->room - 1];
    mappedViewIndex  = viewMap[location->view - 1];
    spriteTable      = *stageSpriteEntry;
    spriteAreaViews  = spriteTable->areaViews;
    areaViews        = spriteAreaViews[location->area - 1];
    return areaViews[mappedViewIndex - 1].batches->spriteCount == 0;
}

/// Queues the current view's clipping areas and depth-sorted full-buffer restores.
///
/// Each nonterminal draw-area record consumes two `DR_AREA` packets from the
/// current frame arena. Coordinates are pixels in the fixed 320x240 view;
/// buffer 1 adds its 272-row VRAM origin. The clip is linked at the last depth
/// tag, then the full-buffer restore at the scaled, masked `restoreDepth`.
/// Requires live view resources, sufficient packet storage and a 1024-tag OT.
static void _spriteQueueViewDrawAreas(void)
{
    enum {
        SPRITE_VIEW_WIDTH_PIXELS    = 320,
        SPRITE_VIEW_HEIGHT_PIXELS   = 240,
        SPRITE_VIEW_BUFFER_Y_STRIDE = 272
    };
    RECT            rect;
    SpriteDrawArea* drawArea;
    DR_AREA*        drawAreaPacket;

    drawArea = _spriteGetCurrentView()->drawAreas;
    if (drawArea != NULL) {
        for (; drawArea->restoreDepth != SPRITE_DRAW_AREA_END; drawArea++) {
            // Apply the view clip before depth-sorted drawing begins.
            rect = drawArea->clipRect;
            if (gDisplayState.drawBuffer != 0) {
                rect.y += SPRITE_VIEW_BUFFER_Y_STRIDE;
            }
            drawAreaPacket = _spriteCreateDrawAreaPacket(&rect);
            addPrim(&gGpuCurrentOt[GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt)], drawAreaPacket);
            // Restore the full draw buffer at this record's sorting boundary.
            if (gDisplayState.drawBuffer != 0) {
                rect.y = SPRITE_VIEW_BUFFER_Y_STRIDE;
            } else {
                rect.y = 0;
            }
            rect.x         = 0;
            rect.w         = SPRITE_VIEW_WIDTH_PIXELS;
            rect.h         = SPRITE_VIEW_HEIGHT_PIXELS;
            drawAreaPacket = _spriteCreateDrawAreaPacket(&rect);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)drawArea->restoreDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), drawAreaPacket);
        }
    }
}

s32 viewGetMappedIndex(void)
{
    const GameSession*     session;
    const GameLocationKey* location;
    u8***                  areaViewMaps;
    u8**                   roomViewMaps;
    const u8*              viewMap;

    session      = gGameSession;
    location     = &session->location.loc;
    areaViewMaps = Gp_ViewIndexTables[location->stage - 1]->viewMaps;
    roomViewMaps = areaViewMaps[location->area - 1];
    viewMap      = roomViewMaps[location->room - 1];
    return viewMap[location->view - 1];
}

SpriteDrawArea* spriteGetViewDrawAreas(void)
{
    return _spriteGetCurrentView()->drawAreas;
}

void loadingUpdateRoomResourcesTask(Task* task)
{
    // The spawn argument becomes a cache of the last logical view composed.
    if (task->spawnArg1.value != gGameSession->location.loc.view) {
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        task->spawnArg1.value = gGameSession->location.loc.view;
    }
    if (gGameSession->roomObjsDirty != 0) {
        _worldCollisionBindCurrentRoomResources(task);
        gGameSession->roomObjsDirty = 0;
    }
    _spriteQueueViewDrawAreas();
}

/// Links one included batch from the cached packet cursor at current source depths.
///
/// `firstSprite` and `spriteCount` select a valid source range in elements.
/// Requires initialized packets and a 1024-tag depth-sorted ordering table.
/// Hidden batches advance the cursor without linking; a missing allocation
/// leaves the cursor intact. The caller skips `skipCachedPackets` batches.
static void _spriteLinkCachedBatch(const SpriteSource* sources, const SpriteBatch* batch)
{
    u32                   spriteIndex;
    SpriteDrawModePacket* packet;
    const SpriteSource*   source;

    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    packet = Gp_SprtCursor;
    source = sources + batch->firstSprite;
    // Hiding a batch preserves its packet positions for later batches.
    for (spriteIndex = 0; spriteIndex < batch->spriteCount; packet++, spriteIndex++, source++) {
        if (batch->hidden == 0) {
            addPrim(&gGpuCurrentOt[((u32)source->depth << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)], packet);
        }
    }
    Gp_SprtCursor = packet;
}

void loadingRoomResourcesTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    // Snapshot all three handlers before testing whether room updates are frozen.
    stateHandlers = Gp_RoomObjStates;
    if (gGameSession->freezeRoomObjs == 0) {
        stateHandlers.funcs[task->state](task);
    } else {
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
}

void spriteAllocateViewCachedPacketsTask(Task* task)
{
    spriteAllocateViewCachedPackets();
    taskKill(task);
}

void spriteViewTask(Task* task)
{
    enum {
        SPRITE_VIEW_STATE_INIT_BACKGROUND = 0,
        SPRITE_VIEW_STATE_LINK_PACKETS    = 1
    };
    TaskFunc stateHandlers[] = {
        [SPRITE_VIEW_STATE_INIT_BACKGROUND] = _spriteInitViewBackgroundTask,
        [SPRITE_VIEW_STATE_LINK_PACKETS]    = _spriteLinkViewCachedPacketsTask
    };

    if (gGameSession->freezeRoomObjs == 0) {
        stateHandlers[task->state](task);
    }
}

/// Selects the mapped view's initial background source and advances to linking.
///
/// The first batch count selects decoded strips (zero) or no image (nonzero).
/// Called in state 0 by the unfrozen view-sprite task; advances to state 1.
static void _spriteInitViewBackgroundTask(Task* task)
{
    s32 useImageStrips;

    useImageStrips                          = _spriteViewUsesImageStrips();
    gDisplayState.control.flags.imageSource = useImageStrips;
    task->state++;
}

/// Links cached view sprites when the game drawing path is enabled.
///
/// Task-owned presentation or nonzero `skipDraw` instead refreshes the selected
/// background (first batch count zero: decoded strips; nonzero: no image).
/// Called repeatedly in state 1 of the unfrozen view-sprite task; `task` is
/// unused but preserves the `TaskFunc` callback signature. Linking requires
/// live view resources and cached packets satisfying `spriteLinkViewCachedPackets`.
static void _spriteLinkViewCachedPacketsTask(Task* task)
{
    DisplayState* display;
    s32           useImageStrips;

    display = &gDisplayState;
    if ((display->displayOwner != DISPLAY_OWNER_TASK) && (display->skipDraw == 0)) {
        spriteLinkViewCachedPackets();
    } else {
        useImageStrips                          = _spriteViewUsesImageStrips();
        gDisplayState.control.flags.imageSource = useImageStrips;
    }
}
