/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <stdlib.h> // qsort

#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
	m_pSegments = 0;
	m_SegmentCount = 0;
	m_HSegmentCount = 0;
}

CCollision::~CCollision()
{
	if(m_pSegments)
		mem_free(m_pSegments);
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;
		m_pTiles[i].m_Reserved = Index;

		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}
	}

	BuildSegments();
}

void CCollision::BuildSegments()
{
	int VSegmentCount = 0;
	int HSegmentCount = 0;
	// We start counting segments
	// Horizontal segments
	for(int j = 1; j < m_Height-1; j++)
	{
		bool top = false;
		bool bottom = false;
		for(int i = 0; i < m_Width; i++)
		{
			// Segment on the bottom of line j
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+(j+1)*m_Width].m_Index <= 128 && m_pTiles[i+(j+1)*m_Width].m_Index & COLFLAG_SOLID))
				bottom = true;
			else if(bottom)
			{
				HSegmentCount++;
				bottom = false;
			}

			// Segment on the top of line j
			if(((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+(j-1)*m_Width].m_Index <= 128 && m_pTiles[i+(j-1)*m_Width].m_Index & COLFLAG_SOLID)))
				top = true;
			else if(top)
			{
				HSegmentCount++;
				top = false;
			}
		}
		if(bottom)
			HSegmentCount++;
		if(top)
			HSegmentCount++;
	}
	// Vertical segments
	for(int i = 1;i < m_Width-1; i++)
	{
		bool right = false;
		bool left = false;
		for(int j = 0; j < m_Height; j++)
		{
			// Segment on the right side of column i
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+1+j*m_Width].m_Index <= 128 && m_pTiles[i+1+j*m_Width].m_Index & COLFLAG_SOLID))
				right = true;
			else if(right)
			{
				VSegmentCount++;
				right = false;
			}

			// Segment on the left side of column i
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i-1+j*m_Width].m_Index <= 128 && m_pTiles[i-1+j*m_Width].m_Index & COLFLAG_SOLID))
				left = true;
			else if(left)
			{
				VSegmentCount++;
				left = false;
			}
		}
		if(right)
			VSegmentCount++;
		if(left)
			VSegmentCount++;
	}
	m_SegmentCount = HSegmentCount+VSegmentCount;
	m_HSegmentCount = HSegmentCount;
	if(m_pSegments)
		mem_free(m_pSegments);
	m_pSegments = (CSegment*) mem_alloc(m_SegmentCount*sizeof(CSegment),1);
	CSegment* pSegment = m_pSegments;

	// Horizontal segments
	for(int j = 1; j < m_Height-1; j++)
	{
		bool bottom = false;
		bool top = false;
		int bottom_i, top_i;
		for(int i = 0; i < m_Width; i++)
		{
			// Segment on the bottom of line j
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+(j+1)*m_Width].m_Index <= 128 && m_pTiles[i+(j+1)*m_Width].m_Index & COLFLAG_SOLID))
			{
				if(!bottom)
					bottom_i = i;
				bottom = true;
			}
			else if(bottom)
			{
				pSegment->m_IsVertical = false;
				if(!bottom_i)
					bottom_i = -200;
				pSegment->m_A = vec2(bottom_i*32,(j+1)*32);
				pSegment->m_B = vec2(i*32,(j+1)*32);
				pSegment++;
				bottom = false;
			}

			// Segment on the top of line j
			if(((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+(j-1)*m_Width].m_Index <= 128 && m_pTiles[i+(j-1)*m_Width].m_Index & COLFLAG_SOLID)))
			{
				if(!top)
					top_i = i;
				top = true;
			}
			else if(top)
			{
				pSegment->m_IsVertical = false;
				if(!top_i)
					top_i = -200;
				pSegment->m_A = vec2(top_i*32,j*32);
				pSegment->m_B = vec2(i*32,j*32);
				pSegment++;
				top = false;
			}
		}
		if(bottom)
		{
			pSegment->m_IsVertical = false;
			if(!bottom_i)
				bottom_i = -200;
			pSegment->m_A = vec2(bottom_i*32,(j+1)*32);
			pSegment->m_B = vec2((m_Width+200)*32,(j+1)*32);
			pSegment++;
			bottom = false;
		}
		if(top)
		{
			pSegment->m_IsVertical = false;
			if(!top_i)
				top_i = -200;
			pSegment->m_A = vec2(top_i*32,j*32);
			pSegment->m_B = vec2((m_Width+200)*32,j*32);
			pSegment++;
			top = false;
		}
	}
	// Vertical segments
	for(int i = 1;i < m_Width-1; i++)
	{
		bool left = false;
		bool right = false;
		int left_j, right_j;
		for(int j = 0; j < m_Height; j++)
		{
			// Segment on the right side of column i
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i+1+j*m_Width].m_Index <= 128 && m_pTiles[i+1+j*m_Width].m_Index & COLFLAG_SOLID))
			{
				if(!right)
					right_j = j;
				right = true;
			}
			else if(right)
			{
				pSegment->m_IsVertical = true;
				if(!right_j)
					right_j = -200;
				pSegment->m_A = vec2((i+1)*32,right_j*32);
				pSegment->m_B = vec2((i+1)*32,j*32);
				pSegment++;
				right = false;
			}

			// Segment on the left side of column i
			if((m_pTiles[i+j*m_Width].m_Index <= 128 && m_pTiles[i+j*m_Width].m_Index & COLFLAG_SOLID) && !(m_pTiles[i-1+j*m_Width].m_Index <= 128 && m_pTiles[i-1+j*m_Width].m_Index & COLFLAG_SOLID))
			{
				if(!left)
					left_j = j;
				left = true;
			}
			else if(left)
			{
				pSegment->m_IsVertical = true;
				if(!left_j)
					left_j = -200;
				pSegment->m_A = vec2(i*32,left_j*32);
				pSegment->m_B = vec2(i*32,j*32);
				pSegment++;
				left = false;
			}
		}
		if(right)
		{
			pSegment->m_IsVertical = true;
			if(!right_j)
				right_j = -200;
			pSegment->m_A = vec2((i+1)*32,right_j*32);
			pSegment->m_B = vec2((i+1)*32,(m_Height+200)*32);
			pSegment++;
		}
		if(left)
		{
			pSegment->m_IsVertical = true;
			if(!left_j)
				left_j = -200;
			pSegment->m_A = vec2(i*32,left_j*32);
			pSegment->m_B = vec2(i*32,(m_Height+200)*32);
			pSegment++;
		}
	}
	dbg_msg("collision","Allocate %d segments, use %d", m_SegmentCount, pSegment - m_pSegments);
	// Sort by y-coordinate and x-coordinate
	qsort(m_pSegments,HSegmentCount,sizeof(CSegment),SegmentComp);
	qsort(m_pSegments+HSegmentCount,VSegmentCount,sizeof(CSegment),SegmentComp);
}

