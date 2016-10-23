#ifndef UNIQID_H
#define UNIQID_H
#include "base.h"

typedef unsigned int UniqID;

class UniqIDGenerator
{
public:
	UniqIDGenerator();
	~UniqIDGenerator();
	UniqID getNewID() noexcept;
	void recycleID(UniqID id) noexcept;
	inline int getCount() const noexcept {
		return count;
	}
	inline int getPecycledLength() const noexcept {
		return recycled.size();
	}
private:
	std::list<UniqID> recycled;
	UniqID count;
};

#endif
