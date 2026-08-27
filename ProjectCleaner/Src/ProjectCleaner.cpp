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
#define RS_DUP_TITLE		33	// duplicates scan finished title
#define RS_DUP_TOTAL		34	// "Total embedded parts: "
#define RS_DUP_GROUPS		35	// "Duplicate groups: "
#define RS_DUP_NONE			36	// "No duplicates found."
#define RS_DUP_USED			37	// "used"
#define RS_DUP_UNUSED		38	// "NOT used"
#define RS_DUP_HINT			39	// "Unused duplicates can be deleted via menu item 4."
#define RS_BROKEN_TITLE		40	// broken references report title
#define RS_BROKEN_CHECKED	41	// "Elements checked: "
#define RS_BROKEN_COUNT		42	// "Broken references: "
#define RS_BROKEN_NONE		43	// "No broken references."
#define RS_BROKEN_LIST		44	// "List:"
#define RS_BROKEN_MISSING	45	// "missing library part (index "
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

	// Clean up folders that became empty after the deletion
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
	ACAPI_WriteReport (FormatSafe (title + "\n" + result), true);
	return NoError;
}

// "name(12)" -> "name", "Name.gsm" -> "Name" (base name for duplicate grouping)
static GS::UniString	GetDuplicateBaseName (const GS::UniString& name)
{
	GS::UniString result = name;

	if (result.GetLength () > 4 && result.ToLowerCase ().EndsWith (".gsm"))
		result.Truncate (result.GetLength () - 4);

	UIndex len = result.GetLength ();
	if (len >= 3 && result[len - 1] == ')') {
		UIndex openPos = len - 2;
		while (openPos > 0 && result[openPos].IsDigit ())
			openPos--;
		if (openPos < len - 2 && openPos > 0 && result[openPos] == '(')
			result.Truncate (openPos);
	}
	return result;
}

static GSErrCode Do_ScanDuplicateLibParts (void)
{
	UIndex totalParts = 0;
	bool hasEmbedded = false;
	GS::Array<EmbeddedLibPartInfo> embeddedParts;
	GSErrCode err = CollectEmbeddedLibParts (&embeddedParts, &totalParts, &hasEmbedded);
	if (err != NoError)
		return err;

	GS::UniString title = GetResString (STR_RES_REPORT, RS_DUP_TITLE);
	if (!hasEmbedded) {
		ACAPI_WriteReport (FormatSafe (title + "\n" + GetResString (STR_RES_REPORT, RS_LIB_NO_LIBRARY)), true);
		return NoError;
	}

	// Which embedded parts are referenced by placed elements
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

	// Group by base name (O(n^2) is fine for embedded library sizes)
	GS::Array<GS::UniString> groupBaseNames;
	GS::Array<GS::Array<UIndex>> groups;
	for (UIndex i = 0; i < embeddedParts.GetSize (); i++) {
		GS::UniString base = GetDuplicateBaseName (embeddedParts[i].name);
		bool found = false;
		for (UIndex g = 0; g < groupBaseNames.GetSize (); g++) {
			if (groupBaseNames[g] == base) {
				groups[g].Push (i);
				found = true;
				break;
			}
		}
		if (!found) {
			groupBaseNames.Push (base);
			GS::Array<UIndex> newGroup;
			newGroup.Push (i);
			groups.Push (newGroup);
		}
	}

	GS::Array<GS::Array<UIndex>> duplicateGroups;
	for (const GS::Array<UIndex>& group : groups) {
		if (group.GetSize () >= 2)
			duplicateGroups.Push (group);
	}

	GS::UniString report = GetResString (STR_RES_REPORT, RS_DUP_TOTAL);
	AppendNumber (report, embeddedParts.GetSize ());
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_DUP_GROUPS));
	AppendNumber (report, duplicateGroups.GetSize ());

	if (duplicateGroups.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_DUP_NONE));
	} else {
		report.Append ("\n");
		for (const GS::Array<UIndex>& group : duplicateGroups) {
			report.Append ("\n- ");
			report.Append (groupBaseNames[group[0]]);
			report.Append (": ");
			AppendNumber (report, group.GetSize ());
			for (UIndex memberIdx : group) {
				const EmbeddedLibPartInfo& member = embeddedParts[memberIdx];
				report.Append ("\n    - ");
				report.Append (member.name);
				report.Append (" [");
				report.Append (GetResString (STR_RES_REPORT, usedLibInds.Contains (member.index) ? RS_DUP_USED : RS_DUP_UNUSED));
				report.Append ("]");
			}
		}
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_DUP_HINT));
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
	return NoError;
}

struct LibElemRef {
	Int32				libInd;
	API_Guid			elemGuid;
	API_ElemTypeID		elemType;
};

