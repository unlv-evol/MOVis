@@ -262,14 +261,6 @@ public void flush() throws RocksDBException {
             db.flush(fOptions, newColumnFamily);
         }
 
         @Override
         public void addToBatch(final byte[] key,
                                final byte[] value,
