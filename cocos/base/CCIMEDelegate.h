/****************************************************************************
Copyright (c) 2010 cocos2d-x.org
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

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

#ifndef __CC_IME_DELEGATE_H__
#define __CC_IME_DELEGATE_H__

#include <string>
#include "math/CCGeometry.h"
#include "base/CCEventKeyboard.h"

// AWFramework addition
//// UITextInput: UITextPosition
typedef size_t AWTextPosition;
static const AWTextPosition AWTextPosition_INVALID = SIZE_T_MAX;

// AWFramework addition
//// UITextInput: UITextRange
class AWTextRange {

public:

    bool isInvalid;
    AWTextPosition startPosition;
    AWTextPosition endPosition;

    bool isEmpty() const
    {
        return startPosition == endPosition;
    }

    size_t getLength() const
    {
        return endPosition - startPosition;
    }

    AWTextRange()
    {
        isInvalid = true;
        startPosition = AWTextPosition_INVALID;
        endPosition = AWTextPosition_INVALID;
    }

    AWTextRange(AWTextPosition start, AWTextPosition end)
    {
        isInvalid = start == AWTextPosition_INVALID || end == AWTextPosition_INVALID;
        startPosition = start;
        endPosition = end;
    }
};

// AWFramework addition
//// UITextInput: UITextLayoutDirection
typedef enum AWTextLayoutDirection {
    AWTextLayoutDirection_UP,
    AWTextLayoutDirection_RIGHT,
    AWTextLayoutDirection_DOWN,
    AWTextLayoutDirection_LEFT
} AWTextLayoutDirection;

// AWFramework addition
//// UITextInput: UITextWritingDirection
typedef enum AWTextWritingDirection {
    AWTextWritingDirection_NATURAL,
    AWTextWritingDirection_LEFT_TO_RIGHT,
    AWTextWritingDirection_RIGHT_TO_LEFT
} AWTextWritingDirection;

// AWFramework addition
//// UITextInput: UITextStorageDirection
typedef enum AWTextStorageDirection {
    AWTextStorageDirection_FORWARD,
    AWTextStorageDirection_BACKWARD
} AWTextStorageDirection;

// AWFramework addition
//// UITextInput: ComparisonResult
typedef enum AWTextPositionComparison : int {
    AWTextPositionComparison_ASCENDING  = -1,
    AWTextPositionComparison_DESCENDING = 1,
    AWTextPositionComparison_EQUAL      = 0
} AWTextPositionComparison;

/**
 * @addtogroup base
 * @{
 */
NS_CC_BEGIN

/**
 * A static global empty std::string install.
 */
extern const std::string CC_DLL STD_STRING_EMPTY;


/**
 * Keyboard notification event type.
 */
typedef struct
{
    Rect  begin;              // the soft keyboard rectangle when animation begins
    Rect  end;                // the soft keyboard rectangle when animation ends
    float     duration;           // the soft keyboard animation duration
} IMEKeyboardNotificationInfo;

/**
 *@brief    Input method editor delegate.
 */
class CC_DLL IMEDelegate
{
public:
    /**
     * Default constructor.
     * @js NA
     * @lua NA
     */
    virtual ~IMEDelegate();
    
    /**
     * Default destructor.
     * @js NA
     * @lua NA
     */
    virtual bool attachWithIME();
    
    /**
     * Determine whether the IME is detached or not.
     * @js NA
     * @lua NA
     */
    virtual bool detachWithIME();

protected:
    friend class IMEDispatcher;

    /**
    @brief    Decide if the delegate instance is ready to receive an IME message.

    Called by IMEDispatcher.
    * @js NA
    * @lua NA
    */
    virtual bool canAttachWithIME() { return false; }
    /**
    @brief    When the delegate detaches from the IME, this method is called by IMEDispatcher.
    * @js NA
    * @lua NA
    */
    virtual void didAttachWithIME() {}

    /**
    @brief    Decide if the delegate instance can stop receiving IME messages.
    * @js NA
    * @lua NA
    */
    virtual bool canDetachWithIME() { return false; }

