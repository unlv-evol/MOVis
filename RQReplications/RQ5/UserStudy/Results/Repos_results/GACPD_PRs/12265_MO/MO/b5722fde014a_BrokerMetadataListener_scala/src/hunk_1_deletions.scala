       } finally {
         reader.close()
       }
      _publisher.foreach(publish)
 
      // If we detected a change in metadata.version, generate a local snapshot
      val metadataVersionChanged = Option(_delta.featuresDelta()).exists { featuresDelta =>
        featuresDelta.metadataVersionChange().isPresent
       }
 
      snapshotter.foreach { snapshotter =>
        _bytesSinceLastSnapshot = _bytesSinceLastSnapshot + results.numBytes
        if (shouldSnapshot() || metadataVersionChanged) {
          if (snapshotter.maybeStartSnapshot(_highestTimestamp, _delta.apply())) {
            _bytesSinceLastSnapshot = 0L
          }
        }
      }
     }
   }
 
   private def shouldSnapshot(): Boolean = {
    _bytesSinceLastSnapshot >= maxBytesBetweenSnapshots
   }
 
   /**
