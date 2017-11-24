#include "StdAfx.h"
#include "TableFrameHook.h"


//////////////////////////////////////////////////////////////////////////

//静态变量
const WORD			CPersonalTableFrameHook::m_wPlayerCount=MAX_CHAIR;			//游戏人数

//////////////////////////////////////////////////////////////////////////

//构造函数
CPersonalTableFrameHook::CPersonalTableFrameHook()
{
	//组件变量
	m_pITableFrame=NULL;
	m_pGameServiceOption=NULL;
	m_pPersonalRoomEventSink=NULL;

	return;
}

//析构函数
CPersonalTableFrameHook::~CPersonalTableFrameHook(void)
{
}

//接口查询
void *  CPersonalTableFrameHook::QueryInterface(const IID & Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IPersonalTableFrameHook,Guid,dwQueryVer);
	QUERYINTERFACE(IPersonalTableFrameHook,Guid,dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IPersonalTableFrameHook,Guid,dwQueryVer);
	return NULL;
}

//
bool CPersonalTableFrameHook::SetPersonalRoomEventSink(IUnknownEx * pIUnknownEx)
{
	m_pPersonalRoomEventSink=QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx,IPersonalRoomEventSink);

	return true;
}

//初始化
bool  CPersonalTableFrameHook::InitTableFrameHook(IUnknownEx * pIUnknownEx)
{
	//查询接口
	ASSERT(pIUnknownEx!=NULL);
	m_pITableFrame=QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx,ITableFrame);
	if (m_pITableFrame==NULL) return false;

	//获取参数
	m_pGameServiceOption=m_pITableFrame->GetGameServiceOption();
	ASSERT(m_pGameServiceOption!=NULL);

	return true;
}

//游戏开始
bool CPersonalTableFrameHook::OnEventGameStart(WORD wChairCount)
{
	if(m_pPersonalRoomEventSink!=NULL)
	{
		return m_pPersonalRoomEventSink->OnEventGameStart(m_pITableFrame, wChairCount);
	}

	return false;
}

//游戏开始
void CPersonalTableFrameHook::PersonalRoomWriteJoinInfo(DWORD dwUserID, WORD wTableID,  WORD wChairID,   DWORD dwKindID, TCHAR * szRoomID,  TCHAR * szPersonalRoomGUID)
{
	if(m_pPersonalRoomEventSink!=NULL)
	{
		return m_pPersonalRoomEventSink->PersonalRoomWriteJoinInfo(dwUserID, wTableID,  wChairID,  dwKindID, szRoomID,szPersonalRoomGUID);
	}
}

//游戏结束
bool  CPersonalTableFrameHook::OnEventGameEnd(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason )
{
	if(m_pPersonalRoomEventSink!=NULL)
	{
		return m_pPersonalRoomEventSink->OnEventGameEnd(m_pITableFrame,wChairID,pIServerUserItem,cbReason);
	}
	
	return false;
}

//游戏结束
bool  CPersonalTableFrameHook::OnEventGameEnd(WORD wTableID,  WORD wChairCount, DWORD dwDrawCountLimit, DWORD & dwPersonalPlayCount, int nSpecialInfoLen,  byte * cbSpecialInfo, SYSTEMTIME sysStartTime, tagPersonalUserScoreInfo * PersonalUserScoreInfo )
{
	if(m_pPersonalRoomEventSink!=NULL)
	{
		return m_pPersonalRoomEventSink->OnEventGameEnd(wTableID,  wChairCount, dwDrawCountLimit, dwPersonalPlayCount, nSpecialInfoLen, cbSpecialInfo, sysStartTime , PersonalUserScoreInfo);
	}

	return false;
}


//用户坐下
bool CPersonalTableFrameHook::OnActionUserSitDown(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	if(m_pPersonalRoomEventSink!=NULL)  
	{
		return m_pPersonalRoomEventSink->OnActionUserSitDown(pIServerUserItem->GetTableID(), wChairID, pIServerUserItem, bLookonUser);
	}

	return false;
}

//用户起来
bool CPersonalTableFrameHook::OnActionUserStandUp(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	if(m_pPersonalRoomEventSink!=NULL) 
	{
		return m_pPersonalRoomEventSink->OnActionUserStandUp(pIServerUserItem->GetTableID(), wChairID, pIServerUserItem, bLookonUser);
	}

	return false;
}

//用户同意
bool CPersonalTableFrameHook::OnActionUserOnReady(WORD wChairID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize)
{
	if(m_pPersonalRoomEventSink!=NULL)
	{
		return m_pPersonalRoomEventSink->OnActionUserOnReady(pIServerUserItem->GetTableID(), wChairID, pIServerUserItem ,pData ,wDataSize);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
