#include "StdAfx.h"
#include "PersonalRoomGame.h"
#include  "..\游戏服务器\TableFrame.h"
#include  "..\游戏服务器\AttemperEngineSink.h"


#define ZZMJ_KIND_ID  386
#define HZMJ_KIND_ID  389
#define ZJH_KIND_ID	  6
#define ZJH_MAX_PLAYER 5
#define NN_KIND_ID	  27
#define NN_MAX_PLAYER 4
#define TBZ_KIND_ID	  47
#define TBZ_MAX_PLAYER 4
#define SET_RULE			1
////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//常量定义
#define INVALID_VALUE				0xFFFF								//无效值

//时钟定义
#define IDI_SWITCH_STATUS			(IDI_MATCH_MODULE_START+1)					//切换状态
#define IDI_DISTRIBUTE_USER		    (IDI_MATCH_MODULE_START+2)					//分配用户
#define IDI_CHECK_START_SIGNUP		(IDI_MATCH_MODULE_START+3)					//开始报名
#define IDI_CHECK_END_SIGNUP		(IDI_MATCH_MODULE_START+4)					//开始截止
#define IDI_CHECK_START_MATCH		(IDI_MATCH_MODULE_START+5)					//开始时钟
#define IDI_CHECK_END_MATCH			(IDI_MATCH_MODULE_START+6)					//结束时钟

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//构造函数
CPersonalRoomGame::CPersonalRoomGame()
{

	m_pGameServiceOption=NULL;
	m_pGameServiceAttrib=NULL;

	//内核接口
	m_ppITableFrame=NULL;
	m_pITimerEngine=NULL;
	m_pIDBCorrespondManager=NULL;
	m_pITCPNetworkEngineEvent=NULL;

	//服务接口
	m_pIGameServiceFrame=NULL;
	m_pIServerUserManager=NULL;
	m_pAndroidUserManager=NULL;
}

CPersonalRoomGame::~CPersonalRoomGame(void)
{
	//释放资源
	SafeDeleteArray(m_ppITableFrame);

	//关闭定时器
	m_pITimerEngine->KillTimer(IDI_SWITCH_STATUS);
	m_pITimerEngine->KillTimer(IDI_CHECK_END_MATCH);
	m_pITimerEngine->KillTimer(IDI_DISTRIBUTE_USER);
	m_pITimerEngine->KillTimer(IDI_CHECK_START_SIGNUP);			

}

//接口查询
VOID* CPersonalRoomGame::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{	
	QUERYINTERFACE(IPersonalRoomItem,Guid,dwQueryVer);
	QUERYINTERFACE(IPersonalRoomEventSink,Guid,dwQueryVer);
	QUERYINTERFACE(IServerUserItemSink,Guid,dwQueryVer);	
	QUERYINTERFACE_IUNKNOWNEX(IPersonalRoomItem,Guid,dwQueryVer);
	return NULL;
}

//启动通知
void CPersonalRoomGame::OnStartService()
{

}

//绑定桌子
bool CPersonalRoomGame::BindTableFrame(ITableFrame * pTableFrame,WORD wTableID)
{
	if(pTableFrame==NULL || wTableID>m_pGameServiceOption->wTableCount)
	{
		ASSERT(false);
		return false;
	}

	//创建钩子
	CPersonalTableFrameHook * pTableFrameHook=new CPersonalTableFrameHook();
	pTableFrameHook->InitTableFrameHook(QUERY_OBJECT_PTR_INTERFACE(pTableFrame,IUnknownEx));
	pTableFrameHook->SetPersonalRoomEventSink(QUERY_OBJECT_PTR_INTERFACE(this,IUnknownEx));

	//设置接口
	pTableFrame->SetTableFrameHook(QUERY_OBJECT_PTR_INTERFACE(pTableFrameHook,IUnknownEx));
	m_ppITableFrame[wTableID]=pTableFrame;

	return true;
}

//初始化接口
bool CPersonalRoomGame::InitPersonalRooomInterface(tagPersonalRoomManagerParameter & PersonalRoomManagerParameter)
{
	//服务配置
	m_pPersonalRoomOption=PersonalRoomManagerParameter.pPersonalRoomOption;
	m_pGameServiceOption=PersonalRoomManagerParameter.pGameServiceOption;
	m_pGameServiceAttrib=PersonalRoomManagerParameter.pGameServiceAttrib;

	//内核组件
	m_pITimerEngine=PersonalRoomManagerParameter.pITimerEngine;
	m_pIDBCorrespondManager=PersonalRoomManagerParameter.pICorrespondManager;
	m_pITCPNetworkEngineEvent=PersonalRoomManagerParameter.pTCPNetworkEngine;
	m_pITCPNetworkEngine = PersonalRoomManagerParameter.pITCPNetworkEngine;
	m_pITCPSocketService = PersonalRoomManagerParameter.pITCPSocketService;

	//服务组件		
	m_pIGameServiceFrame=PersonalRoomManagerParameter.pIMainServiceFrame;		
	m_pIServerUserManager=PersonalRoomManagerParameter.pIServerUserManager;
	m_pAndroidUserManager=PersonalRoomManagerParameter.pIAndroidUserManager;
	m_pIServerUserItemSink=PersonalRoomManagerParameter.pIServerUserItemSink;

	//分组设置
	//m_DistributeManage.SetDistributeRule(m_pMatchOption->cbDistributeRule);

	//创建桌子
	if (m_ppITableFrame==NULL)
	{
		m_ppITableFrame=new ITableFrame*[m_pGameServiceOption->wTableCount];
	}	

	return true;
}

//时间事件
bool CPersonalRoomGame::OnEventTimer(DWORD dwTimerID, WPARAM dwBindParameter)
{	
	return true;
}

//数据库事件
bool CPersonalRoomGame::OnEventDataBase(WORD wRequestID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize,DWORD dwContextID)
{
	switch(wRequestID)
	{
	case DBO_GR_CREATE_SUCCESS:			//创建成功
		{
			return OnDBCreateSucess(dwContextID,pData,wDataSize, pIServerUserItem);
		}
	case DBO_GR_CREATE_FAILURE:			//创建失败
		{
			return OnDBCreateFailure(dwContextID,pData,wDataSize, pIServerUserItem);
		}
	case DBO_GR_CANCEL_CREATE_RESULT:		//取消创建
		{
			return OnDBCancelCreateTable(dwContextID,pData,wDataSize, pIServerUserItem);
		}
	case DBO_GR_LOAD_PERSONAL_ROOM_OPTION:
		{
			//ASSERT(wDataSize == sizeof(tagPersonalRoomOption));
			//if (sizeof(tagPersonalRoomOption)!=wDataSize) return false;
			//tagPersonalRoomOption * pPersonalRoomOption = (tagPersonalRoomOption *)pData;
			//memcpy(&m_PersonalRoomOption, pPersonalRoomOption, sizeof(tagPersonalRoomOption) );
			return true;
		}
	case DBO_GR_LOAD_PERSONAL_PARAMETER:
		{
			return OnDBLoadPersonalParameter(dwContextID, pData, wDataSize, pIServerUserItem);
		}
	case DBO_GR_DISSUME_TABLE_RESULTE:
		{
			return OnDBDissumeTableResult(dwContextID, pData, wDataSize, pIServerUserItem);
		}
	case DBO_GR_CURRENCE_ROOMCARD_AND_BEAN:
		{
			return OnDBCurrenceRoomCardAndBeant(dwContextID, pData, wDataSize, pIServerUserItem);
		}
	}
	return true;
}

