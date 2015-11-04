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

	CBotEngine::CPath *m_pPath;

	int m_SnapID;

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
	vec2 m_RealTarget;
	struct CTarget {
		vec2 m_Pos;
		enum {
			TARGET_EMPTY=-1,
			TARGET_PLAYER=0,
			TARGET_FLAG,
			TARGET_ARMOR,
			TARGET_HEALTH,
			TARGET_WEAPON_SHOTGUN,
			TARGET_WEAPON_GRENADE,
			TARGET_POWERUP_NINJA,
			TARGET_WEAPON_RIFLE,
			TARGET_AIR
		} m_Type;
		int m_PlayerCID;
		bool m_NeedUpdate;
	} m_ComputeTarget;

	int m_aFlagTiles[2];

	int m_EmoticonStart;

	int64 m_LastCheck;

	int m_ClientState;

	CNetObj_PlayerInput m_InputData;
	CNetObj_PlayerInput m_LastData;

	int GetTarget();
	void UpdateTarget();
	int GetTeam(int ClientID);
	int IsFalling();
	bool IsGrounded();

	void HandleWeapon(bool SeeTarget);
	void HandleHook(bool SeeTarget);
	void UpdateEdge();
	void MakeChoice();
	void MakeChoice2(bool UseTarget);
	int Predict(class CCharacterCore *pMe, bool Input=false);

	void CheckState();

	int GetTile(int x, int y) { return BotEngine()->GetTile(x/32,y/32);}

public:
	CBot(class CBotEngine *m_pBotEngine, CPlayer *pPlayer);
	~CBot();

	const char *GetName();
	const char *GetClan() { return g_BotClan; }

	int GetID() { return m_SnapID; }
	void Snap(int SnappingClient);

	virtual void OnReset();

	CNetObj_PlayerInput GetInputData();
	CNetObj_PlayerInput GetLastInputData() { return m_LastData; }
};

#endif
