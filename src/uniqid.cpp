#include "uniqid.h"


namespace forwarder {

	UniqIDGenerator::UniqIDGenerator() :
		count(0),
        recycleThreshold(100000),
        recycleEnabled(true)
	{
	};

	UniqIDGenerator::~UniqIDGenerator() {
		recycled.clear();
	}

	UniqID UniqIDGenerator::getNewID() noexcept {
		if (recycleEnabled && count > recycleThreshold) {
			if (recycled.size() > 0 && recycled.front() > 0) {
				UniqID id = recycled.front();
				recycled.pop_front();
				return id;
			}
		}
		count++;
		return count;
	}
	void UniqIDGenerator::recycleID(UniqID id) noexcept {
		recycled.push_back(id);
	}

};
