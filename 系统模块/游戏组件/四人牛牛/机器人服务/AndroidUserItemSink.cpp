#include "Stdafx.h"
#include "AndroidUserItemSink.h"
#include "math.h"
#include <locale>

//////////////////////////////////////////////////////////////////////////

//辅助时间
#define TIME_LESS						2									//最少时间

//游戏时间
#define TIME_USER_CALL_BANKER			3									//叫庄时间
#define TIME_USER_START_GAME			3									//开始时间
#define TIME_USER_ADD_SCORE				3									//下注时间
#define TIME_USER_OPEN_CARD				3									//摊牌时间

#define TIME_CHECK_BANKER				30									//摊牌时间

//游戏时间
#define IDI_START_GAME					(100)									//开始定时器
#define IDI_CALL_BANKER					(101)									//叫庄定时器
#define IDI_USER_ADD_SCORE				(102)									//下注定时器
#define IDI_OPEN_CARD					(103)									//开牌定时器
#define IDI_DELAY_TIME					(105)									//延时定时器



//////////////////////////////////////////////////////////////////////////

//构造函数
CAndroidUserItemSink::CAndroidUserItemSink()
{
	m_lTurnMaxScore = 0;
	ZeroMemory(m_HandCardData,sizeof(m_HandCardData));

	m_nRobotBankStorageMul=0;
	m_lRobotBankGetScore=0;
	m_lRobotBankGetScoreBanker=0;
	ZeroMemory(m_lRobotScoreRange,sizeof(m_lRobotScoreRange));

	m_cbDynamicJoin = FALSE;
	
	//接口变量
	m_pIAndroidUserItem=NULL;
	srand((unsigned)time(NULL));   
	
	return;
}

//析构函数
CAndroidUserItemSink::~CAndroidUserItemSink()
{
}

//接口查询
void * CAndroidUserItemSink::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IAndroidUserItemSink,Guid,dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IAndroidUserItemSink,Guid,dwQueryVer);
	return NULL;
}

//初始接口
bool CAndroidUserItemSink::Initialization(IUnknownEx * pIUnknownEx)
{
	//查询接口
	m_pIAndroidUserItem=QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx,IAndroidUserItem);
	if (m_pIAndroidUserItem==NULL) return false;


	//检查银行
	//UINT nElapse=TIME_CHECK_BANKER+rand()%100;
	//m_pIAndroidUserItem->SetGameTimer(IDI_CHECK_BANKER_OPERATE,nElapse);

	return true;
}

//重置接口
bool CAndroidUserItemSink::RepositionSink()
{
	m_lTurnMaxScore = 0;
	ZeroMemory(m_HandCardData,sizeof(m_HandCardData));

	//检查银行
	//UINT nElapse=TIME_CHECK_BANKER+rand()%100;
	//m_pIAndroidUserItem->SetGameTimer(IDI_CHECK_BANKER_OPERATE,nElapse);

	return true;
}

