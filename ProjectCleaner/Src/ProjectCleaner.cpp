// *****************************************************************************
// Project Cleaner - Add-On for ArchiCAD 26
// Scan and delete unused views in the View Map
// *****************************************************************************

#include	"APIEnvir.h"
#include	"ACAPinc.h"
#include	"APIdefs_Navigator.h"
#include	"APIdefs_LibraryParts.h"
#include	"DG.h"

#include	<functional>

// =============================================================================
// Resource IDs
// =============================================================================

#define MENU_RES_ID			32500
#define STR_RES_ADDON_INFO	32000
#define STR_RES_REPORT		32501

// Report strings (STR# 32501)
#define RS_SCAN_TITLE		1	// scan finished title
#define RS_TOTAL_VIEWS		2	// "Total views: "
#define RS_UNUSED_COUNT		3	// "Unused views: "
#define RS_LIST_HEADER		4	// "Candidates:"
#define RS_NO_PROBLEM		5	// "No unused views found."
#define RS_ABOUT_BODY		6	// about text
#define RS_DELETE_TITLE		7	// delete confirmation title
#define RS_DELETE_COUNT		8	// "Views to delete: "
#define RS_DELETE_WARNING	9	// irreversibility warning
#define RS_DELETE_BUTTON	10	// "Delete"
#define RS_CANCEL_BUTTON	11	// "Cancel"
#define RS_DELETED_COUNT	12	// "Deleted views: "
#define RS_DELETE_ERRORS	13	// "Failed to delete: "
#define RS_NOTHING_DELETE	14	// "Nothing to delete."
#define RS_LIB_TITLE		15	// library scan finished title
#define RS_LIB_TOTAL		16	// "Total library parts: "
#define RS_LIB_EMBEDDED		17	// "Embedded library parts: "
#define RS_LIB_UNUSED		18	// "Unused embedded parts: "
#define RS_LIB_LIST_HEADER	19	// "List:"
#define RS_LIB_NONE			20	// "No unused embedded parts."
#define RS_LIB_WARNING		21	// indirect usage warning
#define RS_LIB_NO_LIBRARY	22	// "No embedded library in this project."
#define RS_LIBDEL_TITLE		23	// library delete confirmation title
#define RS_LIBDEL_COUNT		24	// "Parts to delete: "
#define RS_LIBDEL_WARNING	25	// irreversibility + indirect usage warning
#define RS_LIBDEL_KEEP		26	// "Delete (keep .gsm files)"
#define RS_LIBDEL_FULL		27	// "Delete completely"
#define RS_LIBDEL_DELETED	29	// "Deleted parts: "
#define RS_LIBDEL_ERRORS	30	// "Failed to delete: "
#define RS_LIBDEL_NOTHING	31	// "Nothing to delete."
#define RS_LIBDEL_KEPT		32	// ".gsm files kept on disk."

// =============================================================================
// Resource helpers
// =============================================================================

static GS::UniString	GetResString (Int32 resId, Int32 stringIndex)
{
	GS::UniString result;
	RSGetIndString (&result, resId, stringIndex, ACAPI_GetOwnResModule ());
	return result;
}

static void AppendNumber (GS::UniString& target, UIndex number)
{
	target.Append (GS::UniString::Printf ("%lu", (unsigned long) number));
}

// ACAPI_WriteReport treats the first argument as a printf-style format string,
// so any '%' inside the message text must be escaped.
static GS::UniString	FormatSafe (const GS::UniString& text)
{
	GS::UniString result = text;
	result.ReplaceAll ("%", "%%");
	return result;
}

// =============================================================================
// Navigator tree helpers
// =============================================================================

static bool	GetMapRootItem (API_NavigatorMapID mapId, API_NavigatorItem* rootItem)
{
	API_NavigatorSet set;
	BNZeroMemory (&set, sizeof (API_NavigatorSet));
	set.mapId = mapId;

	Int32 setIndex = 0;
	if (ACAPI_Navigator (APINavigator_GetNavigatorSetID, &set, &setIndex) != NoError)
		return false;

	BNZeroMemory (rootItem, sizeof (API_NavigatorItem));
	rootItem->guid = set.rootGuid;
	rootItem->mapId = mapId;
	return true;
}

