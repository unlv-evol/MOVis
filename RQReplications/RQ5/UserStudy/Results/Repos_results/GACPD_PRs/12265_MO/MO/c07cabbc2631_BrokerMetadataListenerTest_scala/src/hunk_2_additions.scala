     }
   }
 
  @Test
  def testNotSnapshotAfterMetadataVersionChangeBeforePublishing(): Unit = {
    val snapshotter = new MockMetadataSnapshotter()
    val listener = newBrokerMetadataListener(snapshotter = Some(snapshotter),
      maxBytesBetweenSnapshots = 1000L)

    updateFeature(listener, feature = MetadataVersion.FEATURE_NAME, MetadataVersion.latest.featureLevel(), 100L)
    listener.getImageRecords().get()
    assertEquals(-1L, snapshotter.activeSnapshotOffset, "We won't generate snapshot on metadata version change before starting publishing")
  }

  @Test
  def testSnapshotAfterMetadataVersionChangeWhenStarting(): Unit = {
    val snapshotter = new MockMetadataSnapshotter()
    val listener = newBrokerMetadataListener(snapshotter = Some(snapshotter),
      maxBytesBetweenSnapshots = 1000L)

    val endOffset = 100L
    updateFeature(listener, feature = MetadataVersion.FEATURE_NAME, MetadataVersion.latest.featureLevel(), endOffset)
    listener.startPublishing(new MockMetadataPublisher()).get()
    assertEquals(endOffset, snapshotter.activeSnapshotOffset, "We should try to generate snapshot when starting publishing")
  }

  @Test
  def testSnapshotAfterMetadataVersionChange(): Unit = {
    val snapshotter = new MockMetadataSnapshotter()
    val listener = newBrokerMetadataListener(snapshotter = Some(snapshotter),
      maxBytesBetweenSnapshots = 1000L)
    listener.startPublishing(new MockMetadataPublisher()).get()

    val endOffset = 100L
    updateFeature(listener, feature = MetadataVersion.FEATURE_NAME, (MetadataVersion.latest().featureLevel() - 1).toShort, endOffset)
    // Waiting for the metadata version update to get processed
    listener.getImageRecords().get()
    assertEquals(endOffset, snapshotter.activeSnapshotOffset, "We should generate snapshot on feature update")
  }

   private def registerBrokers(
     listener: BrokerMetadataListener,
     brokerIds: Iterable[Int],
