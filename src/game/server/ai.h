#ifndef GAME_SERVER_AI_H
#define GAME_SERVER_AI_H

#include "alloc.h"
#include "gamecontext.h"
#include "player.h"

// player object
class CAI
{
public:
	CAI(CPlayer *pPlayer) : m_pPlayer(pPlayer) {}
	virtual ~CAI() {}

	virtual CNetObj_PlayerInput GetInput() { return {0}; }

protected:
	CPlayer *m_pPlayer;
};

#endif