//时间消息
bool CAndroidUserItemSink::OnEventTimer(UINT nTimerID)
{
	try
	{
		switch (nTimerID)
		{
		case IDI_DELAY_TIME:
			{
				//开始时间
				UINT nElapse= rand() % TIME_LESS;
				m_pIAndroidUserItem->SetGameTimer(IDI_START_GAME,nElapse);

				return true;
			}

		case IDI_START_GAME:		//开始定时器
			{			
				//发送准备
				m_pIAndroidUserItem->SendUserReady(NULL,0);

				return true;
			}
		case IDI_CALL_BANKER:		//叫庄定时
			{
				//设置变量
				CMD_C_CallBanker CallBanker;
				CallBanker.bBanker = rand()%2;

				//发送信息
				m_pIAndroidUserItem->SendSocketData(SUB_C_CALL_BANKER,&CallBanker,sizeof(CallBanker));

				return true;
			}
		case IDI_USER_ADD_SCORE:	//加注定时
			{
				//可下注筹码
				LONGLONG lUserMaxScore[MAX_JETTON_AREA];
				ZeroMemory(lUserMaxScore,sizeof(lUserMaxScore));
				LONGLONG lTemp=m_lTurnMaxScore;
				for (WORD i=0;i<MAX_JETTON_AREA;i++)
				{
					if ( lTemp > 0 )
						//lUserMaxScore[i] = __max(lTemp/((i+1)),1L);
						lUserMaxScore[i]=__max(lTemp/(pow(2,i)),1L);
					else
						lUserMaxScore[i] = 1;
				}
				
				//下注区域 60%概率选择中大注下
				BYTE cbAddScoreIndex = 0;
				BYTE cbRand1 = rand() % 100;
				BYTE cbRand2 = cbRand1 % 2;

				cbAddScoreIndex = (cbRand1 < 60) ? cbRand2 : (2 + cbRand2);
		
				//发送消息
				CMD_C_AddScore AddScore;
				AddScore.lScore=lUserMaxScore[cbAddScoreIndex%MAX_JETTON_AREA];
				m_pIAndroidUserItem->SendSocketData(SUB_C_ADD_SCORE,&AddScore,sizeof(AddScore));

				CString strdebug;
				strdebug.Format(TEXT("机器人USERID = 【%d】, 下注【%I64d】，此时身上的金币【%I64d】\n"), m_pIAndroidUserItem->GetUserID(), AddScore.lScore, m_pIAndroidUserItem->GetMeUserItem()->GetUserScore());
				WriteInfo(strdebug);

				return true;	
			}
		case IDI_OPEN_CARD:			//开牌定时
			{
				//发送消息
				CMD_C_OxCard OxCard;
				OxCard.bOX=(m_GameLogic.GetCardType(m_HandCardData,MAX_COUNT)>0)?TRUE:FALSE;
				m_pIAndroidUserItem->SendSocketData(SUB_C_OPEN_CARD,&OxCard,sizeof(OxCard));

				//BankOperate();

				return true;	
			}

		}

	}
	catch (...)
	{
		CString cs;
		cs.Format(TEXT("异常ID=%d"),nTimerID);
		CTraceService::TraceString(cs,TraceLevel_Debug);
	}	
	
	return false;
}

//游戏消息
bool CAndroidUserItemSink::OnEventGameMessage(WORD wSubCmdID, void * pData, WORD wDataSize)
{
	switch (wSubCmdID)
	{
	case SUB_S_CALL_BANKER:	//用户叫庄
		{
			//消息处理
			return OnSubCallBanker(pData,wDataSize);
		}
	case SUB_S_GAME_START:	//游戏开始
		{
			//消息处理
			return OnSubGameStart(pData,wDataSize);
		}
	case SUB_S_ADD_SCORE:	//用户下注
		{
			//消息处理
			return OnSubAddScore(pData,wDataSize);
		}
	case SUB_S_SEND_CARD:	//发牌消息
		{
			//消息处理
			return OnSubSendCard(pData,wDataSize);
		}
	case SUB_S_OPEN_CARD:	//用户摊牌
		{
			//消息处理
			return OnSubOpenCard(pData,wDataSize);
		}
	case SUB_S_PLAYER_EXIT:	//用户强退
		{
			//消息处理
			return OnSubPlayerExit(pData,wDataSize);
		}
	case SUB_S_GAME_END:	//游戏结束
		{
			//消息处理
			return OnSubGameEnd(pData,wDataSize);
		}
	case SUB_S_ANDROID_BANKOPERATOR:
		{
			if (wDataSize!=sizeof(bool)) return false;
			bool bStart=*((bool *)pData);

			BankOperate(2);

			if(bStart)
			{
				bool bStart = *((bool *)pData);
				UINT nElapse= rand() % TIME_LESS + (rand() % TIME_USER_START_GAME) +2;
				m_pIAndroidUserItem->SetGameTimer(IDI_START_GAME,nElapse);
			}

			return true;
		}
	case SUB_S_ADMIN_STORAGE_INFO:
	case SUB_S_REQUEST_QUERY_RESULT:
	case SUB_S_USER_CONTROL:
    case SUB_S_USER_CONTROL_COMPLETE:
	case SUB_S_OPERATION_RECORD:
	case SUB_S_REQUEST_UDPATE_ROOMINFO_RESULT:
		{
			return true;
		}
	}

	//错误断言
	ASSERT(FALSE);

	return true;
}

