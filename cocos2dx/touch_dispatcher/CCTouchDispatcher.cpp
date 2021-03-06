/****************************************************************************
Copyright (c) 2010-2011 cocos2d-x.org
Copyright (c) 2009      Valentin Milea

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "CCTouchDispatcher.h"
#include "CCTouchHandler.h"
#include "CCMutableArray.h"
#include "CCSet.h"
#include "CCTouch.h"
#include "CCTexture2D.h"
#include "support/data_support/ccCArray.h"

#include <assert.h>
namespace   cocos2d {

bool CCTouchDispatcher::isDispatchEvents(void)
{
	return m_bDispatchEvents;
}

void CCTouchDispatcher::setDispatchEvents(bool bDispatchEvents)
{
	m_bDispatchEvents = bDispatchEvents;
}

static CCTouchDispatcher *pSharedDispatcher = NULL;

CCTouchDispatcher* CCTouchDispatcher::sharedDispatcher(void)
{
	// synchronized ??
	if (pSharedDispatcher == NULL)
	{
		pSharedDispatcher = new CCTouchDispatcher();
		pSharedDispatcher->init();
	}

	return pSharedDispatcher;
}

/*
+(id) allocWithZone:(CCZone *)zone
{
	@synchronized(self) {
		CCAssert(sharedDispatcher == nil, @"Attempted to allocate a second instance of a singleton.");
		return [super allocWithZone:zone];
	}
	return nil; // on subsequent allocation attempts return nil
}
*/

bool CCTouchDispatcher::init(void)
{
	m_bDispatchEvents = true;
 	m_pTargetedHandlers = new CCMutableArray<CCTouchHandler*>(8);
 	m_pStandardHandlers = new CCMutableArray<CCTouchHandler*>(4);

 	m_pHandlersToAdd = new CCMutableArray<CCTouchHandler*>(8);
    m_pHandlersToRemove = ccCArrayNew(8);

	m_bToRemove = false;
	m_bToAdd = false;
	m_bToQuit = false;
	m_bLocked = false;

	m_sHandlerHelperData[ccTouchBegan].m_type = ccTouchBegan;
	m_sHandlerHelperData[ccTouchMoved].m_type = ccTouchMoved;
	m_sHandlerHelperData[ccTouchEnded].m_type = ccTouchEnded;
	m_sHandlerHelperData[ccTouchCancelled].m_type = ccTouchCancelled;

	return true;
}

CCTouchDispatcher::~CCTouchDispatcher(void)
{
 	m_pTargetedHandlers->release();
 	m_pStandardHandlers->release();
 	m_pHandlersToAdd->release();
	ccCArrayFree(m_pHandlersToRemove);

    m_pTargetedHandlers = NULL;
    m_pStandardHandlers = NULL;
    m_pHandlersToAdd = NULL;
	m_pHandlersToRemove = NULL;
}

//
// handlers management
//
void CCTouchDispatcher::forceAddHandler(CCTouchHandler *pHandler, CCMutableArray<CCTouchHandler*> *pArray)
{
	unsigned int u = 0;

 	CCMutableArray<CCTouchHandler*>::CCMutableArrayIterator iter;
 	for (iter = pArray->begin(); iter != pArray->end(); ++iter)
 	{
 		CCTouchHandler *h = *iter;
         if (h)
         {
 		    if (h->getPriority() < pHandler->getPriority())
 		    {
 			    ++u;
 		    }
 
 		    if (h->getDelegate() == pHandler->getDelegate())
 		    {
 			    assert(0);
 			    return;
 		    }
         }
 	}

	pArray->insertObjectAtIndex(pHandler, u);
}

void CCTouchDispatcher::addStandardDelegate(CCTouchDelegate *pDelegate, int nPriority)
{
	CCTouchHandler *pHandler = CCStandardTouchHandler::handlerWithDelegate(pDelegate, nPriority);
	if (! m_bLocked)
	{
		forceAddHandler(pHandler, m_pStandardHandlers);
	}
	else
	{
		m_pHandlersToAdd->addObject(pHandler);
		m_bToAdd = true;
	}
}