    /**
    @brief    When the delegate detaches from the IME, this method is called by IMEDispatcher.
    * @js NA
    * @lua NA
    */
    virtual void didDetachWithIME() {}

    /**
    @brief    Called by IMEDispatcher when text input received from the IME.
    * @js NA
    * @lua NA
    */

    // AWFramework implementation
    //// UIKeyInput: func insertText(String)
    virtual void insertText(const char * text, size_t len)
    {
        AWTextRange selectedRange = selectedTextRange();
        AWTextRange markedRange = markedTextRange();

        if (!markedRange.isInvalid) {
            replaceText(markedRange, std::string(text));
            selectedRange.startPosition = markedRange.startPosition + len;
            selectedRange.endPosition = selectedRange.startPosition;
            selectedRange.isInvalid = false;
            setSelectedTextRange(selectedRange);
            unmarkText();
        }
        else if (!selectedRange.isInvalid && selectedRange.getLength() > 0) {
            replaceText(selectedRange, std::string(text));
            selectedRange.startPosition += len;
            selectedRange.endPosition = selectedRange.startPosition;
            setSelectedTextRange(selectedRange);
        }
        else {
            if (selectedRange.isInvalid) {
                selectedRange.startPosition = endOfDocument();
                selectedRange.endPosition = selectedRange.startPosition;
                selectedRange.isInvalid = false;
                setSelectedTextRange(selectedRange);
            }
            replaceText(selectedRange, std::string(text));
        }
    }

    /**
    @brief    Called by IMEDispatcher after the user clicks the backward key.
    * @js NA
    * @lua NA
    */

    // AWFramework implementation
    //// UIKeyInput: func deleteBackward()
    virtual void deleteBackward()
    {
        AWTextRange selectedRange = selectedTextRange();
        AWTextRange markedRange = markedTextRange();
        AWTextPosition beginning = beginningOfDocument();
        AWTextPosition end = endOfDocument();

        if (!markedRange.isInvalid) {
            replaceText(markedRange, STD_STRING_EMPTY);
            selectedRange.startPosition = markedRange.startPosition;
            selectedRange.endPosition = selectedRange.startPosition;
            selectedRange.isInvalid = false;
            setSelectedTextRange(selectedRange);
            unmarkText();
        }
        else if (!selectedRange.isInvalid && selectedRange.getLength() > 0) {
            replaceText(selectedRange, STD_STRING_EMPTY);
            selectedRange.endPosition = selectedRange.startPosition;
            setSelectedTextRange(selectedRange);
        }
        else if (beginning != end) {
            if (selectedRange.isInvalid) {
                selectedRange.startPosition = end - 1;
                selectedRange.endPosition = end;
                selectedRange.isInvalid = false;
                setSelectedTextRange(selectedRange);
            }
            else if (selectedRange.startPosition > beginning) {
                selectedRange.startPosition -= 1;
                setSelectedTextRange(selectedRange);
            }
            replaceText(selectedRange, STD_STRING_EMPTY);
            selectedRange.endPosition = selectedRange.startPosition;
            setSelectedTextRange(selectedRange);
        }
    }

    /**
    @brief    Called by IMEDispatcher after the user press control key.
    * @js NA
    * @lua NA
    */
    virtual void controlKey(EventKeyboard::KeyCode /*keyCode*/) {}

    /**
    @brief    Called by IMEDispatcher for text stored in delegate.
    * @js NA
    * @lua NA
    */
    virtual const std::string& getContentText() { return STD_STRING_EMPTY; }

    //////////////////////////////////////////////////////////////////////////
    // keyboard show/hide notification
    //////////////////////////////////////////////////////////////////////////
    /**
     * @js NA
     * @lua NA
     */
    virtual void keyboardWillShow(IMEKeyboardNotificationInfo& /*info*/)   {}
    /**
     * @js NA
     * @lua NA
     */
    virtual void keyboardDidShow(IMEKeyboardNotificationInfo& /*info*/)    {}
    /**
     * @js NA
     * @lua NA
     */
    virtual void keyboardWillHide(IMEKeyboardNotificationInfo& /*info*/)   {}
    /**
     * @js NA
     * @lua NA
     */
    virtual void keyboardDidHide(IMEKeyboardNotificationInfo& /*info*/)    {}

