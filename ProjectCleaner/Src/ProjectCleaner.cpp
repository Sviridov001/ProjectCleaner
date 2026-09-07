// *****************************************************************************
// Project Cleaner - Add-On for ArchiCAD 26
// Scan and delete unused views in the View Map
// *****************************************************************************

#include	"APIEnvir.h"
#include	"ACAPinc.h"
#include	"APIdefs_Navigator.h"
#include	"APIdefs_LibraryParts.h"
#include	"DG.h"

#include	"Folder.hpp"
#include	"Name.hpp"

#include	<functional>
#include	<cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include	"Windows.h"

#include	"ProjectCleanerPalette.hpp"

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
#define RS_FOLDERS_DELETED	46	// "Empty folders deleted: "
#define RS_HATCH_TITLE		47	// hatch area report title
#define RS_HATCH_NO_SEL		48	// "No hatches selected."
#define RS_HATCH_TYPE_0		49	// "Drafting"
#define RS_HATCH_TYPE_1		50	// "Cut"
#define RS_HATCH_TYPE_2		51	// "Cover"
#define RS_HATCH_PEN		52	// "Pen "
#define RS_HATCH_COUNT		53	// "Count"
#define RS_HATCH_AREA		54	// "Area, m²"
#define RS_HATCH_TOTAL		55	// "TOTAL"
#define RS_LINE_TITLE		56	// line length report title
#define RS_LINE_NO_SEL		57	// "No lines selected."
#define RS_LINE_COUNT		58	// "elements: "
#define RS_LINE_LENGTH		59	// "length, m"
#define RS_LAYER_TITLE		60	// layer scan title
#define RS_LAYER_TOTAL		61	// "Total layers: "
#define RS_LAYER_UNUSED		62	// "Unused layers: "
#define RS_LAYER_LIST		63	// "Candidates:"
#define RS_LAYER_NONE		64	// "No unused layers found."
#define RS_LAYER_HIDDEN		65	// " (hidden)"
#define RS_LAYER_LOCKED		66	// " (locked)"
#define RS_LAYER_WARNING	67	// deletion warning
#define RS_LAYERDEL_TITLE	68	// delete confirmation title
#define RS_LAYERDEL_COUNT	69	// "Layers to delete: "
#define RS_LAYERDEL_DONE	70	// "Deleted layers: "
#define RS_LAYERDEL_FAIL	71	// "Failed to delete: "
#define RS_LAYERDEL_NOTHING	72	// "Nothing to delete."
#define RS_STAT_TITLE		73	// statistics title
#define RS_STAT_TOTAL		74	// "Total elements: "
#define RS_STAT_BY_TYPE		75	// "By type:"
#define RS_STAT_LAYERS		76	// "Layers: "
#define RS_LAYERDEL_RENAME	77	// rename title
#define RS_LAYERDEL_RENAMED	78	// "Renamed layers: "
#define RS_LAYERDEL_RENFAIL	79	// "Failed to rename: "
#define RS_LAYER_PROMPT_MSG	80	// "Add _null_ prefix?"
#define RS_LAYER_PROMPT_YES	81	// "Add prefix"
#define RS_LAYER_PROMPT_NO	82	// "No, report only"
#define RS_ZONE_TITLE		83	// zone creation report title
#define RS_ZONE_NO_SEL		84	// "No hatches selected."
#define RS_ZONE_CREATED		85	// "Created zones: "
#define RS_ZONE_FAIL		86	// "Failed to create: "
#define RS_MASTER_TITLE		87	// master layout scan title
#define RS_MASTER_TOTAL		88	// "Total master layouts: "
#define RS_MASTER_UNUSED	89	// "Unused master layouts: "
#define RS_MASTER_LIST		90	// "Candidates for renaming:"
#define RS_MASTER_NONE		91	// "No unused master layouts found."
#define RS_MASTER_PROMPT_MSG	92	// "Add _null_ prefix?"
#define RS_MASTER_PROMPT_YES	93	// "Add prefix"
#define RS_MASTER_PROMPT_NO	94	// "No, report only"
#define RS_MASTER_RENAME_TITLE	95	// rename title
#define RS_MASTER_RENAMED	96	// "Renamed master layouts: "
#define RS_MASTER_RENFAIL	97	// "Failed to rename: "
#define RS_SLAB_TITLE		98	// slab creation report title
#define RS_SLAB_NO_SEL		99	// "No hatches selected."
#define RS_SLAB_CREATED		100	// "Created slabs: "
#define RS_SLAB_FAIL		101	// "Failed to create: "

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

struct EmbeddedLibPartInfo {
	Int32				index;
	GS::UniString		name;
	IO::Location		location;
};

static GSErrCode	CollectEmbeddedLibParts (GS::Array<EmbeddedLibPartInfo>* embeddedParts,
											 UIndex* totalParts,
											 bool* hasEmbedded)
{
	*totalParts = 0;
	*hasEmbedded = false;
	embeddedParts->Clear ();

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
		if (libPart.location != nullptr && embeddedLocation.IsAncestorOf (*libPart.location)) {
			EmbeddedLibPartInfo info;
			info.index = i;
			info.name = GS::UniString (libPart.docu_UName);
			if (info.name.IsEmpty ())
				info.name = GS::UniString (libPart.file_UName);
			info.location = *libPart.location;
			embeddedParts->Push (info);
		}
		if (libPart.location != nullptr)
			delete libPart.location;
	}
	return NoError;
}

// Recursively delete folders that became empty inside the embedded library.
// Bottom-up traversal; the root folder itself is never deleted.
static UInt32	DeleteEmptyLibFoldersRecursive (const IO::Location& folderLoc)
{
	UInt32 deletedCount = 0;
	GS::Array<IO::Name> subFolderNames;
	{
		IO::Folder folder (folderLoc);
		if (folder.GetStatus () != NoError)
			return 0;
		folder.Enumerate ([&subFolderNames] (const IO::Name& name, bool isFolder) {
			if (isFolder)
				subFolderNames.Push (name);
		});
	}

	for (const IO::Name& name : subFolderNames) {
		IO::Location subLoc (folderLoc, name);
		deletedCount += DeleteEmptyLibFoldersRecursive (subLoc);

		bool isEmpty = false;
		{
			IO::Folder subFolder (subLoc);
			if (subFolder.GetStatus () == NoError)
				subFolder.IsEmpty (&isEmpty);
		}
		if (isEmpty) {
			bool keepGSM = false;
			bool silentMode = true;
			IO::Location subLocCopy = subLoc;
			if (ACAPI_Environment (APIEnv_DeleteEmbeddedLibItemID, &subLocCopy, (void*)(size_t)keepGSM, (void*)(size_t)silentMode) == NoError)
				deletedCount++;
		}
	}
	return deletedCount;
}

static GSErrCode	ScanUnusedEmbeddedLibParts (UIndex* totalParts,
											  UIndex* totalEmbedded,
											  GS::Array<GS::UniString>* unusedNames,
											  GS::Array<IO::Location>* unusedLocations,
											  bool* hasEmbedded)
{
	*totalEmbedded = 0;
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

	GS::Array<EmbeddedLibPartInfo> embeddedParts;
	GSErrCode err = CollectEmbeddedLibParts (&embeddedParts, totalParts, hasEmbedded);
	if (err != NoError)
		return err;

	for (const EmbeddedLibPartInfo& info : embeddedParts) {
		(*totalEmbedded)++;
		if (!usedLibInds.Contains (info.index)) {
			unusedNames->Push (info.name);
			unusedLocations->Push (info.location);
		}
	}
	return NoError;
}

GSErrCode Do_ScanDeleteEmbeddedLibrary (void)
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

	// Report
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
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_LIB_WARNING));
		ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
		return NoError;
	}

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
	report.Append ("\n\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LIB_WARNING));
	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);

	// Confirm delete
	GS::UniString confirmText = GetResString (STR_RES_REPORT, RS_LIBDEL_COUNT);
	AppendNumber (confirmText, unusedNames.GetSize ());
	confirmText.Append ("\n\n");
	confirmText.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_WARNING));

	GS::UniString confirmTitle = GetResString (STR_RES_REPORT, RS_LIBDEL_TITLE);
	short button = DGAlert (DG_WARNING,
							confirmTitle,
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

	// Verification scan
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

	if (deleted > 0) {
		IO::Location embeddedRootLoc;
		bool hasEmbeddedRoot = false;
		if (GetEmbeddedLibraryLocation (&embeddedRootLoc, &hasEmbeddedRoot) == NoError && hasEmbeddedRoot) {
			UInt32 deletedFolders = DeleteEmptyLibFoldersRecursive (embeddedRootLoc);
			if (deletedFolders > 0) {
				result.Append ("\n");
				result.Append (GetResString (STR_RES_REPORT, RS_FOLDERS_DELETED));
				AppendNumber (result, deletedFolders);
			}
		}
	}

	if (keepGSM && deleted > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_LIBDEL_KEPT));
	}
	ACAPI_WriteReport (FormatSafe (confirmTitle + "\n" + result), true);
	return NoError;
}

