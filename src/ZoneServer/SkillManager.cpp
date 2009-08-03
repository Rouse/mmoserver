/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "SkillManager.h"
#include "CreatureObject.h"
#include "MessageLib/MessageLib.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DataBinding.h"
#include "DatabaseManager/DatabaseResult.h"
#include "LogManager/LogManager.h"
#include "UIManager.h"


//======================================================================================================================

bool			SkillManager::mInsFlag = false;
SkillManager*	SkillManager::mSingleton = NULL;

//======================================================================================================================

SkillManager::SkillManager(Database* database) :
mDatabase(database),
mLoadCounter(0),
mTotalLoadCount(4),
mDBAsyncPool(sizeof(SMAsyncContainer))
{
	mSkillList.reserve(1100);
	mSkillModList.reserve(300);
	mSkillCommandList.reserve(1100);
	mXpTypeList.reserve(50);
	mSkillInfoList.reserve(1100);

	// load skillmods
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillMods),"SELECT * FROM skillmods ORDER BY skillmod_id");

	// load skillcommands
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillCommands),"SELECT * FROM skillcommands ORDER BY id");

	// load xp types
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_XpTypes),"SELECT * FROM xp_types ORDER BY id");

	// load skills
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_Skills),"SELECT * FROM skills ORDER BY skill_id");

	// load extended skill information (tex)
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillDescriptions),"SELECT * FROM skills_description ORDER BY skill_id");
}

//======================================================================================================================

SkillManager* SkillManager::Init(Database* database)
{
	if(mInsFlag == false)
	{
		mSingleton = new SkillManager(database);
		mInsFlag = true;
		return mSingleton;
	}
	else
		return mSingleton;
}

//======================================================================================================================

SkillManager::~SkillManager()
{
	SkillList::iterator it = mSkillList.begin();
	while(it != mSkillList.end())
	{
		delete(*it);
		it = mSkillList.erase(it);
	}

	mInsFlag = false;
	mSingleton = NULL;
}

//======================================================================================================================

