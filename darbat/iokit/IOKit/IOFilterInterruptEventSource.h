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
/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
Copyright (c) 1999 Apple Computer, Inc.	 All rights reserved.

HISTORY
    1999-4-15	Godfrey van der Linden(gvdl)
	Created.
*/
#ifndef _IOKIT_IOFILTERINTERRUPTEVENTSOURCE_H
#define _IOKIT_IOFILTERINTERRUPTEVENTSOURCE_H

#include <IOKit/IOInterruptEventSource.h>

class IOService;

/*! @class IOFilterInterruptEventSource : public IOInterruptEventSource
    @abstract Filtering varient of the $link IOInterruptEventSource.
    @discussion An interrupt event source that calls the client to determine if a interrupt event needs to be scheduled on the work loop.  A filter interrupt event source call's the client in the primary interrupt context, the client can then interrogate its hardware and determine if the interrupt needs to be processed yet.
<br><br>
    As the routine is called in the primary interrupt context great care must be taken in the writing of this routine.  In general none of the generic IOKit environment is safe to call in this context.  We intend this routine to be used by hardware that can interrogate its registers without destroying state.  Primarily this variant of event sources will be used by drivers that share interrupts.  The filter routine will determine if the interrupt is a real interrupt or a ghost and thus optimise the work thread context switch away.
<br><br>
If you are implementing 'SoftDMA' (or pseudo-DMA), you may not want the I/O Kit to automatically start your interrupt handler routine on your work loop when your filter routine returns true.  In this case, you may choose to have your filter routine schedule the work on the work loop itself and then return false.  If you do this, the interrupt will not be disabled in hardware and you could receive additional primary interrupts before your work loop–level service routine completes.  Because this scheme has implications for synchronization between your filter routine and your interrupt service routine, you should avoid doing this unless your driver requires SoftDMA.
<br><br>
CAUTION:  Called in primary interrupt context, if you need to disable interrupt to guard you registers against an unexpected call then it is better to use a straight IOInterruptEventSource and its secondary interrupt delivery mechanism.
*/
class IOFilterInterruptEventSource : public IOInterruptEventSource
{
    OSDeclareDefaultStructors(IOFilterInterruptEventSource)

public:
/*!
    @typedef Filter
    @discussion C Function pointer to a routine to call when an interrupt occurs.
    @param owner Pointer to the owning/client instance.
    @param sender Where is the interrupt comming from.
    @result false if this interrupt can be ignored. */
    typedef bool (*Filter)(OSObject *, IOFilterInterruptEventSource *);

/*! @defined IOFilterInterruptAction
    @discussion Backward compatibilty define for the old non-class scoped type definition.  See $link IOFilterInterruptSource::Filter */
#define IOFilterInterruptAction IOFilterInterruptEventSource::Filter

private:
    // Hide the superclass initializers
    virtual bool init(OSObject *inOwner,
		      IOInterruptEventSource::Action inAction = 0,
		      IOService *inProvider = 0,
		      int inIntIndex = 0);

    static IOInterruptEventSource *
	interruptEventSource(OSObject *inOwner,
			     IOInterruptEventSource::Action inAction = 0,
			     IOService *inProvider = 0,
			     int inIntIndex = 0);

protected:
/*! @var filterAction Filter callout */
    Filter filterAction;

/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the IOWorkLoop in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;

public:
/*! @function filterInterruptEventSource
    @abstract Factor method to create and initialise an IOFilterInterruptEventSource.  See $link init.
    @param owner Owner/client of this event source.
    @param action 'C' Function to call when something happens.
    @param filter 'C' Function to call when interrupt occurs.
    @param provider Service that provides interrupts.
    @param intIndex Defaults to 0.
    @result a new event source if succesful, 0 otherwise.  */
    static IOFilterInterruptEventSource *
	filterInterruptEventSource(OSObject *owner,
				   IOInterruptEventSource::Action action,
				   Filter filter,
				   IOService *provider,
				   int intIndex = 0);

/*! @function init
    @abstract Primary initialiser for the IOFilterInterruptEventSource class.
    @param owner Owner/client of this event source.
    @param action 'C' Function to call when something happens.
    @param filter 'C' Function to call in primary interrupt context.
    @param provider Service that provides interrupts.
    @param intIndex Interrupt source within provider.  Defaults to 0.
    @result true if the inherited classes and this instance initialise
successfully.  */
    virtual bool init(OSObject *owner,
		      IOInterruptEventSource::Action action,
		      Filter filter,
		      IOService *provider,
		      int intIndex = 0);


/*! @function signalInterrupt
    @abstract Cause the work loop to schedule the action.
    @discussion Cause the work loop to schedule the interrupt action even if the filter routine returns 'false'. Note well the interrupting condition MUST be cleared from the hardware otherwise an infinite process interrupt loop will occur.  Use this function when SoftDMA is desired.  See $link IOFilterInterruptSource::Filter */
    virtual void signalInterrupt();

/*! @function getFilterAction
    @abstract Get'ter for filterAction variable.
    @result value of filterAction. */
    virtual Filter getFilterAction() const;

/*! @function normalInterruptOccurred
    @abstract Override $link IOInterruptEventSource::normalInterruptOccured to make a filter callout. */
    virtual void normalInterruptOccurred(void *self, IOService *prov, int ind);

/*! @function disableInterruptOccurred
    @abstract Override $link IOInterruptEventSource::disableInterruptOccurred to make a filter callout. */
    virtual void disableInterruptOccurred(void *self, IOService *prov, int ind);

private:
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 0);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 1);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 2);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 3);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 4);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 5);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 6);
    OSMetaClassDeclareReservedUnused(IOFilterInterruptEventSource, 7);
};

#endif /* !_IOKIT_IOFILTERINTERRUPTEVENTSOURCE_H */
