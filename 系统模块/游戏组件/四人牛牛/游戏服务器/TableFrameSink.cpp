#include "StdAfx.h"
#include "TableFrameSink.h"
#include "DlgCustomRule.h"
#include <conio.h>
#include <locale>

//////////////////////////////////////////////////////////////////////////

//静态变量
bool				CTableFrameSink::m_bFirstInit=true;

//房间玩家信息
CMap<DWORD, DWORD, ROOMUSERINFO, ROOMUSERINFO> g_MapRoomUserInfo;	//玩家USERID映射玩家信息
//房间用户控制
CList<ROOMUSERCONTROL, ROOMUSERCONTROL&> g_ListRoomUserControl;		//房间用户控制链表
//操作控制记录
CList<CString, CString&> g_ListOperationRecord;						//操作控制记录

ROOMUSERINFO	g_CurrentQueryUserInfo;								//当前查询用户信息

//全局变量
LONGLONG						g_lRoomStorageStart = 0LL;								//房间起始库存
LONGLONG						g_lRoomStorageCurrent = 0LL;							//总输赢分
LONGLONG						g_lStorageDeductRoom = 0LL;								//回扣变量
LONGLONG						g_lStorageMax1Room = 0LL;								//库存封顶
LONGLONG						g_lStorageMul1Room = 0LL;								//系统输钱比例
LONGLONG						g_lStorageMax2Room = 0LL;								//库存封顶
LONGLONG						g_lStorageMul2Room = 0LL;								//系统输钱比例
//////////////////////////////////////////////////////////////////////////

#define	IDI_SO_OPERATE							2							//代打定时器
#define	TIME_SO_OPERATE							80000						//代打定时器

//动作标识
#define IDI_DELAY_ENDGAME			10				//动作标识
#define IDI_DELAY_TIME				3000			//延时时间

//////////////////////////////////////////////////////////////////////////

//构造函数
CTableFrameSink::CTableFrameSink()
{
	//游戏变量	
	m_wPlayerCount=GAME_PLAYER;	
	m_lExitScore=0;	
	m_lDynamicScore=0;
	m_wBankerUser=INVALID_CHAIR;
	m_wFisrtCallUser=INVALID_CHAIR;
	m_wCurrentUser=INVALID_CHAIR;
	m_wFirstEnterRoomCard = INVALID_CHAIR;

	//用户状态
	ZeroMemory(m_cbDynamicJoin,sizeof(m_cbDynamicJoin));
	ZeroMemory(m_lTableScore,sizeof(m_lTableScore));
	ZeroMemory(m_cbPlayStatus,sizeof(m_cbPlayStatus));
	ZeroMemory(m_cbCallStatus,sizeof(m_cbCallStatus));
	ZeroMemory(m_bSpecialClient,sizeof(m_bSpecialClient));
	for(BYTE i=0;i<m_wPlayerCount;i++)m_cbOxCard[i]=0xff;

	//扑克变量
	ZeroMemory(m_cbHandCardData,sizeof(m_cbHandCardData));

	//下注信息
	ZeroMemory(m_lTurnMaxScore,sizeof(m_lTurnMaxScore));

	ZeroMemory(m_bBuckleServiceCharge,sizeof(m_bBuckleServiceCharge));

	//组件变量
	m_pITableFrame=NULL;
	m_pGameServiceOption=NULL;

	//清空链表
	g_ListRoomUserControl.RemoveAll();
	g_ListOperationRecord.RemoveAll();
	ZeroMemory(&g_CurrentQueryUserInfo, sizeof(g_CurrentQueryUserInfo));

	//服务控制
	m_hInst = NULL;
	m_pServerControl = NULL;
	m_hInst = LoadLibrary(TEXT("OxNewServerControl.dll"));
	if ( m_hInst )
	{
		typedef void * (*CREATE)(); 
		CREATE ServerControl = (CREATE)GetProcAddress(m_hInst,"CreateServerControl"); 
		if ( ServerControl )
		{
			m_pServerControl = static_cast<IServerControl*>(ServerControl());
		}
	}


	if(m_bFirstInit)
	{
		CString strName = GetFileDialogPath()+	"\\OxNewBattle.log";
		CFileFind findLogFile;
		if(findLogFile.FindFile(strName))
		{
			::DeleteFile(strName);
		}

		m_bFirstInit=false;
	}
	
	time(NULL);

	return;
}

//析构函数
CTableFrameSink::~CTableFrameSink(void)
{
	if( m_pServerControl )
	{
		delete m_pServerControl;
		m_pServerControl = NULL;
	}

	if( m_hInst )
	{
		FreeLibrary(m_hInst);
		m_hInst = NULL;
	}
}

//接口查询--检测相关信息版本
void * CTableFrameSink::QueryInterface(const IID & Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(ITableFrameSink,Guid,dwQueryVer);
	QUERYINTERFACE(ITableUserAction,Guid,dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(ITableFrameSink,Guid,dwQueryVer);
	return NULL;
}

//初始化
bool CTableFrameSink::Initialization(IUnknownEx * pIUnknownEx)
{
	//查询接口
	ASSERT(pIUnknownEx!=NULL);
	m_pITableFrame=QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx,ITableFrame);
	if (m_pITableFrame==NULL) return false;

	m_pITableFrame->SetStartMode(START_MODE_ALL_READY);

	//游戏配置
	m_pGameServiceAttrib=m_pITableFrame->GetGameServiceAttrib();
	m_pGameServiceOption=m_pITableFrame->GetGameServiceOption();
	
	//读取配置
	ReadConfigInformation();


	return true;
}

//复位桌子
void CTableFrameSink::RepositionSink()
{
	//游戏变量
	m_lExitScore=0;	
	m_wCurrentUser=INVALID_CHAIR;
	m_lDynamicScore=0;
	//用户状态
	ZeroMemory(m_cbDynamicJoin,sizeof(m_cbDynamicJoin));
	ZeroMemory(m_lTableScore,sizeof(m_lTableScore));
	ZeroMemory(m_cbPlayStatus,sizeof(m_cbPlayStatus));
	ZeroMemory(m_cbCallStatus,sizeof(m_cbCallStatus));
	ZeroMemory(m_bSpecialClient,sizeof(m_bSpecialClient));
	for(BYTE i=0;i<m_wPlayerCount;i++)m_cbOxCard[i]=0xff;

	//扑克变量
	ZeroMemory(m_cbHandCardData,sizeof(m_cbHandCardData));

	//下注信息
	ZeroMemory(m_lTurnMaxScore,sizeof(m_lTurnMaxScore));

	return;
}

//游戏状态
bool CTableFrameSink::IsUserPlaying(WORD wChairID)
{
	ASSERT(wChairID<m_wPlayerCount && m_cbPlayStatus[wChairID]==TRUE);
	if(wChairID<m_wPlayerCount && m_cbPlayStatus[wChairID]==TRUE)return true;
	return false;
}

//用户断线
bool CTableFrameSink::OnActionUserOffLine(WORD wChairID, IServerUserItem * pIServerUserItem)
{
	//更新房间用户信息
	UpdateRoomUserInfo(pIServerUserItem, USER_OFFLINE);

	return true;
}

//用户坐下
bool CTableFrameSink::OnActionUserSitDown(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	//历史积分
	if (bLookonUser==false) m_HistoryScore.OnEventUserEnter(pIServerUserItem->GetChairID());

	if(m_pITableFrame->GetGameStatus()!=GS_TK_FREE)
		m_cbDynamicJoin[pIServerUserItem->GetChairID()]=TRUE;
	
	//更新房间用户信息
	UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
	
	//更新同桌用户控制
	UpdateUserControl(pIServerUserItem);
	
	//房卡首进玩家
	if (IsRoomCardType() && m_wFirstEnterRoomCard == INVALID_CHAIR)
	{
		m_wFirstEnterRoomCard = wChairID;
	}

	return true;
}

//用户起立
bool CTableFrameSink::OnActionUserStandUp(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	//历史积分
	if (bLookonUser==false) 
	{
		m_HistoryScore.OnEventUserLeave(pIServerUserItem->GetChairID());
		m_cbDynamicJoin[pIServerUserItem->GetChairID()]=FALSE;
		m_bSpecialClient[pIServerUserItem->GetChairID()]=false;
	}
	
	//更新房间用户信息
	UpdateRoomUserInfo(pIServerUserItem, USER_STANDUP);
	
	//房卡首进玩家
	if (IsRoomCardType() && m_wFirstEnterRoomCard == wChairID)
	{
		m_wFirstEnterRoomCard = INVALID_CHAIR;
	}

	return true;
}

//用户同意
bool CTableFrameSink::OnActionUserOnReady(WORD wChairID,IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize)
{
	//私人房设置游戏模式
	if (((m_pGameServiceOption->wServerType) & GAME_GENRE_PERSONAL) != 0)
	{
		//cbGameRule[1] 为  2 、3 、4, 0分别对应 2人 、 3人 、 4人 、 2-4人 这几种配置
		BYTE *pGameRule = m_pITableFrame->GetGameRule();
		switch(pGameRule[1])
		{
		case 2:
		case 3:
		case 4:
			{
				if (m_pITableFrame->GetStartMode() != START_MODE_FULL_READY)
				{
					m_pITableFrame->SetStartMode(START_MODE_FULL_READY);
				}

				break;
			}
		case 0:
			{
				if (m_pITableFrame->GetStartMode() != START_MODE_ALL_READY)
				{
					m_pITableFrame->SetStartMode(START_MODE_ALL_READY);
				}
				break;
			}
		default:
			ASSERT(false);

		}
	}

	return true;
}