void CCTouchDispatcher::addTargetedDelegate(CCTouchDelegate *pDelegate, int nPriority, bool bSwallowsTouches)
{
	CCTouchHandler *pHandler = CCTargetedTouchHandler::handlerWithDelegate(pDelegate, nPriority, bSwallowsTouches);
	if (! m_bLocked)
	{
		forceAddHandler(pHandler, m_pTargetedHandlers);
	}
	else
	{
		m_pHandlersToAdd->addObject(pHandler);
		m_bToAdd = true;
	}
}

void CCTouchDispatcher::forceRemoveDelegate(CCTouchDelegate *pDelegate)
{
	CCTouchHandler *pHandler;
	CCMutableArray<CCTouchHandler*>::CCMutableArrayIterator  iter;

	// XXX: remove it from both handlers ???
	
	// remove handler from m_pStandardHandlers
	for (iter = m_pStandardHandlers->begin(); iter != m_pStandardHandlers->end(); ++iter)
	{
		pHandler = *iter;
		if (pHandler && pHandler->getDelegate() == pDelegate)
		{
			m_pStandardHandlers->removeObject(pHandler);
			break;
		}
	}


    // remove handler from m_pTargetedHandlers
	for (iter = m_pTargetedHandlers->begin(); iter != m_pTargetedHandlers->end(); ++iter)
	{
		pHandler = *iter;
		if (pHandler && pHandler->getDelegate() == pDelegate)
		{
			m_pTargetedHandlers->removeObject(pHandler);
			break;
		}
	}
}

void CCTouchDispatcher::removeDelegate(CCTouchDelegate *pDelegate)
{
	if (pDelegate == NULL)
	{
		return;
	}

	if (! m_bLocked)
	{
		forceRemoveDelegate(pDelegate);
	}
	else
	{
		ccCArrayAppendValue(m_pHandlersToRemove, pDelegate);
		m_bToRemove = true;
	}
}

void CCTouchDispatcher::forceRemoveAllDelegates(void)
{
 	m_pStandardHandlers->removeAllObjects();
 	m_pTargetedHandlers->removeAllObjects();
}

void CCTouchDispatcher::removeAllDelegates(void)
{
	if (! m_bLocked)
	{
		forceRemoveAllDelegates();
	}
	else
	{
		m_bToQuit = true;
	}
}

void CCTouchDispatcher::setPriority(int nPriority, CCTouchDelegate *pDelegate)
{
	assert(0);
}