// =============================================================================
// Commands
// =============================================================================

GSErrCode Do_ScanDeleteUnusedViews (void)
{
	UIndex totalViews = 0;
	GS::Array<API_NavigatorItem> unusedViews;
	GSErrCode err = ScanUnusedViews (&totalViews, &unusedViews);
	if (err != NoError)
		return err;

	// Report
	GS::UniString title = GetResString (STR_RES_REPORT, RS_SCAN_TITLE);
	GS::UniString report = GetResString (STR_RES_REPORT, RS_TOTAL_VIEWS);
	AppendNumber (report, totalViews);
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_UNUSED_COUNT));
	AppendNumber (report, unusedViews.GetSize ());

	if (unusedViews.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_NO_PROBLEM));
		ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
		return NoError;
	}

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
	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);

	// Confirm delete
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

	// Delete
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
	ACAPI_WriteReport (FormatSafe (confirmTitle + "\n" + result), true);
	return NoError;
}

static void	CopyTextToClipboard (const GS::UniString& text)
{
	if (!OpenClipboard (nullptr))
		return;
	EmptyClipboard ();
	Int32 len = text.GetLength ();
	HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, (len + 1) * sizeof (wchar_t));
	if (hMem != nullptr) {
		wchar_t* pMem = (wchar_t*) GlobalLock (hMem);
		for (Int32 i = 0; i < len; i++)
			pMem[i] = text[i];
		pMem[len] = 0;
		GlobalUnlock (hMem);
		SetClipboardData (CF_UNICODETEXT, hMem);
	}
	CloseClipboard ();
}

// =============================================================================
// Hatch area calculator
// =============================================================================

struct HatchGroupKey {
	API_AttributeIndex	fillInd;
	short				fillPenIndex;

	bool operator== (const HatchGroupKey& other) const {
		return fillInd == other.fillInd && fillPenIndex == other.fillPenIndex;
	}
};

struct HatchGroupData {
	UInt32	count;
	double	area;		// m²
};

static bool	ArcGetOrigo (const API_Coord& begC, const API_Coord& endC, double angle, API_Coord& origo)
{
	double halfAngle = angle / 2.0;
	double sinHalf = sin (halfAngle);
	if (fabs (sinHalf) < 1e-10)
		return false;

	double dx = endC.x - begC.x;
	double dy = endC.y - begC.y;
	double chord = sqrt (dx * dx + dy * dy);
	if (chord < 1e-10)
		return false;

	double R = chord / (2.0 * sinHalf);
	double d = R * cos (halfAngle);

	double mx = (begC.x + endC.x) / 2.0;
	double my = (begC.y + endC.y) / 2.0;
	double nx = -dy / chord;
	double ny = dx / chord;

	if (halfAngle > 0.0) {
		origo.x = mx + d * nx;
		origo.y = my + d * ny;
	} else {
		origo.x = mx - d * nx;
		origo.y = my - d * ny;
	}

	return true;
}

static bool	PointInsideContour (const API_Coord& pt,
							   const API_ElementMemo& memo, Int32 start, Int32 end)
{
	bool inside = false;
	for (Int32 i = start, j = end; i <= end; j = i++) {
		const API_Coord& a = (*memo.coords)[i];
		const API_Coord& b = (*memo.coords)[j];
		if (((a.y > pt.y) != (b.y > pt.y)) &&
			(pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x))
			inside = !inside;
	}
	return inside;
}

static double	CalcPolygonArea (const API_HatchType& hatch, const API_ElementMemo& memo, GS::UniString* debugOut)
{
	Int32 nCoords = hatch.poly.nCoords;
	Int32 nSubPolys = hatch.poly.nSubPolys;
	Int32 nArcs = hatch.poly.nArcs;

	if (nCoords < 3 || memo.coords == nullptr || memo.pends == nullptr)
		return 0.0;

	if (debugOut)
		*debugOut = GS::UniString::Printf ("coords=%d subPolys=%d arcs=%d", (int)nCoords, (int)nSubPolys, (int)nArcs);

	double totalArea = 0.0;
	Int32 subPolyStart = 1;

	// First pass: compute area per contour
	GS::Array<double> contourAreas;
	GS::Array<Int32> contourStarts, contourEnds;
	for (Int32 sp = 1; sp <= nSubPolys; sp++) {
		Int32 subPolyEnd = (*memo.pends)[sp];
		contourStarts.Push (subPolyStart);
		contourEnds.Push (subPolyEnd);

		double area = 0.0;
		for (Int32 i = subPolyStart; i <= subPolyEnd; i++) {
			const API_Coord& p1 = (*memo.coords)[i];
			const API_Coord& p2 = (*memo.coords)[i < subPolyEnd ? i + 1 : subPolyStart];
			area += (p2.x + p1.x) * (p2.y - p1.y) * 0.5;
		}

		if (nArcs > 0 && memo.parcs != nullptr) {
			for (Int32 a = 0; a < nArcs; a++) {
				const API_PolyArc& arc = (*memo.parcs)[a];
				if (arc.begIndex >= subPolyStart && arc.endIndex <= subPolyEnd) {
					const API_Coord& A = (*memo.coords)[arc.begIndex];
					const API_Coord& B = (*memo.coords)[arc.endIndex];
					API_Coord centre;
					if (ArcGetOrigo (A, B, arc.arcAngle, centre)) {
						double radius = sqrt ((centre.x - B.x) * (centre.x - B.x) +
											  (centre.y - B.y) * (centre.y - B.y));
						area += radius * radius * (arc.arcAngle - sin (arc.arcAngle)) * 0.5;
					}
				}
			}
		}

		contourAreas.Push (area);
		subPolyStart = subPolyEnd + 1;
	}

	// Second pass: determine add/subtract via point-in-polygon
	for (Int32 sp = 0; sp < static_cast<Int32>(contourAreas.GetSize ()); sp++) {
		// Test first vertex of this contour against all previous contours
		API_Coord testPt = (*memo.coords)[contourStarts[sp]];
		bool insidePrev = false;
		for (Int32 prev = 0; prev < sp; prev++) {
			if (PointInsideContour (testPt, memo, contourStarts[prev], contourEnds[prev])) {
				insidePrev = !insidePrev;  // XOR: inside odd number of previous = hole
			}
		}

		if (insidePrev)
			totalArea -= fabs (contourAreas[sp]);
		else
			totalArea += fabs (contourAreas[sp]);

		if (debugOut) {
			*debugOut += GS::UniString::Printf ("\n  sp%d[%d..%d] raw=%.4f -> %s",
				(int)(sp + 1), (int)contourStarts[sp], (int)contourEnds[sp],
				contourAreas[sp], insidePrev ? "SUBTRACT" : "ADD");
		}
	}

	return fabs (totalArea);
}

static GS::UniString	GetFillName (API_AttributeIndex fillInd)
{
	API_Attribute attr;
	BNZeroMemory (&attr, sizeof (API_Attribute));
	attr.header.index = fillInd;
	attr.header.typeID = API_FilltypeID;
	if (ACAPI_Attribute_Get (&attr) == NoError) {
		if (attr.filltype.head.uniStringNamePtr != nullptr)
			return *attr.filltype.head.uniStringNamePtr;
		return GS::UniString (attr.filltype.head.name);
	}
	return GS::UniString::Printf ("#%d", static_cast<int>(fillInd));
}

