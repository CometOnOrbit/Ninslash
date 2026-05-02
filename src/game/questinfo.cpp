#include "questinfo.h"


const char *GetQuestDisplayName(int Quest)
{
	switch (Quest)
	{
		case QUEST_KILLREMAININGENEMIES: return "Terminate the enemies";
		case QUEST_REACHDOOR: return "Reach the door";
		case QUEST_SURVIVEWAVE: return "Survive the wave of enemies";
		case QUEST_SURVIVEWAVETIME: return "Survive the wave of enemies";
		case QUEST_PREP_SHIELDGENERATOR: return "Construction: shield generator";
		case QUEST_DEFEND_REACTOR: return "Defend the reactor";
		case QUEST_DEFEND_SHIELDGENERATOR: return "Protect the shield generator";
		case QUEST_TRIGGERSWITCH_RISE: return "Trip the purge switch";
		default: return "";
	}
}

const char *GetQuestStartMessage(int Quest, int WaveType)
{
	if (Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Wave of aliens incoming";
			case WAVE_ROBOTS: return "Wave of robots incoming";
			case WAVE_SKELETONS: return "Wave of skeletons incoming";
			case WAVE_FURRIES: return "Wave of furries incoming";
			case WAVE_CYBORGS: return "Wave of cyborgs incoming";
			default: return "Wave incoming";
		}
	}

	if (Quest == QUEST_DEFEND_REACTOR || Quest == QUEST_DEFEND_SHIELDGENERATOR)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Incoming aliens — defend the objective";
			case WAVE_ROBOTS: return "Incoming robots — defend the objective";
			case WAVE_SKELETONS: return "Incoming skeletons — defend the objective";
			case WAVE_FURRIES: return "Incoming hostiles — defend the objective";
			case WAVE_CYBORGS: return "Incoming cyborgs — defend the objective";
			default: return "Incoming hostiles — defend the objective";
		}
	}

	if (Quest == QUEST_TRIGGERSWITCH_RISE)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Fight through aliens — flip the purge switch";
			case WAVE_ROBOTS: return "Fight through robots — flip the purge switch";
			case WAVE_SKELETONS: return "Fight through skeletons — flip the purge switch";
			case WAVE_FURRIES: return "Fight through enemies — flip the purge switch";
			case WAVE_CYBORGS: return "Fight through cyborgs — flip the purge switch";
			default: return "Reach the purge switch";
		}
	}
	
	if (Quest == QUEST_KILLREMAININGENEMIES)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Terminate the aliens";
			case WAVE_ROBOTS: return "Terminate the robots";
			case WAVE_SKELETONS: return "Terminate the skeletons";
			case WAVE_FURRIES: return "Terminate the furries";
			case WAVE_CYBORGS: return "Terminate the cyborgs";
			default: return "Terminate the enemies";
		}
	}
	
	switch (Quest)
	{
		case QUEST_KILLREMAININGENEMIES: return "Terminate the enemies";
		case QUEST_REACHDOOR: return "Seek the door";
		case QUEST_SURVIVEWAVE: return "Wave incoming";
		case QUEST_SURVIVEWAVETIME: return "Wave incoming";
		case QUEST_PREP_SHIELDGENERATOR: return "Deploy a shield generator before the siege";
		case QUEST_DEFEND_REACTOR: return "Keep the reactor online";
		case QUEST_DEFEND_SHIELDGENERATOR: return "Shield generator must survive";
		case QUEST_TRIGGERSWITCH_RISE: return "Hit the purge switch — acid will rise";
		default: return "";
	}
}

const char *GetQuestCompletedMessage(int Quest, int WaveType)
{
	if (Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Alien wave cleared";
			case WAVE_ROBOTS: return "Robot wave cleared";
			case WAVE_FURRIES: return "Furry wave cleared";
			case WAVE_SKELETONS: return "Skeleton wave cleared";
			case WAVE_CYBORGS: return "Cyborg wave cleared";
			default: return "Wave cleared";
		}
	}

	if (Quest == QUEST_DEFEND_REACTOR || Quest == QUEST_DEFEND_SHIELDGENERATOR)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Alien siege broken — objective safe";
			case WAVE_ROBOTS: return "Robot siege broken — objective safe";
			case WAVE_FURRIES: return "Raid broken — shields safe";
			case WAVE_SKELETONS: return "Skeleton siege broken — objective safe";
			case WAVE_CYBORGS: return "Cyborg siege broken — objective safe";
			default: return "Raid broken — objective safe";
		}
	}
	
	if (Quest == QUEST_KILLREMAININGENEMIES)
	{
		switch (WaveType)
		{
			case WAVE_ALIENS: return "Aliens terminated";
			case WAVE_ROBOTS: return "Robots terminated";
			case WAVE_SKELETONS: return "Skeletons terminated";
			case WAVE_FURRIES: return "Furries terminated";
			case WAVE_CYBORGS: return "Cyborgs terminated";
			default: return "Enemies terminated";
		}
	}
	
	switch (Quest)
	{
		case QUEST_REACHDOOR: return "";
		case QUEST_PREP_SHIELDGENERATOR: return "Shield generator online";
		case QUEST_DEFEND_REACTOR: return "Reactor secure";
		case QUEST_DEFEND_SHIELDGENERATOR: return "Generators holding";
		case QUEST_TRIGGERSWITCH_RISE: return "Purge valves open";
		default: return "";
	}
}