#include <game/generated/protocol.h>
#include <game/gamecore.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/layers.h>
#include "gamecontext.h"

#include "gamemodes/ctf.h"

#include "botengine.h"

#include "bot.h"
#include "player.h"
#include "entities/character.h"
#include "entities/flag.h"

CBot::CBot(CBotEngine *pBotEngine, CPlayer *pPlayer)
{
	m_pBotEngine = pBotEngine;
	m_pPlayer = pPlayer;
	m_pGameServer = pBotEngine->GameServer();
	m_State = BOT_IDLE;
	m_HookLength = Tuning()->m_HookLength*0.9f;
	m_Flags = 0;
	m_LastCheck = 0;
	mem_zero(m_AmmoCount, sizeof(m_AmmoCount));
	m_AmmoCount[WEAPON_HAMMER] = -1;
	m_Weapon = WEAPON_GUN;
	m_InputData = {0};
}

CBot::~CBot()
{
	m_WalkingEdge.Reset();
}

void CBot::OnReset()
{
	mem_zero(m_AmmoCount, sizeof(m_AmmoCount));
	m_AmmoCount[WEAPON_HAMMER] = -1;
	m_Weapon = WEAPON_GUN;
	m_Flags = 0;
	m_WalkingEdge.Reset();
}

int CBot::GetTarget()
{
	int Team = m_pPlayer->GetTeam();
	int FlagCarrier = -1;
	bool IsDM = !GameServer()->m_pController->IsTeamplay();
	if(Team >= 0 && (GameServer()->m_pController->IsFlagGame()))
	{
		CGameControllerCTF *pController = (CGameControllerCTF*)GameServer()->m_pController;
		if(pController->m_apFlags[Team] && !pController->m_apFlags[Team]->m_AtStand && pController->m_apFlags[Team]->m_pCarryingCharacter && pController->m_apFlags[Team]->m_pCarryingCharacter->GetPlayer())
				FlagCarrier = pController->m_apFlags[Team]->m_pCarryingCharacter->GetPlayer()->GetCID();
	}
	vec2 Pos = m_pPlayer->GetCharacter()->m_Pos;
	if(FlagCarrier < 0)
		if(m_TargetClient >=0 && m_TargetClient < MAX_CLIENTS)
		{
			CPlayer *pClosest = GameServer()->m_apPlayers[m_TargetClient];
			if(pClosest && (pClosest->GetTeam() != Team || IsDM) && pClosest->GetCharacter() && !(Collision()->IntersectLine(Pos, pClosest->GetCharacter()->m_Pos,0,0)))
				return m_TargetClient;
		}

	float Radius = 1000.0f;
	float ClosestRange = 2*Radius;

	m_TargetClient = -1;

	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(c == m_pPlayer->GetCID())
			continue;
		CPlayer *pClosest = GameServer()->m_apPlayers[c];
		if(pClosest && (pClosest->GetTeam() != Team || IsDM) && pClosest->GetCharacter() && !(Collision()->IntersectLine(Pos, pClosest->GetCharacter()->m_Pos,0,0)))
		{
			float Len = distance(Pos, pClosest->GetCharacter()->m_Pos);
			if(Len < Radius)
			{
				if(c == FlagCarrier && Len < m_HookLength)
				{
					m_TargetClient = c;
					return m_TargetClient;
				}
				if(Len < ClosestRange)
				{
					ClosestRange = Len;
					m_TargetClient = c;
				}
			}
		}
	}
	return m_TargetClient;
}

bool CBot::IsGrounded()
{
	vec2 Pos = m_pPlayer->GetCharacter()->m_Pos;

	float PhysSize = 28.0f;

	// get ground state
	bool Grounded = false;
	if(Collision()->CheckPoint(Pos.x+PhysSize/2, Pos.y+PhysSize/2+5))
		Grounded = true;
	if(Collision()->CheckPoint(Pos.x-PhysSize/2, Pos.y+PhysSize/2+5))
		Grounded = true;
	return Grounded;
}