void SkillManager::handleDatabaseJobComplete(void* ref,DatabaseResult* result)
{
	SMAsyncContainer* asyncContainer = reinterpret_cast<SMAsyncContainer*>(ref);

	switch(asyncContainer->mQueryType)
	{
		case SMQuery_SkillMods:
		{
			SMQueryContainer skillMod;
			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_string,offsetof(SMQueryContainer,mName),64,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&skillMod);
				mSkillModList.push_back(skillMod.mName);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillCommands:
		{
			SMQueryContainer skillCommand;
			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_string,offsetof(SMQueryContainer,mName),64,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&skillCommand);
				mSkillCommandList.push_back(skillCommand.mName);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_XpTypes:
		{
			// Build lists for xp_type default cap, xp_type string and the "reader friendly" version of the xp_type string.
			SMQueryContainer xpType;
			DataBinding* binding = mDatabase->CreateDataBinding(4);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);
			binding->addField(DFT_string,offsetof(SMQueryContainer,mName),64,2);
			binding->addField(DFT_string,offsetof(SMQueryContainer,mName2),64,3);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&xpType);
				mDefaultXpCapList.push_back(xpType.mInt2);
				mXpTypeList.push_back(xpType.mName);
				mXpTypeListEx.push_back(xpType.mName2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_Skills:
		{
			Skill* skill;
			DataBinding* binding = mDatabase->CreateDataBinding(13);
			binding->addField(DFT_uint32,offsetof(Skill,mId),4,0);
			binding->addField(DFT_bstring,offsetof(Skill,mName),64,1);
			binding->addField(DFT_uint8,offsetof(Skill,mGodOnly),1,2);
			binding->addField(DFT_uint8,offsetof(Skill,mIsTitle),1,3);
			binding->addField(DFT_uint8,offsetof(Skill,mIsProfession),1,4);
			binding->addField(DFT_int32,offsetof(Skill,mMoneyRequired),4,5);
			binding->addField(DFT_uint8,offsetof(Skill,mSkillPointsRequired),1,6);
			binding->addField(DFT_uint8,offsetof(Skill,mXpType),1,7);
			binding->addField(DFT_int32,offsetof(Skill,mXpCost),4,8);
			binding->addField(DFT_int32,offsetof(Skill,mXpCap),4,9);
			binding->addField(DFT_uint8,offsetof(Skill,mJediStateRequired),1,10);
			binding->addField(DFT_uint8,offsetof(Skill,mIsSearchable),1,11);
			binding->addField(DFT_int32,offsetof(Skill,mBadgeId),4,12);

			uint64 count = result->getRowCount();
			mTotalLoadCount += count * 6;
			
			gLogger->logMsgF("SkillManager::Loading %lld skills...",MSG_NORMAL,count);

			for(uint64 i = 0;i < count;i++)
			{
				skill = new Skill();
				result->GetNextRow(binding,skill);
				mSkillList.push_back(skill);

				if(skill->mBadgeId > 0
				||(strstr(skill->mName.getAnsi(),"discipline") && strstr(skill->mName.getAnsi(),"master")))
				{
					mMasterProfessionList.push_back(skill);
				}

				// query required species
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillSpecies),"SELECT * FROM skills_species_required WHERE skill_id=%u",skill->mId);

				// query skill preclusions
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillPreclusions),"SELECT * FROM skills_preclusions WHERE skill_id=%u",skill->mId);

				// query required skills
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillRequiredSkills),"SELECT * FROM skills_skill_skillsrequired WHERE skill_id=%u",skill->mId);

				// query skill commands
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillSkillCommands),"SELECT * FROM skills_skillcommands WHERE skill_id=%u",skill->mId);

				// query skill mods
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillSkillMods),"SELECT * FROM skills_skillmods WHERE skill_id=%u",skill->mId);

				// query skill schematic groups
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillSkillSchematicGroups),"SELECT * FROM skills_schematicsgranted WHERE skill_id=%u",skill->mId);

				// query skill xp types
				mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.ordered_malloc()) SMAsyncContainer(SMQuery_SkillSkillXpTypes),"SELECT * FROM skills_base_xp_groups WHERE skill_id=%u",skill->mId);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillSpecies:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSpeciesRequired.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillPreclusions:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSkillPrecusions.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillRequiredSkills:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSkillsRequired.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillSkillXpTypes:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSkillXpTypesList.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillSkillCommands:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mCommands.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillSkillSchematicGroups:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSchematics.push_back(iCont.mInt2);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillSkillMods:
		{
			SMQueryContainer iCont;

			DataBinding* binding = mDatabase->CreateDataBinding(3);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt),4,0);
			binding->addField(DFT_uint32,offsetof(SMQueryContainer,mInt2),4,1);
			binding->addField(DFT_int32,offsetof(SMQueryContainer,mInt3),4,2);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&iCont);
				mSkillList[iCont.mInt-1]->mSkillMods.push_back(std::make_pair(iCont.mInt2,iCont.mInt3));
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case SMQuery_SkillDescriptions:
		{
			struct SkillDescriptions
			{
				uint32 skillId;
				string skillInfo;
			};
						
			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(SkillDescriptions,skillId),4,0);
			binding->addField(DFT_bstring,offsetof(SkillDescriptions,skillInfo),512,1);

			uint64 rowCount = result->getRowCount();
			for (uint64 i = 0; i < rowCount; i++)
			{
				SkillDescriptions *skillDescription = new SkillDescriptions;
				result->GetNextRow(binding,skillDescription);

				// Store data...
				mSkillInfoList.push_back(std::make_pair(skillDescription->skillId, skillDescription->skillInfo));
			}
			mDatabase->DestroyDataBinding(binding);
		}
		break;

		default:break;
	}

	if(++mLoadCounter == mTotalLoadCount)
	{
		gLogger->logMsg("SkillManager::Load complete");
	}

	mDBAsyncPool.ordered_free(asyncContainer);
}

