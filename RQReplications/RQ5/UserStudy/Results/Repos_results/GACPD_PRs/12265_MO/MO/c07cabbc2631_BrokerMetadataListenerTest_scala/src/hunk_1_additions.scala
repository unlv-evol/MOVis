 import java.util
 import java.util.concurrent.atomic.AtomicReference
 import java.util.{Collections, Optional}
import org.apache.kafka.common.metadata.{FeatureLevelRecord, PartitionChangeRecord, PartitionRecord, RegisterBrokerRecord, TopicRecord}
 import org.apache.kafka.common.metrics.Metrics
 import org.apache.kafka.common.utils.Time
 import org.apache.kafka.common.{Endpoint, Uuid}
 import org.apache.kafka.image.{MetadataDelta, MetadataImage}
 import org.apache.kafka.metadata.{BrokerRegistration, RecordTestUtils, VersionRange}
import org.apache.kafka.server.common.{ApiMessageAndVersion, MetadataVersion}
 import org.junit.jupiter.api.Assertions.{assertEquals, assertTrue}
 import org.junit.jupiter.api.Test
 
