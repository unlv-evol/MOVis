             db.flush(fOptions, newColumnFamily);
         }
 
         @Override
         public void addToBatch(final byte[] key,
                                final byte[] value,