//
// dispatch events
//
void CCTouchDispatcher::touches(CCSet *pTouches, CCEvent *pEvent, unsigned int uIndex)
{
	assert(uIndex >= 0 && uIndex < 4);

	CCSet *pMutableTouches;
	m_bLocked = true;

	// optimization to prevent a mutable copy when it is not necessary
 	unsigned int uTargetedHandlersCount = m_pTargetedHandlers->count();
 	unsigned int uStandardHandlersCount = m_pStandardHandlers->count();
	bool bNeedsMutableSet = (uTargetedHandlersCount && uStandardHandlersCount);

	pMutableTouches = (bNeedsMutableSet ? pTouches->mutableCopy() : pTouches);

	struct ccTouchHandlerHelperData sHelper = m_sHandlerHelperData[uIndex];
	//
	// process the target handlers 1st
	//
	if (uTargetedHandlersCount > 0)
	{
        CCTouch *pTouch;
		CCSetIterator setIter;
		for (setIter = pTouches->begin(); setIter != pTouches->end(); ++setIter)
		{
			pTouch = (CCTouch *)(*setIter);
			CCTargetedTouchHandler *pHandler;
			CCMutableArray<CCTouchHandler*>::CCMutableArrayIterator arrayIter;
			for (arrayIter = m_pTargetedHandlers->begin(); arrayIter != m_pTargetedHandlers->end(); ++arrayIter)
			/*for (unsigned int i = 0; i < m_pTargetedHandlers->num; ++i)*/
			{
                pHandler = (CCTargetedTouchHandler *)(*arrayIter);

                if (! pHandler)
                {
				   break;
                }

				bool bClaimed = false;
				if (uIndex == ccTouchBegan)
				{
					bClaimed = pHandler->getDelegate()->ccTouchBegan(pTouch, pEvent);
					if (bClaimed)
					{
						pHandler->getClaimedTouches()->addObject(pTouch);
					}
				} else
				if (pHandler->getClaimedTouches()->containsObject(pTouch))
				{
					// moved ended cancelled
					bClaimed = true;

					switch (sHelper.m_type)
					{
					case ccTouchMoved:
						pHandler->getDelegate()->ccTouchMoved(pTouch, pEvent);
						break;
					case ccTouchEnded:
						pHandler->getDelegate()->ccTouchEnded(pTouch, pEvent);
						pHandler->getClaimedTouches()->removeObject(pTouch);
						break;
					case ccTouchCancelled:
						pHandler->getDelegate()->ccTouchCancelled(pTouch, pEvent);
						pHandler->getClaimedTouches()->removeObject(pTouch);
						break;
					}
				}

				if (bClaimed && pHandler->isSwallowsTouches())
				{
					if (bNeedsMutableSet)
					{
						pMutableTouches->removeObject(pTouch);
					}

					break;
				}
			}
		}
	}

	//
	// process standard handlers 2nd
	//
	if (uStandardHandlersCount > 0 && pMutableTouches->count() > 0)
	{
		CCMutableArray<CCTouchHandler*>::CCMutableArrayIterator iter;
		CCStandardTouchHandler *pHandler;
		for (iter = m_pStandardHandlers->begin(); iter != m_pStandardHandlers->end(); ++iter)
		{
			pHandler = (CCStandardTouchHandler*)(*iter);

            if (! pHandler)
            {
			    break;
            }

			switch (sHelper.m_type)
			{
			case ccTouchBegan:
				pHandler->getDelegate()->ccTouchesBegan(pMutableTouches, pEvent);
				break;
			case ccTouchMoved:
				pHandler->getDelegate()->ccTouchesMoved(pMutableTouches, pEvent);
				break;
			case ccTouchEnded:
				pHandler->getDelegate()->ccTouchesEnded(pMutableTouches, pEvent);
				break;
			case ccTouchCancelled:
				pHandler->getDelegate()->ccTouchesCancelled(pMutableTouches, pEvent);
				break;
			}
		}
	}

	if (bNeedsMutableSet)
	{
		pMutableTouches->release();
	}

	//
	// Optimization. To prevent a [handlers copy] which is expensive
	// the add/removes/quit is done after the iterations
	//
	m_bLocked = false;
	if (m_bToRemove)
	{
		m_bToRemove = false;
		for (unsigned int i = 0; i < m_pHandlersToRemove->num; ++i)
		{
			forceRemoveDelegate((CCTouchDelegate*)m_pHandlersToRemove->arr[i]);
		}
		ccCArrayRemoveAllValues(m_pHandlersToRemove);
	}

	if (m_bToAdd)
	{
		m_bToAdd = false;
 		CCMutableArray<CCTouchHandler*>::CCMutableArrayIterator iter;
         CCTouchHandler *pHandler;
 		for (iter = m_pHandlersToAdd->begin(); iter != m_pHandlersToAdd->end(); ++iter)
 		{
 			pHandler = *iter;
            if (! pHandler)
            {
                break;
            }

			if (pHandler->getDelegate()->getTouchDelegateType() & ccTouchDelegateTargetedBit)
			{				
				forceAddHandler(pHandler, m_pTargetedHandlers);
			}
			else
			{
				forceAddHandler(pHandler, m_pStandardHandlers);
			}
 		}
 
 		m_pHandlersToAdd->removeAllObjects();	
	}

	if (m_bToQuit)
	{
		m_bToQuit = false;
		forceRemoveAllDelegates();
	}
}

void CCTouchDispatcher::touchesBegan(CCSet *touches, CCEvent *pEvent)
{
	if (m_bDispatchEvents)
	{
		this->touches(touches, pEvent, ccTouchBegan);
	}
}

void CCTouchDispatcher::touchesMoved(CCSet *touches, CCEvent *pEvent)
{
    if (m_bDispatchEvents)
	{
		this->touches(touches, pEvent, ccTouchMoved);
	}
}

void CCTouchDispatcher::touchesEnded(CCSet *touches, CCEvent *pEvent)
{
    if (m_bDispatchEvents)
	{
		this->touches(touches, pEvent, ccTouchEnded);
	}
}

void CCTouchDispatcher::touchesCancelled(CCSet *touches, CCEvent *pEvent)
{
    if (m_bDispatchEvents)
	{
		this->touches(touches, pEvent, ccTouchCancelled);
	}
}
}//namespace   cocos2d 
