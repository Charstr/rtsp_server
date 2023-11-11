#ifndef _SELECT_POLLER_H_
#define _SELECT_POLLER_H_
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "PollPoller.h"

class SelectPoller : public Poller {
public:
	static SelectPoller *createNew();

	SelectPoller();
	virtual ~SelectPoller();

	virtual bool addIOEvent(std::shared_ptr<IOEvent> event);
	virtual bool updateIOEvent(std::shared_ptr<IOEvent> event);
	virtual bool removeIOEvent(std::shared_ptr<IOEvent> event);
	virtual void handleEvent();

private:
	fd_set mReadSet;
	fd_set mWriteSet;
	fd_set mExceptionSet;
	int mMaxNumSockets;
	std::vector<std::shared_ptr<IOEvent>> mEvents;
};

#endif //_POLLER_H_