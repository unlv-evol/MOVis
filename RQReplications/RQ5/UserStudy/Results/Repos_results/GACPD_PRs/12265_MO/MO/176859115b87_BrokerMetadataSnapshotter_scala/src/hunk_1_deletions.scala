 trait SnapshotWriterBuilder {
   def build(committedOffset: Long,
             committedEpoch: Int,
            lastContainedLogTime: Long): SnapshotWriter[ApiMessageAndVersion]
 }
 
 class BrokerMetadataSnapshotter(
