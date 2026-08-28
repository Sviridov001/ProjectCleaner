#include "ProjectCleanerPalette.hpp"

// Forward declarations from ProjectCleaner.cpp
extern GSErrCode Do_ScanUnusedViews (void);
extern GSErrCode Do_DeleteUnusedViews (void);
extern GSErrCode Do_ScanEmbeddedLibrary (void);
extern GSErrCode Do_DeleteUnusedEmbeddedLibParts (void);
extern GSErrCode Do_CalcHatchAreas (void);
extern GSErrCode Do_CalcLineLengths (void);
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
	DG::Palette	(ACAPI_GetOwnResModule (), PAL_RES_ID, ACAPI_GetOwnResModule (), PaletteGuid ()),

	btnScanViews		(GetReference (), BtnScanViews),
	btnDeleteViews		(GetReference (), BtnDeleteViews),
	btnScanLibrary		(GetReference (), BtnScanLibrary),
	btnDeleteLibrary	(GetReference (), BtnDeleteLibrary),
	btnHatchArea		(GetReference (), BtnHatchArea),
	btnLineLength		(GetReference (), BtnLineLength),
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
}

void ProjectCleanerPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* /*accepted*/)
{
	Hide ();
}

void ProjectCleanerPalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &btnScanViews)
		Do_ScanUnusedViews ();
	else if (ev.GetSource () == &btnDeleteViews)
		Do_DeleteUnusedViews ();
	else if (ev.GetSource () == &btnScanLibrary)
		Do_ScanEmbeddedLibrary ();
	else if (ev.GetSource () == &btnDeleteLibrary)
		Do_DeleteUnusedEmbeddedLibParts ();
	else if (ev.GetSource () == &btnHatchArea)
		Do_CalcHatchAreas ();
	else if (ev.GetSource () == &btnLineLength)
		Do_CalcLineLengths ();
	else if (ev.GetSource () == &btnAbout)
		Do_About ();
}
