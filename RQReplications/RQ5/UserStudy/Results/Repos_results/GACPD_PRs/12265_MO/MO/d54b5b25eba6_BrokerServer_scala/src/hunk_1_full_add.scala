@@ -62,13 +62,8 @@ class BrokerSnapshotWriterBuilder(raftClient: RaftClient[ApiMessageAndVersion])
     extends SnapshotWriterBuilder {
   override def build(committedOffset: Long,
                      committedEpoch: Int,
+                     lastContainedLogTime: Long): Option[SnapshotWriter[ApiMessageAndVersion]] = {
+    raftClient.createSnapshot(committedOffset, committedEpoch, lastContainedLogTime).asScala
   }
 }
 
