#ifndef PROJECTCLEANER_COMPAT_HPP
#define PROJECTCLEANER_COMPAT_HPP

#include	"ACAPinc.h"
#if defined (ServerMainVers_2800)
#include	"ACAPI_Interface.h"
#include	"ACAPI_Goodies.h"
#endif
#include	<cstring>

// Compatibility layer between ArchiCAD 26 (deprecated dispatchers, implicit
// attribute index conversions) and ArchiCAD 27+ (typed API functions,
// strongly typed API_AttributeIndex).

#if defined (ServerMainVers_2800)

inline API_AttributeIndex	PC_AttrIndex (Int32 index)				{ return ACAPI_CreateAttributeIndex (index); }
inline Int32				PC_AttrToInt (API_AttributeIndex index)	{ return index.ToInt32_Deprecated (); }

inline GSErrCode	PC_Attribute_GetNum (API_AttrTypeID typeID, UInt32* count)
{
	return ACAPI_Attribute_GetNum (typeID, *count);
}

inline GSErrCode	PC_Navigator_GetSet (API_NavigatorSet* set, Int32* index = nullptr)
{
	return ACAPI_Navigator_GetNavigatorSet (set, index);
}

inline GSErrCode	PC_Navigator_GetChildren (API_NavigatorItem* item, GS::Array<API_NavigatorItem>* children)
{
	return ACAPI_Navigator_GetNavigatorChildrenItems (item, children);
}

inline GSErrCode	PC_Navigator_GetItem (const API_Guid* guid, API_NavigatorItem* item)
{
	return ACAPI_Navigator_GetNavigatorItem (guid, item);
}

inline GSErrCode	PC_Navigator_ChangeItem (API_NavigatorItem* item)
{
	return ACAPI_Navigator_ChangeNavigatorItem (item);
}

inline GSErrCode	PC_Navigator_DeleteView (const API_Guid* viewGuid, const bool* silentMode)
{
	return ACAPI_Navigator_DeleteNavigatorView (viewGuid, silentMode);
}

inline GSErrCode	PC_GetLibraries (GS::Array<API_LibraryInfo>* libraries, Int32* embeddedIndex = nullptr)
{
	return ACAPI_LibraryManagement_GetLibraries (libraries, embeddedIndex);
}

inline GSErrCode	PC_DeleteEmbeddedLibItem (const IO::Location* location, bool keepGSMFile, bool silentMode)
{
	return ACAPI_LibraryManagement_DeleteEmbeddedLibItem (location, keepGSMFile, silentMode);
}

inline GSErrCode	PC_DeleteEmbeddedLibItems (const GS::Array<IO::Location>* locations, bool keepGSMFile, bool silentMode)
{
	return ACAPI_LibraryManagement_DeleteEmbeddedLibItems (locations, keepGSMFile, silentMode);
}

inline GSErrCode	PC_GetLayoutSets (API_LayoutInfo* layoutInfo, API_DatabaseUnId* databaseUnId = nullptr)
{
	return ACAPI_Navigator_GetLayoutSets (layoutInfo, databaseUnId);
}

inline GSErrCode	PC_ChangeLayoutSets (const API_LayoutInfo* layoutInfo, API_DatabaseUnId* databaseUnId)
{
	return ACAPI_Navigator_ChangeLayoutSets (layoutInfo, databaseUnId);
}

inline GSErrCode	PC_GetMasterLayoutDatabases (GS::Array<API_DatabaseUnId>* databaseUnIds)
{
	return ACAPI_Database_GetMasterLayoutDatabases (nullptr, databaseUnIds);
}

inline GSErrCode	PC_GetLayoutDatabases (GS::Array<API_DatabaseUnId>* databaseUnIds)
{
	return ACAPI_Database_GetLayoutDatabases (nullptr, databaseUnIds);
}

inline GSErrCode	PC_GetDatabaseInfo (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Window_GetDatabaseInfo (databaseInfo);
}

inline GSErrCode	PC_ModifyDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database_ModifyDatabase (databaseInfo);
}

inline GSErrCode	PC_GetCurrentWindow (API_WindowInfo* windowInfo)
{
	return ACAPI_Window_GetCurrentWindow (windowInfo);
}

inline GSErrCode	PC_GetCurrentDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database_GetCurrentDatabase (databaseInfo);
}

inline GSErrCode	PC_ChangeCurrentDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database_ChangeCurrentDatabase (databaseInfo);
}

inline GSErrCode	PC_LibPart_Get (API_LibPart* libPart)
{
	return ACAPI_LibraryPart_Get (libPart);
}

inline GSErrCode	PC_LibPart_GetNum (Int32* count)
{
	*count = 0;

	for (Int32 i = 1; i <= 100000; ++i) {
		API_LibPart probe = {};
		probe.index = i;
		GSErrCode err = ACAPI_LibraryPart_Get (&probe);
		if (probe.location != nullptr)
			delete probe.location;
		if (err == APIERR_BADINDEX)
			return NoError;
		*count = i;
	}
	return NoError;
}

