#include "uniqid.h"

UniqIDGenerator::UniqIDGenerator() :count(0) {};
	
UniqID UniqIDGenerator::getNewID() noexcept {
	if (count > 10000) {
		if (recycled.front() > 0) {
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
