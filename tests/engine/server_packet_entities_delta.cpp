#include <cstdlib>

#include "engine/server/messaging/server_packet_entities_delta.hpp"

using namespace xash::engine::server;

namespace
{

bool TestHeaderWithoutDeltaSequenceUsesFullPacket()
{
	const PacketEntityHeaderPlan plan =
		BuildPacketEntityHeaderPlan(false, 128, 256, 64, 12);

	return plan.action == PacketEntityHeaderAction::Full &&
		!plan.usePreviousFrame &&
		!plan.warnOutdatedDelta &&
		plan.oldEntityCount == 0;
}

bool TestHeaderWithFreshDeltaSequenceUsesPreviousFrame()
{
	const PacketEntityHeaderPlan plan =
		BuildPacketEntityHeaderPlan(true, 193, 256, 64, 12);

	return plan.action == PacketEntityHeaderAction::Delta &&
		plan.usePreviousFrame &&
		!plan.warnOutdatedDelta &&
		plan.oldEntityCount == 12;
}

bool TestHeaderWithRolledOffDeltaFallsBackToFullPacket()
{
	const PacketEntityHeaderPlan equalBoundary =
		BuildPacketEntityHeaderPlan(true, 192, 256, 64, 12);
	const PacketEntityHeaderPlan older =
		BuildPacketEntityHeaderPlan(true, 191, 256, 64, 12);

	return equalBoundary.action == PacketEntityHeaderAction::Full &&
		equalBoundary.warnOutdatedDelta &&
		!equalBoundary.usePreviousFrame &&
		equalBoundary.oldEntityCount == 0 &&
		older.action == PacketEntityHeaderAction::Full &&
		older.warnOutdatedDelta;
}

bool TestCursorFinishesWhenBothSidesAreExhausted()
{
	const PacketEntityCursorPlan plan =
		BuildPacketEntityCursorPlan(2, 2, 99999, 3, 3, 99999, 99999);

	return plan.action == PacketEntityCursorAction::Finish &&
		plan.advanceNew == 0 &&
		plan.advanceOld == 0;
}

bool TestCursorMatchesEqualEntityNumbers()
{
	const PacketEntityCursorPlan plan =
		BuildPacketEntityCursorPlan(1, 3, 42, 2, 4, 42, 99999);

	return plan.action == PacketEntityCursorAction::DeltaFromOld &&
		plan.advanceNew == 1 &&
		plan.advanceOld == 1;
}

bool TestCursorAddsNewEntityWhenNewNumberIsLower()
{
	const PacketEntityCursorPlan plan =
		BuildPacketEntityCursorPlan(0, 2, 17, 0, 2, 42, 99999);

	return plan.action == PacketEntityCursorAction::AddFromBaseline &&
		plan.advanceNew == 1 &&
		plan.advanceOld == 0;
}

bool TestCursorRemovesOldEntityWhenOldNumberIsLower()
{
	const PacketEntityCursorPlan plan =
		BuildPacketEntityCursorPlan(0, 2, 42, 0, 2, 17, 99999);

	return plan.action == PacketEntityCursorAction::RemoveFromOld &&
		plan.advanceNew == 0 &&
		plan.advanceOld == 1;
}

bool TestCursorHandlesMissingNewOrOldSide()
{
	const PacketEntityCursorPlan oldOnly =
		BuildPacketEntityCursorPlan(2, 2, 1, 0, 2, 17, 99999);
	const PacketEntityCursorPlan newOnly =
		BuildPacketEntityCursorPlan(0, 2, 17, 2, 2, 1, 99999);

	return oldOnly.action == PacketEntityCursorAction::RemoveFromOld &&
		oldOnly.advanceNew == 0 &&
		oldOnly.advanceOld == 1 &&
		newOnly.action == PacketEntityCursorAction::AddFromBaseline &&
		newOnly.advanceNew == 1 &&
		newOnly.advanceOld == 0;
}

}

int main()
{
	if (!TestHeaderWithoutDeltaSequenceUsesFullPacket() ||
		!TestHeaderWithFreshDeltaSequenceUsesPreviousFrame() ||
		!TestHeaderWithRolledOffDeltaFallsBackToFullPacket() ||
		!TestCursorFinishesWhenBothSidesAreExhausted() ||
		!TestCursorMatchesEqualEntityNumbers() ||
		!TestCursorAddsNewEntityWhenNewNumberIsLower() ||
		!TestCursorRemovesOldEntityWhenOldNumberIsLower() ||
		!TestCursorHandlesMissingNewOrOldSide())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