//约战房事件
bool CPersonalRoomGame::OnEventSocketPersonalRoom(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem, DWORD dwSocketID)
{
	switch(wSubCmdID)
	{
	case SUB_GR_CREATE_TABLE:
		{
			return OnTCPNetworkSubCreateTable(pData, wDataSize, dwSocketID, pIServerUserItem);
		}
	case SUB_GR_CANCEL_REQUEST:
		{
			return OnTCPNetworkSubCancelRequest(pData, wDataSize, dwSocketID,  pIServerUserItem);
		}
	case SUB_GR_REQUEST_REPLY:
		{
			return OnTCPNetworkSubRequestReply(pData, wDataSize, dwSocketID,  pIServerUserItem);
		}
	case SUB_GR_HOSTL_DISSUME_TABLE://房主强制解散桌子
		{
			return OnTCPNetworkSubHostDissumeTable(pData, wDataSize, dwSocketID,  pIServerUserItem);
		}
	}

	return true;
}

//用户积分
bool CPersonalRoomGame::OnEventUserItemScore(IServerUserItem * pIServerUserItem,BYTE cbReason)
{
	//效验参数
	ASSERT(pIServerUserItem!=NULL);
	if (pIServerUserItem==NULL) return false;

	//变量定义
	CMD_GR_UserScore UserScore;
	ZeroMemory(&UserScore,sizeof(UserScore));
	tagUserInfo * pUserInfo=pIServerUserItem->GetUserInfo();

	//构造数据
	UserScore.dwUserID=pUserInfo->dwUserID;
	UserScore.UserScore.dwWinCount=pUserInfo->dwWinCount;
	UserScore.UserScore.dwLostCount=pUserInfo->dwLostCount;
	UserScore.UserScore.dwDrawCount=pUserInfo->dwDrawCount;
	UserScore.UserScore.dwFleeCount=pUserInfo->dwFleeCount;	
	UserScore.UserScore.dwExperience=pUserInfo->dwExperience;
	UserScore.UserScore.lLoveLiness=pUserInfo->lLoveLiness;
	UserScore.UserScore.lIntegralCount = pUserInfo->lIntegralCount;

	//构造积分
	UserScore.UserScore.lGrade=pUserInfo->lGrade;
	UserScore.UserScore.lInsure=pUserInfo->lInsure;
	UserScore.UserScore.lIngot=pUserInfo->lIngot;
	UserScore.UserScore.dBeans=pUserInfo->dBeans;

	//构造积分
	UserScore.UserScore.lScore=pUserInfo->lScore;
	UserScore.UserScore.lScore+=pIServerUserItem->GetTrusteeScore();
	UserScore.UserScore.lScore+=pIServerUserItem->GetFrozenedScore();

	//发送数据
	m_pIGameServiceFrame->SendData(BG_COMPUTER,MDM_GR_USER,SUB_GR_USER_SCORE,&UserScore,sizeof(UserScore));

	//变量定义
	CMD_GR_MobileUserScore MobileUserScore;
	ZeroMemory(&MobileUserScore,sizeof(MobileUserScore));

	//构造数据
	MobileUserScore.dwUserID=pUserInfo->dwUserID;
	MobileUserScore.UserScore.dwWinCount=pUserInfo->dwWinCount;
	MobileUserScore.UserScore.dwLostCount=pUserInfo->dwLostCount;
	MobileUserScore.UserScore.dwDrawCount=pUserInfo->dwDrawCount;
	MobileUserScore.UserScore.dwFleeCount=pUserInfo->dwFleeCount;
	MobileUserScore.UserScore.dwExperience=pUserInfo->dwExperience;
	MobileUserScore.UserScore.lIntegralCount = pUserInfo->lIntegralCount;

	//构造积分
	MobileUserScore.UserScore.lScore=pUserInfo->lScore;
	MobileUserScore.UserScore.lScore+=pIServerUserItem->GetTrusteeScore();
	MobileUserScore.UserScore.lScore+=pIServerUserItem->GetFrozenedScore();
	MobileUserScore.UserScore.dBeans=pUserInfo->dBeans;

	//发送数据
	m_pIGameServiceFrame->SendDataBatchToMobileUser(pIServerUserItem->GetTableID(),MDM_GR_USER,SUB_GR_USER_SCORE,&MobileUserScore,sizeof(MobileUserScore));


	//即时写分
	if (
		(CServerRule::IsImmediateWriteScore(m_pGameServiceOption->dwServerRule)==true)
		&&(pIServerUserItem->IsVariation()==true))
	{
		//变量定义
		DBR_GR_WriteGameScore WriteGameScore;
		ZeroMemory(&WriteGameScore,sizeof(WriteGameScore));

		//用户信息
		WriteGameScore.dwUserID=pIServerUserItem->GetUserID();
		WriteGameScore.dwDBQuestID=pIServerUserItem->GetDBQuestID();
		WriteGameScore.dwClientAddr=pIServerUserItem->GetClientAddr();
		WriteGameScore.dwInoutIndex=pIServerUserItem->GetInoutIndex();

		//提取积分
		pIServerUserItem->DistillVariation(WriteGameScore.VariationInfo);

		//私人房间分数数据结构
		DBR_GR_WritePersonalGameScore  WritePersonalScore;
		memcpy(&WritePersonalScore.VariationInfo, &WriteGameScore.VariationInfo, sizeof(WritePersonalScore.VariationInfo));

		//调整分数
		if(pIServerUserItem->IsAndroidUser()==true)
		{
			WriteGameScore.VariationInfo.lScore=0;
			WriteGameScore.VariationInfo.lGrade=0;
			WriteGameScore.VariationInfo.lInsure=0;
			WriteGameScore.VariationInfo.lRevenue=0;
		}


		if ((lstrcmp(m_pGameServiceOption->szDataBaseName,  TEXT("RYTreasureDB")) != 0))
		{
			WriteGameScore.VariationInfo.lScore=0;
			WriteGameScore.VariationInfo.lGrade=0;
			WriteGameScore.VariationInfo.lInsure=0;
			WriteGameScore.VariationInfo.lRevenue=0;
		}


		//投递请求
		m_pIDBCorrespondManager->PostDataBaseRequest(WriteGameScore.dwUserID,DBR_GR_WRITE_GAME_SCORE,0L,&WriteGameScore,sizeof(WriteGameScore), TRUE);


		//约战房间ID
		DWORD dwRoomHostID = 0;
		TCHAR szPersonalRoomID[ROOM_ID_LEN] = {0};
		//INT_PTR nSize = m_TableFrameArray.GetCount();
		for(INT_PTR i = 0; i < m_pGameServiceOption->wTableCount; ++i)
		{
			CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[i];
			if (pTableFrame)
			{
				if (pTableFrame->GetTableID() == pIServerUserItem->GetTableID())
				{
					lstrcpyn(szPersonalRoomID, pTableFrame->GetPersonalTableID(), sizeof(szPersonalRoomID));
					dwRoomHostID = pTableFrame->GetRecordTableOwner();
					wsprintf(WritePersonalScore.szPersonalRoomGUID, TEXT("%s"), pTableFrame->GetPersonalRoomGUID());
					WritePersonalScore.nGamesNum = pTableFrame->GetDrawCount();
				}
			}
		}

		//约战房间分数数据结构
		WritePersonalScore.dwRoomHostID = dwRoomHostID;
		WritePersonalScore.bTaskForward = WriteGameScore.bTaskForward;
		WritePersonalScore.dwClientAddr = WriteGameScore.dwClientAddr;
		WritePersonalScore.dwDBQuestID  = WriteGameScore.dwDBQuestID;
		WritePersonalScore.dwInoutIndex = WriteGameScore.dwInoutIndex;

		WritePersonalScore.dwUserID		= WriteGameScore.dwUserID;
		WritePersonalScore.dwPersonalRoomTax =static_cast<DWORD>(m_pPersonalRoomOption->lPersonalRoomTax);
		lstrcpyn(WritePersonalScore.szRoomID, szPersonalRoomID, sizeof(WritePersonalScore.szRoomID));	


		//请求数据库写入积分
		if (m_pGameServiceOption->wServerType == GAME_GENRE_PERSONAL)
		{
			m_pIDBCorrespondManager->PostDataBaseRequest(WritePersonalScore.dwUserID,DBR_GR_WRITE_PERSONAL_GAME_SCORE,0L,&WritePersonalScore,sizeof(WritePersonalScore), TRUE);
		}
	}

	//通知桌子
	if(pIServerUserItem->GetTableID()!=INVALID_TABLE)
	{
		((CTableFrame*)m_ppITableFrame[pIServerUserItem->GetTableID()])->OnUserScroeNotify(pIServerUserItem->GetChairID(),pIServerUserItem,cbReason);
	}

	return true;
}

