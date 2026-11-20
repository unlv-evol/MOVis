   val eventQueue = new KafkaEventQueue(time, logContext, threadNamePrefix.getOrElse(""))
 
   override def maybeStartSnapshot(lastContainedLogTime: Long, image: MetadataImage): Boolean = synchronized {
    if (_currentSnapshotOffset != -1) {
      info(s"Declining to create a new snapshot at ${image.highestOffsetAndEpoch()} because " +
        s"there is already a snapshot in progress at offset ${_currentSnapshotOffset}")
      false
    } else {
       val writer = writerBuilder.build(
         image.highestOffsetAndEpoch().offset,
         image.highestOffsetAndEpoch().epoch,
         lastContainedLogTime
       )
      if (writer.nonEmpty) {
        _currentSnapshotOffset = image.highestOffsetAndEpoch().offset
        info(s"Creating a new snapshot at offset ${_currentSnapshotOffset}...")
        eventQueue.append(new CreateSnapshotEvent(image, writer.get))
        true
      } else {
        info(s"Declining to create a new snapshot at ${image.highestOffsetAndEpoch()} because " +
          s"there is already a snapshot at offset ${image.highestOffsetAndEpoch().offset}")
        false
      }
     }
   }
 