int CCollision::SegmentComp(const void *a, const void *b)
{
	CSegment *s0 = (CSegment *)a;
	CSegment *s1 = (CSegment *)b;
	if(s0->m_IsVertical && !s1->m_IsVertical)
		return 1;
	if(!s0->m_IsVertical && s1->m_IsVertical)
		return -1;
	if(s0->m_IsVertical)
	{
		if(s0->m_A.x < s1->m_A.x)
			return -1;
		if(s0->m_A.x > s1->m_A.x)
			return 1;
		if(s0->m_A.y < s1->m_A.y)
			return -1;
		if(s0->m_A.y > s1->m_A.y)
			return 1;
		return 0;
	}
	if(s0->m_A.y < s1->m_A.y)
		return -1;
	if(s0->m_A.y > s1->m_A.y)
		return 1;
	if(s0->m_A.x < s1->m_A.x)
		return -1;
	if(s0->m_A.x > s1->m_A.x)
		return 1;
	return 0;
}

int CCollision::GetTile(int x, int y) const
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
}

bool CCollision::IsTileSolid(int x, int y) const
{
	return GetTile(x, y)&COLFLAG_SOLID;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i < End; i++)
	{
		float a = i/float(End);
		vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectSegment(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	// Check if the starting point is in a solid tile
	if(CheckPoint(Pos0))
	{
		if(pOutCollision)
			*pOutCollision = Pos0;
		if(pOutBeforeCollision)
			*pOutBeforeCollision = Pos0;
		return GetCollisionAt(Pos0.x, Pos0.y);
	}
	float T = 1;
	vec2 Pos;
	vec2 DiffPos = Pos1-Pos0;
	vec2 Dir = normalize(DiffPos);
	// Horizontal
	if(DiffPos.y)
	{
		float idy = 1.f/DiffPos.y;
		int i = 0, j = m_HSegmentCount;
		if(DiffPos.y > 0)
		{
			// Look for the first segment with y-coordinate >= Pos0.y
			while(i+1<j)
			{
				int k = (i+j)>>1;
				if(m_pSegments[k].m_A.y < Pos0.y)
					i = k;
				else
					j = k;
			}
			while(j < m_HSegmentCount && m_pSegments[j].m_A.y <= Pos1.y)
			{
				float d1 = det(m_pSegments[j].m_A - Pos0, DiffPos);
				float d2 = det(m_pSegments[j].m_B - Pos0, DiffPos);
				if(d1*d2 <= 0)
				{
					float Temp = (m_pSegments[j].m_A.y-Pos0.y)*idy;
					if(Temp < T)
					{
						T = Temp;
						break;
					}
				}
				j++;
			}
		}
		else // Pos0.y > Pos1.y
		{
			// Look for the last segment with y-coordinate <= Pos0.y
			while(i+1<j)
			{
				int k = (i+j)>>1;
				if(m_pSegments[k].m_A.y <= Pos0.y)
					i = k;
				else
					j = k;
			}
			while(i >= 0 && m_pSegments[i].m_A.y > Pos1.y)
			{
				float d1 = det(m_pSegments[i].m_A - Pos0, DiffPos);
				float d2 = det(m_pSegments[i].m_B - Pos0, DiffPos);
				if(d1*d2 <= 0)
				{
					float Temp = (m_pSegments[i].m_A.y-Pos0.y)*idy;
					if(Temp < T)
					{
						T = Temp;
						break;
					}
				}
				i--;
			}
		}
	}

	bool Vertical = false;
	// Vertical
	if(DiffPos.x)
	{
		float idx = 1.f/DiffPos.x;
		int i = m_HSegmentCount, j = m_SegmentCount;
		if(DiffPos.x > 0)
		{
			// Look for the first segment with x-coordinate >= Pos0.x
			while(i+1<j)
			{
				int k = (i+j)>>1;
				if(m_pSegments[k].m_A.x < Pos0.x)
					i = k;
				else
					j = k;
			}
			while(j < m_SegmentCount && m_pSegments[j].m_A.x <= Pos1.x)
			{
				float d1 = det(m_pSegments[j].m_A - Pos0, DiffPos);
				float d2 = det(m_pSegments[j].m_B - Pos0, DiffPos);
				if(d1*d2 <= 0)
				{
					float Temp = (m_pSegments[j].m_A.x-Pos0.x)*idx;
					if(Temp < T)
					{
						T = Temp;
						Vertical = true;
						break;
					}
				}
				j++;
			}
		}
		else // Pos1.x < Pos0.x
		{
			// Look for the last segment with x-coordinate <= Pos0.x
			while(i+1<j)
			{
				int k = (i+j)>>1;
				if(m_pSegments[k].m_A.x <= Pos0.x)
					i = k;
				else
					j = k;
			}
			while(i >= m_HSegmentCount && m_pSegments[i].m_A.x >= Pos1.x)
			{
				float d1 = det(m_pSegments[i].m_A - Pos0, DiffPos);
				float d2 = det(m_pSegments[i].m_B - Pos0, DiffPos);
				if(d1*d2 <= 0)
				{
					float Temp = (m_pSegments[i].m_A.x-Pos0.x)*idx;
					if(Temp < T)
					{
						T = Temp;
						Vertical = true;
						break;
					}
				}
				i--;
			}
		}
	}
	if(T < 1)
	{
		T = clamp(T,0.f,1.f);
		Pos = mix(Pos0,Pos1,T);
		// Return the real intersection, so it's a bit different from IntersectLine
		// that returns the first point that is in a solid tile.
		if(pOutCollision)
			*pOutCollision = Pos;

		// Get the tile...
		int Index = GetTile(Pos.x,Pos.y);
		if(!Index)
			Index = GetTile(Pos.x+Dir.x,Pos.y+Dir.y);
		if(!Index)
			Index = GetTile(Pos.x+ ((Vertical) ? Dir.x : -Dir.x),Pos.y+((Vertical) ? -Dir.y : Dir.y));

		if(Vertical)
			Dir *= 0.5f / absolute(Dir.x) + 1.f;
		else
			Dir *= 0.5f / absolute(Dir.y) + 1.f;
		// Only used for laser, previous point is compute by removing enough
		// distance so that laser is not stuck in the solid tile (due to MovePoint)
		if(pOutBeforeCollision)
			*pOutBeforeCollision = Pos - Dir;

		return Index;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size) const
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}
