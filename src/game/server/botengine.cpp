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
		for(Start=0; Start < m_NumVertices and m_pVertices[Start].m_Index != m_pEdges[i].m_Start; Start++);
		for(End=0; End < m_NumVertices and m_pVertices[End].m_Index != m_pEdges[i].m_End; End++);
		if(m_pEdges[i].m_Size < Dist[Start + End * m_NumVertices])
			Dist[Start + End * m_NumVertices] = m_pEdges[i].m_Size;
		m_pClosestPath[Start + End * m_NumVertices] = End;
		m_ppAdjacency[Start + End * m_NumVertices] = m_pEdges+i;
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

CEdge CGraph::GetPath(int VStart, int VEnd)
{
	CEdge Edge;
	int Start, End;
	for(Start=0; Start < m_NumVertices and m_pVertices[Start].m_Index != VStart; Start++);
	for(End=0; End < m_NumVertices and m_pVertices[End].m_Index != VEnd; End++);
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
	Edge.m_pPath = (int*) mem_alloc(Size*sizeof(int),1);

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
}

CBotEngine::~CBotEngine()
{
	if(m_pGrid)
		mem_free(m_pGrid);
}

void CBotEngine::Init(CTile *pTiles, int Width, int Height)
{
	m_pTiles = pTiles;

	m_Width = Width;
	m_Height = Height;

	if(m_pGrid)	mem_free(m_pGrid);

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
		const int size = m_Width*m_Height;
		int *Q1 = (int *)mem_alloc(size*sizeof(int),1);
		int *Q2 = (int *)mem_alloc(size*sizeof(int),1);
		if(Q1 and Q2)
		{
			dbg_msg("bot_engine", "start homotopic thinning");
			mem_zero(Q1, size*sizeof(char));
			mem_zero(Q2, size*sizeof(char));
			int size1 = 0;
			int size2 = 0;
			for(int i = 0; i < m_Width; i++)
			{
				for(int j = 0; j < m_Height; j++)
				{
					if((m_pGrid[i+j*m_Width]&GTILE_MASK) < GTILE_WEAPON_SHOTGUN)
						m_pGrid[i+j*m_Width] |= GTILE_SKELETON;// | GTILE_ANCHOR;
					else if((m_pGrid[i+j*m_Width]&GTILE_MASK) < GTILE_AIR)
						m_pGrid[i+j*m_Width] |= GTILE_SKELETON | GTILE_ANCHOR;
					else if(i == 0 or i == m_Width-1 or j == 0 or j == m_Height-1 or (m_pGrid[i+j*m_Width]&GTILE_MASK) != GTILE_AIR)
						m_pGrid[i+j*m_Width] |= GTILE_REMOVED;
					else
						m_pGrid[i+j*m_Width] |= GTILE_SKELETON;
					if((m_pGrid[i+j*m_Width]&GTILE_MASK) <= GTILE_FLAGSTAND_BLUE)
					{
						m_pGrid[i+j*m_Width] |= GTILE_ANCHOR;
						m_aFlagTiles[m_pGrid[i+j*m_Width]&GTILE_MASK] = i+j*m_Width;
					}
				}
			}
			// Remove anchor packets
			bool modified = false;
			while(modified)
			{
				modified = false;
				for(int i = 1;i < m_Width - 1; i++)
				{
					for(int j = 1; j < m_Height - 1; j++)
					{
						if(!(m_pGrid[i+j*m_Width] & GTILE_ANCHOR))
							continue;
						int n = 0;
						for(int k = 0; k < 8; k++)
						{
							int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
							if(Index & GTILE_ANCHOR)
								n += g_PowerTwo[k];
						}
						if(g_IsRemovable[n])
						{
							m_pGrid[i+j*m_Width] ^= GTILE_ANCHOR;
							modified = true;
						}

					}
				}
			}

			// Remove big rectangle of void
			int WidthLeft = m_Width;
			int WidthRight = 0;
			for(j = 0; j < m_Height -MarginH; j++)
			{
				for(int i = Margin; i < WidthLeft; i++)
				{
					if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j+MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
					{
						WidthLeft = i;
						break;
					}

					m_pGrid[i-Margin+j*m_Width] &= ~GTILE_SKELETON;
					m_pGrid[i-Margin+j*m_Width] |= GTILE_REMOVED;
				}
				for(int i = m_Width-Margin-1; i >= WidthRight; i--)
				{
					if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j+MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
					{
						WidthRight = i;
						break;
					}

					m_pGrid[i+Margin+j*m_Width] &= ~GTILE_SKELETON;
					m_pGrid[i+Margin+j*m_Width] |= GTILE_REMOVED;
				}
			}
			WidthLeft = m_Width;
			WidthRight = 0;
			for(j = m_Height-2; j >= MarginH; j--)
			{
				for(int i = Margin; i < WidthLeft; i++)
				{
					if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j-MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
					{
						WidthLeft = i;
						break;
					}

					m_pGrid[i-Margin+j*m_Width] &= ~GTILE_SKELETON;
					m_pGrid[i-Margin+j*m_Width] |= GTILE_REMOVED;
				}
				for(int i = m_Width-Margin-1; i >= WidthRight; i--)
				{
					if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR and (m_pGrid[i+(j-MarginH)*m_Width] & GTILE_MASK) <= GTILE_AIR))
					{
						WidthRight = i;
						break;
					}

					m_pGrid[i+Margin+j*m_Width] &= ~GTILE_SKELETON;
					m_pGrid[i+Margin+j*m_Width] |= GTILE_REMOVED;
				}
			}
			for(int i = m_Width-1; i >= 0 ; i--)
			{
				if(!((m_pGrid[i+j*m_Width] & GTILE_MASK) <= GTILE_AIR))
					break;
				else
					m_pGrid[i+j*m_Width] |= BTILE_RHOLE;
			}
			for(int i = 1;i < m_Width - 1; i++)
			{
				for(int j = 1; j < m_Height - 1; j++)
				{
					if(m_pGrid[i+j*m_Width] & (GTILE_REMOVED | GTILE_ANCHOR))
						continue;
					int n = 0;
					for(int k = 0; k < 8; k++)
					{
						int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
						if(Index & GTILE_SKELETON)
							n += g_PowerTwo[k];
					}
					if(g_IsRemovable[n])
						Q1[size1++] = i+j*m_Width;
				}
			}
			while(size1)
			{
				// Q = emptyset
				// for all points p in P
				//   if (p is simple for X)
				//     X = X \ {p}
				//     for all q in N(p)
				//        Q = Q + {q}
				while(size1)
				{
					size1--;
					int i = Q1[size1] % m_Width;
					int j = Q1[size1] / m_Width;
					int n = 0;
					for(int k = 0; k < 8; k++)
					{
						int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
						if(Index & GTILE_SKELETON)
							n += g_PowerTwo[k];
					}
					if(g_IsRemovable[n])
					{
						m_pGrid[Q1[size1]] &= ~GTILE_SKELETON;
						m_pGrid[Q1[size1]] |= GTILE_REMOVED;
						for(int k = 0; k < 8; k++)
						{
							int id = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
							if(!(m_pGrid[id] & (GTILE_WAIT | GTILE_REMOVED | GTILE_ANCHOR)))
							{
								Q2[size2++] = id;
								m_pGrid[id] |= GTILE_WAIT;
							}
						}
					}
				}
				// P = emptyset
				// for all points p in Q
				//   if (p is simple for X)
				//     P = P+ {p}
				while(size2)
				{
					size2--;
					int i = Q2[size2] % m_Width;
					int j = Q2[size2] / m_Width;
					m_pGrid[Q2[size2]] &= ~GTILE_WAIT;
					int n = 0;
					for(int k = 0; k < 8; k++)
					{
						int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
						if(Index & GTILE_SKELETON)
							n += g_PowerTwo[k];
					}
					if(g_IsRemovable[n])
						Q1[size1++] = Q2[size2];
				}
			}
			int anchor = 0;
			int skel = 0;
			int edges = 0;
			for(int i = 1;i < m_Width - 1; i++)
			{
				for(int j = 1; j < m_Height - 1; j++)
				{
					if(m_pGrid[i+j*m_Width] & GTILE_REMOVED)
						continue;
					int n = 0;
					skel++;
					for(int k = 1; k < 8; k+=2)
					{
						int Index = m_pGrid[i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width];
						if(Index & GTILE_SKELETON)
							n += 1;
					}
					if(n > 2 or m_pGrid[i+j*m_Width] & GTILE_ANCHOR)
					{
						m_pGrid[i+j*m_Width] |= GTILE_ANCHOR;
						edges += n; //g_ConnectedComponents[n];
						anchor++;
					}
				}
			}
			if(m_Width < 1000)
			{
				char line[1000] = " ";
				for(int j = 1; j < m_Height - 1; j++)
				{
					for(int i = 1;i < m_Width - 1; i++)
					{
						if(m_pGrid[i+j*m_Width] & GTILE_REMOVED)
							line[i] = ' ';
						if(m_pGrid[i+j*m_Width] & GTILE_SKELETON)
							line[i] = 'O';
						if(m_pGrid[i+j*m_Width] & GTILE_ANCHOR)
							line[i] = 'X';
					}
					dbg_msg("bot_engine", line);
				}
			}

			//Build Graph
			if(true)
			{
				m_Graph.m_NumVertices = anchor;
				m_Graph.m_NumEdges = edges;
				m_Graph.m_pVertices = (CVertex*)mem_alloc(m_Graph.m_NumVertices*sizeof(CVertex), 1);
				m_Graph.m_pEdges = (CEdge*)mem_alloc(2*m_Graph.m_NumEdges*sizeof(CEdge), 1);
				CVertex *pVertex = m_Graph.m_pVertices;
				CEdge *pEdge = m_Graph.m_pEdges;
				edges = 0;
				for(int i = 1;i < m_Width - 1; i++)
				{
					for(int j = 1; j < m_Height - 1; j++)
					{
						if(!(m_pGrid[i+j*m_Width] & GTILE_ANCHOR))
							continue;
						pVertex->m_Degree = 0;
						pVertex->m_Index = i+j*m_Width;
						for(int k = 1; k < 8; k+=2)
						{
							int Index = i+g_Neighboors[k][0]+(j+g_Neighboors[k][1])*m_Width;
							if(m_pGrid[Index] & GTILE_SKELETON)
							{
								int PrevVertex = pVertex->m_Index;
								int CurVertex = Index;
								int Size = 2;
								int MaxSize = 200;
								bool modified = true;
								while(!(m_pGrid[CurVertex] & GTILE_ANCHOR) and modified and Size < MaxSize)
								{
									modified = false;
									for(int i = 1; i < 8; i += 2)
									{
										int id = CurVertex + g_Neighboors[i][0] + g_Neighboors[i][1]*m_Width;
										if(m_pGrid[id] & GTILE_SKELETON and PrevVertex != id)
										{
											modified = true;
											PrevVertex = CurVertex;
											CurVertex = id;
											Size++;
											break;
										}
									}
								}
								if(!modified)
								{
									char aBuf[256];
									str_format(aBuf, sizeof(aBuf), "deadend: start=(%d,%d) next=(%d,%d) end=(%d,%d)", pVertex->m_Index%m_Width, pVertex->m_Index/m_Width, Index%m_Width, Index/m_Width, CurVertex%m_Width, CurVertex/m_Width);
									dbg_msg("bot_engine", aBuf);
								}


								pEdge->m_Start = pVertex->m_Index;
								pEdge->m_End = CurVertex;
								pEdge->m_Size = Size;
								pEdge->m_pPath = (int*)mem_alloc(Size*sizeof(int),1);

								pEdge->m_pPath[0] = pVertex->m_Index;
								pEdge->m_pPath[1] = Index;

								for(int j = 1 ; j < Size-1 ; j++)
								{
									for(int i = 1; i < 8; i+=2)
									{
										int id = pEdge->m_pPath[j] + g_Neighboors[i][0] + g_Neighboors[i][1]*m_Width;
										if(m_pGrid[id] & GTILE_SKELETON and pEdge->m_pPath[j-1] != id)
										{
											pEdge->m_pPath[j+1] = id;
											break;
										}
									}
								}
								++pEdge;
								edges++;
								pVertex->m_Degree++;
							}
						}
						++pVertex;
					}
				}
			}
			m_Graph.m_NumEdges = edges;
			m_Graph.ComputeClosestPath();
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "anchor=%d skeleton=%d edges=%d", anchor, skel, edges);
			dbg_msg("bot_engine", aBuf);

		}
		else
			dbg_msg("bot_engine", "can't do homotopic thinning");
		if(Q1) mem_free(Q1);
		if(Q2) mem_free(Q2);
	}
}

