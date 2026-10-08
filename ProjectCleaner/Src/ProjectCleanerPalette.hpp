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
		BtnScanDeleteViews	= 1,
		BtnScanDeleteLibrary	= 2,
		BtnScanLayers		= 3,
		BtnMasterLayouts	= 4,
		BtnHatchArea		= 5,
		BtnLineLength		= 6,
		BtnCreateZones		= 7,
		BtnCreateSlabs		= 8,
		BtnDimChain			= 9,
		BtnWallChain		= 10,
		BtnAbout			= 11
	};

	DG::IconButton	btnScanDeleteViews;
	DG::IconButton	btnScanDeleteLibrary;
	DG::IconButton	btnScanLayers;
	DG::IconButton	btnMasterLayouts;
	DG::IconButton	btnHatchArea;
	DG::IconButton	btnLineLength;
	DG::IconButton	btnCreateZones;
	DG::IconButton	btnCreateSlabs;
	DG::IconButton	btnDimChain;
	DG::IconButton	btnWallChain;
	DG::IconButton	btnAbout;

	ProjectCleanerPalette ();

public:
	virtual ~ProjectCleanerPalette ();

	void							UpdateButtonStates ();

	static ProjectCleanerPalette&	GetInstance ();
	static Int32					PaletteRefId ();
	static const GS::Guid&			PaletteGuid ();
	static GSErrCode __ACENV_CALL	PaletteAPIControlCallBack (Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

private:
	bool							IsFloorPlanActive ();

protected:
	virtual void	PanelOpened (const DG::PanelOpenEvent& ev) override;
	virtual void	PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
	virtual void	PanelToolTipRequested (const DG::PanelHelpEvent& ev, GS::UniString* toolTipText) override;
	virtual void	ItemToolTipRequested (const DG::ItemHelpEvent& ev, GS::UniString* toolTipText) override;
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;
};
