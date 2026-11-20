     extends SnapshotWriterBuilder {
   override def build(committedOffset: Long,
                      committedEpoch: Int,
                     lastContainedLogTime: Long): Option[SnapshotWriter[ApiMessageAndVersion]] = {
    raftClient.createSnapshot(committedOffset, committedEpoch, lastContainedLogTime).asScala
   }
 }
 
