#include "ProjectCleanerPalette.hpp"
#include "ProjectCleanerCompat.hpp"
#include "DGApplication.hpp"

// Forward declarations from ProjectCleaner.cpp
extern GSErrCode Do_ScanDeleteUnusedViews (void);
extern GSErrCode Do_ScanDeleteEmbeddedLibrary (void);
extern GSErrCode Do_ScanUnusedLayers (void);
extern GSErrCode Do_ScanUnusedMasterLayouts (void);
extern GSErrCode Do_CalcHatchAreas (void);
extern GSErrCode Do_CalcLineLengths (void);
extern GSErrCode Do_CreateZonesFromHatches (void);
extern GSErrCode Do_CreateSlabsFromHatches (void);
extern GSErrCode Do_DimChainWallOpenings (void);
extern GSErrCode Do_WallThicknessChain (void);
extern GSErrCode Do_About (void);

#define PAL_RES_ID 32600

// Icon button geometry: 32x32 buttons with 2 px gaps (matches the GDLG resource)
static const short kBtnCount = 11;
static const short kBtnStep  = 34;
static const short kHorizWidth  = 420;
static const short kHorizHeight = 34;
static const short kVertWidth   = 48;
static const short kVertHeight  = 2 + kBtnCount * kBtnStep;
static const short kHorizX[kBtnCount] = { 4, 38, 72, 106, 150, 184, 228, 262, 306, 340, 384 };

// {6B1E4A72-3C58-4D9E-9F27-08B5A1C6D3E4}
static const GS::Guid s_PaletteGuid ("6B1E4A72-3C58-4D9E-9F27-08B5A1C6D3E4");

const GS::Guid& ProjectCleanerPalette::PaletteGuid ()
{
	return s_PaletteGuid;
}

GS::Int32 ProjectCleanerPalette::PaletteRefId ()
{
	static GS::Int32 refId (GS::CalculateHashValue (PaletteGuid ()));
	return refId;
}

GSErrCode __ACENV_CALL ProjectCleanerPalette::PaletteAPIControlCallBack (GS::Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param)
{
	GSErrCode err = NoError;
	if (referenceID == PaletteRefId ()) {
		ProjectCleanerPalette& palette = GetInstance ();
		switch (messageID) {
			case APIPalMsg_OpenPalette:
				palette.Show ();
				break;
			case APIPalMsg_ClosePalette:
				palette.SendCloseRequest ();
				break;
			case APIPalMsg_HidePalette_Begin:
				palette.Hide ();
				break;
			case APIPalMsg_HidePalette_End:
				palette.Show ();
				break;
			case APIPalMsg_DisableItems_Begin:
				palette.DisableItems ();
				break;
			case APIPalMsg_DisableItems_End:
				palette.EnableItems ();
				break;
			case APIPalMsg_IsPaletteVisible:
				(*reinterpret_cast<bool*> (param)) = palette.IsVisible ();
				break;
			default:
				break;
		}
	}
	return err;
}

ProjectCleanerPalette& ProjectCleanerPalette::GetInstance ()
{
	static ProjectCleanerPalette instance;
	return instance;
}

ProjectCleanerPalette::ProjectCleanerPalette ():
	DG::Palette	(ACAPI_GetOwnResModule (), PAL_RES_ID, ACAPI_GetOwnResModule (), s_PaletteGuid),

	btnScanDeleteViews	(GetReference (), BtnScanDeleteViews),
	btnScanDeleteLibrary	(GetReference (), BtnScanDeleteLibrary),
	btnScanLayers		(GetReference (), BtnScanLayers),
	btnMasterLayouts	(GetReference (), BtnMasterLayouts),
	btnHatchArea		(GetReference (), BtnHatchArea),
	btnLineLength		(GetReference (), BtnLineLength),
	btnCreateZones		(GetReference (), BtnCreateZones),
	btnCreateSlabs		(GetReference (), BtnCreateSlabs),
	btnDimChain			(GetReference (), BtnDimChain),
	btnWallChain		(GetReference (), BtnWallChain),
	btnAbout			(GetReference (), BtnAbout)
{
	this->Attach (*this);
	AttachToAllItems (*this);
	this->BeginEventProcessing ();

	// Listen for dock orientation changes so the button row can be rotated
	// when the palette is docked to the left/right edge.
	if (DG::Application* app = dynamic_cast<DG::Application*> (GS::Application::GetInstance ()))
		app->Attach (*this);
}