static GSErrCode	CollectNavigatorItems (const API_NavigatorItem& parent, Int32 depth,
										   GS::Array<API_NavigatorItem>* outItems)
{
	if (depth > 64 || outItems == nullptr)
		return NoError;

	GS::Array<API_NavigatorItem> children;
	API_NavigatorItem parentCopy = parent;

	GSErrCode err = ACAPI_Navigator (APINavigator_GetNavigatorChildrenItemsID, &parentCopy, nullptr, &children);
	if (err != NoError)
		return err;

	for (const API_NavigatorItem& child : children) {
		outItems->Push (child);
		GSErrCode childErr = CollectNavigatorItems (child, depth + 1, outItems);
		if (childErr != NoError)
			return childErr;
	}
	return NoError;
}

// =============================================================================
// Core scan: views of the Public View Map that are not referenced
// by any drawing placed in the Layout Book
// =============================================================================

static GSErrCode	ScanUnusedViews (UIndex* totalViews, GS::Array<API_NavigatorItem>* unusedViews)
{
	// 1. Collect all items of the Public View Map
	GS::Array<API_NavigatorItem> viewModelItems;
	{
		API_NavigatorItem root;
		if (!GetMapRootItem (API_PublicViewMap, &root))
			return Error;
		GSErrCode err = CollectNavigatorItems (root, 0, &viewModelItems);
		if (err != NoError)
			return err;
	}

	// 2. Collect GUIDs referenced from the Layout Book (placed drawings)
	GS::Array<API_Guid> usedGuids;
	{
		GS::Array<API_NavigatorItem> layoutItems;
		API_NavigatorItem root;
		if (!GetMapRootItem (API_LayoutMap, &root))
			return Error;
		GSErrCode err = CollectNavigatorItems (root, 0, &layoutItems);
		if (err != NoError)
			return err;

		for (const API_NavigatorItem& item : layoutItems) {
			if (item.itemType == API_DrawingNavItem && item.sourceGuid != APINULLGuid)
				usedGuids.Push (item.sourceGuid);
		}
	}

	// 3. Views that are not referenced anywhere = cleanup candidates
	*totalViews = 0;
	unusedViews->Clear ();
	for (const API_NavigatorItem& item : viewModelItems) {
		if (item.itemType == API_FolderNavItem)
			continue;
		(*totalViews)++;
		if (!usedGuids.Contains (item.guid))
			unusedViews->Push (item);
	}
	return NoError;
}

// =============================================================================
// Embedded library scanner
// =============================================================================

static void	CollectUsedLibInds (API_ElemTypeID elemType,
								const std::function<Int32 (const API_Element&)>& getLibInd,
								GS::Array<Int32>* usedLibInds)
{
	GS::Array<API_Guid> elemList;
	if (ACAPI_Element_GetElemList (elemType, &elemList) != NoError)
		return;

	for (const API_Guid& elemGuid : elemList) {
		API_Element element;
		BNZeroMemory (&element, sizeof (API_Element));
		element.header.guid = elemGuid;
		if (ACAPI_Element_Get (&element) != NoError)
			continue;
		Int32 libInd = getLibInd (element);
		if (libInd > 0 && !usedLibInds->Contains (libInd))
			usedLibInds->Push (libInd);
	}
}

static GSErrCode	GetEmbeddedLibraryLocation (IO::Location* embeddedLocation, bool* found)
{
	GS::Array<API_LibraryInfo> libraries;
	Int32 embeddedIndex = -1;
	*found = false;

	GSErrCode err = ACAPI_Environment (APIEnv_GetLibrariesID, &libraries, &embeddedIndex);
	if (err != NoError)
		return err;

	if (embeddedIndex >= 0 && embeddedIndex < (Int32) libraries.GetSize ()) {
		*embeddedLocation = libraries[embeddedIndex].location;
		*found = true;
		return NoError;
	}

	for (const API_LibraryInfo& lib : libraries) {
		if (lib.libraryType == API_EmbeddedLibrary) {
			*embeddedLocation = lib.location;
			*found = true;
			break;
		}
	}
	return NoError;
}

