#include <base/tl/algorithm.h>
#include <game/gamecore.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/layers.h>
#include "gamecontext.h"

#include "botengine.h"
#include "bot.h"

CGraph::CGraph()
{
	m_pClosestPath = 0;
	m_Diameter = 0;
}

CGraph::~CGraph()
{
	Free();
}

void CGraph::Reset()
{
	Free();
	m_Diameter = 0;
}

void CGraph::Free()
{
	m_lVertices.clear();
	m_lEdges.clear();
	m_pClosestPath = 0;
}

int CGraph::GetVertex(vec2 Pos) const
{
	array<vec2>::range r = ::find_linear(m_lVertices.all(), Pos);
	if (r.empty())
		return -1;
	return m_lVertices.size() - r.size();
}

vec2 CGraph::GetVertex(int Id) const
{
	if(Id < 0 || Id >= m_lVertices.size())
		return vec2(0,0);
	return m_lVertices[Id];
}

int CGraph::AddVertex(vec2 Pos)
{
	int r = GetVertex(Pos);
	if (r < 0)
		return m_lVertices.add(Pos);
	return r;
}

int CGraph::AddEdge(vec2 Start, vec2 End)
{
	return AddEdge(AddVertex(Start), AddVertex(End));
}

int CGraph::AddEdge(int StartId, int EndId)
{
	if (StartId < 0 || EndId < 0)
		return -1;
	if (StartId >= m_lVertices.size() || EndId >= m_lVertices.size())
		return -1;
	CEdge Edge = {
		.m_Start = StartId,
		.m_End = EndId,
		.m_Length = distance(m_lVertices[StartId], m_lVertices[EndId])
	};
	return m_lEdges.add(Edge);
}

CGraph::CEdge CGraph::GetEdge(int Id) const
{
	if( Id < 0 || Id >= m_lEdges.size())
		return {0};
	return m_lEdges[Id];
}

int CGraph::GetEdgeData(int Id) const
{
	if( Id < 0 || Id >= m_lEdges.size())
		return 0;
	return m_lEdges[Id].m_Data;
}

void CGraph::SetEdgeData(int Id, int Data)
{
	if( Id < 0 || Id >= m_lEdges.size())
		return;
	m_lEdges[Id].m_Data = Data;
}

void CGraph::ComputeClosestPath()
{
	if(m_pClosestPath)
		mem_free(m_pClosestPath);
	int NumVertices = m_lVertices.size();
	m_pClosestPath = (int*)mem_alloc(NumVertices*NumVertices*sizeof(int),1);
	if(!m_pClosestPath)
		return;
	int* Size = (int*)mem_alloc(NumVertices*NumVertices*sizeof(int),1);
	if(!Size)
		return;
	double* Dist = (double*)mem_alloc(NumVertices*NumVertices*sizeof(double),1);
	if(!Dist)
	{
		mem_free(Size);
		return;
	}

	double EdgeSizeMax = 0;
	for(int i=0; i < m_lEdges.size() ; i++)
		if(m_lEdges[i].m_Length > EdgeSizeMax)
			EdgeSizeMax = m_lEdges[i].m_Length;

	for(int i = 0 ; i < NumVertices*NumVertices ; i++)
	{
		m_pClosestPath[i] = -1;
		Dist[i] = NumVertices*EdgeSizeMax;
		Size[i] = NumVertices;
	}

	for(int i=0; i < NumVertices ; i++)
	{
		m_pClosestPath[i*(1+NumVertices)] = i;
		Dist[i*(1+NumVertices)] = 0.f;
		Size[i*(1+NumVertices)] = 0;
	}
	for(int i=0; i < m_lEdges.size() ; i++)
	{
		int aPoints[2] = { m_lEdges[i].m_Start, m_lEdges[i].m_End };
		for (int j=0; j < 2; j++)
		{
			Dist[aPoints[j] + aPoints[1-j] * NumVertices] = m_lEdges[i].m_Length;
			m_pClosestPath[aPoints[j] + aPoints[1-j] * NumVertices] = aPoints[1-j];
			Size[aPoints[j] + aPoints[1-j] * NumVertices] = 1;
		}
	}

	for(int k = 0; k < NumVertices ; k++)
	{
		for(int i = 0; i < NumVertices ; i++)
		{
			for(int j = 0; j < NumVertices ; j++)
			{
				if(Dist[i + k * NumVertices] + Dist[k + j * NumVertices] < Dist[i + j * NumVertices])
				{
					Dist[i + j * NumVertices] = Dist[i + k * NumVertices] + Dist[k + j * NumVertices];
					Size[i + j * NumVertices] = Size[i + k * NumVertices] + Size[k + j * NumVertices];
					m_pClosestPath[i + j * NumVertices] = m_pClosestPath[i + k * NumVertices];
				}
			}
		}
	}
	m_Diameter = 1;
	for(int i = 0 ; i < NumVertices*NumVertices ; i++)
		if(m_pClosestPath[i] >= 0 && m_Diameter < Size[i])
			m_Diameter = Size[i];
	dbg_msg("botengine","closest path computed, diameter=%d", m_Diameter);
	mem_free(Size);
	mem_free(Dist);
}

