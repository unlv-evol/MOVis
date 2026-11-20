@@ -117,26 +117,34 @@ class BrokerMetadataListener(
       } finally {
         reader.close()
       }
 
+      _bytesSinceLastSnapshot = _bytesSinceLastSnapshot + results.numBytes
+      if (shouldSnapshot()) {
+        maybeStartSnapshot()
       }
 
+      _publisher.foreach(publish)
     }
   }
 
   private def shouldSnapshot(): Boolean = {
+    (_bytesSinceLastSnapshot >= maxBytesBetweenSnapshots) || metadataVersionChanged()
+  }
+
+  private def metadataVersionChanged(): Boolean = {
+    // The _publisher is empty before starting publishing, and we won't compute feature delta
+    // until we starting publishing
+    _publisher.nonEmpty && Option(_delta.featuresDelta()).exists { featuresDelta =>
+      featuresDelta.metadataVersionChange().isPresent
+    }
+  }
+
+  private def maybeStartSnapshot(): Unit = {
+    snapshotter.foreach { snapshotter =>
+      if (snapshotter.maybeStartSnapshot(_highestTimestamp, _delta.apply())) {
+        _bytesSinceLastSnapshot = 0L
+      }
+    }
   }
 
   /**
