#include <game/gamecore.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/layers.h>
#include "gamecontext.h"

#include "botengine.h"

CEdge::CEdge()
{
	m_Size = 0;
	m_pPath = 0;
	m_pSnapID = 0;
}
CEdge::~CEdge()
{
	//Free();
}
void CEdge::Reset()
{
	Free();
	m_Size = 0;
}
void CEdge::Free()
{
	if(m_pPath)
		mem_free(m_pPath);
	if(m_pSnapID)
		mem_free(m_pSnapID);
	m_pPath = 0;
}

CGraph::CGraph()
{
	m_NumVertices = 0;
	m_NumEdges = 0;
	m_pEdges = 0;
	m_pVertices = 0;
}
CGraph::~CGraph()
{
	Free();
}
void CGraph::Reset()
{
	Free();
	m_NumVertices = 0;
	m_NumEdges = 0;
}
void CGraph::Free()
{
	if(m_pEdges)
	{
		for(int i = 0 ; i < m_NumEdges ; i++)
			m_pEdges[i].Free();
		mem_free(m_pEdges);
	}
	if(m_pVertices)
		mem_free(m_pVertices);
	if(m_pClosestPath)
		mem_free(m_pClosestPath);
	if(m_ppAdjacency)
		mem_free(m_ppAdjacency);
	m_pVertices = 0;
	m_pEdges = 0;
	m_pClosestPath = 0;
	m_ppAdjacency = 0;
}

void CGraph::ComputeClosestPath()
{
	if(!m_pEdges or !m_pVertices)
		return;
	if(m_pClosestPath)
		mem_free(m_pClosestPath);
	if(m_ppAdjacency)
		mem_free(m_ppAdjacency);
	m_pClosestPath = (int*)mem_alloc(m_NumVertices*m_NumVertices*sizeof(int),1);
	if(!m_pClosestPath)
		return;
	m_ppAdjacency = (CEdge**)mem_alloc(m_NumVertices*m_NumVertices*sizeof(CEdge*),1);
	if(!m_ppAdjacency)
		return;
	mem_zero(m_ppAdjacency, m_NumVertices*m_NumVertices*sizeof(CEdge*));
	int* Dist = (int*)mem_alloc(m_NumVertices*m_NumVertices*sizeof(int),1);
	if(!Dist)
		return;
	for(int i = 0 ; i < m_NumVertices*m_NumVertices ; i++)
	{
		m_pClosestPath[i] = -1;
		Dist[i] = 1000000;
	}
	for(int i=0; i < m_NumVertices ; i++)
		Dist[i*(1+m_NumVertices)] = 0;
	for(int i=0; i < m_NumEdges ; i++)
	{
		int Start, End;
		for(Start=0; Start < m_NumVertices and m_pVertices[Start].m_Pos != m_pEdges[i].m_Start; Start++);
		for(End=0; End < m_NumVertices and m_pVertices[End].m_Pos != m_pEdges[i].m_End; End++);
		if(m_pEdges[i].m_Size < Dist[Start + End * m_NumVertices])
			Dist[Start + End * m_NumVertices] = m_pEdges[i].m_Size;
		m_pClosestPath[Start + End * m_NumVertices] = End;
		m_ppAdjacency[Start + End * m_NumVertices] = m_pEdges+i;
		dbg_msg("botengine", "edge %d (%d, %d)", i, Start, End);
	}

	for(int k = 0; k < m_NumVertices ; k++)
	{
		for(int i = 0; i < m_NumVertices ; i++)
		{
			for(int j = 0; j < m_NumVertices ; j++)
			{
				if(Dist[i + k * m_NumVertices] + Dist[k + j * m_NumVertices] < Dist[i + j * m_NumVertices])
				{
					Dist[i + j * m_NumVertices] = Dist[i + k * m_NumVertices] + Dist[k + j * m_NumVertices];
					m_pClosestPath[i + j * m_NumVertices] = m_pClosestPath[i + k * m_NumVertices];
				}
			}
		}
	}
	mem_free(Dist);
	dbg_msg("botengine", "closest path done");
}

// CBotEngine::CEdge CBotEngine::CGraph::GetPartialPath(CEdge *EStart, int IdS, CEdge *EEnd, int IdE)
// {
// 	CEdge Edge;
// 	int Start, End;
// 	for(Start=0; Start < m_NumVertices and m_pVertices[Start].m_Index != VStart; Start++);
// 	for(End=0; End < m_NumVertices and m_pVertices[End].m_Index != VEnd; End++);
// 	if(Start == m_NumVertices or End == m_NumVertices)
// 		return Edge;
// 	int Size = 0;
// 	int Cur = Start;
// 	while(Cur != End)
// 	{
// 		Size += m_ppAdjacency[Cur + m_pClosestPath[Cur + End * m_NumVertices]*m_NumVertices]->m_Size;
// 		Cur = m_pClosestPath[Cur + End * m_NumVertices];
// 	}
// 	Edge.m_Start = VStart;
// 	Edge.m_End = VEnd;
// 	Edge.m_Size = Size;
// 	Edge.m_pPath = (int*) mem_alloc(Size*sizeof(int),1);
// 	int i = 0;
// 	Cur = Start;
// 	while(Cur != End)
// 	{
// 		int Next = m_pClosestPath[Cur + End * m_NumVertices];
// 		for(int j = 0; j < m_ppAdjacency[Cur + Next*m_NumVertices]->m_Size ; j++)
// 			Edge.m_pPath[i++] = m_ppAdjacency[Cur + Next*m_NumVertices]->m_pPath[j];
// 		Cur = Next;
// 	}
// 	return Edge;
// }

