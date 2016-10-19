#ifndef UNIQID_H
#define UNIQID_H
#include "base.h"

typedef unsigned int UniqID;

class UniqIDGenerator
{
public:
	UniqIDGenerator();
	UniqID getNewID() noexcept;
	void recycleID(UniqID id) noexcept;
private:
	std::list<UniqID> recycled;
	UniqID count;
};

#endif
