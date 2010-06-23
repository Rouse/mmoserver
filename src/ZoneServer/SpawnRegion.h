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

#ifndef ANH_ZONESERVER_SPAWNREGION_H
#define ANH_ZONESERVER_SPAWNREGION_H

#include "RegionObject.h"
#include "SpawnManager.h"
#include "MathLib/Rectangle.h"
#include "Utils/typedefs.h"

//=============================================================================

class ZoneTree;
class LairObject;
class PlayerObject;
class QTRegion;

typedef std::list<LairObject*>				LairObjectList;

//=============================================================================

enum ESpawnStatus
{
	ESpawnStatus_Unspawned		= 1,
	ESpawnStatus_Spawned		= 2,
	ESpawnStatus_AwaitDespawn	= 3

};

class SpawnRegion : public RegionObject
{
	friend class SpawnRegionFactory;
	friend class MissionSpawnRegionFactory;

	public:

		SpawnRegion();
		virtual ~SpawnRegion();

		uint32				getSpawnType(){ return mSpawnType; }
		void				setSpawnType(uint32 type){ mSpawnType = type; }
		bool				isMission(){return (mMission != 0);}

		void				addLair(LairObject* lair){mLairObjectList.push_back(lair);};

		virtual void		update();
		virtual void		onObjectEnter(Object* object);
		virtual void		onObjectLeave(Object* object);

		void				spawnArea();
		void				despawnArea();
		glm::vec3			getSpawnLocation();
		bool				checkSpawnLocation(glm::vec3 location);
		bool				checkSpawnDensity(glm::vec3 location);
		void				setSpawnData(SpawnDataStruct*	spawnData);
		uint32				getSpawnDensity(){return mDensity;}

		
	protected:

		Anh_Math::Rectangle mQueryRect;
		QTRegion*			mQTRegion;
		ZoneTree*			mSI;
		uint32				mMission;
		uint32				mSpawnType;

		uint32				mDensity;

		uint64				mInactivityTimer;

		LairObjectList		mLairObjectList;
		SpawnDataStruct*	mSpawnData;

		ESpawnStatus		mSpawnStatus;
};


#endif