static GSErrCode	ScanUnusedEmbeddedLibParts (UIndex* totalParts,
											  UIndex* totalEmbedded,
											  GS::Array<GS::UniString>* unusedNames,
											  GS::Array<IO::Location>* unusedLocations,
											  bool* hasEmbedded)
{
	*totalParts = 0;
	*totalEmbedded = 0;
	*hasEmbedded = false;
	unusedNames->Clear ();
	unusedLocations->Clear ();

	GS::Array<Int32> usedLibInds;
	CollectUsedLibInds (API_ObjectID,	[] (const API_Element& e) { return e.object.libInd; },					&usedLibInds);
	CollectUsedLibInds (API_LampID,		[] (const API_Element& e) { return e.lamp.libInd; },					&usedLibInds);
	CollectUsedLibInds (API_DoorID,		[] (const API_Element& e) { return e.door.openingBase.libInd; },		&usedLibInds);
	CollectUsedLibInds (API_WindowID,	[] (const API_Element& e) { return e.window.openingBase.libInd; },		&usedLibInds);
	CollectUsedLibInds (API_SkylightID,	[] (const API_Element& e) { return e.skylight.openingBase.libInd; },	&usedLibInds);
	CollectUsedLibInds (API_ZoneID,		[] (const API_Element& e) { return e.zone.libInd; },					&usedLibInds);
	CollectUsedLibInds (API_LabelID,	[] (const API_Element& e) {
							return (e.label.labelClass == APILblClass_Symbol) ? e.label.u.symbol.libInd : 0; },
						&usedLibInds);
	CollectUsedLibInds (API_DrawingID,	[] (const API_Element& e) { return e.drawing.title.libInd; },			&usedLibInds);

	IO::Location embeddedLocation;
	GSErrCode err = GetEmbeddedLibraryLocation (&embeddedLocation, hasEmbedded);
	if (err != NoError)
		return err;
	if (!*hasEmbedded)
		return NoError;

	Int32 libCount = 0;
	err = ACAPI_LibPart_GetNum (&libCount);
	if (err != NoError)
		return err;

	for (Int32 i = 1; i <= libCount; i++) {
		API_LibPart libPart;
		BNZeroMemory (&libPart, sizeof (API_LibPart));
		libPart.index = i;
		if (ACAPI_LibPart_Get (&libPart) != NoError)
			continue;

		(*totalParts)++;
		bool isEmbedded = (libPart.location != nullptr && embeddedLocation.IsAncestorOf (*libPart.location));
		if (isEmbedded) {
			(*totalEmbedded)++;
			if (!usedLibInds.Contains (i)) {
				GS::UniString name (libPart.docu_UName);
				if (name.IsEmpty ())
					name = GS::UniString (libPart.file_UName);
				unusedNames->Push (name);
				if (libPart.location != nullptr)
					unusedLocations->Push (IO::Location (*libPart.location));
			}
		}
		if (libPart.location != nullptr)
			delete libPart.location;
	}
	return NoError;
}

