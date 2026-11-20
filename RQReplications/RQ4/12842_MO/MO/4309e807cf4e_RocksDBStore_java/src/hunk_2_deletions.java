             db.flush(fOptions, columnFamily);
         }
 
        @Override
        public void prepareBatchForRestore(final Collection<KeyValue<byte[], byte[]>> records,
                                           final WriteBatch batch) throws RocksDBException {
            for (final KeyValue<byte[], byte[]> record : records) {
                addToBatch(record.key, record.value, batch);
            }
        }

         @Override
         public void addToBatch(final byte[] key,
                                final byte[] value,