static void	CollectLibRefs (API_ElemTypeID elemType,
							const std::function<Int32 (const API_Element&)>& getLibInd,
							GS::Array<LibElemRef>* refs)
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
		if (libInd > 0) {
			LibElemRef ref;
			ref.libInd = libInd;
			ref.elemGuid = elemGuid;
			ref.elemType = elemType;
			refs->Push (ref);
		}
	}
}

static GS::UniString	GetElemTypeName (API_ElemTypeID elemType)
{
	switch (elemType) {
		case API_ObjectID:		return "Объект";
		case API_LampID:		return "Светильник";
		case API_DoorID:		return "Дверь";
		case API_WindowID:		return "Окно";
		case API_SkylightID:	return "Мансардное окно";
		case API_ZoneID:		return "Зона";
		case API_LabelID:		return "Выноска";
		case API_DrawingID:		return "Чертёж";
		default:				break;
	}
	return GS::UniString::Printf ("type %d", (int) elemType);
}

static GSErrCode Do_ReportBrokenLibRefs (void)
{
	GS::Array<LibElemRef> refs;
	CollectLibRefs (API_ObjectID,	[] (const API_Element& e) { return e.object.libInd; },					&refs);
	CollectLibRefs (API_LampID,		[] (const API_Element& e) { return e.lamp.libInd; },					&refs);
	CollectLibRefs (API_DoorID,		[] (const API_Element& e) { return e.door.openingBase.libInd; },		&refs);
	CollectLibRefs (API_WindowID,	[] (const API_Element& e) { return e.window.openingBase.libInd; },		&refs);
	CollectLibRefs (API_SkylightID,	[] (const API_Element& e) { return e.skylight.openingBase.libInd; },	&refs);
	CollectLibRefs (API_ZoneID,		[] (const API_Element& e) { return e.zone.libInd; },					&refs);
	CollectLibRefs (API_LabelID,	[] (const API_Element& e) {
						return (e.label.labelClass == APILblClass_Symbol) ? e.label.u.symbol.libInd : 0; },
					&refs);
	CollectLibRefs (API_DrawingID,	[] (const API_Element& e) { return e.drawing.title.libInd; },			&refs);

	GS::Array<LibElemRef> brokenRefs;
	GS::Array<Int32> checkedInds;
	for (const LibElemRef& ref : refs) {
		if (checkedInds.Contains (ref.libInd))
			continue;
		checkedInds.Push (ref.libInd);

		API_LibPart libPart;
		BNZeroMemory (&libPart, sizeof (API_LibPart));
		libPart.index = ref.libInd;
		bool isBroken = (ACAPI_LibPart_Get (&libPart) != NoError || libPart.missingDef);
		if (isBroken) {
			for (const LibElemRef& r : refs) {
				if (r.libInd == ref.libInd)
					brokenRefs.Push (r);
			}
		}
		if (libPart.location != nullptr)
			delete libPart.location;
	}

	GS::UniString title = GetResString (STR_RES_REPORT, RS_BROKEN_TITLE);
	GS::UniString report = GetResString (STR_RES_REPORT, RS_BROKEN_CHECKED);
	AppendNumber (report, refs.GetSize ());
	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_BROKEN_COUNT));
	AppendNumber (report, brokenRefs.GetSize ());

	if (brokenRefs.IsEmpty ()) {
		report.Append ("\n");
		report.Append (GetResString (STR_RES_REPORT, RS_BROKEN_NONE));
	} else {
		report.Append ("\n\n");
		report.Append (GetResString (STR_RES_REPORT, RS_BROKEN_LIST));
		const UInt32 maxRefsToList = 40;
		UInt32 listed = 0;
		for (const LibElemRef& ref : brokenRefs) {
			if (listed >= maxRefsToList) {
				report.Append ("\n...");
				break;
			}
			report.Append ("\n- ");
			report.Append (GetElemTypeName (ref.elemType));
			report.Append (" ");
			report.Append (APIGuidToString (ref.elemGuid));
			report.Append (" — ");
			report.Append (GetResString (STR_RES_REPORT, RS_BROKEN_MISSING));
			AppendNumber (report, (UIndex) ref.libInd);
			report.Append (")");
			listed++;
		}
	}

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
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

// =============================================================================
// Hatch area calculator
// =============================================================================

struct HatchGroupKey {
	short	determination;		// 0 = Drafting, 1 = Cut, 2 = Cover
	short	fillPenIndex;

	bool operator== (const HatchGroupKey& other) const {
		return determination == other.determination && fillPenIndex == other.fillPenIndex;
	}
};

struct HatchGroupData {
	UInt32	count;
	double	area;		// m²
};