CEdge CGraph::GetPath(vec2 VStart, vec2 VEnd)
{
	CEdge Edge;
	int Start, End;
	for(Start=0; Start < m_NumVertices and m_pVertices[Start].m_Pos != VStart; Start++);
	for(End=0; End < m_NumVertices and m_pVertices[End].m_Pos != VEnd; End++);
	if(Start == m_NumVertices or End == m_NumVertices)
		return Edge;
	int Size = 0;
	int Cur = Start;
	while(Cur != End)
	{
		Size += m_ppAdjacency[Cur + m_pClosestPath[Cur + End * m_NumVertices]*m_NumVertices]->m_Size;
		Cur = m_pClosestPath[Cur + End * m_NumVertices];
	}
	Edge.m_Start = VStart;
	Edge.m_End = VEnd;
	Edge.m_Size = Size;
	Edge.m_pPath = (vec2*) mem_alloc(Size*sizeof(vec2),1);

	int i = 0;
	Cur = Start;
	while(Cur != End)
	{
		int Next = m_pClosestPath[Cur + End * m_NumVertices];
		for(int j = 0; j < m_ppAdjacency[Cur + Next*m_NumVertices]->m_Size ; j++)
			Edge.m_pPath[i++] = m_ppAdjacency[Cur + Next*m_NumVertices]->m_pPath[j];
		Cur = Next;
	}

	return Edge;
}


CBotEngine::CBotEngine(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pGrid = 0;
	m_Triangulation.m_pTriangles = 0;
	m_Triangulation.m_Size = 0;
	m_pCorners = 0;
	m_CornerCount = 0;
}

CBotEngine::~CBotEngine()
{
	if(m_pGrid)
		mem_free(m_pGrid);
	if(m_Triangulation.m_pTriangles)
		mem_free(m_Triangulation.m_pTriangles);
		if(m_pCorners) mem_free(m_pCorners);
	for(int k = 0; k < m_Graph.m_NumEdges; k++)
	{
		CEdge *pEdge = m_Graph.m_pEdges + k;
		for(int l = 1 ; l < pEdge->m_Size ; l++)
			GameServer()->Server()->SnapFreeID(pEdge->m_pSnapID[l]);
	}
}

