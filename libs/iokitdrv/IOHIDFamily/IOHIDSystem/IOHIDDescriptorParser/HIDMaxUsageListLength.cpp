/*
 * @NICTA_MODIFICATIONS_START@
 * 
 * This source code is licensed under Apple Public Source License Version 2.0.
 * Portions copyright Apple Computer, Inc.
 * Portions copyright National ICT Australia.
 *
 * All rights reserved.
 *
 * This code was modified 2006-06-20.
 *
 * @NICTA_MODIFICATIONS_END@
 */
#ifdef L4IOKIT
/*Compile with c++ due to fucking -Werror.  Spastic piece of shit!*/
extern "C" {
#endif
/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
	File:		HIDMaxUsageListLength.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(BWS)	Brent Schorsch

	Change History (most recent first):

	  <USB3>	 11/1/99	BWS		[2405720]  We need a better check for 'bit padding' items,
									rather than just is constant. We will check to make sure the
									item is constant, and has no usage, or zero usage. This means we
									need to pass an additional parameter to some internal functions
	  <USB2>	  3/6/99	BWS		Eliminate warning
	  <USB1>	  3/5/99	BWS		first checked in
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDMaxUsageListLength
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  ptPreparsedData		- Pre-Parsed Data
 *	 Output:
 *	 Returns: length of list
 *
 *------------------------------------------------------------------------------
*/
UInt32
HIDMaxUsageListLength	   (HIDReportType reportType,
							HIDUsage usagePage,
							HIDPreparsedDataRef preparsedDataRef)
{
#pragma unused(usagePage)	// not used, see comment below

	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDReportItem *ptReportItem;
	int iButtons;
	int i;

	
/*
 *	Disallow Null Pointers
*/
	if (ptPreparsedData == NULL)
		return 0;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	Go through the ReportItems
 *	Filter on ReportType
 *	Sum the button counts
 *
 * NOTE: A more precise value for the maximum list length
 *		 may be obtained by filtering out the usages that
 *		 are not on the specified usage page.  Most of
 *		 the time the number returned below is the same
 *		 as that returned by filtering usages.	It is
 *		 never smaller.	 The tradeoff is sometimes wasting
 *		 a few words of RAM in exchange for speed.
*/
	iButtons = 0;
	for (i=0; i<ptPreparsedData->reportItemCount; i++)
	{
		ptReportItem = &ptPreparsedData->reportItems[i];
		if ((ptReportItem->reportType == reportType)
		 && HIDIsButton(ptReportItem, preparsedDataRef))
			iButtons += ptReportItem->globals.reportCount;
	}
	return iButtons;
}
#ifdef L4IOKIT
};
#endif