static double	CalcPolygonAreaWithArcs (const API_HatchType& hatch, const API_ElementMemo& memo)
{
	Int32 nCoords = hatch.poly.nCoords;
	Int32 nSubPolys = hatch.poly.nSubPolys;
	Int32 nArcs = hatch.poly.nArcs;

	if (nCoords < 3 || memo.coords == nullptr || memo.pends == nullptr)
		return 0.0;

	// Shoelace formula (signed area)
	double area = 0.0;
	Int32 subPolyStart = 1;
	for (Int32 sp = 1; sp <= nSubPolys; sp++) {
		Int32 subPolyEnd = (*memo.pends)[sp];
		double subArea = 0.0;
		for (Int32 i = subPolyStart; i < subPolyEnd; i++) {
			subArea += (*memo.coords)[i].x * (*memo.coords)[i + 1].y;
			subArea -= (*memo.coords)[i + 1].x * (*memo.coords)[i].y;
		}
		area += 0.5 * subArea;
		subPolyStart = subPolyEnd + 1;
	}

	// Arc segment corrections
	if (nArcs > 0 && memo.parcs != nullptr) {
		for (Int32 a = 0; a < nArcs; a++) {
			const API_PolyArc& arc = (*memo.parcs)[a];
			if (arc.begIndex < 1 || arc.begIndex > nCoords ||
				arc.endIndex < 1 || arc.endIndex > nCoords)
				continue;

			API_Coord A = (*memo.coords)[arc.begIndex];
			API_Coord B = (*memo.coords)[arc.endIndex];

			double dx = B.x - A.x;
			double dy = B.y - A.y;
			double chord = sqrt (dx * dx + dy * dy);
			if (chord < 1e-10)
				continue;

			double theta = fabs (arc.arcAngle);
			if (theta < 1e-10 || theta > 2.0 * 3.14159265358979)
				continue;

			double R = chord / (2.0 * sin (theta / 2.0));
			double segmentArea = R * R * (theta - sin (theta)) / 2.0;

			// Find circle center
			double mx = (A.x + B.x) / 2.0;
			double my = (A.y + B.y) / 2.0;
			double d = chord / 2.0;
			double h = sqrt (R * R - d * d);
			double nx = -dy / chord;
			double ny = dx / chord;

			// Two candidate centers
			double cx1 = mx + h * nx;
			double cy1 = my + h * ny;

			// Determine sign via cross product AB × OA
			double OAx = A.x - cx1;
			double OAy = A.y - cy1;
			double cross = dx * OAy - dy * OAx;

			double sign = (cross * arc.arcAngle > 0) ? 1.0 : -1.0;
			area += sign * segmentArea;
		}
	}

	return fabs (area);
}

static GS::UniString	GetHatchTypeName (short determination)
{
	switch (determination) {
		case 0:		return GS::UniString (GetResString (STR_RES_REPORT, RS_HATCH_TYPE_0));
		case 1:		return GS::UniString (GetResString (STR_RES_REPORT, RS_HATCH_TYPE_1));
		case 2:		return GS::UniString (GetResString (STR_RES_REPORT, RS_HATCH_TYPE_2));
		default:	return GS::UniString::Printf ("%d", (int) determination);
	}
}

static GSErrCode Do_CalcHatchAreas (void)
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

		double hatchArea = CalcPolygonAreaWithArcs (elem.hatch, memo);
		ACAPI_DisposeElemMemoHdls (&memo);

		if (hatchArea < 1e-10)
			continue;

		// Group by determination + fillPen
		HatchGroupKey key;
		key.determination = elem.hatch.determination;
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
	report.Append (GetResString (STR_RES_REPORT, RS_HATCH_TYPE_0));
	report.Append ("...");		// column header hint

	// Simple formatted table
	const UInt32 maxNameLen = 16;
	report.Append ("\n");

	for (UIndex g = 0; g < groupKeys.GetSize (); g++) {
		GS::UniString typeName = GetHatchTypeName (groupKeys[g].determination);
		GS::UniString penStr = GetResString (STR_RES_REPORT, RS_HATCH_PEN);
		AppendNumber (penStr, (UIndex) groupKeys[g].fillPenIndex);

		GS::UniString countStr;
		AppendNumber (countStr, groupData[g].count);

		GS::UniString areaStr = GS::UniString::Printf ("%.2f", groupData[g].area);

		// Format: "TypeName | Pen N | count | area"
		report.Append ("- ");
		report.Append (typeName);
		report.Append (", ");
		report.Append (penStr);
		report.Append (": ");
		report.Append (countStr);
		report.Append (" шт., ");
		report.Append (areaStr);
		report.Append (" м²\n");
	}

	report.Append ("\n");
	report.Append (GetResString (STR_RES_REPORT, RS_HATCH_TOTAL));
	report.Append (": ");
	AppendNumber (report, totalHatches);
	report.Append (" шт., ");
	report.Append (GS::UniString::Printf ("%.2f", totalArea));
	report.Append (" м²");

	ACAPI_WriteReport (FormatSafe (title + "\n" + report), true);
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
		case 5:		return Do_ScanDuplicateLibParts ();
		case 6:		return Do_ReportBrokenLibRefs ();
		case 7:		return Do_CalcHatchAreas ();
		case 9:		return Do_About ();
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