//======================================================================================================================
//
// TODO: figure deltas for updating
// using baselines for now
//

bool SkillManager::learnSkill(uint32 skillId,CreatureObject* creatureObject,bool subXp)
{
	Skill* skill = getSkillById(skillId);

	if (skill == NULL)
	{
		gLogger->logMsgF("SkillManager::learnSkill: could not find skill %u",MSG_NORMAL,skillId);
		return false;
	}

	if (creatureObject->checkSkill(skillId))
	{
		gLogger->logMsgF("SkillManager::learnSkill: %lld already got skill %u",MSG_NORMAL,creatureObject->getId(),skillId);
		return false;
	}

	//Check SkillPoints
	if (creatureObject->getType() == ObjType_Player)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(creatureObject);

		// I'm not sure IF we should bother checking the skill points here?
		if (player->getSkillPointsLeft() < skill->mSkillPointsRequired)
		{
			return false;
		}

		// Continue with basic stuff, like adding the skill
		creatureObject->addSkill(skill);
		
		//finding all the new schems for the skill!
		//player->addSchematicIds(skill);

		mDatabase->ExecuteSqlAsync(NULL,NULL,"INSERT INTO character_skills VALUES (%lld,%u)",player->getId(),skillId);

		creatureObject->prepareSkillMods();
		creatureObject->prepareSkillCommands();
		player->prepareSchematicIds();

		// Check if we already have the badge
		if((skill->mBadgeId > 0) && (!(player->checkBadges(skill->mBadgeId))))
		{
			player->addBadge(skill->mBadgeId);
		}

		// The order we do this is important for the client side messages.
		gMessageLib->sendSkillDeltasCreo1(skill,SMSkillAdd,player);
		
		//gMessageLib->sendSkillModUpdateCreo4(player);
		gMessageLib->sendBaselinesCREO_4(player);
		gMessageLib->sendSkillCmdDeltasPLAY_9(player);
		gMessageLib->sendSchematicDeltasPLAY_9(player);
		//gMessageLib->sendSchematicDeltasAddPLAY_9(player);

		gMessageLib->sendSystemMessage(player,L"","skill_teacher","prose_skill_learned","skl_n",skill->mName);

		// Update cap for this type of xp.
		int32 newXpCap = getXpCap(player, skill->mXpType);
		(void)player->UpdateXpCap(skill->mXpType, newXpCap);

		int32 xpCost = skill->mXpCost;
		if (!subXp)
		{
			// When training skillines...
			xpCost = 0;
		}

		// gLogger->logMsg("SkillManager::learnSkill: Trained a skill");

		// handle XP cap and system messages.
		int32 newXpCost = handleExperienceCap(skill->mXpType, -xpCost, player);
	
		// We don't wanna miss any "You now qualify for the skill: ..."
		(void)player->UpdateXp(skill->mXpType, newXpCost);

		// gLogger->logMsgF("SkillManager::learnSkill: Removing %i xp of type %u",MSG_NORMAL, -newXpCost, skill->mXpType);
		mDatabase->ExecuteSqlAsync(NULL,NULL,"UPDATE character_xp SET value=value+%i WHERE xp_id=%u AND character_id=%lld",newXpCost, skill->mXpType, player->getId());
		gMessageLib->sendXpUpdate(skill->mXpType,player);
	}
	else
	{
		creatureObject->addSkill(skill);	// Don't know if non-player have skillpoints to check before we learn skills?
		creatureObject->prepareSkillMods();
		creatureObject->prepareSkillCommands();
	}
	return true;
}

//************************************************************************************
//Checks whether its a teachable skill
//************************************************************************************
bool SkillManager::checkLearnSkill(uint32 skillId,PlayerObject* pupilObject)
{
	SkillsRequiredList::iterator reqSkillIt = getSkillById(skillId)->mSkillsRequired.begin();
	bool requirementsMet = true;

	while(reqSkillIt != getSkillById(skillId)->mSkillsRequired.end())
	{
		// we don't have the requirements
		if(!pupilObject->checkSkill(*reqSkillIt))
		{
			return false;
		}

		++reqSkillIt;
	}
	return true;
}

