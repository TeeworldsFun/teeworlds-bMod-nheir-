#include <game/gamecore.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/layers.h>
#include "gamecontext.h"

#include "gamemodes/ctf.h"

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
	mem_zero(&m_InputData, sizeof(m_InputData));

	m_SnapID = GameServer()->Server()->SnapNewID();
}

CBot::~CBot()
{
	m_WalkingEdge.Reset();
	GameServer()->Server()->SnapFreeID(m_SnapID);
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
		if(pController->m_apFlags[Team] && !pController->m_apFlags[Team]->IsAtStand() && pController->m_apFlags[Team]->GetCarrier() && pController->m_apFlags[Team]->GetCarrier()->GetPlayer())
				FlagCarrier = pController->m_apFlags[Team]->GetCarrier()->GetPlayer()->GetCID();
	}
	vec2 Pos = m_pPlayer->GetCharacter()->GetPos();
	if(FlagCarrier < 0)
		if(m_TargetClient >=0 && m_TargetClient < MAX_CLIENTS)
		{
			CPlayer *pClosest = GameServer()->m_apPlayers[m_TargetClient];
			if(pClosest && (pClosest->GetTeam() != Team || IsDM) && pClosest->GetCharacter() && !(Collision()->IntersectLine(Pos, pClosest->GetCharacter()->GetPos(),0,0)))
				return m_TargetClient;
		}

	float Radius = 100000.0f;
	float ClosestRange = 2*Radius;

	m_TargetClient = -1;

	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(c == m_pPlayer->GetCID())
			continue;
		CPlayer *pClosest = GameServer()->m_apPlayers[c];
		if(pClosest && (pClosest->GetTeam() != Team || IsDM) && pClosest->GetCharacter())// && !(Collision()->IntersectLine(Pos, pClosest->GetCharacter()->GetPos(),0,0)))
		{
			float Len = distance(Pos, pClosest->GetCharacter()->GetPos());
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
	vec2 Pos = m_pPlayer->GetCharacter()->GetPos();

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

	UpdateEdge(false);

	if(ClientID < 0)
	{
		m_State = BOT_IDLE;
		m_Fire = false;
		m_InputData.m_WantedWeapon = WEAPON_GUN +1;

		if(GameServer()->m_pController->IsFlagGame())
		{
			CGameControllerCTF *pController = (CGameControllerCTF*)GameServer()->m_pController;
			CFlag **apFlags = pController->m_apFlags;

			if(apFlags[Team])
			{
				vec2 myFlag = apFlags[Team]->GetPos();
				if(!(Collision()->IntersectLine(pMe->m_Pos,myFlag,0,0)) && ( !apFlags[Team]->IsAtStand() || apFlags[Team^1]->GetCarrier() == m_pPlayer->GetCharacter()))
				{
					m_Target = myFlag - pMe->m_Pos;
					m_State = BOT_FLAG;
				}
			}
			if(apFlags[Team^1])
			{
				vec2 thFlag = apFlags[Team^1]->GetPos();

				if(m_State != BOT_FLAG && !(Collision()->IntersectLine(pMe->m_Pos,thFlag,0,0)) && !apFlags[Team^1]->GetCarrier())
				{
					m_Target = thFlag - pMe->m_Pos;
					m_State = BOT_FLAG;
				}
			}
		}
		if(m_State == BOT_FLAG)
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
		else if(m_Flags & BFLAG_FIRE && m_LastData.m_Fire == 0)
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

	bool InSight = !Collision()->IntersectLine(Pos, pClosest->m_Pos,0,0);

	m_RealTarget = pClosest->m_Pos;

	bool UseTarget = ClosestRange < m_HookLength*0.9f && InSight;
	if(GameServer()->m_pController->IsFlagGame()) {
		CGameControllerCTF *pController = (CGameControllerCTF*)GameServer()->m_pController;
		CFlag **apFlags = pController->m_apFlags;
		if(apFlags[Team^1])
			UseTarget = UseTarget || apFlags[Team^1]->GetCarrier() != m_pPlayer->GetCharacter();
	}
	if(UseTarget)
		m_Target = diffPos;
	MakeChoice2(UseTarget);

	HandleWeapon(pClosest);

	if(m_Flags & BFLAG_LEFT)
			m_InputData.m_Direction = -1;
	if(m_Flags & BFLAG_RIGHT)
			m_InputData.m_Direction = 1;
	if(m_Flags & BFLAG_JUMP)
			m_InputData.m_Jump = 1;
	else if(!m_InputData.m_Fire && m_Flags & BFLAG_FIRE && m_LastData.m_Fire == 0)
		m_InputData.m_Fire = 1;

	if(InSight && diffPos.y < - Close && diffVel.y < 0)
		m_InputData.m_Jump = 1;

	if(pMe->m_HookState == HOOK_IDLE && m_InputData.m_Hook)
		m_InputData.m_Hook = 0;
	else if(ClosestRange > m_HookLength*0.9f && !m_InputData.m_Fire)
	{
		if(m_Flags & BFLAG_HOOK)
			m_InputData.m_Hook = 1;
		else
			m_InputData.m_Hook = 0;
	}
	else if(InSight && !m_InputData.m_Fire && !(pMe->m_HookState == HOOK_FLYING  || (pMe->m_HookState == HOOK_GRABBED && pMe->m_HookedPlayer == ClientID)))
	{
		m_Target = diffPos;
		m_InputData.m_Hook ^= 1;
	}

	if(m_InputData.m_Hook || m_InputData.m_Fire) {
		m_InputData.m_TargetX = m_Target.x;
		m_InputData.m_TargetY = m_Target.y;
	}


	if(!g_Config.m_SvBotAllowMove) {
		m_InputData.m_Direction = 0;
		m_InputData.m_Jump = 0;
		m_InputData.m_Hook = 0;
	}
	if(!g_Config.m_SvBotAllowHook)
		m_InputData.m_Hook = 0;

	m_LastData = m_InputData;
	return m_InputData;
}

void CBot::HandleWeapon(const CCharacterCore *pTarget)
{
	CCharacter *pMe = m_pPlayer->GetCharacter();

	if(!pTarget || !pMe)
		return;

	vec2 Pos = pMe->GetCore()->m_Pos;
	vec2 Vel = pMe->GetCore()->m_Vel;
	float ClosestRange = distance(Pos, pTarget->m_Pos);
	float Close = 60.0f;
	vec2 Target = pTarget->m_Pos - Pos;

	int Weapon = -1;

	if(ClosestRange < Close)
	{
		Weapon = WEAPON_HAMMER;
	}
	if(pMe->m_aWeapons[WEAPON_LASER].m_Ammo != 0 && ClosestRange < GameServer()->Tuning()->m_LaserReach && !Collision()->IntersectLine(Pos, pTarget->m_Pos, 0, 0))
  {
    Weapon = WEAPON_LASER;
  }
	else
	{
		int GoodDir = -1;

		vec2 aProjectilePos[BOT_HOOK_DIRS];

		for(int i = 0 ; i < BOT_HOOK_DIRS ; i++) {
			vec2 dir = direction(2*i*pi / BOT_HOOK_DIRS);
			aProjectilePos[i] = Pos + dir*28.*0.75;
		}
		int Weapons[] = {WEAPON_GRENADE, WEAPON_SHOTGUN, WEAPON_GUN};
		for(int j = 0 ; j < 3 ; j++)
		{
			if(!pMe->m_aWeapons[Weapons[j]].m_Ammo)
				continue;
			float Curvature, Speed, DTime;
			switch(Weapons[j])
			{
				case WEAPON_GRENADE:
					Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
					Speed = GameServer()->Tuning()->m_GrenadeSpeed;
					DTime = GameServer()->Tuning()->m_GrenadeLifetime / 10.;
					break;

				case WEAPON_SHOTGUN:
					Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
					Speed = GameServer()->Tuning()->m_ShotgunSpeed;
					DTime = GameServer()->Tuning()->m_ShotgunLifetime / 10.;
					break;

				case WEAPON_GUN:
					Curvature = GameServer()->Tuning()->m_GunCurvature;
					Speed = GameServer()->Tuning()->m_GunSpeed;
					DTime = GameServer()->Tuning()->m_GunLifetime / 10.;
					break;
			}

			int DTick = (int) (DTime*GameServer()->Server()->TickSpeed());
			DTime *= Speed;

			vec2 TargetPos = pTarget->m_Pos;
			vec2 TargetVel = pTarget->m_Vel*DTick;

			int aIsDead[BOT_HOOK_DIRS] = {0};

			for(int k = 0; k < 10 && GoodDir == -1; k++) {
				for(int i = 0; i < BOT_HOOK_DIRS; i++) {
					if(aIsDead[i])
						continue;
					vec2 dir = direction(2*i*pi / BOT_HOOK_DIRS);
					vec2 NextPos = aProjectilePos[i];
					NextPos.x += dir.x*DTime;
					NextPos.y += dir.y*DTime + Curvature/10000*(DTime*DTime)*(2*k+1);
					aIsDead[i] = Collision()->IntersectLine(aProjectilePos[i], NextPos, &NextPos, 0);
					vec2 InterPos = closest_point_on_line(aProjectilePos[i],NextPos, TargetPos);
					if(distance(TargetPos, InterPos)< 28) {
						GoodDir = i;
					}
					aProjectilePos[i] = NextPos;
				}
				Collision()->IntersectLine(TargetPos, TargetPos+TargetVel, 0, &TargetPos);
				TargetVel.y += GameServer()->Tuning()->m_Gravity*DTick*DTick;
			}
			if(GoodDir != -1)
			{
				Target = direction(2*GoodDir*pi / BOT_HOOK_DIRS)*50;
				Weapon = Weapons[j];
				break;
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
	if(m_InputData.m_Fire)
		m_Target = Target;

	// Accuracy
	// float Angle = angle(m_Target) + (rand()%64-32)*pi / 1024.0f;
	// m_Target = direction(Angle)*length(m_Target);
}

void CBot::UpdateEdge(bool Reset)
{
	vec2 Pos = m_pPlayer->GetCharacter()->GetPos();
	int x = round(Pos.x/32);
	int y = round(Pos.y/32);

	int Width = BotEngine()->GetWidth();

	int NewStart = -1;
	int ClosestRange = BotEngine()->DistanceToEdge(m_WalkingEdge,Pos);
	if(!Reset)
	{
		if(ClosestRange < 0 || ClosestRange > 2000)
		{
			Reset = true;
		}
		else {
			ClosestRange = distance(Pos, BotEngine()->GetGraph()->ConvertIndex(m_WalkingEdge.m_End));
			if(ClosestRange < 10. || ((BotEngine()->GetTile(m_WalkingEdge.m_End) & GTILE_MASK) >= GTILE_AIR && ClosestRange < 100))
			NewStart = m_WalkingEdge.m_End;
		}
	}
	if(!Reset && m_WalkStart + 60 * SERVER_TICK_SPEED < GameServer()->Server()->Tick())
		Reset = true;
	if(NewStart > -1)
	{
		m_WalkStart = GameServer()->Server()->Tick();
		int NewEnd = -1;
		if(GameServer()->m_pController->IsFlagGame()) {
			int Team = m_pPlayer->GetTeam();
			CGameControllerCTF *pController = (CGameControllerCTF*)GameServer()->m_pController;
			CFlag **apFlags = pController->m_apFlags;
			if(apFlags[Team^1])
			{
				if(apFlags[Team^1]->IsAtStand())
					NewEnd = BotEngine()->GetFlagStandPos(Team^1);
				if(apFlags[Team^1]->GetCarrier() == m_pPlayer->GetCharacter())
					NewEnd = BotEngine()->GetFlagStandPos(Team);
			}
		}

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
	else if(Reset || !m_WalkingEdge.m_Size)
	{
		m_WalkingEdge.Reset();
		int d = BotEngine()->GetClosestEdge(Pos, 1000, &m_WalkingEdge);
		if(d < 0)
			dbg_msg("bot", "closest edge failed with %d", m_pPlayer->GetCID());
	}
}

void CBot::MakeChoice2(bool UseTarget)
{
	if(UseTarget)
	{
		MakeChoice(UseTarget);
		return;
	}
	vec2 Pos = m_pPlayer->GetCharacter()->GetPos();

	if(m_WalkingEdge.m_Size)
	{
		int dist = BotEngine()->FarestPointOnEdge(m_WalkingEdge, Pos, &m_Target);
		if(dist >= 0)
		{
			UseTarget = true;
			m_RealTarget = (m_pPlayer->GetCID()%2) ? m_Target : BotEngine()->GetGraph()->ConvertIndex(m_WalkingEdge.m_End);
			m_Target -= Pos;
		}
		else
		{
			UpdateEdge(true);
			dbg_msg("bot", "force edge update");
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

	int NextTile = GetTile(TempChar.m_Pos.x, TempChar.m_Pos.y);
	vec2 NextPos = TempChar.m_Pos;

	if(TempChar.m_Input.m_Direction > 0)
		Flags |= BFLAG_RIGHT;

	if(TempChar.m_Input.m_Direction < 0)
		Flags |= BFLAG_LEFT;

	if(CurTile & BTILE_SAFE && NextTile & BTILE_HOLE && Grounded)
	{
		if(!UseTarget || m_Target.y < 0)
			Flags |= BFLAG_JUMP;
	}
	if(CurTile & BTILE_SAFE && NextTile & BTILE_SAFE)
	{
		static bool tried = false;
		if(absolute(CurPos.x - NextPos.x) < 1.0f && TempChar.m_Input.m_Direction)
		{
			if(Grounded)
			{
				Flags |= BFLAG_JUMP;
				tried = true;
			}
			else if(tried && !(TempChar.m_Jumped) && TempChar.m_Vel.y > 0)
				Flags |= BFLAG_JUMP;
			else if(tried && TempChar.m_Jumped & 2 && TempChar.m_Vel.y > 0)
				Flags ^= BFLAG_RIGHT | BFLAG_LEFT;
		}
		else
			tried = false;
		// if(UseTarget && m_Target.y < 0 && TempChar.m_Vel.y > 1.f && !(TempChar.m_Jumped) && !Grounded)
		// 	Flags |= BFLAG_JUMP;
	}
	if(pMe->m_HookState == HOOK_GRABBED && pMe->m_HookedPlayer == -1)
	{
		vec2 HookVel = normalize(pMe->m_HookPos-pMe->m_Pos)*TempWorld.m_Tuning.m_HookDragAccel;

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
		if(ps > 0 || (CurTile & BTILE_HOLE && (!UseTarget || m_Target.y < 0) && pMe->m_Vel.y > 0.f && pMe->m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED/2))
			Flags |= BFLAG_HOOK;
		if(pMe->m_HookTick > 4*SERVER_TICK_SPEED || length(pMe->m_HookPos-pMe->m_Pos) < 20.0f)
			Flags &= ~BFLAG_HOOK;
		// if(Flags & BFLAG_HOOK && ps < dot(Target,HookVel-Accel))
		// 	Flags ^= BFLAG_RIGHT | BFLAG_LEFT;
	}
	if(pMe->m_HookState == HOOK_FLYING)
		Flags |= BFLAG_HOOK;
	// do random hook
	if(!m_Fire && m_InputData.m_Hook == 0 && pMe->m_HookState == HOOK_IDLE && (rand()%10 == 0 || (CurTile & BTILE_HOLE && rand()%4 == 0)))
	{
		int NumDir = BOT_HOOK_DIRS;
		vec2 HookDir(0.0f,0.0f);
		float MaxForce = (CurTile & BTILE_HOLE) ? -10000.0f : 0;
		vec2 Target = (UseTarget) ? m_Target : pMe->m_Vel;
		for(int i = 0 ; i < NumDir; i++)
		{
			float a = 2*i*pi / NumDir;
			vec2 dir = direction(a);
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
			if(Collision()->CheckPoint(pMe->m_Pos+normalize(vec2(0,m_Target.y))*28) && absolute(Target.x) < 30)
				Flags = (Flags & (~BFLAG_LEFT)) | BFLAG_RIGHT;
		}
	}
	else if(!(pMe->m_Jumped))
	{
		vec2 Vel(pMe->m_Vel.x, min(pMe->m_Vel.y, 0.0f));
		if(Collision()->IntersectLine(pMe->m_Pos,pMe->m_Pos+Vel*10.0f,0,0) && !Collision()->IntersectLine(pMe->m_Pos,pMe->m_Pos+(Vel-vec2(0,TempWorld.m_Tuning.m_AirJumpImpulse))*10.0f,0,0) && (!UseTarget || m_Target.y < 0))
			Flags |= BFLAG_JUMP;
		if(UseTarget && m_Target.y < 0 && absolute(m_Target.x) < 28.f && pMe->m_Vel.y > -1.f)
			Flags |= BFLAG_JUMP;
	}
	if(Flags & BFLAG_JUMP || pMe->m_Vel.y < 0)
		m_InputData.m_WantedWeapon = WEAPON_GRENADE +1;
	if(m_Target.y < -400 && pMe->m_Vel.y < 0 && absolute(m_Target.x) < 30 && Collision()->CheckPoint(pMe->m_Pos+vec2(0,50)))
	{
		Flags &= ~BFLAG_HOOK;
		Flags |= BFLAG_FIRE;
		m_Target = vec2(0,28);
	}
	else if(m_Target.y < -300 && pMe->m_Vel.y < 0 && absolute(m_Target.x) < 30 && Collision()->CheckPoint(pMe->m_Pos+vec2(32,48)))
	{
		Flags &= ~BFLAG_HOOK;
		Flags |= BFLAG_FIRE;
		m_Target = vec2(14,28);
	}
	else if(m_Target.y < -300 && pMe->m_Vel.y < 0 && absolute(m_Target.x) < 30 && Collision()->CheckPoint(pMe->m_Pos+vec2(-32,48)))
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

void CBot::Snap(int SnappingClient)
{
	if(SnappingClient == -1)
		return;

	CCharacter *pMe = m_pPlayer->GetCharacter();
	if(!pMe)
		return;

	vec2 Pos = pMe->GetCore()->m_Pos;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(GameServer()->Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)(m_RealTarget.x);
	pObj->m_Y = (int)(m_RealTarget.y);
	pObj->m_FromX = (int)Pos.x;
	pObj->m_FromY = (int)Pos.y;
	pObj->m_StartTick = GameServer()->Server()->Tick();
}

const char *CBot::GetName() {
	return g_BotName[m_pPlayer->GetCID()];
}