int CGraph::GetPath(vec2 VStart, vec2 VEnd, vec2 *pVertices) const
{
	int Start = GetVertex(VStart);
	int End = GetVertex(VEnd);
	int NumVertices = m_lVertices.size();

	if(Start < 0 || End < 0 || m_pClosestPath[Start + End * NumVertices] < 0)
		return 0;

	int Size = 0;
	int Cur = Start;
	while(Cur != End)
	{
		pVertices[Size++] = m_lVertices[Cur];
		Cur = m_pClosestPath[Cur + End * NumVertices];
	}
	pVertices[Size++] = VEnd;

	return Size;
}

bool CGraph::GetNextInPath(vec2 VStart, vec2 VEnd, vec2 *pNextVertex) const
{
	int Start = GetVertex(VStart);
	int End = GetVertex(VEnd);
	int NumVertices = m_lVertices.size();

	if(Start < 0 || End < 0 || m_pClosestPath[Start + End * NumVertices] < 0)
		return false;

	if (Start == End)
		return true;

	if(pNextVertex)
		*pNextVertex = GetVertex(m_pClosestPath[Start + End * NumVertices]);

	return true;
}


CBotEngine::CBotEngine(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pGrid = 0;
	m_Triangulation.m_pTriangles = 0;
	m_Triangulation.m_Size = 0;
	m_pCorners = 0;
	m_CornerCount = 0;
	m_SegmentCount = 0;
	mem_zero(m_aPaths,sizeof(m_aPaths));
}

void CBotEngine::Free()
{
	if(m_pGrid)
		mem_free(m_pGrid);

	for (int i = 0; i < m_Triangulation.m_Size; i++)
		for(int k = 0 ; k < 3; k++)
			GameServer()->Server()->SnapFreeID(m_Triangulation.m_pTriangles[i].m_aSnapID[k]);
	if(m_Triangulation.m_pTriangles)
		mem_free(m_Triangulation.m_pTriangles);
	m_Triangulation.m_Size = 0;
	m_Triangulation.m_pTriangles = 0;

	if(m_pCorners)
		mem_free(m_pCorners);
	m_CornerCount = 0;
	m_pCorners = 0;

	m_lSegments.clear();
	m_SegmentCount = 0;

	for(int k = 0; k < m_Graph.NumEdges(); k++)
		GameServer()->Server()->SnapFreeID(m_Graph.GetEdgeData(k));
	m_Graph.Free();

	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(m_aPaths[c].m_pVertices)
			mem_free(m_aPaths[c].m_pVertices);
		if(m_aPaths[c].m_pSnapID)
		{
			for(int i = 0 ; i < m_aPaths[c].m_MaxSize; i++)
				GameServer()->Server()->SnapFreeID(m_aPaths[c].m_pSnapID[i]);
			mem_free(m_aPaths[c].m_pSnapID);
		}
	}
	mem_zero(m_aPaths,sizeof(m_aPaths));
}

