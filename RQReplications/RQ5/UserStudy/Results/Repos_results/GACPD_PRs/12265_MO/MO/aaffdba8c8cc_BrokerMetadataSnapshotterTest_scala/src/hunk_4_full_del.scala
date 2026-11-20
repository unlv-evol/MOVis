@@ -62,7 +62,7 @@ class BrokerMetadataSnapshotterTest {
         lastContainedLogTime,
         CompressionType.NONE,
         MetadataRecordSerde.INSTANCE
-      ).get();
     }
 
     def consumeSnapshotBuffer(committedOffset: Long, committedEpoch: Int)(buffer: ByteBuffer): Unit = {
