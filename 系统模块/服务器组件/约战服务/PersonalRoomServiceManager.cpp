#include "StdAfx.h"
#include "AfxTempl.h"
#include "PersonalRoomGame.h"
#include "PersonalRoomServiceManager.h"
	
////////////////////////////////////////////////////////////////////////

//构造函数
CPersonalRoomServiceManager::CPersonalRoomServiceManager(void)
{
	//状态变量
	m_bIsService=false;

	//设置变量
	m_pIPersonalRoomItem=NULL;

	return;
}

//析构函数
CPersonalRoomServiceManager::~CPersonalRoomServiceManager(void)
{	
	//释放指针
	if(m_pIPersonalRoomItem!=NULL) SafeDelete(m_pIPersonalRoomItem);
}

//停止服务
bool CPersonalRoomServiceManager::StopService()
{
	////状态判断
	//ASSERT(m_bIsService==true);

	////设置状态
	//m_bIsService=false;

	////释放指针
	//if(m_pIPersonalRoomItem!=NULL) SafeRelease(m_pIPersonalRoomItem);

	return true;
}

//启动服务
bool CPersonalRoomServiceManager::StartService()
{
	//状态判断
	ASSERT(m_bIsService==false);
	if(m_bIsService==true) return false;

	//设置状态
	m_bIsService=true;

	//启动通知
	if(m_pIPersonalRoomItem!=NULL) m_pIPersonalRoomItem->OnStartService();

	return true;
}
//接口查询
void *  CPersonalRoomServiceManager::QueryInterface(const IID & Guid, DWORD dwQueryVer)
{
								 
	QUERYINTERFACE(IPersonalRoomServiceManager,Guid,dwQueryVer);	
	QUERYINTERFACE_IUNKNOWNEX(IPersonalRoomServiceManager,Guid,dwQueryVer);
	return NULL;
}

//创建约战房
bool CPersonalRoomServiceManager::CreatePersonalRoom(BYTE cbPersonalRoomType)
{
	//接口判断
	ASSERT(m_pIPersonalRoomItem==NULL);
	if(m_pIPersonalRoomItem!=NULL) return false;

	try
	{
		m_pIPersonalRoomItem = new CPersonalRoomGame();
		if(m_pIPersonalRoomItem==NULL) throw TEXT("创建约战房间失败！");
	}
	catch(...)
	{
		ASSERT(FALSE);
		return false;
	}

	return m_pIPersonalRoomItem!=NULL;
}

//初始化桌子框架
bool CPersonalRoomServiceManager::BindTableFrame(ITableFrame * pTableFrame,WORD wTableID)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->BindTableFrame(pTableFrame,wTableID);	
	}

	return true;
}


//初始化接口
bool CPersonalRoomServiceManager::InitPersonalRooomInterface(tagPersonalRoomManagerParameter & PersonalManagerParameter)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->InitPersonalRooomInterface(PersonalManagerParameter);
	}

	return true;
}

//时间事件
bool CPersonalRoomServiceManager::OnEventTimer(DWORD dwTimerID, WPARAM dwBindParameter)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventTimer(dwTimerID,dwBindParameter);	
	}

	return true;
}

//数据库事件
bool CPersonalRoomServiceManager::OnEventDataBase(WORD wRequestID, IServerUserItem * pIServerUserItem, VOID * pData, WORD wDataSize,DWORD dwContextID)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventDataBase(wRequestID,pIServerUserItem,pData,wDataSize,dwContextID);	
	}

	return true;
}


//命令消息
bool CPersonalRoomServiceManager::OnEventSocketPersonalRoom(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem, DWORD dwSocketID)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventSocketPersonalRoom(wSubCmdID,pData,wDataSize,pIServerUserItem,dwSocketID);	
	}

	return true;
}

//约战服务器事件
bool CPersonalRoomServiceManager::OnTCPSocketMainServiceInfo(WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnTCPSocketMainServiceInfo(wSubCmdID,pData,wDataSize);	
	}
	return true;
}

//用户登录
bool CPersonalRoomServiceManager::OnEventUserLogon(IServerUserItem * pIServerUserItem)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventUserLogon(pIServerUserItem);	
	}

	return true;
}

//用户登出
bool CPersonalRoomServiceManager::OnEventUserLogout(IServerUserItem * pIServerUserItem)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventUserLogout(pIServerUserItem);	
	}

	return true;
}

//登录完成
bool CPersonalRoomServiceManager::OnEventUserLogonFinish(IServerUserItem * pIServerUserItem)
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return m_pIPersonalRoomItem->OnEventUserLogonFinish(pIServerUserItem);	
	}

	return true;
}


//用户接口
IUnknownEx * CPersonalRoomServiceManager::GetServerUserItemSink()
{
	ASSERT(m_pIPersonalRoomItem!=NULL);
	if(m_pIPersonalRoomItem!=NULL)
	{
		return QUERY_OBJECT_PTR_INTERFACE(m_pIPersonalRoomItem,IUnknownEx);
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////

//组件创建函数
DECLARE_CREATE_MODULE(PersonalRoomServiceManager);
											 

////////////////////////////////////////////////////////////////////////