//************************************************************************************
//returns either the name of the profession if its not already on the skillist or the language string
//************************************************************************************
string SkillManager::getSkillProfession(uint32 skillId,string leaveAsIs)
{
	Skill* theSkill= getSkillById(skillId);
	string skillString = theSkill->mName.getAnsi();
	int8 str[128];

	//just return languages
	if(strstr(skillString.getAnsi(),"language"))
	{
		sprintf(str,"@skl_n:%s",theSkill->mName.getAnsi());
		return BString(str);
	}
	//if its the profession we selected to view just return the proper skill entry
	if(leaveAsIs.getLength()>1)
	{
		if(strstr(skillString.getAnsi(),leaveAsIs.getAnsi()))
		{
			sprintf(str,"@skl_n:%s",theSkill->mName.getAnsi());
			return BString(str);
		}
	}
	//otherwise just return the profession

	BStringVector		splitSkill;
	if(theSkill->mName.split(splitSkill,'_') >= 2)
	{
		sprintf(str,"@skl_n:%s_%s",splitSkill[0].getAnsi(),splitSkill[1].getAnsi());

		return BString(str);
	}
	return BString("");
}

//======================================================================================================================

bool SkillManager::learnSkillLine(uint32 skillId, CreatureObject* creatureObject, bool subXP)
{
	//Get list of pre-req skills
	SkillsRequiredList::iterator reqSkillIt = getSkillById(skillId)->mSkillsRequired.begin();

	//iterate through list of pre-requisites
	while(reqSkillIt != getSkillById(skillId)->mSkillsRequired.end())
	{
		// if we don't have the requirements
		if(!creatureObject->checkSkill(*reqSkillIt))
		{
			//train the skill by recursively learning skill line
			if(!learnSkillLine(*reqSkillIt, creatureObject, subXP))
			{
				return false;
			}
		}		
		++reqSkillIt;
	}

	//if we have all pre-reqs
	//learn skill
	return learnSkill(skillId, creatureObject, subXP);
}

//======================================================================================================================

void SkillManager::teach(PlayerObject* pupilObject,PlayerObject* teacherObject,string show)
{
	// pupil and teacher bozh exist and are grouped
	// we will now compare the teachers skill list to the pupils skill list 
	// and assemble a list with the skills the pupil does not have but were she/he has the prerequesits

	BStringVector availableSkills;
	SkillList*	teacherSkills = teacherObject->getSkills();
	SkillList::iterator teacherIt = teacherSkills->begin();
	BStringVector::iterator bStringIt = availableSkills.begin();

	//SkillTeachContainer* teachContainer = new SkillTeachContainer();
	
	uint32 nr = 0;
	while(teacherIt != teacherSkills->end())
	{
		//does the pupil have this skill?
		if (!pupilObject->checkSkill((*teacherIt)->mId))
		{
			//since the pupil doesnt have it ... are the requirements met?
			if(checkLearnSkill((*teacherIt)->mId,pupilObject))
			{
				//is it teachable???
				if (checkTeachSkill((*teacherIt)->mId,pupilObject))
				{
					string str;
			
					//now get the corresponding profession
					//languages or the profession provided are just returned as is
					str = getSkillProfession((*teacherIt)->mId,show);
					
					//now check whether we have double entries
					bStringIt = availableSkills.begin();
					bool found = false;
					while (bStringIt != availableSkills.end())
					{
						if(bStringIt->getCrc()==str.getCrc())
							found = true;
						bStringIt++;
					}
					if(!found)
					{
						availableSkills.push_back(str.getAnsi());
						nr++;
					}
				}
			}
		}
		teacherIt++;
	}

	if(nr > 0)
	{
		gUIManager->createNewSkillSelectListBox(teacherObject,"handleSkillteach","select skill","Select a skill to teach",availableSkills,teacherObject,SUI_LB_OK,pupilObject);
	}
	else
	{
		gMessageLib->sendSystemMessage(teacherObject,L"","teaching","no_skills");

		pupilObject->getTrade()->setTeacher(NULL);
	}
}