//游戏消息
bool CAndroidUserItemSink::OnEventFrameMessage(WORD wSubCmdID, void * pData, WORD wDataSize)
{
	return true;
}

//场景消息
bool CAndroidUserItemSink::OnEventSceneMessage(BYTE cbGameStatus, bool bLookonOther, void * pData, WORD wDataSize)
{
	switch (cbGameStatus)
	{
	case GAME_STATUS_FREE:		//空闲状态
		{
			//效验数据
			if (wDataSize!=sizeof(CMD_S_StatusFree)) return false;

			//消息处理
			CMD_S_StatusFree * pStatusFree=(CMD_S_StatusFree *)pData;

			
			ReadConfigInformation(&(pStatusFree->CustomAndroid));

			BankOperate(2);

			//开始时间
			UINT nElapse= rand() % TIME_LESS + (rand() % TIME_USER_START_GAME);
			m_pIAndroidUserItem->SetGameTimer(IDI_START_GAME,nElapse);

			return true;
		}
	case GS_TK_CALL:	// 叫庄状态
		{
			//效验数据
			if (wDataSize!=sizeof(CMD_S_StatusCall)) return false;
			CMD_S_StatusCall * pStatusCall=(CMD_S_StatusCall *)pData;
			
			m_cbDynamicJoin = pStatusCall->cbDynamicJoin;
			
			ReadConfigInformation(&(pStatusCall->CustomAndroid));

			BankOperate(2);

			//始叫用户
			if(pStatusCall->wCallBanker==m_pIAndroidUserItem->GetChairID() && m_cbDynamicJoin == FALSE)
			{
				//叫庄时间
				UINT nElapse = TIME_LESS + (rand() % TIME_USER_CALL_BANKER);
				m_pIAndroidUserItem->SetGameTimer(IDI_CALL_BANKER,nElapse);
			}

			return true;
		}
	case GS_TK_SCORE:	//下注状态
		{
			//效验数据
			if (wDataSize!=sizeof(CMD_S_StatusScore)) return false;
			CMD_S_StatusScore * pStatusScore=(CMD_S_StatusScore *)pData;

			m_cbDynamicJoin = pStatusScore->cbDynamicJoin;

			ReadConfigInformation(&(pStatusScore->CustomAndroid));

			BankOperate(2);

			//设置变量
			m_lTurnMaxScore=pStatusScore->lTurnMaxScore;
			WORD wMeChairId = m_pIAndroidUserItem->GetChairID();

			//设置筹码
			if (pStatusScore->lTurnMaxScore>0L && pStatusScore->lTableScore[wMeChairId]==0L && m_cbDynamicJoin == FALSE)
			{
				//下注时间
				UINT nElapse = TIME_LESS + (rand() % TIME_USER_ADD_SCORE);
				m_pIAndroidUserItem->SetGameTimer(IDI_USER_ADD_SCORE,nElapse);
			}

			return true;
		}
	case GS_TK_PLAYING:	//游戏状态
		{
			//效验数据
			if (wDataSize!=sizeof(CMD_S_StatusPlay)) return false;
			CMD_S_StatusPlay * pStatusPlay=(CMD_S_StatusPlay *)pData;

			m_cbDynamicJoin = pStatusPlay->cbDynamicJoin;

			ReadConfigInformation(&(pStatusPlay->CustomAndroid));

			BankOperate(2);

			//设置变量
			m_lTurnMaxScore=pStatusPlay->lTurnMaxScore;
			WORD wMeChiarID=m_pIAndroidUserItem->GetChairID();
			CopyMemory(m_HandCardData,pStatusPlay->cbHandCardData[wMeChiarID],MAX_COUNT);

			//控件处理
			if(pStatusPlay->bOxCard[wMeChiarID]==0xff && m_cbDynamicJoin == FALSE)
			{
				//开牌时间
				UINT nElapse = TIME_LESS + 2 + (rand() % TIME_USER_OPEN_CARD);
				m_pIAndroidUserItem->SetGameTimer(IDI_OPEN_CARD,nElapse);
			}

			return true;
		}
	}

	ASSERT(FALSE);

	return false;
}

