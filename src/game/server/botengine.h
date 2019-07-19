#ifndef GAME_SERVER_BOTENGINE_H
#define GAME_SERVER_BOTENGINE_H

#include <base/vmath.h>
#include <base/tl/array.h>

const char g_IsRemovable[256] = { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0};
const char g_ConnectedComponents[256] = { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 2, 2, 3, 2, 2, 2, 3, 2, 3, 3, 4, 3, 3, 3, 3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1 };
const char g_IsInnerCorner[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
const char g_IsOuterCorner[256] = {0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char g_IsOnEdge[256] = {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
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

class CTriangle {
	private:
		static float sign(vec2 p1, vec2 p2, vec2 p3) {
			return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
		};

		static float det(float a11, float a12, float a13, float a21, float a22, float a23, float a31, float a32, float a33) {
			return a11 * a22 * a33 - a11 * a23 * a32 - a12 * a21 * a33 + a12 * a23 * a31 + a13 * a21 * a32 - a13 * a22 * a31;
		};

		static bool intersect(vec2 a1, vec2 a2, vec2 b1, vec2 b2) {
			float v1 = (b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x);
			float v2 = (b2.x - b1.x) * (a2.y - b1.y) - (b2.y - b1.y) * (a2.x - b1.x);
			float v3 = (a2.x - a1.x) * (b1.y - a1.y) - (a2.y - a1.y) * (b1.x - a1.x);
			float v4 = (a2.x - a1.x) * (b2.y - a1.y) - (a2.y - a1.y) * (b2.x - a1.x);
			return (v1 * v2 < 0) && (v3 * v4 < 0);
		};
	public:
		CTriangle(vec2 a, vec2 b, vec2 c) {
			m_aPoints[0] = a;
			m_aPoints[1] = b;
			m_aPoints[2] = c;
		};

		CTriangle(const CTriangle &c) {
			m_aPoints[0] = c.m_aPoints[0];
			m_aPoints[1] = c.m_aPoints[1];
			m_aPoints[2] = c.m_aPoints[2];
		}
		const CTriangle &operator =(const CTriangle &c) {
			m_aPoints[0] = c.m_aPoints[0];
			m_aPoints[1] = c.m_aPoints[1];
			m_aPoints[2] = c.m_aPoints[2];
			return *this;
		}

		vec2 m_aPoints[3];

		bool Inside(vec2 pt) {
			bool b1 = sign(pt, m_aPoints[0], m_aPoints[1]) < 0.0f;
			bool b2 = sign(pt, m_aPoints[1], m_aPoints[2]) < 0.0f;
			bool b3 = sign(pt, m_aPoints[2], m_aPoints[0]) < 0.0f;

			return (b1 == b2) && (b2 == b3);
		}

		bool InsideOrSide(vec2 pt) {
			if(Inside(pt))
				return true;
			float f1 = sign(pt, m_aPoints[0], m_aPoints[1]);
			float f2 = sign(pt, m_aPoints[1], m_aPoints[2]);
			float f3 = sign(pt, m_aPoints[2], m_aPoints[0]);

			if(f1 == 0.0f)
				return (f2 < 0.0f) == (f3 < 0.0f);
			if(f2 == 0.0f)
				return (f1 < 0.0f) == (f3 < 0.0f);
			if(f3 == 0.0f)
				return (f1 < 0.0f) == (f2 < 0.0f);
			return false;
		}

		bool InsideOuterCircle(vec2 pt) {
			vec2 Center = OuterCircleCenter();

			return distance(pt, Center) <= distance(m_aPoints[0],Center);
		}

		float Square() {
			float a = distance(m_aPoints[0], m_aPoints[1]);
			float b = distance(m_aPoints[1], m_aPoints[2]);
			float c = distance(m_aPoints[2], m_aPoints[0]);

			float p = (a + b + c) * 0.5f;

			return sqrtf(p * (p - a) * (p - b) * (p - c));
		}

		vec2 CenterA() {
			return (m_aPoints[0] + m_aPoints[1]) * 0.5;
		}

		vec2 CenterB() {
			return (m_aPoints[1] + m_aPoints[2]) * 0.5;
		}

		vec2 CenterC() {
			return (m_aPoints[2] + m_aPoints[0]) * 0.5;
		}

		vec2 Centroid() {
			return vec2((m_aPoints[0].x + m_aPoints[1].x + m_aPoints[2].x) / 3.0f, (m_aPoints[0].y + m_aPoints[1].y + m_aPoints[2].y) / 3.0f);
		}

		bool IsFlat() {
			vec2 a = m_aPoints[0];
			vec2 b = m_aPoints[1];
			vec2 c = m_aPoints[2];
			return abs(det(a.x, a.y, 1,
								 b.x, b.y, 1,
								 c.x, c.y, 1)) < 0.0001;
		}

		vec2 OuterCircleCenter() {
			vec2 a = m_aPoints[0];
			vec2 b = m_aPoints[1];
			vec2 c = m_aPoints[2];

			float d = 2 * det(a.x, a.y, 1,
												b.x, b.y, 1,
												c.x, c.y, 1);
			return vec2(
				det(a.x * a.x + a.y * a.y, a.y, 1,
						b.x * b.x + b.y * b.y, b.y, 1,
						c.x * c.x + c.y * c.y, c.y, 1) / d,
				det(a.x, a.x * a.x + a.y * a.y, 1,
						b.x, b.x * b.x + b.y * b.y, 1,
						c.x, c.x * c.x + c.y * c.y, 1) / d
			);
		}

		bool Intersects(CTriangle t) {
			int count = 0;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (intersect(m_aPoints[i], m_aPoints[(i + 1) % 3], t.m_aPoints[j], t.m_aPoints[(j + 1) % 3]))
						count++;
				}
			}
			return count >= 1;
		}
};


// Planar graph
class CGraph {

public:
	struct CEdge {
		int m_Start;
		int m_End;
		double m_Length;
		int m_Data;
	};

	CGraph();
	~CGraph();
	bool Reserve(int NumVertices, int NumEdges);
	void Reset();
	void Free();

	int AddVertex(vec2 Pos);
	vec2 GetVertex(int id) const;
	int NumVertices()  const { return m_lVertices.size(); }

	int AddEdge(vec2 Start, vec2 End);
	int AddEdge(int StartId, int EndId);
	CEdge GetEdge(int Id) const;
	int NumEdges() const { return m_lEdges.size(); }

	int GetEdgeData(int Id) const;
	void SetEdgeData(int Id, int Data);

	int Diameter() const { return m_Diameter; };

	void ComputeClosestPath();

	int GetPath(vec2 VStart, vec2 VEnd, vec2 *pVertices) const;
	bool GetNextInPath(vec2 VStart, vec2 VEnd, vec2 *pNextVertex) const;

protected:
	array<vec2> m_lVertices;
	array<CEdge> m_lEdges;

	int *m_pClosestPath;

	int m_Diameter;

	int m_Width;

	int GetVertex(vec2 Pos) const;
};

class CBotEngine
{
	class CGameContext *m_pGameServer;
protected:

	class CTile *m_pTiles;
	int *m_pGrid;

	vec2 *m_pCorners;
	int m_CornerCount;

	struct CSegment {
		bool m_IsVertical;
		vec2 m_A;
		vec2 m_B;
	};
	array<CSegment> m_lSegments;

	int m_SegmentCount;
	int m_HSegmentCount;

	int m_Width;
	int m_Height;

	CGraph m_Graph;
	struct CTriangulation {
		struct CTriangleData {
			CTriangle m_Triangle;
			int m_aSnapID[3];
			int m_IsAir;
		} *m_pTriangles;
		int m_Size;
		int m_CornerCount;
	} m_Triangulation;

	void Free();

	bool IsCorner(int i, int j);
	bool IsOnEdge(int i, int j);

	void GenerateCorners();
	void GenerateSegments();
	void GenerateTriangles();
	void GenerateGraphFromTriangles();

	vec2 m_aFlagStandPos[2];
	int m_aBotSnapID[MAX_CLIENTS];

	class CBot *m_apBot[MAX_CLIENTS];

public:
	CBotEngine(class CGameContext *pGameServer);
	~CBotEngine();

	struct CPath {
		vec2 *m_pVertices;
		int *m_pSnapID;
		int m_Size;
		int m_MaxSize;
	} m_aPaths[MAX_CLIENTS];

	int GetWidth() { return m_Width; }
	CGraph *GetGraph() { return &m_Graph; }
	vec2 GetFlagStandPos(int Team) { return m_aFlagStandPos[Team&1]; }

	void GetPath(vec2 VStart, vec2 VEnd, CPath* pPath);
	void SmoothPath(CPath *pPath);
	bool NextPoint(vec2 Pos, vec2 Target, vec2* NewPoint);

	int GetTile(int x, int y);
	int GetTile(vec2 Pos);
	int GetTile(int i) { return m_pGrid[i]; };
	int FastIntersectLine(int Id1, int Id2);
	int IntersectSegment(vec2 P1, vec2 P2, vec2 *pPos);

	//int GetClosestEdge(vec2 Pos, int ClosestRange, CGraph::CEdge *pEdge);
	vec2 GetClosestVertex(vec2 Pos);
	double FarestPointOnEdge(CPath *pPath, vec2 Pos, vec2 *pTarget);
	//int DistanceToEdge(CGraph::CEdge Edge, vec2 Pos);

	vec2 ConvertIndex(int ID) { return vec2(ID%m_Width,ID/m_Width)*32 + vec2(16.,16.); }
	int ConvertFromIndex(vec2 Pos);

	class CGameContext *GameServer() { return m_pGameServer;}

	void Init(class CTile *pTiles, int Width, int Height);
	void Snap(int SnappingClient);
	void OnRelease();

	int NetworkClipped(int SnappingClient, vec2 CheckPos);

	void OnCharacterDeath(int Victim, int Killer, int Weapon);
	void RegisterBot(int CID, class CBot *pBot);
	void UnRegisterBot(int CID);

	static int SegmentComp(const void *a, const void *b);
};

#endif