    // AWFramework addition
    //// UIKeyInput: var hasText: Bool
    virtual bool hasText()
    {
        return false;
    }

    // AWFramework addition
    //// UITextInput: func text(in: UITextRange)
    virtual std::string textInRange(const AWTextRange& range)
    {
        return STD_STRING_EMPTY;
    }

    // AWFramework addition
    //// UITextInput: func replace(UITextRange, withText: String)
    virtual void replaceText(const AWTextRange& range, const std::string& replacementText)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: func shouldChangeText(in: UITextRange, replacementText: String)
    virtual bool shouldChangeText(const AWTextRange& range, const std::string& replacementText)
    {
        return true;
    }

    // AWFramework addition
    //// UITextInput: Not part of protocol
    virtual void setSelectedTextRange(const AWTextRange& range)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: var selectedTextRange: UITextRange?
    virtual AWTextRange selectedTextRange()
    {
        return AWTextRange();
    }

    // AWFramework addition
    //// UITextInput: Not part of protocol
    virtual void setMarkedTextRange(const AWTextRange& range)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: var markedTextRange: UITextRange?
    virtual AWTextRange markedTextRange()
    {
        return AWTextRange();
    }

    // AWFramework addition
    //// UITextInput: var markedTextStyle: [AnyHashable : Any]?
    //    - (NSDictionary *)markedTextStyle

    // AWFramework addition
    //// UITextInput: func setMarkedText(String?, selectedRange: NSRange)
    virtual void setMarkedText(const std::string& text, const AWTextRange& selectedRange)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: func unmarkText()
    virtual void unmarkText()
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: var selectionAffinity: UITextStorageDirection
    virtual void setSelectionAffinity(AWTextStorageDirection direction)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: var selectionAffinity: UITextStorageDirection
    virtual AWTextStorageDirection selectionAffinity()
    {
        return AWTextStorageDirection::AWTextStorageDirection_BACKWARD;
    }

    // AWFramework addition
    //// UITextInput: func textRange(from: UITextPosition, to: UITextPosition)
    virtual AWTextRange textRangeBetweenPositions(const AWTextPosition& startPosition, const AWTextPosition& endPosition)
    {
        return AWTextRange(MIN(startPosition, endPosition), MAX(startPosition, endPosition));
    }

    // AWFramework addition
    //// UITextInput: func position(from: UITextPosition, offset: Int)
    virtual AWTextPosition offsetTextPosition(const AWTextPosition& startPosition, const AWTextPosition& offset)
    {
        AWTextPosition position = startPosition + offset;
        if (position < beginningOfDocument() || position > endOfDocument()) {
            return AWTextPosition_INVALID;
        }
        else {
            return position;
        }
    }

    // AWFramework addition
    //// UITextInput: func position(from: UITextPosition, in: UITextLayoutDirection, offset: Int)
    virtual AWTextPosition offsetTextPosition(const AWTextPosition& startPosition, const AWTextLayoutDirection& direction, const AWTextPosition& offset)
    {
        if (direction == AWTextLayoutDirection::AWTextLayoutDirection_RIGHT) {
            return offsetTextPosition(startPosition, offset);
        }
        else if (direction == AWTextLayoutDirection::AWTextLayoutDirection_LEFT) {
            return offsetTextPosition(startPosition, -offset);
        }
        else {
            return AWTextPosition_INVALID;
        }
    }

    // AWFramework addition
    //// UITextInput: var beginningOfDocument: UITextPosition
    virtual AWTextPosition beginningOfDocument()
    {
        return 0;
    }

    // AWFramework addition
    //// UITextInput: var endOfDocument: UITextPosition
    virtual AWTextPosition endOfDocument()
    {
        return getContentText().size();
    }

