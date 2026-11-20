         lastContainedLogTime,
         CompressionType.NONE,
         MetadataRecordSerde.INSTANCE
      ).get();
     }
 
     def consumeSnapshotBuffer(committedOffset: Long, committedEpoch: Int)(buffer: ByteBuffer): Unit = {
