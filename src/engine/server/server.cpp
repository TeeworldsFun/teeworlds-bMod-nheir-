/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>

#include <mastersrv/mastersrv.h>

#include "register.h"
#include "server.h"

#if defined(CONF_FAMILY_WINDOWS)
	#define _WIN32_WINNT 0x0501
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

static const char *StrLtrim(const char *pStr)
{
	while(*pStr && *pStr >= 0 && *pStr <= 32)
		pStr++;
	return pStr;
}

static void StrRtrim(char *pStr)
{
	int i = str_length(pStr);
	while(i >= 0)
	{
		if(pStr[i] < 0 || pStr[i] > 32)
			break;
		pStr[i] = 0;
		i--;
	}
}


CSnapIDPool::CSnapIDPool()
{
	Reset();
}

void CSnapIDPool::Reset()
{
	for(int i = 0; i < MAX_IDS; i++)
	{
		m_aIDs[i].m_Next = i+1;
		m_aIDs[i].m_State = 0;
	}

	m_aIDs[MAX_IDS-1].m_Next = -1;
	m_FirstFree = 0;
	m_FirstTimed = -1;
	m_LastTimed = -1;
	m_Usage = 0;
	m_InUsage = 0;
}


void CSnapIDPool::RemoveFirstTimeout()
{
	int NextTimed = m_aIDs[m_FirstTimed].m_Next;

	// add it to the free list
	m_aIDs[m_FirstTimed].m_Next = m_FirstFree;
	m_aIDs[m_FirstTimed].m_State = 0;
	m_FirstFree = m_FirstTimed;

	// remove it from the timed list
	m_FirstTimed = NextTimed;
	if(m_FirstTimed == -1)
		m_LastTimed = -1;

	m_Usage--;
}

int CSnapIDPool::NewID()
{
	int64 Now = time_get();

	// process timed ids
	while(m_FirstTimed != -1 && m_aIDs[m_FirstTimed].m_Timeout < Now)
		RemoveFirstTimeout();

	int ID = m_FirstFree;
	dbg_assert(ID != -1, "id error");
	if(ID == -1)
		return ID;
	m_FirstFree = m_aIDs[m_FirstFree].m_Next;
	m_aIDs[ID].m_State = 1;
	m_Usage++;
	m_InUsage++;
	return ID;
}

void CSnapIDPool::TimeoutIDs()
{
	// process timed ids
	while(m_FirstTimed != -1)
		RemoveFirstTimeout();
}

void CSnapIDPool::FreeID(int ID)
{
	if(ID < 0)
		return;
	dbg_assert(m_aIDs[ID].m_State == 1, "id is not alloced");

	m_InUsage--;
	m_aIDs[ID].m_State = 2;
	m_aIDs[ID].m_Timeout = time_get()+time_freq()*5;
	m_aIDs[ID].m_Next = -1;

	if(m_LastTimed != -1)
	{
		m_aIDs[m_LastTimed].m_Next = ID;
		m_LastTimed = ID;
	}
	else
	{
		m_FirstTimed = ID;
		m_LastTimed = ID;
	}
}

void CServer::CClient::Reset()
{
	// reset input
	for(int i = 0; i < 200; i++)
		m_aInputs[i].m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = 0;
}

CServer::CServer() : m_DemoRecorder(&m_SnapshotDelta)
{
	m_TickSpeed = SERVER_TICK_SPEED;

	m_pGameServer = 0;

	m_CurrentGameTick = 0;
	m_RunServer = 1;

	m_pCurrentMapData = 0;
	m_CurrentMapSize = 0;

	m_MapReload = 0;

	m_RconClientID = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	Init();
}


