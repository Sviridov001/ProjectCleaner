#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "DGModule.hpp"

class ProjectCleanerPalette :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ButtonItemObserver,
								public DG::CompoundItemObserver
{
private:
	enum {
		BtnScanViews		= 1,
		BtnDeleteViews		= 2,
		BtnScanLibrary		= 3,
		BtnDeleteLibrary	= 4,
		BtnHatchArea		= 5,
		BtnLineLength		= 6,
		BtnAbout			= 7
	};

	DG::Button		btnScanViews;
	DG::Button		btnDeleteViews;
	DG::Button		btnScanLibrary;
	DG::Button		btnDeleteLibrary;
	DG::Button		btnHatchArea;
	DG::Button		btnLineLength;
	DG::Button		btnAbout;

	ProjectCleanerPalette ();

public:
	virtual ~ProjectCleanerPalette ();

	static ProjectCleanerPalette&	GetInstance ();
	static Int32					PaletteRefId ();
	static const GS::Guid&			PaletteGuid ();
	static GSErrCode __ACENV_CALL	PaletteAPIControlCallBack (Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

protected:
	virtual void	PanelOpened (const DG::PanelOpenEvent& ev) override;
	virtual void	PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;
};