CNetObj_PlayerInput CBot::GetInputData()
{
	CheckState();
	if(!m_pPlayer->GetCharacter())
		return m_InputData;
	const CCharacterCore *pMe = m_pPlayer->GetCharacter()->GetCore();
	int Team = m_pPlayer->GetTeam();

	int ClientID = GetTarget();

	m_InputData.m_Jump = 0;

	if(ClientID < 0)
	{
		UpdateEdge(false);

		m_State = BOT_IDLE;
		m_Fire = false;
		m_InputData.m_WantedWeapon = WEAPON_GUN +1;

		// if(m_pClient->m_Snap.m_pGameDataObj)
		// {
		// 	int myFlagOwner = (Team == TEAM_BLUE) ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
		// 	int thFlagOwner = (Team == TEAM_RED) ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
		//
		// 	if(m_pClient->m_Snap.m_paFlags[Team])
		// 	{
		// 		vec2 myFlag(m_pClient->m_Snap.m_paFlags[Team]->m_X, m_pClient->m_Snap.m_paFlags[Team]->m_Y);
		// 		if(!(Collision()->IntersectLine(pMe->m_Pos,myFlag,0,0)) and (myFlagOwner == FLAG_TAKEN or thFlagOwner == m_pClient->m_Snap.m_LocalClientID))
		// 		{
		// 			m_Target = myFlag - pMe->m_Pos;
		// 			m_State = BOT_FLAG;
		// 		}
		// 	}
		// 	if(m_pClient->m_Snap.m_paFlags[Team^1])
		// 	{
		// 		vec2 thFlag(m_pClient->m_Snap.m_paFlags[Team^1]->m_X, m_pClient->m_Snap.m_paFlags[Team^1]->m_Y);
		//
		// 		if(m_State != BOT_FLAG and !(Collision()->IntersectLine(pMe->m_Pos,thFlag,0,0)) and thFlagOwner < 0)
		// 		{
		// 			m_Target = thFlag - pMe->m_Pos;
		// 			m_State = BOT_FLAG;
		// 		}
		// 	}
		// }
		if(m_State == BOT_FLAG and rand()%2)
			MakeChoice2(true);
		else
			MakeChoice2(false);
		m_InputData.m_Hook = 0;
		m_InputData.m_Fire = 0;
		if(m_Flags & BFLAG_JUMP)
			m_InputData.m_Jump = 1;
		if(m_Flags & BFLAG_LEFT)
			m_InputData.m_Direction = -1;
		if(m_Flags & BFLAG_RIGHT)
			m_InputData.m_Direction = 1;
		if(m_Flags & BFLAG_HOOK)
		{
			m_InputData.m_Hook = 1;
			m_InputData.m_TargetX = m_Target.x;
			m_InputData.m_TargetY = m_Target.y;
		}
		else if(m_Flags & BFLAG_FIRE and m_LastData.m_Fire == 0)
		{
			m_InputData.m_Fire = 1;
			m_InputData.m_TargetX = m_Target.x;
			m_InputData.m_TargetY = m_Target.y;
		}
		m_LastData = m_InputData;
		return m_InputData;
	}

	m_State = BOT_FIGHT;
	const CCharacterCore *pClosest = GameServer()->m_apPlayers[ClientID]->GetCharacter()->GetCore();

	vec2 Pos = pMe->m_Pos;
	float ClosestRange = distance(Pos, pClosest->m_Pos);
	float Close = 65.0f;
	vec2 diffPos = pClosest->m_Pos - Pos;
	vec2 diffVel = pClosest->m_Vel - pMe->m_Vel;

	HandleWeapon(pClosest);
	diffPos = m_Target;

	// int thFlagOwner = -1;
	// if(m_pClient->m_Snap.m_pGameDataObj)
	// 	thFlagOwner = (Team == TEAM_RED) ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
	// bool UseTarget = thFlagOwner != m_pClient->m_Snap.m_LocalClientID or ClosestRange < m_HookLength*0.9f;
	MakeChoice2(true);

	if(m_Flags & BFLAG_LEFT)
			m_InputData.m_Direction = -1;
	if(m_Flags & BFLAG_RIGHT)
			m_InputData.m_Direction = 1;

	if(diffPos.y < - Close and diffVel.y < 0)
		m_InputData.m_Jump = 1;

	if(pMe->m_HookState == HOOK_IDLE and m_InputData.m_Hook)
		m_InputData.m_Hook = 0;
	else if(ClosestRange > m_HookLength*0.9f and !m_InputData.m_Fire)
	{
		if(m_Flags & BFLAG_HOOK)
			m_InputData.m_Hook = 1;
		else
			m_InputData.m_Hook = 0;
	}
	else if(!m_InputData.m_Fire and !(pMe->m_HookState == HOOK_FLYING  or (pMe->m_HookState == HOOK_GRABBED and pMe->m_HookedPlayer == ClientID)))
	{
		m_Target = diffPos;
		m_InputData.m_Hook ^= 1;
	}

	m_Target = (true) ? m_Target : diffPos;
	m_InputData.m_TargetX = m_Target.x;
	m_InputData.m_TargetY = m_Target.y;

	m_LastData = m_InputData;
	return m_InputData;
}

