@@ -34,6 +33,7 @@ import org.apache.kafka.snapshot.{MockRawSnapshotWriter, RecordsSnapshotWriter,
 import org.junit.jupiter.api.Assertions.{assertEquals, assertFalse, assertTrue}
 import org.junit.jupiter.api.Test
 
+import scala.compat.java8.OptionConverters._
 
 class BrokerMetadataSnapshotterTest {
   @Test