    // AWFramework addition
    //// UITextInput: func compare(UITextPosition, to: UITextPosition)
    virtual AWTextPositionComparison comparePositions(const AWTextPosition& firstPosition, const AWTextPosition& secondPosition)
    {
        if (firstPosition < secondPosition) {
            return AWTextPositionComparison::AWTextPositionComparison_ASCENDING;
        }
        else if (firstPosition > secondPosition) {
            return AWTextPositionComparison::AWTextPositionComparison_DESCENDING;
        }
        else {
            return AWTextPositionComparison::AWTextPositionComparison_EQUAL;
        }
    }

    // AWFramework addition
    //// UITextInput: func offset(from: UITextPosition, to: UITextPosition)
    virtual size_t offsetBetweenTextPositions(const AWTextPosition& startPosition, const AWTextPosition& endPosition)
    {
        return endPosition - startPosition;
    }

    // AWFramework addition
    //// UITextInput: func position(within: UITextRange, farthestIn: UITextLayoutDirection)
    virtual AWTextPosition farthestPosition(const AWTextRange& range, const AWTextLayoutDirection& direction)
    {
        if (direction == AWTextLayoutDirection::AWTextLayoutDirection_LEFT || direction == AWTextLayoutDirection::AWTextLayoutDirection_UP) {
            return range.startPosition;
        }
        else {
            return range.endPosition;
        }
    }

    // AWFramework addition
    //// UITextInput: func characterRange(byExtending: UITextPosition, in: UITextLayoutDirection)
    virtual AWTextRange farthestRange(const AWTextPosition& startPosition, const AWTextLayoutDirection& direction)
    {
        if (direction == AWTextLayoutDirection::AWTextLayoutDirection_LEFT || direction == AWTextLayoutDirection::AWTextLayoutDirection_UP) {
            return AWTextRange(beginningOfDocument(), startPosition);
        }
        else {
            return AWTextRange(startPosition, endOfDocument());
        }
    }

    // AWFramework addition
    //// UITextInput: func baseWritingDirection(for: UITextPosition, in: UITextStorageDirection)
    virtual AWTextWritingDirection baseWritingDirection(const AWTextPosition& position, const AWTextStorageDirection& direction)
    {
        return AWTextWritingDirection::AWTextWritingDirection_NATURAL;
    }

    // AWFramework addition
    //// UITextInput: func setBaseWritingDirection(UITextWritingDirection, for: UITextRange)
    virtual void setBaseWritingDirection(const AWTextWritingDirection& direction, const AWTextRange& range)
    {
        // Do nothing
    }

    // AWFramework addition
    //// UITextInput: func firstRect(for: UITextRange)
    virtual Rect firstRect(const AWTextRange& range)
    {
        return Rect::ZERO;
    }

    // AWFramework addition
    //// UITextInput: func caretRect(for: UITextPosition)
    virtual Rect caretRect(const AWTextPosition& position)
    {
        return Rect::ZERO;
    }

    // AWFramework addition
    //// UITextInput: func closestPosition(to: CGPoint)
    virtual AWTextPosition closestPosition(const Point& point)
    {
        return AWTextPosition_INVALID;
    }

    // AWFramework addition
    //// UITextInput: func selectionRects(for: UITextRange)
    virtual std::vector<Rect> selectionRects(const AWTextRange& range)
    {
        return {};
    }

    // AWFramework addition
    //// UITextInput: func closestPosition(to: CGPoint, within: UITextRange)
    virtual AWTextPosition closestPosition(const Point& point, const AWTextRange& range)
    {
        return AWTextPosition_INVALID;
    }

    // AWFramework addition
    //// UITextInput: func characterRange(at: CGPoint)
    virtual AWTextRange characterRange(const Point& point)
    {
        return AWTextRange();
    }

protected:
    /**
     * @js NA
     * @lua NA
     */
    IMEDelegate();
};


NS_CC_END
// end of base group
/// @}

#endif    // __CC_IME_DELEGATE_H__
