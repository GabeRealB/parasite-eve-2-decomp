/* Part of the model placement library; see model_placement.h. */

/// Tick state of the child in the second state table: copies the spawner's
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW` and `TMD_OBJECT_SKIP_AUTO_BUFFER` onto this
/// task's model. When the spawner's skip bit is clear, the child clears its
/// own and `tmdAllocPrimitiveBuffer` fills a missing buffer; a set bit is copied and
/// the child's buffer is left alone.
void modelPlacementMirrorParent(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = ((Task*)task->spawnArg2.pointer)->extra.tmd;
    object       = task->extra.tmd;

    if (!(parentObject->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        object->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObject->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(object);
        return;
    }
    object->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}
