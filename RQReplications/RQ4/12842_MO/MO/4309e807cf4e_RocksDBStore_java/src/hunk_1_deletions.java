 
         void flush() throws RocksDBException;
 
        void prepareBatchForRestore(final Collection<KeyValue<byte[], byte[]>> records,
                                    final WriteBatch batch) throws RocksDBException;

         void addToBatch(final byte[] key,
                         final byte[] value,
                         final WriteBatch batch) throws RocksDBException;