//游戏开始
bool CTableFrameSink::OnEventGameStart()
{
	//库存
	if(g_lRoomStorageCurrent>0 && NeedDeductStorage())g_lRoomStorageCurrent=g_lRoomStorageCurrent-g_lRoomStorageCurrent*g_lStorageDeductRoom/1000;

	CString strInfo;
	strInfo.Format(TEXT("当前库存：%I64d"), g_lRoomStorageCurrent);
	NcaTextOut(strInfo, m_pGameServiceOption->szServerName);
	
	//私人房间
	if ((m_pGameServiceOption->wServerType&GAME_GENRE_PERSONAL) !=0 )
	{
		//cbGameRule[1] 为  2 、3 、4 , 0分别对应 2人 、 3人 、 4人  、 2-4人 这几种配置
		BYTE *pGameRule = m_pITableFrame->GetGameRule();
		if (pGameRule[1] != 0)
		{
			m_wPlayerCount = pGameRule[1];

			//设置人数
			m_pITableFrame->SetTableChairCount(m_wPlayerCount);
		}
		else
		{
			m_wPlayerCount = GAME_PLAYER;

			//设置人数
			m_pITableFrame->SetTableChairCount(GAME_PLAYER);
		}
	}

	//用户状态
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		//获取用户
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);

		if(pIServerUserItem==NULL)
		{
			m_cbPlayStatus[i]=FALSE;
		}
		else
		{
			m_cbPlayStatus[i]=TRUE;

			//更新房间用户信息
			UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
		}
	}

	ZeroMemory(m_bBuckleServiceCharge,sizeof(m_bBuckleServiceCharge));

	//获取坐庄模式
	BANKER_CONFIG bankerConfig = GetBankerConfig();
	ASSERT (bankerConfig != INVALID_BANKER);
	
	//房卡类型
	if (IsRoomCardType())
	{
		switch(bankerConfig)
		{
			//霸王庄
		case DESPOT_BANKER:
			{
				ASSERT (m_wFirstEnterRoomCard !=INVALID_CHAIR);

				//房主强制为庄，若房主不参与游戏，则第一个进去此游戏的玩家强制为庄
				//获取房主
				WORD wRoomOwenrChairID = INVALID_CHAIR;
				DWORD dwRoomOwenrUserID = INVALID_DWORD;
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					//获取用户
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
					if (!pIServerUserItem)
					{
						continue;
					}

					m_cbCallStatus[i] = TRUE;
					
					if (pIServerUserItem->GetUserID() == m_pITableFrame->GetTableOwner())
					{
						dwRoomOwenrUserID = pIServerUserItem->GetUserID();
						wRoomOwenrChairID = pIServerUserItem->GetChairID();
						break;
					}
				}
				
				//房主参与游戏
				if (dwRoomOwenrUserID != INVALID_DWORD && wRoomOwenrChairID != INVALID_CHAIR)
				{
					m_wBankerUser = wRoomOwenrChairID;
				}
				else      //房主不参与游戏
				{
					m_wBankerUser = m_wFirstEnterRoomCard;
				}

				m_wCurrentUser=INVALID_CHAIR;

				m_bBuckleServiceCharge[m_wBankerUser]=true;

				//设置状态
				m_pITableFrame->SetGameStatus(GS_TK_SCORE);

				//更新房间用户信息
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					//获取用户
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
					if (pIServerUserItem != NULL)
					{
						UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
					}
				}

				//庄家积分
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(m_wBankerUser);
				LONGLONG lBankerScore = pIServerUserItem->GetUserScore();

				//最大下注
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					if (m_cbPlayStatus[i] != TRUE || i == m_wBankerUser)
					{
						continue;
					}

					//下注变量
					m_lTurnMaxScore[i] = GetUserMaxTurnScore(i);
				}

				//设置变量
				CMD_S_GameStart GameStart;
				ZeroMemory(&GameStart, sizeof(GameStart));
				GameStart.wBankerUser=m_wBankerUser;
				GameStart.lTurnMaxScore=0;
				GameStart.banker_config = DESPOT_BANKER;
				GameStart.bRoomCardScore = IsRoomCardScoreType();
				CopyMemory(GameStart.cbPlayerStatus, m_cbPlayStatus, sizeof(m_cbPlayStatus));
				
				//积分类型
				if (IsRoomCardScoreType())
				{
					//设置文件名
					TCHAR szPath[MAX_PATH] = TEXT("");
					TCHAR szFileName[MAX_PATH] = TEXT("");
					GetCurrentDirectory(sizeof(szPath),szPath);
					_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

					//读取配置
					LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
					ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
					lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
					lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
					lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
					lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

					CopyMemory(GameStart.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
				}

				//发送数据
				for (WORD i=0;i<m_wPlayerCount;i++)
				{
					if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
					GameStart.lTurnMaxScore=m_lTurnMaxScore[i];
					m_pITableFrame->SendTableData(i,SUB_S_GAME_START,&GameStart,sizeof(GameStart));
				}
				m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_START,&GameStart,sizeof(GameStart));

				break;
			}
			//传统庄
		case NORMAL_BANKER:
			{
				//设置状态
				m_pITableFrame->SetGameStatus(GS_TK_CALL);

				//首局随机始叫
				if(m_wFisrtCallUser==INVALID_CHAIR)
				{
					m_wFisrtCallUser=rand()%m_wPlayerCount;
				}
				else
				{
					m_wFisrtCallUser=(m_wFisrtCallUser+1)%m_wPlayerCount;
				}

				//始叫用户
				while(m_cbPlayStatus[m_wFisrtCallUser]!=TRUE)
				{
					m_wFisrtCallUser=(m_wFisrtCallUser+1)%m_wPlayerCount;
				}

				//当前用户
				m_wCurrentUser=m_wFisrtCallUser;

				//设置变量
				CMD_S_CallBanker CallBanker;
				CallBanker.wCallBanker=m_wCurrentUser;
				CallBanker.bFirstTimes=true;
				CopyMemory(CallBanker.cbPlayerStatus,m_cbPlayStatus,sizeof(m_cbPlayStatus));
				//发送数据
				for (WORD i=0;i<m_wPlayerCount;i++)
				{
					if(m_cbPlayStatus[i]!=TRUE) continue;
					m_pITableFrame->SendTableData(i,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
				}
				m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
				//m_pITableFrame->SetGameTimer(IDI_SO_OPERATE,TIME_SO_OPERATE,1,0);

				break;
			}
			//随机庄
		case RANDOM_BANKER:
			{
				ASSERT (m_wFirstEnterRoomCard !=INVALID_CHAIR);
				
				//随机坐庄
				BYTE cbRandomArray[GAME_PLAYER];
				memset(cbRandomArray, INVALID_BYTE, sizeof(cbRandomArray));
				BYTE cbRandomIndex = 0;
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					//获取用户
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
					if (!pIServerUserItem)
					{
						continue;
					}

					m_cbCallStatus[i] = TRUE;
					cbRandomArray[cbRandomIndex++] = i;
				}

				m_wBankerUser = cbRandomArray[rand() % cbRandomIndex];

				m_wCurrentUser=INVALID_CHAIR;

				m_bBuckleServiceCharge[m_wBankerUser]=true;

				//设置状态
				m_pITableFrame->SetGameStatus(GS_TK_SCORE);

				//更新房间用户信息
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					//获取用户
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
					if (pIServerUserItem != NULL)
					{
						UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
					}
				}

				//庄家积分
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(m_wBankerUser);
				LONGLONG lBankerScore = pIServerUserItem->GetUserScore();

				//最大下注
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					if (m_cbPlayStatus[i] != TRUE || i == m_wBankerUser)
					{
						continue;
					}

					//下注变量
					m_lTurnMaxScore[i] = GetUserMaxTurnScore(i);
				}

				//设置变量
				CMD_S_GameStart GameStart;
				GameStart.wBankerUser=m_wBankerUser;
				GameStart.lTurnMaxScore=0;
				GameStart.banker_config = RANDOM_BANKER;
				GameStart.bRoomCardScore = IsRoomCardScoreType();
				CopyMemory(GameStart.cbPlayerStatus, m_cbPlayStatus, sizeof(m_cbPlayStatus));

				//积分类型
				if (IsRoomCardScoreType())
				{
					//设置文件名
					TCHAR szPath[MAX_PATH] = TEXT("");
					TCHAR szFileName[MAX_PATH] = TEXT("");
					GetCurrentDirectory(sizeof(szPath),szPath);
					_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

					//读取配置
					LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
					ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
					lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
					lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
					lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
					lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

					CopyMemory(GameStart.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
				}

				//发送数据
				for (WORD i=0;i<m_wPlayerCount;i++)
				{
					if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
					GameStart.lTurnMaxScore=m_lTurnMaxScore[i];
					m_pITableFrame->SendTableData(i,SUB_S_GAME_START,&GameStart,sizeof(GameStart));
				}
				m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_START,&GameStart,sizeof(GameStart));

				break;
			}
		default:
			break;
		}
	}
	else	//非房卡类型
	{
		//设置状态
		m_pITableFrame->SetGameStatus(GS_TK_CALL);

		//首局随机始叫
		if(m_wFisrtCallUser==INVALID_CHAIR)
		{
			m_wFisrtCallUser=rand()%m_wPlayerCount;
		}
		else
		{
			m_wFisrtCallUser=(m_wFisrtCallUser+1)%m_wPlayerCount;
		}

		//始叫用户
		while(m_cbPlayStatus[m_wFisrtCallUser]!=TRUE)
		{
			m_wFisrtCallUser=(m_wFisrtCallUser+1)%m_wPlayerCount;
		}

		//当前用户
		m_wCurrentUser=m_wFisrtCallUser;

		//设置变量
		CMD_S_CallBanker CallBanker;
		CallBanker.wCallBanker=m_wCurrentUser;
		CallBanker.bFirstTimes=true;
		CopyMemory(CallBanker.cbPlayerStatus,m_cbPlayStatus,sizeof(m_cbPlayStatus));
		//发送数据
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]!=TRUE) continue;
			m_pITableFrame->SendTableData(i,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
		}
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
		m_pITableFrame->SetGameTimer(IDI_SO_OPERATE,TIME_SO_OPERATE,1,0);
	}

	//随机扑克
	CMD_S_AllCard AllCard={0};
	BYTE bTempArray[GAME_PLAYER*MAX_COUNT];
	m_GameLogic.RandCardList(bTempArray,sizeof(bTempArray));
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		IServerUserItem *pIServerUser=m_pITableFrame->GetTableUserItem(i);	
		if(pIServerUser==NULL)continue;
		if(pIServerUser->IsAndroidUser())AllCard.bAICount[i] =true;

		//派发扑克
		CopyMemory(m_cbHandCardData[i],&bTempArray[i*MAX_COUNT],MAX_COUNT);
		CopyMemory(AllCard.cbCardData[i],&bTempArray[i*MAX_COUNT],MAX_COUNT);
	}

	return true;
}

//游戏结束
bool CTableFrameSink::OnEventGameConclude(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason)
{
	switch (cbReason)
	{
	case GER_DISMISS:		//游戏解散
		{
			////效验参数
			//ASSERT(pIServerUserItem!=NULL);
			//ASSERT(wChairID<GAME_PLAYER);

			//构造数据
			CMD_S_GameEnd GameEnd = {0};

			//发送信息
			m_pITableFrame->SendTableData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
			m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));

			//结束游戏
			m_pITableFrame->ConcludeGame(GAME_STATUS_FREE);
			m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);

			//更新房间用户信息
			for (WORD i=0; i<m_wPlayerCount; i++)
			{
				//获取用户
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);

				if (!pIServerUserItem)
				{
					continue;
				}
				
				UpdateRoomUserInfo(pIServerUserItem, USER_STANDUP);
			}

			return true;
		}
	case GER_NORMAL:		//常规结束
		{
			//删除时间
			m_pITableFrame->KillGameTimer(IDI_SO_OPERATE);

			//定义变量
			CMD_S_GameEnd GameEnd;
			ZeroMemory(&GameEnd,sizeof(GameEnd));
			WORD wWinTimes[GAME_PLAYER],wWinCount[GAME_PLAYER];
			ZeroMemory(wWinCount,sizeof(wWinCount));
			ZeroMemory(wWinTimes,sizeof(wWinTimes));

			//保存扑克
			BYTE cbUserCardData[GAME_PLAYER][MAX_COUNT];
			CopyMemory(cbUserCardData,m_cbHandCardData,sizeof(cbUserCardData));

			//庄家倍数
			ASSERT(m_cbOxCard[m_wBankerUser]<2);
			if(m_cbOxCard[m_wBankerUser]==TRUE)
				wWinTimes[m_wBankerUser]=m_GameLogic.GetTimes(cbUserCardData[m_wBankerUser],MAX_COUNT);
			else wWinTimes[m_wBankerUser]=1;

			//对比玩家
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(i==m_wBankerUser || m_cbPlayStatus[i]==FALSE)continue;

				ASSERT(m_cbOxCard[i]<2);

				//对比扑克
				if (m_GameLogic.CompareCard(cbUserCardData[i],cbUserCardData[m_wBankerUser],MAX_COUNT,m_cbOxCard[i],m_cbOxCard[m_wBankerUser])) 
				{
					wWinCount[i]++;
					//获取倍数
					if(m_cbOxCard[i]==TRUE)
						wWinTimes[i]=m_GameLogic.GetTimes(cbUserCardData[i],MAX_COUNT);
					else wWinTimes[i]=1;
				}
				else
				{
					wWinCount[m_wBankerUser]++;
				}
			}

			//统计得分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(i==m_wBankerUser || m_cbPlayStatus[i]==FALSE)continue;

				if(wWinCount[i]>0)	//闲家胜利
				{
					GameEnd.lGameScore[i]=m_lTableScore[i]*wWinTimes[i];
					GameEnd.lGameScore[m_wBankerUser]-=GameEnd.lGameScore[i];
					m_lTableScore[i]=0;
				}
				else					//庄家胜利
				{
					GameEnd.lGameScore[i]=(-1)*m_lTableScore[i]*wWinTimes[m_wBankerUser];
					GameEnd.lGameScore[m_wBankerUser]+=(-1)*GameEnd.lGameScore[i];
					m_lTableScore[i]=0;
				}
			}

			//闲家强退分数	
			GameEnd.lGameScore[m_wBankerUser]+=m_lExitScore;

			//离开用户
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(m_lTableScore[i]>0)GameEnd.lGameScore[i]=-m_lTableScore[i];
			}

			//修改积分
			tagScoreInfo ScoreInfoArray[GAME_PLAYER];
			ZeroMemory(ScoreInfoArray,sizeof(ScoreInfoArray));

			bool bDelayOverGame=false;

			//积分税收
			for(WORD i=0;i<m_wPlayerCount;i++)
			{
				if(m_cbPlayStatus[i]==FALSE)continue;

				if(GameEnd.lGameScore[i]>0L)
				{
					GameEnd.lGameTax[i] = m_pITableFrame->CalculateRevenue(i,GameEnd.lGameScore[i]);
					if(GameEnd.lGameTax[i]>0)
						GameEnd.lGameScore[i] -= GameEnd.lGameTax[i];
				}

				//历史积分
				m_HistoryScore.OnEventUserScore(i,GameEnd.lGameScore[i]);

				//保存积分
				ScoreInfoArray[i].lScore = GameEnd.lGameScore[i];
				ScoreInfoArray[i].lRevenue = GameEnd.lGameTax[i];
				ScoreInfoArray[i].cbType = (GameEnd.lGameScore[i]>0L)?SCORE_TYPE_WIN:SCORE_TYPE_LOSE;

				if(ScoreInfoArray[i].cbType ==SCORE_TYPE_LOSE && bDelayOverGame==false)
				{
					IServerUserItem *pUserItem=m_pITableFrame->GetTableUserItem(i);
					if(pUserItem!=NULL && (pUserItem->GetUserScore()+GameEnd.lGameScore[i] )<m_pGameServiceOption->lMinTableScore)
					{
	    				bDelayOverGame=true;
					}
				}
			}

#ifdef _DEBUG

			bDelayOverGame=true;

