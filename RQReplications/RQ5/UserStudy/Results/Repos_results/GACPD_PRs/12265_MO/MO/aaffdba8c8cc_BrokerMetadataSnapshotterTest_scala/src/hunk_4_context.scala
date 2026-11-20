         lastContainedLogTime,
         CompressionType.NONE,
         MetadataRecordSerde.INSTANCE
     }
 
     def consumeSnapshotBuffer(committedOffset: Long, committedEpoch: Int)(buffer: ByteBuffer): Unit = {