int CBotEngine::GetTile(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return m_pGrid[Ny*m_Width+Nx];
}

CEdge* CBotEngine::GetClosestEdge(vec2 Pos, float ClosestRange)
{
	CEdge* ClosestEdge = 0;

	int x = round(Pos.x/32);
	int y = round(Pos.y/32);

	for(int k = 0; k < m_Graph.m_NumEdges; k++)
	{
		CEdge *pEdge = m_Graph.m_pEdges + k;
		for(int l = 0 ; l < pEdge->m_Size ; l++)
		{
			int Index = pEdge->m_pPath[l];
			int i = Index%m_Width-x;
			int j = Index/m_Width-y;
			float D = i*i+j*j;
			if( D < ClosestRange)
			{
				vec2 VertexPos(Index % m_Width, Index / m_Width);
				VertexPos *= 32;
				VertexPos += vec2(16,16);
				vec2 W = direction(angle(normalize(VertexPos-Pos))+pi/2)*16.f;
				if(!(GameServer()->Collision()->IntersectLine(Pos-W,VertexPos-W,0,0)) and !(GameServer()->Collision()->IntersectLine(Pos+W,VertexPos+W,0,0)))
				{
					ClosestRange = D;
					ClosestEdge = pEdge;
					break;
				}
			}
		}
	}
	return ClosestEdge;
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