#endif // _DEBUG

			if(bDelayOverGame)
			{
				GameEnd.cbDelayOverGame = 3;
			}
			
			TryWriteTableScore(ScoreInfoArray);

			//发送信息
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE) continue;
				m_pITableFrame->SendTableData(i,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
			}

			m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
			
			
			
			//库存统计
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				//获取用户
				IServerUserItem * pIServerUserIte=m_pITableFrame->GetTableUserItem(i);
				if (pIServerUserIte==NULL) continue;

				//库存累计
				if(!pIServerUserIte->IsAndroidUser())
					g_lRoomStorageCurrent-=GameEnd.lGameScore[i];

			}
			

			//检查机器人存储款
			//for (WORD i=0;i<GAME_PLAYER;i++)
			//{
			//	//获取用户
			//	IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
			//	if(!pIServerUserItem)
			//		continue;
			//	if(!pIServerUserItem->IsAndroidUser())
			//		continue;

			//	if (m_cbDynamicJoin[i] == TRUE)
			//	{
			//		continue;
			//	}

			//	m_pITableFrame->SendTableData(i,SUB_S_ANDROID_BANKOPERATOR);
			//}

			//结束游戏
			if(bDelayOverGame)
			{
				ZeroMemory(m_cbPlayStatus,sizeof(m_cbPlayStatus));
				m_pITableFrame->SetGameTimer(IDI_DELAY_ENDGAME,IDI_DELAY_TIME,1,0L);
			}
			else
			{
				m_pITableFrame->ConcludeGame(GS_TK_FREE);
				m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);

				bool bStart = false;
				//更新房间用户信息
				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					//获取用户
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);

					if (!pIServerUserItem)
					{
						continue;
					}
					
					UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);

					if(!pIServerUserItem->IsAndroidUser())
						continue;

					if (m_cbDynamicJoin[i] == TRUE)
					{
						continue;
					}

					m_pITableFrame->SendTableData(i,SUB_S_ANDROID_BANKOPERATOR, &bStart, sizeof(bStart));
				}
			}

			//发送库存
			CString strInfo;
			strInfo.Format(TEXT("当前库存：%I64d"), g_lRoomStorageCurrent);
			for (WORD i=0; i<m_wPlayerCount; i++)
			{
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
				if (!pIServerUserItem)
				{
					continue;
				}
				if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
				{
					m_pITableFrame->SendGameMessage(pIServerUserItem, strInfo, SMT_CHAT);

					CMD_S_ADMIN_STORAGE_INFO StorageInfo;
					ZeroMemory(&StorageInfo, sizeof(StorageInfo));
					StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
					StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
					StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
					StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
					StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
					StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
					StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
					m_pITableFrame->SendTableData(i, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
					m_pITableFrame->SendLookonData(i, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				}
			}
			
			return true;
		}
	case GER_USER_LEAVE:		//用户强退
	case GER_NETWORK_ERROR:
		{
			//效验参数
			ASSERT(pIServerUserItem!=NULL);
			ASSERT(wChairID<m_wPlayerCount && (m_cbPlayStatus[wChairID]==TRUE||m_cbDynamicJoin[wChairID]==FALSE));

			if(m_cbPlayStatus[wChairID]==FALSE) return true;
			//设置状态
			m_cbPlayStatus[wChairID]=FALSE;
			m_cbDynamicJoin[wChairID]=FALSE;

			//定义变量
			CMD_S_PlayerExit PlayerExit;
			PlayerExit.wPlayerID=wChairID;

			//发送信息
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(i==wChairID || (m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE))continue;
				m_pITableFrame->SendTableData(i,SUB_S_PLAYER_EXIT,&PlayerExit,sizeof(PlayerExit));
			}
			m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_PLAYER_EXIT,&PlayerExit,sizeof(PlayerExit));


			WORD wWinTimes[GAME_PLAYER];
			
			ZeroMemory(wWinTimes,sizeof(wWinTimes));

			if (m_pITableFrame->GetGameStatus()>GS_TK_CALL)
			{
				if (wChairID==m_wBankerUser)	//庄家强退
				{
					//定义变量
					CMD_S_GameEnd GameEnd;
					ZeroMemory(&GameEnd,sizeof(GameEnd));
					ZeroMemory(wWinTimes,sizeof(wWinTimes));

					BYTE cbUserCardData[GAME_PLAYER][MAX_COUNT];
					CopyMemory(cbUserCardData,m_cbHandCardData,sizeof(cbUserCardData));

					//得分倍数
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(i==m_wBankerUser || m_cbPlayStatus[i]==FALSE)continue;
						wWinTimes[i]=(m_pITableFrame->GetGameStatus()!=GS_TK_PLAYING)?(1):(m_GameLogic.GetTimes(cbUserCardData[i],MAX_COUNT));
					}

					//统计得分 已下或没下
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(i==m_wBankerUser || m_cbPlayStatus[i]==FALSE)continue;
						GameEnd.lGameScore[i]=m_lTableScore[i]*wWinTimes[i];
						GameEnd.lGameScore[m_wBankerUser]-=GameEnd.lGameScore[i];
						m_lTableScore[i]=0;
					}

					//闲家强退分数 
					GameEnd.lGameScore[m_wBankerUser]+=m_lExitScore;

					//离开用户
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_lTableScore[i]>0)GameEnd.lGameScore[i]=-m_lTableScore[i];
					}

					//修改积分
					tagScoreInfo ScoreInfoArray[GAME_PLAYER];
					ZeroMemory(&ScoreInfoArray,sizeof(ScoreInfoArray));

					//积分税收
					for(WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_cbPlayStatus[i]==FALSE && i!=m_wBankerUser)continue;
						
						if(GameEnd.lGameScore[i]>0L)
						{
							GameEnd.lGameTax[i]=m_pITableFrame->CalculateRevenue(i,GameEnd.lGameScore[i]);
							if(GameEnd.lGameTax[i]>0)
								GameEnd.lGameScore[i]-=GameEnd.lGameTax[i];
						}

						//保存积分
						ScoreInfoArray[i].lRevenue = GameEnd.lGameTax[i];
						ScoreInfoArray[i].lScore = GameEnd.lGameScore[i];

						if(i==m_wBankerUser)
							ScoreInfoArray[i].cbType =SCORE_TYPE_FLEE;
						else if(m_cbPlayStatus[i]==TRUE)
							ScoreInfoArray[i].cbType =(GameEnd.lGameScore[i]>0L)?SCORE_TYPE_WIN:SCORE_TYPE_LOSE;
					}
					
					TryWriteTableScore(ScoreInfoArray);

					//发送信息
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(i==m_wBankerUser || (m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE))continue;
						m_pITableFrame->SendTableData(i,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
					}
					m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
						
					//写入库存
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_cbPlayStatus[i]==FALSE && i!=m_wBankerUser)continue;

						//获取用户
						IServerUserItem * pIServerUserIte=m_pITableFrame->GetTableUserItem(i);

						//库存累计
						if ((pIServerUserIte!=NULL)&&(!pIServerUserIte->IsAndroidUser())) 
							g_lRoomStorageCurrent-=GameEnd.lGameScore[i];

					}
					//结束游戏
					m_pITableFrame->ConcludeGame(GS_TK_FREE);
					m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);
				}
				else						//闲家强退
				{
					//已经下注
					if (m_lTableScore[wChairID]>0L)
					{
						ZeroMemory(wWinTimes,sizeof(wWinTimes));

						//用户扑克
						BYTE cbUserCardData[MAX_COUNT];
						CopyMemory(cbUserCardData,m_cbHandCardData[m_wBankerUser],MAX_COUNT);

						//用户倍数
						wWinTimes[m_wBankerUser]=(m_pITableFrame->GetGameStatus()==GS_TK_SCORE)?(1):(m_GameLogic.GetTimes(cbUserCardData,MAX_COUNT));

						//修改积分
						LONGLONG lScore=-m_lTableScore[wChairID]*wWinTimes[m_wBankerUser];
						m_lExitScore+=(-1*lScore);
						m_lTableScore[wChairID]=(-1*lScore);
						
						tagScoreInfo ScoreInfoArray[GAME_PLAYER];
						ZeroMemory(ScoreInfoArray,sizeof(ScoreInfoArray));
						ScoreInfoArray[wChairID].lScore = lScore;
						ScoreInfoArray[wChairID].cbType = SCORE_TYPE_FLEE;
						
						TryWriteTableScore(ScoreInfoArray);

						//获取用户
						IServerUserItem * pIServerUserIte=m_pITableFrame->GetTableUserItem(wChairID);
						
						//库存累计
						if ((pIServerUserIte!=NULL)&&(!pIServerUserIte->IsAndroidUser())) 
							g_lRoomStorageCurrent-=lScore;
					}

					//玩家人数
					WORD wUserCount=0;
					for (WORD i=0;i<m_wPlayerCount;i++)if(m_cbPlayStatus[i]==TRUE)wUserCount++;

					//结束游戏
					if(wUserCount==1)
					{
						//定义变量
						CMD_S_GameEnd GameEnd;
						ZeroMemory(&GameEnd,sizeof(GameEnd));
						ASSERT(m_lExitScore>=0L); 

						//统计得分
						GameEnd.lGameScore[m_wBankerUser]+=m_lExitScore;
						GameEnd.lGameTax[m_wBankerUser]=m_pITableFrame->CalculateRevenue(m_wBankerUser,GameEnd.lGameScore[m_wBankerUser]);
						GameEnd.lGameScore[m_wBankerUser]-=GameEnd.lGameTax[m_wBankerUser];

						//离开用户
						for (WORD i=0;i<m_wPlayerCount;i++)
						{
							if(m_lTableScore[i]>0)GameEnd.lGameScore[i]=-m_lTableScore[i];
						}

						//发送信息
						m_pITableFrame->SendTableData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
						m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));

						WORD Zero = 0;
						for (Zero=0;Zero<m_wPlayerCount;Zero++)if(m_lTableScore[Zero]!=0)break;
						if(Zero!=m_wPlayerCount)
						{
							//修改积分
							tagScoreInfo ScoreInfoArray[GAME_PLAYER];
							ZeroMemory(&ScoreInfoArray,sizeof(ScoreInfoArray));
							ScoreInfoArray[m_wBankerUser].lScore=GameEnd.lGameScore[m_wBankerUser];
							ScoreInfoArray[m_wBankerUser].lRevenue = GameEnd.lGameTax[m_wBankerUser];
							ScoreInfoArray[m_wBankerUser].cbType = SCORE_TYPE_WIN;

							TryWriteTableScore(ScoreInfoArray);

							//获取用户
							IServerUserItem * pIServerUserIte=m_pITableFrame->GetTableUserItem(wChairID);
							
							//库存累计
							if ((pIServerUserIte!=NULL)&&(!pIServerUserIte->IsAndroidUser())) 
								g_lRoomStorageCurrent-=GameEnd.lGameScore[m_wBankerUser];

						}

						//结束游戏
						m_pITableFrame->ConcludeGame(GS_TK_FREE);
						m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);
					}
					else if	(m_pITableFrame->GetGameStatus()==GS_TK_SCORE && m_lTableScore[wChairID]==0L)
					{
						OnUserAddScore(wChairID,0);
					}
					else if (m_pITableFrame->GetGameStatus()==GS_TK_PLAYING && m_cbOxCard[wChairID]==0xff)
					{
						OnUserOpenCard(wChairID,0);
					}
				}
			}
			else 
			{
				//玩家人数
				WORD wUserCount=0;
				for (WORD i=0;i<m_wPlayerCount;i++)if(m_cbPlayStatus[i]==TRUE)wUserCount++;

				//结束游戏
				if(wUserCount==1)
				{
					//定义变量
					CMD_S_GameEnd GameEnd;
					ZeroMemory(&GameEnd,sizeof(GameEnd));

					//发送信息
					for (WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
						m_pITableFrame->SendTableData(i,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
					}
					m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));

					//结束游戏
					m_pITableFrame->ConcludeGame(GS_TK_FREE);
					m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);
				}
				else if(m_wCurrentUser==wChairID)OnUserCallBanker(wChairID,0);
			}

			//发送库存
			CString strInfo;
			strInfo.Format(TEXT("当前库存：%I64d"), g_lRoomStorageCurrent);
			for (WORD i=0; i<m_wPlayerCount; i++)
			{
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
				if (!pIServerUserItem)
				{
					continue;
				}
				if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
				{
					m_pITableFrame->SendGameMessage(pIServerUserItem, strInfo, SMT_CHAT);

					CMD_S_ADMIN_STORAGE_INFO StorageInfo;
					ZeroMemory(&StorageInfo, sizeof(StorageInfo));
					StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
					StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
					StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
					StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
					StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
					StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
					StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
					m_pITableFrame->SendTableData(i, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
					m_pITableFrame->SendLookonData(i, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				}
			}
			
			UpdateRoomUserInfo(pIServerUserItem, USER_STANDUP);

			//更新房间用户信息
			for (WORD i=0; i<m_wPlayerCount; i++)
			{
				if (i == wChairID)
				{
					continue;
				}

				//获取用户
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);

				if (!pIServerUserItem)
				{
					continue;
				}
				
				UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
			}

			return true;
		}
	}

	return false;
}