void CBot::HandleWeapon(const CCharacterCore *pTarget)
{
	CCharacter *pMe = m_pPlayer->GetCharacter();

	if(!pTarget || !pMe)
		return;

	vec2 Pos = pMe->m_Pos;
	vec2 Vel = pMe->GetCore()->m_Vel;
	float ClosestRange = distance(Pos+Vel, pTarget->m_Pos+pTarget->m_Vel);
	float Close = 65.0f;
	vec2 diffPos = pTarget->m_Pos - Pos;
	m_Target = diffPos;

	int Weapon = -1;

	if(ClosestRange < Close)
	{
		Weapon = WEAPON_HAMMER;
	}
	else
	{
		CTuningParams *T = GameServer()->Tuning();
		if(pMe->m_aWeapons[WEAPON_RIFLE].m_Ammo != 0 and ClosestRange < T->m_LaserReach)
		{
			Weapon = WEAPON_RIFLE;
		}
		else if(pMe->m_aWeapons[WEAPON_SHOTGUN].m_Ammo != 0 and ClosestRange < T->m_ShotgunLifetime*T->m_ShotgunSpeed)
		{
			Weapon = WEAPON_SHOTGUN;
		}
		else
		{
			float Curvature = 0;
			float Speed = 0;
			int Weapons[] = {WEAPON_GUN, WEAPON_GRENADE};
			for(int i = 0; i < 2; i++)
			{
				if(pMe->m_aWeapons[Weapons[i]].m_Ammo == 0)
					continue;
				switch(Weapons[i])
				{
				case WEAPON_GRENADE:
					Curvature = T->m_GrenadeCurvature;
					Speed = (T->m_GrenadeSpeed) ? T->m_GrenadeSpeed : 1;
					break;
				case WEAPON_GUN:
				default:
					Curvature = T->m_GunCurvature;
					Speed = (T->m_GunSpeed) ? T->m_GunSpeed : 1;
					break;
				}
				diffPos += pTarget->m_Vel*(SERVER_TICK_SPEED/Speed);
				float a = dot(diffPos, diffPos);
				float b = - diffPos.x * diffPos.x * (diffPos.y * 2 * Curvature/10000 + 1);
				float c = diffPos.x;
				c *= c;
				c *= c;
				c *= (Curvature/10000) * (Curvature/10000);
				float delta = b*b-4*a*c;
				if(delta >= 0)
				{
					float x1 = (-b + sqrt(delta)) / (2*a);
					float x2 = (-b - sqrt(delta)) / (2*a);
					float xs = -2;
					if(0 <= x1 and x1 <= 1)
						xs = x1;
					if(0 <= x2 and x2 <= 1 and xs < x2)
						xs = x2;
					if(xs > -1)
					{
						Weapon = Weapons[i];
						float dx = sqrt(xs);
						float dy = sqrt(1-xs);
						if(diffPos.x < 0)
							dx *= -1.0f;
						if(xs*diffPos.y-diffPos.x*diffPos.x * (Curvature/10000) < 0)
							dy *= -1.0f;
						m_Target = vec2(dx,dy);
						m_Target *= 50.0f;
					}
				}
			}
		}
	}
	if(m_InputData.m_WantedWeapon != Weapon+1)
	{
		m_InputData.m_WantedWeapon = (Weapon < 0) ? WEAPON_GUN+1 : Weapon+1;
		m_InputData.m_Fire = 0;
	}
	else
		m_InputData.m_Fire = (m_LastData.m_Fire) ? 0 : 1;

	// Accuracy
	float Angle = GetAngle(m_Target) + (rand()%64-32)*pi / 1024.0f;
	m_Target = GetDir(Angle)*length(m_Target);
}

