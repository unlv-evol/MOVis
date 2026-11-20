             s" ${messageAndVersion.message}")
         }
 
 
         delta.replay(highestMetadataOffset, epoch, messageAndVersion.message())
         numRecords += 1
