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

#include "resmgr.hpp"
#include "helper/watcher.hpp"

namespace KBEngine{
KBE_SINGLETON_INIT(Resmgr);

//-------------------------------------------------------------------------------------
Resmgr::Resmgr()
{
}

//-------------------------------------------------------------------------------------
Resmgr::~Resmgr()
{
}

//-------------------------------------------------------------------------------------
bool Resmgr::initializeWatcher()
{
	WATCH_OBJECT("syspaths/KBE_ROOT", kb_env_.root);
	WATCH_OBJECT("syspaths/KBE_RES_PATH", kb_env_.res_path);
	WATCH_OBJECT("syspaths/KBE_HYBRID_PATH", kb_env_.hybrid_path);
	return true;
}

//-------------------------------------------------------------------------------------
bool Resmgr::initialize()
{
	//if(isInit())
	//	return true;

	// 获取引擎环境配置
	kb_env_.root			= getenv("KBE_ROOT") == NULL ? "" : getenv("KBE_ROOT");
	kb_env_.res_path		= getenv("KBE_RES_PATH") == NULL ? "" : getenv("KBE_RES_PATH"); 
	kb_env_.hybrid_path		= getenv("KBE_HYBRID_PATH") == NULL ? "" : getenv("KBE_HYBRID_PATH"); 

	//kb_env_.root				= "/home/kbe/kbengine/";
	//kb_env_.res_path			= "/home/kbe/kbengine/kbe/res/;/home/kbe/kbengine/demo/;/home/kbe/kbengine/demo/res/"; 
	//kb_env_.hybrid_path		= "/home/kbe/kbengine/kbe/bin/Hybrid/"; 
	
	char ch;
	
	if(kb_env_.root.size() > 0)
	{
		ch =  kb_env_.root.at(kb_env_.root.size() - 1);
		if(ch != '/' && ch != '\\')
			kb_env_.root += "/";

		strutil::kbe_replace(kb_env_.root, "\\", "/");
		strutil::kbe_replace(kb_env_.root, "//", "/");
	}

	if(kb_env_.hybrid_path.size() > 0)
	{
		ch =  kb_env_.hybrid_path.at(kb_env_.hybrid_path.size() - 1);
		if(ch != '/' && ch != '\\')
			kb_env_.hybrid_path += "/";

		strutil::kbe_replace(kb_env_.hybrid_path, "\\", "/");
		strutil::kbe_replace(kb_env_.hybrid_path, "//", "/");
	}

	respaths_.clear();
	std::string tbuf = kb_env_.res_path;
	char splitFlag = ';';
	strutil::kbe_split<char>(tbuf, splitFlag, respaths_);

	if(respaths_.size() < 2)
	{
		respaths_.clear();
		splitFlag = ':';
		strutil::kbe_split<char>(tbuf, splitFlag, respaths_);
	}

	kb_env_.res_path = "";
	std::vector<std::string>::iterator iter = respaths_.begin();
	for(; iter != respaths_.end(); iter++)
	{
		if((*iter).size() <= 0)
			continue;

		ch =  (*iter).at((*iter).size() - 1);
		if(ch != '/' && ch != '\\')
			(*iter) += "/";

		kb_env_.res_path += (*iter);
		kb_env_.res_path += splitFlag;
		strutil::kbe_replace(kb_env_.res_path, "\\", "/");
		strutil::kbe_replace(kb_env_.res_path, "//", "/");
	}

	if(kb_env_.res_path.size() > 0)
		kb_env_.res_path.erase(kb_env_.res_path.size() - 1);

	isInit_ = true;
	return true;
}

//-------------------------------------------------------------------------------------
void Resmgr::pirnt(void)
{
	INFO_MSG(boost::format("Resmgr::initialize: KBE_ROOT=%1%\n") % kb_env_.root.c_str());
	INFO_MSG(boost::format("Resmgr::initialize: KBE_RES_PATH=%1%\n") % kb_env_.res_path.c_str());
	INFO_MSG(boost::format("Resmgr::initialize: KBE_HYBRID_PATH=%1%\n") % kb_env_.hybrid_path.c_str());
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchRes(std::string path)
{
	return matchRes(path.c_str());
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchRes(const char* path)
{
	std::vector<std::string>::iterator iter = respaths_.begin();

	for(; iter != respaths_.end(); iter++)
	{
		std::string fpath = ((*iter) + path);

		strutil::kbe_replace(fpath, "\\", "/");
		strutil::kbe_replace(fpath, "//", "/");

		FILE * f = fopen (fpath.c_str(), "r");
		if(f != NULL)
		{
			fclose(f);
			return fpath;
		}
	}
	return path;
}

//-------------------------------------------------------------------------------------
std::string Resmgr::getPySysResPath()
{
	if(respaths_.size() > 0)
	{
		return respaths_[0];
	}

	return "";
}

//-------------------------------------------------------------------------------------
FILE* Resmgr::openResource(const char* res, const char* model)
{
	return fopen(matchRes(res).c_str(), model);
}

//-------------------------------------------------------------------------------------		
}
