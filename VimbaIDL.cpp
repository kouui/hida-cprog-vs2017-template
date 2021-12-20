
/*
History:

2021.12.20    u.k.    project initialized

*/

#include "pch.h"
#include "stdio.h"
#include "idl_export.h"
#include <malloc.h>


/****************************************************************/
/*  TEST													*/
/****************************************************************/
int IDL_STDCALL Hello(int argc, void* argv[])
{

	{
		MessageBox(NULL, "Hello", "Hello", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
		return 0;
	}

}