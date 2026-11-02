#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "DGModule.hpp"
#include "DGApplicationObserver.hpp"
#include "DGStaticItem.hpp"

class ProjectCleanerPalette :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ButtonItemObserver,
								public DG::CompoundItemObserver,
								public DG::ApplicationObserver
{
private:
	enum {
		BtnScanDeleteViews	= 1,
		BtnScanDeleteLibrary	= 2,
		BtnScanLayers		= 3,
		BtnMasterLayouts	= 4,
		BtnHatchArea		= 5,
		BtnLineLength		= 6,
		BtnWallVolume		= 7,
		BtnCreateZones		= 8,
		BtnCreateSlabs		= 9,
		BtnDimChain			= 10,
		BtnWallChain		= 11,
		BtnAbout			= 12
	};

	DG::IconButton	btnScanDeleteViews;
	DG::IconButton	btnScanDeleteLibrary;
	DG::IconButton	btnScanLayers;
	DG::IconButton	btnMasterLayouts;
	DG::IconButton	btnHatchArea;
	DG::IconButton	btnLineLength;
	DG::IconButton	btnWallVolume;
	DG::IconButton	btnCreateZones;
	DG::IconButton	btnCreateSlabs;
	DG::IconButton	btnDimChain;
	DG::IconButton	btnWallChain;
	DG::IconButton	btnAbout;

	// Thin lines between the button groups (same grouping as the add-on menu)
	DG::Separator	sepGroup1;
	DG::Separator	sepGroup2;
	DG::Separator	sepGroup3;
	DG::Separator	sepGroup4;

	ProjectCleanerPalette ();

public:
	virtual ~ProjectCleanerPalette ();

	void							UpdateButtonStates ();

	static ProjectCleanerPalette&	GetInstance ();
	static GS::Int32					PaletteRefId ();
	static const GS::Guid&			PaletteGuid ();
	static GSErrCode __ACENV_CALL	PaletteAPIControlCallBack (GS::Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

private:
	API_WindowTypeID				GetCurrentWindowType ();
	bool							IsFloorPlanActive ();
	bool							IsMeasurementWindowActive ();
	bool							IsWallVolumeActive ();
	void							ApplyDockLayout (bool horizontal);

	bool							pendingLayout = false;
	bool							pendingHorizontal = true;

protected:
	virtual void	PanelOpened (const DG::PanelOpenEvent& ev) override;
	virtual void	PanelIdle (const DG::PanelIdleEvent& ev) override;
	virtual void	PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
	virtual void	PanelToolTipRequested (const DG::PanelHelpEvent& ev, GS::UniString* toolTipText) override;
	virtual void	ItemToolTipRequested (const DG::ItemHelpEvent& ev, GS::UniString* toolTipText) override;
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;
	virtual bool	PaletteDockOrientationChanged (const GS::Guid& paletteGuid, bool isDocked, bool horizontal) override;
};