int CServer::TrySetClientName(int ClientID, const char *pName)
{
	char aTrimmedName[64];

	// trim the name
	str_copy(aTrimmedName, StrLtrim(pName), sizeof(aTrimmedName));
	StrRtrim(aTrimmedName);

	// check for empty names
	if(!aTrimmedName[0])
		return -1;

	// check if new and old name are the same
	if(m_aClients[ClientID].m_aName[0] && str_comp(m_aClients[ClientID].m_aName, aTrimmedName) == 0)
		return 0;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "'%s' -> '%s'", pName, aTrimmedName);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
	pName = aTrimmedName;

	// make sure that two clients doesn't have the same name
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(i != ClientID && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if(str_comp(pName, m_aClients[i].m_aName) == 0)
				return -1;
		}

	// set the client name
	str_copy(m_aClients[ClientID].m_aName, pName, MAX_NAME_LENGTH);
	return 0;
}



void CServer::SetClientName(int ClientID, const char *pName)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	if(!pName)
		return;

	char aCleanName[MAX_NAME_LENGTH];
	str_copy(aCleanName, pName, sizeof(aCleanName));

	// clear name
	for(char *p = aCleanName; *p; ++p)
	{
		if(*p < 32)
			*p = ' ';
	}

	if(TrySetClientName(ClientID, aCleanName))
	{
		// auto rename
		for(int i = 1;; i++)
		{
			char aNameTry[MAX_NAME_LENGTH];
			str_format(aNameTry, sizeof(aCleanName), "(%d)%s", i, aCleanName);
			if(TrySetClientName(ClientID, aNameTry) == 0)
				break;
		}
	}
}

void CServer::SetClientClan(int ClientID, const char *pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_copy(m_aClients[ClientID].m_aClan, pClan, MAX_CLAN_LENGTH);
}

void CServer::SetClientCountry(int ClientID, int Country)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Country = Country;
}

void CServer::SetClientScore(int ClientID, int Score)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;
	m_aClients[ClientID].m_Score = Score;
}

/*int CServer::Tick()
{
	return m_CurrentGameTick;
}*/

int64 CServer::TickStartTime(int Tick)
{
	return m_GameStartTime + (time_freq()*Tick)/SERVER_TICK_SPEED;
}

/*int CServer::TickSpeed()
{
	return SERVER_TICK_SPEED;
}*/

int CServer::Init()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aClients[i].m_State = CClient::STATE_EMPTY;
		m_aClients[i].m_aName[0] = 0;
		m_aClients[i].m_aClan[0] = 0;
		m_aClients[i].m_Country = -1;
		m_aClients[i].m_Snapshots.Init();
		m_aClients[i].m_IsBot = false;
	}

	m_CurrentGameTick = 0;

	return 0;
}

void CServer::SetRconCID(int ClientID)
{
	m_RconClientID = ClientID;
}

bool CServer::IsAuthed(int ClientID)
{
	return m_aClients[ClientID].m_Authed;
}

int CServer::GetClientInfo(int ClientID, CClientInfo *pInfo)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "client_id is not valid");
	dbg_assert(pInfo != 0, "info can not be null");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		pInfo->m_pName = m_aClients[ClientID].m_aName;
		pInfo->m_Latency = m_aClients[ClientID].m_Latency;
		return 1;
	}
	return 0;
}

const char *CServer::ClientName(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "(invalid)";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aName;
	else
		return "(connecting)";

}

const char *CServer::ClientClan(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aClan;
	else
		return "";
}

int CServer::ClientCountry(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_Country;
	else
		return -1;
}

bool CServer::ClientIngame(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME;
}

int CServer::MaxClients() const
{
	return g_Config.m_SvMaxClients;
}

int CServer::SendMsg(CMsgPacker *pMsg, int Flags, int ClientID)
{
	return SendMsgEx(pMsg, Flags, ClientID, false);
}

