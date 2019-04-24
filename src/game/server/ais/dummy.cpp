#include "dummy.h"

CNetObj_PlayerInput CAIDummy::GetInput()
{
	CNetObj_PlayerInput Input = {0};
	Input.m_Direction = (m_pPlayer->GetCID()&1)?-1:1;
	return Input;
}