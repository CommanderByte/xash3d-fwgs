#include "engine/server/server_packet_entities_delta.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

PacketEntityHeaderPlan MakeHeaderPlan(PacketEntityHeaderAction action)
{
	PacketEntityHeaderPlan plan = {};
	plan.action = action;
	return plan;
}

PacketEntityCursorPlan MakeCursorPlan(
	PacketEntityCursorAction action,
	int advanceNew,
	int advanceOld)
{
	PacketEntityCursorPlan plan = {};
	plan.action = action;
	plan.advanceNew = advanceNew;
	plan.advanceOld = advanceOld;
	return plan;
}

}

PacketEntityHeaderPlan BuildPacketEntityHeaderPlan(
	bool hasDeltaSequence,
	int oldFrameFirstEntity,
	int nextClientEntity,
	int clientEntityCapacity,
	int oldEntityCount)
{
	if (!hasDeltaSequence)
		return MakeHeaderPlan(PacketEntityHeaderAction::Full);

	if (oldFrameFirstEntity <= nextClientEntity - clientEntityCapacity)
	{
		PacketEntityHeaderPlan plan =
			MakeHeaderPlan(PacketEntityHeaderAction::Full);
		plan.warnOutdatedDelta = true;
		return plan;
	}

	PacketEntityHeaderPlan plan =
		MakeHeaderPlan(PacketEntityHeaderAction::Delta);
	plan.usePreviousFrame = true;
	plan.oldEntityCount = oldEntityCount;
	return plan;
}

PacketEntityCursorPlan BuildPacketEntityCursorPlan(
	int newIndex,
	int newCount,
	int newNumber,
	int oldIndex,
	int oldCount,
	int oldNumber,
	int endNumber)
{
	if (newIndex >= newCount)
		newNumber = endNumber;

	if (oldIndex >= oldCount)
		oldNumber = endNumber;

	if (newNumber == endNumber && oldNumber == endNumber)
		return MakeCursorPlan(PacketEntityCursorAction::Finish, 0, 0);

	if (newNumber == oldNumber)
		return MakeCursorPlan(PacketEntityCursorAction::DeltaFromOld, 1, 1);

	if (newNumber < oldNumber)
		return MakeCursorPlan(PacketEntityCursorAction::AddFromBaseline, 1, 0);

	return MakeCursorPlan(PacketEntityCursorAction::RemoveFromOld, 0, 1);
}

}
}
}