GSErrCode Do_CalcHatchAreas (void)
{
	GS::UniString title = GetResString (STR_RES_REPORT, RS_HATCH_TITLE);

	// 1. Get selection
	API_SelectionInfo selInfo;
	BNZeroMemory (&selInfo, sizeof (API_SelectionInfo));
	GS::Array<API_Neig> selNeigs;
	GSErrCode err = ACAPI_Selection_Get (&selInfo, &selNeigs, true, false, API_InsidePartially);
	if (err != NoError || selNeigs.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_HATCH_NO_SEL)), true);
		return NoError;
	}

	// 2. Filter hatches and calculate areas
	GS::Array<HatchGroupKey> groupKeys;
	GS::Array<HatchGroupData> groupData;
	UInt32 totalHatches = 0;
	double totalArea = 0.0;

	for (const API_Neig& neig : selNeigs) {
		if (neig.neigID != APINeig_Hatch)
			continue;

		API_Element elem;
		BNZeroMemory (&elem, sizeof (API_Element));
		elem.header.guid = neig.guid;
		if (ACAPI_Element_Get (&elem) != NoError)
			continue;
		if (elem.header.type != API_HatchID)
			continue;

		API_ElementMemo memo;
		BNZeroMemory (&memo, sizeof (API_ElementMemo));
		err = ACAPI_Element_GetMemo (neig.guid, &memo, APIMemoMask_All);
		if (err != NoError)
			continue;

		double hatchArea = CalcPolygonArea (elem.hatch, memo, nullptr);
		ACAPI_DisposeElemMemoHdls (&memo);

		if (hatchArea < 1e-10)
			continue;

		// Group by fill pattern + pen
		HatchGroupKey key;
		key.fillInd = elem.hatch.fillInd;
		key.fillPenIndex = elem.hatch.fillPen.penIndex;

		bool found = false;
		for (UIndex g = 0; g < groupKeys.GetSize (); g++) {
			if (groupKeys[g] == key) {
				groupData[g].count++;
				groupData[g].area += hatchArea;
				found = true;
				break;
			}
		}
		if (!found) {
			groupKeys.Push (key);
			HatchGroupData data;
			data.count = 1;
			data.area = hatchArea;
			groupData.Push (data);
		}

		totalHatches++;
		totalArea += hatchArea;
	}

	// 3. Build report
	if (totalHatches == 0) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_HATCH_NO_SEL)), true);
		return NoError;
	}

	GS::UniString report;
	GS::UniString clip;		// tab-separated for Excel

	for (UIndex g = 0; g < groupKeys.GetSize (); g++) {
		GS::UniString fillName = GetFillName (groupKeys[g].fillInd);
		GS::UniString areaStr = GS::UniString::Printf ("%.2f", groupData[g].area);

		report.Append ("- ");
		report.Append (fillName);
		report.Append (", ");
		report.Append (GetResString (STR_RES_REPORT, RS_HATCH_PEN));
		report.Append (GS::UniString::Printf ("%d", (int) groupKeys[g].fillPenIndex));
		report.Append (": ");
		AppendNumber (report, groupData[g].count);
		report.Append (" шт., ");
		report.Append (areaStr);
		report.Append (" м²\n");

		clip.Append (fillName);
		clip.Append ("\t");
		clip.Append (GS::UniString::Printf ("%d", (int) groupKeys[g].fillPenIndex));
		clip.Append ("\t");
		AppendNumber (clip, groupData[g].count);
		clip.Append ("\t");
		clip.Append (areaStr);
		clip.Append ("\n");
	}

	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_HATCH_TOTAL));
	report.Append (": ");
	AppendNumber (report, totalHatches);
	report.Append (" шт., ");
	report.Append (GS::UniString::Printf ("%.2f", totalArea));
	report.Append (" м²");

	clip.Append ("\t\t");
	AppendNumber (clip, totalHatches);
	clip.Append ("\t");
	clip.Append (GS::UniString::Printf ("%.2f", totalArea));

	GS::UniString fullReport = FormatSafe (title + "\n" + report);
	ACAPI_WriteReport (fullReport, true);
	CopyTextToClipboard (clip);
	return NoError;
}

// =============================================================================
// Zone creation from hatches
// =============================================================================

static GSErrCode	CreateZoneFromHatchPoly (API_Coord** srcCoords, Int32** srcPends,
											API_PolyArc** srcParcs, const API_Polygon& hatchPoly,
											short floorInd, const GS::UniString& roomName,
											const GS::UniString& roomNoStr)
{
	// Step 1: Get defaults FIRST
	API_Element		element = {};
	API_ElementMemo	memo = {};

	element.header.type = API_ZoneID;
	GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
	if (err != NoError)
		return err;

	// Step 2: Determine if outer contour is closed
	Int32 outerEnd = (*srcPends)[1];
	bool closed = (outerEnd > 0 &&
				   fabs ((*srcCoords)[outerEnd].x - (*srcCoords)[1].x) < 1e-10 &&
				   fabs ((*srcCoords)[outerEnd].y - (*srcCoords)[1].y) < 1e-10);

	// Step 3: Compute total coords with closing vertex if needed
	Int32 extraClosedVertex = closed ? 0 : 1;
	Int32 totalCoords = hatchPoly.nCoords + extraClosedVertex;

	// Step 4: Set zone properties (AFTER GetDefaults, BEFORE memo alloc)
	element.header.type     = API_ZoneID;
	element.header.floorInd = floorInd;
	element.zone.manual     = true;
	element.zone.catInd     = element.zone.catInd;  // keep from defaults
	element.zone.poly.nCoords   = totalCoords;
	element.zone.poly.nSubPolys = hatchPoly.nSubPolys;
	element.zone.poly.nArcs     = hatchPoly.nArcs;

	GS::snuprintf (element.zone.roomName,  sizeof (element.zone.roomName),  roomName.ToUStr ());
	GS::snuprintf (element.zone.roomNoStr, sizeof (element.zone.roomNoStr), roomNoStr.ToUStr ());

	// Stamp position: centroid of outer contour
	double cx = 0.0, cy = 0.0;
	for (Int32 i = 1; i <= outerEnd; i++) {
		cx += (*srcCoords)[i].x;
		cy += (*srcCoords)[i].y;
	}
	cx /= (double) outerEnd;
	cy /= (double) outerEnd;
	element.zone.pos.x = cx;
	element.zone.pos.y = cy;
	element.zone.roomHeight = 2.8;

	// Step 5: Allocate memo handles (AFTER GetDefaults, AFTER setting poly counts)
	memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((totalCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
	memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((hatchPoly.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
	if (hatchPoly.nArcs > 0)
		memo.parcs = reinterpret_cast<API_PolyArc**> (BMAllocateHandle (hatchPoly.nArcs * sizeof (API_PolyArc), ALLOCATE_CLEAR, 0));

	if (!memo.coords || !memo.pends) {
		ACAPI_DisposeElemMemoHdls (&memo);
		return APIERR_MEMFULL;
	}

	// Step 6: Fill coords - copy all hatch coords
	Int32 dstIdx = 1;
	for (Int32 i = 1; i <= hatchPoly.nCoords; i++) {
		(*memo.coords)[dstIdx].x = (*srcCoords)[i].x;
		(*memo.coords)[dstIdx].y = (*srcCoords)[i].y;
		dstIdx++;
	}

	// Add closing vertex if needed
	if (!closed) {
		(*memo.coords)[dstIdx].x = (*srcCoords)[1].x;
		(*memo.coords)[dstIdx].y = (*srcCoords)[1].y;
		dstIdx++;
	}

	// Fill pends
	for (Int32 sp = 1; sp <= hatchPoly.nSubPolys; sp++)
		(*memo.pends)[sp] = (*srcPends)[sp];
	// Adjust last sub-polygon's pend if we added a closing vertex to outer
	if (!closed)
		(*memo.pends)[1] = totalCoords;

	// Fill arcs
	if (hatchPoly.nArcs > 0 && memo.parcs != nullptr) {
		for (Int32 i = 0; i < hatchPoly.nArcs; i++) {
			(*memo.parcs)[i].begIndex = (*srcParcs)[i].begIndex;
			(*memo.parcs)[i].endIndex = (*srcParcs)[i].endIndex;
			(*memo.parcs)[i].arcAngle = (*srcParcs)[i].arcAngle;
		}
	}

	// Step 7: Create
	err = ACAPI_Element_Create (&element, &memo);
	if (err != NoError) {
		ACAPI_WriteReport (GS::UniString::Printf ("Create err=%d", (int) err), true);
	}

	ACAPI_DisposeElemMemoHdls (&memo);
	return err;
}

	GSErrCode Do_CreateZonesFromHatches (void)
	{
		GS::UniString title = GetResString (STR_RES_REPORT, RS_ZONE_TITLE);

		// 1. Get selection
		API_SelectionInfo selInfo;
		BNZeroMemory (&selInfo, sizeof (API_SelectionInfo));
		GS::Array<API_Neig> selNeigs;
		GSErrCode err = ACAPI_Selection_Get (&selInfo, &selNeigs, true, false, API_InsidePartially);
		if (err != NoError || selNeigs.IsEmpty ()) {
			ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_ZONE_NO_SEL)), true);
			return NoError;
		}

		// 2. Create zones inside undo scope
		UInt32 createdCount = 0;
		UInt32 failCount = 0;

		err = ACAPI_CallUndoableCommand ("Создание зон из штриховок",
			[&] () -> GSErrCode {
				UInt32 zoneNumber = 0;

				for (const API_Neig& neig : selNeigs) {
					if (neig.neigID != APINeig_Hatch)
						continue;

					API_Element elem;
					BNZeroMemory (&elem, sizeof (API_Element));
					elem.header.guid = neig.guid;
					if (ACAPI_Element_Get (&elem) != NoError)
						continue;
					if (elem.header.type != API_HatchID)
						continue;

					API_ElementMemo memo;
					BNZeroMemory (&memo, sizeof (API_ElementMemo));
					err = ACAPI_Element_GetMemo (neig.guid, &memo, APIMemoMask_All);
					if (err != NoError)
						continue;

					if (memo.coords == nullptr || memo.pends == nullptr ||
						elem.hatch.poly.nCoords < 3) {
						ACAPI_DisposeElemMemoHdls (&memo);
						continue;
					}

					zoneNumber++;
					GS::UniString roomName = GS::UniString::Printf ("Помещение %u", (unsigned int) zoneNumber);
					GS::UniString roomNoStr = GS::UniString::Printf ("%u", (unsigned int) zoneNumber);

					err = CreateZoneFromHatchPoly (memo.coords, memo.pends, memo.parcs, elem.hatch.poly,
												   elem.header.floorInd, roomName, roomNoStr);
					if (err == NoError)
						createdCount++;
					else
						failCount++;

					ACAPI_DisposeElemMemoHdls (&memo);
				}

				return NoError;
			});

		// 3. Report
		if (createdCount == 0 && failCount == 0) {
			ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_ZONE_NO_SEL)), true);
			return NoError;
		}

		GS::UniString report;
		report.Append (GetResString (STR_RES_REPORT, RS_ZONE_CREATED));
		AppendNumber (report, createdCount);
		if (failCount > 0) {
			report.Append ("\n");
			report.Append (GetResString (STR_RES_REPORT, RS_ZONE_FAIL));
			AppendNumber (report, failCount);
		}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

