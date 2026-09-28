#include "ProjectCleanerPalette.hpp"

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

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
static const GS::Guid s_PaletteGuid ("A1B2C3D4-E5F6-7890-ABCD-EF1234567890");

const GS::Guid& ProjectCleanerPalette::PaletteGuid ()
{
	return s_PaletteGuid;
}

Int32 ProjectCleanerPalette::PaletteRefId ()
{
	static Int32 refId (GS::CalculateHashValue (PaletteGuid ()));
	return refId;
}

GSErrCode __ACENV_CALL ProjectCleanerPalette::PaletteAPIControlCallBack (Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param)
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
	DG::Palette	(ACAPI_GetOwnResModule (), PAL_RES_ID, ACAPI_GetOwnResModule ()),

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
}

ProjectCleanerPalette::~ProjectCleanerPalette ()
{
	this->EndEventProcessing ();
	this->Detach (*this);
	DetachFromAllItems (*this);
}

void ProjectCleanerPalette::PanelOpened (const DG::PanelOpenEvent& /*ev*/)
{
	UpdateButtonStates ();
}

void ProjectCleanerPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* /*accepted*/)
{
	Hide ();
}

bool ProjectCleanerPalette::IsFloorPlanActive ()
{
	API_WindowInfo windowInfo = {};
	if (ACAPI_Database (APIDb_GetCurrentWindowID, &windowInfo) != NoError)
		return false;

	return windowInfo.typeID == APIWind_FloorPlanID;
}

void ProjectCleanerPalette::UpdateButtonStates ()
{
	bool enabled = IsFloorPlanActive ();

	btnScanDeleteViews.SetStatus (enabled);
	btnScanDeleteLibrary.SetStatus (enabled);
	btnScanLayers.SetStatus (enabled);
	btnMasterLayouts.SetStatus (enabled);
	btnHatchArea.SetStatus (enabled);
	btnLineLength.SetStatus (enabled);
	btnCreateZones.SetStatus (enabled);
	btnCreateSlabs.SetStatus (enabled);
	btnDimChain.SetStatus (enabled);
	btnWallChain.SetStatus (enabled);
	btnAbout.SetStatus (true);
}

void ProjectCleanerPalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () != &btnAbout && !IsFloorPlanActive ())
		return;

	if (ev.GetSource () == &btnScanDeleteViews)
		Do_ScanDeleteUnusedViews ();
	else if (ev.GetSource () == &btnScanDeleteLibrary)
		Do_ScanDeleteEmbeddedLibrary ();
	else if (ev.GetSource () == &btnScanLayers)
		Do_ScanUnusedLayers ();
	else if (ev.GetSource () == &btnMasterLayouts)
		Do_ScanUnusedMasterLayouts ();
	else if (ev.GetSource () == &btnHatchArea)
		Do_CalcHatchAreas ();
	else if (ev.GetSource () == &btnLineLength)
		Do_CalcLineLengths ();
	else if (ev.GetSource () == &btnCreateZones)
		Do_CreateZonesFromHatches ();
	else if (ev.GetSource () == &btnCreateSlabs)
		Do_CreateSlabsFromHatches ();
	else if (ev.GetSource () == &btnWallChain)
		Do_WallThicknessChain ();
	else if (ev.GetSource () == &btnDimChain)
		Do_DimChainWallOpenings ();
	else if (ev.GetSource () == &btnAbout)
		Do_About ();
}