//************************************************************************************
//Checks whether its a teachable skill
//************************************************************************************
bool SkillManager::checkTeachSkill(uint32 skillId,PlayerObject* pupilObject)
{
	Skill* theSkill= getSkillById(skillId);
	string skillString = theSkill->mName.getAnsi();
	
	//make sure its no novice profession			
	if(strstr(skillString.getAnsi(),"novice"))
	{
		return false;
	}

	//1) dont add the basic skill nodes for languages - only speak and comprehend
	if(strstr(skillString.getAnsi(),"language"))
	{
		//is it the language  base_node???
		if((!strstr(skillString.getAnsi(),"comprehend"))^!(!strstr(skillString.getAnsi(),"speak")))
		{
			return false;
		}

		//still have to make sure not to teach shyriiwook and  lekku
		if (!checkRaceLearnSkill(skillId,pupilObject))
		{
			return false;
		}
		
		return true;
	}

	//1) dont add the basic skill nodes for other skills - like basic_species_human
	if(strstr(skillString.getAnsi(),"species"))
	{
		return false;
	}
	
	if ((pupilObject->getXpAmount(theSkill->mXpType) < theSkill->mXpCost))
	{
		return false;
	}
	
	return true;

}

//************************************************************************************
//Checks the race prerequisits to learn a language
//************************************************************************************
bool SkillManager::checkRaceLearnSkill(uint32 skillId,CreatureObject* creatureObject)
{
	//check if Shyriiwook speak 655
	//check if lekku speak -> 667
	//check if lekku understand -> 668
	switch (skillId){
		case 655:{
			//wookiee = 5
			if (creatureObject->getRaceId() == 4){
				return true;
			}else
				return false;
		}
		break;
		 
		case 667:{
			if (creatureObject->getRaceId() == 6)
			{
				return true;
			}
			else
				return false;
		}
		break;

		case 668:{
			if (creatureObject->getRaceId() == 6){
				return true;
			}else
				return false;
		}
		break;

	}

	return(false);

}

//======================================================================================================================
// TODO: figure creo4 deltas for updating
// using baselines for now
void SkillManager::dropSkill(uint32 skillId,CreatureObject* creatureObject)
{
	Skill* skill = getSkillById(skillId);

	if(skill == NULL)
	{
		gLogger->logMsgF("SkillManager::dropSkill: could not find skill %u",MSG_NORMAL,skillId);
		return;
	}

	if(!(creatureObject->checkSkill(skillId)))
	{
		gLogger->logMsgF("SkillManager::dropSkill: %lld hasn't got skill %u",MSG_NORMAL,creatureObject->getId(),skillId);
		return;
	}

	if(!(creatureObject->removeSkill(skill)))
		gLogger->logMsgF("SkillManager::dropSkill: failed removing %u from %lld",MSG_NORMAL,skillId,creatureObject->getId());

	creatureObject->prepareSkillMods();
	creatureObject->prepareSkillCommands();

	if(creatureObject->getType() == ObjType_Player)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(creatureObject);

		player->prepareSchematicIds();

		mDatabase->ExecuteSqlAsync(NULL,NULL,"DELETE FROM character_skills WHERE character_id=%lld AND skill_id=%u",player->getId(),skillId);

		gMessageLib->sendSkillDeltasCreo1(skill,SMSkillRemove,player);

		gMessageLib->sendBaselinesCREO_4(player);
		gMessageLib->sendSkillCmdDeltasPLAY_9(player);
		gMessageLib->sendSchematicDeltasPLAY_9(player);
		gMessageLib->sendSystemMessage(player,L"Skill surrendered.");

		// Update the cap for this type of xp, but do NOT adjust the xp down below cap.
		int32 newXpCap = getXpCap(player, skill->mXpType);
		(void)player->UpdateXpCap(skill->mXpType, newXpCap);


		//gMessageLib->sendSkillDeltasCreo4(player);
	}
}