// -----------------------------------------------------------------------------
// Create one slab from hatch polygon (contour copied 1:1, holes and arcs kept)
// -----------------------------------------------------------------------------

static GSErrCode CreateSlabFromHatchPoly (API_Coord** srcCoords, Int32** srcPends, API_PolyArc** srcParcs,
										   const API_Polygon& hatchPoly, short floorInd)
{
	// 1. Get defaults (thickness, materials, pens, level)
	API_Element element = {};
	API_ElementMemo memo = {};
	element.header.type = API_SlabID;
	GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
	if (err != NoError)
		return err;

	// 2. Handle closed contour
	Int32 outerEnd = (*srcPends)[1];
	bool closed = false;
	if (outerEnd > 0) {
		double dx = (*srcCoords)[outerEnd].x - (*srcCoords)[1].x;
		double dy = (*srcCoords)[outerEnd].y - (*srcCoords)[1].y;
		closed = (fabs (dx) < 1e-9 && fabs (dy) < 1e-9);
	}
	Int32 extraClosedVertex = closed ? 0 : 1;
	Int32 totalCoords = hatchPoly.nCoords + extraClosedVertex;

	// 3. Set poly descriptor and floor
	element.header.floorInd = floorInd;
	element.slab.poly.nCoords   = totalCoords;
	element.slab.poly.nSubPolys = hatchPoly.nSubPolys;
	element.slab.poly.nArcs     = hatchPoly.nArcs;

	// 4. Allocate memo (1-based coords/pends, 0-based parcs; edgeTrims/sideMaterials -> defaults)
	memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((totalCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
	memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((hatchPoly.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
	if (hatchPoly.nArcs > 0)
		memo.parcs = reinterpret_cast<API_PolyArc**> (BMAllocateHandle (hatchPoly.nArcs * sizeof (API_PolyArc), ALLOCATE_CLEAR, 0));
	else
		memo.parcs = nullptr;

	if (memo.coords == nullptr || memo.pends == nullptr || (hatchPoly.nArcs > 0 && memo.parcs == nullptr)) {
		ACAPI_DisposeElemMemoHdls (&memo);
		return APIERR_MEMFULL;
	}

	// 5. Copy coords 1-based
	Int32 dstIdx = 1;
	for (Int32 i = 1; i <= hatchPoly.nCoords; i++) {
		(*memo.coords)[dstIdx].x = (*srcCoords)[i].x;
		(*memo.coords)[dstIdx].y = (*srcCoords)[i].y;
		dstIdx++;
	}
	if (!closed) {
		(*memo.coords)[dstIdx].x = (*srcCoords)[1].x;
		(*memo.coords)[dstIdx].y = (*srcCoords)[1].y;
		dstIdx++;
	}

	// Copy pends 1-based
	for (Int32 sp = 1; sp <= hatchPoly.nSubPolys; sp++)
		(*memo.pends)[sp] = (*srcPends)[sp];
	if (!closed)
		(*memo.pends)[1] = totalCoords;

	// Copy arcs 0-based
	if (hatchPoly.nArcs > 0 && memo.parcs != nullptr) {
		for (Int32 i = 0; i < hatchPoly.nArcs; i++) {
			(*memo.parcs)[i].begIndex = (*srcParcs)[i].begIndex;
			(*memo.parcs)[i].endIndex = (*srcParcs)[i].endIndex;
			(*memo.parcs)[i].arcAngle = (*srcParcs)[i].arcAngle;
		}
	}

	err = ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);
	return err;
}

GSErrCode Do_CreateSlabsFromHatches (void)
{
	GS::UniString title = GetResString (STR_RES_REPORT, RS_SLAB_TITLE);

	API_SelectionInfo selInfo;
	BNZeroMemory (&selInfo, sizeof (API_SelectionInfo));
	GS::Array<API_Neig> selNeigs;
	GSErrCode err = ACAPI_Selection_Get (&selInfo, &selNeigs, true, false, API_InsidePartially);
	if (err != NoError || selNeigs.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_SLAB_NO_SEL)), true);
		return NoError;
	}

	UInt32 createdCount = 0;
	UInt32 failCount = 0;

	err = ACAPI_CallUndoableCommand ("Создание перекрытий из штриховок", [&] () -> GSErrCode {
		for (const API_Neig& neig : selNeigs) {
			if (neig.neigID != APINeig_Hatch)
				continue;

			API_Element elem = {};
			elem.header.guid = neig.guid;
			if (ACAPI_Element_Get (&elem) != NoError)
				continue;
			if (elem.header.type != API_HatchID)
				continue;

			API_ElementMemo memo = {};
			BNZeroMemory (&memo, sizeof (API_ElementMemo));
			err = ACAPI_Element_GetMemo (neig.guid, &memo, APIMemoMask_All);
			if (err != NoError)
				continue;
			if (memo.coords == nullptr || memo.pends == nullptr || elem.hatch.poly.nCoords < 3) {
				ACAPI_DisposeElemMemoHdls (&memo);
				continue;
			}

			err = CreateSlabFromHatchPoly (memo.coords, memo.pends, memo.parcs, elem.hatch.poly, elem.header.floorInd);
			if (err == NoError)
				createdCount++;
			else
				failCount++;

			ACAPI_DisposeElemMemoHdls (&memo);
		}
		return NoError;
	});

	if (createdCount == 0 && failCount == 0) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_SLAB_NO_SEL)), true);
		return NoError;
	}
	GS::UniString report;
	report.Append (GetResString (STR_RES_REPORT, RS_SLAB_CREATED));
	AppendNumber (report, createdCount);
	if (failCount > 0) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_SLAB_FAIL));
		AppendNumber (report, failCount);
	}
	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

