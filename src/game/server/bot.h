#ifndef GAME_SERVER_BOT_H
#define GAME_SERVER_BOT_H

#include <base/vmath.h>

#include "gamecontext.h"
#include "botengine.h"

const char g_BotClan[12] = "Love";
const char g_BotName[MAX_CLIENTS][16] = {
	"Anna",
	"Bob",
	"Carlos",
	"David",
	"Eli",
	"Florianne",
	"Gaia",
	"Hannibal",
	"Isis",
	"Juda",
	"Kevin",
	"Lucile",
	"Marc",
	"Naustradamus",
	"Ondine",
	"Platon"
};

#define	BOT_HOOK_DIRS	32

#define BOT_CHECK_TIME (20*60*1000000)

class CBot
{
	class CBotEngine *m_pBotEngine;
	class CPlayer *m_pPlayer;
	class CGameContext *m_pGameServer;

protected:

	class CBotEngine *BotEngine() { return m_pBotEngine; }
	class CGameContext *GameServer() { return BotEngine()->GameServer(); }

	class CCollision *Collision() { return GameServer()->Collision(); }
	class CTuningParams *Tuning() { return GameServer()->Tuning(); }

	int m_TargetClient;

	enum {
		BOT_IDLE=0,
		BOT_FLAG,
		BOT_FIGHT
	};

	int m_State;

	float m_HookLength;
	int m_HookLocked;

	enum {
		BFLAG_LOST	= 0,
		BFLAG_LEFT	= 1,
		BFLAG_RIGHT	= 2,
		BFLAG_JUMP	= 4,
		BFLAG_HOOK	= 8,
		BFLAG_FIRE	= 16
	};

	int m_CurAnchor;

	CEdge m_WalkingEdge;
	int m_WalkStart;

	int m_Flags;
	int m_HookDir;
	vec2 m_HookTarget;

	int m_Weapon;
	int m_AmmoCount[NUM_WEAPONS];
	bool m_Fire;

	int m_Suicide;

	vec2 m_Target;

	int m_aFlagTiles[2];

	int m_EmoticonStart;

	int64 m_LastCheck;

	int m_ClientState;

	CNetObj_PlayerInput m_InputData;
	CNetObj_PlayerInput m_LastData;

	int GetTarget();
	int GetTeam(int ClientID);
	int IsFalling();
	bool IsGrounded();

	void HandleWeapon(const CCharacterCore *pTarget);
	void UpdateEdge(bool Reset);
	void MakeChoice(bool UseTarget);
	void MakeChoice2(bool UseTarget);
	int Predict(class CCharacterCore *pMe, bool Input=false);

	void CheckState();

	int GetTile(int x, int y) { return BotEngine()->GetTile(x,y);}
	int GetTile(int i) { return BotEngine()->GetTile(i);}
	int GetTile(float x, float y) { return GetTile(round(x), round(y)); }

public:
	CBot(class CBotEngine *m_pBotEngine, CPlayer *pPlayer);
	~CBot();

	const char *GetName();
	const char *GetClan() { return g_BotClan; }

	virtual void OnReset();

	CNetObj_PlayerInput GetInputData();
};

#endif
