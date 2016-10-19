#include "uniqid.h"

UniqIDGenerator::UniqIDGenerator() :
	count(0), 
	recycled(new std::list<UniqID>)
{
};

UniqIDGenerator::~UniqIDGenerator() {
	delete recycled;
}
	
UniqID UniqIDGenerator::getNewID() noexcept {
	if (count > 10000) {
		if (recycled->size()>0 && recycled->front() > 0) {
			UniqID id = recycled->front();
			recycled->pop_front();
			return id;
		}
	}
	count++;
	return count;
}
void UniqIDGenerator::recycleID(UniqID id) noexcept {
	recycled->push_back(id);
}