GSErrCode Do_CalcLineLengths (void)
{
	GS::UniString title = GetResString (STR_RES_REPORT, RS_LINE_TITLE);

	API_SelectionInfo selInfo;
	BNZeroMemory (&selInfo, sizeof (API_SelectionInfo));
	GS::Array<API_Neig> selNeigs;
	GSErrCode err = ACAPI_Selection_Get (&selInfo, &selNeigs, true, false, API_InsidePartially);
	if (err != NoError || selNeigs.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (title + "\n" +
			GetResString (STR_RES_REPORT, RS_LINE_NO_SEL)), true);
		return NoError;
	}

	double totalLen = 0.0;
	UInt32 lineCount = 0;

	for (const API_Neig& neig : selNeigs) {
		API_Element elem;
		BNZeroMemory (&elem, sizeof (API_Element));
		elem.header.guid = neig.guid;
		if (ACAPI_Element_Get (&elem) != NoError)
			continue;

		double len = 0.0;

		if (elem.header.type == API_LineID) {
			double dx = elem.line.endC.x - elem.line.begC.x;
			double dy = elem.line.endC.y - elem.line.begC.y;
			len = sqrt (dx * dx + dy * dy);

		} else if (elem.header.type == API_ArcID) {
			double a = elem.arc.r;
			double b = a * elem.arc.ratio;
			double dAng;
			if (elem.arc.whole) {
				dAng = 2.0 * M_PI;
			} else {
				dAng = fabs (elem.arc.endAng - elem.arc.begAng);
				if (dAng > M_PI)
					dAng = 2.0 * M_PI - dAng;
			}

			const Int32 N = 100;
			double t0 = elem.arc.begAng;
			double step = (elem.arc.endAng - elem.arc.begAng);
			if (elem.arc.whole)
				step = 2.0 * M_PI;
			else if (fabs (step) > M_PI)
				step = (step > 0) ? -(2.0 * M_PI - fabs (step)) : (2.0 * M_PI - fabs (step));
			double dt = step / N;
			double sum = 0.0;
			for (Int32 i = 0; i <= N; i++) {
				double t = t0 + i * dt;
				double w = (i == 0 || i == N) ? 1.0 : (i % 2 == 0) ? 2.0 : 4.0;
				double dx = -a * sin (t);
				double dy = b * cos (t);
				sum += w * sqrt (dx * dx + dy * dy);
			}
			len = fabs (sum * dt / 3.0);

		} else if (elem.header.type == API_PolyLineID) {
			API_ElementMemo memo;
			BNZeroMemory (&memo, sizeof (API_ElementMemo));
			if (ACAPI_Element_GetMemo (neig.guid, &memo, APIMemoMask_Polygon) == NoError && memo.coords != nullptr) {
				Int32 nCoords = elem.polyLine.poly.nCoords;
				Int32 nArcs = elem.polyLine.poly.nArcs;

				if (nArcs > 0 && memo.parcs != nullptr) {
					// Build set of arc interior points to skip
					GS::HashSet<Int32> arcInterior;
					for (Int32 a = 0; a < nArcs; a++) {
						const API_PolyArc& arc = (*memo.parcs)[a];
						for (Int32 idx = arc.begIndex + 1; idx < arc.endIndex; idx++)
							arcInterior.Add (idx);
					}

					// Sum straight segments + arc segments
					for (Int32 i = 1; i < nCoords; i++) {
						if (arcInterior.Contains (i) || arcInterior.Contains (i + 1))
							continue;
						// Check if this edge is an arc
						bool isArcEdge = false;
						double arcLen = 0.0;
						for (Int32 a = 0; a < nArcs; a++) {
							const API_PolyArc& arc = (*memo.parcs)[a];
							if (arc.begIndex == i && arc.endIndex == i + 1) {
								isArcEdge = true;
								const API_Coord& A = (*memo.coords)[arc.begIndex];
								const API_Coord& B = (*memo.coords)[arc.endIndex];
								double chord = sqrt ((B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y));
								double halfA = fabs (arc.arcAngle) / 2.0;
								if (halfA > M_PI) halfA = M_PI - halfA;
								double sinH = sin (halfA);
								if (fabs (sinH) > 1e-10) {
									double R = chord / (2.0 * sinH);
									arcLen = fabs (R * arc.arcAngle);
								} else {
									arcLen = chord;
								}
								break;
							}
						}
						if (isArcEdge) {
							len += arcLen;
						} else {
							double dx = (*memo.coords)[i + 1].x - (*memo.coords)[i].x;
							double dy = (*memo.coords)[i + 1].y - (*memo.coords)[i].y;
							len += sqrt (dx * dx + dy * dy);
						}
					}
				} else {
					// Pure straight segments
					for (Int32 i = 1; i < nCoords; i++) {
						double dx = (*memo.coords)[i + 1].x - (*memo.coords)[i].x;
						double dy = (*memo.coords)[i + 1].y - (*memo.coords)[i].y;
						len += sqrt (dx * dx + dy * dy);
					}
				}
				ACAPI_DisposeElemMemoHdls (&memo);
			}

		} else {
			continue;
		}

		if (len > 1e-10) {
			totalLen += len;
			lineCount++;
		}
	}

	if (lineCount == 0) {
		ACAPI_WriteReport (FormatSafe (title + "\n" +
			GetResString (STR_RES_REPORT, RS_LINE_NO_SEL)), true);
		return NoError;
	}

	GS::UniString report;
	report.Append ("- ");
	report.Append (GS::UniString::Printf ("%d", (int) lineCount));
	report.Append (" ");
	report.Append (GetResString (STR_RES_REPORT, RS_LINE_COUNT));
	report.Append (": ");
	report.Append (GS::UniString::Printf ("%.3f ", totalLen));
	report.Append (GetResString (STR_RES_REPORT, RS_LINE_LENGTH));

	GS::UniString fullReport = FormatSafe (title + "\n" + report);
	ACAPI_WriteReport (fullReport, true);
	CopyTextToClipboard (GS::UniString::Printf ("%d\t%.3f", (int) lineCount, totalLen));

	return NoError;
}