//用户数据
bool CPersonalRoomGame::OnEventUserItemGameData(IServerUserItem *pIServerUserItem, BYTE cbReason)
{
	if(m_pIServerUserItemSink!=NULL)
	{
		return m_pIServerUserItemSink->OnEventUserItemGameData(pIServerUserItem,cbReason);
	}

	return true;
}

//用户状态
bool CPersonalRoomGame::OnEventUserItemStatus(IServerUserItem * pIServerUserItem,WORD wLastTableID,WORD wLastChairID)
{
	//清除数据
	if(pIServerUserItem->GetUserStatus()==US_NULL) pIServerUserItem->SetMatchData(NULL);

	if(m_pIServerUserItemSink!=NULL)
	{
		return m_pIServerUserItemSink->OnEventUserItemStatus(pIServerUserItem,wLastTableID,wLastChairID);
	}

	return true;
}

//用户权限
bool CPersonalRoomGame::OnEventUserItemRight(IServerUserItem *pIServerUserItem, DWORD dwAddRight, DWORD dwRemoveRight,BYTE cbRightKind)
{
	if(m_pIServerUserItemSink!=NULL)
	{
		return m_pIServerUserItemSink->OnEventUserItemRight(pIServerUserItem,dwAddRight,dwRemoveRight,cbRightKind);
	}

	return true;
}

//用户登录
bool CPersonalRoomGame::OnEventUserLogon(IServerUserItem * pIServerUserItem)
{
	

	return true;
}

//用户登出
bool CPersonalRoomGame::OnEventUserLogout(IServerUserItem * pIServerUserItem)
{

	return true;
}

//登录完成
bool CPersonalRoomGame::OnEventUserLogonFinish(IServerUserItem * pIServerUserItem)
{
	
	return true;
}



//游戏开始
bool CPersonalRoomGame::OnEventGameStart(ITableFrame *pITableFrame, WORD wChairCount)
{
	return true;
}

//游戏开始
void CPersonalRoomGame::PersonalRoomWriteJoinInfo(DWORD dwUserID, WORD wTableID,  WORD wChairID, DWORD dwKindID,  TCHAR * szRoomID,  TCHAR * szPersonalRoomGUID)
{
	//写参与信息
	DBR_GR_WriteJoinInfo JoinInfo;
	ZeroMemory(&JoinInfo, sizeof(DBR_GR_WriteJoinInfo));

	JoinInfo.dwUserID = dwUserID;
	JoinInfo.wTableID = wTableID;
	JoinInfo.wChairID = wChairID;
	JoinInfo.wKindID = dwKindID;
	lstrcpyn(JoinInfo.szRoomID,  szRoomID, sizeof(JoinInfo.szRoomID) );
	
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[wTableID];
	if(pTableFrame)
	{
		wsprintf(JoinInfo.szPeronalRoomGUID,  TEXT("%s"), pTableFrame->GetPersonalRoomGUID() );
	}

	//投递数据
	m_pIDBCorrespondManager->PostDataBaseRequest(0, DBR_GR_WRITE_JOIN_INFO, 0, &JoinInfo, sizeof(JoinInfo));
	// 			//任务推进
	// 			CAttemperEngineSink *pAttemperEngineSink= (CAttemperEngineSink *)m_pIGameServiceFrame;						
	// 			WORD wBindIndex = pUserItem->GetBindIndex();
	// 			tagBindParameter * pBind=pAttemperEngineSink->GetBindParameter(wBindIndex);
	// 			DBR_GR_PerformRoomTaskProgress PerformRoom;
	// 			ZeroMemory(&PerformRoom, sizeof(PerformRoom));
	// 			PerformRoom.dwUserID = pUserItem->GetUserID();
	// 			PerformRoom.wKindID=pTableFrame->GetGameServiceOption()->wKindID;
	// 			PerformRoom.nDrawCount =1;			
	// 			m_pIDBCorrespondManager->PostDataBaseRequest(PerformRoom.dwUserID, DBR_GR_WRITE_ROOM_TASK_PROGRESS,pBind->dwSocketID, &PerformRoom, sizeof(PerformRoom));
}

//游戏结束
bool CPersonalRoomGame::OnEventGameEnd(ITableFrame *pITableFrame,WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason)
{
	return true;
}

//游戏结束
bool CPersonalRoomGame::OnEventGameEnd(WORD wTableID,  WORD wChairCount, DWORD dwDrawCountLimit, DWORD & dwPersonalPlayCount, int nSpecialInfoLen ,byte * cbSpecialInfo,SYSTEMTIME sysStartTime,  tagPersonalUserScoreInfo * PersonalUserScoreInfo)
{
	DWORD dwTimeNow = (DWORD)time(NULL);
	if((dwDrawCountLimit!=0 && dwDrawCountLimit <= dwPersonalPlayCount) )
	{

		CMD_GR_PersonalTableEnd PersonalTableEnd;
		ZeroMemory(&PersonalTableEnd, sizeof(CMD_GR_PersonalTableEnd));

		lstrcpyn(PersonalTableEnd.szDescribeString, TEXT("约战结束。"), CountArray(PersonalTableEnd.szDescribeString));
		TCHAR szInfo[260] = {0};
		for(int i = 0; i < wChairCount; ++i)
		{
			PersonalTableEnd.lScore[i] = PersonalUserScoreInfo[i].lScore;
		}
		//添加特殊信息
		memcpy( PersonalTableEnd.cbSpecialInfo, cbSpecialInfo, nSpecialInfoLen);
		PersonalTableEnd.nSpecialInfoLen = nSpecialInfoLen;
		CopyMemory(&PersonalTableEnd.sysStartTime,&sysStartTime,sizeof(SYSTEMTIME));
		GetLocalTime(&PersonalTableEnd.sysEndTime);
		WORD wDataSize = sizeof(CMD_GR_PersonalTableEnd) - sizeof(PersonalTableEnd.lScore) + sizeof(LONGLONG) * wChairCount;

		CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[wTableID];
		for(int i = 0; i < wChairCount; ++i)
		{
			IServerUserItem* pUserItem = pTableFrame->GetTableUserItem(i);
			if(pUserItem == NULL) continue;
			
			

			PersonalTableEnd.lScore[i] =PersonalUserScoreInfo[i].lScore;
			m_pIGameServiceFrame->SendData(pUserItem, MDM_GR_PERSONAL_TABLE, SUB_GR_PERSONAL_TABLE_END, &PersonalTableEnd, sizeof(CMD_GR_PersonalTableEnd) );
		}


		//防止客户端因为状态为free获取不到用户指针
		for(int i = 0; i < wChairCount; ++i)
		{
			IServerUserItem* pTableUserItem = pTableFrame->GetTableUserItem(i);
			if(pTableUserItem == NULL) continue;

			pTableFrame->PerformStandUpAction(pTableUserItem);
		}

		//解散桌子
		m_pIGameServiceFrame->DismissPersonalTable(m_pGameServiceOption->wServerID, wTableID);
		return true;
	}

	return false;
}

