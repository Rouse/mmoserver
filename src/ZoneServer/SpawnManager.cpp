/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2010 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "SpawnManager.h"
#include "SpawnRegion.h"
#include "AttackableCreature.h"
#include "CreatureObject.h"
#include "PlayerObject.h"
#include "WorldConfig.h"
#include "WorldManager.h"
#include "ZoneServer/NonPersistentNpcFactory.h"
#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"

#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"

#include "Utils/utils.h"
#include "Utils/rand.h"

#include <cassert>

//=============================================================================

// Basic lair data when loading from DB.

class SpawnLairEntity
{
	public:
		SpawnLairEntity(){}

		uint64	mLairsId;
		uint64	mLairTemplateId;
		uint32	mNumberOfLairs;
};


//=============================================================================

SpawnManager* SpawnManager::mInstance  = NULL;


//======================================================================================================================

SpawnManager* SpawnManager::Instance(void)
{
	if (!mInstance)
	{
		mInstance = new SpawnManager(WorldManager::getSingletonPtr()->getDatabase());
	}
	return mInstance;
}


// This constructor prevents the default constructor to be used, as long as the constructor is kept private.

SpawnManager::SpawnManager()
{
}

//=============================================================================


SpawnManager::SpawnManager(Database* database) :mDatabase(database)
{
	// _setupDatabindings();
	this->loadSpawns();
}


//=============================================================================

SpawnManager::~SpawnManager()
{
	// _destroyDatabindings();
	mInstance = NULL;
}

//=============================================================================

//=============================================================================
//
//	Handle npc.
//
//	Main purposes are to get the npc in suitable timer queue and handling generall issues


void SpawnManager::handleObjectReady(Object* object)
{
	CreatureObject* creature = dynamic_cast<CreatureObject*>(object);
	if (creature)
	{
		creature->respawn();
	}
}


//=============================================================================================================================
//
// This part is where the natural lairs are loaded from DB.
//
//=============================================================================================================================

//
void SpawnManager::loadSpawns(void)
{
	// load spawn data

	mDatabase->ExecuteSqlAsync(this,new SpawnAsyncContainer(SpawnQuery_Spawns),
								"SELECT s.id, s.spawn_x, s.spawn_z, s.spawn_width, s.spawn_length, s.spawn_density "
								"FROM spawns s "
								"WHERE s.spawn_planet=%u;",gWorldManager->getZoneId());
}

//these are the lairs that are part of the spawn we want to load
void SpawnManager::loadSpawnGroup(uint32 spawn)
{
	//load lair and creature spawn, and optionally heightmaps cache.
	// NpcFamily_NaturalLairs

	SpawnAsyncContainer* asyncContainer = new SpawnAsyncContainer(SpawnQuery_Group);
	asyncContainer->spawnGroup = spawn;

	mDatabase->ExecuteSqlAsync(this,asyncContainer,
								"SELECT sg.id, l.id, l.lair_template, l.creature_groups_id "
								"FROM spawn_groups sg "
								"INNER JOIN  lairs l ON (l.spawn_group_id = sg.id) "
								"WHERE sg.spawn_id=%u ORDER BY l.id;", spawn);
}



void SpawnManager::handleDatabaseJobComplete(void* ref, DatabaseResult* result)
{
	SpawnAsyncContainer* asyncContainer = reinterpret_cast<SpawnAsyncContainer*>(ref);
	switch(asyncContainer->mQuery)
	{
		//lair data gets loaded and matched to the relevant spawns
		case SpawnQuery_Group:
		{
			LairData	data;

			DataBinding* lairBinding = mDatabase->CreateDataBinding(4);
			lairBinding->addField(DFT_uint32,offsetof(LairData,spawnGroupId),4,0);
			lairBinding->addField(DFT_uint32,offsetof(LairData,lairId),4,1);
			lairBinding->addField(DFT_uint32,offsetof(LairData,templateId),4,2);
			lairBinding->addField(DFT_uint32,offsetof(LairData,creatureGroupId),4,3);

			uint64 count = result->getRowCount();

			uint32 group = asyncContainer->spawnGroup;

			SpawnMap::iterator spawnIt = mSpawnListMap.find(group);
			if(spawnIt != mSpawnListMap.end())
			{
				for (uint64 i = 0;i < count;i++)
				{
					result->GetNextRow(lairBinding,&data);

					(*spawnIt).second.lairTypeList.push_back(data);

				
				}
				//now create the spawnregion
				SpawnRegion* spawnRegion = new(SpawnRegion);
				spawnRegion->mPosition.x = (*spawnIt).second.posX;
				spawnRegion->mPosition.z = (*spawnIt).second.posZ;
				spawnRegion->setWidth((float)(*spawnIt).second.width);
				spawnRegion->setHeight((float)(*spawnIt).second.height);

				spawnRegion->setId(gWorldManager->getRandomNpNpcIdSequence());

				spawnRegion->spawnData = &(*spawnIt).second;

				gWorldManager->addObject(spawnRegion);

			}
			else
			{
				assert(false && "spawnmanager::nospawngroup???");
			}

			
		}
		break;

		//these are the spawndatasets
		case SpawnQuery_Spawns:
		{
			SpawnDataStruct	data;

			DataBinding* spawnBinding = mDatabase->CreateDataBinding(6);
			spawnBinding->addField(DFT_uint32,offsetof(SpawnDataStruct,spawnId),4,0);
			spawnBinding->addField(DFT_float,offsetof(SpawnDataStruct,posX),4,1);
			spawnBinding->addField(DFT_float,offsetof(SpawnDataStruct,posZ),4,2);
			spawnBinding->addField(DFT_uint32,offsetof(SpawnDataStruct,width),4,3);
			spawnBinding->addField(DFT_uint32,offsetof(SpawnDataStruct,height),4,4);
			spawnBinding->addField(DFT_uint32,offsetof(SpawnDataStruct,density),4,5);

			uint64 count = result->getRowCount();

			for (uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(spawnBinding,&data);

				mSpawnListMap.insert(std::make_pair(data.spawnId,data));
				loadSpawnGroup(data.spawnId);

			}

			mDatabase->DestroyDataBinding(spawnBinding);

		}
		break;

		default:
		{
		}
		break;
	}
	delete(asyncContainer );
}


