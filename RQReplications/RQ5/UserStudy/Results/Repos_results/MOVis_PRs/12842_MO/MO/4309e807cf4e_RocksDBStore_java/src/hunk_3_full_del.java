@@ -727,17 +716,15 @@ public void close() {
 
     void restoreBatch(final Collection<ConsumerRecord<byte[], byte[]>> records) {
         try (final WriteBatch batch = new WriteBatch()) {
-            final List<KeyValue<byte[], byte[]>> keyValues = new ArrayList<>();
             for (final ConsumerRecord<byte[], byte[]> record : records) {
                 ChangelogRecordDeserializationHelper.applyChecksAndUpdatePosition(
                     record,
                     consistencyEnabled,
                     position
                 );
                 // If version headers are not present or version is V0
-                keyValues.add(new KeyValue<>(record.key(), record.value()));
             }
-            dbAccessor.prepareBatchForRestore(keyValues, batch);
             write(batch);
         } catch (final RocksDBException e) {
             throw new ProcessorStateException("Error restoring batch to store " + name, e);
