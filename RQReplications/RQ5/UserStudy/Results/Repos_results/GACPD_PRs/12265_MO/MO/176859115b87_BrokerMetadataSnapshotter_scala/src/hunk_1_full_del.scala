@@ -27,7 +27,7 @@ import org.apache.kafka.snapshot.SnapshotWriter
 trait SnapshotWriterBuilder {
   def build(committedOffset: Long,
             committedEpoch: Int,
-            lastContainedLogTime: Long): SnapshotWriter[ApiMessageAndVersion]
 }
 
 class BrokerMetadataSnapshotter(