//用户进入
void CAndroidUserItemSink::OnEventUserEnter(IAndroidUserItem * pIAndroidUserItem, bool bLookonUser)
{
	return;
}

//用户离开
void CAndroidUserItemSink::OnEventUserLeave(IAndroidUserItem * pIAndroidUserItem, bool bLookonUser)
{
	return;
}

//用户积分
void CAndroidUserItemSink::OnEventUserScore(IAndroidUserItem * pIAndroidUserItem, bool bLookonUser)
{
	return;
}

//用户状态
void CAndroidUserItemSink::OnEventUserStatus(IAndroidUserItem * pIAndroidUserItem, bool bLookonUser)
{
	return;
}

//用户段位
void CAndroidUserItemSink::OnEventUserSegment(IAndroidUserItem * pIAndroidUserItem, bool bLookonUser)
{
	return;
}

//用户叫庄
bool CAndroidUserItemSink::OnSubCallBanker(const void * pBuffer, WORD wDataSize)
{

	//效验数据
	if (wDataSize!=sizeof(CMD_S_CallBanker)) return false;
	CMD_S_CallBanker * pCallBanker=(CMD_S_CallBanker *)pBuffer;

	//始叫用户
	if(pCallBanker->wCallBanker==m_pIAndroidUserItem->GetChairID() && m_cbDynamicJoin == FALSE)
	{
		//叫庄时间
		UINT nElapse = TIME_LESS + (rand() % TIME_USER_CALL_BANKER);
		m_pIAndroidUserItem->SetGameTimer(IDI_CALL_BANKER,nElapse);
	}

	return true;
}

//游戏开始
bool CAndroidUserItemSink::OnSubGameStart(const void * pBuffer, WORD wDataSize)
{
	//效验数据
	if (wDataSize!=sizeof(CMD_S_GameStart)) return false;
	CMD_S_GameStart * pGameStart=(CMD_S_GameStart *)pBuffer;

	//设置变量
	m_lTurnMaxScore=pGameStart->lTurnMaxScore;

	//设置筹码
	if (pGameStart->lTurnMaxScore>0 && m_cbDynamicJoin == FALSE)
	{
		//下注时间
		UINT nElapse = TIME_LESS + (rand() % TIME_USER_ADD_SCORE);
		m_pIAndroidUserItem->SetGameTimer(IDI_USER_ADD_SCORE,nElapse);
	}
	
	CString strdebug;
	strdebug.Format(TEXT("机器人USERID = 【%d】, 游戏开始，此时身上的金币【%I64d】\n"), m_pIAndroidUserItem->GetUserID(), m_pIAndroidUserItem->GetMeUserItem()->GetUserScore());
	WriteInfo(strdebug);

	return true;
}

//用户下注
bool CAndroidUserItemSink::OnSubAddScore(const void * pBuffer, WORD wDataSize)
{
	//效验数据
	if (wDataSize!=sizeof(CMD_S_AddScore)) return false;
	CMD_S_AddScore * pAddScore=(CMD_S_AddScore *)pBuffer;

	return true;
}

//发牌消息
bool CAndroidUserItemSink::OnSubSendCard(const void * pBuffer, WORD wDataSize)
{
	//效验数据
	if (wDataSize!=sizeof(CMD_S_SendCard)) return false;
	CMD_S_SendCard * pSendCard=(CMD_S_SendCard *)pBuffer;

	//设置数据
	WORD wMeChiarID=m_pIAndroidUserItem->GetChairID();
	CopyMemory(m_HandCardData,pSendCard->cbCardData[wMeChiarID],sizeof(m_HandCardData));

	//开牌时间
	if (m_cbDynamicJoin == FALSE)
	{
		UINT nElapse = TIME_LESS + 2 + (rand() % TIME_USER_OPEN_CARD);
		m_pIAndroidUserItem->SetGameTimer(IDI_OPEN_CARD,nElapse);
	}

	return true;
}

