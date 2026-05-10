#ifndef XASH_ENGINE_SERVER_GAME_DLL_USER_MESSAGE_REGISTRY_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_USER_MESSAGE_REGISTRY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	SV_GAMEDLL_USERMSG_ACTION_REJECT = 0,
	SV_GAMEDLL_USERMSG_ACTION_RETURN_EXISTING = 1,
	SV_GAMEDLL_USERMSG_ACTION_REGISTER_NEW = 2
};

enum
{
	SV_GAMEDLL_USERMSG_REJECT_NONE = 0,
	SV_GAMEDLL_USERMSG_REJECT_EMPTY_NAME = 1,
	SV_GAMEDLL_USERMSG_REJECT_NAME_TOO_LONG = 2,
	SV_GAMEDLL_USERMSG_REJECT_SIZE_TOO_LARGE = 3,
	SV_GAMEDLL_USERMSG_REJECT_CAPACITY_EXCEEDED = 4
};

typedef struct sv_gamedll_user_message_slot_s
{
	const char *name;
	int number;
	int size;
} sv_gamedll_user_message_slot_t;

typedef struct sv_gamedll_user_message_registration_plan_s
{
	int action;
	int reject_reason;
	int slot_index;
	int message_number;
	int stored_size;
	int resend_registration;
} sv_gamedll_user_message_registration_plan_t;

sv_gamedll_user_message_registration_plan_t
SV_GameDllUserMessage_BuildRegistrationPlan(
	const sv_gamedll_user_message_slot_t *slots,
	int slot_count,
	const char *name,
	int requested_size,
	int name_capacity,
	int server_active);

#ifdef __cplusplus
}
#endif

#endif
