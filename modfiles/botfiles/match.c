//===========================================================================
//
// Name:			match.c
// Function:		match templates
// Programmer:		Mr Elusive
// Last update:
// Tab Size:		4 (real tabs)
// Notes:			currently maximum of 8 match variables
//===========================================================================

#include "match.h"

// this is rare but people can always fuckup
// don't use EC"(", EC")", EC"[", EC"]" or EC":" inside player names
// don't use EC": " inside map locations

//entered the game message
MTCONTEXT_MISC
{
	//enter game message
	NETNAME, " entered the game" = (MSG_ENTERGAME, 0);
	NETNAME, " is the new team leader" = (MSG_NEWLEADER, 0);
} //end MTCONTEXT_ENTERGAME

//initial team command chat messages
MTCONTEXT_INITIALTEAMCHAT
{
	//help someone (and meet at the rendezvous point)
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": help "|" meet ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_HELP, ST_NEARITEM);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": help "|" meet ", TEAMMATE = (MSG_HELP, ST_SOMEWHERE);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " help "|" meet ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_HELP, $evalint(ST_NEARITEM|ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " help "|" meet ", TEAMMATE = (MSG_HELP, $evalint(ST_SOMEWHERE|ST_ADDRESSED));

	//accompany someone (and meet at the rendezvous point) ("hunk follow me", "hunk go with babe", etc.)
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "accompany "|"go with "|"follow "|"cover "|" protect ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM, " for", TIME = (MSG_ACCOMPANY, $evalint(ST_NEARITEM|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "accompany "|"go with "|"follow "|"cover "|" protect ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_ACCOMPANY, ST_NEARITEM);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "accompany "|"go with "|"follow "|"cover "|" protect ", TEAMMATE, " for", TIME = (MSG_ACCOMPANY, $evalint(ST_SOMEWHERE|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "accompany "|"go with "|"follow "|"cover "|" protect ", TEAMMATE = (MSG_ACCOMPANY, ST_SOMEWHERE);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " accompany "|" go with "|" follow "|" cover "|" protect ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM, " for", TIME = (MSG_ACCOMPANY, $evalint(ST_NEARITEM|ST_ADDRESSED|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " accompany "|" go with "|" follow "|" cover "|" protect ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_ACCOMPANY, $evalint(ST_NEARITEM|ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " accompany "|" go with "|" follow "|" cover "|" protect ", TEAMMATE, " for", TIME = (MSG_ACCOMPANY, $evalint(ST_SOMEWHERE|ST_ADDRESSED|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " accompany "|" go with "|" follow "|" cover "|" protect ", TEAMMATE = (MSG_ACCOMPANY, $evalint(ST_SOMEWHERE|ST_ADDRESSED));

	//teamplay task preference
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to defend" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to defend the ", "Red Flag"|"Blue Flag"|"Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to be ", "on "|"", "defense" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to capture the ", "Red Flag"|"Blue Flag" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to get the ", "Red Flag"|"Blue Flag" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to ", "attack"|"assault" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to ", "attack"|"assault", " the ", "Red Flag"|"Blue Flag"|"Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to harvest", " skulls"|" cubes"|"" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to be ", "on "|"", "offense" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will defend" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will defend", " the ", "Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will be ", "on "|"", "defense" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will not harvest", " skulls"|" cubes"|"" = (MSG_TASKPREFERENCE, ST_DEFENDER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I am ", "on "|"", "defense" = (MSG_TASKPREFERENCE, ST_DEFENDER);

	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to defend" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to defend the ", "Red Flag"|"Blue Flag"|"Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to be ", "on "|"", "defense" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to capture the ", "Red Flag"|"Blue Flag" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to get the ", "Red Flag"|"Blue Flag" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to ", "attack"|"assault" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to ", "attack"|"assault", " the ", "Red Flag"|"Blue Flag"|"Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to harvest", " skulls"|" cubes"|"" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to be ", "on "|"", "offense" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will ","attack"|"assault" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will ","attack"|"assault", " the ", "Red Obelisk"|"Blue Obelisk" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will be ", "on "|"", "offense" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will harvest", " skulls"|" cubes"|"" = (MSG_TASKPREFERENCE, ST_ATTACKER);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I am ", "on "|"", "offense" = (MSG_TASKPREFERENCE, ST_ATTACKER);

	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I want to roam" = (MSG_TASKPREFERENCE, ST_ROAMER);

	//get the flag in CTF
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": get ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": go get ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": capture ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": go capture ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go get ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " get ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go capture ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " capture ", "the "|"", "blue "|"red "|"enemy "|"", "flag" = (MSG_GETFLAG, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": kill the flag carrier" = (MSG_GETFLAG, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " kill the flag carrier" = (MSG_GETFLAG, ST_ADDRESSED);

	//attack the enemy base
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " attack "|" assault ", "the "|"", "enemy "|"red "|"blue "|"", "base"|"flag"|"obelisk" = (MSG_ATTACKENEMYBASE, ST_ADDRESSED);

	//go harvesting
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " harvest" = (MSG_HARVEST, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go harvesting" = (MSG_HARVEST, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " collect skulls" = (MSG_HARVEST, ST_ADDRESSED);

	//kill someone (NOTE: make sure these are after the get flag match templates because of the "kill"
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": kill ", ENEMY = (MSG_KILL, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " kill ", ENEMY = (MSG_KILL, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": hunt down ", ENEMY = (MSG_KILL, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " hunt down ", ENEMY = (MSG_KILL, ST_ADDRESSED);

	//get item
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": get ", "the "|"", ITEM = (MSG_GETITEM, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": go get ", "the "|"", ITEM = (MSG_GETITEM, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " get ", "the "|"", ITEM = (MSG_GETITEM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go get ", "the "|"", ITEM = (MSG_GETITEM, ST_ADDRESSED);

	//defend/guard a key area
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "defend "|"guard ", "the "|"checkpoint "|"waypoint "|"", KEYAREA, " for", TIME = (MSG_DEFENDKEYAREA, ST_TIME);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "defend "|"guard ", "the "|"checkpoint "|"waypoint "|"", KEYAREA = (MSG_DEFENDKEYAREA, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " defend "|" guard ", "the "|"checkpoint "|"waypoint "|"", KEYAREA, " for", TIME = (MSG_DEFENDKEYAREA, $evalint(ST_ADDRESSED|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " defend "|" guard ", "the "|"checkpoint "|"waypoint "|"", KEYAREA = (MSG_DEFENDKEYAREA, ST_ADDRESSED);

	//camp somewhere ("hunk camp here", "hunk camp there", "hunk camp near the rl", etc.)
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "there "|"over there ", " for", TIME = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_TIME|ST_THERE));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "there"|"over there" = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_THERE));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "here"|"over here ", " for", TIME = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_TIME|ST_HERE));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "here"|"over here" = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_HERE));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "near "|"at "|"", "the "|"checkpoint "|"waypoint "|"", KEYAREA, " for", TIME = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_NEARITEM|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " camp ", "near "|"at "|"", "the "|"checkpoint "|"waypoint "|"", KEYAREA = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_NEARITEM));
	//go to (same as camp)
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go to ", "the "|"checkpoint "|"waypoint "|"", KEYAREA = (MSG_CAMP, $evalint(ST_ADDRESSED|ST_NEARITEM));

	//rush to the base in CTF
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " rush base" = (MSG_RUSHBASE, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " rush to base" = (MSG_RUSHBASE, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " rush to the base" = (MSG_RUSHBASE, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go to base" = (MSG_RUSHBASE, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " go to the base" = (MSG_RUSHBASE, ST_ADDRESSED);

	//return the flag
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " return the flag" = (MSG_RETURNFLAG, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " return our flag" = (MSG_RETURNFLAG, ST_ADDRESSED);


	//who is the team leader
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": who is ", "the leader"|"the team leader"|"team leader"|"leader","?"|"" = (MSG_WHOISTEAMLAEDER, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": is there a ", "leader"|"team leader","?"|"" = (MSG_WHOISTEAMLAEDER, 0);

	//become the team leader
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " will be ", THE_TEAM, "leader" = (MSG_STARTTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " want to be ", THE_TEAM, "leader" = (MSG_STARTTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " wants to be ", THE_TEAM, "leader" = (MSG_STARTTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " is ", THE_TEAM, "leader" = (MSG_STARTTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " you are ", THE_TEAM, "leader" = (MSG_STARTTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I am ", "the leader"|"the team leader"|"team leader"|"leader" = (MSG_STARTTEAMLEADERSHIP, ST_I);

	//stop being the team leader
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " is not ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " does not want to be ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " quits being ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", TEAMMATE, " stops being ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I will not be ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, ST_I);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I do not want to be ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, ST_I);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I quit being ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, ST_I);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": I stop being ", THE_TEAM, "leader" = (MSG_STOPTEAMLEADERSHIP, ST_I);

	//wait for someone
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " wait for me", " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_WAIT, $evalint(ST_NEARITEM|ST_ADDRESSED|ST_I));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " wait for me" = (MSG_WAIT, $evalint(ST_SOMEWHERE|ST_ADDRESSED|ST_I));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " wait for ", TEAMMATE, " near "|" at ", "the "|"checkpoint "|"waypoint "|"", ITEM = (MSG_WAIT, $evalint(ST_NEARITEM|ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " wait for ", TEAMMATE = (MSG_WAIT, $evalint(ST_SOMEWHERE|ST_ADDRESSED));

	//ask what someone/everyone is doing
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what are you doing", "?"|"" = (MSG_WHATAREYOUDOING, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": what are you doing ", ADDRESSEE, "?"|"" = (MSG_WHATAREYOUDOING, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " report" = (MSG_WHATAREYOUDOING, ST_ADDRESSED);

	//ask the team leader what to do
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": what is my command", "?"|"" = (MSG_WHATISMYCOMMAND, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": what should I do", "?"|"" = (MSG_WHATISMYCOMMAND, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": what am I supposed to do", "?"|"" = (MSG_WHATISMYCOMMAND, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": what is my job", "?"|"" = (MSG_WHATISMYCOMMAND, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what is my command", "?"|"" = (MSG_WHATISMYCOMMAND, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what should I do", "?"|"" = (MSG_WHATISMYCOMMAND, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what am I supposed to do", "?"|"" = (MSG_WHATISMYCOMMAND, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what is my job", "?"|"" = (MSG_WHATISMYCOMMAND, ST_ADDRESSED);

	//ask where someone or everyone is
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " where are you", "?"|"" = (MSG_WHEREAREYOU, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": where are you ", ADDRESSEE, "?"|"" = (MSG_WHEREAREYOU, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": where is ", ADDRESSEE, "?"|"" = (MSG_WHEREAREYOU, ST_ADDRESSED);

	//join a sub team
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " create team ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " create squad ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " join team ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " join squad ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " you"|" we"|"", " are", " in"|"", " team ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " you"|" we"|"", " are", " in"|"", " squad ", TEAMNAME = (MSG_JOINSUBTEAM, ST_ADDRESSED);

	//leave a sub team
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " leave your team" = (MSG_LEAVESUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " leave your squad" = (MSG_LEAVESUBTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " ungroup" = (MSG_LEAVESUBTEAM, ST_ADDRESSED);

	//what team are you in
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " which "|" what ", "team"|"squad", " are you ", "in"|"on", "?"|"" = (MSG_WHICHTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " in"|" on", " which "|" what ", "team"|"squad", " are you ", "?"|"" = (MSG_WHICHTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " what is your ", "team"|"squad","?"|"" = (MSG_WHICHTEAM, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " are you ", "in"|"on", " a ", "team"|"squad","?"|"" = (MSG_WHICHTEAM, ST_ADDRESSED);

	//dismiss
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " dismissed" = (MSG_DISMISS, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " dismiss" = (MSG_DISMISS, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " roam" = (MSG_DISMISS, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " stop action" = (MSG_DISMISS, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " cancel order" = (MSG_DISMISS, ST_ADDRESSED);

	//remember checkpoint
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "checkpoint "|"waypoint ", NAME, " is at gps ", POSITION = (MSG_CHECKPOINT, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", "checkpoint "|"waypoint ", NAME, " is at ", POSITION = (MSG_CHECKPOINT, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " checkpoint "|" waypoint ", NAME, " is at gps ", POSITION = (MSG_CHECKPOINT, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " checkpoint "|" waypoint ", NAME, " is at ", POSITION = (MSG_CHECKPOINT, ST_ADDRESSED);

	//patrol
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": patrol ", "from "|"", KEYAREA, " for", TIME = (MSG_PATROL, ST_TIME);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": patrol ", "from "|"", KEYAREA = (MSG_PATROL, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " patrol ", "from "|"", KEYAREA, " for", TIME = (MSG_PATROL, $evalint(ST_ADDRESSED|ST_TIME));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " patrol ", "from "|"", KEYAREA = (MSG_PATROL, ST_ADDRESSED);

	//create new formation
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " create a new ", FORMATION, " formation" = (MSG_CREATENEWFORMATION, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " we are going to create a new ", FORMATION, " formation" = (MSG_CREATENEWFORMATION, ST_ADDRESSED);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " we are going to create a new formation called ", FORMATION = (MSG_CREATENEWFORMATION, ST_ADDRESSED);

	//formation position
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " your formation position is ", POSITION, " relative to ", TEAMMATE = (MSG_FORMATIONPOSITION, ST_ADDRESSED);

	//form a known formation
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " form the ", FORMATION, " formation" = (MSG_DOFORMATION, ST_ADDRESSED);

	//the formation intervening space
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " the formation intervening space is ", NUMBER, " meter" = (MSG_FORMATIONSPACE, $evalint(ST_ADDRESSED|ST_METER));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " the formation intervening space is ", NUMBER, " feet" = (MSG_FORMATIONSPACE, $evalint(ST_ADDRESSED|ST_FEET));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " the"|""," follow distance is ", NUMBER, " meter" = (MSG_FORMATIONSPACE, $evalint(ST_ADDRESSED|ST_METER));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " the"|""," follow distance is ", NUMBER, " feet" = (MSG_FORMATIONSPACE, $evalint(ST_ADDRESSED|ST_FEET));

	//lead the way
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": lead the way" = (MSG_LEADTHEWAY, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": lead the way ", ADDRESSEE = (MSG_LEADTHEWAY, $evalint(ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " lead the way" = (MSG_LEADTHEWAY, $evalint(ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": lead ", TEAMMATE, " the way ", ADDRESSEE = (MSG_LEADTHEWAY, $evalint(ST_ADDRESSED|ST_SOMEONE));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " lead ", TEAMMATE, " the way" = (MSG_LEADTHEWAY, $evalint(ST_ADDRESSED|ST_SOMEONE));

	// suicide
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": suicide" = (MSG_SUICIDE, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " suicide" = (MSG_SUICIDE, $evalint(ST_ADDRESSED));
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": die" = (MSG_SUICIDE, 0);
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " die" = (MSG_SUICIDE, $evalint(ST_ADDRESSED));

	// make love
	EC"("|EC"[", NETNAME, EC")"|EC"]", PLACE, EC": ", ADDRESSEE, " fuck "|" make love to ", TEAMMATE = (MSG_MAKELOVE, $evalint(ST_ADDRESSED));

} //end MTCONTEXT_INITIALTEAMCHAT

MTCONTEXT_CTF
{
	NETNAME, " got the ", FLAG, " flag", "!"|"" = (MSG_CTF, ST_GOTFLAG);
	NETNAME, " captured the ", FLAG, " flag", "!"|"" = (MSG_CTF, ST_CAPTUREDFLAG);
	NETNAME, " returned the ", FLAG, " flag", "!"|"" = (MSG_CTF, ST_RETURNEDFLAG);
	//for One Flag CTF
	NETNAME, " got the flag", "!"|"" = (MSG_CTF, ST_1FCTFGOTFLAG);
} //end MTCONTEXT_CTF

MTCONTEXT_TIME
{
	TIME, " minute"|" min","s"|"" = (MSG_MINUTES, 0);
	TIME, " second"|" sec","s"|"" = (MSG_SECONDS, 0);
	"ever" = (MSG_FOREVER, 0);
	" a long time" = (MSG_FORALONGTIME, 0);
	" a while" = (MSG_FORAWHILE, 0);
} //end MTCONTEXT_TIME

MTCONTEXT_PATROLKEYAREA
{
	"the "|"checkpoint "|"waypoint "|"", KEYAREA, " to "|" and ", MORE = (MSG_PATROLKEYAREA, ST_MORE);
	"the "|"checkpoint "|"waypoint "|"", KEYAREA, " and loop"|" and back", " to the start"|"" = (MSG_PATROLKEYAREA, ST_BACK);
	"the "|"checkpoint "|"waypoint "|"", KEYAREA, " and reverse" = (MSG_PATROLKEYAREA, ST_REVERSE);
	"the "|"checkpoint "|"waypoint "|"", KEYAREA = (MSG_PATROLKEYAREA, 0);
} //end MTCONTEXT_PATROL

MTCONTEXT_TEAMMATE
{
	"me"|"I" = (MSG_ME, 0);
} //end MTCONTEXT_TEAMMATE

MTCONTEXT_ADDRESSEE
{
	"everyone"|"everybody" = (MSG_EVERYONE, 0);
	TEAMMATE, " and "|", "|","|" ,", MORE = (MSG_MULTIPLENAMES, 0);
	TEAMMATE = (MSG_NAME, 0);
} //end MTCONTEXT_ADDRESSEE

MTCONTEXT_REPLYCHAT
{
	EC"(", NETNAME, EC")", PLACE, EC": ", MESSAGE = (MSG_CHATTEAM, ST_TEAM);
	EC"[", NETNAME, EC"]", PLACE, EC": ", MESSAGE = (MSG_CHATTELL, ST_TEAM);
	NETNAME, EC": ", MESSAGE = (MSG_CHATALL, 0);
} //end MTCONTEXT_REPLYCHAT