CBotEngine::~CBotEngine()
{
	Free();
}

void CBotEngine::Init(CTile *pTiles, int Width, int Height)
{
	m_pTiles = pTiles;

	m_Width = Width;
	m_Height = Height;

	Free();

	m_pGrid = (int *)mem_alloc(m_Width*m_Height*sizeof(int),1);
	if(m_pGrid)
	{
		mem_zero(m_pGrid, m_Width*m_Height*sizeof(char));

		int j = m_Height-1;
		int Margin = 6;
		// int MarginH = 3;
		for(int i = 0; i < m_Width; i++)
		{
			int Index = m_pTiles[i+j*m_Width].m_Reserved;

			if(Index <= TILE_NOHOOK)
			{
				m_pGrid[i+j*m_Width] = Index + GTILE_AIR - TILE_AIR;
			}
			if(Index >= ENTITY_OFFSET + ENTITY_FLAGSTAND_RED && Index < ENTITY_OFFSET + NUM_ENTITIES)
			{
				m_pGrid[i+j*m_Width] = Index + GTILE_FLAGSTAND_RED - ENTITY_FLAGSTAND_RED - ENTITY_OFFSET;
				if(m_pGrid[i+j*m_Width] <= GTILE_FLAGSTAND_BLUE)
					m_aFlagStandPos[m_pGrid[i+j*m_Width]] = vec2(i,j)*32+vec2(16,16);
			}

			if(m_pGrid[i+j*m_Width] == GTILE_SOLID || m_pGrid[i+j*m_Width] == GTILE_NOHOOK)
				m_pGrid[i+j*m_Width] |= BTILE_SAFE;
			else
				m_pGrid[i+j*m_Width] |= BTILE_HOLE;
		}
		for(int i = Margin; i < m_Width; i++)
		{
			if(m_pGrid[i+j*m_Width] & BTILE_SAFE)
				break;
			else
				m_pGrid[i-Margin+j*m_Width] |= BTILE_LHOLE;
		}
		for(int i = m_Width-1; i >= Margin ; i--)
		{
			if(m_pGrid[i-Margin+j*m_Width] & BTILE_SAFE)
				break;
			else
				m_pGrid[i+j*m_Width] |= BTILE_RHOLE;
		}
		for(int j = m_Height - 2; j >= 0; j--)
		{
			for(int i = 0; i < m_Width; i++)
			{
				int Index = m_pTiles[i+j*m_Width].m_Reserved;

				if(Index <= TILE_NOHOOK)
				{
					m_pGrid[i+j*m_Width] = Index + GTILE_AIR - TILE_AIR;
				}
				else if(Index >= ENTITY_OFFSET + ENTITY_FLAGSTAND_RED && Index < ENTITY_OFFSET + NUM_ENTITIES)
				{
					m_pGrid[i+j*m_Width] = Index + GTILE_FLAGSTAND_RED - ENTITY_FLAGSTAND_RED - ENTITY_OFFSET;
					if(m_pGrid[i+j*m_Width] <= GTILE_FLAGSTAND_BLUE)
						m_aFlagStandPos[m_pGrid[i+j*m_Width]] = vec2(i,j)*32+vec2(16,16);
				}
				else
				{
					m_pGrid[i+j*m_Width] = GTILE_AIR;
				}

				if(m_pGrid[i+j*m_Width] == GTILE_SOLID || m_pGrid[i+j*m_Width] == GTILE_NOHOOK)
					m_pGrid[i+j*m_Width] |= BTILE_SAFE;
				else if(m_pGrid[i+j*m_Width] == GTILE_DEATH)
					m_pGrid[i+j*m_Width] |= BTILE_HOLE;
				else
				{
					bool Left = i > 0 && m_pGrid[i-1+(j+1)*m_Width] & BTILE_SAFE && !(m_pGrid[i+(j+1)*m_Width] & BTILE_RHOLE) && (m_pGrid[i-1+(j+1)*m_Width]&GTILE_MASK) <= GTILE_AIR;
					bool Right = i < m_Width - 1 && m_pGrid[i+1+(j+1)*m_Width] & BTILE_SAFE && !(m_pGrid[i+(j+1)*m_Width] & BTILE_LHOLE) && (m_pGrid[i+1+(j+1)*m_Width]&GTILE_MASK) <= GTILE_AIR;

					if(Left || Right || m_pGrid[i+(j+1)*m_Width] & BTILE_SAFE)
						m_pGrid[i+j*m_Width] |= BTILE_SAFE;
					else
						m_pGrid[i+j*m_Width] |= BTILE_HOLE;
				}
			}
			for(int i = Margin; i < m_Width; i++)
			{
				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR))
					break;
				else
					m_pGrid[i-Margin+j*m_Width] |= BTILE_LHOLE;
			}
			for(int i = m_Width-1; i >= Margin ; i--)
			{
				if(!((m_pGrid[i-Margin+j*m_Width] & GTILE_MASK) <= GTILE_AIR))
					break;
				else
					m_pGrid[i+j*m_Width] |= BTILE_RHOLE;
			}
		}

		GenerateCorners();
		GenerateSegments();
		GenerateTriangles();

		GenerateGraphFromTriangles();

		for(int c = 0; c < MAX_CLIENTS; c++)
		{
			m_aPaths[c].m_MaxSize = m_Graph.Diameter()+3;
			m_aPaths[c].m_pVertices = (vec2*) mem_alloc(m_aPaths[c].m_MaxSize*sizeof(vec2),1);
			m_aPaths[c].m_pSnapID = (int*) mem_alloc(m_aPaths[c].m_MaxSize*sizeof(int),1);
			for(int i = 0 ; i < m_aPaths[c].m_MaxSize; i++)
				m_aPaths[c].m_pSnapID[i] = GameServer()->Server()->SnapNewID();
			m_aPaths[c].m_Size = 0;
		}
	}
}

