       _publisher = Some(publisher)
       log.info(s"Starting to publish metadata events at offset $highestMetadataOffset.")
       try {
         publish(publisher)
         future.complete(null)
       } catch {