int CServer::SendMsgEx(CMsgPacker *pMsg, int Flags, int ClientID, bool System)
{
	CNetChunk Packet;
	if(!pMsg)
		return -1;

	mem_zero(&Packet, sizeof(CNetChunk));

	Packet.m_ClientID = ClientID;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	// HACK: modify the message id in the packet and store the system flag
	*((unsigned char*)Packet.m_pData) <<= 1;
	if(System)
		*((unsigned char*)Packet.m_pData) |= 1;

	if(Flags&MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags&MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	// write message to demo recorder
	if(!(Flags&MSGFLAG_NORECORD))
		m_DemoRecorder.RecordMessage(pMsg->Data(), pMsg->Size());
	return 0;
}

void CServer::DoSnapshot()
{
	GameServer()->OnPreSnap();

	// create snapshot for demo recording
	if(m_DemoRecorder.IsRecording())
	{
		char aData[CSnapshot::MAX_SIZE];
		int SnapshotSize;

		// build snap and possibly add some messages
		m_SnapshotBuilder.Init();
		GameServer()->OnSnap(-1);
		SnapshotSize = m_SnapshotBuilder.Finish(aData);

		// write snapshot
		m_DemoRecorder.RecordSnapshot(Tick(), aData, SnapshotSize);
	}

	// create snapshots for all clients
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// client must be ingame to recive snapshots
		if(m_aClients[i].m_State != CClient::STATE_INGAME)
			continue;

		// client must be human to recive snapshots
		if(m_aClients[i].m_IsBot)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick()%50) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick()%10) != 0)
			continue;

		{
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot *pData = (CSnapshot*)aData;	// Fix compiler warning for strict-aliasing
			char aDeltaData[CSnapshot::MAX_SIZE];
			char aCompData[CSnapshot::MAX_SIZE];
			int SnapshotSize;
			int Crc;
			static CSnapshot EmptySnap;
			CSnapshot *pDeltashot = &EmptySnap;
			int DeltashotSize;
			int DeltaTick = -1;
			int DeltaSize;

			m_SnapshotBuilder.Init();

			GameServer()->OnSnap(i);

			// finish snapshot
			SnapshotSize = m_SnapshotBuilder.Finish(pData);
			Crc = pData->Crc();

			// remove old snapshos
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick-SERVER_TICK_SPEED*3);

			// save it the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0);

			// find snapshot that we can preform delta against
			EmptySnap.Clear();

			{
				DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, 0, &pDeltashot, 0);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData);

			if(DeltaSize)
			{
				// compress it
				int SnapshotSize;
				const int MaxSize = MAX_SNAPSHOT_PACKSIZE;
				int NumPackets;

				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData);
				NumPackets = (SnapshotSize+MaxSize-1)/MaxSize;

				for(int n = 0, Left = SnapshotSize; Left; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick-DeltaTick);
				SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
			}
		}
	}

	GameServer()->OnPostSnap();
}

int CServer::NewBot(int ClientID)
{
	if(m_aClients[ClientID].m_State > CClient::STATE_EMPTY && !m_aClients[ClientID].m_IsBot)
		return 1;
	m_aClients[ClientID].m_State = CClient::STATE_INGAME;
	m_aClients[ClientID].m_IsBot = true;
	return 0;
}

int CServer::DelBot(int ClientID)
{
	if( !m_aClients[ClientID].m_IsBot )
		return 1;
	m_aClients[ClientID].m_State = CClient::STATE_EMPTY;
	m_aClients[ClientID].m_aName[0] = 0;
	m_aClients[ClientID].m_aClan[0] = 0;
	m_aClients[ClientID].m_Country = -1;
	m_aClients[ClientID].m_Authed = AUTHED_NO;
	m_aClients[ClientID].m_AuthTries = 0;
	m_aClients[ClientID].m_pRconCmdToSend = 0;
	m_aClients[ClientID].m_IsBot = false;
	m_aClients[ClientID].m_Snapshots.PurgeAll();
	return 0;
}

