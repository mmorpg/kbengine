/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2012 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "dbmgr.hpp"
#include "dbmgr_interface.hpp"
#include "dbtasks.hpp"
#include "billinghandler.hpp"
#include "db_mysql/kbe_table_mysql.hpp"
#include "network/common.hpp"
#include "network/tcp_packet.hpp"
#include "network/udp_packet.hpp"
#include "network/message_handler.hpp"
#include "thread/threadpool.hpp"
#include "server/componentbridge.hpp"
#include "server/components.hpp"
#include "dbmgr_lib/db_interface.hpp"
#include "db_mysql/db_interface_mysql.hpp"
#include "entitydef/scriptdef_module.hpp"

#include "baseapp/baseapp_interface.hpp"
#include "cellapp/cellapp_interface.hpp"
#include "baseappmgr/baseappmgr_interface.hpp"
#include "cellappmgr/cellappmgr_interface.hpp"
#include "loginapp/loginapp_interface.hpp"

namespace KBEngine{
	
ServerConfig g_serverConfig;
KBE_SINGLETON_INIT(Dbmgr);

//-------------------------------------------------------------------------------------
Dbmgr::Dbmgr(Mercury::EventDispatcher& dispatcher, 
			 Mercury::NetworkInterface& ninterface, 
			 COMPONENT_TYPE componentType,
			 COMPONENT_ID componentID):
	ServerApp(dispatcher, ninterface, componentType, componentID),
	loopCheckTimerHandle_(),
	mainProcessTimer_(),
	idServer_(1, 1024),
	pGlobalData_(NULL),
	pGlobalBases_(NULL),
	pCellAppData_(NULL),
	bufferedDBTasks_(),
	dbThreadPool_(),
	numWrittenEntity_(0),
	numRemovedEntity_(0),
	numQueryEntity_(0),
	numExecuteRawDatabaseCommand_(0),
	numCreatedAccount_(0),
	pBillingHandler_(NULL)
{
}

//-------------------------------------------------------------------------------------
Dbmgr::~Dbmgr()
{
	loopCheckTimerHandle_.cancel();
	mainProcessTimer_.cancel();
	KBEngine::sleep(300);

	SAFE_RELEASE(pBillingHandler_);
}

//-------------------------------------------------------------------------------------
bool Dbmgr::initializeWatcher()
{
	WATCH_OBJECT("numWrittenEntity", numWrittenEntity_);
	WATCH_OBJECT("numRemovedEntity", numRemovedEntity_);
	WATCH_OBJECT("numQueryEntity", numQueryEntity_);
	WATCH_OBJECT("numExecuteRawDatabaseCommand", numExecuteRawDatabaseCommand_);
	WATCH_OBJECT("numCreatedAccount", numCreatedAccount_);
	return ServerApp::initializeWatcher();
}

//-------------------------------------------------------------------------------------
bool Dbmgr::run()
{
	return ServerApp::run();
}

//-------------------------------------------------------------------------------------
void Dbmgr::handleTimeout(TimerHandle handle, void * arg)
{
	switch (reinterpret_cast<uintptr>(arg))
	{
		case TIMEOUT_TICK:
			this->handleMainTick();
			break;
		case TIMEOUT_CHECK_STATUS:
			this->handleCheckStatusTick();
			break;
		default:
			break;
	}

	ServerApp::handleTimeout(handle, arg);
}

//-------------------------------------------------------------------------------------
void Dbmgr::handleMainTick()
{
	 //time_t t = ::time(NULL);
	 //DEBUG_MSG("Dbmgr::handleGameTick[%"PRTime"]:%u\n", t, time_);
	
	g_kbetime++;
	threadPool_.onMainThreadTick();
	dbThreadPool_.onMainThreadTick();
	getNetworkInterface().handleChannels(&DbmgrInterface::messageHandlers);
}

//-------------------------------------------------------------------------------------
void Dbmgr::handleCheckStatusTick()
{
}

//-------------------------------------------------------------------------------------
bool Dbmgr::initializeBegin()
{
	return true;
}

//-------------------------------------------------------------------------------------
bool Dbmgr::inInitialize()
{
	// 初始化数据类别
	// demo/res/scripts/entity_defs/alias.xml
	if(!DataTypes::initialize("scripts/entity_defs/alias.xml"))
		return false;

	// 初始化所有扩展模块
	// demo/res/scripts/
	std::vector<PyTypeObject*>	scriptBaseTypes;
	if(!EntityDef::initialize(Resmgr::getSingleton().respaths()[1] + "res/scripts/", 
		scriptBaseTypes, componentType_)){
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool Dbmgr::initializeEnd()
{
	// 添加一个timer， 每秒检查一些状态
	loopCheckTimerHandle_ = this->getMainDispatcher().addTimer(1000000, this,
							reinterpret_cast<void *>(TIMEOUT_CHECK_STATUS));

	mainProcessTimer_ = this->getMainDispatcher().addTimer(1000000 / g_kbeSrvConfig.gameUpdateHertz(), this,
							reinterpret_cast<void *>(TIMEOUT_TICK));

	// 添加globalData, globalBases, cellAppData支持
	pGlobalData_ = new GlobalDataServer(GlobalDataServer::GLOBAL_DATA);
	pGlobalBases_ = new GlobalDataServer(GlobalDataServer::GLOBAL_BASES);
	pCellAppData_ = new GlobalDataServer(GlobalDataServer::CELLAPP_DATA);
	pGlobalData_->addConcernComponentType(CELLAPP_TYPE);
	pGlobalData_->addConcernComponentType(BASEAPP_TYPE);
	pGlobalBases_->addConcernComponentType(BASEAPP_TYPE);
	pCellAppData_->addConcernComponentType(CELLAPP_TYPE);
	return initBillingHandler() && initDB();
}

//-------------------------------------------------------------------------------------		
bool Dbmgr::initBillingHandler()
{
	pBillingHandler_ = BillingHandlerFactory::create(g_kbeSrvConfig.billingSystemType(), threadPool(), dbThreadPool_);

	INFO_MSG(boost::format("Dbmgr::initBillingHandler: billing addr(%1%), type:%2%.\n") % 
		g_kbeSrvConfig.billingSystemAddr().c_str() %
		g_kbeSrvConfig.billingSystemType());

	if(strlen(g_kbeSrvConfig.billingSystemThirdpartyAccountServiceAddr()) > 0)
	{
		INFO_MSG(boost::format("Dbmgr::initBillingHandler: thirdpartyAccountService_addr(%1%:%2%).\n") % 
			g_kbeSrvConfig.billingSystemThirdpartyAccountServiceAddr() %
			g_kbeSrvConfig.billingSystemThirdpartyAccountServicePort());
	}

	if(strlen(g_kbeSrvConfig.billingSystemThirdpartyChargeServiceAddr()) > 0)
	{
		INFO_MSG(boost::format("Dbmgr::initBillingHandler: thirdpartyChargeService_addr(%1%:%2%).\n") % 
			g_kbeSrvConfig.billingSystemThirdpartyChargeServiceAddr() %
			g_kbeSrvConfig.billingSystemThirdpartyChargeServicePort());
	}

	return pBillingHandler_->initialize();
}

//-------------------------------------------------------------------------------------		
bool Dbmgr::initDB()
{
	ENGINE_COMPONENT_INFO& dbcfg = g_kbeSrvConfig.getDBMgr();

	DBInterface* pDBInterface = DBUtil::createInterface();
	if(pDBInterface == NULL)
	{
		ERROR_MSG("Dbmgr::initializeEnd: can't create dbinterface!\n");
		return false;
	}

	bool ret = DBUtil::initialize(pDBInterface);
	pDBInterface->detach();
	SAFE_RELEASE(pDBInterface);

	if(!dbThreadPool_.isInitialize())
	{
		dbThreadPool_.createThreadPool(dbcfg.db_numConnections, 
			dbcfg.db_numConnections, dbcfg.db_numConnections);
		return ret;
	}

	return false;
}

//-------------------------------------------------------------------------------------
void Dbmgr::finalise()
{
	SAFE_RELEASE(pGlobalData_);
	SAFE_RELEASE(pGlobalBases_);
	SAFE_RELEASE(pCellAppData_);

	dbThreadPool_.finalise();
	ServerApp::finalise();
}

//-------------------------------------------------------------------------------------
void Dbmgr::onReqAllocEntityID(Mercury::Channel* pChannel, int8 componentType, COMPONENT_ID componentID)
{
	KBEngine::COMPONENT_TYPE ct = static_cast<KBEngine::COMPONENT_TYPE>(componentType);

	// 获取一个id段 并传输给IDClient
	std::pair<ENTITY_ID, ENTITY_ID> idRange = idServer_.allocRange();
	Mercury::Bundle* pBundle = Mercury::Bundle::ObjPool().createObject();

	if(ct == BASEAPP_TYPE)
		(*pBundle).newMessage(BaseappInterface::onReqAllocEntityID);
	else	
		(*pBundle).newMessage(CellappInterface::onReqAllocEntityID);

	(*pBundle) << idRange.first;
	(*pBundle) << idRange.second;
	(*pBundle).send(this->getNetworkInterface(), pChannel);
	Mercury::Bundle::ObjPool().reclaimObject(pBundle);
}

//-------------------------------------------------------------------------------------
void Dbmgr::onRegisterNewApp(Mercury::Channel* pChannel, int32 uid, std::string& username, 
						int8 componentType, uint64 componentID, 
						uint32 intaddr, uint16 intport, uint32 extaddr, uint16 extport)
{
	ServerApp::onRegisterNewApp(pChannel, uid, username, componentType, componentID, 
						intaddr, intport, extaddr, extport);

	KBEngine::COMPONENT_TYPE tcomponentType = (KBEngine::COMPONENT_TYPE)componentType;

	// 下一步:
	// 如果是连接到dbmgr则需要等待接收app初始信息
	// 例如：初始会分配entityID段以及这个app启动的顺序信息（是否第一个baseapp启动）
	if(tcomponentType == BASEAPP_TYPE || 
		tcomponentType == CELLAPP_TYPE || 
		tcomponentType == LOGINAPP_TYPE)
	{
		Mercury::Bundle* pBundle = Mercury::Bundle::ObjPool().createObject();
		int32 startGlobalOrder = Componentbridge::getComponents().getGlobalOrderLog()[getUserUID()];
		int32 startGroupOrder = 0;

		switch(tcomponentType)
		{
		case BASEAPP_TYPE:
			{
				startGroupOrder = Componentbridge::getComponents().getBaseappGroupOrderLog()[getUserUID()];
				
				onGlobalDataClientLogon(pChannel, BASEAPP_TYPE);

				std::pair<ENTITY_ID, ENTITY_ID> idRange = idServer_.allocRange();
				(*pBundle).newMessage(BaseappInterface::onDbmgrInitCompleted);
				BaseappInterface::onDbmgrInitCompletedArgs5::staticAddToBundle((*pBundle), g_kbetime, idRange.first, 
					idRange.second, startGlobalOrder, startGroupOrder);
			}
			break;
		case CELLAPP_TYPE:
			{
				startGroupOrder = Componentbridge::getComponents().getCellappGroupOrderLog()[getUserUID()];
				
				onGlobalDataClientLogon(pChannel, CELLAPP_TYPE);

				std::pair<ENTITY_ID, ENTITY_ID> idRange = idServer_.allocRange();
				(*pBundle).newMessage(CellappInterface::onDbmgrInitCompleted);
				CellappInterface::onDbmgrInitCompletedArgs5::staticAddToBundle((*pBundle), g_kbetime, idRange.first, 
					idRange.second, startGlobalOrder, startGroupOrder);
			}
			break;
		case LOGINAPP_TYPE:
			startGroupOrder = Componentbridge::getComponents().getLoginappGroupOrderLog()[getUserUID()];

			(*pBundle).newMessage(LoginappInterface::onDbmgrInitCompleted);
			LoginappInterface::onDbmgrInitCompletedArgs2::staticAddToBundle((*pBundle), 
				startGlobalOrder, startGroupOrder);

			break;
		default:
			break;
		}

		(*pBundle).send(networkInterface_, pChannel);
		Mercury::Bundle::ObjPool().reclaimObject(pBundle);
	}

	// 如果是baseapp或者cellapp则将自己注册到所有其他baseapp和cellapp
	if(tcomponentType == BASEAPP_TYPE || 
		tcomponentType == CELLAPP_TYPE)
	{
		KBEngine::COMPONENT_TYPE broadcastCpTypes[2] = {BASEAPP_TYPE, CELLAPP_TYPE};
		for(int idx = 0; idx < 2; idx++)
		{
			Components::COMPONENTS cts = Components::getSingleton().getComponents(broadcastCpTypes[idx]);
			Components::COMPONENTS::iterator fiter = cts.begin();
			for(; fiter != cts.end(); fiter++)
			{
				if((*fiter).cid == componentID)
					continue;

				Mercury::Bundle* pBundle = Mercury::Bundle::ObjPool().createObject();
				ENTITTAPP_COMMON_MERCURY_MESSAGE(broadcastCpTypes[idx], (*pBundle), onGetEntityAppFromDbmgr);
				
				if(tcomponentType == BASEAPP_TYPE)
				{
					BaseappInterface::onGetEntityAppFromDbmgrArgs8::staticAddToBundle((*pBundle), 
						uid, username, componentType, componentID, 
							intaddr, intport, extaddr, extport);
				}
				else
				{
					CellappInterface::onGetEntityAppFromDbmgrArgs8::staticAddToBundle((*pBundle), 
						uid, username, componentType, componentID, 
							intaddr, intport, extaddr, extport);
				}
				
				KBE_ASSERT((*fiter).pChannel != NULL);
				(*pBundle).send(networkInterface_, (*fiter).pChannel);
				Mercury::Bundle::ObjPool().reclaimObject(pBundle);
			}
		}
	}
}

//-------------------------------------------------------------------------------------
void Dbmgr::onGlobalDataClientLogon(Mercury::Channel* pChannel, COMPONENT_TYPE componentType)
{
	if(BASEAPP_TYPE == componentType)
	{
		pGlobalBases_->onGlobalDataClientLogon(pChannel, componentType);
		pGlobalData_->onGlobalDataClientLogon(pChannel, componentType);
	}
	else if(CELLAPP_TYPE == componentType)
	{
		pGlobalData_->onGlobalDataClientLogon(pChannel, componentType);
		pCellAppData_->onGlobalDataClientLogon(pChannel, componentType);
	}
	else
	{
		ERROR_MSG(boost::format("Dbmgr::onGlobalDataClientLogon: nonsupport %1%!\n") %
			COMPONENT_NAME_EX(componentType));
	}
}

//-------------------------------------------------------------------------------------
void Dbmgr::onBroadcastGlobalDataChange(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	uint8 dataType;
	std::string key, value;
	bool isDelete;
	COMPONENT_TYPE componentType;
	
	s >> dataType;
	s >> isDelete;

	s.readBlob(key);

	if(!isDelete)
	{
		s.readBlob(value);
	}

	s >> componentType;

	switch(dataType)
	{
	case GlobalDataServer::GLOBAL_DATA:
		if(isDelete)
			pGlobalData_->del(pChannel, componentType, key);
		else
			pGlobalData_->write(pChannel, componentType, key, value);
		break;
	case GlobalDataServer::GLOBAL_BASES:
		if(isDelete)
			pGlobalBases_->del(pChannel, componentType, key);
		else
			pGlobalBases_->write(pChannel, componentType, key, value);
		break;
	case GlobalDataServer::CELLAPP_DATA:
		if(isDelete)
			pCellAppData_->del(pChannel, componentType, key);
		else
			pCellAppData_->write(pChannel, componentType, key, value);
		break;
	default:
		KBE_ASSERT(false && "dataType is error!\n");
		break;
	};
}

//-------------------------------------------------------------------------------------
void Dbmgr::reqCreateAccount(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	std::string registerName, password, datas;
	s >> registerName >> password;
	s.readBlob(datas);

	if(registerName.size() == 0)
	{
		ERROR_MSG("Dbmgr::reqCreateAccount: registerName is empty.\n");
		return;
	}

	pBillingHandler_->createAccount(pChannel, registerName, password, datas);
	numCreatedAccount_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::onCreateAccountCBFromBilling(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	pBillingHandler_->onCreateAccountCB(s);
}

//-------------------------------------------------------------------------------------
void Dbmgr::onAccountLogin(Mercury::Channel* pChannel, KBEngine::MemoryStream& s) 
{
	std::string loginName, password, datas;
	s >> loginName >> password;
	s.readBlob(datas);

	if(loginName.size() == 0)
	{
		ERROR_MSG("Dbmgr::onAccountLogin: loginName is empty.\n");
		return;
	}

	pBillingHandler_->loginAccount(pChannel, loginName, password, datas);
}

//-------------------------------------------------------------------------------------
void Dbmgr::onLoginAccountCBBFromBilling(Mercury::Channel* pChannel, KBEngine::MemoryStream& s) 
{
	pBillingHandler_->onLoginAccountCB(s);
}

//-------------------------------------------------------------------------------------
void Dbmgr::queryAccount(Mercury::Channel* pChannel, 
						 std::string& accountName, 
						 std::string& password,
						 COMPONENT_ID componentID,
						 ENTITY_ID entityID,
						 DBID entityDBID)
{
	if(accountName.size() == 0)
	{
		ERROR_MSG("Dbmgr::queryAccount: accountName is empty.\n");
		return;
	}

	bufferedDBTasks_.addTask(new DBTaskQueryAccount(pChannel->addr(), 
		accountName, password, componentID, entityID, entityDBID));

	numQueryEntity_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::onAccountOnline(Mercury::Channel* pChannel, 
							std::string& accountName, 
							COMPONENT_ID componentID, 
							ENTITY_ID entityID)
{
	// PUSH_THREAD_TASK(new DBTaskAccountOnline(pChannel->addr(), 
	//	accountName, componentID, entityID));
}

//-------------------------------------------------------------------------------------
void Dbmgr::onEntityOffline(Mercury::Channel* pChannel, DBID dbid)
{
	dbThreadPool_.addTask(new DBTaskEntityOffline(pChannel->addr(), dbid));
}

//-------------------------------------------------------------------------------------
void Dbmgr::executeRawDatabaseCommand(Mercury::Channel* pChannel, 
									  KBEngine::MemoryStream& s)
{
	dbThreadPool_.addTask(new DBTaskExecuteRawDatabaseCommand(pChannel->addr(), s));
	s.opfini();

	numExecuteRawDatabaseCommand_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::writeEntity(Mercury::Channel* pChannel, 
						KBEngine::MemoryStream& s)
{
	ENTITY_ID eid;
	DBID entityDBID;
	COMPONENT_ID componentID;

	s >> componentID >> eid >> entityDBID;

	bufferedDBTasks_.addTask(new DBTaskWriteEntity(pChannel->addr(), componentID, eid, entityDBID, s));
	s.opfini();

	numWrittenEntity_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::removeEntity(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	ENTITY_ID eid;
	DBID entityDBID;
	COMPONENT_ID componentID;

	s >> componentID >> eid >> entityDBID;
	KBE_ASSERT(entityDBID > 0);

	bufferedDBTasks_.addTask(new DBTaskRemoveEntity(pChannel->addr(), 
		componentID, eid, entityDBID, s));

	s.opfini();

	numRemovedEntity_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::queryEntity(Mercury::Channel* pChannel, COMPONENT_ID componentID, DBID dbid, 
	std::string& entityType, CALLBACK_ID callbackID, ENTITY_ID entityID)
{
	bufferedDBTasks_.addTask(new DBTaskQueryEntity(pChannel->addr(), entityType, 
		dbid, componentID, callbackID, entityID));

	numQueryEntity_++;
}

//-------------------------------------------------------------------------------------
void Dbmgr::syncEntityStreamTemplate(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	KBEAccountTable* pTable = 
		static_cast<KBEAccountTable*>(EntityTables::getSingleton().findKBETable("kbe_accountinfos"));

	KBE_ASSERT(pTable);

	pTable->accountDefMemoryStream(s);
	s.opfini();
}

//-------------------------------------------------------------------------------------
void Dbmgr::charge(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	pBillingHandler_->charge(pChannel, s);
}

//-------------------------------------------------------------------------------------
void Dbmgr::onChargeCB(Mercury::Channel* pChannel, KBEngine::MemoryStream& s)
{
	pBillingHandler_->onChargeCB(s);
}

//-------------------------------------------------------------------------------------

}
