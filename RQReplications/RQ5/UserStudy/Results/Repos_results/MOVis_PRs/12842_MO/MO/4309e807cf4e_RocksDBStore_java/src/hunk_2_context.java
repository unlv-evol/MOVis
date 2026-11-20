             db.flush(fOptions, columnFamily);
         }
 
         @Override
         public void addToBatch(final byte[] key,
                                final byte[] value,