static GSErrCode Do_ScanEmbeddedLibrary (void)
{
	UIndex totalParts = 0;
	UIndex totalEmbedded = 0;
	GS::Array<GS::UniString> unusedNames;
	GS::Array<IO::Location> unusedLocations;
	bool hasEmbedded = false;
	GSErrCode err = ScanUnusedEmbeddedLibParts (&totalParts, &totalEmbedded, &unusedNames, &unusedLocations, &hasEmbedded);
	if (err != NoError)
		return err;

	GS::UniString title = GetResString (STR_RES_REPORT, RS_LIB_TITLE);
	if (!hasEmbedded) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_LIB_NO_LIBRARY)), true);
		return NoError;
	}

	GS::UniString report = GetResString (STR_RES_REPORT, RS_LIB_TOTAL);
	AppendNumber (report, totalParts);
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LIB_EMBEDDED));
	AppendNumber (report, totalEmbedded);
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LIB_UNUSED));
	AppendNumber (report, unusedNames.GetSize ());

	if (unusedNames.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_LIB_NONE));
	} else {
		const UInt32 maxNamesToList = 40;
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_LIB_LIST_HEADER));
		UInt32 listed = 0;
		for (const GS::UniString& name : unusedNames) {
			if (listed >= maxNamesToList) {
				report.Append ("\n...");
				break;
			}
			report.Append ("\n- ");
			report.Append (name);
			listed++;
		}
	}
	report.Append ("\n\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LIB_WARNING));

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

static GSErrCode Do_DeleteUnusedEmbeddedLibParts (void)
{
	UIndex totalParts = 0;
	UIndex totalEmbedded = 0;
	GS::Array<GS::UniString> unusedNames;
	GS::Array<IO::Location> unusedLocations;
	bool hasEmbedded = false;
	GSErrCode err = ScanUnusedEmbeddedLibParts (&totalParts, &totalEmbedded, &unusedNames, &unusedLocations, &hasEmbedded);
	if (err != NoError)
		return err;

	GS::UniString title = GetResString (STR_RES_REPORT, RS_LIBDEL_TITLE);
	if (!hasEmbedded) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_LIB_NO_LIBRARY)), true);
		return NoError;
	}
	if (unusedNames.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_LIBDEL_NOTHING)), true);
		return NoError;
	}

	GS::UniString confirmText = GetResString (STR_RES_REPORT, RS_LIBDEL_COUNT);
	AppendNumber (confirmText, unusedNames.GetSize ());
	confirmText.Append ("\n\n");
	confirmText.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_WARNING));

	short button = DGAlert (DG_WARNING,
							title,
							confirmText,
							GS::UniString (),
							GetResString (STR_RES_REPORT, RS_LIBDEL_KEEP),
							GetResString (STR_RES_REPORT, RS_LIBDEL_FULL),
							GetResString (STR_RES_REPORT, RS_CANCEL_BUTTON));
	if (button != 1 && button != 2)
		return NoError;

	bool keepGSM = (button == 2);
	bool silentMode = true;

	GS::Array<IO::Location> locations;
	for (const IO::Location& loc : unusedLocations) {
		locations.Push (loc);
	}

	err = ACAPI_Environment (APIEnv_DeleteEmbeddedLibItemsID, &locations, (void*)(size_t)keepGSM, (void*)(size_t)silentMode);

	// Verification scan to count actual deleted parts
	UIndex totalPartsAfter = 0;
	UIndex totalEmbeddedAfter = 0;
	GS::Array<GS::UniString> unusedNamesAfter;
	GS::Array<IO::Location> unusedLocationsAfter;
	bool hasEmbeddedAfter = false;
	ScanUnusedEmbeddedLibParts (&totalPartsAfter, &totalEmbeddedAfter, &unusedNamesAfter, &unusedLocationsAfter, &hasEmbeddedAfter);

	UInt32 requestedCount = unusedNames.GetSize ();
	UInt32 remainingCount = unusedNamesAfter.GetSize ();
	UInt32 deleted = (requestedCount > remainingCount) ? (requestedCount - remainingCount) : 0;
	UInt32 failed = requestedCount - deleted;

	GS::UniString result;
	if (deleted > 0) {
		result.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_DELETED));
		AppendNumber (result, deleted);
	}
	if (failed > 0) {
		if (!result.IsEmpty ())
			result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_ERRORS));
		AppendNumber (result, failed);
	}
	if (deleted == 0 && failed == 0) {
		result.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_NOTHING));
	}
	if (keepGSM && deleted > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_KEPT));
	}
	ACAPI_WriteReport (FormatSafe (title + "\n" + result), true);
	return NoError;
}

// =============================================================================
// Commands
// =============================================================================

static GSErrCode Do_ScanUnusedViews (void)
{
	UIndex totalViews = 0;
	GS::Array<API_NavigatorItem> unusedViews;
	GSErrCode err = ScanUnusedViews (&totalViews, &unusedViews);
	if (err != NoError)
		return err;

	GS::UniString report = GetResString (STR_RES_REPORT, RS_TOTAL_VIEWS);
	AppendNumber (report, totalViews);
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_UNUSED_COUNT));
	AppendNumber (report, unusedViews.GetSize ());

	if (unusedViews.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_NO_PROBLEM));
	} else {
		const UInt32 maxNamesToList = 40;
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_LIST_HEADER));
		UInt32 listed = 0;
		for (const API_NavigatorItem& view : unusedViews) {
			if (listed >= maxNamesToList) {
				report.Append ("\n...");
				break;
			}
			report.Append ("\n- ");
			report.Append (GS::UniString (view.uName));
			listed++;
		}
	}

	GS::UniString title = GetResString (STR_RES_REPORT, RS_SCAN_TITLE);
	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