ProjectCleanerPalette::~ProjectCleanerPalette ()
{
	if (DG::Application* app = dynamic_cast<DG::Application*> (GS::Application::GetInstance ()))
		app->Detach (*this);

	this->EndEventProcessing ();
	this->Detach (*this);
	DetachFromAllItems (*this);
}

// Re-arranges the icon buttons: a row for the horizontal (top/bottom, floating)
// layout, a column for the vertical (left/right) docked layout.
void ProjectCleanerPalette::ApplyDockLayout (bool horizontal)
{
	DG::IconButton* buttons[kBtnCount] = {
		&btnScanDeleteViews, &btnScanDeleteLibrary, &btnScanLayers, &btnMasterLayouts,
		&btnHatchArea, &btnLineLength, &btnCreateZones, &btnCreateSlabs,
		&btnDimChain, &btnWallChain, &btnAbout
	};

	if (horizontal) {
		for (short i = 0; i < kBtnCount; i++)
			buttons[i]->SetPosition (kHorizX[i], 1);
		SetClientSize (kHorizWidth, kHorizHeight);
	} else {
		for (short i = 0; i < kBtnCount; i++)
			buttons[i]->SetPosition (2, (short) (2 + i * kBtnStep));
		SetClientSize (kVertWidth, kVertHeight);
	}
}

bool ProjectCleanerPalette::PaletteDockOrientationChanged (const GS::Guid& paletteGuid, bool isDocked, bool horizontal)
{
	if (paletteGuid != s_PaletteGuid)
		return false;

	// Do not resize the palette inside the docking callback: changing the size
	// while ArchiCAD is placing the palette cancels the dock operation.
	// Remember the requested orientation and apply it on the next idle event.
	pendingHorizontal = (!isDocked || horizontal);
	pendingLayout = true;
	return true;
}

void ProjectCleanerPalette::PanelIdle (const DG::PanelIdleEvent& /*ev*/)
{
	if (!pendingLayout)
		return;

	pendingLayout = false;
	ApplyDockLayout (pendingHorizontal);
}

void ProjectCleanerPalette::PanelOpened (const DG::PanelOpenEvent& /*ev*/)
{
	UpdateButtonStates ();
}

void ProjectCleanerPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* /*accepted*/)
{
	Hide ();
}

#define TOOLTIP_RES_ID 32503

void ProjectCleanerPalette::ItemToolTipRequested (const DG::ItemHelpEvent& ev, GS::UniString* toolTipText)
{
	if (toolTipText == nullptr)
		return;
	DG::Item* item = ev.GetSource ();
	if (item == nullptr)
		return;

	short index = 0;
	if (item == &btnScanDeleteViews)
		index = 1;
	else if (item == &btnScanDeleteLibrary)
		index = 2;
	else if (item == &btnScanLayers)
		index = 3;
	else if (item == &btnMasterLayouts)
		index = 4;
	else if (item == &btnHatchArea)
		index = 5;
	else if (item == &btnLineLength)
		index = 6;
	else if (item == &btnCreateZones)
		index = 7;
	else if (item == &btnCreateSlabs)
		index = 8;
	else if (item == &btnDimChain)
		index = 9;
	else if (item == &btnWallChain)
		index = 10;
	else if (item == &btnAbout)
		index = 11;

	if (index > 0)
		RSGetIndString (toolTipText, TOOLTIP_RES_ID, index, ACAPI_GetOwnResModule ());
}

