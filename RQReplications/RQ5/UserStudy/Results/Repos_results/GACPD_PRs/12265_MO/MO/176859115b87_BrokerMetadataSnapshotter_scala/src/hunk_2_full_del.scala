@@ -51,20 +51,26 @@ class BrokerMetadataSnapshotter(
   val eventQueue = new KafkaEventQueue(time, logContext, threadNamePrefix.getOrElse(""))
 
   override def maybeStartSnapshot(lastContainedLogTime: Long, image: MetadataImage): Boolean = synchronized {
-    if (_currentSnapshotOffset == -1L) {
       val writer = writerBuilder.build(
         image.highestOffsetAndEpoch().offset,
         image.highestOffsetAndEpoch().epoch,
         lastContainedLogTime
       )
-      _currentSnapshotOffset = image.highestOffsetAndEpoch().offset
-      info(s"Creating a new snapshot at offset ${_currentSnapshotOffset}...")
-      eventQueue.append(new CreateSnapshotEvent(image, writer))
-      true
-    } else {
-      warn(s"Declining to create a new snapshot at ${image.highestOffsetAndEpoch()} because " +
-           s"there is already a snapshot in progress at offset ${_currentSnapshotOffset}")
-      false
     }
   }
 