void CBot::UpdateEdge(bool Reset)
{
	vec2 Pos = m_pPlayer->GetCharacter()->m_Pos;
	int x = round(Pos.x/32);
	int y = round(Pos.y/32);

	int Width = BotEngine()->GetWidth();

	int NewStart = -1;
	if(m_WalkingEdge.m_Size and !Reset)
	{
		//int ClosestVertex = m_WalkingEdge.m_End;
		int Dx = x - (m_WalkingEdge.m_End % Width);
		int Dy = y - (m_WalkingEdge.m_End / Width);
		int D = Dx*Dx+Dy*Dy;
		if(D > 10 or ((GetTile(m_WalkingEdge.m_End) & GTILE_MASK) < GTILE_AIR and D > 0))
		{
			int ClosestRange = 200;
			for(int i = 0 ; i < m_WalkingEdge.m_Size ; i++)
			{
				Dx = x - (m_WalkingEdge.m_pPath[i] % Width);
				Dy = y - (m_WalkingEdge.m_pPath[i] / Width);
				D = Dx*Dx+Dy*Dy;
				if( D < ClosestRange)
				{
					vec2 VertexPos(m_WalkingEdge.m_pPath[i] % Width, m_WalkingEdge.m_pPath[i] / Width);
					VertexPos *= 32;
					VertexPos += vec2(16,16);
					vec2 W = GetDir(GetAngle(normalize(VertexPos-Pos))+pi/2)*15.f;
					if(!(Collision()->IntersectLine(Pos-W,VertexPos-W,0,0)) and !(Collision()->IntersectLine(Pos+W,VertexPos+W,0,0)))
					{
						ClosestRange = D;
						//ClosestVertex = m_WalkingEdge.m_pPath[i];
					}
				}
			}
			if(ClosestRange == 200)
			{
				Reset = true;
			}
		}
		else //if(ClosestVertex == m_WalkingEdge.m_End)
		{
			NewStart = m_WalkingEdge.m_End;
		}
	}
	if(!Reset and m_WalkStart + 60 * SERVER_TICK_SPEED < GameServer()->Server()->Tick())
		Reset = true;
	if(Reset or !m_WalkingEdge.m_Size)
	{
		m_WalkingEdge.Reset();
		CEdge* ClosestEdge = BotEngine()->GetClosestEdge(Pos, 100);
		if(ClosestEdge)
		{
			m_WalkStart = GameServer()->Server()->Tick();
			m_WalkingEdge = *ClosestEdge;
			m_WalkingEdge.m_pPath = (int*) mem_alloc(m_WalkingEdge.m_Size*sizeof(int),1);
			mem_copy(m_WalkingEdge.m_pPath, ClosestEdge->m_pPath, m_WalkingEdge.m_Size*sizeof(int));
		}
	}
	if(NewStart > -1)
	{
		m_WalkStart = GameServer()->Server()->Tick();
		int Team = m_pPlayer->GetTeam();
		//int myFlagOwner = (Team == TEAM_BLUE) ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
		int thFlagOwner = -1;
		// if(m_pClient->m_Snap.m_pGameDataObj)
		// 	thFlagOwner = (Team == TEAM_RED) ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
		int NewEnd = -1;
		// if(thFlagOwner == FLAG_ATSTAND)
		// 	NewEnd = m_aFlagTiles[Team^1];
		// if(thFlagOwner == m_pClient->m_Snap.m_LocalClientID)
		// 	NewEnd = m_aFlagTiles[Team];
		if(NewEnd < 0)
		{
			int r = rand()%(BotEngine()->GetGraph()->m_NumVertices-1);
			if(BotEngine()->GetGraph()->m_pVertices[r].m_Index == NewStart)
				r++;
			NewEnd = BotEngine()->GetGraph()->m_pVertices[r].m_Index;
		}
		m_WalkingEdge.Reset();
		m_WalkingEdge = BotEngine()->GetGraph()->GetPath(NewStart, NewEnd);
	}
}