//用户坐下
bool CPersonalRoomGame::OnActionUserSitDown(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{ 
	return true; 
}

//用户起立
bool CPersonalRoomGame::OnActionUserStandUp(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{

	return true;
}

 //用户同意
bool CPersonalRoomGame::OnActionUserOnReady(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize)
{ 
	return true; 
}


//创建桌子
bool CPersonalRoomGame::OnTCPNetworkSubCreateTable(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem)
{
	//效验数据
	ASSERT(wDataSize==sizeof(CMD_GR_CreateTable));
	if (wDataSize!=sizeof(CMD_GR_CreateTable)) return false;

	//提取数据
	CMD_GR_CreateTable * pCreateTable = (CMD_GR_CreateTable*)pData;
	ASSERT(pCreateTable!=NULL);
	
	int nStatus = pIServerUserItem->GetUserStatus() ;
	//检查玩家状态，判断玩家是否在游戏中，如果在，则不能创建房间
	if ((pIServerUserItem->GetUserStatus() == US_OFFLINE) || (nStatus <= US_PLAYING   &&  nStatus  >= US_SIT))
	{
		//构造数据
		CMD_GR_CreateFailure CreateFailure;
		ZeroMemory(&CreateFailure, sizeof(CMD_GR_CreateFailure));

		CreateFailure.lErrorCode = 1;
		lstrcpyn(CreateFailure.szDescribeString, TEXT("玩家正在约战房游戏中，不能创建房间！"), CountArray(CreateFailure.szDescribeString));

		//发送数据
		m_pITCPNetworkEngine->SendData(dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CREATE_FAILURE, &CreateFailure, sizeof(CMD_GR_CreateFailure));
		return true;
	}

	//IServerUserItem * pIServerUserItem=m_pIServerUserManager->SetServerUserItemSink();


	//寻找空闲桌子
	//INT_PTR nSize = m_ppITableFrame.GetCount();
	TCHAR szInfo[260] = {0};
	for(INT_PTR i = 0; i < m_pGameServiceOption->wTableCount; ++i)
	{
		CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[i];

		if(pTableFrame->GetNullChairCount() == pTableFrame->GetChairCount() && pTableFrame->IsPersonalTableLocked() == false)
		{
			//锁定桌子
			pTableFrame->SetPersonalTableLlocked(true);

			//桌子号
			DWORD dwTableID = pTableFrame->GetTableID();

			//构造数据
			DBR_GR_CreateTable CreateTable;
			ZeroMemory(&CreateTable, sizeof(DBR_GR_CreateTable));

			CreateTable.dwUserID = pIServerUserItem->GetUserID();
			CreateTable.dwClientAddr = pIServerUserItem->GetClientAddr();
			CreateTable.dwServerID = m_pGameServiceOption->wServerID;
			CreateTable.dwTableID = dwTableID;
			CreateTable.dwDrawCountLimit = pCreateTable->dwDrawCountLimit;
			CreateTable.dwDrawTimeLimit = pCreateTable->dwDrawTimeLimit;
			CreateTable.lCellScore = static_cast<LONG>(pCreateTable->lCellScore);
			CreateTable.dwRoomTax = pCreateTable->dwRoomTax;
			CreateTable.wJoinGamePeopleCount = pCreateTable->wJoinGamePeopleCount;
			lstrcpyn(CreateTable.szPassword, pCreateTable->szPassword, CountArray(CreateTable.szPassword));
			memcpy(CreateTable.cbGameRule, pCreateTable->cbGameRule,  CountArray(CreateTable.cbGameRule));

			//投递数据
			m_pIDBCorrespondManager->PostDataBaseRequest(CreateTable.dwUserID, DBR_GR_CREATE_TABLE, dwSocketID, &CreateTable, sizeof(DBR_GR_CreateTable));

			return true;
		}
	}

	//构造数据
	CMD_GR_CreateFailure CreateFailure;
	ZeroMemory(&CreateFailure, sizeof(CMD_GR_CreateFailure));

	CreateFailure.lErrorCode = 1;
	lstrcpyn(CreateFailure.szDescribeString, TEXT("目前该游戏约战房间已满，请稍后再试！"), CountArray(CreateFailure.szDescribeString));

	//发送数据
	m_pITCPNetworkEngine->SendData(dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CREATE_FAILURE, &CreateFailure, sizeof(CMD_GR_CreateFailure));

	return true;
}
//取消请求
bool CPersonalRoomGame::OnTCPNetworkSubCancelRequest(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem)
{
	//校验数据
	ASSERT(wDataSize == sizeof(CMD_GR_CancelRequest));
	if(wDataSize != sizeof(CMD_GR_CancelRequest)) return false;

	//提取数据
	CMD_GR_CancelRequest * pCancelRequest = (CMD_GR_CancelRequest*)pData;
	ASSERT(pCancelRequest!=NULL);
	//获取用户
	//WORD wBindIndex=LOWORD(dwSocketID);
	//IServerUserItem * pIServerUserItem=GetBindUserItem(wBindIndex);
	//if (pIServerUserItem==NULL) return false;

	//获取桌子
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pCancelRequest->dwTableID];
	ASSERT(pTableFrame != NULL);

	//如果请求解散的房间不是当前房间则返回
	//if (lstrcmp(pTableFrame->GetPersonalTableID(),  pCancelRequest->szRoomID) != 0)
	//{

	//	return true;
	//}

	if(pTableFrame->CancelTableRequest(pCancelRequest->dwUserID, static_cast<WORD>(pCancelRequest->dwChairID)) == false) return false;

	//转发数据
	WORD wChairCount = pTableFrame->GetChairCount();
	for(int i = 0; i < wChairCount; ++i)
	{
		//过滤用户

		IServerUserItem* pUserItem = pTableFrame->GetTableUserItem(i);
		if(pUserItem == NULL || pUserItem == pIServerUserItem) continue;

		m_pIGameServiceFrame->SendData(pUserItem, MDM_GR_PERSONAL_TABLE, SUB_GR_CANCEL_REQUEST, pData, wDataSize);
	}

	return true;
}
//取消答复
bool CPersonalRoomGame::OnTCPNetworkSubRequestReply(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem)
{
	//校验数据
	ASSERT(wDataSize == sizeof(CMD_GR_RequestReply));
	if(wDataSize != sizeof(CMD_GR_RequestReply)) return false;

	//提取数据
	CMD_GR_RequestReply * pRequestReply = (CMD_GR_RequestReply*)pData;
	ASSERT(pRequestReply!=NULL);

	//获取用户
	//WORD wBindIndex=LOWORD(dwSocketID);
	//IServerUserItem * pIServerUserItem=GetBindUserItem(wBindIndex);
	if (pIServerUserItem==NULL) return false;

	//获取桌子
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pRequestReply->dwTableID];
	ASSERT(pTableFrame != NULL);
	//if(pRequestReply->dwUserID == pTableFrame->GetTableOwner()->GetUserID()) return false;

	//转发数据
	WORD wChairCount = pTableFrame->GetChairCount();
	for(int i = 0; i < wChairCount; ++i)
	{
		//过滤用户
		IServerUserItem* pUserItem = pTableFrame->GetTableUserItem(i);
		if(pUserItem == NULL || pUserItem == pIServerUserItem) continue;

		m_pIGameServiceFrame->SendData(pUserItem, MDM_GR_PERSONAL_TABLE, SUB_GR_REQUEST_REPLY, pData, wDataSize);
	}

	if(pTableFrame->CancelTableRequestReply(pRequestReply->dwUserID, pRequestReply->cbAgree) == false) return false;

	return true;
}

