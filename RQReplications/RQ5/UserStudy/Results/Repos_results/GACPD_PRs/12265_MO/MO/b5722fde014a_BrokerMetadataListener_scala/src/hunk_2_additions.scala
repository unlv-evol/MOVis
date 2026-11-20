             s" ${messageAndVersion.message}")
         }
 
        _highestOffset = lastCommittedOffset.getOrElse(batch.baseOffset() + index)
 
         delta.replay(highestMetadataOffset, epoch, messageAndVersion.message())
         numRecords += 1
