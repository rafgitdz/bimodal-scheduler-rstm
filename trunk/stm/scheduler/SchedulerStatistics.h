#ifndef __STM_SCHEDULERSTATISTICS__
#define __STM_SCHEDULERSTATISTICS__

#include <iostream>

namespace stm {
	namespace scheduler {
		class SchedulerStatistics {
			public:
			
			
				long finalEpoch;
				long numConflicts;
				long numFalsePositive;
				long numAllQueueEmpty;
				long numPushToRO;
			
				SchedulerStatistics() : finalEpoch(0), numConflicts(0), 
				numFalsePositive(0), numAllQueueEmpty(0), numPushToRO(0) {}
				void printStats() {
					std::cout << "Final Epoch: " << finalEpoch << "\n"
					<< "Number of conflicts: " << numConflicts << "\n"
					<< "Number of false positives: " << numFalsePositive << "\n"
					<< "Scheduler went to read epoch because all queues were empty " << numAllQueueEmpty << " times\n"
					<< numPushToRO << " transactions passed through the RO queue\n" ;
				}
		};
		
	}
}


#endif //__STM_SCHEDULERSTATISTICS__