//用户摊牌
bool CAndroidUserItemSink::OnSubOpenCard(const void * pBuffer, WORD wDataSize)
{
	//效验数据
	if (wDataSize!=sizeof(CMD_S_Open_Card)) return false;
	CMD_S_Open_Card * pOpenCard=(CMD_S_Open_Card *)pBuffer;

	return true;
}

//用户强退
bool CAndroidUserItemSink::OnSubPlayerExit(const void * pBuffer, WORD wDataSize)
{
	//效验数据
	if (wDataSize!=sizeof(CMD_S_PlayerExit)) return false;
	CMD_S_PlayerExit * pPlayerExit=(CMD_S_PlayerExit *)pBuffer;

	return true;
}

//游戏结束
bool CAndroidUserItemSink::OnSubGameEnd(const void * pBuffer, WORD wDataSize)
{
	//效验参数
	if (wDataSize!=sizeof(CMD_S_GameEnd)) return false;
	CMD_S_GameEnd * pGameEnd=(CMD_S_GameEnd *)pBuffer;

	//删除定时器
	m_pIAndroidUserItem->KillGameTimer(IDI_CALL_BANKER);
	m_pIAndroidUserItem->KillGameTimer(IDI_USER_ADD_SCORE);
	m_pIAndroidUserItem->KillGameTimer(IDI_OPEN_CARD);

	//开始时间
	if(pGameEnd->cbDelayOverGame==0)
	{
		BankOperate(2);

		UINT nElapse= rand() % TIME_LESS + (rand() % TIME_USER_START_GAME) +2;
		m_pIAndroidUserItem->SetGameTimer(IDI_START_GAME,nElapse);
	}
	else
	{
		BankOperate(2);
		//m_pIAndroidUserItem->SetGameTimer(IDI_DELAY_TIME, (pGameEnd->cbDelayOverGame + rand() % TIME_LESS + 1));
	}

	//清理变量
	m_lTurnMaxScore = 0;
	ZeroMemory(m_HandCardData,sizeof(m_HandCardData));

	m_cbDynamicJoin = FALSE;

	CString strdebug;
	strdebug.Format(TEXT("机器人USERID = 【%d】, 游戏结束，此时身上的金币【%I64d】，游戏得分【%I64d】游戏税收【%I64d】\n"), m_pIAndroidUserItem->GetUserID(), m_pIAndroidUserItem->GetMeUserItem()->GetUserScore(), pGameEnd->lGameScore[m_pIAndroidUserItem->GetChairID()], pGameEnd->lGameTax[m_pIAndroidUserItem->GetChairID()]);
	WriteInfo(strdebug);

	return true;
}

