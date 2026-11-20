         lastContainedLogTime,
         CompressionType.NONE,
         MetadataRecordSerde.INSTANCE
      ).asScala
     }
 
     def consumeSnapshotBuffer(committedOffset: Long, committedEpoch: Int)(buffer: ByteBuffer): Unit = {