//======================================================================================================================

Skill* SkillManager::getSkillByName(string skillName)
{
	SkillList::iterator it = mSkillList.begin();

	while(it != mSkillList.end())
	{
		if(strcmp((*it)->mName.getAnsi(),skillName.getAnsi()) == 0)
			return(*it);
		++it;
	}

	return(NULL);
}

//======================================================================================================================
string SkillManager::getSkillInfoById(uint32 skillId)
{
	static string empty("");
	SkillInfoList::iterator it = mSkillInfoList.begin();	// find(skillId);
	while (it != mSkillInfoList.end())
	{
		if ((*it).first == skillId)
		{
			return((*it).second);
		}
		it++;
	}
	return empty;
}

//======================================================================================================================

int32 SkillManager::getXpCap(PlayerObject* playerObject, uint8 xpType)
{
	int32 cap = getDefaultXPCapById(xpType);

	// Get current cap value for this xp type.
	SkillList::iterator skillIt = playerObject->getSkills()->begin();
	while (skillIt != playerObject->getSkills()->end())
	{
		if ((*skillIt)->mXpType == xpType)
		{
			if ((*skillIt)->mXpCap > cap)
			{
				cap = (*skillIt)->mXpCap;
			}
		}
		++skillIt;
	}
	return cap;
}

//======================================================================================================================
int32 SkillManager::getMaxXpCap(uint8 xpType)
{
	int32 cap = getDefaultXPCapById(xpType);

	SkillList::iterator skillIt = mSkillList.begin();	// Iterate all skills.
	while (skillIt != mSkillList.end())
	{
		Skill* skill = (*skillIt);
		if (skill->mXpType == xpType)
		{
			// Get the cap for it.
			if (skill->mXpCap > cap)
			{
				cap = skill->mXpCap;
			}
		}
		++skillIt;
	}
	return cap;
}

//======================================================================================================================

void SkillManager::initExperience(PlayerObject* playerObject)
{
	for (int xpType = 1; xpType < 49; xpType++)
	{
		if (!playerObject->checkXpType(xpType))
		{
			// gLogger->logMsgF("SkillManager::initExperience: Did not have xpType %u",MSG_NORMAL, xpType);
			// Check if xpType is valid, in regards to JTL, Jedi and Pre-Pub14.
			if (!playerObject->restrictedXpType(xpType))
			{
				// gLogger->logMsgF("SkillManager::initExperience: Updating xpType %u",MSG_NORMAL, xpType);
				
				// Add this type of xp.
				playerObject->addXpType(xpType);

				// Create entry in DB.
				mDatabase->ExecuteSqlAsync(NULL,NULL,"INSERT INTO character_xp VALUES (%lld,%u,0)",playerObject->getId(),xpType);

				// Add this type of xp cap.
				int32 newXpCap = getXpCap(playerObject, xpType);
				playerObject->addXpCapType(xpType, newXpCap);
			}
		}
		else
		{
			// We did have this type already, update the cap.

			// Add this type of xp cap.
			int32 newXpCap = getXpCap(playerObject, xpType);
			playerObject->addXpCapType(xpType, newXpCap);
			// gLogger->logMsgF("SkillManager::addExperience: New Cap for %u = %u",MSG_NORMAL, xpType, newXpCap);
		}
	}
}

//======================================================================================================================