void CBot::MakeChoice2(bool UseTarget)
{
	if(UseTarget)
	{
		MakeChoice(UseTarget);
		return;
	}
	int Width = BotEngine()->GetWidth();
	vec2 Pos = m_pPlayer->GetCharacter()->m_Pos;

	if(m_WalkingEdge.m_Size)
	{
		int x = round(Pos.x/32);
		int y = round(Pos.y/32);
		int ClosestRange = 1000;
		for(int k = m_WalkingEdge.m_Size-1 ; k >=0 ; k--)
		{
			int Index = m_WalkingEdge.m_pPath[k];
			int i = (Index%Width)-x;
			int j = (Index/Width)-y;
			float D = i*i+j*j;
			if( D < ClosestRange)
			{
				vec2 VertexPos(m_WalkingEdge.m_pPath[k] % Width, m_WalkingEdge.m_pPath[k] / Width);
				VertexPos *= 32;
				VertexPos += vec2(16,16);
				vec2 W = GetDir(GetAngle(normalize(VertexPos-Pos))+pi/2)*15.f;
				if(!(Collision()->IntersectLine(Pos-W,VertexPos-W,0,0)) and !(Collision()->IntersectLine(Pos+W,VertexPos+W,0,0)))
				{
					ClosestRange = D;
					UseTarget = true;
					m_Target = VertexPos-Pos;
					break;
				}
			}
		}
	}
	MakeChoice(UseTarget);
}

