// *****************************************************************************
// General settings for AddOn developments
// *****************************************************************************

#ifndef	_APIENVIR_H_
#define	_APIENVIR_H_


#if defined (_MSC_VER)
	#if !defined (WINDOWS)
		#define WINDOWS
	#endif
#endif

#if defined (WINDOWS)
	#include "Win32Interface.hpp"
#endif

#if defined (macintosh)
	#include <CoreServices/CoreServices.h>
#endif

#if !defined (ACExtension)
	#define ACExtension
#endif

// __ACENV_CALL was removed from the DevKit headers in ArchiCAD 27+
#if !defined (__ACENV_CALL)
	#if defined (_MSC_VER) && defined (WINDOWS)
		#define __ACENV_CALL	__stdcall
	#else
		#define __ACENV_CALL
	#endif
#endif


#endif
