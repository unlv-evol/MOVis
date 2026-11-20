     extends SnapshotWriterBuilder {
   override def build(committedOffset: Long,
                      committedEpoch: Int,
                     lastContainedLogTime: Long): SnapshotWriter[ApiMessageAndVersion] = {
    raftClient.createSnapshot(committedOffset, committedEpoch, lastContainedLogTime).
        asScala.getOrElse(
      throw new RuntimeException("A snapshot already exists with " +
        s"committedOffset=$committedOffset, committedEpoch=$committedEpoch, " +
        s"lastContainedLogTime=$lastContainedLogTime")
    )
   }
 }
 
