#ifndef LOCKTIME_MATCH_HEAD_FILE
#define LOCKTIME_MATCH_HEAD_FILE

#pragma once

//引入文件
#include "TableFrameHook.h"
#include "..\..\服务器组件\游戏服务器\PersonalRoomServiceHead.h"


///////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////
typedef CWHArray<tagPersonalTableParameter *> CPersonalTableParameterArray;	//私人配置

//定时赛
class CPersonalRoomGame : public IPersonalRoomItem, public IPersonalRoomEventSink, public IServerUserItemSink
{
	//状态变量
protected:

	//约战房配置
protected:
	tagPersonalRoomOption *				m_pPersonalRoomOption;	//约战配置	
	tagGameServiceOption *				m_pGameServiceOption;			//服务配置
	tagGameServiceAttrib *				m_pGameServiceAttrib;			//服务属性

	tagPersonalRoomOption				m_PersonalRoomOption;//约战房配置信息
	CPersonalTableParameterArray	m_PersonalTableParameterArray;		//私人配置

	//内核接口
protected:
	ITableFrame	**						m_ppITableFrame;				//框架接口
	ITimerEngine *						m_pITimerEngine;				//时间引擎
	IDBCorrespondManager *				m_pIDBCorrespondManager;				//数据引擎
	ITCPNetworkEngineEvent *			m_pITCPNetworkEngineEvent;		//网络引擎
	ITCPNetworkEngine *				m_pITCPNetworkEngine;				//网络引擎
	ITCPSocketService *				m_pITCPSocketService;				//网络服务

	//服务接口
protected:
	IMainServiceFrame *					m_pIGameServiceFrame;			//功能接口
	IServerUserManager *				m_pIServerUserManager;			//用户管理
	IAndroidUserManager	*				m_pAndroidUserManager;			//机器管理
	IServerUserItemSink *				m_pIServerUserItemSink;			//用户回调

	//函数定义
public:
	//构造函数
	CPersonalRoomGame();
	//析构函数
	virtual ~CPersonalRoomGame(void);

	//基础接口
public:
 	//释放对象
 	virtual VOID Release(){ delete this; }
 	//接口查询
	virtual VOID * QueryInterface(REFGUID Guid, DWORD dwQueryVer);

	//控制接口
public:
	//启动通知
	virtual void OnStartService();

	//管理接口
public:
	//绑定桌子
	virtual bool BindTableFrame(ITableFrame * pTableFrame,WORD wTableID);
	//初始化接口
	virtual bool InitPersonalRooomInterface(tagPersonalRoomManagerParameter & MatchManagerParameter);	

	//系统事件
public:
	//时间事件
	virtual bool OnEventTimer(DWORD dwTimerID, WPARAM dwBindParameter);
	//数据库事件
	virtual bool OnEventDataBase(WORD wRequestID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize,DWORD dwContextID);

	//网络事件
public:
	//私人房事件
	virtual bool OnEventSocketPersonalRoom(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem, DWORD dwSocketID);	

	//用户接口
public:
	//用户积分
	virtual bool OnEventUserItemScore(IServerUserItem * pIServerUserItem, BYTE cbReason);
	//用户数据
	virtual bool OnEventUserItemGameData(IServerUserItem *pIServerUserItem, BYTE cbReason);
	//用户状态
	virtual bool OnEventUserItemStatus(IServerUserItem * pIServerUserItem,WORD wLastTableID,WORD wLastChairID);
	//用户权限
	virtual bool OnEventUserItemRight(IServerUserItem *pIServerUserItem, DWORD dwAddRight, DWORD dwRemoveRight,BYTE cbRightKind);

	//事件接口
public:
	//用户登录
	virtual bool OnEventUserLogon(IServerUserItem * pIServerUserItem);
	//用户登出
	virtual bool OnEventUserLogout(IServerUserItem * pIServerUserItem);
	//登录完成
	virtual bool OnEventUserLogonFinish(IServerUserItem * pIServerUserItem);

	 //功能函数
public:
	 //游戏开始
	 virtual bool OnEventGameStart(ITableFrame *pITableFrame, WORD wChairCount);
	 //约战房写参与信息
	 virtual void	PersonalRoomWriteJoinInfo(DWORD dwUserID, WORD wTableID,  WORD wChairID,   DWORD dwKindID, TCHAR * szRoomID,  TCHAR * szPersonalRoomGUID);
	 //游戏结束
	 virtual bool OnEventGameEnd(ITableFrame *pITableFrame,WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason);
	 //游戏结束
	 virtual bool OnEventGameEnd(WORD wTableID,  WORD wChairCount, DWORD dwDrawCountLimit, DWORD & dwPersonalPlayCount, int nSpecialInfoLen, byte * cbSpecialInfo, SYSTEMTIME sysStartTime,tagPersonalUserScoreInfo * PersonalUserScoreInfo);

	 //用户事件
public:
	 //用户坐下
	 virtual bool OnActionUserSitDown(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser);
	 //用户起来
	 virtual bool OnActionUserStandUp(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser);
	 //用户同意
	 virtual bool OnActionUserOnReady(WORD wTableID, WORD wChairID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize);


	//约战房间
protected:
	//创建桌子
	bool OnTCPNetworkSubCreateTable(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem);
	//取消请求
	bool OnTCPNetworkSubCancelRequest(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem);
	//取消答复
	bool OnTCPNetworkSubRequestReply(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem);
	//房主强制请求桌子
	bool OnTCPNetworkSubHostDissumeTable(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem);
	//玩家请求房间成绩
	bool OnTCPNetworkSubQueryUserRoomScore(VOID * pData, WORD wDataSize, DWORD dwSocketID, IServerUserItem * pIServerUserItem);

	//数据库响应
protected:
	//创建成功
	bool OnDBCreateSucess(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//创建失败
	bool OnDBCreateFailure(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//取消创建
	bool OnDBCancelCreateTable(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//私人配置
	bool OnDBLoadPersonalParameter(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//解散房间
	bool OnDBDissumeTableResult(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//约战房间玩家请求房间信息
	bool OnDBQueryUserRoomScore(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);
	//当前用户的房卡和游戏豆
	bool OnDBCurrenceRoomCardAndBeant(DWORD dwContextID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem);

	//约战服务器事件
public:
	//约战服务器事件
	virtual bool OnTCPSocketMainServiceInfo(WORD wSubCmdID, VOID * pData, WORD wDataSize);

};

#endif