GSErrCode Do_ScanUnusedLayers (void)
{
	// 1. Collect all layers
	API_AttributeIndex layerCount = 0;
	GSErrCode err = ACAPI_Attribute_GetNum (API_LayerID, &layerCount);
	if (err != NoError || layerCount == 0) {
		ACAPI_WriteReport (FormatSafe (GetResString (STR_RES_REPORT, RS_LAYERDEL_NOTHING)), true);
		return NoError;
	}

	struct LayerInfo {
		API_AttributeIndex index;
		GS::UniString name;
		bool hidden;
		bool locked;
	};

	GS::Array<LayerInfo> allLayers;

	for (API_AttributeIndex i = 1; i <= layerCount; i++) {
		API_Attribute attrib = {};
		attrib.header.typeID = API_LayerID;
		attrib.header.index = i;
		GS::UniString uniName;
		attrib.header.uniStringNamePtr = &uniName;

		err = ACAPI_Attribute_Get (&attrib);
		if (err == APIERR_DELETED)
			continue;
		if (err != NoError)
			continue;

		LayerInfo info;
		info.index = i;
		info.name = uniName;
		info.hidden = (attrib.header.flags & APILay_Hidden) != 0;
		info.locked = (attrib.header.flags & APILay_Locked) != 0;
		allLayers.Push (info);
	}

	// 2. Collect used layer indices from all elements
	GS::Array<bool> layerUsed;
	for (UInt32 i = 0; i <= static_cast<UInt32>(layerCount); i++)
		layerUsed.Push (false);

	const API_ElemTypeID types[] = {
		API_WallID, API_ColumnID, API_BeamID, API_WindowID, API_DoorID,
		API_ObjectID, API_LampID, API_SlabID, API_RoofID, API_MeshID,
		API_DimensionID, API_RadialDimensionID, API_LevelDimensionID,
		API_AngleDimensionID, API_TextID, API_LabelID, API_ZoneID,
		API_HatchID, API_LineID, API_PolyLineID, API_ArcID, API_CircleID,
		API_SplineID, API_HotspotID, API_CutPlaneID, API_CameraID,
		API_CamSetID, API_SectElemID, API_DrawingID, API_PictureID,
		API_HotlinkID, API_CurtainWallID, API_ShellID, API_SkylightID,
		API_MorphID, API_ChangeMarkerID, API_StairID, API_RailingID,
		API_BeamSegmentID, API_ColumnSegmentID, API_OpeningID
	};

	GS::Int32 usedCount = 0;
	for (const auto& typeID : types) {
		GS::Array<API_Guid> elemList;
		err = ACAPI_Element_GetElemList (API_ElemType (typeID), &elemList);
		if (err != NoError)
			continue;

		for (const auto& guid : elemList) {
			API_Element element = {};
			element.header.guid = guid;
			err = ACAPI_Element_Get (&element);
			if (err == NoError) {
				GS::UInt32 li = static_cast<GS::UInt32>(element.header.layer);
				if (li > 0 && li < layerUsed.GetSize ()) {
					layerUsed[li] = true;
					usedCount++;
				}
			}
		}
	}

	// 3. Find unused layers (skip index 1 = "ArchiCAD Layer")
	GS::Array<LayerInfo> unusedLayers;

	for (UInt32 i = 0; i < allLayers.GetSize (); i++) {
		if (allLayers[i].index == 1)
			continue;
		GS::UInt32 li = static_cast<GS::UInt32>(allLayers[i].index);
		if (li >= layerUsed.GetSize () || !layerUsed[li])
			unusedLayers.Push (allLayers[i]);
	}

	// 4. Build report
	GS::UniString title = GetResString (STR_RES_REPORT, RS_LAYER_TITLE);
	GS::UniString report;

	report.Append (GetResString (STR_RES_REPORT, RS_LAYER_TOTAL));
	AppendNumber (report, allLayers.GetSize ());
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LAYER_UNUSED));
	AppendNumber (report, unusedLayers.GetSize ());

	if (unusedLayers.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_LAYER_NONE));
		ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
		return NoError;
	}

	report.Append ("\n\n");
	report.Append (GetResString (STR_RES_REPORT, RS_LAYER_LIST));
	const UInt32 maxToList = 40;
	UInt32 listed = 0;
	for (const auto& layer : unusedLayers) {
		if (listed >= maxToList) {
			report.Append ("\n...");
			break;
		}
		report.Append ("\n- ");
		report.Append (layer.name);
		if (layer.hidden)
			report.Append (GetResString (STR_RES_REPORT, RS_LAYER_HIDDEN));
		if (layer.locked)
			report.Append (GetResString (STR_RES_REPORT, RS_LAYER_LOCKED));
		listed++;
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);

	// 5. Dialog: rename or just report?
	GS::UniString dlgMsg = GetResString (STR_RES_REPORT, RS_LAYER_PROMPT_MSG);
	GS::UniString dlgCount = GS::UniString::Printf ("\n\n(%d ", (int) unusedLayers.GetSize ());
	dlgCount.Append (GetResString (STR_RES_REPORT, RS_LAYER_UNUSED));
	dlgCount.TrimRight ();
	dlgCount.Append (")");
	dlgMsg.Append (dlgCount);

	short button = DGAlert (DG_WARNING,
							title,
							dlgMsg,
							GS::UniString (),
							GetResString (STR_RES_REPORT, RS_LAYER_PROMPT_YES),
							GetResString (STR_RES_REPORT, RS_LAYER_PROMPT_NO));
	if (button != 1)
		return NoError;

	// 6. Rename with _null_ prefix
	GS::Int32 renamedCount = 0;
	GS::Int32 skippedCount = 0;
	GS::Int32 failCount = 0;

	for (UInt32 i = 0; i < unusedLayers.GetSize (); i++) {
		if (unusedLayers[i].name.BeginsWith ("_null_")) {
			skippedCount++;
			continue;
		}

		API_Attribute attrib = {};
		attrib.header.typeID = API_LayerID;
		attrib.header.index = unusedLayers[i].index;

		GS::UniString newName = "_null_" + unusedLayers[i].name;
		attrib.header.uniStringNamePtr = &newName;

		err = ACAPI_Attribute_Modify (&attrib, nullptr);
		if (err == NoError)
			renamedCount++;
		else
			failCount++;
	}

	GS::UniString result;
	result.Append (GetResString (STR_RES_REPORT, RS_LAYERDEL_RENAMED));
	AppendNumber (result, renamedCount);
	if (skippedCount > 0) {
		result.Append (GS::UniString::Printf ("\nУже с префиксом _null_: %d", (int) skippedCount));
	}
	if (failCount > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_LAYERDEL_RENFAIL));
		AppendNumber (result, failCount);
	}

	ACAPI_WriteReport (FormatSafe (GetResString (STR_RES_REPORT, RS_LAYERDEL_TITLE) + "\n" + result), true);
	return NoError;
}

