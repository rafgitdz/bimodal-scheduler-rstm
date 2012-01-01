# pull in all global Make variable definitions
include ../../Makefile.in

INCLUDEPATH = -I./ -I../ -I../../

SCHEDULER_OBJS = BiModalScheduler.o RunnerThread.o ThreadLock.o Queue.o ThreadData.o

LIBSCHEDULER = ../obj/libscheduler.a

.PHONY: all
all:  $(SCHEDULER_OBJS) $(LIBSCHEDULER)
scheduler: $(SCHEDULER_OBJS) $(LIBSCHEDULER)

BiModalScheduler.o: BiModalScheduler.cpp BiModalScheduler.h RescheduleException.h RunnerThread.o ThreadLock.o Queue.o ThreadData.o
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

RunnerThread.o: RunnerThread.cpp RunnerThread.h RescheduleException.h Queue.o ThreadData.o
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

ThreadLock.o: ThreadLock.cpp ThreadLock.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

Queue.o: Queue.cpp Queue.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

ThreadData.o: ThreadData.cpp ThreadData.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(LIBSCHEDULER): $(SCHEDULER_OBJS)
	$(AR) cru $@ $^

clean:
	rm -f $(SCHEDULER_OBJS)
	rm -f $(LIBSCHEDULER) 
