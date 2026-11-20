 
     override def build(committedOffset: Long,
                        committedEpoch: Int,
                       lastContainedLogTime: Long): Option[SnapshotWriter[ApiMessageAndVersion]] = {
       val offsetAndEpoch = new OffsetAndEpoch(committedOffset, committedEpoch)
       RecordsSnapshotWriter.createWithHeader(
         () => {
