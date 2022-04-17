/* https://github.com/pelya
// https://github.com/Siile/Ninslash/pull/23

#include <base/math.h>

#include <engine/demo.h>
#include <engine/keys.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/storage.h>

#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <game/client/ui.h>

#include <game/generated/client_data.h>
#include <engine/shared/config.h>

#include "menus.h"

#if defined(WIN32)
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>

static HANDLE serverProcess = -1;
#else
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

static sorted_array<string> s_maplist;
static sorted_array<string> s_modelist;
static bool atexitRegistered = false;

static int MapScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	sorted_array<string> *maplist = (sorted_array<string> *)pUser;
	int l = str_length(pName);
	if(l < 4 || IsDir || str_comp(pName+l-4, ".map") != 0)
		return 0;
	maplist->add(string(pName, l - 4));
	return 0;
}

static int ModeScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	sorted_array<string> *maplist = (sorted_array<string> *)pUser;
	maplist->add("Death Match");
    maplist->add("Team Death Match");
    maplist->add("Catch The Flag");
    maplist->add("Insta CTF");
    maplist->add("Ball");
    maplist->add("Defense");
    maplist->add("Invasion");
	return 0;
}

void CMenus::ServerCreatorInit()
{
	if( s_maplist.size() == 0 )
	{
		Storage()->ListDirectory(IStorage::TYPE_ALL, "maps", MapScan, &s_maplist);
	}

    if( s_modelist.size() == 0 )
	{
		Storage()->ListDirectory(IStorage::TYPE_ALL, "modes", ModeScan, &s_modelist);
	}
}

static void StopServer()
{
#if defined(WIN32)
	if( serverProcess != -1 )
		TerminateProcess(serverProcess, 0);
	serverProcess = -1;
#elif defined(__ANDROID__)
	system("$SECURE_STORAGE_DIR/busybox killall ninslash_srv");
#else
	system("killall ninslash_srv ninslash_srv_d");
#endif
}

static void StartServer(const char *name, const char *type, const char *map, int bots)
{
	char aBuf[4096];
	str_format(aBuf, sizeof(aBuf),
		"sv_port 8303\n"
		"sv_name \"%s\"\n"
		"sv_gametype %s\n"
		"sv_map %s\n"
		"sv_maprotation %s\n"
		"sv_preferredteamsize %d\n"
		"sv_scorelimit 0\n"
		"sv_randomweapons 1\n"
		"sv_vanillapickups 1\n"
		"sv_weapondrops 1\n",
		name, type, map, map, bots);

	FILE *ff = fopen("server.cfg", "wb");
	if( !ff )
		return;
	fwrite(aBuf, str_length(aBuf), 1, ff);
	fclose(ff);

#if defined(WIN32)
	serverProcess = (HANDLE) _spawnl(_P_NOWAIT, "ninslash_srv.exe", "ninslash_srv.exe", "-f", "server.cfg", NULL);
#elif defined(__ANDROID__)
	system("$SECURE_STORAGE_DIR/ninslash_srv -f server.cfg >/dev/null 2>&1 &");
#else
	system("./ninslash_srv_d -f server.cfg || ./ninslash_srv -f server.cfg &");
#endif

	if( !atexitRegistered )
		atexit(&StopServer);
	atexitRegistered = true;
}

static bool ServerStatus()
{
#if defined(WIN32)
	return serverProcess != -1;
#elif defined(__ANDROID__)
	int status = system("$SECURE_STORAGE_DIR/busybox sh -c 'ps | grep ninslash_srv'");
	return WEXITSTATUS(status) == 0;
#else
	int status = system("ps | grep ninslash_srv");
	return WEXITSTATUS(status) == 0;
#endif
}

void CMenus::ServerCreatorProcess(CUIRect MainView)
{
	static int s_map = 0;
	static int s_bots = 5;

	static int64 LastUpdateTime = 0;
	static bool ServerRunning = false;
	static bool ServerStarting = false;

	bool ServerStarted = false;

	ServerCreatorInit();

	// background
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_ALL, 20.0f);
	MainView.Margin(20.0f, &MainView);

	MainView.HSplitTop(10, 0, &MainView);
	CUIRect MsgBox = MainView;
	UI()->DoLabelScaled(&MsgBox, Localize("Local server"), 20.0f, 0);

	if (time_get() / time_freq() > LastUpdateTime + 2)
	{
		LastUpdateTime = time_get() / time_freq();
		ServerRunning = ServerStatus();
		if (ServerRunning && ServerStarting)
			ServerStarted = true;
		ServerStarting = false;
	}

	MainView.HSplitTop(30, 0, &MainView);
	MsgBox = MainView;
	UI()->DoLabelScaled(&MsgBox, ServerStarting ? Localize("Server is starting") :
								 ServerRunning ? Localize("Server is running") :
								 Localize("Server stopped"), 20.0f, 0);

	MainView.HSplitTop(50, 0, &MainView);

	CUIRect Button;

	MainView.VSplitLeft(50, 0, &Button);
	Button.h = 30;
	Button.w = 200;
	static int s_StopServerButton = 0;
	if( ServerRunning && DoButton_Menu(&s_StopServerButton, Localize("Stop server"), 0, &Button))
	{
		StopServer();
		LastUpdateTime = time_get() / time_freq() - 2;
	}

	static int s_JoinServerButton = 0;
	if(ServerStarted || (ServerRunning && DoButton_Menu(&s_JoinServerButton, Localize("Join server"), 0, &Button)))
	{
		strcpy(g_Config.m_UiServerAddress, "127.0.0.1");
		Client()->Connect(g_Config.m_UiServerAddress);
	}

	MainView.HSplitTop(60, 0, &MainView);

	MainView.VSplitLeft(50, 0, &MsgBox);
	MsgBox.w = 100;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Bots"), s_bots);
	UI()->DoLabelScaled(&MsgBox, aBuf, 20.0f, -1);

	MainView.VSplitLeft(150, 0, &Button);
	Button.h = 30;
	Button.w = 500;

	s_bots = (int)(DoScrollbarH(&s_bots, &Button, s_bots/15.0f)*15.0f+0.1f);

	CUIRect DirectoryButton, ReloadButton;
	
	//DirectoryButton.VSplitLeft(50.0f, 0, &DirectoryButton);
	//DirectoryButton.HSplitTop(60.0f, 0, &DirectoryButton);
	
	DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, 0);
	DirectoryButton.y = 500;
	DirectoryButton.x = 100;
	static int s_AssetsDirID = 0;
	if(DoButton_Menu(&s_AssetsDirID, Localize("Maps directory"), 0, &DirectoryButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "data/maps", aBuf, sizeof(aBuf));
		if(!open_file(aBuf))
		{
			dbg_msg("menus", "couldn't open file");
		}
	}
}

//
*/