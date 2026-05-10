#ifndef XASH_ENGINE_SERVER_CHALLENGE_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_CHALLENGE_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned int SV_ChallengePolicy_TimeWindow(double realtime_seconds);
unsigned int SV_ChallengePolicy_PreviousTimeWindow(unsigned int current_window);

#ifdef __cplusplus
}
#endif

#endif
