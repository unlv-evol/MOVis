       _publisher = Some(publisher)
       log.info(s"Starting to publish metadata events at offset $highestMetadataOffset.")
       try {
        if (metadataVersionChanged()) {
          maybeStartSnapshot()
        }
         publish(publisher)
         future.complete(null)
       } catch {
