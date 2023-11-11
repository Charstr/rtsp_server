#ifndef _POLLPOLLER_H_
#define _POLLPOLLER_H_
#include <map>
#include <poll.h>
#include <vector>

#include "Poller.h"

class PollPoller : public Poller {
public:
	static PollPoller *createNew();

	PollPoller();
	virtual ~PollPoller();

	virtual bool addIOEvent(std::shared_ptr<IOEvent> event);
	virtual bool updateIOEvent(std::shared_ptr<IOEvent> event);
	virtual bool removeIOEvent(std::shared_ptr<IOEvent> event);
	virtual void handleEvent();

private:
	typedef std::vector<struct pollfd> PollFdList;
	PollFdList mPollFdList;
	typedef std::map<int, int> PollFdMap;
	PollFdMap mPollFdMap; // fd -> index in PollFdList
	std::vector<std::shared_ptr<IOEvent>> mEvents;
};

#endif //_POLLPOLLER_H_