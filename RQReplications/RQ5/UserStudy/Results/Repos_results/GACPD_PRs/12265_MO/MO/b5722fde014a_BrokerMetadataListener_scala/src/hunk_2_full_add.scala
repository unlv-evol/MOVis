@@ -213,7 +221,7 @@ class BrokerMetadataListener(
             s" ${messageAndVersion.message}")
         }
 
+        _highestOffset = lastCommittedOffset.getOrElse(batch.baseOffset() + index)
 
         delta.replay(highestMetadataOffset, epoch, messageAndVersion.message())
         numRecords += 1
