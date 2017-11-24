#ifndef GAME_MATCH_SINK_HEAD_FILE
#define GAME_MATCH_SINK_HEAD_FILE

#pragma once

//引入文件
#include "..\..\服务器组件\游戏服务器\PersonalRoomServiceHead.h"
//////////////////////////////////////////////////////////////////////////

//桌子钩子类
class CPersonalTableFrameHook : public IPersonalTableFrameHook, ITableUserAction
{
	//友元定义
	friend class CGameServiceManager;
	
	//接口变量
public:
	ITableFrame						* m_pITableFrame;					//框架接口
	
	//事件接口
protected:
	IPersonalRoomEventSink				* m_pPersonalRoomEventSink;				//约战事件

	//配置变量
protected:
	const tagGameServiceOption		* m_pGameServiceOption;				//配置参数
	
	//属性变量
protected:
	static const WORD				m_wPlayerCount;						//游戏人数

	//函数定义
public:
	//构造函数
	CPersonalTableFrameHook();
	//析构函数
	virtual ~CPersonalTableFrameHook();

	//基础接口
public:
	//释放对象
	virtual VOID  Release() { delete this; }
	//接口查询
	virtual void *  QueryInterface(const IID & Guid, DWORD dwQueryVer);

	//管理接口
public:
	//设置桌子事件接口
	virtual bool SetPersonalRoomEventSink(IUnknownEx * pIUnknownEx);
	//初始化
	virtual bool InitTableFrameHook(IUnknownEx * pIUnknownEx);		

	//游戏事件
public:
	//游戏开始
	virtual bool OnEventGameStart(WORD wChairCount);
	//约战房写参与信息
	virtual void	PersonalRoomWriteJoinInfo(DWORD dwUserID, WORD wTableID,  WORD wChairID,  DWORD dwKindID,  TCHAR * szRoomID,  TCHAR * szPersonalRoomGUID);
	//游戏结束
	virtual bool OnEventGameEnd(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason);
	//游戏结束
	virtual bool  OnEventGameEnd(WORD wTableID,  WORD wChairCount, DWORD dwDrawCountLimit, DWORD & dwPersonalPlayCount, int nSpecialInfoLen, byte * cbSpecialInfo, SYSTEMTIME sysStartTime,tagPersonalUserScoreInfo * PersonalUserScoreInfo);
	//用户动作
public:	
	//用户断线
	virtual bool OnActionUserOffLine(WORD wChairID, IServerUserItem * pIServerUserItem) { return true; }
	//用户重入
	virtual bool OnActionUserConnect(WORD wChairID, IServerUserItem * pIServerUserItem) { return true; }
	//用户坐下
	virtual bool OnActionUserSitDown(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser);
	//用户起来
	virtual bool OnActionUserStandUp(WORD wChairID, IServerUserItem * pIServerUserItem, bool bLookonUser);
	//用户同意
	virtual bool OnActionUserOnReady(WORD wChairID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize);	
};

//////////////////////////////////////////////////////////////////////////

#endif