//取消请求
bool CPersonalRoomGame::OnTCPNetworkSubHostDissumeTable(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem)
{
	//校验数据
	ASSERT(wDataSize == sizeof(CMD_GR_HostDissumeGame));
	if(wDataSize != sizeof(CMD_GR_HostDissumeGame)) return false;

	//提取数据
	CMD_GR_HostDissumeGame * pCancelRequest = (CMD_GR_HostDissumeGame*)pData;
	ASSERT(pCancelRequest!=NULL);
	//获取用户
	WORD wBindIndex=LOWORD(dwSocketID);
	//IServerUserItem * pIServerUserItem=GetBindUserItem(wBindIndex);
	//if (pIServerUserItem==NULL) return false;

	//获取桌子
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pCancelRequest->dwTableID];

	ASSERT(pTableFrame != NULL);

	//if (lstrcmp(pTableFrame->GetPersonalTableID(),  pCancelRequest->szRoomID) != 0)
	//{
	//	SendData(pIServerUserItem, MDM_GR_PERSONAL_TABLE, SUB_GF_ALREADY_CANCLE, NULL, 0);
	//	return true;
	//}


	//投递数据
	DBR_GR_CancelCreateTable CancelCreateTable;
	ZeroMemory(&CancelCreateTable, sizeof(DBR_GR_CancelCreateTable));

	CancelCreateTable.dwUserID = pTableFrame->GetTableOwner();
	CancelCreateTable.dwReason = pTableFrame->GetDrawCount();
	CancelCreateTable.dwDrawCountLimit = pTableFrame->GetPersonalTableParameter().dwPlayTurnCount;
	CancelCreateTable.dwDrawTimeLimit = pTableFrame->GetPersonalTableParameter().dwPlayTimeLimit;
	CancelCreateTable.dwServerID = m_pGameServiceOption->wServerID;
	lstrcpyn(CancelCreateTable.szRoomID,  pTableFrame->GetPersonalTableID(), sizeof(CancelCreateTable.szRoomID) );



	m_pIDBCorrespondManager->PostDataBaseRequest(CancelCreateTable.dwUserID, DBR_GR_HOST_CANCEL_CREATE_TABLE, dwSocketID, &CancelCreateTable, sizeof(DBR_GR_CancelCreateTable));


	pTableFrame->HostDissumeGame(true);

	if(pTableFrame)
	{
		pTableFrame->SetPersonalTableLlocked(false);
	}


	//发送数据
	CMD_CS_C_DismissTable DismissTable;
	ZeroMemory(&DismissTable, sizeof(CMD_CS_C_DismissTable));
	DismissTable.dwSocketID = dwSocketID;
	DismissTable.dwServerID = m_pGameServiceOption->wServerID;
	DismissTable.dwTableID = pTableFrame->GetTableID();

	//发送消息
	m_pITCPSocketService->SendData(MDM_CS_SERVICE_INFO, SUB_CS_C_DISMISS_TABLE, &DismissTable, sizeof(CMD_CS_C_DismissTable));


	return true;
}

//约战房间玩家请求房间信息
bool CPersonalRoomGame::OnDBCurrenceRoomCardAndBeant(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//效验参数
	ASSERT(wDataSize == sizeof(DBO_GR_CurrenceRoomCardAndBeans));
	if (sizeof(DBO_GR_CurrenceRoomCardAndBeans)!=wDataSize) return false;
	DBO_GR_CurrenceRoomCardAndBeans * pCardAndBeans = (DBO_GR_CurrenceRoomCardAndBeans *)pData;
	CMD_GR_CurrenceRoomCardAndBeans  CurrenceRoomCardAndBeans;
	CurrenceRoomCardAndBeans.dbBeans = pCardAndBeans->dbBeans;
	CurrenceRoomCardAndBeans.lRoomCard = pCardAndBeans->lRoomCard;

	m_pITCPNetworkEngine->SendData(dwContextID, MDM_GR_PERSONAL_TABLE, SUB_GR_CURRECE_ROOMCARD_AND_BEAN, &CurrenceRoomCardAndBeans, sizeof(CMD_GR_CurrenceRoomCardAndBeans));

	return true;

}

//私人配置
bool CPersonalRoomGame::OnDBDissumeTableResult(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//效验参数
	ASSERT(wDataSize == sizeof(DBO_GR_DissumeResult));
	if (sizeof(DBO_GR_DissumeResult)!=wDataSize) return false;
	DBO_GR_DissumeResult * pDissumeResult = (DBO_GR_DissumeResult *)pData;
	CMD_GR_DissumeTable  DissumeTable;
	DissumeTable.cbIsDissumSuccess = pDissumeResult->cbIsDissumSuccess;
	lstrcpyn(DissumeTable.szRoomID , pDissumeResult->szRoomID, sizeof(DissumeTable.szRoomID));
	DissumeTable.sysDissumeTime = pDissumeResult->sysDissumeTime;

	//获取房间玩家的信息
	for (int i = 0; i < PERSONAL_ROOM_CHAIR; i++)
	{
		memcpy(&DissumeTable.PersonalUserScoreInfo[i],  &pDissumeResult->PersonalUserScoreInfo[i],  sizeof(tagPersonalUserScoreInfo));
	}

	m_pITCPNetworkEngine->SendData(pDissumeResult->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_HOST_DISSUME_TABLE_RESULT, &DissumeTable, sizeof(CMD_GR_DissumeTable));

	return true;

}


//私人配置
bool CPersonalRoomGame::OnDBLoadPersonalParameter(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//效验参数
	ASSERT(wDataSize%sizeof(tagPersonalTableParameter)==0);
	if (wDataSize%sizeof(tagPersonalTableParameter)!=0) return false;

	//清楚缓存
	ASSERT(m_PersonalTableParameterArray.GetCount() == 0);
	if(m_PersonalTableParameterArray.GetCount() != 0)
	{
		INT_PTR nSize = m_PersonalTableParameterArray.GetCount();
		for(INT_PTR i = 0; i < nSize; ++i)
		{
			tagPersonalTableParameter* pPersonalTableParameter = m_PersonalTableParameterArray.GetAt(i);
			SafeDelete(pPersonalTableParameter);
		}
		m_PersonalTableParameterArray.RemoveAll();
	}

	//数据转换
	WORD wCount = wDataSize/sizeof(tagPersonalTableParameter);
	tagPersonalTableParameter* pParameterData = (tagPersonalTableParameter*)pData;

	//保存配置
	for(int i = 0; i < wCount; ++i)
	{
		tagPersonalTableParameter* pPersonalTableParameter = new tagPersonalTableParameter;
		CopyMemory(pPersonalTableParameter,&pParameterData[i],sizeof(tagPersonalTableParameter));
		m_PersonalTableParameterArray.Add(pPersonalTableParameter);
	}

	return true;
}

