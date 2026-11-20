 trait SnapshotWriterBuilder {
   def build(committedOffset: Long,
             committedEpoch: Int,
            lastContainedLogTime: Long): Option[SnapshotWriter[ApiMessageAndVersion]]
 }
 
 class BrokerMetadataSnapshotter(
