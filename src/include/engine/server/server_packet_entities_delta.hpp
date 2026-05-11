#ifndef XASH_ENGINE_SERVER_PACKET_ENTITIES_DELTA_HPP
#define XASH_ENGINE_SERVER_PACKET_ENTITIES_DELTA_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class PacketEntityHeaderAction
{
	Full = 0,
	Delta = 1
};

struct PacketEntityHeaderPlan
{
	PacketEntityHeaderAction action;
	bool usePreviousFrame;
	bool warnOutdatedDelta;
	int oldEntityCount;
};

enum class PacketEntityCursorAction
{
	Finish = 0,
	DeltaFromOld = 1,
	AddFromBaseline = 2,
	RemoveFromOld = 3
};

struct PacketEntityCursorPlan
{
	PacketEntityCursorAction action;
	int advanceNew;
	int advanceOld;
};

PacketEntityHeaderPlan BuildPacketEntityHeaderPlan(
	bool hasDeltaSequence,
	int oldFrameFirstEntity,
	int nextClientEntity,
	int clientEntityCapacity,
	int oldEntityCount);

PacketEntityCursorPlan BuildPacketEntityCursorPlan(
	int newIndex,
	int newCount,
	int newNumber,
	int oldIndex,
	int oldCount,
	int oldNumber,
	int endNumber);

}
}
}

#endif