inline GSErrCode	PC_GetConnectedElements (const API_Guid& guid, API_ElemTypeID typeID, GS::Array<API_Guid>* connectedElements)
{
	return ACAPI_Grouping_GetConnectedElements (guid, API_ElemType (typeID), connectedElements);
}

inline GSErrCode	PC_GetPoint (const GS::UniString& prompt, API_Coord* outPos, short /*floorInd*/)
{
	API_GetPointType gp = {};
	BNZeroMemory (&gp, sizeof (gp));

	// API char strings are UTF-8 in ArchiCAD 26+
	auto promptCStr = prompt.ToCStr (CC_UTF8);
	strncpy (gp.prompt, promptCStr.Get (), sizeof (gp.prompt) - 1);
	gp.prompt[sizeof (gp.prompt) - 1] = '\0';

	gp.changeFilter = false;
	gp.changePlane  = false;	// use the plane of the active floor plan view

	GSErrCode err = ACAPI_UserInput_GetPoint (&gp);
	if (err == NoError && outPos != nullptr)
		*outPos = API_Coord { gp.pos.x, gp.pos.y };

	return err;
}

inline GSErrCode	PC_NeigToCoord (API_Neig* neig, API_Coord3D* coord3D)
{
	return ACAPI_Element_NeigToCoord (neig, coord3D);
}

inline void	PC_DeselectAll (void)
{
	ACAPI_Selection_DeselectAll ();
}

inline void	PC_SelectElement (const API_Guid& guid)
{
	// Ask the server for a neig that represents this element;
	// ACAPI_Selection_Select requires a valid neigID.
	API_Neig neig = {};
	BNZeroMemory (&neig, sizeof (neig));
	neig.guid = guid;

	if (ACAPI_Selection_SetSelectedElementNeig (&guid, &neig) == NoError && neig.neigID != APINeig_None) {
		if (ACAPI_Selection_Select (GS::Array<API_Neig> { neig }, false) == NoError)
			return;
	}

	// Fallback: generic neig for the element type
	API_Elem_Head head = {};
	head.guid = guid;
	API_ElemTypeID typeID = API_ZombieElemID;
	if (ACAPI_Element_GetHeader (&head) == NoError)
		typeID = head.type.typeID;

	API_NeigID neigID = APINeig_None;
	if (ACAPI_Element_ElemTypeToNeigID (API_ElemType (typeID), neigID) == NoError && neigID != APINeig_None) {
		API_Neig fallbackNeig = {};
		fallbackNeig.neigID = neigID;
		fallbackNeig.guid = guid;
		ACAPI_Selection_Select (GS::Array<API_Neig> { fallbackNeig }, false);
	}
}

inline GSErrCode	PC_RegisterMenu (short menuStrResID, short promptStrResID, APIMenuCodeID menuPosCode, GSFlags menuFlags)
{
	return ACAPI_MenuItem_RegisterMenu (menuStrResID, promptStrResID, menuPosCode, menuFlags);
}

inline GSErrCode	PC_InstallMenuHandler (short menuStrResID, APIMenuCommandProc* handlerProc)
{
	return ACAPI_MenuItem_InstallMenuHandler (menuStrResID, handlerProc);
}

inline GSErrCode	PC_CatchProjectEvent (API_NotifyEventID whichNotification, APIProjectEventHandlerProc* handlerProc)
{
	return ACAPI_ProjectOperation_CatchProjectEvent (whichNotification, handlerProc);
}

#else

inline API_AttributeIndex	PC_AttrIndex (Int32 index)				{ return index; }
inline Int32				PC_AttrToInt (API_AttributeIndex index)	{ return (Int32) index; }

inline GSErrCode	PC_Attribute_GetNum (API_AttrTypeID typeID, UInt32* count)
{
	API_AttributeIndex n = 0;
	GSErrCode err = ACAPI_Attribute_GetNum (typeID, &n);
	*count = (UInt32) (Int32) n;
	return err;
}

inline GSErrCode	PC_Navigator_GetSet (API_NavigatorSet* set, Int32* index = nullptr)
{
	return ACAPI_Navigator (APINavigator_GetNavigatorSetID, set, index);
}

inline GSErrCode	PC_Navigator_GetChildren (API_NavigatorItem* item, GS::Array<API_NavigatorItem>* children)
{
	return ACAPI_Navigator (APINavigator_GetNavigatorChildrenItemsID, item, nullptr, children);
}

inline GSErrCode	PC_Navigator_GetItem (const API_Guid* guid, API_NavigatorItem* item)
{
	return ACAPI_Navigator (APINavigator_GetNavigatorItemID, (void*) guid, item);
}

inline GSErrCode	PC_Navigator_ChangeItem (API_NavigatorItem* item)
{
	return ACAPI_Navigator (APINavigator_ChangeNavigatorItemID, item, nullptr);
}

inline GSErrCode	PC_Navigator_DeleteView (const API_Guid* viewGuid, const bool* silentMode)
{
	return ACAPI_Navigator (APINavigator_DeleteNavigatorViewID, (void*) viewGuid, (void*) silentMode);
}

inline GSErrCode	PC_GetLibraries (GS::Array<API_LibraryInfo>* libraries, Int32* embeddedIndex = nullptr)
{
	return ACAPI_Environment (APIEnv_GetLibrariesID, libraries, embeddedIndex);
}