int32 SkillManager::handleExperienceCap(uint32 xpType,int32 valueDiff, PlayerObject* playerObject)
{
	int32 delta = 0;
	
	int32 xpAmount = playerObject->getXpAmount(xpType);
	int32 xpCap = playerObject->getXpCapAmount(xpType);
		
	if (valueDiff > 0)
	{
		// gLogger->logMsgF("SkillManager::handleExperienceCap: Request Add of %d xp, amount = %d, Cap = %u", MSG_NORMAL, valueDiff, xpAmount, xpCap);
		if (xpAmount + valueDiff >= xpCap)
		{
			// We are or will become capped at xp.
			if (xpAmount == xpCap)
			{
				// We already have been through the process of advertisingat cap etc....
				// gLogger->logMsgF("SkillManager::handleExperienceCap: At Cap, pxp amount %u = Cap %u",MSG_NORMAL, xpAmount, xpCap);
			}
			else if (xpAmount > xpCap)
			{
				// gLogger->logMsgF("SkillManager::handleExperienceCap: xp amount %u > Cap %u",MSG_NORMAL, xpAmount, xpCap);
				// We where already capped and above, and will lose xp.
				delta = xpCap - xpAmount;	// add negative number to subtract overflow of xp so we reach xp cap.
				if (delta == -1)
				{
					// You lose 1 point of %TO experience
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_revoke_xp1","exp_n",getXPTypeById(xpType),L"",-delta);
				}
				else
				{
					// You lose %DI points of %TO experience. 
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_revoke_xp","exp_n",getXPTypeById(xpType),L"",-delta);
				}

				// You have achieved your current limit for %TO experience. 
				gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_hit_xp_cap","exp_n",getXPTypeById(xpType),L"",0);
				
				// gLogger->logMsgF("SkillManager::handleExperienceCap: Sub %u XP",MSG_NORMAL, -delta);
			}
			else // (xpAmount < xpCap)
			{
				// We become capped now.
				// gLogger->logMsgF("SkillManager::handleExperienceCap: We become capped now. xp amount %u, Cap %u",MSG_NORMAL, xpAmount, xpCap);

				delta = xpCap - xpAmount;	// Add a positive number to reach the cap level.
				if (delta == 1)
				{
					// You receive 1 point of %TO experience. 
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_grant_xp1","exp_n",getXPTypeById(xpType),L"",delta);
				}
				else
				{
					// You receive %DI points of %TO experience. 
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_grant_xp","exp_n",getXPTypeById(xpType),L"",delta);
				}
				// Do we have the max cap?
				if (xpCap == getMaxXpCap(xpType))
				{
					// Yes.
					// You have achieved your limit of %DIpts for experience type '%TO'. 
					gMessageLib->sendSystemMessage(playerObject,L"","error_message","prose_hit_xp_limit","exp_n",getXPTypeById(xpType),L"",xpCap);
				}
				else
				{
					// You have achieved your current limit for %TO experience. 
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_hit_xp_cap","exp_n",getXPTypeById(xpType));
				}
				// gLogger->logMsgF("SkillManager::handleExperienceCap: Adding %u XP",MSG_NORMAL, delta);
			}
		}
		else
		{
			// We will not be capped after this xp addition.
			// gLogger->logMsgF("SkillManager::handleExperienceCap: Adding all xp %u",MSG_NORMAL, valueDiff);
			delta = valueDiff;	// Return amount of xp to add.
			if (delta == 1)
			{
				// You receive 1 point of %TO experience. 
				gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_grant_xp1","exp_n",getXPTypeById(xpType),L"",delta);
			}
			else
			{
				// You receive %DI points of %TO experience. 
				gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_grant_xp","exp_n",getXPTypeById(xpType),L"",delta);
			}
		}
	}
	else if (valueDiff < 0)
	{
		// gLogger->logMsgF("SkillManager::handleExperienceCap: Request Sub of %d xp, amount = %d, Cap = %u", MSG_NORMAL, -valueDiff, xpAmount, xpCap);

		delta = valueDiff;	// Amount of xp to sub.
		if (xpAmount >= xpCap)	// We where already capped.
		{
			// gLogger->logMsgF("SkillManager::handleExperienceCap: We where already capped. xp amount %d, Cap %u",MSG_NORMAL, xpAmount, xpCap);
			if (xpAmount + valueDiff > xpCap)	// Reduce xp?
			{
				// Still capped after reducution of XP. Inform user about theXP loss.
				// This is the value we show the client (Only the reduction due to the cap overflow).
				delta = xpCap - (xpAmount + valueDiff);
				// gLogger->logMsgF("SkillManager::handleExperienceCap: Show XP reduction of %d, Cap %u",MSG_NORMAL, -delta);
				if (delta == -1)
				{
					// You lose 1 point of %TO experience
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_revoke_xp1","exp_n",getXPTypeById(xpType),L"",-delta);
				}
				else
				{
					// You lose %DI points of %TO experience. 
					gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_revoke_xp","exp_n",getXPTypeById(xpType),L"",-delta);
				}

				// You have achieved your current limit for %TO experience. 
				gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_hit_xp_cap","exp_n",getXPTypeById(xpType),L"",0);
				
				// This is the value we should sub from DB (Everything down to the cap level.
				delta = xpCap - xpAmount;
				// gLogger->logMsgF("SkillManager::handleExperienceCap: Reduce XP with %d",MSG_NORMAL, -delta);
			}
			else if (xpAmount + valueDiff == xpCap)
			{
				// We landed at the cap level.
				// You have achieved your current limit for %TO experience. 
				gMessageLib->sendSystemMessage(playerObject,L"","base_player","prose_hit_xp_cap","exp_n",getXPTypeById(xpType),L"",0);
				// gLogger->logMsgF("SkillManager::handleExperienceCap: At XP Cap limit, reduce XP with %d",MSG_NORMAL, -delta);
			}
			else
			{
				// We will not be capped after this xp reduction.
				// gLogger->logMsgF("SkillManager::handleExperienceCap: Subtracted all the XP %d",MSG_NORMAL, -delta);
			}
		}
		else
		{
			// We will not be capped after this xp reduction.
			// gLogger->logMsgF("SkillManager::handleExperienceCap: Subtracted all the XP %d",MSG_NORMAL, -delta);
		}
	}
	return delta;
}