void CServer::SendMap(int ClientID)
{
	CMsgPacker Msg(NETMSG_MAP_CHANGE);
	Msg.AddString(GetMapName(), 0);
	Msg.AddInt(m_CurrentMapCrc);
	Msg.AddInt(m_CurrentMapSize);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID, true);
}

void CServer::SendConnectionReady(int ClientID)
{
	CMsgPacker Msg(NETMSG_CON_READY);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID, true);
}

void CServer::SendRconLine(int ClientID, const char *pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE);
	Msg.AddString(pLine, 512);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::SendRconLineAuthed(const char *pLine, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	static volatile int ReentryGuard = 0;
	int i;

	if(ReentryGuard) return;
	ReentryGuard++;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY && pThis->m_aClients[i].m_Authed >= pThis->m_RconAuthLevel)
			pThis->SendRconLine(i, pLine);
	}

	ReentryGuard--;
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_ADD);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_REM);
	Msg.AddString(pCommandInfo->m_pName, 256);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::UpdateClientRconCommands()
{
	int ClientID = Tick() % MAX_CLIENTS;

	if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
	{
		int ConsoleAccessLevel = m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : IConsole::ACCESS_LEVEL_MOD;
		for(int i = 0; i < MAX_RCONCMD_SEND && m_aClients[ClientID].m_pRconCmdToSend; ++i)
		{
			SendRconCmdAdd(m_aClients[ClientID].m_pRconCmdToSend, ClientID);
			m_aClients[ClientID].m_pRconCmdToSend = m_aClients[ClientID].m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
		}
	}
}


void CServer::PumpNetwork()
{
	m_Econ.Update();
}

char *CServer::GetMapName()
{
	// get the name of the map without his path
	char *pMapShortName = &g_Config.m_SvMap[0];
	for(int i = 0; i < str_length(g_Config.m_SvMap)-1; i++)
	{
		if(g_Config.m_SvMap[i] == '/' || g_Config.m_SvMap[i] == '\\')
			pMapShortName = &g_Config.m_SvMap[i+1];
	}
	return pMapShortName;
}

int CServer::LoadMap(const char *pMapName)
{
	//DATAFILE *df;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);

	/*df = datafile_load(buf);
	if(!df)
		return 0;*/

	// check for valid standard map
	if(!m_MapChecker.ReadAndValidateMap(Storage(), aBuf, IStorage::TYPE_ALL))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapchecker", "invalid standard map");
		return 0;
	}

	if(!m_pMap->Load(aBuf))
		return 0;

	// stop recording when we change map
	m_DemoRecorder.Stop();

	// reinit snapshot ids
	m_IDPool.TimeoutIDs();

	// get the crc of the map
	m_CurrentMapCrc = m_pMap->Crc();
	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "%s crc is %08x", aBuf, m_CurrentMapCrc);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);

	str_copy(m_aCurrentMap, pMapName, sizeof(m_aCurrentMap));
	//map_set(df);

	// load complete map into memory for download
	{
		IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		m_CurrentMapSize = (int)io_length(File);
		if(m_pCurrentMapData)
			mem_free(m_pCurrentMapData);
		m_pCurrentMapData = (unsigned char *)mem_alloc(m_CurrentMapSize, 1);
		io_read(File, m_pCurrentMapData, m_CurrentMapSize);
		io_close(File);
	}
	return 1;
}

