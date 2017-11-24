#ifndef GAME_MATCH_SERVICE_MANAGER_HEAD_FILE
#define GAME_MATCH_SERVICE_MANAGER_HEAD_FILE

#pragma once

#include "Stdafx.h"
#include "..\..\服务器组件\游戏服务器\PersonalRoomServiceHead.h"

////////////////////////////////////////////////////////////////////////

//约战服务管理类
class PERSONAL_ROOM_SERVICE_CLASS CPersonalRoomServiceManager : public IPersonalRoomServiceManager
{
	//状态变量
protected:
	bool								m_bIsService;					//服务标识	
	
	//接口变量
protected:	
	IPersonalRoomItem *					m_pIPersonalRoomItem;				//约战子项

	//服务接口
protected:
	IMainServiceFrame *					m_pIGameServiceFrame;			//功能接口

	//函数定义
public:
	//构造函数
	CPersonalRoomServiceManager(void);
	//析构函数
	virtual ~CPersonalRoomServiceManager(void);

	//基础接口
public:
	//释放对象
	virtual VOID  Release() { delete this; }
	//接口查询
	virtual VOID *  QueryInterface(const IID & Guid, DWORD dwQueryVer);
	
	//控制接口
public:
	//停止服务
	virtual bool StopService();
	//启动服务
	virtual bool StartService();
	
	//管理接口
public:
	//创建约战房间
	virtual bool CreatePersonalRoom(BYTE cbPersonalRoomType);
	//绑定桌子
	virtual bool BindTableFrame(ITableFrame * pTableFrame,WORD wTableID);
	//初始化接口
	virtual bool InitPersonalRooomInterface(tagPersonalRoomManagerParameter & PersonalManagerParameter);	

	//系统事件
public:
	//时间事件
	virtual bool OnEventTimer(DWORD dwTimerID, WPARAM dwBindParameter);
	//数据库事件
	virtual bool OnEventDataBase(WORD wRequestID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize,DWORD dwContextID);

	//网络事件
public:
	//约战事件
	virtual bool OnEventSocketPersonalRoom(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem, DWORD dwSocketID);	
	//约战服务器事件
	virtual bool OnTCPSocketMainServiceInfo(WORD wSubCmdID, VOID * pData, WORD wDataSize);

	//用户事件
public:
	//用户登录
	virtual bool OnEventUserLogon(IServerUserItem * pIServerUserItem);
	//用户登出
	virtual bool OnEventUserLogout(IServerUserItem * pIServerUserItem);
	//登录完成
	virtual bool OnEventUserLogonFinish(IServerUserItem * pIServerUserItem);

	//接口信息
public:
	//用户接口
	virtual IUnknownEx * GetServerUserItemSink();
};

//////////////////////////////////////////////////////////////////////////
#endif