static GSErrCode Do_DeleteUnusedViews (void)
{
	UIndex totalViews = 0;
	GS::Array<API_NavigatorItem> unusedViews;
	GSErrCode err = ScanUnusedViews (&totalViews, &unusedViews);
	if (err != NoError)
		return err;

	if (unusedViews.IsEmpty ()) {
		GS::UniString title = GetResString (STR_RES_REPORT, RS_DELETE_TITLE);
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_NOTHING_DELETE)), true);
		return NoError;
	}

	// Confirmation (the operation is NOT undoable!)
	GS::UniString confirmText = GetResString (STR_RES_REPORT, RS_DELETE_COUNT);
	AppendNumber (confirmText, unusedViews.GetSize ());
	confirmText.Append ("\n\n");
	confirmText.Append (GetResString (STR_RES_REPORT, RS_DELETE_WARNING));

	GS::UniString confirmTitle = GetResString (STR_RES_REPORT, RS_DELETE_TITLE);
	short button = DGAlert (DG_WARNING,
							confirmTitle,
							confirmText,
							GS::UniString (),
							GetResString (STR_RES_REPORT, RS_DELETE_BUTTON),
							GetResString (STR_RES_REPORT, RS_CANCEL_BUTTON));
	if (button != 1)
		return NoError;

	// Delete views (non-undoable operation)
	UInt32 deleted = 0;
	UInt32 failed = 0;
	bool silentMode = true;
	for (const API_NavigatorItem& view : unusedViews) {
		API_Guid viewGuid = view.guid;
		GSErrCode delErr = ACAPI_Navigator (APINavigator_DeleteNavigatorViewID, &viewGuid, &silentMode);
		if (delErr == NoError)
			deleted++;
		else
			failed++;
	}

	GS::UniString result = GetResString (STR_RES_REPORT, RS_DELETED_COUNT);
	AppendNumber (result, deleted);
	if (failed > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_DELETE_ERRORS));
		AppendNumber (result, failed);
	}
	GS::UniString resultTitle = GetResString (STR_RES_REPORT, RS_DELETE_TITLE);
	ACAPI_WriteReport (FormatSafe (resultTitle + "\n" + result), true);
	return NoError;
}

static GSErrCode Do_About (void)
{
	GS::UniString title = GetResString (STR_RES_ADDON_INFO, 1);
	GS::UniString body = GetResString (STR_RES_REPORT, RS_ABOUT_BODY);
	ACAPI_WriteReport (FormatSafe (title + "\n" + body), true);
	return NoError;
}

// =============================================================================
// Menu handler
// =============================================================================

GSErrCode __ACENV_CALL	MenuHandler (const API_MenuParams* menuParams)
{
	switch (menuParams->menuItemRef.itemIndex) {
		case 1:		return Do_ScanUnusedViews ();
		case 2:		return Do_DeleteUnusedViews ();
		case 3:		return Do_ScanEmbeddedLibrary ();
		case 4:		return Do_DeleteUnusedEmbeddedLibParts ();
		case 6:		return Do_About ();
		default:	break;
	}
	return NoError;
}

// =============================================================================
// Required entry points
// =============================================================================

API_AddonType __ACENV_CALL	CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, STR_RES_ADDON_INFO, 1, ACAPI_GetOwnResModule ());
	RSGetIndString (&envir->addOnInfo.description, STR_RES_ADDON_INFO, 2, ACAPI_GetOwnResModule ());
	return APIAddon_Preload;
}

GSErrCode __ACENV_CALL	RegisterInterface (void)
{
	return ACAPI_Register_Menu (MENU_RES_ID, 0, MenuCode_UserDef, MenuFlag_SeparatorBefore);
}

GSErrCode __ACENV_CALL	Initialize (void)
{
	return ACAPI_Install_MenuHandler (MENU_RES_ID, MenuHandler);
}

GSErrCode __ACENV_CALL	FreeData (void)
{
	return NoError;
}