//======================================================================================================================

void SkillManager::addExperience(uint32 xpType,int32 valueDiff,PlayerObject* playerObject)
{
	if (valueDiff > 0)	// Negative xp not accepted.
	{
		// Handle XP cap and system messages.
		// Note: newXpBoost can be negative if we where above cap.
		int32 newXpBoost = handleExperienceCap(xpType, valueDiff, playerObject);

		if (!(playerObject->UpdateXp(xpType, newXpBoost)))
		{
			gLogger->logMsgF("SkillManager::addExperience: could not find xptype %u for %lld",MSG_NORMAL,xpType,playerObject->getId());
			return;
		}
		// gLogger->logMsgF("SkillManager::addExperience: XP cap = %u",MSG_NORMAL, xpCap);
		// gLogger->logMsgF("SkillManager::addExperience: Adding %u xp of type %u to database",MSG_NORMAL, newXpBoost, xpType);
		
		mDatabase->ExecuteSqlAsync(NULL,NULL,"UPDATE character_xp SET value=value+%i WHERE character_id=%lld AND xp_id=%u", newXpBoost, playerObject->getId(), xpType);

		// ...THEN we get any messages of new skills qualifications.
		gMessageLib->sendXpUpdate(xpType,playerObject);
	}
}

//======================================================================================================================

/*
// This will be needed when we have to drop XP, like trading FS XP in the Village, so don't delete...
void SkillManager::removeExperience(uint32 xpType,int32 valueDiff,PlayerObject* playerObject)
{
	if(!(playerObject->UpdateXp(xpType,-valueDiff)))
	{
		gLogger->logMsgF("SkillManager::gainXp: could not find xptype %u for %lld",MSG_NORMAL,xpType,playerObject->getId());
		return;
	}

	gLogger->logMsgF("SkillManager::removeExperience: Removing %i xp of type %u",MSG_NORMAL, -valueDiff, xpType);
	mDatabase->ExecuteSqlAsync(NULL,NULL,"UPDATE character_xp SET value=value-%i WHERE character_id=%lld AND xp_id=%u",valueDiff,playerObject->getId(),xpType);

	gMessageLib->sendXpUpdate(xpType,playerObject);
}
*/
//======================================================================================================================