void CBotEngine::GenerateSegments()
{
	int VSegmentCount = 0;
	int HSegmentCount = 0;
	// Vertical segments
	for(int i = 0; i < m_Width; i++)
	{
		bool started = false;
		int last = 0;
		for(int j = 0; j < m_Height; j++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				started = false;
			else if(IsCorner(i,j))
			{
				if(started)
				{
					CSegment Segment = {
						.m_IsVertical = true,
						.m_A = vec2(i,last),
						.m_B = vec2(i,j)
					};
					m_lSegments.add(Segment);
					VSegmentCount++;
				}
				else
					started = true;
				last = j;
			}
			else if(!IsOnEdge(i,j))
				started = false;
		}
	}
	// Horizontal segments
	for(int j = 0; j < m_Height; j++)
	{
		bool started = false;
		int last = 0;
		for(int i = 0; i < m_Width; i++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				started = false;
			else if(IsCorner(i,j))
			{
				if(started)
				{
					CSegment Segment = {
						.m_IsVertical = false,
						.m_A = vec2(last,j),
						.m_B = vec2(i,j)
					};
					m_lSegments.add(Segment);
					HSegmentCount++;
				}
				else
					started = true;
				last = i;
			}
			else if(!IsOnEdge(i,j))
				started = false;
		}
	}
	dbg_msg("botengine","Found %d vertical segments, and %d horizontal segments", VSegmentCount, HSegmentCount);
}

