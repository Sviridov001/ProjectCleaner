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

GSErrCode Do_ScanEmbeddedLibrary (void)
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

GSErrCode Do_DeleteUnusedEmbeddedLibParts (void)
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

// =============================================================================
// Commands
// =============================================================================

GSErrCode Do_ScanUnusedViews (void)
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

GSErrCode Do_DeleteUnusedViews (void)
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

GSErrCode Do_About (void)
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
		case 5:		return Do_CalcHatchAreas ();
		case 6:		return Do_CalcLineLengths ();
		case 8:		return Do_About ();
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
	GSErrCode err = ACAPI_Install_MenuHandler (MENU_RES_ID, MenuHandler);

	ACAPI_RegisterModelessWindow (ProjectCleanerPalette::PaletteRefId (),
								  ProjectCleanerPalette::PaletteAPIControlCallBack,
								  API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
								  API_PalEnabled_InteriorElevation + API_PalEnabled_3D +
								  API_PalEnabled_Detail + API_PalEnabled_Worksheet + API_PalEnabled_Layout +
								  API_PalEnabled_DocumentFrom3D, GSGuid2APIGuid (ProjectCleanerPalette::PaletteGuid ()));

	ProjectCleanerPalette::GetInstance ().Show ();

	return err;
}

GSErrCode __ACENV_CALL	FreeData (void)
{
	ACAPI_UnregisterModelessWindow (ProjectCleanerPalette::PaletteRefId ());
	return NoError;
}
