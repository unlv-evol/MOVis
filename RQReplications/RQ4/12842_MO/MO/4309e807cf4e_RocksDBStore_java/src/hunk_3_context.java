 
     void restoreBatch(final Collection<ConsumerRecord<byte[], byte[]>> records) {
         try (final WriteBatch batch = new WriteBatch()) {
             for (final ConsumerRecord<byte[], byte[]> record : records) {
                 ChangelogRecordDeserializationHelper.applyChecksAndUpdatePosition(
                     record,
                     consistencyEnabled,
                     position
                 );
                 // If version headers are not present or version is V0
             }
             write(batch);
         } catch (final RocksDBException e) {
             throw new ProcessorStateException("Error restoring batch to store " + name, e);
