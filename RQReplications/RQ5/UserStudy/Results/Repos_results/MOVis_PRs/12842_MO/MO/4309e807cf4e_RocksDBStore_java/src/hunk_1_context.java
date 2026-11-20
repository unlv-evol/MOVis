 
         void flush() throws RocksDBException;
 
         void addToBatch(final byte[] key,
                         final byte[] value,
                         final WriteBatch batch) throws RocksDBException;
