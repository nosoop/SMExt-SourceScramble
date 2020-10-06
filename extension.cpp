/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"

#include "types/mempatch.h"
#include "types/memblock.h"
#include "userconf/mempatches.h"
#include "native_utils.h"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

MemPatchExt g_Extension;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_Extension);

MemoryPatchHandler g_MemoryPatchHandler;
MemoryBlockHandler g_MemoryBlockHandler;

bool MemPatchExt::SDK_OnLoad(char* error, size_t maxlength, bool late) {
	sharesys->AddNatives(myself, g_MemoryPatchNatives);
	sharesys->AddNatives(myself, g_MemoryBlockNatives);
	sharesys->AddNatives(myself, g_UtilNatives);
	
	gameconfs->AddUserConfigHook("MemPatches", &g_MemPatchConfig);
	
	g_MemoryPatchType = g_pHandleSys->CreateType("MemoryPatch",
			&g_MemoryPatchHandler, 0, NULL, NULL, myself->GetIdentity(), NULL);
	
	g_MemoryBlockType = g_pHandleSys->CreateType("MemoryBlock",
			&g_MemoryBlockHandler, 0, NULL, NULL, myself->GetIdentity(), NULL);
	
	return true;
}

void MemPatchExt::SDK_OnUnload() {
	gameconfs->RemoveUserConfigHook("MemPatches", &g_MemPatchConfig);
	g_pHandleSys->RemoveType(g_MemoryPatchType, myself->GetIdentity());
	g_pHandleSys->RemoveType(g_MemoryBlockType, myself->GetIdentity());
}