//发送场景
bool CTableFrameSink::OnEventSendGameScene(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbGameStatus, bool bSendSecret)
{
	switch (cbGameStatus)
	{
	case GAME_STATUS_FREE:		//空闲状态
		{
			//构造数据
			CMD_S_StatusFree StatusFree;
			ZeroMemory(&StatusFree,sizeof(StatusFree));

			//设置变量
			StatusFree.lCellScore=0L;
			StatusFree.lRoomStorageStart = g_lRoomStorageStart;
			StatusFree.lRoomStorageCurrent = g_lRoomStorageCurrent;
			
			StatusFree.banker_config = GetBankerConfig();
			StatusFree.bRoomCardScore = IsRoomCardScoreType();

			if (IsRoomCardScoreType())
			{
				//设置文件名
				TCHAR szPath[MAX_PATH] = TEXT("");
				TCHAR szFileName[MAX_PATH] = TEXT("");
				GetCurrentDirectory(sizeof(szPath),szPath);
				_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

				//读取配置
				LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
				ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
				lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
				lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
				lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
				lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

				CopyMemory(StatusFree.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
			}

			//历史积分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				tagHistoryScore * pHistoryScore=m_HistoryScore.GetHistoryScore(i);
				StatusFree.lTurnScore[i]=pHistoryScore->lTurnScore;
				StatusFree.lCollectScore[i]=pHistoryScore->lCollectScore;
			}

			
			//获取自定义配置
			tagCustomRule *pCustomRule = (tagCustomRule *)m_pGameServiceOption->cbCustomRule;
			ASSERT(pCustomRule);
			tagCustomAndroid CustomAndroid;
			ZeroMemory(&CustomAndroid, sizeof(CustomAndroid));
			CustomAndroid.lRobotBankGet = pCustomRule->lRobotBankGet;
			CustomAndroid.lRobotBankGetBanker = pCustomRule->lRobotBankGetBanker;
			CustomAndroid.lRobotBankStoMul = pCustomRule->lRobotBankStoMul;
			CustomAndroid.lRobotScoreMax = pCustomRule->lRobotScoreMax;
			CustomAndroid.lRobotScoreMin = pCustomRule->lRobotScoreMin;
			CopyMemory(&StatusFree.CustomAndroid, &CustomAndroid, sizeof(CustomAndroid));
			
			//防作弊
			if(CServerRule::IsAllowAvertCheatMode(m_pGameServiceOption->dwServerRule))
			{
				StatusFree.bIsAllowAvertCheat = true;
			}

			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
			{
				CMD_S_ADMIN_STORAGE_INFO StorageInfo;
				ZeroMemory(&StorageInfo, sizeof(StorageInfo));
				StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
				StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
				StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
				StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
				StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
				StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
				StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
				
				m_pITableFrame->SendTableData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				m_pITableFrame->SendLookonData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
			}

			//发送场景
			return m_pITableFrame->SendGameScene(pIServerUserItem,&StatusFree,sizeof(StatusFree));
		}
	case GS_TK_CALL:	//叫庄状态
		{
			//构造数据
			CMD_S_StatusCall StatusCall;
			ZeroMemory(&StatusCall,sizeof(StatusCall));

			//历史积分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				tagHistoryScore * pHistoryScore=m_HistoryScore.GetHistoryScore(i);
				StatusCall.lTurnScore[i]=pHistoryScore->lTurnScore;
				StatusCall.lCollectScore[i]=pHistoryScore->lCollectScore;
			}

			//设置变量
			StatusCall.wCallBanker=m_wCurrentUser;
			StatusCall.cbDynamicJoin=m_cbDynamicJoin[wChairID];
			CopyMemory(StatusCall.cbPlayStatus,m_cbPlayStatus,sizeof(StatusCall.cbPlayStatus));
			StatusCall.lRoomStorageStart = g_lRoomStorageStart;
			StatusCall.lRoomStorageCurrent = g_lRoomStorageCurrent;

			StatusCall.banker_config = GetBankerConfig();
			StatusCall.bRoomCardScore = IsRoomCardScoreType();

			if (IsRoomCardScoreType())
			{
				//设置文件名
				TCHAR szPath[MAX_PATH] = TEXT("");
				TCHAR szFileName[MAX_PATH] = TEXT("");
				GetCurrentDirectory(sizeof(szPath),szPath);
				_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

				//读取配置
				LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
				ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
				lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
				lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
				lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
				lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

				CopyMemory(StatusCall.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
			}

			//获取自定义配置
			tagCustomRule *pCustomRule = (tagCustomRule *)m_pGameServiceOption->cbCustomRule;
			ASSERT(pCustomRule);
			tagCustomAndroid CustomAndroid;
			ZeroMemory(&CustomAndroid, sizeof(CustomAndroid));
			CustomAndroid.lRobotBankGet = pCustomRule->lRobotBankGet;
			CustomAndroid.lRobotBankGetBanker = pCustomRule->lRobotBankGetBanker;
			CustomAndroid.lRobotBankStoMul = pCustomRule->lRobotBankStoMul;
			CustomAndroid.lRobotScoreMax = pCustomRule->lRobotScoreMax;
			CustomAndroid.lRobotScoreMin = pCustomRule->lRobotScoreMin;
			CopyMemory(&StatusCall.CustomAndroid, &CustomAndroid, sizeof(CustomAndroid));
			
			//防作弊
			if(CServerRule::IsAllowAvertCheatMode(m_pGameServiceOption->dwServerRule))
			{
				StatusCall.bIsAllowAvertCheat = true;
			}

			//更新房间用户信息
			UpdateRoomUserInfo(pIServerUserItem, USER_RECONNECT);
			
			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
			{
				CMD_S_ADMIN_STORAGE_INFO StorageInfo;
				ZeroMemory(&StorageInfo, sizeof(StorageInfo));
				StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
				StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
				StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
				StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
				StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
				StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
				StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
				m_pITableFrame->SendTableData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				m_pITableFrame->SendLookonData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
			}

			//发送场景
			return m_pITableFrame->SendGameScene(pIServerUserItem,&StatusCall,sizeof(StatusCall));
		}
	case GS_TK_SCORE:	//下注状态
		{
			//构造数据
			CMD_S_StatusScore StatusScore;
			memset(&StatusScore,0,sizeof(StatusScore));

			//历史积分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				tagHistoryScore * pHistoryScore=m_HistoryScore.GetHistoryScore(i);
				StatusScore.lTurnScore[i]=pHistoryScore->lTurnScore;
				StatusScore.lCollectScore[i]=pHistoryScore->lCollectScore;
			}

			//加注信息
			StatusScore.lTurnMaxScore=GetUserMaxTurnScore(wChairID);
			StatusScore.wBankerUser=m_wBankerUser;
			StatusScore.cbDynamicJoin=m_cbDynamicJoin[wChairID];
			CopyMemory(StatusScore.cbPlayStatus,m_cbPlayStatus,sizeof(StatusScore.cbPlayStatus));
			StatusScore.lRoomStorageStart = g_lRoomStorageStart;
			StatusScore.lRoomStorageCurrent = g_lRoomStorageCurrent;

			StatusScore.banker_config = GetBankerConfig();
			StatusScore.bRoomCardScore = IsRoomCardScoreType();

			if (IsRoomCardScoreType())
			{
				//设置文件名
				TCHAR szPath[MAX_PATH] = TEXT("");
				TCHAR szFileName[MAX_PATH] = TEXT("");
				GetCurrentDirectory(sizeof(szPath),szPath);
				_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

				//读取配置
				LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
				ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
				lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
				lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
				lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
				lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

				CopyMemory(StatusScore.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
			}

			//设置积分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				if(m_cbPlayStatus[i]==FALSE)continue;
				StatusScore.lTableScore[i]=m_lTableScore[i];
			}

			
			//获取自定义配置
			tagCustomRule *pCustomRule = (tagCustomRule *)m_pGameServiceOption->cbCustomRule;
			ASSERT(pCustomRule);
			tagCustomAndroid CustomAndroid;
			ZeroMemory(&CustomAndroid, sizeof(CustomAndroid));
			CustomAndroid.lRobotBankGet = pCustomRule->lRobotBankGet;
			CustomAndroid.lRobotBankGetBanker = pCustomRule->lRobotBankGetBanker;
			CustomAndroid.lRobotBankStoMul = pCustomRule->lRobotBankStoMul;
			CustomAndroid.lRobotScoreMax = pCustomRule->lRobotScoreMax;
			CustomAndroid.lRobotScoreMin = pCustomRule->lRobotScoreMin;
			CopyMemory(&StatusScore.CustomAndroid, &CustomAndroid, sizeof(CustomAndroid));
			
			//防作弊
			if(CServerRule::IsAllowAvertCheatMode(m_pGameServiceOption->dwServerRule))
			{
				StatusScore.bIsAllowAvertCheat = true;
			}

			//更新房间用户信息
			UpdateRoomUserInfo(pIServerUserItem, USER_RECONNECT);
			
			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
			{
				CMD_S_ADMIN_STORAGE_INFO StorageInfo;
				ZeroMemory(&StorageInfo, sizeof(StorageInfo));
				StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
				StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
				StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
				StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
				StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
				StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
				StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
				m_pITableFrame->SendTableData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				m_pITableFrame->SendLookonData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
			}

			//发送场景
			return m_pITableFrame->SendGameScene(pIServerUserItem,&StatusScore,sizeof(StatusScore));
		}
	case GS_TK_PLAYING:	//游戏状态
		{
			//构造数据
			CMD_S_StatusPlay StatusPlay;
			memset(&StatusPlay,0,sizeof(StatusPlay));

			//历史积分
			for (WORD i=0;i<m_wPlayerCount;i++)
			{
				tagHistoryScore * pHistoryScore=m_HistoryScore.GetHistoryScore(i);
				StatusPlay.lTurnScore[i]=pHistoryScore->lTurnScore;
				StatusPlay.lCollectScore[i]=pHistoryScore->lCollectScore;
			}

			//设置信息
			StatusPlay.lTurnMaxScore=GetUserMaxTurnScore(wChairID);
			StatusPlay.wBankerUser=m_wBankerUser;
			StatusPlay.cbDynamicJoin=m_cbDynamicJoin[wChairID];
			CopyMemory(StatusPlay.bOxCard,m_cbOxCard,sizeof(StatusPlay.bOxCard));
			CopyMemory(StatusPlay.cbPlayStatus,m_cbPlayStatus,sizeof(StatusPlay.cbPlayStatus));
			StatusPlay.lRoomStorageStart = g_lRoomStorageStart;
			StatusPlay.lRoomStorageCurrent = g_lRoomStorageCurrent;
			
			StatusPlay.banker_config = GetBankerConfig();
			StatusPlay.bRoomCardScore = IsRoomCardScoreType();

			if (IsRoomCardScoreType())
			{
				//设置文件名
				TCHAR szPath[MAX_PATH] = TEXT("");
				TCHAR szFileName[MAX_PATH] = TEXT("");
				GetCurrentDirectory(sizeof(szPath),szPath);
				_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

				//读取配置
				LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
				ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
				lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
				lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
				lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
				lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

				CopyMemory(StatusPlay.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
			}

			//获取自定义配置
			tagCustomRule *pCustomRule = (tagCustomRule *)m_pGameServiceOption->cbCustomRule;
			ASSERT(pCustomRule);
			tagCustomAndroid CustomAndroid;
			ZeroMemory(&CustomAndroid, sizeof(CustomAndroid));
			CustomAndroid.lRobotBankGet = pCustomRule->lRobotBankGet;
			CustomAndroid.lRobotBankGetBanker = pCustomRule->lRobotBankGetBanker;
			CustomAndroid.lRobotBankStoMul = pCustomRule->lRobotBankStoMul;
			CustomAndroid.lRobotScoreMax = pCustomRule->lRobotScoreMax;
			CustomAndroid.lRobotScoreMin = pCustomRule->lRobotScoreMin;
			CopyMemory(&StatusPlay.CustomAndroid, &CustomAndroid, sizeof(CustomAndroid));
			
			//防作弊
			if(CServerRule::IsAllowAvertCheatMode(m_pGameServiceOption->dwServerRule))
			{
				StatusPlay.bIsAllowAvertCheat = true;
			}

			//设置扑克
			for (WORD i=0;i< m_wPlayerCount;i++)
			{
				if(m_cbPlayStatus[i]==FALSE)continue;
				WORD j= i;
				StatusPlay.lTableScore[j]=m_lTableScore[j];
				CopyMemory(StatusPlay.cbHandCardData[j],m_cbHandCardData[j],MAX_COUNT);
			}
			
			//更新房间用户信息
			UpdateRoomUserInfo(pIServerUserItem, USER_RECONNECT);
			
			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) && !pIServerUserItem->IsAndroidUser())
			{
				CMD_S_ADMIN_STORAGE_INFO StorageInfo;
				ZeroMemory(&StorageInfo, sizeof(StorageInfo));
				StorageInfo.lRoomStorageStart = g_lRoomStorageStart;
				StorageInfo.lRoomStorageCurrent = g_lRoomStorageCurrent;
				StorageInfo.lRoomStorageDeduct = g_lStorageDeductRoom;
				StorageInfo.lMaxRoomStorage[0] = g_lStorageMax1Room;
				StorageInfo.lMaxRoomStorage[1] = g_lStorageMax2Room;
				StorageInfo.wRoomStorageMul[0] = (WORD)g_lStorageMul1Room;
				StorageInfo.wRoomStorageMul[1] = (WORD)g_lStorageMul2Room;
				m_pITableFrame->SendTableData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
				m_pITableFrame->SendLookonData(wChairID, SUB_S_ADMIN_STORAGE_INFO, &StorageInfo, sizeof(StorageInfo));
			}

			//发送场景
			return m_pITableFrame->SendGameScene(pIServerUserItem,&StatusPlay,sizeof(StatusPlay));
		}
	}
	//效验错误
	ASSERT(FALSE);

	return false;
}

//定时器事件
bool CTableFrameSink::OnTimerMessage(DWORD dwTimerID, WPARAM wBindParam)
{

	switch(dwTimerID)
	{
	case IDI_DELAY_ENDGAME:
		{
			m_pITableFrame->ConcludeGame(GAME_STATUS_FREE);
			m_pITableFrame->SetGameStatus(GAME_STATUS_FREE);
			m_pITableFrame->KillGameTimer(IDI_DELAY_ENDGAME);

			bool bStart = true;
			//更新房间用户信息			
			for (WORD i=0; i<m_wPlayerCount; i++)
			{
				//获取用户
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);

				if (!pIServerUserItem)
				{
					continue;
				}
				
				UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);

				if(!pIServerUserItem->IsAndroidUser())
					continue;

				if (m_cbDynamicJoin[i] == TRUE)
				{
					continue;
				}

				m_pITableFrame->SendTableData(i,SUB_S_ANDROID_BANKOPERATOR, &bStart, sizeof(bStart));
			}

			return true;
		}
	case IDI_SO_OPERATE:
		{
			//删除时间
			m_pITableFrame->KillGameTimer(IDI_SO_OPERATE);

			//游戏状态
			switch( m_pITableFrame->GetGameStatus() )
			{
			case GS_TK_CALL:			//用户叫庄
				{
					OnUserCallBanker(m_wCurrentUser, 0);
					break;
				}
			case GS_TK_SCORE:			//下注操作
				{
					for(WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_lTableScore[i]>0L || m_cbPlayStatus[i]==FALSE || i==m_wBankerUser)
							continue;

						if ( m_lTurnMaxScore[i] > 0 )
						{
							OnUserAddScore(i,m_lTurnMaxScore[i]/8);
						}
						else
						{
							OnUserAddScore(i,1);
						}
					}

					break;
				}
			case GS_TK_PLAYING:			//用户开牌
				{
					for(WORD i=0;i<m_wPlayerCount;i++)
					{
						if(m_cbOxCard[i]<2 || m_cbPlayStatus[i]==FALSE)continue;
						OnUserOpenCard(i, 0);
					}

					break;
				}
			default:
				{
					break;
				}
			}

			if(m_pITableFrame->GetGameStatus()!=GS_TK_FREE)
				m_pITableFrame->SetGameTimer(IDI_SO_OPERATE,TIME_SO_OPERATE,1,0);
			return true;
		}
	}
	return false;
}