bool CBotEngine::IsCorner(int i, int j) const
{
	if( i < 0 || i >= m_Width || j < 0 || j >= m_Height)
		return false;
	if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
		return false;
	int n = 0;
	for(int k = 0; k < 8; k++)
	{
		int Index = clamp(i+g_Neighboors[k][0], 0, m_Width-1)+clamp(j+g_Neighboors[k][1], 0, m_Height-1)*m_Width;
		if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
			n += g_PowerTwo[k];
	}
	return g_IsInnerCorner[n] || g_IsOuterCorner[n];
}

bool CBotEngine::IsOnEdge(int i, int j) const
{
	if( i < 0 || i >= m_Width || j < 0 || j >= m_Height)
		return false;
	if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
		return false;
	int n = 0;
	for(int k = 0; k < 8; k++)
	{
		int Index = clamp(i+g_Neighboors[k][0], 0, m_Width-1)+clamp(j+g_Neighboors[k][1], 0, m_Height-1)*m_Width;
		if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
			n += g_PowerTwo[k];
	}
	return g_IsOnEdge[n];
}

void CBotEngine::GenerateCorners()
{
	int CornerCount = 0;
	for(int i = 1;i < m_Width - 1; i++)
		for(int j = 1; j < m_Height - 1; j++)
			if(IsCorner(i,j))
				CornerCount++;

	dbg_msg("botengine","Found %d corners", CornerCount);
	m_pCorners = (vec2*)mem_alloc((CornerCount+3)*sizeof(vec2),1);
	m_CornerCount = CornerCount;
	int m = 0;
	for(int i = 1;i < m_Width - 1; i++)
		for(int j = 1; j < m_Height - 1; j++)
			if(IsCorner(i,j))
				m_pCorners[m++] = vec2(i, j);
}