//创建成功
bool CPersonalRoomGame::OnDBCreateSucess(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//数据校验
	ASSERT(wDataSize == sizeof(DBO_GR_CreateSuccess));
	if(wDataSize != sizeof(DBO_GR_CreateSuccess)) return false;

	//判断在线
	CAttemperEngineSink * pAttemperEngineSink= (CAttemperEngineSink *)m_pIGameServiceFrame;
	tagBindParameter * pBindParameter=pAttemperEngineSink->GetBindParameter(LOWORD(dwContextID));
	if ((pBindParameter->dwSocketID!=dwContextID)||(pBindParameter->pIServerUserItem==NULL)) return true;

	//获取用户
	if (pIServerUserItem==NULL) return false;

	DBO_GR_CreateSuccess* pCreateSuccess = (DBO_GR_CreateSuccess*)pData;

	//获取信息
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pCreateSuccess->dwTableID];

	if(pTableFrame == NULL)
	{
		return true;
	}

	WORD wChairID = pTableFrame->GetNullChairID();

	//初始分数
	LONGLONG lIniScore = 0;
	byte cbIsJoinGame = 0;
	INT_PTR nSize = m_PersonalTableParameterArray.GetCount();
	for(INT_PTR i = 0; i < nSize; ++i)
	{
		tagPersonalTableParameter* pTableParameter = m_PersonalTableParameterArray.GetAt(i);
		if(pTableParameter->dwPlayTurnCount == pCreateSuccess->dwDrawCountLimit && pTableParameter->dwPlayTimeLimit == pCreateSuccess->dwDrawTimeLimit)
		{
			lIniScore = pTableParameter->lIniScore;

			//将配置信息改为制定的配置信息
			pTableParameter->wJoinGamePeopleCount = pCreateSuccess->wJoinGamePeopleCount;	//参加游戏的最大人数
			pTableParameter->lCellScore = pCreateSuccess->lCellScore;													//约战房的最大底分

			pTableParameter->dwPlayTurnCount = pCreateSuccess->dwDrawCountLimit; 						//私人放进行游戏的最大局数
			pTableParameter->dwPlayTimeLimit = pCreateSuccess->dwDrawTimeLimit;							//约战房进行游戏的最大时间 单位秒

			cbIsJoinGame = pTableParameter->cbIsJoinGame ;

			break;
		}
	}

	cbIsJoinGame = m_pPersonalRoomOption->cbIsJoinGame;
	if (pCreateSuccess->lCellScore != 0)
	{
			pTableFrame->SetCellScore(pCreateSuccess->lCellScore);
	}

	pTableFrame->SetGameRule(pCreateSuccess->cbGameRule, RULE_LEN);
	//生成房间唯一标识
	TCHAR szInfo[32] = {0};
	SYSTEMTIME tm;
	GetLocalTime(&tm);
	wsprintf(szInfo, TEXT("%d%d%d%d%d%d%d%d"), pIServerUserItem->GetUserID(),  tm.wYear,  tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond, tm.wMilliseconds);
	pTableFrame->SetCreatePersonalTime(tm);
	//设置私人房间唯一标识
	pTableFrame->SetPersonalRoomGUID(szInfo, lstrlen(szInfo));


	//如果使用的是金币数据库
	if (lstrcmp(m_pGameServiceOption->szDataBaseName,  TEXT("RYTreasureDB")) == 0)
	{
		pTableFrame->SetPersonalTable(pCreateSuccess->dwDrawCountLimit, pCreateSuccess->dwDrawTimeLimit, 0);
		pTableFrame->SetDataBaseMode(0);
	}
	else
	{
		pTableFrame->SetPersonalTable(pCreateSuccess->dwDrawCountLimit, pCreateSuccess->dwDrawTimeLimit, lIniScore);
		pTableFrame->SetDataBaseMode(1);
	}


	//设置桌子配置信息
	tagPersonalTableParameter PersonalTableParameter;
	PersonalTableParameter.lIniScore = lIniScore;
	PersonalTableParameter.wJoinGamePeopleCount = pCreateSuccess->wJoinGamePeopleCount;		//参加游戏的最大人数
	PersonalTableParameter.lCellScore = pCreateSuccess->lCellScore;													//约战房的最大底分
	PersonalTableParameter.dwPlayTurnCount = pCreateSuccess->dwDrawCountLimit; 						//私人放进行游戏的最大局数
	PersonalTableParameter.dwPlayTimeLimit = pCreateSuccess->dwDrawTimeLimit;								//约战房进行游戏的最大时间 单位秒
	PersonalTableParameter.cbIsJoinGame = cbIsJoinGame;

	PersonalTableParameter.dwTimeAfterBeginCount = m_pPersonalRoomOption->dwTimeAfterBeginCount;
	PersonalTableParameter.dwTimeAfterCreateRoom =m_pPersonalRoomOption->dwTimeAfterCreateRoom;
	PersonalTableParameter.dwTimeNotBeginGame = m_pPersonalRoomOption->dwTimeNotBeginGame;
	PersonalTableParameter.dwTimeOffLineCount = m_pPersonalRoomOption->dwTimeOffLineCount;

	pTableFrame->SetPersonalTableParameter(PersonalTableParameter, *m_pPersonalRoomOption);

	//cbGameRule[1] 为  2 、3 、4 、5, 0分别对应 2人 、 3人 、 4人 、 5人 、 2-5人 这几种配置
	if (pCreateSuccess->cbGameRule[0] == SET_RULE ) 
	{
		if( pCreateSuccess->cbGameRule[1] != 0)
		{
			pTableFrame->SetTableChairCount(pCreateSuccess->cbGameRule[1]);
			CMD_GR_ChangeChairCount  ChangeChairCount;
			ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[1];

			//向客户端发送桌子上椅子数量改变的消息
			m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
		}
		else
		{
			pTableFrame->SetTableChairCount( pCreateSuccess->cbGameRule[2]);
			CMD_GR_ChangeChairCount  ChangeChairCount;
			ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[2];

			//向客户端发送桌子上椅子数量改变的消息
			m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
		}

	}


	//转转麻将设置一张桌子上椅子的个数
	if (m_pGameServiceAttrib->wKindID == ZZMJ_KIND_ID || m_pGameServiceAttrib->wKindID  == HZMJ_KIND_ID)
	{
		if (pCreateSuccess->cbGameRule[0] == SET_RULE) 
		{
			pTableFrame->SetTableChairCount(pCreateSuccess->cbGameRule[1]);

			CMD_GR_ChangeChairCount  ChangeChairCount;
			ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[1];

			//向客户端发送桌子上椅子数量改变的消息
			m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
		}
	}

	if ( ((m_pGameServiceOption->wServerType) & GAME_GENRE_PERSONAL) != 0 )
	{
		//诈金花房卡设置椅子个数
		if (m_pGameServiceAttrib->wKindID == ZJH_KIND_ID)
		{
			//cbGameRule[1] 为  2 、3 、4 、5, 0分别对应 2人 、 3人 、 4人 、 5人 、 2-5人 这几种配置
			if (pCreateSuccess->cbGameRule[0] == SET_RULE && pCreateSuccess->cbGameRule[1] != 0) 
			{
				pTableFrame->SetTableChairCount(pCreateSuccess->cbGameRule[1]);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[1];

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
			else
			{

				pTableFrame->SetTableChairCount(ZJH_MAX_PLAYER);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = ZJH_MAX_PLAYER;

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
		}

		//诈金花房卡设置椅子个数
		if (m_pGameServiceAttrib->wKindID == NN_KIND_ID)
		{
			//cbGameRule[1] 为  2 、3 、4 、5, 0分别对应 2人 、 3人 、 4人 、 5人 、 2-5人 这几种配置
			if (pCreateSuccess->cbGameRule[0] == SET_RULE && pCreateSuccess->cbGameRule[1] != 0) 
			{
				pTableFrame->SetTableChairCount(pCreateSuccess->cbGameRule[1]);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[1];

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
			else
			{

				pTableFrame->SetTableChairCount(NN_MAX_PLAYER);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = NN_MAX_PLAYER;

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
		}

		//诈金花房卡设置椅子个数
		if (m_pGameServiceAttrib->wKindID == TBZ_KIND_ID)
		{
			//cbGameRule[1] 为  2 、3 、4 、5, 0分别对应 2人 、 3人 、 4人 、 5人 、 2-5人 这几种配置
			if (pCreateSuccess->cbGameRule[0] == SET_RULE && pCreateSuccess->cbGameRule[1] != 0) 
			{
				pTableFrame->SetTableChairCount(pCreateSuccess->cbGameRule[1]);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = pCreateSuccess->cbGameRule[1];

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
			else
			{

				pTableFrame->SetTableChairCount(TBZ_MAX_PLAYER);
				CMD_GR_ChangeChairCount  ChangeChairCount;
				ChangeChairCount.dwChairCount = TBZ_MAX_PLAYER;

				//向客户端发送桌子上椅子数量改变的消息
				m_pITCPNetworkEngine->SendData(pBindParameter->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CHANGE_CHAIR_COUNT, &ChangeChairCount,  sizeof(CMD_GR_ChangeChairCount));	
			}
		}
	}


	//设置桌子	
	tagUserInfo* pUserInfo = pIServerUserItem->GetUserInfo();
	pUserInfo->dBeans = pCreateSuccess->dCurBeans;
	pUserInfo->lRoomCard = pCreateSuccess->lRoomCard;
	pTableFrame->SetTableOwner(pUserInfo->dwUserID);
	pTableFrame->SetTimerNotBeginAfterCreate();
	pTableFrame->SetRoomCardFee(pCreateSuccess->iRoomCardFee);
	tagUserRule* pUserRule = pIServerUserItem->GetUserRule();
	lstrcpyn(pUserRule->szPassword, pCreateSuccess->szPassword, CountArray(pUserRule->szPassword));

	//如果房主参与游戏
	if (pCreateSuccess->cbIsJoinGame)
	{

		//执行坐下
		if(pTableFrame->PerformSitDownAction(wChairID, pIServerUserItem, pCreateSuccess->szPassword)== FALSE)
		{
			CTraceService::TraceString(TEXT("创建桌子坐下失败"), TraceLevel_Info);
			//解锁桌子
			pTableFrame->SetPersonalTableLlocked(false);
			pTableFrame->SetPersonalTable(0, 0, 0);
			pTableFrame->SetCellScore(m_pGameServiceOption->lCellScore);
			pTableFrame->SetTableOwner(0);
			pTableFrame->SetRoomCardFee(0);
			//退还费用
			DBR_GR_CancelCreateTable CancelCreateTable;
			ZeroMemory(&CancelCreateTable, sizeof(DBR_GR_CancelCreateTable));

			CancelCreateTable.dwUserID = pCreateSuccess->dwUserID;
			CancelCreateTable.dwClientAddr = pBindParameter->dwClientAddr;
			CancelCreateTable.dwTableID = pCreateSuccess->dwTableID;
			CancelCreateTable.dwReason = CANCELTABLE_REASON_SYSTEM;
			CancelCreateTable.dwDrawCountLimit = pCreateSuccess->dwDrawCountLimit;
			CancelCreateTable.dwDrawTimeLimit = pCreateSuccess->dwDrawTimeLimit;
			CancelCreateTable.dwServerID = m_pGameServiceOption->wServerID;

			//投递数据
			m_pIDBCorrespondManager->PostDataBaseRequest(pCreateSuccess->dwUserID, DBR_GR_CANCEL_CREATE_TABLE, dwContextID, &CancelCreateTable, sizeof(DBR_GR_CancelCreateTable));

			return true;
		}
	}




	//数据汇总
	CMD_CS_C_CreateTable CS_CreateTable;
	ZeroMemory(&CS_CreateTable, sizeof(CMD_CS_C_CreateTable));

	CS_CreateTable.dwSocketID = dwContextID;
	CS_CreateTable.dwClientAddr = pBindParameter->dwClientAddr;
	CS_CreateTable.PersonalTable.dwServerID	= m_pGameServiceOption->wServerID;
	CS_CreateTable.PersonalTable.dwKindID = m_pGameServiceOption->wKindID;
	CS_CreateTable.PersonalTable.dwTableID = pCreateSuccess->dwTableID;
	CS_CreateTable.PersonalTable.dwUserID = pIServerUserItem->GetUserID();
	CS_CreateTable.PersonalTable.dwDrawCountLimit = pCreateSuccess->dwDrawCountLimit;
	CS_CreateTable.PersonalTable.dwDrawTimeLimit = pCreateSuccess->dwDrawTimeLimit;
	CS_CreateTable.PersonalTable.lCellScore = pCreateSuccess->lCellScore;
	CS_CreateTable.PersonalTable.dwRoomTax = pCreateSuccess->dwRoomTax;
	CS_CreateTable.PersonalTable.wJoinGamePeopleCount = pCreateSuccess->wJoinGamePeopleCount;
	lstrcpyn(CS_CreateTable.PersonalTable.szPassword, pCreateSuccess->szPassword, CountArray(CS_CreateTable.PersonalTable.szPassword));
	lstrcpyn(CS_CreateTable.szClientAddr, pCreateSuccess->szClientAddr, CountArray(CS_CreateTable.szClientAddr));

	//发送数据
	m_pITCPSocketService->SendData(MDM_CS_SERVICE_INFO, SUB_CS_C_CREATE_TABLE, &CS_CreateTable, sizeof(CMD_CS_C_CreateTable));


	return true;
}

//创建失败
bool CPersonalRoomGame::OnDBCreateFailure(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//数据校验
	ASSERT(wDataSize == sizeof(DBO_GR_CreateFailure));
	if(wDataSize != sizeof(DBO_GR_CreateFailure)) return false;

	//判断在线
	CAttemperEngineSink * pAttemperEngineSink= (CAttemperEngineSink *)m_pIGameServiceFrame;
	tagBindParameter * pBindParameter=pAttemperEngineSink->GetBindParameter(LOWORD(dwContextID));
	if ((pBindParameter->dwSocketID!=dwContextID)||(pBindParameter->pIServerUserItem==NULL)) return true;

	//获取用户
	if (pIServerUserItem==NULL) return false;

	DBO_GR_CreateFailure* pCreateFailure = (DBO_GR_CreateFailure*)pData;

	//构造数据
	CMD_GR_CreateFailure CreateFailure;
	ZeroMemory(&CreateFailure, sizeof(CMD_GR_CreateFailure));

	CreateFailure.lErrorCode = pCreateFailure->lErrorCode;
	lstrcpyn(CreateFailure.szDescribeString, pCreateFailure->szDescribeString, CountArray(CreateFailure.szDescribeString));

	//发送数据
	m_pITCPNetworkEngine->SendData(dwContextID, MDM_GR_PERSONAL_TABLE, SUB_GR_CREATE_FAILURE, &CreateFailure, sizeof(CMD_GR_CreateFailure));

	return true;
}

//取消创建
bool CPersonalRoomGame::OnDBCancelCreateTable(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	//数据校验
	ASSERT(wDataSize == sizeof(DBO_GR_CancelCreateResult));
	if(wDataSize != sizeof(DBO_GR_CancelCreateResult)) return false;

	////判断在线
	CAttemperEngineSink * pAttemperEngineSink= (CAttemperEngineSink *)m_pIGameServiceFrame;
	//tagBindParameter * pBindParameter=pAttemperEngineSink->GetBindParameter(LOWORD(dwContextID));
	//if(pBindParameter == NULL) return true;
	//if ((pBindParameter->dwSocketID!=dwContextID)||(pBindParameter->pIServerUserItem==NULL)) return true;

	////获取用户
	//if (pIServerUserItem==NULL) return false;

	//获取数据
	DBO_GR_CancelCreateResult* pCancelCreateResult = (DBO_GR_CancelCreateResult*)pData;
	CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pCancelCreateResult->dwTableID];
	ASSERT(pTableFrame != NULL);


	pTableFrame->SetPersonalTableLlocked(false);
	pTableFrame->SetPersonalTable(0, 0, 0);
	pTableFrame->SetTableOwner(0);
	pTableFrame->SetRoomCardFee(0);
	WORD wChairCount = pTableFrame->GetChairCount();

	
	for(int i = 0; i < wChairCount; ++i)
	{
		IServerUserItem* pUserItem =pTableFrame->GetTableUserItem(i);
		if(pUserItem == NULL) continue;
		if(pUserItem->GetUserStatus() == US_OFFLINE) 
		{
			pTableFrame->PerformStandUpAction(pUserItem);
			continue;
		}
		//绑定参数
		WORD wBindIndex = pUserItem->GetBindIndex();
		tagBindParameter * pBind=pAttemperEngineSink->GetBindParameter(wBindIndex);

		//构造数据
		CMD_GR_CancelTable CancelTable;
		ZeroMemory(&CancelTable, sizeof(CMD_GR_CancelTable));
		CancelTable.dwReason = pCancelCreateResult->dwReason;
		if(pCancelCreateResult->dwReason == CANCELTABLE_REASON_SYSTEM)
			lstrcpyn(CancelTable.szDescribeString, TEXT("游戏自动解散。"), CountArray(CancelTable.szDescribeString));
		else if(pCancelCreateResult->dwReason == CANCELTABLE_REASON_PLAYER)
			lstrcpyn(CancelTable.szDescribeString, TEXT("游戏未开始，游戏自动解散。"), CountArray(CancelTable.szDescribeString));
		else if(pCancelCreateResult->dwReason == CANCELTABLE_REASON_ENFOCE)
			lstrcpyn(CancelTable.szDescribeString, TEXT("房主退出游戏或游戏超时，游戏解散。"), CountArray(CancelTable.szDescribeString));
		else if(pCancelCreateResult->dwReason == CANCELTABLE_REASON_OVER_TIME)
			lstrcpyn(CancelTable.szDescribeString, TEXT("游戏超时，游戏解散。"), CountArray(CancelTable.szDescribeString));
		else if(pCancelCreateResult->dwReason == CANCELTABLE_REASON_NOT_START)
			lstrcpyn(CancelTable.szDescribeString, TEXT("约战规定开始时间到未开始游戏，游戏解散。"), CountArray(CancelTable.szDescribeString));

		if (CANCELTABLE_REASON_NOT_START == pCancelCreateResult->dwReason || CANCELTABLE_REASON_OVER_TIME == pCancelCreateResult->dwReason)
		{
			CancelTable.dwReason = CANCELTABLE_REASON_SYSTEM;
		}
		//解散消息
		m_pITCPNetworkEngine->SendData(pBind->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CANCEL_TABLE, &CancelTable, sizeof(CMD_GR_CancelTable));

		//用户状态
		//pUserItem->SetUserStatus(US_FREE, INVALID_TABLE, INVALID_CHAIR);
		pTableFrame->PerformStandUpAction(pUserItem);
	}

	////构造数据
	//CMD_GR_CancelTable CancelTable;
	//ZeroMemory(&CancelTable, sizeof(CMD_GR_CancelTable));
	//CancelTable.dwReason = pCancelCreateResult->dwReason;
	//lstrcpyn(CancelTable.szDescribeString, pCancelCreateResult->szDescribeString, CountArray(CancelTable.szDescribeString));

	////发送数据
	//m_pITCPNetworkEngine->SendData(dwContextID, MDM_GR_PERSONAL_TABLE, SUB_GR_CANCEL_TABLE, &CancelTable, sizeof(CMD_GR_CancelTable));

	////退出用户
	//pIServerUserItem->SetUserStatus(US_NULL, INVALID_TABLE, INVALID_CHAIR);

	return true;
}


//约战服务器事件
bool CPersonalRoomGame::OnTCPSocketMainServiceInfo(WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	switch (wSubCmdID)
	{
	case SUB_CS_S_CREATE_TABLE_RESULT:		//创建结果
		{
			//效验参数
			ASSERT(wDataSize==sizeof(CMD_CS_S_CreateTableResult));
			if (wDataSize!=sizeof(CMD_CS_S_CreateTableResult)) return false;

			//变量定义
			CMD_CS_S_CreateTableResult * pCreateTableResult=(CMD_CS_S_CreateTableResult *)pData;

			//获取用户
			WORD wBindIndex=LOWORD(pCreateTableResult->dwSocketID);
			//判断在线
			CAttemperEngineSink * pAttemperEngineSink= (CAttemperEngineSink *)m_pIGameServiceFrame;
			IServerUserItem * pIServerUserItem=pAttemperEngineSink->GetBindUserItem(wBindIndex);
			if (pIServerUserItem==NULL) return false;

			if(pCreateTableResult->PersonalTable.dwDrawCountLimit == 0 && pCreateTableResult->PersonalTable.dwDrawTimeLimit == 0)
			{
				//构造数据
				CMD_GR_CreateFailure CreateFailure;
				ZeroMemory(&CreateFailure, sizeof(CMD_GR_CreateFailure));

				CreateFailure.lErrorCode = 1;
				lstrcpyn(CreateFailure.szDescribeString, TEXT("创建房间参数不正确！"), CountArray(CreateFailure.szDescribeString));

				//发送数据
				m_pITCPNetworkEngine->SendData(pCreateTableResult->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CREATE_FAILURE, &CreateFailure, sizeof(CMD_GR_CreateFailure));

				return false;
			}

			//获取桌子
			CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pCreateTableResult->PersonalTable.dwTableID];
			ASSERT(pTableFrame != NULL);

			tagPersonalTableParameter PersonalTableParameter = pTableFrame->GetPersonalTableParameter();
			pTableFrame->SetPersonalTableID(pCreateTableResult->PersonalTable.szRoomID);

			//构造数据
			CMD_GR_CreateSuccess CreateSuccess;
			ZeroMemory(&CreateSuccess, sizeof(CMD_GR_CreateSuccess));

			CreateSuccess.dwDrawCountLimit = PersonalTableParameter.dwPlayTurnCount;
			CreateSuccess.dwDrawTimeLimit = PersonalTableParameter.dwPlayTimeLimit;
			CreateSuccess.dBeans = pIServerUserItem->GetUserInfo()->dBeans;
			CreateSuccess.lRoomCard = pIServerUserItem->GetUserInfo()->lRoomCard;
			lstrcpyn(CreateSuccess.szServerID, pCreateTableResult->PersonalTable.szRoomID, CountArray(CreateSuccess.szServerID));

			m_pITCPNetworkEngine->SendData(pCreateTableResult->dwSocketID, MDM_GR_PERSONAL_TABLE, SUB_GR_CREATE_SUCCESS, &CreateSuccess, sizeof(CMD_GR_CreateSuccess));


			//插入桌子创建记录
			DBR_GR_InsertCreateRecord  CreateRecord;
			ZeroMemory(&CreateRecord, sizeof(DBR_GR_InsertCreateRecord));

			//桌子数据
			CreateRecord.dwServerID	= pCreateTableResult->PersonalTable.dwServerID;
			CreateRecord.dwUserID = pCreateTableResult->PersonalTable.dwUserID;
			CreateRecord.lCellScore = pCreateTableResult->PersonalTable.lCellScore;
			CreateRecord.dwDrawCountLimit = pCreateTableResult->PersonalTable.dwDrawCountLimit;
			CreateRecord.dwDrawTimeLimit = pCreateTableResult->PersonalTable.dwDrawTimeLimit;
			lstrcpyn(CreateRecord.szPassword, pCreateTableResult->PersonalTable.szPassword, CountArray(CreateRecord.szPassword));
			lstrcpyn(CreateRecord.szRoomID, pCreateTableResult->PersonalTable.szRoomID, CountArray(CreateRecord.szRoomID));
			CreateRecord.wJoinGamePeopleCount = pCreateTableResult->PersonalTable.wJoinGamePeopleCount;
			CreateRecord.dwRoomTax =  pCreateTableResult->PersonalTable.dwRoomTax;
			lstrcpyn(CreateRecord.szClientAddr, pCreateTableResult->szClientAddr, CountArray(CreateRecord.szClientAddr));

			m_pIDBCorrespondManager->PostDataBaseRequest(0, DBR_GR_INSERT_CREATE_RECORD, 0, &CreateRecord, sizeof(CreateRecord));

			return true;
		}
	case SUB_CS_C_DISMISS_TABLE_RESULT:		//解散结果
		{
			ASSERT(wDataSize == sizeof(CMD_CS_C_DismissTableResult));
			if(wDataSize != sizeof(CMD_CS_C_DismissTableResult)) return false;

			CMD_CS_C_DismissTableResult* pDismissTable = (CMD_CS_C_DismissTableResult*)pData;
			//获取桌子
			CTableFrame* pTableFrame = (CTableFrame*)m_ppITableFrame[pDismissTable->PersonalTableInfo.dwTableID];
			ASSERT(pTableFrame != NULL);

			for (int i = 0; i < pTableFrame->GetChairCount(); i++)
			{
				memcpy(&(pDismissTable->PersonalUserScoreInfo[i]), &(pTableFrame->m_PersonalUserScoreInfo[i]),  sizeof(tagPersonalUserScoreInfo));
			}
			for (int i = 0; i < pTableFrame->GetChairCount(); i++)
			{
				ZeroMemory(&(pTableFrame->m_PersonalUserScoreInfo[i]), sizeof(pTableFrame->m_PersonalUserScoreInfo[i]));
			}

			m_pIDBCorrespondManager->PostDataBaseRequest(0, DBR_GR_DISSUME_ROOM, 0, pDismissTable, sizeof(CMD_CS_C_DismissTableResult));
			return true;

		}
	}
	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