//游戏消息处理 
bool CTableFrameSink::OnGameMessage(WORD wSubCmdID, void * pDataBuffer, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	bool bResult=false;
	switch (wSubCmdID)
	{
	case SUB_C_CALL_BANKER:			//用户叫庄
		{
			//效验数据
			ASSERT(wDataSize==sizeof(CMD_C_CallBanker));
			if (wDataSize!=sizeof(CMD_C_CallBanker)) return false;

			//变量定义
			CMD_C_CallBanker * pCallBanker=(CMD_C_CallBanker *)pDataBuffer;

			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus!=US_PLAYING) return true;

			//状态判断
			ASSERT(IsUserPlaying(pUserData->wChairID));
			if (!IsUserPlaying(pUserData->wChairID)) return false;

			//消息处理
			bResult=OnUserCallBanker(pUserData->wChairID,pCallBanker->bBanker);
			break;
		}
	case SUB_C_ADD_SCORE:			//用户加注
		{
			//效验数据
			ASSERT(wDataSize==sizeof(CMD_C_AddScore));
			if (wDataSize!=sizeof(CMD_C_AddScore)) 
			{
				return false;
			}

			//变量定义
			CMD_C_AddScore * pAddScore=(CMD_C_AddScore *)pDataBuffer;

			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus!=US_PLAYING) return true;

			//状态判断
			ASSERT(IsUserPlaying(pUserData->wChairID));
			if (!IsUserPlaying(pUserData->wChairID)) 
			{
				return false;
			}

			//消息处理
			bResult=OnUserAddScore(pUserData->wChairID,pAddScore->lScore);
			break;
		}
	case SUB_C_OPEN_CARD:			//用户摊牌
		{
			//效验数据
			ASSERT(wDataSize==sizeof(CMD_C_OxCard));
			if (wDataSize!=sizeof(CMD_C_OxCard)) 
			{
				return false;
			}

			//变量定义
			CMD_C_OxCard * pOxCard=(CMD_C_OxCard *)pDataBuffer;

			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus!=US_PLAYING) return true;

			//状态判断
			ASSERT(m_cbPlayStatus[pUserData->wChairID]!=FALSE);
			if(m_cbPlayStatus[pUserData->wChairID]==FALSE)
			{
				return false;
			}

			//消息处理
			bResult=OnUserOpenCard(pUserData->wChairID,pOxCard->bOX);
			break;
		}
	case SUB_C_STORAGE:
		{
			ASSERT(wDataSize==sizeof(CMD_C_UpdateStorage));
			if(wDataSize!=sizeof(CMD_C_UpdateStorage)) return false;

			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight())==false)
				return false;

			CMD_C_UpdateStorage *pUpdateStorage=(CMD_C_UpdateStorage *)pDataBuffer;
			g_lRoomStorageCurrent = pUpdateStorage->lRoomStorageCurrent;
			g_lStorageDeductRoom = pUpdateStorage->lRoomStorageDeduct;
			
			//20条操作记录
			if (g_ListOperationRecord.GetSize() == MAX_OPERATION_RECORD)
			{
				g_ListOperationRecord.RemoveHead();
			}

			CString strOperationRecord;
			CTime time = CTime::GetCurrentTime();
			strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d, 控制账户[%s],修改当前库存为 %I64d,衰减值为 %I64d"),
				time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), 
				g_lRoomStorageCurrent, g_lStorageDeductRoom);

			g_ListOperationRecord.AddTail(strOperationRecord);
			
			//写入日志
			strOperationRecord += TEXT("\n");
			WriteInfo(strOperationRecord);
			
			//变量定义
			CMD_S_Operation_Record OperationRecord;
			ZeroMemory(&OperationRecord, sizeof(OperationRecord));
			POSITION posListRecord = g_ListOperationRecord.GetHeadPosition();
			WORD wIndex = 0;//数组下标
			while(posListRecord)
			{
				CString strRecord = g_ListOperationRecord.GetNext(posListRecord);

				CopyMemory(OperationRecord.szRecord[wIndex], strRecord, sizeof(OperationRecord.szRecord[wIndex]));
				wIndex++;
			}
			
			ASSERT(wIndex <= MAX_OPERATION_RECORD);

			//发送数据
			m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));
			m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));

			return true;
		}
	case SUB_C_STORAGEMAXMUL:
		{
			ASSERT(wDataSize==sizeof(CMD_C_ModifyStorage));
			if(wDataSize!=sizeof(CMD_C_ModifyStorage)) return false;

			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight())==false)
				return false;

			CMD_C_ModifyStorage *pModifyStorage = (CMD_C_ModifyStorage *)pDataBuffer;
			g_lStorageMax1Room = pModifyStorage->lMaxRoomStorage[0];
			g_lStorageMax2Room = pModifyStorage->lMaxRoomStorage[1];
			g_lStorageMul1Room = (SCORE)(pModifyStorage->wRoomStorageMul[0]);
			g_lStorageMul2Room = (SCORE)(pModifyStorage->wRoomStorageMul[1]);

			//20条操作记录
			if (g_ListOperationRecord.GetSize() == MAX_OPERATION_RECORD)
			{
				g_ListOperationRecord.RemoveHead();
			}

			CString strOperationRecord;
			CTime time = CTime::GetCurrentTime();
			strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d,控制账户[%s], 修改库存上限值1为 %I64d,赢分概率1为 %I64d,上限值2为 %I64d,赢分概率2为 %I64d"),
				time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), g_lStorageMax1Room, g_lStorageMul1Room, g_lStorageMax2Room, g_lStorageMul2Room);

			g_ListOperationRecord.AddTail(strOperationRecord);

			//写入日志
			strOperationRecord += TEXT("\n");
			WriteInfo(strOperationRecord);
			
			//变量定义
			CMD_S_Operation_Record OperationRecord;
			ZeroMemory(&OperationRecord, sizeof(OperationRecord));
			POSITION posListRecord = g_ListOperationRecord.GetHeadPosition();
			WORD wIndex = 0;//数组下标
			while(posListRecord)
			{
				CString strRecord = g_ListOperationRecord.GetNext(posListRecord);

				CopyMemory(OperationRecord.szRecord[wIndex], strRecord, sizeof(OperationRecord.szRecord[wIndex]));
				wIndex++;
			}
			
			ASSERT(wIndex <= MAX_OPERATION_RECORD);

			//发送数据
			m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));
			m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));

			return true;
		}
	case SUB_C_REQUEST_QUERY_USER:
		{
			ASSERT(wDataSize == sizeof(CMD_C_RequestQuery_User));
			if (wDataSize != sizeof(CMD_C_RequestQuery_User)) 
			{
				return false;
			}

			//权限判断
			if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) == false || pIServerUserItem->IsAndroidUser())
			{
				return false;
			}
			
			CMD_C_RequestQuery_User *pQuery_User = (CMD_C_RequestQuery_User *)pDataBuffer;

			//遍历映射
			POSITION ptHead = g_MapRoomUserInfo.GetStartPosition();
			DWORD dwUserID = 0;
			ROOMUSERINFO userinfo;
			ZeroMemory(&userinfo, sizeof(userinfo));

			CMD_S_RequestQueryResult QueryResult;
			ZeroMemory(&QueryResult, sizeof(QueryResult));

			while(ptHead)
			{
				g_MapRoomUserInfo.GetNextAssoc(ptHead, dwUserID, userinfo);
				if (pQuery_User->dwGameID == userinfo.dwGameID || _tcscmp(pQuery_User->szNickName, userinfo.szNickName) == 0)
				{
					//拷贝用户信息数据
					QueryResult.bFind = true;
					CopyMemory(&(QueryResult.userinfo), &userinfo, sizeof(userinfo));

					ZeroMemory(&g_CurrentQueryUserInfo, sizeof(g_CurrentQueryUserInfo));
					CopyMemory(&(g_CurrentQueryUserInfo), &userinfo, sizeof(userinfo));
				}
			}
			
			//发送数据
			m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_REQUEST_QUERY_RESULT, &QueryResult, sizeof(QueryResult));
			m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_REQUEST_QUERY_RESULT, &QueryResult, sizeof(QueryResult));

			return true;
		}
	case SUB_C_USER_CONTROL:
		{
			ASSERT(wDataSize == sizeof(CMD_C_UserControl));
			if (wDataSize != sizeof(CMD_C_UserControl)) 
			{
				return false;
			}

			//权限判断
			if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) == false || pIServerUserItem->IsAndroidUser() == true)
			{
				return false;
			}

			CMD_C_UserControl *pUserControl = (CMD_C_UserControl *)pDataBuffer;

			//遍历映射
			POSITION ptMapHead = g_MapRoomUserInfo.GetStartPosition();
			DWORD dwUserID = 0;
			ROOMUSERINFO userinfo;
			ZeroMemory(&userinfo, sizeof(userinfo));
			
			//20条操作记录
			if (g_ListOperationRecord.GetSize() == MAX_OPERATION_RECORD)
			{
				g_ListOperationRecord.RemoveHead();
			}

			//变量定义
			CMD_S_UserControl serverUserControl;
			ZeroMemory(&serverUserControl, sizeof(serverUserControl));

			TCHAR szNickName[LEN_NICKNAME];
			ZeroMemory(szNickName, sizeof(szNickName));

			//激活控制
			if (pUserControl->userControlInfo.bCancelControl == false)
			{
				ASSERT(pUserControl->userControlInfo.control_type == CONTINUE_WIN || pUserControl->userControlInfo.control_type == CONTINUE_LOST);
				
				while(ptMapHead)
				{
					g_MapRoomUserInfo.GetNextAssoc(ptMapHead, dwUserID, userinfo);
					
					if (_tcscmp(pUserControl->szNickName, szNickName) == 0 && _tcscmp(userinfo.szNickName, szNickName) == 0)
					{
						continue;
					}

					if (pUserControl->dwGameID == userinfo.dwGameID || _tcscmp(pUserControl->szNickName, userinfo.szNickName) == 0)
					{
						//激活控制标识
						bool bEnableControl = false;
						IsSatisfyControl(userinfo, bEnableControl);
						
						//满足控制
						if (bEnableControl)
						{
							ROOMUSERCONTROL roomusercontrol;
							ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));
							CopyMemory(&(roomusercontrol.roomUserInfo), &userinfo, sizeof(userinfo));
							CopyMemory(&(roomusercontrol.userControl), &(pUserControl->userControlInfo), sizeof(roomusercontrol.userControl));
							
							
							//遍历链表，除重
							TravelControlList(roomusercontrol);

							//压入链表（不压入同GAMEID和NICKNAME)
							g_ListRoomUserControl.AddHead(roomusercontrol);
							
							//复制数据
							serverUserControl.dwGameID = userinfo.dwGameID;
							CopyMemory(serverUserControl.szNickName, userinfo.szNickName, sizeof(userinfo.szNickName));
							serverUserControl.controlResult = CONTROL_SUCCEED;
							serverUserControl.controlType = pUserControl->userControlInfo.control_type;
							serverUserControl.cbControlCount = pUserControl->userControlInfo.cbControlCount;
							
							//操作记录
							CString strOperationRecord;
							CString strControlType;
							GetControlTypeString(serverUserControl.controlType, strControlType);
							CTime time = CTime::GetCurrentTime();
							strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d, 控制账户[%s], 控制玩家%s,%s,控制局数%d "),
								time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), serverUserControl.szNickName, strControlType, serverUserControl.cbControlCount);

							g_ListOperationRecord.AddTail(strOperationRecord);

							//写入日志
							strOperationRecord += TEXT("\n");
							WriteInfo(strOperationRecord);
						}
						else	//不满足
						{
							//复制数据
							serverUserControl.dwGameID = userinfo.dwGameID;
							CopyMemory(serverUserControl.szNickName, userinfo.szNickName, sizeof(userinfo.szNickName));
							serverUserControl.controlResult = CONTROL_FAIL;
							serverUserControl.controlType = pUserControl->userControlInfo.control_type;
							serverUserControl.cbControlCount = 0;

							//操作记录
							CString strOperationRecord;
							CString strControlType;
							GetControlTypeString(serverUserControl.controlType, strControlType);
							CTime time = CTime::GetCurrentTime();
							strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d, 控制账户[%s], 控制玩家%s,%s,失败！"),
								time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), serverUserControl.szNickName, strControlType);

							g_ListOperationRecord.AddTail(strOperationRecord);

							//写入日志
							strOperationRecord += TEXT("\n");
							WriteInfo(strOperationRecord);
						}

						//发送数据
						m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));
						m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));
						
						CMD_S_Operation_Record OperationRecord;
						ZeroMemory(&OperationRecord, sizeof(OperationRecord));
						POSITION posListRecord = g_ListOperationRecord.GetHeadPosition();
						WORD wIndex = 0;//数组下标
						while(posListRecord)
						{
							CString strRecord = g_ListOperationRecord.GetNext(posListRecord);

							CopyMemory(OperationRecord.szRecord[wIndex], strRecord, sizeof(OperationRecord.szRecord[wIndex]));
							wIndex++;
						}

						ASSERT(wIndex <= MAX_OPERATION_RECORD);

						//发送数据
						m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));
						m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));

						return true;
					}
				}

				ASSERT(FALSE);
				return false;
			}
			else	//取消控制
			{
				ASSERT(pUserControl->userControlInfo.control_type == CONTINUE_CANCEL);

				POSITION ptListHead = g_ListRoomUserControl.GetHeadPosition();
				POSITION ptTemp;
				ROOMUSERCONTROL roomusercontrol;
				ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));

				//遍历链表
				while(ptListHead)
				{
					ptTemp = ptListHead;
					roomusercontrol = g_ListRoomUserControl.GetNext(ptListHead);
					if (pUserControl->dwGameID == roomusercontrol.roomUserInfo.dwGameID || _tcscmp(pUserControl->szNickName, roomusercontrol.roomUserInfo.szNickName) == 0)
					{
						//复制数据
						serverUserControl.dwGameID = roomusercontrol.roomUserInfo.dwGameID;
						CopyMemory(serverUserControl.szNickName, roomusercontrol.roomUserInfo.szNickName, sizeof(roomusercontrol.roomUserInfo.szNickName));
						serverUserControl.controlResult = CONTROL_CANCEL_SUCCEED;
						serverUserControl.controlType = pUserControl->userControlInfo.control_type;
						serverUserControl.cbControlCount = 0;

						//移除元素
						g_ListRoomUserControl.RemoveAt(ptTemp);

						//发送数据
						m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));
						m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));
						
						//操作记录
						CString strOperationRecord;
						CTime time = CTime::GetCurrentTime();
						strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d, 控制账户[%s], 取消对玩家%s的控制！"),
							time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), serverUserControl.szNickName);

						g_ListOperationRecord.AddTail(strOperationRecord);

						//写入日志
						strOperationRecord += TEXT("\n");
						WriteInfo(strOperationRecord);
						
						CMD_S_Operation_Record OperationRecord;
						ZeroMemory(&OperationRecord, sizeof(OperationRecord));
						POSITION posListRecord = g_ListOperationRecord.GetHeadPosition();
						WORD wIndex = 0;//数组下标
						while(posListRecord)
						{
							CString strRecord = g_ListOperationRecord.GetNext(posListRecord);

							CopyMemory(OperationRecord.szRecord[wIndex], strRecord, sizeof(OperationRecord.szRecord[wIndex]));
							wIndex++;
						}

						ASSERT(wIndex <= MAX_OPERATION_RECORD);

						//发送数据
						m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));
						m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));

						return true;
					}
				}

				//复制数据
				serverUserControl.dwGameID = pUserControl->dwGameID;
				CopyMemory(serverUserControl.szNickName, pUserControl->szNickName, sizeof(serverUserControl.szNickName));
				serverUserControl.controlResult = CONTROL_CANCEL_INVALID;
				serverUserControl.controlType = pUserControl->userControlInfo.control_type;
				serverUserControl.cbControlCount = 0;
		
				//发送数据
				m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));
				m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_USER_CONTROL, &serverUserControl, sizeof(serverUserControl));

				//操作记录
				CString strOperationRecord;
				CTime time = CTime::GetCurrentTime();
				strOperationRecord.Format(TEXT("操作时间: %d/%d/%d-%d:%d:%d, 控制账户[%s], 取消对玩家%s的控制，操作无效！"),
					time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), pIServerUserItem->GetNickName(), serverUserControl.szNickName);

				g_ListOperationRecord.AddTail(strOperationRecord);

				//写入日志
				strOperationRecord += TEXT("\n");
				WriteInfo(strOperationRecord);

				CMD_S_Operation_Record OperationRecord;
				ZeroMemory(&OperationRecord, sizeof(OperationRecord));
				POSITION posListRecord = g_ListOperationRecord.GetHeadPosition();
				WORD wIndex = 0;//数组下标
				while(posListRecord)
				{
					CString strRecord = g_ListOperationRecord.GetNext(posListRecord);

					CopyMemory(OperationRecord.szRecord[wIndex], strRecord, sizeof(OperationRecord.szRecord[wIndex]));
					wIndex++;
				}

				ASSERT(wIndex <= MAX_OPERATION_RECORD);

				//发送数据
				m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));
				m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_OPERATION_RECORD, &OperationRecord, sizeof(OperationRecord));

			}

			return true;
		}
	case SUB_C_REQUEST_UDPATE_ROOMINFO:
		{
			//权限判断
			if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) == false || pIServerUserItem->IsAndroidUser() == true)
			{
				return false;
			}

			CMD_S_RequestUpdateRoomInfo_Result RoomInfo_Result;
			ZeroMemory(&RoomInfo_Result, sizeof(RoomInfo_Result));

			RoomInfo_Result.lRoomStorageCurrent = g_lRoomStorageCurrent;

			
			DWORD dwKeyGameID = g_CurrentQueryUserInfo.dwGameID;
			TCHAR szKeyNickName[LEN_NICKNAME];	
			ZeroMemory(szKeyNickName, sizeof(szKeyNickName));
			CopyMemory(szKeyNickName, g_CurrentQueryUserInfo.szNickName, sizeof(szKeyNickName));

			//遍历映射
			POSITION ptHead = g_MapRoomUserInfo.GetStartPosition();
			DWORD dwUserID = 0;
			ROOMUSERINFO userinfo;
			ZeroMemory(&userinfo, sizeof(userinfo));

			while(ptHead)
			{
				g_MapRoomUserInfo.GetNextAssoc(ptHead, dwUserID, userinfo);
				if (dwKeyGameID == userinfo.dwGameID && _tcscmp(szKeyNickName, userinfo.szNickName) == 0)
				{
					//拷贝用户信息数据
					CopyMemory(&(RoomInfo_Result.currentqueryuserinfo), &userinfo, sizeof(userinfo));

					ZeroMemory(&g_CurrentQueryUserInfo, sizeof(g_CurrentQueryUserInfo));
					CopyMemory(&(g_CurrentQueryUserInfo), &userinfo, sizeof(userinfo));
				}
			}

		
			//
			//变量定义
			POSITION ptListHead = g_ListRoomUserControl.GetHeadPosition();
			POSITION ptTemp;
			ROOMUSERCONTROL roomusercontrol;
			ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));

			//遍历链表
			while(ptListHead)
			{
				ptTemp = ptListHead;
				roomusercontrol = g_ListRoomUserControl.GetNext(ptListHead);

				//寻找玩家
				if ((dwKeyGameID == roomusercontrol.roomUserInfo.dwGameID) &&
					_tcscmp(szKeyNickName, roomusercontrol.roomUserInfo.szNickName) == 0)
				{
					RoomInfo_Result.bExistControl = true;
					CopyMemory(&(RoomInfo_Result.currentusercontrol), &(roomusercontrol.userControl), sizeof(roomusercontrol.userControl));
					break;				
				}
			}

			//发送数据
			m_pITableFrame->SendTableData(pIServerUserItem->GetChairID(), SUB_S_REQUEST_UDPATE_ROOMINFO_RESULT, &RoomInfo_Result, sizeof(RoomInfo_Result));
			m_pITableFrame->SendLookonData(pIServerUserItem->GetChairID(), SUB_S_REQUEST_UDPATE_ROOMINFO_RESULT, &RoomInfo_Result, sizeof(RoomInfo_Result));
			
			return true;
		}
	case SUB_C_CLEAR_CURRENT_QUERYUSER:
		{
			//权限判断
			if (CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) == false || pIServerUserItem->IsAndroidUser() == true)
			{
				return false;
			}
			
			ZeroMemory(&g_CurrentQueryUserInfo, sizeof(g_CurrentQueryUserInfo));

			return true;
		}
	}

	//操作定时器
	if(bResult && !IsRoomCardType())
	{
		m_pITableFrame->SetGameTimer(IDI_SO_OPERATE,TIME_SO_OPERATE,1,0);
		return true;
	}

	return true;
}

