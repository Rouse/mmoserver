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

#ifndef ANH_ZONESERVER_SPAWN_MANAGER_H
#define ANH_ZONESERVER_SPAWN_MANAGER_H


#include "DatabaseManager/DatabaseCallback.h"
#include "Utils/typedefs.h"
#include "ObjectFactoryCallback.h"
#include <map>
#include <list>

//=============================================================================

class AttackableCreature;
class CreatureObject;
class Database;
class NPCObject;
class Weapon;
class SpawnData;

struct SpawnDataStruct;
struct LairData;

// please note that all spawns / lairs creatures are non persistent objects
// so any id we will give them comes from the non persitent object pool
// the db ids are just datacollection  references

typedef std::list<LairData>		SpawnLairList;

struct SpawnDataStruct
{
	uint32			spawnId;
	float			posX;
	float			posZ;
	uint32			width;
	uint32			height;
	uint32			planet;
	uint32			density;
	SpawnLairList	lairTypeList;
	
};

struct LairData
{
	uint32	lairId; 
	uint32	spawnGroupId;
	uint32	templateId;
	uint32	creatureGroupId;
	string	lairObjectString;
	string	stfName;
	string	stfFile;
	string	stfDetailName;
	string	stfDetailFile;
	float	posZ;
	uint32	width;
	uint32	height;
	uint32	planet;
	uint32	density;
};

typedef std::map<uint32,SpawnDataStruct>	SpawnMap;

//======================================================================================================================
//
// Container for asyncronous database queries
//
enum SpawnQuery
{
	SpawnQuery_Lairs				= 0,
	SpawnQuery_Spawns				= 1,
	SpawnQuery_Group				= 2
};

class SpawnAsyncContainer
{
	public:
		SpawnAsyncContainer(SpawnQuery query){ mQuery = query;}
		SpawnQuery mQuery;

		uint32	spawnGroup;
};


//=============================================================================


class SpawnManager  : public ObjectFactoryCallback, public DatabaseCallback
{
	public:

		static SpawnManager* Instance(void);

		static inline void deleteManager(void)
		{
			if (mInstance)
			{
				delete mInstance;
				mInstance = 0;
			}
		}

		// Inherited  interface that we have to provide.
		virtual void	handleObjectReady(Object* object);
		virtual void    handleDatabaseJobComplete(void* ref, DatabaseResult* result);

		// void	addCreature(uint64 creatureId, const SpawnData *spawn);
		//void	handleExpiredCreature(uint64 creatureId);
		// void	removeNpc(uint64 npcId);
		//bool	handleAttack(CreatureObject *attacker, uint64 targetId);

		//uint64	handleNpc(NPCObject* npc, uint64 timeOverdue);

		void	loadSpawns(void);
		void	loadSpawnGroup(uint32 spawn);


	protected:
		SpawnManager(Database* database);
		~SpawnManager();

	private:
		// This constructor prevents the default constructor to be used, since it is private.
		SpawnManager();

		// Simulated Combat Manager
		// void			_setupDatabindings();
		// void			_destroyDatabindings();

		static SpawnManager* mInstance;
		Database* mDatabase;

		SpawnMap		mSpawnListMap;
		// DataBinding*	mItemIdentifierBinding;
		// DataBinding*	mItemBinding;
};

//=============================================================================



#endif

