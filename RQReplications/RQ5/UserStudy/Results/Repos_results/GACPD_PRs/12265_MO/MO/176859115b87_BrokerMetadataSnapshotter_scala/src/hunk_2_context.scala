   val eventQueue = new KafkaEventQueue(time, logContext, threadNamePrefix.getOrElse(""))
 
   override def maybeStartSnapshot(lastContainedLogTime: Long, image: MetadataImage): Boolean = synchronized {
       val writer = writerBuilder.build(
         image.highestOffsetAndEpoch().offset,
         image.highestOffsetAndEpoch().epoch,
         lastContainedLogTime
       )
     }
   }
 
