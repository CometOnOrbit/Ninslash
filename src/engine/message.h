

#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <engine/shared/packer.h>

class CMsgPacker : public CPacker
{
	int m_MsgID;

public:
	explicit CMsgPacker(int Type) :
		m_MsgID(Type)
	{
		Reset();
		AddInt(Type);
	}

	int MsgID() const { return m_MsgID; }
	int HeaderSize() const
	{
		int Result = 1;
		while(Result < Size() && (Data()[Result - 1] & 0x80))
			Result++;
		return Result;
	}
};

#endif
