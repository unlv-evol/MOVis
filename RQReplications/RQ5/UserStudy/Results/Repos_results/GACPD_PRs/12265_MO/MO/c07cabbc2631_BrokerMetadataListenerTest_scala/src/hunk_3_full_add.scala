@@ -285,4 +321,23 @@ class BrokerMetadataListenerTest {
     )
   }
 
+  private def updateFeature(
+    listener: BrokerMetadataListener,
+    feature: String,
+    version: Short,
+    endOffset: Long
+  ): Unit = {
+    listener.handleCommit(
+      RecordTestUtils.mockBatchReader(
+        endOffset,
+        0,
+        util.Arrays.asList(
+          new ApiMessageAndVersion(new FeatureLevelRecord().
+            setName(feature).
+            setFeatureLevel(version), 0.toShort)
+        )
+      )
+    )
+  }
+
 }
