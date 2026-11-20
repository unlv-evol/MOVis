 trait SnapshotWriterBuilder {
   def build(committedOffset: Long,
             committedEpoch: Int,
 }
 
 class BrokerMetadataSnapshotter(
