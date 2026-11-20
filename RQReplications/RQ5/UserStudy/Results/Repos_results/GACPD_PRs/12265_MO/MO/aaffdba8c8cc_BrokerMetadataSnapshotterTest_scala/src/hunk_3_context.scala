 
     override def build(committedOffset: Long,
                        committedEpoch: Int,
       val offsetAndEpoch = new OffsetAndEpoch(committedOffset, committedEpoch)
       RecordsSnapshotWriter.createWithHeader(
         () => {