//框架消息处理
bool CTableFrameSink::OnFrameMessage(WORD wSubCmdID, void * pDataBuffer, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	return false;
}


//叫庄事件
bool CTableFrameSink::OnUserCallBanker(WORD wChairID, BYTE bBanker)
{
	//状态效验
	BYTE cbGameStatus = m_pITableFrame->GetGameStatus();
	ASSERT(cbGameStatus==GS_TK_CALL);
	if (cbGameStatus!=GS_TK_CALL) return true;
	ASSERT(m_wCurrentUser==wChairID);
	if (m_wCurrentUser!=wChairID) return true;

	//设置变量
	m_cbCallStatus[wChairID]=TRUE;

	//叫庄人数
	WORD wCallUserCount=0;
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		if(m_cbPlayStatus[i]==TRUE && m_cbCallStatus[i]==TRUE) wCallUserCount++;
		else if(m_cbPlayStatus[i]!=TRUE) wCallUserCount++;
	}

	if(bBanker==FALSE && wCallUserCount==m_wPlayerCount)
	{
		m_wBankerUser=m_wFisrtCallUser;
	}

	

	//下注开始
	if(bBanker==TRUE || wCallUserCount==m_wPlayerCount)
	{
		//始叫用户
		if(bBanker==TRUE)
		{
			m_wBankerUser=wChairID;
		}
		m_wCurrentUser=INVALID_CHAIR;
		
		//过滤最后一个叫庄用户强退情况
		while(m_cbPlayStatus[m_wBankerUser]==FALSE)
		{
			m_wBankerUser=(m_wBankerUser+1)%GAME_PLAYER;
		}
		m_bBuckleServiceCharge[m_wBankerUser]=true;
		//设置状态
		m_pITableFrame->SetGameStatus(GS_TK_SCORE);

		//更新房间用户信息
		for (WORD i=0; i<m_wPlayerCount; i++)
		{
			//获取用户
			IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
			if (pIServerUserItem != NULL)
			{
				UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
			}
		}

		//庄家积分
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(m_wBankerUser);
		LONGLONG lBankerScore=pIServerUserItem->GetUserScore();

		//最大下注
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]!=TRUE || i==m_wBankerUser)continue;

			//下注变量 客户要求
			m_lTurnMaxScore[i]=GetUserMaxTurnScore(i);
		}

		//设置变量
		CMD_S_GameStart GameStart;
		GameStart.wBankerUser=m_wBankerUser;
		GameStart.lTurnMaxScore=0;
		GameStart.banker_config = NORMAL_BANKER;
		GameStart.bRoomCardScore = IsRoomCardScoreType();
		CopyMemory(GameStart.cbPlayerStatus, m_cbPlayStatus, sizeof(m_cbPlayStatus));

		//积分类型
		if (IsRoomCardScoreType())
		{
			//设置文件名
			TCHAR szPath[MAX_PATH] = TEXT("");
			TCHAR szFileName[MAX_PATH] = TEXT("");
			GetCurrentDirectory(sizeof(szPath),szPath);
			_sntprintf(szFileName,sizeof(szFileName),TEXT("%s\\OxNewRoomCard.ini"),szPath);

			//读取配置
			LONGLONG lRoomCardJetton[MAX_JETTON_AREA];
			ZeroMemory(lRoomCardJetton, sizeof(lRoomCardJetton));
			lRoomCardJetton[0] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonOne"), 20, szFileName);
			lRoomCardJetton[1] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonTwo"), 30, szFileName);
			lRoomCardJetton[2] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonThree"), 50, szFileName);
			lRoomCardJetton[3] = GetPrivateProfileInt(m_pGameServiceOption->szServerName, TEXT("lJettonFour"), 80, szFileName);

			CopyMemory(GameStart.lRoomCardJetton, lRoomCardJetton, sizeof(lRoomCardJetton));
		}

		//发送数据
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
			GameStart.lTurnMaxScore=m_lTurnMaxScore[i];
			m_pITableFrame->SendTableData(i,SUB_S_GAME_START,&GameStart,sizeof(GameStart));
		}
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_START,&GameStart,sizeof(GameStart));
	}
	else		 //用户叫庄
	{
		//查找下个玩家
		do{
			m_wCurrentUser=(m_wCurrentUser+1)%m_wPlayerCount;
		}while(m_cbPlayStatus[m_wCurrentUser]==FALSE);

		//设置变量
		CMD_S_CallBanker CallBanker;
		CallBanker.wCallBanker=m_wCurrentUser;
		CallBanker.bFirstTimes=false;

		//发送数据
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
			m_pITableFrame->SendTableData(i,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
		}
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_CALL_BANKER,&CallBanker,sizeof(CallBanker));
	}
	return true;
}

//加注事件
bool CTableFrameSink::OnUserAddScore(WORD wChairID, LONGLONG lScore)
{
	//状态效验
	BYTE cbGameStatus = m_pITableFrame->GetGameStatus();
	ASSERT(cbGameStatus==GS_TK_SCORE);
	if (cbGameStatus!=GS_TK_SCORE) return true;

	//金币效验
	if(m_cbPlayStatus[wChairID]==TRUE)
	{
		ASSERT(lScore>0 && lScore<=m_lTurnMaxScore[wChairID]);
		if (lScore<=0 || lScore>m_lTurnMaxScore[wChairID])
		{
			return false;
		}
	}
	else //没下注玩家强退
	{
		ASSERT(lScore==0);
		if (lScore!=0) 
		{
			return false;
		}
	}

	if(!UserCanAddScore(wChairID,lScore)) 
	{
		return false;
	}

	if(lScore>0L)
	{
		//下注金币
		m_lTableScore[wChairID]=lScore;
		m_bBuckleServiceCharge[wChairID]=true;
		//构造数据
		CMD_S_AddScore AddScore;
		AddScore.wAddScoreUser=wChairID;
		AddScore.lAddScoreCount=m_lTableScore[wChairID];

		//发送数据
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
			m_pITableFrame->SendTableData(i,SUB_S_ADD_SCORE,&AddScore,sizeof(AddScore));
		}
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_ADD_SCORE,&AddScore,sizeof(AddScore));
	}

	//下注人数
	BYTE bUserCount=0;
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		if(m_lTableScore[i]>0L && m_cbPlayStatus[i]==TRUE)bUserCount++;
		else if(m_cbPlayStatus[i]==FALSE || i==m_wBankerUser)bUserCount++;
	}

	//闲家全到
	if(bUserCount==m_wPlayerCount)
	{
		//设置状态
		m_pITableFrame->SetGameStatus(GS_TK_PLAYING);

		//更新房间用户信息
		for (WORD i=0; i<m_wPlayerCount; i++)
		{
			//获取用户
			IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
			if (pIServerUserItem != NULL)
			{
				UpdateRoomUserInfo(pIServerUserItem, USER_SITDOWN);
			}
		}

		//构造数据
		CMD_S_SendCard SendCard;
		ZeroMemory(SendCard.cbCardData,sizeof(SendCard.cbCardData));

		//分析扑克
		AnalyseCard();
		
		//变量定义
		ROOMUSERCONTROL roomusercontrol;
		ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));
		POSITION posKeyList;

		//控制
		if( m_pServerControl != NULL && AnalyseRoomUserControl(roomusercontrol, posKeyList))
		{
			//校验数据
			ASSERT(roomusercontrol.roomUserInfo.wChairID != INVALID_CHAIR && roomusercontrol.userControl.cbControlCount != 0 
				&& roomusercontrol.userControl.control_type != CONTINUE_CANCEL);
	
			if(m_pServerControl->ControlResult(m_cbHandCardData, roomusercontrol))
			{
				//获取元素
				ROOMUSERCONTROL &tmproomusercontrol = g_ListRoomUserControl.GetAt(posKeyList);
				
				//校验数据
				ASSERT(roomusercontrol.userControl.cbControlCount == tmproomusercontrol.userControl.cbControlCount);

				//控制局数
				tmproomusercontrol.userControl.cbControlCount--;

				CMD_S_UserControlComplete UserControlComplete;
				ZeroMemory(&UserControlComplete, sizeof(UserControlComplete));
				UserControlComplete.dwGameID = roomusercontrol.roomUserInfo.dwGameID;
				CopyMemory(UserControlComplete.szNickName, roomusercontrol.roomUserInfo.szNickName, sizeof(UserControlComplete.szNickName));
				UserControlComplete.controlType = roomusercontrol.userControl.control_type;
				UserControlComplete.cbRemainControlCount = tmproomusercontrol.userControl.cbControlCount;

				for (WORD i=0; i<m_wPlayerCount; i++)
				{
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
					if (!pIServerUserItem)
					{
						continue;
					}
					if (pIServerUserItem->IsAndroidUser() == true || CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()) == false)
					{
						continue;
					}

					//发送数据
					m_pITableFrame->SendTableData(i, SUB_S_USER_CONTROL_COMPLETE, &UserControlComplete, sizeof(UserControlComplete));
					m_pITableFrame->SendLookonData(i, SUB_S_USER_CONTROL_COMPLETE, &UserControlComplete, sizeof(UserControlComplete));

				}
			}			
		}

		//发送扑克
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]==FALSE)continue;

			//派发扑克
			CopyMemory(SendCard.cbCardData[i],m_cbHandCardData[i],MAX_COUNT);
		}

		//发送数据
		for (WORD i=0;i< m_wPlayerCount;i++)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;

			m_pITableFrame->SendTableData(i,SUB_S_SEND_CARD,&SendCard,sizeof(SendCard));
		}
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_SEND_CARD,&SendCard,sizeof(SendCard));
	}

	return true;
}

//摊牌事件
bool CTableFrameSink::OnUserOpenCard(WORD wChairID, BYTE bOx)
{
	//状态效验
	BYTE cbGameStatus = m_pITableFrame->GetGameStatus();
	ASSERT (cbGameStatus==GS_TK_PLAYING);
	if (cbGameStatus!=GS_TK_PLAYING) return true;
	if (m_cbOxCard[wChairID]!=0xff) return true;

	//效验数据
	ASSERT(bOx<=TRUE);
	if(bOx>TRUE)
	{
		return false;
	}

	//效验数据
	if(bOx)
	{
		ASSERT(m_GameLogic.GetCardType(m_cbHandCardData[wChairID],MAX_COUNT)>0);
		if(!(m_GameLogic.GetCardType(m_cbHandCardData[wChairID],MAX_COUNT)>0))
		{
			return false;
		}
	}
	else if(m_cbPlayStatus[wChairID]==TRUE)
	{
		if(m_GameLogic.GetCardType(m_cbHandCardData[wChairID],MAX_COUNT)>=OX_THREE_SAME)bOx=TRUE;
	}

	//牛牛数据
	m_cbOxCard[wChairID] = bOx;

	//摊牌人数
	BYTE bUserCount=0;
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		if(m_cbOxCard[i]<2 && m_cbPlayStatus[i]==TRUE)bUserCount++;
		else if(m_cbPlayStatus[i]==FALSE)bUserCount++;
	}

	 //构造变量
	CMD_S_Open_Card OpenCard;
	ZeroMemory(&OpenCard,sizeof(OpenCard));

	//设置变量
	OpenCard.bOpen=bOx;
	OpenCard.wPlayerID=wChairID;

	//发送数据
	for (WORD i=0;i< m_wPlayerCount;i++)
	{
		if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
		m_pITableFrame->SendTableData(i,SUB_S_OPEN_CARD,&OpenCard,sizeof(OpenCard));
	}
	m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_OPEN_CARD,&OpenCard,sizeof(OpenCard));	

	//结束游戏
	if(bUserCount == m_wPlayerCount)
	{
		return OnEventGameConclude(INVALID_CHAIR,NULL,GER_NORMAL);
	}

	return true;
}