inline GSErrCode	PC_DeleteEmbeddedLibItem (const IO::Location* location, bool keepGSMFile, bool silentMode)
{
	return ACAPI_Environment (APIEnv_DeleteEmbeddedLibItemID, (void*) location, (void*) (size_t) keepGSMFile, (void*) (size_t) silentMode);
}

inline GSErrCode	PC_DeleteEmbeddedLibItems (const GS::Array<IO::Location>* locations, bool keepGSMFile, bool silentMode)
{
	return ACAPI_Environment (APIEnv_DeleteEmbeddedLibItemsID, (void*) locations, (void*) (size_t) keepGSMFile, (void*) (size_t) silentMode);
}

inline GSErrCode	PC_GetLayoutSets (API_LayoutInfo* layoutInfo, API_DatabaseUnId* databaseUnId = nullptr)
{
	return ACAPI_Environment (APIEnv_GetLayoutSetsID, layoutInfo, databaseUnId);
}

inline GSErrCode	PC_ChangeLayoutSets (const API_LayoutInfo* layoutInfo, API_DatabaseUnId* databaseUnId)
{
	return ACAPI_Environment (APIEnv_ChangeLayoutSetsID, (void*) layoutInfo, databaseUnId);
}

inline GSErrCode	PC_GetMasterLayoutDatabases (GS::Array<API_DatabaseUnId>* databaseUnIds)
{
	return ACAPI_Database (APIDb_GetMasterLayoutDatabasesID, nullptr, databaseUnIds);
}

inline GSErrCode	PC_GetLayoutDatabases (GS::Array<API_DatabaseUnId>* databaseUnIds)
{
	return ACAPI_Database (APIDb_GetLayoutDatabasesID, nullptr, databaseUnIds);
}

inline GSErrCode	PC_GetDatabaseInfo (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database (APIDb_GetDatabaseInfoID, databaseInfo);
}

inline GSErrCode	PC_ModifyDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database (APIDb_ModifyDatabaseID, databaseInfo);
}

inline GSErrCode	PC_GetCurrentWindow (API_WindowInfo* windowInfo)
{
	return ACAPI_Database (APIDb_GetCurrentWindowID, windowInfo, nullptr);
}

inline GSErrCode	PC_GetCurrentDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database (APIDb_GetCurrentDatabaseID, databaseInfo, nullptr);
}

inline GSErrCode	PC_ChangeCurrentDatabase (API_DatabaseInfo* databaseInfo)
{
	return ACAPI_Database (APIDb_ChangeCurrentDatabaseID, databaseInfo, nullptr);
}

inline GSErrCode	PC_LibPart_Get (API_LibPart* libPart)
{
	return ACAPI_LibPart_Get (libPart);
}

inline GSErrCode	PC_LibPart_GetNum (Int32* count)
{
	return ACAPI_LibPart_GetNum (count);
}

inline GSErrCode	PC_GetConnectedElements (const API_Guid& guid, API_ElemTypeID typeID, GS::Array<API_Guid>* connectedElements)
{
	return ACAPI_Element_GetConnectedElements (guid, typeID, connectedElements);
}

inline GSErrCode	PC_GetPoint (const GS::UniString& prompt, API_Coord* outPos, short /*floorInd*/)
{
	API_GetPointType gp = {};
	BNZeroMemory (&gp, sizeof (gp));

	auto promptCStr = prompt.ToCStr (CC_UTF8);
	strncpy (gp.prompt, promptCStr.Get (), sizeof (gp.prompt) - 1);
	gp.prompt[sizeof (gp.prompt) - 1] = '\0';

	gp.changeFilter = false;
	gp.changePlane  = false;

	GSErrCode err = ACAPI_Interface (APIIo_GetPointID, &gp, nullptr, nullptr);
	if (err == NoError && outPos != nullptr)
		*outPos = API_Coord { gp.pos.x, gp.pos.y };

	return err;
}

inline GSErrCode	PC_NeigToCoord (API_Neig* neig, API_Coord3D* coord3D)
{
	return ACAPI_Goodies (APIAny_NeigToCoordID, neig, coord3D, nullptr);
}

inline void	PC_DeselectAll (void)
{
}

inline void	PC_SelectElement (const API_Guid& /*guid*/)
{
}

inline GSErrCode	PC_RegisterMenu (short menuStrResID, short promptStrResID, APIMenuCodeID menuPosCode, GSFlags menuFlags)
{
	return ACAPI_Register_Menu (menuStrResID, promptStrResID, menuPosCode, menuFlags);
}

inline GSErrCode	PC_InstallMenuHandler (short menuStrResID, APIMenuCommandProc* handlerProc)
{
	return ACAPI_Install_MenuHandler (menuStrResID, handlerProc);
}

inline GSErrCode	PC_CatchProjectEvent (API_NotifyEventID whichNotification, APIProjectEventHandlerProc* handlerProc)
{
	return ACAPI_Notify_CatchProjectEvent (whichNotification, handlerProc);
}

#endif

#endif // PROJECTCLEANER_COMPAT_HPP