int CServer::Run()
{
	//
	m_PrintCBIndex = Console()->RegisterPrintCallback(g_Config.m_ConsoleOutputLevel, SendRconLineAuthed, this);

	// load map
	if(!LoadMap(g_Config.m_SvMap))
	{
		dbg_msg("server", "failed to load map. mapname='%s'", g_Config.m_SvMap);
		return -1;
	}

	m_Econ.Init(Console());

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", g_Config.m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	GameServer()->OnInit();
	str_format(aBuf, sizeof(aBuf), "version %s", GameServer()->NetVersion());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// process pending commands
	m_pConsole->StoreCommands(false);

	// start game
	{
		int64 ReportTime = time_get();
		int ReportInterval = 3;

		m_Lastheartbeat = 0;
		m_GameStartTime = time_get();

		if(g_Config.m_Debug)
		{
			str_format(aBuf, sizeof(aBuf), "baseline memory usage %dk", mem_stats()->allocated/1024);
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}

		while(m_RunServer)
		{
			int64 t = time_get();
			int NewTicks = 0;

			// load new map TODO: don't poll this
			if(str_comp(g_Config.m_SvMap, m_aCurrentMap) != 0 || m_MapReload)
			{
				m_MapReload = 0;

				// load map
				if(LoadMap(g_Config.m_SvMap))
				{
					// new map loaded
					GameServer()->OnShutdown();

					for(int c = 0; c < MAX_CLIENTS; c++)
					{
						if(m_aClients[c].m_State <= CClient::STATE_AUTH)
							continue;

						SendMap(c);
						m_aClients[c].Reset();
						m_aClients[c].m_State = CClient::STATE_CONNECTING;
					}

					m_GameStartTime = time_get();
					m_CurrentGameTick = 0;
					Kernel()->ReregisterInterface(GameServer());
					GameServer()->OnInit();
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "failed to load map. mapname='%s'", g_Config.m_SvMap);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					str_copy(g_Config.m_SvMap, m_aCurrentMap, sizeof(g_Config.m_SvMap));
				}
			}

			{
				m_CurrentGameTick++;
				NewTicks++;

				// apply new input
				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State == CClient::STATE_EMPTY)
						continue;
					for(int i = 0; i < 200; i++)
					{
						if(m_aClients[c].m_aInputs[i].m_GameTick == Tick())
						{
							if(m_aClients[c].m_State == CClient::STATE_INGAME)
								GameServer()->OnClientPredictedInput(c, m_aClients[c].m_aInputs[i].m_aData);
							break;
						}
					}
				}

				GameServer()->OnTick();
			}

			// snap game
			if(NewTicks)
			{
				if(g_Config.m_SvHighBandwidth || (m_CurrentGameTick%2) == 0)
					DoSnapshot();

				UpdateClientRconCommands();
			}

			PumpNetwork();

			if(ReportTime < time_get())
			{
				if(g_Config.m_Debug)
				{
					/*
					static NETSTATS prev_stats;
					NETSTATS stats;
					netserver_stats(net, &stats);

					perf_next();

					if(config.dbg_pref)
						perf_dump(&rootscope);

					dbg_msg("server", "send=%8d recv=%8d",
						(stats.send_bytes - prev_stats.send_bytes)/reportinterval,
						(stats.recv_bytes - prev_stats.recv_bytes)/reportinterval);

					prev_stats = stats;
					*/
				}

				ReportTime += time_freq()*ReportInterval;
			}
		}
	}
	m_Econ.Shutdown();

	GameServer()->OnShutdown();
	m_pMap->Unload();

	if(m_pCurrentMapData)
		mem_free(m_pCurrentMapData);
	return 0;
}

void CServer::ConStatus(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[1024];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	CServer* pThis = static_cast<CServer *>(pUser);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
			{
				const char *pAuthStr = pThis->m_aClients[i].m_Authed == CServer::AUTHED_ADMIN ? "(Admin)" :
										pThis->m_aClients[i].m_Authed == CServer::AUTHED_MOD ? "(Mod)" : 
										pThis->m_aClients[i].m_IsBot ? "(Bot)" : "";
				str_format(aBuf, sizeof(aBuf), "id=%d name='%s' score=%d %s", i,
					pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score, pAuthStr);
				pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
			}
		}
	}
}

void CServer::ConShutdown(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_RunServer = 0;
}