void CBotEngine::GenerateTriangles()
{
	m_Triangulation.m_CornerCount = m_CornerCount;
	m_Triangulation.m_pTriangles = (CTriangulation::CTriangleData*)mem_alloc(2*m_CornerCount*sizeof(CTriangulation::CTriangleData),1);

	m_Triangulation.m_Size = 0;

	for (int i = 0; i < m_CornerCount - 2; i++)
	{
		for (int j = i + 1; j < m_CornerCount - 1; j++)
		{
			if(FastIntersectLine(m_pCorners[i].x+m_pCorners[i].y*m_Width,m_pCorners[j].x+m_pCorners[j].y*m_Width))
				continue;
			for (int k = j + 1; k < m_CornerCount; k++)
			{
				if(FastIntersectLine(m_pCorners[i].x+m_pCorners[i].y*m_Width,m_pCorners[k].x+m_pCorners[k].y*m_Width) || FastIntersectLine(m_pCorners[j].x+m_pCorners[j].y*m_Width,m_pCorners[k].x+m_pCorners[k].y*m_Width))
					continue;
				CTriangle triangle(m_pCorners[i], m_pCorners[j], m_pCorners[k]);
				if(triangle.IsFlat())
					continue;
				vec2 cc = triangle.OuterCircleCenter();
				float radius = distance(m_pCorners[i], cc);

				bool found = false;
				for(int w = 0 ; w < m_Triangulation.m_Size; w++)
				{
					if(triangle.Intersects(m_Triangulation.m_pTriangles[w].m_Triangle))
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;
				for (int w = 0; w < m_CornerCount; w++)
				{
					if (w == i || w == j || w == k)
						continue;
					if (distance(cc, m_pCorners[w]) < radius)
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_Triangle = triangle;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[0] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[1] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[2] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_Size++;
			}
		}
	}
	for (int i = 0; i < m_CornerCount - 2; i++)
	{
		for (int j = i + 1; j < m_CornerCount - 1; j++)
		{
			if(FastIntersectLine(m_pCorners[i].x+m_pCorners[i].y*m_Width,m_pCorners[j].x+m_pCorners[j].y*m_Width))
				continue;
			for (int k = j + 1; k < m_CornerCount; k++)
			{
				if(FastIntersectLine(m_pCorners[i].x+m_pCorners[i].y*m_Width,m_pCorners[k].x+m_pCorners[k].y*m_Width) || FastIntersectLine(m_pCorners[j].x+m_pCorners[j].y*m_Width,m_pCorners[k].x+m_pCorners[k].y*m_Width))
					continue;
				CTriangle triangle(m_pCorners[i], m_pCorners[j], m_pCorners[k]);
				if(triangle.IsFlat())
					continue;

				bool found = false;
				for(int w = 0 ; w < m_Triangulation.m_Size; w++)
				{
					if(triangle.Intersects(m_Triangulation.m_pTriangles[w].m_Triangle) || (triangle.m_aPoints[0] == m_Triangulation.m_pTriangles[w].m_Triangle.m_aPoints[0] && triangle.m_aPoints[1] == m_Triangulation.m_pTriangles[w].m_Triangle.m_aPoints[1] && triangle.m_aPoints[2] == m_Triangulation.m_pTriangles[w].m_Triangle.m_aPoints[2]))
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;
				for (int w = 0; w < m_CornerCount; w++)
				{
					if (w == i || w == j || w == k)
						continue;
					if (triangle.InsideOrSide(m_pCorners[w]))
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_Triangle = triangle;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[0] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[1] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[2] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_Size++;
			}
		}
	}
	dbg_msg("botengine","Build %d triangles", m_Triangulation.m_Size);
}

void CBotEngine::GenerateGraphFromTriangles()
{
	for (int i = 0; i < m_Triangulation.m_Size; i++)
		m_Graph.AddVertex(m_Triangulation.m_pTriangles[i].m_Triangle.Centroid()*32 + vec2(16,16));
	for (int i = 0; i < m_Triangulation.m_Size; i++)
	{
		for (int j = 0; j < m_Triangulation.m_Size; j++)
		{
			int count[] = {0,0,0};
			for(int k = 0 ; k < 3; k++)
				for(int l = 0; l < 3; l++)
					if(m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[k] == m_Triangulation.m_pTriangles[j].m_Triangle.m_aPoints[l])
						count[k] = 1;
			if(count[0] + count[1] + count[2] == 2)
			{
				if(i < j)
				{
					int k = m_Graph.AddEdge(i,j);
					if(k >= 0)
						m_Graph.SetEdgeData(k, GameServer()->Server()->SnapNewID());
					vec2 A = (count[0]) ? m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[0] : m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[1];
					vec2 B = (count[2]) ? m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[2] : m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[1];
					k = m_Graph.AddEdge(A*32 + vec2(16,16),B*32 + vec2(16,16));
					if(k >= 0)
						m_Graph.SetEdgeData(k, GameServer()->Server()->SnapNewID());
				}
			}
		}
	}
	for (int i = 0; i < m_Triangulation.m_Size; i++)
	{
		vec2 Center = m_Triangulation.m_pTriangles[i].m_Triangle.Centroid()*32 + vec2(16,16);
		for (int j = 0 ; j < 3 ; j++)
		{
			vec2 P = m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[j]*32 + vec2(16,16);
			int k = m_Graph.AddEdge(Center, P);
			if(k >= 0)
				m_Graph.SetEdgeData(k, GameServer()->Server()->SnapNewID());
		}
	}
	for (int i = 0; i < m_lSegments.size(); i++)
	{
		const CSegment & Segment = m_lSegments[i];
		int k = m_Graph.AddEdge(Segment.m_A*32 + vec2(16,16), Segment.m_B*32 + vec2(16,16));
		if(k >= 0)
			m_Graph.SetEdgeData(k, GameServer()->Server()->SnapNewID());
	}
	dbg_msg("botengine","Graph with %d vertices and %d edges", m_Graph.NumVertices(), m_Graph.NumEdges());
	m_Graph.ComputeClosestPath();
}

int CBotEngine::GetTile(int x, int y) const
{
	x = clamp(x,0,m_Width-1);
	y = clamp(y,0,m_Height-1);
	return m_pGrid[y*m_Width+x];
}

int CBotEngine::GetTile(vec2 Pos) const
{
	return GetTile(round_to_int(Pos.x/32), round_to_int(Pos.y/32));
}

int CBotEngine::ConvertFromIndex(vec2 Pos) const
{
	return clamp(round_to_int(Pos.x/32), 0, m_Width-1)+clamp(round_to_int(Pos.y/32), 0, m_Width-1)*m_Width;
}

int CBotEngine::FastIntersectLine(int Id1, int Id2) const
{
	int i1 = Id1%m_Width;
	int i2 = Id2%m_Width;
	int j1 = Id1/m_Width;
	int j2 = Id2/m_Width;
	int di = abs(i2-i1);
	int dj = abs(j2-j1);
	int i = 0;
	int j = 0;
	int si = (i2>i1) ? 1: (i2<i1) ? -1 : 0;
	int sj = (j2>j1) ? 1: (j2<j1) ? -1 : 0;
	int Neigh[4] = {Id1 + si, Id2 - si, Id1 + sj*m_Width, Id2 - sj*m_Width};
	for(int k = 0; k < 4; k++)
		if((GetTile(Neigh[k]) & GTILE_MASK) > GTILE_AIR)
			return GetTile(Neigh[k]);
	while(i <= di && j <= dj)
	{
		if((GetTile(i*si+i1, j*sj+j1) & GTILE_MASK) > GTILE_AIR)
			return GetTile(i*si+i1, j*sj+j1);
		if(abs((j+1)*di-i*dj) < abs(j*di-(i+1)*dj))
			j++;
		else
			i++;
	}
	return 0;
}

void CBotEngine::GetPath(vec2 VStart, vec2 VEnd, CPath *pPath) const
{
	pPath->m_Size = m_Graph.GetPath(GetClosestVertex(VStart),GetClosestVertex(VEnd),pPath->m_pVertices+1);
	pPath->m_pVertices[0] = VStart;
	pPath->m_pVertices[pPath->m_Size+1] = VEnd;
	pPath->m_Size = pPath->m_Size+2;
	SmoothPath(pPath);
}

void CBotEngine::SmoothPath(CPath *pPath) const
{
	vec2 NullVec(0,0);
	for(int i = 0; i < pPath->m_Size; i++) {
		if (NullVec.x > pPath->m_pVertices[i].x)
			NullVec.x = pPath->m_pVertices[i].x - 1;
		if (NullVec.y > pPath->m_pVertices[i].y)
			NullVec.y = pPath->m_pVertices[i].y - 1;
	}
	bool Smoothed = true;
	for(int It = 0; It < g_Config.m_SvBotSmoothPath && Smoothed && pPath->m_Size > 2; It++)
	{
		Smoothed = false;
		for(int i = 0; i < pPath->m_Size-2; i++)
		{
			vec2 W = direction(angle(normalize(pPath->m_pVertices[i+2]-pPath->m_pVertices[i]))+pi/2)*15.f;
			if(!(GameServer()->Collision()->FastIntersectLine(pPath->m_pVertices[i]-W,pPath->m_pVertices[i+2]-W,0,0)) && !(GameServer()->Collision()->FastIntersectLine(pPath->m_pVertices[i]+W,pPath->m_pVertices[i+2]+W,0,0)))
			{
				pPath->m_pVertices[i+1] = NullVec;
				i++;
				Smoothed = true;
			}
		}
		if(Smoothed)
		{
			int Size = 0;
			for(int i = 0; i < pPath->m_Size; i++)
			{
				if(pPath->m_pVertices[i] != NullVec)
				{
					pPath->m_pVertices[Size] = pPath->m_pVertices[i];
					Size++;
				}
			}
			pPath->m_Size = Size;
		}
	}
}

bool CBotEngine::NextPoint(vec2 Pos, vec2 Target, vec2* NewPoint) const
{
	return m_Graph.GetNextInPath(GetClosestVertex(Pos), GetClosestVertex(Target), NewPoint);
}

// Need something smarter
double CBotEngine::FarestPointOnEdge(CPath *pPath, vec2 Pos, vec2 *pTarget) const
{
	for(int k = pPath->m_Size-1 ; k >=0 ; k--)
	{
		double D = distance(Pos, pPath->m_pVertices[k]);
		if( D < 10000.f)
		{
			vec2 VertexPos = pPath->m_pVertices[k];
			vec2 W = direction(angle(normalize(VertexPos-Pos))+pi/2)*15.f;
			if(!(GameServer()->Collision()->FastIntersectLine(Pos-W,VertexPos-W,0,0)) && !(GameServer()->Collision()->FastIntersectLine(Pos+W,VertexPos+W,0,0)))
			//if(!(FastIntersectLine(ConvertFromIndex(Pos),ConvertFromIndex(VertexPos))))
			{
				if(pTarget)
					*pTarget = VertexPos;
				return D;
			}
		}
	}
	return -1;
}


vec2 CBotEngine::GetClosestVertex(vec2 Pos) const
{
	int i = 0;
	double d = 1000.f;
	vec2 pt = Pos / 32;
	for(int k = 0; k < m_Triangulation.m_Size; k++)
	{
		if(m_Triangulation.m_pTriangles[k].m_Triangle.InsideOrSide(pt))
			return m_Graph.GetVertex(k);
		double dist = distance(m_Graph.GetVertex(k),Pos);
		if(dist < d)
		{
			d = dist;
			i = k;
		}
	}
	return m_Graph.GetVertex(i);
}

int CBotEngine::NetworkClipped(int SnappingClient, vec2 CheckPos) const
{
	if(SnappingClient == -1)
		return 1;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if(distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 1100.0f)
		return 1;
	return 0;
}

void CBotEngine::Snap(int SnappingClient) const
{
	for(int k = 0; k < 0 && m_Triangulation.m_Size; k++)
	{
		for(int i = 0 ; i < 3; i++)
		{
			if(NetworkClipped(SnappingClient, m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[i]*32) && NetworkClipped(SnappingClient, m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[(i+1)%3]*32))
				continue;
			CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(GameServer()->Server()->SnapNewItem(NETOBJTYPE_LASER, m_Triangulation.m_pTriangles[k].m_aSnapID[i], sizeof(CNetObj_Laser)));
			if(!pObj)
				return;
			pObj->m_X = m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[i].x*32+16;
			pObj->m_Y = m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[i].y*32+16;
			pObj->m_FromX = m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[(i+1)%3].x*32+16;
			pObj->m_FromY = m_Triangulation.m_pTriangles[k].m_Triangle.m_aPoints[(i+1)%3].y*32+16;
			pObj->m_StartTick = GameServer()->Server()->Tick();
		}
	}
	for(int k = 0; k < m_Graph.NumEdges(); k++)
	{
		CGraph::CEdge Edge = m_Graph.GetEdge(k);

		vec2 From = m_Graph.GetVertex(Edge.m_Start);
		vec2 To = m_Graph.GetVertex(Edge.m_End);
		if(NetworkClipped(SnappingClient, To) && NetworkClipped(SnappingClient, From))
			continue;
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(GameServer()->Server()->SnapNewItem(NETOBJTYPE_LASER, Edge.m_Data, sizeof(CNetObj_Laser)));
		if(!pObj)
			return;
		pObj->m_X = (int) To.x;
		pObj->m_Y = (int) To.y;
		pObj->m_FromX = (int) From.x;
		pObj->m_FromY = (int) From.y;
		pObj->m_StartTick = GameServer()->Server()->Tick();
	}
}
