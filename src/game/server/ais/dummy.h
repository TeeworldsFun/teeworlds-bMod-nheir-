#ifndef GAME_SERVER_AI_DUMMY_H
#define GAME_SERVER_AI_DUMMY_H

#include <game/server/ai.h>

class CAIDummy : public CAI
{
public:
	CAIDummy(CPlayer *pPlayer) : CAI(pPlayer) {}

	virtual CNetObj_PlayerInput GetInput();

};

#endif