void CBotEngine::Init(CTile *pTiles, int Width, int Height)
{
	m_pTiles = pTiles;

	m_Width = Width;
	m_Height = Height;

	if(m_pGrid)	mem_free(m_pGrid);
	if(m_Triangulation.m_pTriangles) mem_free(m_Triangulation.m_pTriangles);
	m_Triangulation.m_Size = 0;
	if(m_pCorners) mem_free(m_pCorners);
	m_CornerCount = 0;

	for(int k = 0; k < m_Graph.m_NumEdges; k++)
	{
		CEdge *pEdge = m_Graph.m_pEdges + k;
		for(int l = 1 ; l < pEdge->m_Size ; l++)
			GameServer()->Server()->SnapFreeID(pEdge->m_pSnapID[l]);
	}
	m_Graph.Free();

	m_pGrid = (int *)mem_alloc(m_Width*m_Height*sizeof(int),1);
	if(m_pGrid)
	{
		mem_zero(m_pGrid, m_Width*m_Height*sizeof(char));

		int j = m_Height-1;
		int Margin = 6;
		int MarginH = 3;
		for(int i = 0; i < m_Width; i++)
		{
			int Index = m_pTiles[i+j*m_Width].m_Reserved;

			if(Index <= TILE_NOHOOK)
			{
				m_pGrid[i+j*m_Width] = Index + GTILE_AIR - TILE_AIR;
			}
			if(Index >= ENTITY_OFFSET + ENTITY_FLAGSTAND_RED and Index < ENTITY_OFFSET + NUM_ENTITIES)
			{
				m_pGrid[i+j*m_Width] = Index + GTILE_FLAGSTAND_RED - ENTITY_FLAGSTAND_RED - ENTITY_OFFSET;
			}

			if(m_pGrid[i+j*m_Width] == GTILE_SOLID or m_pGrid[i+j*m_Width] == GTILE_NOHOOK)
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
				else if(Index >= ENTITY_OFFSET + ENTITY_FLAGSTAND_RED and Index < ENTITY_OFFSET + NUM_ENTITIES)
				{
					m_pGrid[i+j*m_Width] = Index + GTILE_FLAGSTAND_RED - ENTITY_FLAGSTAND_RED - ENTITY_OFFSET;
				}
				else
				{
					m_pGrid[i+j*m_Width] = GTILE_AIR;
				}

				if(m_pGrid[i+j*m_Width] == GTILE_SOLID or m_pGrid[i+j*m_Width] == GTILE_NOHOOK)
					m_pGrid[i+j*m_Width] |= BTILE_SAFE;
				else if(m_pGrid[i+j*m_Width] == GTILE_DEATH)
					m_pGrid[i+j*m_Width] |= BTILE_HOLE;
				else
				{
					bool Left = i > 0 and m_pGrid[i-1+(j+1)*m_Width] & BTILE_SAFE and !(m_pGrid[i+(j+1)*m_Width] & BTILE_RHOLE) and (m_pGrid[i-1+(j+1)*m_Width]&GTILE_MASK) <= GTILE_AIR;
					bool Right = i < m_Width - 1 and m_pGrid[i+1+(j+1)*m_Width] & BTILE_SAFE and !(m_pGrid[i+(j+1)*m_Width] & BTILE_LHOLE) and (m_pGrid[i+1+(j+1)*m_Width]&GTILE_MASK) <= GTILE_AIR;

					if(Left or Right or m_pGrid[i+(j+1)*m_Width] & BTILE_SAFE)
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
		GenerateTriangles();

		GenerateGraphFromTriangles();

		// if(m_Width < 1000)
		// {
		// 	char line[1000] = " ";
		// 	for(int j = 1; j < m_Height - 1; j++)
		// 	{
		// 		for(int i = 1;i < m_Width - 1; i++)
		// 		{
		// 			if(m_pGrid[i+j*m_Width] & BTILE_SAFE)
		// 				line[i] = '0';
		// 			else if(m_pGrid[i+j*m_Width] & BTILE_RHOLE)
		// 				line[i] = 'R';
		// 			else if(m_pGrid[i+j*m_Width] & BTILE_LHOLE)
		// 				line[i] = 'L';
		// 			else line[i] = ' ';
		// 			line[i] = (GTILE_MASK&m_pGrid[i+j*m_Width])+'g';
		// 		}
		// 		dbg_msg("bot_engine", line);
		// 	}
		// }
		// Homotopic thinning
	// 	const int size = m_Width*m_Height;
	// 	int *Q1 = (int *)mem_alloc(size*sizeof(int),1);
	// 	int *Q2 = (int *)mem_alloc(size*sizeof(int),1);
	// 	if(Q1 and Q2)
	// 	{
	// 		dbg_msg("bot_engine", "start homotopic thinning");
	// 		mem_zero(Q1, size*sizeof(char));
	// 		mem_zero(Q2, size*sizeof(char));
	// 		int size1 = 0;
	// 		int size2 = 0;
	// 		for(int i = 0; i < m_Width; i++)
	// 		{
	// 			for(int j = 0; j < m_Height; j++)
	// 			{
	// 				if((m_pGrid[i+j*m_Width]&GTILE_MASK) < GTILE_WEAPON_SHOTGUN)
	// 					m_pGrid[i+j*m_Width] |= GTILE_SKELETON;// | GTILE_ANCHOR;
	// 				else if((m_pGrid[i+j*m_Width]&GTILE_MASK) < GTILE_AIR)
	// 					m_pGrid[i+j*m_Width] |= GTILE_SKELETON | GTILE_ANCHOR;
	// 				else if(i == 0 or i == m_Width-1 or j == 0 or j == m_Height-1 or (m_pGrid[i+j*m_Width]&GTILE_MASK) != GTILE_AIR)
	// 					m_pGrid[i+j*m_Width] |= GTILE_REMOVED;
	// 				else
	// 					m_pGrid[i+j*m_Width] |= GTILE_SKELETON;
	// 				if((m_pGrid[i+j*m_Width]&GTILE_MASK) <= GTILE_FLAGSTAND_BLUE)
	// 				{
	// 					m_pGrid[i+j*m_Width] |= GTILE_ANCHOR;
	// 					m_aFlagTiles[m_pGrid[i+j*m_Width]&GTILE_MASK] = i+j*m_Width;
	// 				}
	// 			}
	// 		}
	// 		// Remove anchor packets
	// 		bool modified = false;
	// 		while(modified)
	// 		{
	// 			modified = false;
	// 			for(int i = 1;i < m_Width - 1; i++)
	// 			{
	// 				for(int j = 1; j < m_Height - 1; j++)
	// 				{
	// 					if(!(m_pGrid[i+j*m_Width] & GTILE_ANCHOR))
	// 						continue;
	// 					int n = 0;
	// 					for(int k = 0; k < 8; k++)
	// 					{
	// 						int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
	// 						if(Index & GTILE_ANCHOR)
	// 							n += g_PowerTwo[k];
	// 					}
	// 					if(g_IsRemovable[n])
	// 					{
	// 						m_pGrid[i+j*m_Width] ^= GTILE_ANCHOR;
	// 						modified = true;
	// 					}
	//
	// 				}
	// 			}
	// 		}
	//
	// 		// Remove big rectangle of void
	// 		int WidthLeft = m_Width;
	// 		int WidthRight = 0;
	// 		for(j = 0; j < m_Height -MarginH; j++)
	// 		{
	// 			for(int i = Margin; i < WidthLeft; i++)
	// 			{
	// 				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j+MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
	// 				{
	// 					WidthLeft = i;
	// 					break;
	// 				}
	//
	// 				m_pGrid[i-Margin+j*m_Width] &= ~GTILE_SKELETON;
	// 				m_pGrid[i-Margin+j*m_Width] |= GTILE_REMOVED;
	// 			}
	// 			for(int i = m_Width-Margin-1; i >= WidthRight; i--)
	// 			{
	// 				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j+MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
	// 				{
	// 					WidthRight = i;
	// 					break;
	// 				}
	//
	// 				m_pGrid[i+Margin+j*m_Width] &= ~GTILE_SKELETON;
	// 				m_pGrid[i+Margin+j*m_Width] |= GTILE_REMOVED;
	// 			}
	// 		}
	// 		WidthLeft = m_Width;
	// 		WidthRight = 0;
	// 		for(j = m_Height-2; j >= MarginH; j--)
	// 		{
	// 			for(int i = Margin; i < WidthLeft; i++)
	// 			{
	// 				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j-MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
	// 				{
	// 					WidthLeft = i;
	// 					break;
	// 				}
	//
	// 				m_pGrid[i-Margin+j*m_Width] &= ~GTILE_SKELETON;
	// 				m_pGrid[i-Margin+j*m_Width] |= GTILE_REMOVED;
	// 			}
	// 			for(int i = m_Width-Margin-1; i >= WidthRight; i--)
	// 			{
	// 				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j-MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
	// 				{
	// 					WidthRight = i;
	// 					break;
	// 				}
	//
	// 				m_pGrid[i+Margin+j*m_Width] &= ~GTILE_SKELETON;
	// 				m_pGrid[i+Margin+j*m_Width] |= GTILE_REMOVED;
	// 			}
	// 		}
	// 		for(int i = m_Width-1; i >= 0 ; i--)
	// 		{
	// 			if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR))
	// 				break;
	// 			else
	// 				m_pGrid[i+j*m_Width] |= BTILE_RHOLE;
	// 		}
	// 		for(int i = 1;i < m_Width - 1; i++)
	// 		{
	// 			for(int j = 1; j < m_Height - 1; j++)
	// 			{
	// 				if(m_pGrid[i+j*m_Width] & (GTILE_REMOVED | GTILE_ANCHOR))
	// 					continue;
	// 				int n = 0;
	// 				for(int k = 0; k < 8; k++)
	// 				{
	// 					int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
	// 					if(Index & GTILE_SKELETON)
	// 						n += g_PowerTwo[k];
	// 				}
	// 				if(g_IsRemovable[n])
	// 					Q1[size1++] = i+j*m_Width;
	// 			}
	// 		}
	// 		while(size1)
	// 		{
	// 			// Q = emptyset
	// 			// for all points p in P
	// 			//	 if (p is simple for X)
	// 			//		 X = X \ {p}
	// 			//		 for all q in N(p)
	// 			//				Q = Q + {q}
	// 			while(size1)
	// 			{
	// 				size1--;
	// 				int i = Q1[size1] % m_Width;
	// 				int j = Q1[size1] / m_Width;
	// 				int n = 0;
	// 				for(int k = 0; k < 8; k++)
	// 				{
	// 					int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
	// 					if(Index & GTILE_SKELETON)
	// 						n += g_PowerTwo[k];
	// 				}
	// 				if(g_IsRemovable[n])
	// 				{
	// 					m_pGrid[Q1[size1]] &= ~GTILE_SKELETON;
	// 					m_pGrid[Q1[size1]] |= GTILE_REMOVED;
	// 					for(int k = 0; k < 8; k++)
	// 					{
	// 						int id = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
	// 						if(!(m_pGrid[id] & (GTILE_WAIT | GTILE_REMOVED | GTILE_ANCHOR)))
	// 						{
	// 							Q2[size2++] = id;
	// 							m_pGrid[id] |= GTILE_WAIT;
	// 						}
	// 					}
	// 				}
	// 			}
	// 			// P = emptyset
	// 			// for all points p in Q
	// 			//	 if (p is simple for X)
	// 			//		 P = P+ {p}
	// 			while(size2)
	// 			{
	// 				size2--;
	// 				int i = Q2[size2] % m_Width;
	// 				int j = Q2[size2] / m_Width;
	// 				m_pGrid[Q2[size2]] &= ~GTILE_WAIT;
	// 				int n = 0;
	// 				for(int k = 0; k < 8; k++)
	// 				{
	// 					int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
	// 					if(Index & GTILE_SKELETON)
	// 						n += g_PowerTwo[k];
	// 				}
	// 				if(g_IsRemovable[n])
	// 					Q1[size1++] = Q2[size2];
	// 			}
	// 		}
	// 		int anchor = 0;
	// 		int skel = 0;
	// 		int edges = 0;
	// 		for(int i = 1;i < m_Width - 1; i++)
	// 		{
	// 			for(int j = 1; j < m_Height - 1; j++)
	// 			{
	// 				if(m_pGrid[i+j*m_Width] & GTILE_REMOVED)
	// 					continue;
	// 				int n = 0;
	// 				skel++;
	// 				for(int k = 1; k < 8; k+=2)
	// 				{
	// 					int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
	// 					if(Index & GTILE_SKELETON)
	// 						n += 1;
	// 				}
	// 				if(n > 2 or m_pGrid[i+j*m_Width] & GTILE_ANCHOR)
	// 				{
	// 					m_pGrid[i+j*m_Width] |= GTILE_ANCHOR;
	// 					edges += n; //g_ConnectedComponents[n];
	// 					anchor++;
	// 				}
	// 			}
	// 		}
	// 		if(m_Width < 1000)
	// 		{
	// 			char line[1000] = " ";
	// 			for(int j = 1; j < m_Height - 1; j++)
	// 			{
	// 				for(int i = 1;i < m_Width - 1; i++)
	// 				{
	// 					if(m_pGrid[i+j*m_Width] & GTILE_REMOVED)
	// 						line[i] = ' ';
	// 					if(m_pGrid[i+j*m_Width] & GTILE_SKELETON)
	// 						line[i] = 'O';
	// 					if(m_pGrid[i+j*m_Width] & GTILE_ANCHOR)
	// 						line[i] = 'X';
	// 				}
	// 				dbg_msg("bot_engine", line);
	// 			}
	// 		}
	//
	// 		//Build Graph
	// 		if(true)
	// 		{
	// 			m_Graph.m_Width = m_Width;
	// 			m_Graph.m_NumVertices = anchor;
	// 			m_Graph.m_NumEdges = edges;
	// 			m_Graph.m_pVertices = (CVertex*)mem_alloc(m_Graph.m_NumVertices*sizeof(CVertex), 1);
	// 			m_Graph.m_pEdges = (CEdge*)mem_alloc(2*m_Graph.m_NumEdges*sizeof(CEdge), 1);
	// 			CVertex *pVertex = m_Graph.m_pVertices;
	// 			CEdge *pEdge = m_Graph.m_pEdges;
	// 			edges = 0;
	// 			for(int i = 1;i < m_Width - 1; i++)
	// 			{
	// 				for(int j = 1; j < m_Height - 1; j++)
	// 				{
	// 					if(!(m_pGrid[i+j*m_Width] & GTILE_ANCHOR))
	// 						continue;
	// 					pVertex->m_Degree = 0;
	// 					pVertex->m_Index = i+j*m_Width;
	// 					for(int k = 1; k < 8; k+=2)
	// 					{
	// 						int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
	// 						if(m_pGrid[Index] & GTILE_SKELETON)
	// 						{
	// 							int dir = k;
	// 							int PrevVertex = pVertex->m_Index;
	// 							int CurVertex = Index;
	// 							int Size = 2;
	// 							int MaxSize = 200;
	// 							bool modified = true;
	// 							while(!(m_pGrid[CurVertex] & GTILE_ANCHOR) and modified and Size < MaxSize)
	// 							{
	// 								modified = false;
	// 								for(int i = 1; i < 8; i += 2)
	// 								{
	// 									int id = CurVertex + g_Neighboors[i][0] + g_Neighboors[i][1]*m_Width;
	// 									if(m_pGrid[id] & GTILE_SKELETON and PrevVertex != id)
	// 									{
	// 										modified = true;
	// 										PrevVertex = CurVertex;
	// 										CurVertex = id;
	// 										if(dir != i)
	// 										{
	// 											Size++;
	// 											dir = i;
	// 										}
	// 										break;
	// 									}
	// 								}
	// 							}
	// 							if(!modified)
	// 							{
	// 								char aBuf[256];
	// 								str_format(aBuf, sizeof(aBuf), "deadend: start=(%d,%d) next=(%d,%d) end=(%d,%d)", pVertex->m_Index%m_Width, pVertex->m_Index/m_Width, Index%m_Width, Index/m_Width, CurVertex%m_Width, CurVertex/m_Width);
	// 								dbg_msg("bot_engine", aBuf);
	// 							}
	//
	//
	// 							pEdge->m_Start = pVertex->m_Index;
	// 							pEdge->m_End = CurVertex;
	// 							pEdge->m_Size = Size;
	// 							pEdge->m_pPath = (int*)mem_alloc(Size*sizeof(int),1);
	// 							pEdge->m_pSnapID = (int*)mem_alloc(Size*sizeof(int),1);
	//
	// 							pEdge->m_pPath[0] = pVertex->m_Index;
	// 							pEdge->m_pPath[1] = Index;
	// 							pEdge->m_pSnapID[1] = GameServer()->Server()->SnapNewID();
	// 							dir = k;
	// 							CurVertex = Index;
	// 							int j = 1;
	// 							while(pEdge->m_pPath[j] != pEdge->m_End)
	// 							{
	// 								for(int i = 1; i < 8; i+=2)
	// 								{
	// 									int id = pEdge->m_pPath[j] + g_Neighboors[i][0] + g_Neighboors[i][1]*m_Width;
	// 									if(m_pGrid[id] & GTILE_SKELETON and i != (dir+4) % 8)
	// 									{
	// 										if(dir != i)
	// 										{
	// 											dir = i;
	// 											j++;
	// 											pEdge->m_pSnapID[j] = GameServer()->Server()->SnapNewID();
	// 										}
	// 										pEdge->m_pPath[j] = id;
	// 										break;
	// 									}
	// 								}
	// 							}
	// 							++pEdge;
	// 							edges++;
	// 							pVertex->m_Degree++;
	// 						}
	// 					}
	// 					++pVertex;
	// 				}
	// 			}
	// 		}
	// 		m_Graph.m_NumEdges = edges;
	// 		m_Graph.ComputeClosestPath();
	// 		char aBuf[256];
	// 		str_format(aBuf, sizeof(aBuf), "anchor=%d skeleton=%d edges=%d", anchor, skel, edges);
	// 		dbg_msg("bot_engine", aBuf);
	//
	// 	}
	// 	else
	// 		dbg_msg("bot_engine", "can't do homotopic thinning");
	// 	if(Q1) mem_free(Q1);
	// 	if(Q2) mem_free(Q2);
	}
}

void CBotEngine::GenerateCorners()
{
	int CornerCount = 0;
	for(int i = 1;i < m_Width - 1; i++)
	{
		for(int j = 1; j < m_Height - 1; j++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				continue;
			int n = 0;
			for(int k = 0; k < 8; k++)
			{
				int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
				if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
					n += g_PowerTwo[k];
			}
			if(g_IsOuterCorner[n])
				CornerCount++;
		}
	}
	dbg_msg("botengine","Found %d outer corners", CornerCount);
	m_pCorners = (vec2*)mem_alloc(CornerCount*sizeof(vec2),1);
	m_CornerCount = CornerCount;
	int m = 0;
	for(int i = 1;i < m_Width - 1; i++)
	{
		for(int j = 1; j < m_Height - 1; j++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				continue;
			int n = 0;
			for(int k = 0; k < 8; k++)
			{
				int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
				if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
					n += g_PowerTwo[k];
			}
			if(g_IsOuterCorner[n])
				m_pCorners[m++] = ConvertIndex(i+j*m_Width);
		}
	}
}

void CBotEngine::GenerateTriangles()
{
	int CornerCount = 0;
	for(int i = 1;i < m_Width - 1; i++)
	{
		for(int j = 1; j < m_Height - 1; j++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				continue;
			int n = 0;
			for(int k = 0; k < 8; k++)
			{
				int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
				if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
					n += g_PowerTwo[k];
			}
			if(g_IsInnerCorner[n] || g_IsOuterCorner[n])
				CornerCount++;
		}
	}
	dbg_msg("botengine","Found %d corners", CornerCount);
	m_Triangulation.m_pTriangles = (CTriangulation::CTriangleData*)mem_alloc(4*CornerCount*sizeof(CTriangulation::CTriangleData),1);
	vec2 *Corners = (vec2*)mem_alloc((CornerCount+3)*sizeof(vec2),1);
	int m = 0;
	for(int i = 1;i < m_Width - 1; i++)
	{
		for(int j = 1; j < m_Height - 1; j++)
		{
			if((m_pGrid[i+j*m_Width] & GTILE_MASK) > GTILE_AIR)
				continue;
			int n = 0;
			for(int k = 0; k < 8; k++)
			{
				int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
				if((m_pGrid[Index] & GTILE_MASK) > GTILE_AIR)
					n += g_PowerTwo[k];
			}
			if(g_IsInnerCorner[n] || g_IsOuterCorner[n])
				Corners[m++] = vec2(i, j);
		}
	}
	vec2 BL = Corners[0], TR = Corners[0];
	for(int i = 1; i < CornerCount ; i++)
	{
		vec2 c = Corners[i];
		if(c.x < BL.x) BL.x = c.x;
		if(c.y < BL.y) BL.y = c.y;
		if(c.x > TR.x) TR.x = c.x;
		if(c.y > TR.y) TR.y = c.y;
	}
	Corners[CornerCount] = BL;
	Corners[CornerCount+1].x = 2*TR.x-BL.x;
	Corners[CornerCount+1].y = BL.y;
	Corners[CornerCount+2].x = BL.x;
	Corners[CornerCount+2].y = 2*TR.y-BL.y;

	m_Triangulation.m_Size = 0;
	// CTriangle triangle(Corners[CornerCount], Corners[CornerCount+1], Corners[CornerCount+2]);
	// m_Triangulation.m_pTriangles[m_Triangulation.m_Size++].m_Triangle = triangle;
	for (int i = 0; i < CornerCount - 2; i++)
	{
		for (int j = i + 1; j < CornerCount - 1; j++)
		{
			if(FastIntersectLine(Corners[i].x+Corners[i].y*m_Width,Corners[j].x+Corners[j].y*m_Width))
				continue;
			for (int k = j + 1; k < CornerCount; k++)
			{
				if(FastIntersectLine(Corners[i].x+Corners[i].y*m_Width,Corners[k].x+Corners[k].y*m_Width) || FastIntersectLine(Corners[j].x+Corners[j].y*m_Width,Corners[k].x+Corners[k].y*m_Width))
					continue;
				CTriangle triangle(Corners[i], Corners[j], Corners[k]);
				if(triangle.IsFlat())
					continue;
				vec2 cc = triangle.OuterCircleCenter();
				float radius = distance(Corners[i], cc);

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
				for (int w = 0; w < CornerCount; w++)
				{
					if (w == i || w == j || w == k)
						continue;
					if (distance(cc, Corners[w]) < radius)
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;

				vec2 a = triangle.CenterA();
				vec2 b = triangle.CenterB();
				vec2 c = triangle.CenterC();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_Triangle = triangle;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[0] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[1] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[2] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_Size++;
				dbg_msg("botengine","Found %de triangle with radius %f [(%f, %f) (%f, %f) (%f, %f)]", m_Triangulation.m_Size, radius, triangle.m_aPoints[0].x, triangle.m_aPoints[0].y, triangle.m_aPoints[1].x, triangle.m_aPoints[1].y, triangle.m_aPoints[2].x, triangle.m_aPoints[2].y);
			}
		}
	}
	for (int i = 0; i < CornerCount - 2; i++)
	{
		for (int j = i + 1; j < CornerCount - 1; j++)
		{
			if(FastIntersectLine(Corners[i].x+Corners[i].y*m_Width,Corners[j].x+Corners[j].y*m_Width))
				continue;
			for (int k = j + 1; k < CornerCount; k++)
			{
				if(FastIntersectLine(Corners[i].x+Corners[i].y*m_Width,Corners[k].x+Corners[k].y*m_Width) || FastIntersectLine(Corners[j].x+Corners[j].y*m_Width,Corners[k].x+Corners[k].y*m_Width))
					continue;
				CTriangle triangle(Corners[i], Corners[j], Corners[k]);
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
				for (int w = 0; w < CornerCount; w++)
				{
					if (w == i || w == j || w == k)
						continue;
					if (triangle.Inside(Corners[w]))
					{
						found = true;
						break;
					}
				}
				if (found)
					continue;

				vec2 a = triangle.CenterA();
				vec2 b = triangle.CenterB();
				vec2 c = triangle.CenterC();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_Triangle = triangle;

				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[0] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[1] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_pTriangles[m_Triangulation.m_Size].m_aSnapID[2] = GameServer()->Server()->SnapNewID();
				m_Triangulation.m_Size++;
				dbg_msg("botengine","Found %de triangle [(%f, %f) (%f, %f) (%f, %f)]", m_Triangulation.m_Size, triangle.m_aPoints[0].x, triangle.m_aPoints[0].y, triangle.m_aPoints[1].x, triangle.m_aPoints[1].y, triangle.m_aPoints[2].x, triangle.m_aPoints[2].y);
			}
		}
	}
	dbg_msg("botengine","Build %d triangles", m_Triangulation.m_Size);
	mem_free(Corners);
}

void CBotEngine::GenerateGraphFromTriangles()
{
	m_Graph.m_NumEdges = 0;
	for (int i = 0; i < m_Triangulation.m_Size - 1; i++)
	{
		for (int j = i + 1; j < m_Triangulation.m_Size; j++)
		{
			int count = 0;
			for(int k = 0 ; k < 3; k++)
				for(int l = 0; l < 3; l++)
					if(m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[k] == m_Triangulation.m_pTriangles[j].m_Triangle.m_aPoints[l])
						count++;
			if(count == 2)
				m_Graph.m_NumEdges+=2;
		}
	}
	m_Graph.m_Width = m_Width;
	m_Graph.m_NumVertices = m_Triangulation.m_Size;
	m_Graph.m_pVertices = (CVertex*)mem_alloc(m_Graph.m_NumVertices*sizeof(CVertex), 1);
	m_Graph.m_pEdges = (CEdge*)mem_alloc(m_Graph.m_NumEdges*sizeof(CEdge), 1);
	for (int i = 0; i < m_Triangulation.m_Size; i++)
		m_Graph.m_pVertices[i].m_Pos = m_Triangulation.m_pTriangles[i].m_Triangle.Centroid()*32 + vec2(16,16);
	CEdge *pEdge = m_Graph.m_pEdges;
	for (int i = 0; i < m_Triangulation.m_Size - 1; i++)
	{
		for (int j = i + 1; j < m_Triangulation.m_Size; j++)
		{
			int count = 0;
			for(int k = 0 ; k < 3; k++)
				for(int l = 0; l < 3; l++)
					if(m_Triangulation.m_pTriangles[i].m_Triangle.m_aPoints[k] == m_Triangulation.m_pTriangles[j].m_Triangle.m_aPoints[l])
						count++;
			if(count == 2)
			{
				pEdge->m_Start = m_Graph.m_pVertices[i].m_Pos;
				pEdge->m_End = m_Graph.m_pVertices[j].m_Pos;
				pEdge->m_Size = 2;
				pEdge->m_pPath = (vec2*)mem_alloc(2*sizeof(vec2),1);
				pEdge->m_pSnapID = (int*)mem_alloc(2*sizeof(int),1);
				pEdge->m_pPath[0] = pEdge->m_Start;
				pEdge->m_pPath[1] = pEdge->m_End;
				pEdge->m_pSnapID[0] = GameServer()->Server()->SnapNewID();
				pEdge->m_pSnapID[1] = GameServer()->Server()->SnapNewID();
				pEdge++;
				pEdge->m_Start = m_Graph.m_pVertices[j].m_Pos;
				pEdge->m_End = m_Graph.m_pVertices[i].m_Pos;
				pEdge->m_Size = 2;
				pEdge->m_pPath = (vec2*)mem_alloc(2*sizeof(vec2),1);
				pEdge->m_pSnapID = (int*)mem_alloc(2*sizeof(int),1);
				pEdge->m_pPath[0] = pEdge->m_Start;
				pEdge->m_pPath[1] = pEdge->m_End;
				pEdge->m_pSnapID[0] = GameServer()->Server()->SnapNewID();
				pEdge->m_pSnapID[1] = GameServer()->Server()->SnapNewID();
				pEdge++;
			}
		}
	}
	m_Graph.ComputeClosestPath();
}

int CBotEngine::GetTile(int x, int y)
{
	x = clamp(x,0,m_Width-1);
	y = clamp(y,0,m_Height-1);
	return m_pGrid[y*m_Width+x];
}

int CBotEngine::FastIntersectLine(int Id1, int Id2)
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

int CBotEngine::DistanceToEdge(CEdge Edge, vec2 Pos)
{
	if(!Edge.m_Size)
		return -1;

	int ClosestRange = -1;

	for(int i = 1 ; i < Edge.m_Size ; i++)
	{
		vec2 InterPos = closest_point_on_line(Edge.m_pPath[i-1],Edge.m_pPath[i], Pos);
		int D = distance(InterPos,Pos);
		if( D < ClosestRange || ClosestRange < 0)
		{
				ClosestRange = D;
		}
	}
	return ClosestRange;
}

int CBotEngine::FarestPointOnEdge(CEdge Edge, vec2 Pos, vec2 *pTarget)
{
	int x = round(Pos.x/32);
	int y = round(Pos.y/32);
	for(int k = Edge.m_Size-1 ; k >=0 ; k--)
	{
		int D = distance(Pos, Edge.m_pPath[k]);
		if( D < 100000)
		{
			vec2 VertexPos = Edge.m_pPath[k];
			vec2 W = direction(angle(normalize(VertexPos-Pos))+pi/2)*14.f;
			if(!(GameServer()->Collision()->IntersectLine(Pos-W,VertexPos-W,0,0)) and !(GameServer()->Collision()->IntersectLine(Pos+W,VertexPos+W,0,0)))
			{
				if(pTarget)
					*pTarget = VertexPos;
				return D;
			}
		}
	}
	return -1;
}

int CBotEngine::GetClosestEdge(vec2 Pos, int ClosestRange, CEdge *pEdge)
{
	CEdge* ClosestEdge = 0;

	int x = round(Pos.x/32);
	int y = round(Pos.y/32);

	for(int k = 0; k < m_Graph.m_NumEdges; k++)
	{
		CEdge *pE = m_Graph.m_pEdges + k;
		int D = DistanceToEdge(*pE, Pos);
		if(D < 0)
			continue;
		if(D < ClosestRange || ClosestRange < 0)
		{
			ClosestEdge = pE;
			ClosestRange = D;
		}
	}
	if(ClosestEdge)
	{
		if(pEdge)
		{
			*pEdge = *ClosestEdge;
			pEdge->m_pSnapID = 0;
			pEdge->m_pPath = (vec2*) mem_alloc(pEdge->m_Size*sizeof(vec2),1);
			mem_copy(pEdge->m_pPath, ClosestEdge->m_pPath, pEdge->m_Size*sizeof(int));
		}
		return ClosestRange;
	}
	return -1;
}

// CBotEngine::CEdge CBotEngine::GetClosestVertex(vec2 Pos, float ClosestRange)
// {
// 	CEdge* ClosestVertex = 0;
// 	int x = round(pMe->m_Pos.x/32);
// 	int y = round(pMe->m_Pos.y/32);
// 	for(int k = 0; k < m_Graph.m_NumVertices; k++)
// 	{
// 		CVertex *pVertex = m_Graph.m_pVertices + k;
// 		int Index = m_pVertices->m_Index;
// 		int i = Index%m_Width-x;
// 		int j = Index/m_Width-y;
// 		float D = i*i+j*j;
// 		if( D < ClosestRange)
// 		{
// 			vec2 VertexPos(Index % m_Width, Index / m_Width);
// 			VertexPos *= 32;
// 			VertexPos += vec2(16,16);
// 			vec2 W = direction(angle(normalize(VertexPos-Pos))+pi/2)*16.f;
// 			if(!(Collision()->IntersectLine(Pos-W,VertexPos-W,0,0)) and !(Collision()->IntersectLine(Pos+W,VertexPos+W,0,0)))
// 			{
// 				ClosestRange = D;
// 				ClosestVertex = pVertex;
// 			}
// 		}
// 	}
// 	return ClosestVertex;
// }

int CBotEngine::NetworkClipped(int SnappingClient, vec2 CheckPos)
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

void CBotEngine::Snap(int SnappingClient)
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
	for(int k = 0; k < m_Graph.m_NumEdges; k++)
	{
		CEdge *pEdge = m_Graph.m_pEdges + k;

		for(int l = 1 ; l < pEdge->m_Size ; l++)
		{
			vec2 From = pEdge->m_pPath[l-1];
			vec2 To = pEdge->m_pPath[l];
			if(NetworkClipped(SnappingClient, To) && NetworkClipped(SnappingClient, From))
				continue;
			CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(GameServer()->Server()->SnapNewItem(NETOBJTYPE_LASER, pEdge->m_pSnapID[l], sizeof(CNetObj_Laser)));
			if(!pObj)
				return;
			pObj->m_X = (int) To.x;
			pObj->m_Y = (int) To.y;
			pObj->m_FromX = (int) From.x;
			pObj->m_FromY = (int) From.y;
			pObj->m_StartTick = GameServer()->Server()->Tick();
		}
	}
}
