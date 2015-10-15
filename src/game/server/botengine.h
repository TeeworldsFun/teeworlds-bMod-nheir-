#ifndef GAME_SERVER_BOTENGINE_H
#define GAME_SERVER_BOTENGINE_H

#include <base/vmath.h>

const char g_IsRemovable[256] = { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0};
const char g_ConnectedComponents[256] = { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 2, 2, 3, 2, 2, 2, 3, 2, 3, 3, 4, 3, 3, 3, 3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1 };
const int g_Neighboors[8][2] = { {-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
const int g_PowerTwo[8] = {1,2,4,8,16,32,64,128};

enum {
	GTILE_FLAGSTAND_RED=0,
	GTILE_FLAGSTAND_BLUE,
	GTILE_ARMOR,
	GTILE_HEALTH,
	GTILE_WEAPON_SHOTGUN,
	GTILE_WEAPON_GRENADE,
	GTILE_POWERUP_NINJA,
	GTILE_WEAPON_RIFLE,

	GTILE_AIR,
	GTILE_SOLID,
	GTILE_DEATH,
	GTILE_NOHOOK,
	GTILE_MASK=15,
	// GTILE_FLAG=16,

	BTILE_SAFE=16,
	BTILE_HOLE=32,
	BTILE_LHOLE=64,
	BTILE_RHOLE=128,
	//BTILE_FLAG=256

	GTILE_SKELETON=256,
	GTILE_ANCHOR=512,
	GTILE_REMOVED=1024,
	GTILE_WAIT=2048,

	GVERTEX_USE_ONCE=4096,
	GVERTEX_USE_TWICE=8192
};

class CVertex {
public:
  int m_Index;
  int m_Degree;
};

class CEdge {
public:
  int m_Start;
  int m_End;
  int m_Size;
  int *m_pPath;

  CEdge();
  ~CEdge();
  void Reset();
  void Free();
};

class CGraph {
public:
  CEdge *m_pEdges;
  int m_NumEdges;
  CVertex *m_pVertices;
  int m_NumVertices;

  int *m_pClosestPath;
  CEdge** m_ppAdjacency;

  CGraph();
  ~CGraph();
  void Reset();
  void Free();

  void ComputeClosestPath();

  CEdge GetPath(int VStart, int VEnd);
};

class CBotEngine
{
  class CGameContext *m_pGameServer;
protected:

	class CTile *m_pTiles;
	int *m_pGrid;
	int m_Width;
	int m_Height;

	CGraph m_Graph;

	int m_aFlagTiles[2];

public:
	CBotEngine(class CGameContext *pGameServer);
	~CBotEngine();

  int GetWidth() { return m_Width; }
  CGraph *GetGraph() { return &m_Graph; }
  int GetFlagStandPos(int Team) { return m_aFlagTiles[Team&1]; }

  int GetTile(int x, int y);
  int GetTile(int i) { return GetTile(i % m_Width, i / m_Width); };

	CEdge* GetClosestEdge(vec2 Pos, float ClosestRange);

  class CGameContext *GameServer() { return m_pGameServer;}

	void Init(class CTile *pTiles, int Width, int Height);
	void OnRelease();
};

#endif