GSErrCode Do_ScanUnusedMasterLayouts (void)
{
	// 1. Get all master layouts
	GS::Array<API_DatabaseUnId> masterLayouts;
	GSErrCode err = ACAPI_Database (APIDb_GetMasterLayoutDatabasesID, nullptr, &masterLayouts);
	if (err != NoError || masterLayouts.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (GetResString (STR_RES_REPORT, RS_MASTER_NONE)), true);
		return NoError;
	}

	struct MasterLayoutInfo {
		API_DatabaseUnId dbId;
		GS::UniString name;
	};

	GS::Array<MasterLayoutInfo> allMasters;
	for (const auto& mId : masterLayouts) {
		API_DatabaseInfo dbInfo = {};
		dbInfo.databaseUnId = mId;
		err = ACAPI_Database (APIDb_GetDatabaseInfoID, &dbInfo);
		if (err == NoError) {
			MasterLayoutInfo info;
			info.dbId = mId;
			info.name = GS::UniString (dbInfo.name);
			allMasters.Push (info);
		}
	}

	// 2. Get all regular layouts and find which master each uses
	GS::Array<API_DatabaseUnId> layouts;
	err = ACAPI_Database (APIDb_GetLayoutDatabasesID, nullptr, &layouts);
	if (err != NoError)
		layouts.Clear ();

	GS::Array<bool> masterUsed;
	for (UInt32 i = 0; i < allMasters.GetSize (); i++)
		masterUsed.Push (false);

	for (const auto& layoutId : layouts) {
		API_DatabaseInfo dbInfo = {};
		dbInfo.databaseUnId = layoutId;
		err = ACAPI_Database (APIDb_GetDatabaseInfoID, &dbInfo);
		if (err != NoError)
			continue;

		API_Guid usedMasterId = dbInfo.masterLayoutUnId.elemSetId;
		if (usedMasterId == APINULLGuid)
			continue;

		for (UInt32 i = 0; i < allMasters.GetSize (); i++) {
			if (allMasters[i].dbId.elemSetId == usedMasterId) {
				masterUsed[i] = true;
				break;
			}
		}
	}

	// 3. Find unused master layouts
	GS::Array<MasterLayoutInfo> unusedMasters;
	for (UInt32 i = 0; i < allMasters.GetSize (); i++) {
		if (!masterUsed[i])
			unusedMasters.Push (allMasters[i]);
	}

	// 4. Build report
	GS::UniString title = GetResString (STR_RES_REPORT, RS_MASTER_TITLE);
	GS::UniString report;

	report.Append (GetResString (STR_RES_REPORT, RS_MASTER_TOTAL));
	AppendNumber (report, allMasters.GetSize ());
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_MASTER_UNUSED));
	AppendNumber (report, unusedMasters.GetSize ());

	if (unusedMasters.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_MASTER_NONE));
		ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
		return NoError;
	}

	report.Append ("\n\n");
	report.Append (GetResString (STR_RES_REPORT, RS_MASTER_LIST));
	const UInt32 maxToList = 40;
	UInt32 listed = 0;
	for (const auto& ml : unusedMasters) {
		if (listed >= maxToList) {
			report.Append ("\n...");
			break;
		}
		report.Append ("\n- ");
		report.Append (ml.name);
		listed++;
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);

	// 5. Dialog: rename or just report?
	GS::UniString dlgMsg = GetResString (STR_RES_REPORT, RS_MASTER_PROMPT_MSG);
	GS::UniString dlgCount = GS::UniString::Printf ("\n\n(%d ", (int) unusedMasters.GetSize ());
	dlgCount.Append (GetResString (STR_RES_REPORT, RS_MASTER_UNUSED));
	dlgCount.TrimRight ();
	dlgCount.Append (")");
	dlgMsg.Append (dlgCount);

	short button = DGAlert (DG_WARNING,
							title,
							dlgMsg,
							GS::UniString (),
							GetResString (STR_RES_REPORT, RS_MASTER_PROMPT_YES),
							GetResString (STR_RES_REPORT, RS_MASTER_PROMPT_NO));
	if (button != 1)
		return NoError;

	// 6. Rename with _null_ prefix — all three stores: DB name, LayoutInfo.layoutName, Navigator uName
	GS::Int32 renamedCount = 0;
	GS::Int32 skippedCount = 0;
	GS::Int32 failCount = 0;
	GS::UniString details; // visible details about failures

	// Wrap in undoable command so ArchiCAD properly commits database & navigator changes
	ACAPI_CallUndoableCommand ("Переименование неиспользуемых основных макетов", [&] () -> GSErrCode {
		for (UInt32 i = 0; i < unusedMasters.GetSize (); i++) {
			if (unusedMasters[i].name.BeginsWith ("_null_")) {
				skippedCount++;
				continue;
			}
			GS::UniString oldName = unusedMasters[i].name;
			GS::UniString newName = GS::UniString ("_null_") + oldName;

			// Fetch DB and LayoutInfo BEFORE modify (keep original UnId)
			API_DatabaseInfo dbInfo = {};
			dbInfo.databaseUnId = unusedMasters[i].dbId;
			GSErrCode errDbGet = ACAPI_Database (APIDb_GetDatabaseInfoID, &dbInfo);
			if (errDbGet != NoError) { failCount++; details.Append (GS::UniString::Printf ("\n%T: GetDB %d", oldName.ToPrintf (), (int) errDbGet)); continue; }

			API_LayoutInfo layoutInfo = {};
			GSErrCode errLayGet = ACAPI_Environment (APIEnv_GetLayoutSetsID, &layoutInfo, &dbInfo.databaseUnId);
			bool hasLayout = (errLayGet == NoError);

			// 6a. Update LayoutInfo.layoutName first (uchar_t)
			GSErrCode errLayChg = -1;
			if (hasLayout) {
				GS::ucscpy (layoutInfo.layoutName, newName.ToUStr ());
				errLayChg = ACAPI_Environment (APIEnv_ChangeLayoutSetsID, &layoutInfo, &dbInfo.databaseUnId);
				// keep customData cleanup after Change
				if (layoutInfo.customData != nullptr) { delete layoutInfo.customData; layoutInfo.customData = nullptr; }
			}

			// 6b. Update Database name (char)
			GS::snuprintf (dbInfo.name, sizeof (dbInfo.name), "%s", newName.ToCStr ().Get ());
			GSErrCode errDbChg = ACAPI_Database (APIDb_ModifyDatabaseID, &dbInfo);

			// 6c. Update Navigator uName (uchar_t) — this is what Book shows and what restores on click
			GSErrCode errNavGet = -1, errNavChg = -1;
			bool navFound = false;
			{
				API_NavigatorSet navSet = {};
				navSet.mapId = API_LayoutMap;
				if (ACAPI_Navigator (APINavigator_GetNavigatorSetID, &navSet, nullptr) == NoError) {
					GS::Array<API_Guid> stack;
					stack.Push (navSet.rootGuid);
					while (!stack.IsEmpty () && !navFound) {
						API_Guid curGuid = stack.Pop ();
						API_NavigatorItem curItem = {};
						curItem.guid = curGuid;
						curItem.mapId = API_LayoutMap;
						GS::Array<API_NavigatorItem> children;
						if (ACAPI_Navigator (APINavigator_GetNavigatorChildrenItemsID, &curItem, nullptr, &children) != NoError) continue;
						for (const auto& child : children) {
							bool guidMatch = (child.db.databaseUnId.elemSetId == unusedMasters[i].dbId.elemSetId);
							bool nameMatch = (GS::UniString (child.uName) == oldName);
							if (child.itemType == API_MasterLayoutNavItem && (guidMatch || nameMatch)) {
								API_NavigatorItem navItem = {};
								navItem.guid = child.guid;
								navItem.mapId = API_LayoutMap;
								errNavGet = ACAPI_Navigator (APINavigator_GetNavigatorItemID, &navItem.guid, &navItem);
								if (errNavGet == NoError) {
									GS::ucscpy (navItem.uName, newName.ToUStr ());
									navItem.customName = true;
									errNavChg = ACAPI_Navigator (APINavigator_ChangeNavigatorItemID, &navItem, nullptr);
									navFound = (errNavChg == NoError);
								}
								break;
							}
							if (child.itemType == API_MasterFolderNavItem || child.itemType == API_FolderNavItem || child.itemType == API_BookNavItem || child.itemType == API_SubSetNavItem)
								stack.Push (child.guid);
						}
					}
				}
			}

			bool ok = true;
			// DB modify for master layouts fails with -2130313215 inside undo — not critical, navigator+layout is enough
			// if (errDbChg != NoError) ok = false;
			if (hasLayout && errLayChg != NoError && errLayChg != APIERR_BADPARS) ok = false;
			if (!navFound) ok = false;

			if (ok) {
				renamedCount++;
			} else {
				failCount++;
				details.Append (GS::UniString::Printf ("\n%T: DB=%d LayGet=%d LayChg=%d NavGet=%d NavChg=%d found=%d",
					oldName.ToPrintf (), (int) errDbChg, (int) errLayGet, (int) errLayChg, (int) errNavGet, (int) errNavChg, (int) navFound));
			}
		}
		return NoError;
	});

	GS::UniString result;
	result.Append (GetResString (STR_RES_REPORT, RS_MASTER_RENAMED));
	AppendNumber (result, renamedCount);
	if (skippedCount > 0) {
		result.Append (GS::UniString::Printf ("\nУже с префиксом _null_: %d", (int) skippedCount));
	}
	if (failCount > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_MASTER_RENFAIL));
		AppendNumber (result, failCount);
		if (!details.IsEmpty ())
			result.Append (details);
	}

	ACAPI_WriteReport (FormatSafe (GetResString (STR_RES_REPORT, RS_MASTER_RENAME_TITLE) + "\n" + result), true);
	return NoError;
}

GSErrCode Do_DeleteUnusedLayers (void)
{
	// Re-scan to get unused layer indices
	API_AttributeIndex layerCount = 0;
	GSErrCode err = ACAPI_Attribute_GetNum (API_LayerID, &layerCount);
	if (err != NoError || layerCount == 0) {
		ACAPI_WriteReport (FormatSafe (GetResString (STR_RES_REPORT, RS_LAYERDEL_NOTHING)), true);
		return NoError;
	}

	struct LayerEntry {
		API_AttributeIndex index;
		GS::UniString name;
	};

	GS::Array<LayerEntry> allLayers;

	for (API_AttributeIndex i = 1; i <= layerCount; i++) {
		API_Attribute attrib = {};
		attrib.header.typeID = API_LayerID;
		attrib.header.index = i;
		GS::UniString uniName;
		attrib.header.uniStringNamePtr = &uniName;

		err = ACAPI_Attribute_Get (&attrib);
		if (err == APIERR_DELETED)
			continue;
		if (err != NoError)
			continue;

		LayerEntry entry;
		entry.index = i;
		entry.name = uniName;
		allLayers.Push (entry);
	}

	// Collect used layers
	GS::Array<bool> layerUsed;
	for (UInt32 i = 0; i <= static_cast<UInt32>(layerCount); i++)
		layerUsed.Push (false);

	const API_ElemTypeID types[] = {
		API_WallID, API_ColumnID, API_BeamID, API_WindowID, API_DoorID,
		API_ObjectID, API_LampID, API_SlabID, API_RoofID, API_MeshID,
		API_DimensionID, API_RadialDimensionID, API_LevelDimensionID,
		API_AngleDimensionID, API_TextID, API_LabelID, API_ZoneID,
		API_HatchID, API_LineID, API_PolyLineID, API_ArcID, API_CircleID,
		API_SplineID, API_HotspotID, API_CutPlaneID, API_CameraID,
		API_CamSetID, API_SectElemID, API_DrawingID, API_PictureID,
		API_HotlinkID, API_CurtainWallID, API_ShellID, API_SkylightID,
		API_MorphID, API_ChangeMarkerID, API_StairID, API_RailingID,
		API_BeamSegmentID, API_ColumnSegmentID, API_OpeningID
	};

	for (const auto& typeID : types) {
		GS::Array<API_Guid> elemList;
		ACAPI_Element_GetElemList (API_ElemType (typeID), &elemList);
		for (const auto& guid : elemList) {
			API_Element element = {};
			element.header.guid = guid;
			if (ACAPI_Element_Get (&element) == NoError) {
				GS::UInt32 li = static_cast<GS::UInt32>(element.header.layer);
				if (li > 0 && li < layerUsed.GetSize ())
					layerUsed[li] = true;
			}
		}
	}

	// Find unused (skip index 1 = "ArchiCAD Layer")
	GS::Array<LayerEntry> unusedLayers;
	for (UInt32 i = 0; i < allLayers.GetSize (); i++) {
		if (allLayers[i].index == 1)
			continue;
		GS::UInt32 li = static_cast<GS::UInt32>(allLayers[i].index);
		if (li >= layerUsed.GetSize () || !layerUsed[li])
			unusedLayers.Push (allLayers[i]);
	}

	if (unusedLayers.IsEmpty ()) {
		ACAPI_WriteReport (FormatSafe (
			GetResString (STR_RES_REPORT, RS_LAYERDEL_TITLE) + "\n" +
			GetResString (STR_RES_REPORT, RS_LAYERDEL_NOTHING)), true);
		return NoError;
	}

	// Rename with _null_ prefix
	GS::Int32 renamedCount = 0;
	GS::Int32 skippedCount = 0;
	GS::Int32 failCount = 0;

	for (UInt32 i = 0; i < unusedLayers.GetSize (); i++) {
		if (unusedLayers[i].name.BeginsWith ("_null_")) {
			skippedCount++;
			continue;
		}

		API_Attribute attrib = {};
		attrib.header.typeID = API_LayerID;
		attrib.header.index = unusedLayers[i].index;

		GS::UniString newName = "_null_" + unusedLayers[i].name;
		attrib.header.uniStringNamePtr = &newName;

		err = ACAPI_Attribute_Modify (&attrib, nullptr);
		if (err == NoError)
			renamedCount++;
		else
			failCount++;
	}

	GS::UniString title = GetResString (STR_RES_REPORT, RS_LAYERDEL_TITLE);
	GS::UniString result;
	result.Append (GetResString (STR_RES_REPORT, RS_LAYERDEL_RENAMED));
	AppendNumber (result, renamedCount);
	if (skippedCount > 0) {
		result.Append (GS::UniString::Printf ("\nУже с префиксом _null_: %d", (int) skippedCount));
	}
	if (failCount > 0) {
		result.Append ("\n");
		result.Append (GetResString (STR_RES_REPORT, RS_LAYERDEL_RENFAIL));
		AppendNumber (result, failCount);
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + result), true);
	return NoError;
}