//扑克分析
void CTableFrameSink::AnalyseCard()
{
	//机器人数
	bool bIsAiBanker = false;
	WORD wAiCount = 0;
	WORD wPlayerCount = 0;
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if (pIServerUserItem!=NULL)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
			if(pIServerUserItem->IsAndroidUser()) 
			{
				wAiCount++ ;
				if(!bIsAiBanker && i==m_wBankerUser)bIsAiBanker = true;
			}
			wPlayerCount++; 
		}
	}

	//全部机器
	if(wPlayerCount == wAiCount || wAiCount==0)return;

	//扑克变量
	BYTE cbUserCardData[GAME_PLAYER][MAX_COUNT];
	CopyMemory(cbUserCardData,m_cbHandCardData,sizeof(m_cbHandCardData));

	//牛牛数据
	BOOL bUserOxData[GAME_PLAYER];
	ZeroMemory(bUserOxData,sizeof(bUserOxData));
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		if(m_cbPlayStatus[i]==FALSE)continue;
		bUserOxData[i] = (m_GameLogic.GetCardType(cbUserCardData[i],MAX_COUNT)>0)?TRUE:FALSE;
	}

	//排列扑克
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		m_GameLogic.SortCardList(cbUserCardData[i],MAX_COUNT);
	}

	//变量定义
	LONGLONG lAndroidScore=0;

	//倍数变量
	BYTE cbCardTimes[GAME_PLAYER];
	ZeroMemory(cbCardTimes,sizeof(cbCardTimes));

	//查找倍数
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		if (m_cbPlayStatus[i]==TRUE)
		{
			cbCardTimes[i]=m_GameLogic.GetTimes(cbUserCardData[i],MAX_COUNT);
		}
	}

	//机器庄家
	if(bIsAiBanker)
	{
		//对比扑克
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			//用户过滤
			if ((i==m_wBankerUser)||(m_cbPlayStatus[i]==FALSE)) continue;

			//获取用户
			IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);

			//机器过滤
			if ((pIServerUserItem!=NULL)&&(pIServerUserItem->IsAndroidUser())) continue;

			//对比扑克
			if (m_GameLogic.CompareCard(cbUserCardData[i],cbUserCardData[m_wBankerUser],MAX_COUNT,bUserOxData[i],bUserOxData[m_wBankerUser])==true)
			{
				lAndroidScore-=cbCardTimes[i]*m_lTableScore[i];
			}
			else
			{
				lAndroidScore+=cbCardTimes[m_wBankerUser]*m_lTableScore[i];
			}
		}
	}
	else//用户庄家
	{
		//对比扑克
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			//获取用户
			IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);

			//用户过滤
			if ((i==m_wBankerUser)||(pIServerUserItem==NULL)||!(pIServerUserItem->IsAndroidUser())) continue;

			//对比扑克
			if (m_GameLogic.CompareCard(cbUserCardData[i],cbUserCardData[m_wBankerUser],MAX_COUNT,bUserOxData[i],bUserOxData[m_wBankerUser])==true)
			{
				lAndroidScore+=cbCardTimes[i]*m_lTableScore[i];
			}
			else
			{
				lAndroidScore-=cbCardTimes[m_wBankerUser]*m_lTableScore[i];
			}
		}
	}

	//变量定义
	WORD wMaxUser=INVALID_CHAIR;
	WORD wMinAndroid=INVALID_CHAIR;
	WORD wMaxAndroid=INVALID_CHAIR;

	//查找特殊玩家
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if(pIServerUserItem==NULL) continue;

		//真人玩家
		if (pIServerUserItem->IsAndroidUser()==false)
		{
			//初始设置
			if(wMaxUser==INVALID_CHAIR) wMaxUser=i;

			//获取较大者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMaxUser],MAX_COUNT,bUserOxData[i],bUserOxData[wMaxUser])==true)
			{
				wMaxUser=i;
			}
		}

		//机器玩家
		if (pIServerUserItem->IsAndroidUser()==true)
		{
			//初始设置
			if(wMinAndroid==INVALID_CHAIR) wMinAndroid=i;
			if(wMaxAndroid==INVALID_CHAIR) wMaxAndroid=i;

			//获取较小者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMinAndroid],MAX_COUNT,bUserOxData[i],bUserOxData[wMinAndroid])==false)
			{
				wMinAndroid=i;
			}

			//获取较大者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMaxAndroid],MAX_COUNT,bUserOxData[i],bUserOxData[wMaxAndroid])==true)
			{
				wMaxAndroid=i;
			}
		}
	}	

	
	LONGLONG lGameEndStorage = g_lRoomStorageCurrent+lAndroidScore;
	//库存判断
	if(g_lRoomStorageCurrent+lAndroidScore<0 || (lGameEndStorage < (g_lRoomStorageCurrent * (double)(1 - (double)5 / (double)100))))
	{
		//变量定义
		WORD wWinUser=wMaxUser;

		//机器坐庄
		if(bIsAiBanker)
		{
			//查找数据
			for (WORD i=0;i<m_wPlayerCount;i++)
			{

				//用户过滤
				if (m_cbPlayStatus[i]==FALSE) continue;

				//获取较大者
				if (m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wWinUser],MAX_COUNT,bUserOxData[i],bUserOxData[wWinUser])==true)
				{
					wWinUser=i;
				}
			}
		}
		else
		{
			//获取较小者
			if (m_GameLogic.CompareCard(cbUserCardData[wMaxAndroid],m_cbHandCardData[wWinUser],MAX_COUNT,bUserOxData[wMaxAndroid],bUserOxData[wWinUser])==false)
			{
				wWinUser=wMaxAndroid;				
			}
		}

		if(bIsAiBanker)
		{
			//交换数据
			BYTE cbTempData[MAX_COUNT];
			CopyMemory(cbTempData,m_cbHandCardData[m_wBankerUser],MAX_COUNT);
			CopyMemory(m_cbHandCardData[m_wBankerUser],m_cbHandCardData[wWinUser],MAX_COUNT);
			CopyMemory(m_cbHandCardData[wWinUser],cbTempData,MAX_COUNT);
		}
		else
		{
			BYTE bUser = m_wBankerUser;

			//每一个玩家代替庄家，直到不会出现负库存为止
			do 
			{
				bUser = (bUser+1)%GAME_PLAYER;

				if(m_cbPlayStatus[bUser]==TRUE)
				{
					//交换数据
					BYTE cbTempData[MAX_COUNT];
					CopyMemory(cbTempData,m_cbHandCardData[m_wBankerUser],MAX_COUNT);
					CopyMemory(m_cbHandCardData[m_wBankerUser],m_cbHandCardData[bUser],MAX_COUNT);
					CopyMemory(m_cbHandCardData[bUser],cbTempData,MAX_COUNT);
				}

			} while (!JudgeStock() && (bUser != m_wBankerUser));

		}
	}
	else if(g_lRoomStorageCurrent>0 /*&& lAndroidScore>0*/ && g_lRoomStorageCurrent > g_lStorageMax2Room && g_lRoomStorageCurrent - lAndroidScore > 0 && rand()%100 < g_lStorageMul2Room)
	{
		if (m_GameLogic.CompareCard(cbUserCardData[wMaxAndroid],m_cbHandCardData[wMaxUser],MAX_COUNT,bUserOxData[wMaxAndroid],bUserOxData[wMaxUser])==true)
		{
			//交换数据
			BYTE cbTempData[MAX_COUNT];
			CopyMemory(cbTempData,m_cbHandCardData[wMaxUser],MAX_COUNT);
			CopyMemory(m_cbHandCardData[wMaxUser],m_cbHandCardData[wMaxAndroid],MAX_COUNT);
			CopyMemory(m_cbHandCardData[wMaxAndroid],cbTempData,MAX_COUNT);

			//库存不够换回来
			if (JudgeStock() == false)
			{
				CopyMemory(cbTempData,m_cbHandCardData[wMaxUser],MAX_COUNT);
				CopyMemory(m_cbHandCardData[wMaxUser],m_cbHandCardData[wMaxAndroid],MAX_COUNT);
				CopyMemory(m_cbHandCardData[wMaxAndroid],cbTempData,MAX_COUNT);
			}
		}			
	}
	else if(g_lRoomStorageCurrent>0 /*&& lAndroidScore>0*/ && g_lRoomStorageCurrent > g_lStorageMax1Room && g_lRoomStorageCurrent - lAndroidScore > 0 && rand()%100 < g_lStorageMul1Room)
	{
		if (m_GameLogic.CompareCard(cbUserCardData[wMaxAndroid],m_cbHandCardData[wMaxUser],MAX_COUNT,bUserOxData[wMaxAndroid],bUserOxData[wMaxUser])==true)
		{
			//交换数据
			BYTE cbTempData[MAX_COUNT];
			CopyMemory(cbTempData,m_cbHandCardData[wMaxUser],MAX_COUNT);
			CopyMemory(m_cbHandCardData[wMaxUser],m_cbHandCardData[wMaxAndroid],MAX_COUNT);
			CopyMemory(m_cbHandCardData[wMaxAndroid],cbTempData,MAX_COUNT);

			//库存不够换回来
			if (JudgeStock() == false)
			{
				CopyMemory(cbTempData,m_cbHandCardData[wMaxUser],MAX_COUNT);
				CopyMemory(m_cbHandCardData[wMaxUser],m_cbHandCardData[wMaxAndroid],MAX_COUNT);
				CopyMemory(m_cbHandCardData[wMaxAndroid],cbTempData,MAX_COUNT);
			}
		}			
	}

	return;
}



//判断库存
bool CTableFrameSink::JudgeStock()
{

	//机器人数
	bool bIsAiBanker = false;
	WORD wAiCount = 0;
	WORD wPlayerCount = 0;
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if (pIServerUserItem!=NULL)
		{
			if(m_cbPlayStatus[i]==FALSE&&m_cbDynamicJoin[i]==FALSE)continue;
			if(pIServerUserItem->IsAndroidUser()) 
			{
				wAiCount++ ;
				if(!bIsAiBanker && i==m_wBankerUser)bIsAiBanker = true;
			}
			wPlayerCount++; 
		}
	}

	//扑克变量
	BYTE cbUserCardData[GAME_PLAYER][MAX_COUNT];
	CopyMemory(cbUserCardData,m_cbHandCardData,sizeof(m_cbHandCardData));

	//牛牛数据
	BOOL bUserOxData[GAME_PLAYER];
	ZeroMemory(bUserOxData,sizeof(bUserOxData));
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		if(m_cbPlayStatus[i]==FALSE)continue;
		bUserOxData[i] = (m_GameLogic.GetCardType(cbUserCardData[i],MAX_COUNT)>0)?TRUE:FALSE;
	}

	//排列扑克
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		m_GameLogic.SortCardList(cbUserCardData[i],MAX_COUNT);
	}

	//变量定义
	LONGLONG lAndroidScore=0;

	//倍数变量
	BYTE cbCardTimes[GAME_PLAYER];
	ZeroMemory(cbCardTimes,sizeof(cbCardTimes));

	//查找倍数
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		if (m_cbPlayStatus[i]==TRUE)
		{
			cbCardTimes[i]=m_GameLogic.GetTimes(cbUserCardData[i],MAX_COUNT);
		}
	}

	//机器庄家
	if(bIsAiBanker)
	{
		//对比扑克
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			//用户过滤
			if ((i==m_wBankerUser)||(m_cbPlayStatus[i]==FALSE)) continue;

			//获取用户
			IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);

			//机器过滤
			if ((pIServerUserItem!=NULL)&&(pIServerUserItem->IsAndroidUser())) continue;

			//对比扑克
			if (m_GameLogic.CompareCard(cbUserCardData[i],cbUserCardData[m_wBankerUser],MAX_COUNT,bUserOxData[i],bUserOxData[m_wBankerUser])==true)
			{
				lAndroidScore-=cbCardTimes[i]*m_lTableScore[i];
			}
			else
			{
				lAndroidScore+=cbCardTimes[m_wBankerUser]*m_lTableScore[i];
			}
		}
	}
	else//用户庄家
	{
		//对比扑克
		for (WORD i=0;i<m_wPlayerCount;i++)
		{
			//获取用户
			IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);

			//用户过滤
			if ((i==m_wBankerUser)||(pIServerUserItem==NULL)||!(pIServerUserItem->IsAndroidUser())) continue;

			//对比扑克
			if (m_GameLogic.CompareCard(cbUserCardData[i],cbUserCardData[m_wBankerUser],MAX_COUNT,bUserOxData[i],bUserOxData[m_wBankerUser])==true)
			{
				lAndroidScore+=cbCardTimes[i]*m_lTableScore[i];
			}
			else
			{
				lAndroidScore-=cbCardTimes[m_wBankerUser]*m_lTableScore[i];
			}
		}
	}

	//变量定义
	WORD wMaxUser=INVALID_CHAIR;
	WORD wMinAndroid=INVALID_CHAIR;
	WORD wMaxAndroid=INVALID_CHAIR;

	//查找特殊玩家
	for(WORD i=0;i<m_wPlayerCount;i++)
	{
		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if(pIServerUserItem==NULL) continue;

		//真人玩家
		if (pIServerUserItem->IsAndroidUser()==false)
		{
			//初始设置
			if(wMaxUser==INVALID_CHAIR) wMaxUser=i;

			//获取较大者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMaxUser],MAX_COUNT,bUserOxData[i],bUserOxData[wMaxUser])==true)
			{
				wMaxUser=i;
			}
		}

		//机器玩家
		if (pIServerUserItem->IsAndroidUser()==true)
		{
			//初始设置
			if(wMinAndroid==INVALID_CHAIR) wMinAndroid=i;
			if(wMaxAndroid==INVALID_CHAIR) wMaxAndroid=i;

			//获取较小者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMinAndroid],MAX_COUNT,bUserOxData[i],bUserOxData[wMinAndroid])==false)
			{
				wMinAndroid=i;
			}

			//获取较大者
			if(m_GameLogic.CompareCard(cbUserCardData[i],m_cbHandCardData[wMaxAndroid],MAX_COUNT,bUserOxData[i],bUserOxData[wMaxAndroid])==true)
			{
				wMaxAndroid=i;
			}
		}
	}	

	return g_lRoomStorageCurrent+lAndroidScore>0;

}

//查询是否扣服务费
bool CTableFrameSink::QueryBuckleServiceCharge(WORD wChairID)
{
	for (BYTE i=0;i<m_wPlayerCount;i++)
	{
		IServerUserItem *pUserItem=m_pITableFrame->GetTableUserItem(i);
		if(pUserItem==NULL) continue;
		
		if (m_bBuckleServiceCharge[i]&&i==wChairID)
		{
			return true;
		}
		
	}
	return false;
}