void CBot::MakeChoice(bool UseTarget)
{
	int Flags = 0;
	CCharacterCore *pMe = m_pPlayer->GetCharacter()->GetCore();
	CCharacterCore TempChar = *pMe;
	TempChar.m_Input = m_InputData;
	vec2 CurPos = TempChar.m_Pos;

	int CurTile = GetTile(TempChar.m_Pos.x, TempChar.m_Pos.y);
	bool Grounded = IsGrounded();

	if(TempChar.m_Input.m_Direction == 0)
		TempChar.m_Input.m_Direction = (rand()%2)*2-1;
	if(UseTarget)
		TempChar.m_Input.m_Direction = (m_Target.x > 28.f) ? 1 : (m_Target.x < -28.f) ? -1:0;
	CWorldCore TempWorld;
	TempWorld.m_Tuning = *GameServer()->Tuning();
	TempChar.Init(&TempWorld, Collision());
	TempChar.Tick(true);
	TempChar.Move();
	TempChar.Quantize();

	vec2 Target = m_Target;

	int NextTile = GetTile(TempChar.m_Pos.x, TempChar.m_Pos.y);
	vec2 NextPos = TempChar.m_Pos;

	if(TempChar.m_Input.m_Direction > 0)
		Flags |= BFLAG_RIGHT;

	if(TempChar.m_Input.m_Direction < 0)
		Flags |= BFLAG_LEFT;

	if(CurTile & BTILE_SAFE and NextTile & BTILE_HOLE and Grounded)
	{
		if(!UseTarget or m_Target.y < 0)
			Flags |= BFLAG_JUMP;
	}
	if(CurTile & BTILE_SAFE and NextTile & BTILE_SAFE)
	{
		static bool tried = false;
		if(absolute(CurPos.x - NextPos.x) < 1.0f and TempChar.m_Input.m_Direction)
		{
			if(Grounded)
			{
				Flags |= BFLAG_JUMP;
				tried = true;
			}
			else if(tried and !(TempChar.m_Jumped) and TempChar.m_Vel.y > 0)
				Flags |= BFLAG_JUMP;
			else if(tried and TempChar.m_Jumped & 2 and TempChar.m_Vel.y > 0)
				Flags ^= BFLAG_RIGHT | BFLAG_LEFT;
		}
		else
			tried = false;
		// if(UseTarget and m_Target.y < 0 and TempChar.m_Vel.y > 1.f and !(TempChar.m_Jumped) and !Grounded)
		// 	Flags |= BFLAG_JUMP;
	}
	if(pMe->m_HookState == HOOK_GRABBED and pMe->m_HookedPlayer == -1)
	{
		vec2 HookVel = normalize(pMe->m_HookPos-pMe->m_Pos)*TempWorld.m_Tuning.m_HookDragAccel;
		float Acc = Grounded ? TempWorld.m_Tuning.m_GroundControlAccel : TempWorld.m_Tuning.m_AirControlAccel;
		vec2 Accel = vec2((Flags & BFLAG_RIGHT) ? Acc : -Acc,0);

		// from gamecore;cpp
		if(HookVel.y > 0)
			HookVel.y *= 0.3f;
		if((HookVel.x < 0 && pMe->m_Input.m_Direction < 0) || (HookVel.x > 0 && pMe->m_Input.m_Direction > 0))
			HookVel.x *= 0.95f;
		else
			HookVel.x *= 0.75f;

		HookVel += vec2(0,1)*TempWorld.m_Tuning.m_Gravity;

		vec2 Target = (UseTarget) ? m_Target : pMe->m_Vel;
		float ps = dot(Target, HookVel);
		if(ps > 0 or (CurTile & BTILE_HOLE and (!UseTarget or m_Target.y < 0) and pMe->m_Vel.y > 0.f and pMe->m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED/2))
			Flags |= BFLAG_HOOK;
		if(pMe->m_HookTick > 4*SERVER_TICK_SPEED or length(pMe->m_HookPos-pMe->m_Pos) < 20.0f)
			Flags &= ~BFLAG_HOOK;
		// if(Flags & BFLAG_HOOK and ps < dot(Target,HookVel-Accel))
		// 	Flags ^= BFLAG_RIGHT | BFLAG_LEFT;
	}
	if(pMe->m_HookState == HOOK_FLYING)
		Flags |= BFLAG_HOOK;
	// do random hook
	if(!m_Fire and m_InputData.m_Hook == 0 and pMe->m_HookState == HOOK_IDLE and (rand()%10 == 0 or (CurTile & BTILE_HOLE and rand()%4 == 0)))
	{
		int NumDir = BOT_HOOK_DIRS;
		vec2 HookDir(0.0f,0.0f);
		float MaxForce = (CurTile & BTILE_HOLE) ? -10000.0f : 0;
		vec2 Target = (UseTarget) ? m_Target : pMe->m_Vel;
		float Acc = Grounded ? TempWorld.m_Tuning.m_GroundControlAccel : TempWorld.m_Tuning.m_AirControlAccel;
		vec2 Accel = vec2((Flags & BFLAG_RIGHT) ? Acc : -Acc,0);
		for(int i = 0 ; i < NumDir; i++)
		{
			float a = 2*i*pi / NumDir;
			vec2 dir = GetDir(a);
			vec2 Pos = pMe->m_Pos+dir*m_HookLength;

			if((Collision()->IntersectLine(pMe->m_Pos,Pos,&Pos,0) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
			{
				vec2 HookVel = dir*TempWorld.m_Tuning.m_HookDragAccel;

				// from gamecore;cpp
				if(HookVel.y > 0)
					HookVel.y *= 0.3f;
				if((HookVel.x < 0 && pMe->m_Input.m_Direction < 0) || (HookVel.x > 0 && pMe->m_Input.m_Direction > 0))
					HookVel.x *= 0.95f;
				else
					HookVel.x *= 0.75f;

				HookVel += vec2(0,1)*TempWorld.m_Tuning.m_Gravity;

				float ps = dot(Target, HookVel);
				if( ps > MaxForce)
				{
					MaxForce = ps;
					HookDir = dir;
				}
			}
		}
		if(length(HookDir) > 0.5f)
		{
			// vec2 HookVel = HookDir*TempWorld.m_Tuning.m_HookDragAccel;

			// if(HookVel.y > 0)
			// 	HookVel.y *= 0.3f;
			// if((HookVel.x < 0 && pMe->m_Input.m_Direction < 0) || (HookVel.x > 0 && pMe->m_Input.m_Direction > 0))
			// 	HookVel.x *= 0.95f;
			// else
			// 	HookVel.x *= 0.75f;

			// HookVel += vec2(0,1)*TempWorld.m_Tuning.m_Gravity;

			// if(MaxForce < dot(Target,HookVel-Accel))
			// 	Flags ^= BFLAG_RIGHT | BFLAG_LEFT;

			m_Target = HookDir * 50.0f;
			Flags |= BFLAG_HOOK;
			if(Collision()->CheckPoint(pMe->m_Pos+normalize(vec2(0,m_Target.y))*28) and absolute(Target.x) < 30)
				Flags = (Flags & (~BFLAG_LEFT)) | BFLAG_RIGHT;
		}
	}
	else if(!(pMe->m_Jumped))
	{
		vec2 Vel(pMe->m_Vel.x, min(pMe->m_Vel.y, 0.0f));
		if(Collision()->IntersectLine(pMe->m_Pos,pMe->m_Pos+Vel*10.0f,0,0) and !Collision()->IntersectLine(pMe->m_Pos,pMe->m_Pos+(Vel-vec2(0,TempWorld.m_Tuning.m_AirJumpImpulse))*10.0f,0,0) and (!UseTarget or m_Target.y < 0))
			Flags |= BFLAG_JUMP;
		if(UseTarget and m_Target.y < 0 and absolute(m_Target.x) < 28.f and pMe->m_Vel.y > -1.f)
			Flags |= BFLAG_JUMP;
	}
	if(Flags & BFLAG_JUMP or pMe->m_Vel.y < 0)
		m_InputData.m_WantedWeapon = WEAPON_GRENADE +1;
	if(m_Target.y < -400 and pMe->m_Vel.y < 0 and absolute(m_Target.x) < 30 and Collision()->CheckPoint(pMe->m_Pos+vec2(0,50)))
	{
		Flags &= ~BFLAG_HOOK;
		Flags |= BFLAG_FIRE;
		m_Target = vec2(0,28);
	}
	else if(m_Target.y < -300 and pMe->m_Vel.y < 0 and absolute(m_Target.x) < 30 and Collision()->CheckPoint(pMe->m_Pos+vec2(32,48)))
	{
		Flags &= ~BFLAG_HOOK;
		Flags |= BFLAG_FIRE;
		m_Target = vec2(14,28);
	}
	else if(m_Target.y < -300 and pMe->m_Vel.y < 0 and absolute(m_Target.x) < 30 and Collision()->CheckPoint(pMe->m_Pos+vec2(-32,48)))
	{
		Flags &= ~BFLAG_HOOK;
		Flags |= BFLAG_FIRE;
		m_Target = vec2(-14,28);
	}
	m_Flags = Flags;
}

void CBot::CheckState()
{
	if(time_get() - m_LastCheck > BOT_CHECK_TIME)
	{
		//Say(0, "I am 100% computer controlled.");
	}
}

const char *CBot::GetName() {
	return g_BotName[m_pPlayer->GetCID()];
}