void ProjectCleanerPalette::PanelToolTipRequested (const DG::PanelHelpEvent& ev, GS::UniString* toolTipText)
{
	if (toolTipText == nullptr)
		return;
	const DG::Item* item = ev.GetItem ();
	if (item == nullptr)
		return;

	short index = 0;
	if (item == &btnScanDeleteViews)
		index = 1;
	else if (item == &btnScanDeleteLibrary)
		index = 2;
	else if (item == &btnScanLayers)
		index = 3;
	else if (item == &btnMasterLayouts)
		index = 4;
	else if (item == &btnHatchArea)
		index = 5;
	else if (item == &btnLineLength)
		index = 6;
	else if (item == &btnCreateZones)
		index = 7;
	else if (item == &btnCreateSlabs)
		index = 8;
	else if (item == &btnDimChain)
		index = 9;
	else if (item == &btnWallChain)
		index = 10;
	else if (item == &btnAbout)
		index = 11;

	if (index > 0)
		RSGetIndString (toolTipText, TOOLTIP_RES_ID, index, ACAPI_GetOwnResModule ());
}

API_WindowTypeID ProjectCleanerPalette::GetCurrentWindowType ()
{
	API_WindowInfo windowInfo = {};
	if (PC_GetCurrentWindow (&windowInfo) != NoError)
		return APIWind_AllID;	// unknown: treat as "nothing is available"

	return windowInfo.typeID;
}

bool ProjectCleanerPalette::IsFloorPlanActive ()
{
	return GetCurrentWindowType () == APIWind_FloorPlanID;
}

// Area / length measurement works on the floor plan and in every 2D drawing
// database: sections, elevations, interior elevations, details, worksheets,
// layouts (annotation drawn directly on the sheet) and 3D documents.
bool ProjectCleanerPalette::IsMeasurementWindowActive ()
{
	switch (GetCurrentWindowType ()) {
		case APIWind_FloorPlanID:
		case APIWind_SectionID:
		case APIWind_ElevationID:
		case APIWind_InteriorElevationID:
		case APIWind_DetailID:
		case APIWind_WorksheetID:
		case APIWind_LayoutID:
		case APIWind_MasterLayoutID:
		case APIWind_DocumentFrom3DID:
			return true;
		default:
			return false;
	}
}

void ProjectCleanerPalette::UpdateButtonStates ()
{
	const bool plan		= IsFloorPlanActive ();
	const bool measure	= IsMeasurementWindowActive ();

	// Project cleanup, hatch-to-zone/slab conversion and wall dimensions need
	// the floor plan database (stories, zones, slabs).
	btnScanDeleteViews.SetStatus (plan);
	btnScanDeleteLibrary.SetStatus (plan);
	btnScanLayers.SetStatus (plan);
	btnMasterLayouts.SetStatus (plan);
	btnCreateZones.SetStatus (plan);
	btnCreateSlabs.SetStatus (plan);
	btnDimChain.SetStatus (plan);
	btnWallChain.SetStatus (plan);

	btnHatchArea.SetStatus (measure);
	btnLineLength.SetStatus (measure);
	btnAbout.SetStatus (true);
}

void ProjectCleanerPalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &btnAbout) {
		Do_About ();
		return;
	}

	if (ev.GetSource () == &btnHatchArea) {
		if (IsMeasurementWindowActive ())
			Do_CalcHatchAreas ();
		return;
	}
	if (ev.GetSource () == &btnLineLength) {
		if (IsMeasurementWindowActive ())
			Do_CalcLineLengths ();
		return;
	}

	if (!IsFloorPlanActive ())
		return;

	if (ev.GetSource () == &btnScanDeleteViews)
		Do_ScanDeleteUnusedViews ();
	else if (ev.GetSource () == &btnScanDeleteLibrary)
		Do_ScanDeleteEmbeddedLibrary ();
	else if (ev.GetSource () == &btnScanLayers)
		Do_ScanUnusedLayers ();
	else if (ev.GetSource () == &btnMasterLayouts)
		Do_ScanUnusedMasterLayouts ();
	else if (ev.GetSource () == &btnCreateZones)
		Do_CreateZonesFromHatches ();
	else if (ev.GetSource () == &btnCreateSlabs)
		Do_CreateSlabsFromHatches ();
	else if (ev.GetSource () == &btnWallChain)
		Do_WallThicknessChain ();
	else if (ev.GetSource () == &btnDimChain)
		Do_DimChainWallOpenings ();
}