bool CTableFrameSink::TryWriteTableScore(tagScoreInfo ScoreInfoArray[])
{
	//修改积分
	tagScoreInfo ScoreInfo[GAME_PLAYER];
	ZeroMemory(&ScoreInfo,sizeof(ScoreInfo));
	memcpy(&ScoreInfo,ScoreInfoArray,sizeof(ScoreInfo));
	//记录异常
	LONGLONG beforeScore[GAME_PLAYER];
	ZeroMemory(beforeScore,sizeof(beforeScore));
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		IServerUserItem *pItem=m_pITableFrame->GetTableUserItem(i);
		if(pItem!=NULL)
		{
			beforeScore[i]=pItem->GetUserScore();
			m_pITableFrame->WriteUserScore(i, ScoreInfo[i]);
		}
	}
	/*m_pITableFrame->WriteTableScore(ScoreInfo,CountArray(ScoreInfo));*/
	LONGLONG afterScore[GAME_PLAYER];
	ZeroMemory(afterScore,sizeof(afterScore));
	for (WORD i=0;i<m_wPlayerCount;i++)
	{
		IServerUserItem *pItem=m_pITableFrame->GetTableUserItem(i);
		if(pItem!=NULL)
		{
			afterScore[i]=pItem->GetUserScore();

			if(afterScore[i]<0)
			{
				//异常写入日志

				CString strInfo;
				strInfo.Format(TEXT("[%s] 出现负分"),pItem->GetNickName());
				NcaTextOut(strInfo, m_pGameServiceOption->szServerName);

				strInfo.Format(TEXT("写分前分数：%I64d"),beforeScore[i]);
				NcaTextOut(strInfo, m_pGameServiceOption->szServerName);

				strInfo.Format(TEXT("写分信息：写入积分 %I64d，税收 %I64d"),ScoreInfoArray[i].lScore,ScoreInfoArray[i].lRevenue);
				NcaTextOut(strInfo, m_pGameServiceOption->szServerName);

				strInfo.Format(TEXT("写分后分数：%I64d"),afterScore[i]);
				NcaTextOut(strInfo, m_pGameServiceOption->szServerName);

			}

		}
	}
	return true;
}

//最大下分
SCORE CTableFrameSink::GetUserMaxTurnScore(WORD wChairID)
{

	SCORE lMaxTurnScore=0L;
	if(wChairID==m_wBankerUser)  return 0;
	//庄家积分
	IServerUserItem *pIBankerItem=m_pITableFrame->GetTableUserItem(m_wBankerUser);
	LONGLONG lBankerScore=0L;
	if(pIBankerItem!=NULL)
		lBankerScore=pIBankerItem->GetUserScore();

	//玩家人数
	WORD wUserCount=0;
	for (WORD i=0;i<m_wPlayerCount;i++)
		if(m_cbPlayStatus[i]==TRUE )wUserCount++;

	//获取用户
	IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);

	if(pIServerUserItem!=NULL)
	{
		//获取积分
		LONGLONG lScore=pIServerUserItem->GetUserScore();

		lMaxTurnScore=__min(lBankerScore/(wUserCount-1)/MAX_TIMES,lScore/MAX_TIMES);
	}

	return lMaxTurnScore;
	

}

//是否可加
bool CTableFrameSink::UserCanAddScore(WORD wChairID, LONGLONG lAddScore)
{

	//获取用户
	IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);

	if(pIServerUserItem!=NULL)
	{
		//获取积分
		LONGLONG lScore=pIServerUserItem->GetUserScore();

		if(lAddScore>lScore/MAX_TIMES)
		{
			return false;
		}
	}
	return true;

}

//查询限额
SCORE CTableFrameSink::QueryConsumeQuota(IServerUserItem * pIServerUserItem)
{
	SCORE consumeQuota=0L;
	/*SCORE lMinTableScore=m_pGameServiceOption->lMinTableScore;
	if(m_pITableFrame->GetGameStatus()==GAME_STATUS_FREE)
	{
		consumeQuota=pIServerUserItem->GetUserScore()-lMinTableScore;

	}*/
	return consumeQuota;
}

//是否衰减
bool CTableFrameSink::NeedDeductStorage()
{
	for ( int i = 0; i < m_wPlayerCount; ++i )
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if (pIServerUserItem == NULL ) continue; 

		if(!pIServerUserItem->IsAndroidUser())
		{
			return true;
		}
	}

	return false;

}

//读取配置
void CTableFrameSink::ReadConfigInformation()
{	
	//获取自定义配置
	tagCustomRule *pCustomRule = (tagCustomRule *)m_pGameServiceOption->cbCustomRule;
	ASSERT(pCustomRule);
	
	g_lRoomStorageStart = pCustomRule->lRoomStorageStart;
	g_lRoomStorageCurrent = pCustomRule->lRoomStorageStart;
	g_lStorageDeductRoom = pCustomRule->lRoomStorageDeduct;
	g_lStorageMax1Room = pCustomRule->lRoomStorageMax1;
	g_lStorageMul1Room = pCustomRule->lRoomStorageMul1;
	g_lStorageMax2Room = pCustomRule->lRoomStorageMax2;
	g_lStorageMul2Room = pCustomRule->lRoomStorageMul2;

	if( g_lStorageDeductRoom < 0 || g_lStorageDeductRoom > 1000 )
		g_lStorageDeductRoom = 0;
	if ( g_lStorageDeductRoom > 1000 )
		g_lStorageDeductRoom = 1000;
	if (g_lStorageMul1Room < 0 || g_lStorageMul1Room > 100) 
		g_lStorageMul1Room = 50;
	if (g_lStorageMul2Room < 0 || g_lStorageMul2Room > 100) 
		g_lStorageMul2Room = 80;
}

//更新房间用户信息
void CTableFrameSink::UpdateRoomUserInfo(IServerUserItem *pIServerUserItem, USERACTION userAction)
{
	//变量定义
	ROOMUSERINFO roomUserInfo;
	ZeroMemory(&roomUserInfo, sizeof(roomUserInfo));

	roomUserInfo.dwGameID = pIServerUserItem->GetGameID();
	CopyMemory(&(roomUserInfo.szNickName), pIServerUserItem->GetNickName(), sizeof(roomUserInfo.szNickName));
	roomUserInfo.cbUserStatus = pIServerUserItem->GetUserStatus();
	roomUserInfo.cbGameStatus = m_pITableFrame->GetGameStatus();

	roomUserInfo.bAndroid = pIServerUserItem->IsAndroidUser();

	//用户坐下和重连
	if (userAction == USER_SITDOWN || userAction == USER_RECONNECT)
	{
		roomUserInfo.wChairID = pIServerUserItem->GetChairID();
		roomUserInfo.wTableID = pIServerUserItem->GetTableID() + 1;
	}
	else if (userAction == USER_STANDUP || userAction == USER_OFFLINE)
	{
		roomUserInfo.wChairID = INVALID_CHAIR;
		roomUserInfo.wTableID = INVALID_TABLE;
	}
	
	g_MapRoomUserInfo.SetAt(pIServerUserItem->GetUserID(), roomUserInfo);

	//遍历映射，删除离开房间的玩家，
	POSITION ptHead = g_MapRoomUserInfo.GetStartPosition();
	DWORD dwUserID = 0;
	ROOMUSERINFO userinfo;
	ZeroMemory(&userinfo, sizeof(userinfo));
	TCHAR szNickName[LEN_NICKNAME];
	ZeroMemory(szNickName, sizeof(szNickName));
	DWORD *pdwRemoveKey	= new DWORD[g_MapRoomUserInfo.GetSize()];
	ZeroMemory(pdwRemoveKey, sizeof(DWORD) * g_MapRoomUserInfo.GetSize());
	WORD wRemoveKeyIndex = 0;

	while(ptHead)
	{
		g_MapRoomUserInfo.GetNextAssoc(ptHead, dwUserID, userinfo);
		
		if (userinfo.dwGameID == 0 && (_tcscmp(szNickName, userinfo.szNickName) == 0) && userinfo.cbUserStatus == 0 )
		{
			pdwRemoveKey[wRemoveKeyIndex++] = dwUserID;
		}

	}
	
	for (WORD i=0; i<wRemoveKeyIndex; i++)
	{
		g_MapRoomUserInfo.RemoveKey(pdwRemoveKey[i]);
		
		CString strtip;
		strtip.Format(TEXT("RemoveKey,wRemoveKeyIndex = %d, g_MapRoomUserInfosize = %d \n"), wRemoveKeyIndex, g_MapRoomUserInfo.GetSize());

		WriteInfo(strtip);
	}

	delete[] pdwRemoveKey;
}

//更新同桌用户控制
void CTableFrameSink::UpdateUserControl(IServerUserItem *pIServerUserItem)
{
	//变量定义
	POSITION ptListHead;
	POSITION ptTemp;
	ROOMUSERCONTROL roomusercontrol;

	//初始化
	ptListHead = g_ListRoomUserControl.GetHeadPosition();
	ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));

	//遍历链表
	while(ptListHead)
	{
		ptTemp = ptListHead;
		roomusercontrol = g_ListRoomUserControl.GetNext(ptListHead);

		//寻找已存在控制玩家
		if ((pIServerUserItem->GetGameID() == roomusercontrol.roomUserInfo.dwGameID) &&
			_tcscmp(pIServerUserItem->GetNickName(), roomusercontrol.roomUserInfo.szNickName) == 0)
		{
			//获取元素
			ROOMUSERCONTROL &tmproomusercontrol = g_ListRoomUserControl.GetAt(ptTemp);

			//重设参数
			tmproomusercontrol.roomUserInfo.wChairID = pIServerUserItem->GetChairID();
			tmproomusercontrol.roomUserInfo.wTableID = m_pITableFrame->GetTableID() + 1;

			return;
		}
	}
}

//除重用户控制
void CTableFrameSink::TravelControlList(ROOMUSERCONTROL keyroomusercontrol)
{
	//变量定义
	POSITION ptListHead;
	POSITION ptTemp;
	ROOMUSERCONTROL roomusercontrol;

	//初始化
	ptListHead = g_ListRoomUserControl.GetHeadPosition();
	ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));

	//遍历链表
	while(ptListHead)
	{
		ptTemp = ptListHead;
		roomusercontrol = g_ListRoomUserControl.GetNext(ptListHead);

		//寻找已存在控制玩家在用一张桌子切换椅子
		if ((keyroomusercontrol.roomUserInfo.dwGameID == roomusercontrol.roomUserInfo.dwGameID) &&
			_tcscmp(keyroomusercontrol.roomUserInfo.szNickName, roomusercontrol.roomUserInfo.szNickName) == 0)
		{
			g_ListRoomUserControl.RemoveAt(ptTemp);
		}
	}
}

//是否满足控制条件
void CTableFrameSink::IsSatisfyControl(ROOMUSERINFO &userInfo, bool &bEnableControl)
{
	if (userInfo.wChairID == INVALID_CHAIR || userInfo.wTableID == INVALID_TABLE)
	{
		bEnableControl = FALSE;
		return;
	}

	if (userInfo.cbUserStatus == US_SIT || userInfo.cbUserStatus == US_READY || userInfo.cbUserStatus == US_PLAYING)
	{
		bEnableControl = TRUE;
		return;	
	}
	else
	{
		bEnableControl = FALSE;
		return;
	}
}

//分析房间用户控制
bool CTableFrameSink::AnalyseRoomUserControl(ROOMUSERCONTROL &Keyroomusercontrol, POSITION &ptList)
{
	//变量定义
	POSITION ptListHead;
	POSITION ptTemp;
	ROOMUSERCONTROL roomusercontrol;
	
	//遍历链表
	for (WORD i=0; i<m_wPlayerCount; i++)
	{
		IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
		if (!pIServerUserItem)
		{
			continue;
		}
		
		//初始化
		ptListHead = g_ListRoomUserControl.GetHeadPosition();
		ZeroMemory(&roomusercontrol, sizeof(roomusercontrol));

		//遍历链表
		while(ptListHead)
		{
			ptTemp = ptListHead;
			roomusercontrol = g_ListRoomUserControl.GetNext(ptListHead);
			
			//寻找玩家
			if ((pIServerUserItem->GetGameID() == roomusercontrol.roomUserInfo.dwGameID) &&
				_tcscmp(pIServerUserItem->GetNickName(), roomusercontrol.roomUserInfo.szNickName) == 0)
			{
				//清空控制局数为0的元素
				if (roomusercontrol.userControl.cbControlCount == 0)
				{
					g_ListRoomUserControl.RemoveAt(ptTemp);
					break;
				}

				if (roomusercontrol.userControl.control_type == CONTINUE_CANCEL)
				{
					g_ListRoomUserControl.RemoveAt(ptTemp);
					break;
				}

				//拷贝数据
				CopyMemory(&Keyroomusercontrol, &roomusercontrol, sizeof(roomusercontrol));
				ptList = ptTemp;

				return true;
			}

		}
		
	}
	
	return false;
}

void CTableFrameSink::GetControlTypeString(CONTROL_TYPE &controlType, CString &controlTypestr)
{
	switch(controlType)
	{
	case CONTINUE_WIN:
		{
			controlTypestr = TEXT("控制类型为连赢");
			break;
		}
	case CONTINUE_LOST:
		{
			controlTypestr = TEXT("控制类型为连输");
			break;
		}
	case CONTINUE_CANCEL:
		{
			controlTypestr = TEXT("控制类型为取消控制");
			break;
		}
	}
}

//写日志文件
void CTableFrameSink::WriteInfo(LPCTSTR pszString)
{
	//设置语言区域
	char* old_locale = _strdup( setlocale(LC_CTYPE,NULL) );
	setlocale( LC_CTYPE, "chs" );

	CStdioFile myFile;
	CString strFileName = TEXT("新牛牛控制KKKLOG.txt");
	BOOL bOpen = myFile.Open(strFileName, CFile::modeReadWrite|CFile::modeCreate|CFile::modeNoTruncate);
	if ( bOpen )
	{	
		myFile.SeekToEnd();
		myFile.WriteString( pszString );
		myFile.Flush();
		myFile.Close();
	}

	//还原区域设定
	setlocale( LC_CTYPE, old_locale );
	free( old_locale );
}

//判断积分约占房间
bool CTableFrameSink::IsRoomCardScoreType()
{
	return (m_pITableFrame->GetDataBaseMode() == 1) && (((m_pGameServiceOption->wServerType) & GAME_GENRE_PERSONAL) != 0);
}

//判断金币约占房间
bool CTableFrameSink::IsRoomCardTreasureType()
{
	return (m_pITableFrame->GetDataBaseMode() == 0) && (((m_pGameServiceOption->wServerType) & GAME_GENRE_PERSONAL) != 0);
}

//判断房卡房间
bool CTableFrameSink::IsRoomCardType()
{
	return ((m_pGameServiceOption->wServerType) & GAME_GENRE_PERSONAL) != 0;
}

//获取坐庄模式
BANKER_CONFIG CTableFrameSink::GetBankerConfig()
{
	//房卡类型
	if (IsRoomCardType())
	{
		BYTE *pGameRule = m_pITableFrame->GetGameRule();
		if (pGameRule[2] == DESPOT_BANKER)
		{
			return DESPOT_BANKER;
		}
		else if (pGameRule[2] == NORMAL_BANKER)
		{
			return NORMAL_BANKER;
		}
		else if (pGameRule[2] == RANDOM_BANKER)
		{
			return RANDOM_BANKER;
		}
		else 
		{
			return NORMAL_BANKER;
		}
	}
	else	//非房卡模式传统坐庄
	{
		return NORMAL_BANKER;
	}

	return NORMAL_BANKER;
}

//////////////////////////////////////////////////////////////////////////
