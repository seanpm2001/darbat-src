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
	File:		HIDSetScaledUsageValue.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	� 1999-2001 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(KH)	Keithen Hayenga
		(BWS)	Brent Schorsch

	Change History (most recent first):

	  <USB5>	 1/18/01	KH		Fix for complex descriptors only needed for buttons.
	  <USB4>	 3/24/00	KH		Complex report descriptors could lead to reporting
									kHIDUsageNotFoundErr's as kHIDIncompatibleReportErr's instead.
	  <USB3>	 11/1/99	BWS		[2405720]  We need a better check for 'bit padding' items,
									rather than just is constant. We will check to make sure the
									item is constant, and has no usage, or zero usage. This means we
									need to pass an additional parameter to some internal functions
	  <USB2>	  4/7/99	BWS		Add support for reversed report items
	  <USB1>	  3/5/99	BWS		first checked in
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDSetScaledUsageValue - Set the value for a usage
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  iCollection			- Collection Criteria or zero
 *			  usage				   - usage Criteria or zero
 *			  iValue				- The usage Value
 *			  ptPreparsedData		- Pre-Parsed Data
 *			  psReport				- An HID Report
 *			  iReportLength			- The length of the Report
 *	 Output:
 *			  piValue				- Pointer to usage Value
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDSetScaledUsageValue(HIDReportType reportType,
									 HIDUsage usagePage,
									 UInt32 iCollection,
									 HIDUsage usage,
									 SInt32 iUsageValue,
									 HIDPreparsedDataRef preparsedDataRef,
									 void *psReport,
									 ByteCount iReportLength)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDReportItem *ptReportItem;
	OSStatus iStatus;
	int iR;
	SInt32 data;
	int iStart;
	int iReportItem;
	UInt32 iUsageIndex;
	Boolean bIncompatibleReport = false;
/*
 *	Disallow Null Pointers
*/
	if ((ptPreparsedData == NULL)
	 || (psReport == NULL))
		return kHIDNullPointerErr;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	The Collection must be in range
*/
	if ((iCollection < 0) || (iCollection >= ptPreparsedData->collectionCount))
		return kHIDBadParameterErr;
/*
 *	Search only the scope of the Collection specified
 *	Go through the ReportItems
 *	Filter on ReportType and usagePage
*/
	ptCollection = &ptPreparsedData->collections[iCollection];
	for (iR=0; iR<ptCollection->reportItemCount; iR++)
	{
		iReportItem = ptCollection->firstReportItem + iR;
		ptReportItem = &ptPreparsedData->reportItems[iReportItem];
		if ((ptReportItem->reportType == reportType)
		 && HIDIsVariable(ptReportItem, preparsedDataRef)
		 && HIDHasUsage(preparsedDataRef,ptReportItem,usagePage,usage,&iUsageIndex,NULL))
		{
/*
 *			This may be the proper place to write data
 *			Let's check for the proper Report ID, Type, and Length
*/
			iStatus = HIDCheckReport(reportType,preparsedDataRef,ptReportItem,psReport,iReportLength);
/*
 *			The Report ID or Type may not match.
 *			This may not be an error (yet)
*/
			if (iStatus == kHIDIncompatibleReportErr)
				bIncompatibleReport = true;
			else if (iStatus != kHIDSuccess)
				return iStatus;
			else
			{
				iStatus = HIDScaleUsageValueOut(ptReportItem,iUsageValue,&data);
				if (iStatus != kHIDSuccess)
					return iStatus;
				iStart = ptReportItem->startBit + (ptReportItem->globals.reportSize * iUsageIndex);
				iStatus = HIDPreProcessRIValue (ptReportItem, &data);
				iStatus = HIDPutData(psReport, iReportLength, iStart, ptReportItem->globals.reportSize, data);
				if (iStatus != kHIDSuccess)
					return iStatus;
				return kHIDSuccess;
			}
		}
	}
	if (bIncompatibleReport)
		return kHIDIncompatibleReportErr;
	return kHIDUsageNotFoundErr;
}
#ifdef L4IOKIT
};
#endif