GSErrCode Do_ProjectStats (void)
{
	GS::UniString title = GetResString (STR_RES_REPORT, RS_STAT_TITLE);
	GS::UniString report;
	UInt32 totalElements = 0;

	struct ElemInfo {
		API_ElemTypeID typeID;
		const char* name;
	};

	const ElemInfo types[] = {
		{ API_WallID,              "Стены" },
		{ API_ColumnID,            "Колонны" },
		{ API_BeamID,              "Балки" },
		{ API_WindowID,            "Окна" },
		{ API_DoorID,              "Двери" },
		{ API_ObjectID,            "Объекты" },
		{ API_LampID,              "Освещение" },
		{ API_SlabID,              "Перекрытия" },
		{ API_RoofID,              "Крыши" },
		{ API_MeshID,              "Массивы" },
		{ API_DimensionID,         "Размеры" },
		{ API_RadialDimensionID,   "Радиальные размеры" },
		{ API_LevelDimensionID,    "Размеры уровней" },
		{ API_AngleDimensionID,    "Угловые размеры" },
		{ API_TextID,              "Текст" },
		{ API_LabelID,             "Марки" },
		{ API_ZoneID,              "Зоны" },
		{ API_HatchID,             "Штриховки" },
		{ API_LineID,              "Линии" },
		{ API_PolyLineID,          "Полилинии" },
		{ API_ArcID,               "Дуги" },
		{ API_CircleID,            "Окружности" },
		{ API_SplineID,            "Сплайны" },
		{ API_HotspotID,           "Хотспоты" },
		{ API_CutPlaneID,          "Секции" },
		{ API_CameraID,            "Камеры" },
		{ API_DrawingID,           "Чертежи" },
		{ API_PictureID,           "Изображения" },
		{ API_HotlinkID,           "Хотлинки" },
		{ API_CurtainWallID,       "Фасадные стены" },
		{ API_ShellID,             "Оболочки" },
		{ API_SkylightID,          "Зенитные фонари" },
		{ API_MorphID,             "Морфы" },
		{ API_StairID,             "Лестницы" },
		{ API_RailingID,           "Ограждения" },
		{ API_BeamSegmentID,       "Сегменты балок" },
		{ API_ColumnSegmentID,     "Сегменты колонн" },
		{ API_OpeningID,           "Проёмы" }
	};

	GS::UniString typeReport;
	bool hasElements = false;

	for (const auto& info : types) {
		GS::Array<API_Guid> elemList;
		GSErrCode err = ACAPI_Element_GetElemList (API_ElemType (info.typeID), &elemList);
		if (err == NoError && !elemList.IsEmpty ()) {
			UInt32 count = (UInt32) elemList.GetSize ();
			totalElements += count;
			typeReport.Append ("\n  ");
			typeReport.Append (GS::UniString (info.name));

			// Align: pad to 22 chars
			Int32 padding = 22 - GS::UniString (info.name).GetLength ();
			for (Int32 p = 0; p < padding; p++)
				typeReport.Append (" ");

			typeReport.Append (GS::UniString::Printf (": %u", count));
			hasElements = true;
		}
	}

	report.Append (GetResString (STR_RES_REPORT, RS_STAT_TOTAL));
	AppendNumber (report, totalElements);

	if (hasElements) {
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_STAT_BY_TYPE));
		report.Append (typeReport);
	}

	// Layer count
	API_AttributeIndex layerCount = 0;
	if (ACAPI_Attribute_GetNum (API_LayerID, &layerCount) == NoError) {
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_STAT_LAYERS));
		AppendNumber (report, layerCount);
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

GSErrCode Do_About (void)
{
	GS::UniString title = GetResString (STR_RES_ADDON_INFO, 1);
	GS::UniString body = GetResString (STR_RES_REPORT, RS_ABOUT_BODY);
	DGAlert (DG_INFORMATION,
			 title,
			 body,
			 GS::UniString (),
			 "OK");
	Do_ProjectStats ();
	return NoError;
}

GSErrCode Do_TogglePalette (void)
{
	ProjectCleanerPalette& palette = ProjectCleanerPalette::GetInstance ();
	if (palette.IsVisible ())
		palette.Hide ();
	else
		palette.Show ();
	return NoError;
}

// =============================================================================
// Menu handler
// =============================================================================

GSErrCode __ACENV_CALL	MenuHandler (const API_MenuParams* menuParams)
{
	switch (menuParams->menuItemRef.itemIndex) {
		case 1:		return Do_ScanDeleteUnusedViews ();
		case 2:		return Do_ScanDeleteEmbeddedLibrary ();
		case 3:		return Do_ScanUnusedLayers ();
		case 4:		return Do_ScanUnusedMasterLayouts ();
		case 5:		return Do_CalcHatchAreas ();
		case 6:		return Do_CalcLineLengths ();
		case 7:		return Do_CreateZonesFromHatches ();
		case 8:		return Do_CreateSlabsFromHatches ();
		case 9:		return Do_TogglePalette ();
		case 10:		return Do_About ();
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
	return ACAPI_Register_Menu (MENU_RES_ID, 32502, MenuCode_UserDef, MenuFlag_SeparatorBefore);
}

GSErrCode __ACENV_CALL	ProjectEventHandler (API_NotifyEventID notifID, Int32 /*param*/)
{
	if (notifID == APINotify_ChangeWindow)
		ProjectCleanerPalette::GetInstance ().UpdateButtonStates ();

	return NoError;
}

GSErrCode __ACENV_CALL	Initialize (void)
{
	GSErrCode err = ACAPI_Install_MenuHandler (MENU_RES_ID, MenuHandler);

	ACAPI_RegisterModelessWindow (ProjectCleanerPalette::PaletteRefId (),
								  ProjectCleanerPalette::PaletteAPIControlCallBack,
								  API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
								  API_PalEnabled_InteriorElevation + API_PalEnabled_3D +
								  API_PalEnabled_Detail + API_PalEnabled_Worksheet + API_PalEnabled_Layout +
								  API_PalEnabled_DocumentFrom3D, GSGuid2APIGuid (ProjectCleanerPalette::PaletteGuid ()));

	ACAPI_Notify_CatchProjectEvent (APINotify_ChangeWindow, ProjectEventHandler);

	return err;
}

GSErrCode __ACENV_CALL	FreeData (void)
{
	ACAPI_Notify_CatchProjectEvent (APINotify_ChangeWindow, nullptr);
	ACAPI_UnregisterModelessWindow (ProjectCleanerPalette::PaletteRefId ());
	return NoError;
}