void CServer::DemoRecorder_HandleAutoStart()
{
	if(g_Config.m_SvAutoDemoRecord)
	{
		m_DemoRecorder.Stop();
		char aFilename[128];
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", "auto/autorecord", aDate);
		m_DemoRecorder.Start(Storage(), m_pConsole, aFilename, GameServer()->NetVersion(), m_aCurrentMap, m_CurrentMapCrc, "server");
		if(g_Config.m_SvAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/server", "autorecord", ".demo", g_Config.m_SvAutoDemoMax);
		}
	}
}

bool CServer::DemoRecorder_IsRecording()
{
	return m_DemoRecorder.IsRecording();
}

void CServer::ConRecord(IConsole::IResult *pResult, void *pUser)
{
	CServer* pServer = (CServer *)pUser;
	char aFilename[128];

	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aDate);
	}
	pServer->m_DemoRecorder.Start(pServer->Storage(), pServer->Console(), aFilename, pServer->GameServer()->NetVersion(), pServer->m_aCurrentMap, pServer->m_CurrentMapCrc, "server");
}

void CServer::ConStopRecord(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_DemoRecorder.Stop();
}

void CServer::ConMapReload(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_MapReload = 1;
}

void CServer::ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->Console()->SetPrintOutputLevel(pThis->m_PrintCBIndex, pResult->GetInteger(0));
	}
}

void CServer::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGameServer = Kernel()->RequestInterface<IGameServer>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	// register console commands
	Console()->Register("status", "", CFGFLAG_SERVER, ConStatus, this, "List players");
	Console()->Register("shutdown", "", CFGFLAG_SERVER, ConShutdown, this, "Shut down");

	Console()->Register("record", "?s", CFGFLAG_SERVER|CFGFLAG_STORE, ConRecord, this, "Record to a file");
	Console()->Register("stoprecord", "", CFGFLAG_SERVER, ConStopRecord, this, "Stop recording");

	Console()->Register("reload", "", CFGFLAG_SERVER, ConMapReload, this, "Reload the map");

	Console()->Chain("console_output_level", ConchainConsoleOutputLevelUpdate, this);

	// register console commands in sub parts
	m_pGameServer->OnConsoleInit();
}


int CServer::SnapNewID()
{
	return m_IDPool.NewID();
}

void CServer::SnapFreeID(int ID)
{
	m_IDPool.FreeID(ID);
}


void *CServer::SnapNewItem(int Type, int ID, int Size)
{
	dbg_assert(Type >= 0 && Type <=0xffff, "incorrect type");
	dbg_assert(ID >= 0 && ID <=0xffff, "incorrect id");
	return ID < 0 ? 0 : m_SnapshotBuilder.NewItem(Type, ID, Size);
}

void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

static CServer *CreateServer() { return new CServer(); }

int main(int argc, const char **argv) // ignore_convention
{
#if defined(CONF_FAMILY_WINDOWS)
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0) // ignore_convention
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			break;
		}
	}
#endif

	CServer *pServer = CreateServer();
	IKernel *pKernel = IKernel::Create();

	// create the components
	IEngine *pEngine = CreateEngine("Teeworlds");
	IEngineMap *pEngineMap = CreateEngineMap();
	IGameServer *pGameServer = CreateGameServer();
	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER|CFGFLAG_ECON);
	IEngineMasterServer *pEngineMasterServer = CreateEngineMasterServer();
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfig *pConfig = CreateConfig();

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pServer); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap));
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pGameServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfig);

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfig->Init();

	// register all console commands
	pServer->RegisterCommands();

	// execute autoexec file
	pConsole->ExecuteFile("autoexec.cfg");

	// parse the command line arguments
	if(argc > 1) // ignore_convention
		pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention

	// restore empty config strings to their defaults
	pConfig->RestoreStrings();

	pEngine->InitLogfile();

	// run the server
	dbg_msg("server", "starting...");
	pServer->Run();

	// free
	delete pServer;
	delete pKernel;
	delete pEngineMap;
	delete pGameServer;
	delete pConsole;
	delete pStorage;
	delete pConfig;
	return 0;
}