//银行操作
void CAndroidUserItemSink::BankOperate(BYTE cbType)
{
	CString strdebug;
	strdebug.Format(TEXT("机器人USERID = 【%d】, 开始存储款，此时身上的金币【%I64d】\n"), m_pIAndroidUserItem->GetUserID(), m_pIAndroidUserItem->GetMeUserItem()->GetUserScore());
	WriteInfo(strdebug);

	if (cbType == 3)
	{
		if (rand() % 100 > 33)
		{
			return;
		}
	}
	IServerUserItem *pUserItem = m_pIAndroidUserItem->GetMeUserItem();
	if(pUserItem->GetUserStatus()>=US_SIT)
	{
		if(cbType==1)
		{
			CString strInfo;
			strInfo.Format(TEXT("大厅：状态不对，不执行存取款"));
			NcaTextOut(strInfo, TEXT(""));

			return;
			
		}
	}
	
	//变量定义
	LONGLONG lRobotScore = pUserItem->GetUserScore();

	{
		CString strInfo;
		strInfo.Format(TEXT("[%s] 分数(%I64d)"),pUserItem->GetNickName(),lRobotScore);

		if (lRobotScore > m_lRobotScoreRange[1])
		{
			CString strInfo1;
			strInfo1.Format(TEXT("满足存款条件(%I64d)"),m_lRobotScoreRange[1]);
			strInfo+=strInfo1;
			//if(cbType==1) 
				NcaTextOut(strInfo, TEXT(""));
		}
		else if (lRobotScore < m_lRobotScoreRange[0])
		{
			CString strInfo1;
			strInfo1.Format(TEXT("满足取款条件(%I64d)"),m_lRobotScoreRange[0]);
			strInfo+=strInfo1;
			//if(cbType==1) 
				NcaTextOut(strInfo, TEXT(""));
		}

		//判断存取
		if (lRobotScore > m_lRobotScoreRange[1])
		{			
			LONGLONG lSaveScore=0L;

			lSaveScore = LONGLONG(lRobotScore*m_nRobotBankStorageMul/100);
			if (lSaveScore > lRobotScore)  lSaveScore = lRobotScore;

			if (lSaveScore > 0)
				m_pIAndroidUserItem->PerformSaveScore(lSaveScore);

			LONGLONG lRobotNewScore = pUserItem->GetUserScore();

			CString strInfo;
			strInfo.Format(TEXT("[%s] 执行存款：存款前金币(%I64d)，存款后金币(%I64d)"),pUserItem->GetNickName(),lRobotScore,lRobotNewScore);

			//if(cbType==1) 
				NcaTextOut(strInfo, TEXT(""));

		}
		else if (lRobotScore < m_lRobotScoreRange[0])
		{

			CString strInfo;
			//strInfo.Format(TEXT("配置信息：取款最小值(%I64d)，取款最大值(%I64d)"),m_lRobotBankGetScore,m_lRobotBankGetScoreBanker);

			//if(cbType==1) 
			//	NcaTextOut(strInfo);
			
			SCORE lScore=rand()%m_lRobotBankGetScoreBanker+m_lRobotBankGetScore;
			if (lScore > 0)
				m_pIAndroidUserItem->PerformTakeScore(lScore);

			LONGLONG lRobotNewScore = pUserItem->GetUserScore();

			//CString strInfo;
			strInfo.Format(TEXT("[%s] 执行取款：取款前金币(%I64d)，取款后金币(%I64d)"),pUserItem->GetNickName(),lRobotScore,lRobotNewScore);

			//if(cbType==1) 
				NcaTextOut(strInfo, TEXT(""));
					
		}
	}

	strdebug.Format(TEXT("机器人USERID = 【%d】, 存储款完成，此时身上的金币【%I64d】\n"), m_pIAndroidUserItem->GetUserID(), m_pIAndroidUserItem->GetMeUserItem()->GetUserScore());
	WriteInfo(strdebug);
}

//写日志文件
void CAndroidUserItemSink::WriteInfo(LPCTSTR pszString)
{
	return;

	//设置语言区域
	char* old_locale = _strdup( setlocale(LC_CTYPE,NULL) );
	setlocale( LC_CTYPE, "chs" );

	CStdioFile myFile;
	CString strFileName = TEXT("新牛牛机器人调试.txt");
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

//读取配置
void CAndroidUserItemSink::ReadConfigInformation(tagCustomAndroid *pCustomAndroid)
{
	m_lRobotScoreRange[0] = pCustomAndroid->lRobotScoreMin;
	m_lRobotScoreRange[1] = pCustomAndroid->lRobotScoreMax;

	if (m_lRobotScoreRange[1] < m_lRobotScoreRange[0])	
		m_lRobotScoreRange[1] = m_lRobotScoreRange[0];

	m_lRobotBankGetScore = pCustomAndroid->lRobotBankGet;
	m_lRobotBankGetScoreBanker = pCustomAndroid->lRobotBankGetBanker;
	m_nRobotBankStorageMul = pCustomAndroid->lRobotBankStoMul;
	
	if (m_nRobotBankStorageMul<0||m_nRobotBankStorageMul>100) 
		m_nRobotBankStorageMul =20;
}


//组件创建函数
DECLARE_CREATE_MODULE(AndroidUserItemSink);

//////////////////////////////////////////////////////////////////////////
