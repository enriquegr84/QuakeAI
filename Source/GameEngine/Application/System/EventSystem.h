// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.2 (2011/08/13)

#ifndef EVENTSYSTEM_H
#define EVENTSYSTEM_H

#include "KeyCodes.h"

#include "Mathematic/Algebra/Vector2.h"

static unsigned int LocaleIdToCodepage(unsigned int lcid)
{
	switch (lcid)
	{
	case 1098:  // Telugu
	case 1095:  // Gujarati
	case 1094:  // Punjabi
	case 1103:  // Sanskrit
	case 1111:  // Konkani
	case 1114:  // Syriac
	case 1099:  // Kannada
	case 1102:  // Marathi
	case 1125:  // Divehi
	case 1067:  // Armenian
	case 1081:  // Hindi
	case 1079:  // Georgian
	case 1097:  // Tamil
		return 0;
	case 1054:  // Thai
		return 874;
	case 1041:  // Japanese
		return 932;
	case 2052:  // Chinese (PRC)
	case 4100:  // Chinese (Singapore)
		return 936;
	case 1042:  // Korean
		return 949;
	case 5124:  // Chinese (Macau S.A.R.)
	case 3076:  // Chinese (Hong Kong S.A.R.)
	case 1028:  // Chinese (Taiwan)
		return 950;
	case 1048:  // Romanian
	case 1060:  // Slovenian
	case 1038:  // Hungarian
	case 1051:  // Slovak
	case 1045:  // Polish
	case 1052:  // Albanian
	case 2074:  // Serbian (Latin)
	case 1050:  // Croatian
	case 1029:  // Czech
		return 1250;
	case 1104:  // Mongolian (Cyrillic)
	case 1071:  // FYRO Macedonian
	case 2115:  // Uzbek (Cyrillic)
	case 1058:  // Ukrainian
	case 2092:  // Azeri (Cyrillic)
	case 1092:  // Tatar
	case 1087:  // Kazakh
	case 1059:  // Belarusian
	case 1088:  // Kyrgyz (Cyrillic)
	case 1026:  // Bulgarian
	case 3098:  // Serbian (Cyrillic)
	case 1049:  // Russian
		return 1251;
	case 8201:  // English (Jamaica)
	case 3084:  // French (Canada)
	case 1036:  // French (France)
	case 5132:  // French (Luxembourg)
	case 5129:  // English (New Zealand)
	case 6153:  // English (Ireland)
	case 1043:  // Dutch (Netherlands)
	case 9225:  // English (Caribbean)
	case 4108:  // French (Switzerland)
	case 4105:  // English (Canada)
	case 1110:  // Galician
	case 10249:  // English (Belize)
	case 3079:  // German (Austria)
	case 6156:  // French (Monaco)
	case 12297:  // English (Zimbabwe)
	case 1069:  // Basque
	case 2067:  // Dutch (Belgium)
	case 2060:  // French (Belgium)
	case 1035:  // Finnish
	case 1080:  // Faroese
	case 1031:  // German (Germany)
	case 3081:  // English (Australia)
	case 1033:  // English (United States)
	case 2057:  // English (United Kingdom)
	case 1027:  // Catalan
	case 11273:  // English (Trinidad)
	case 7177:  // English (South Africa)
	case 1030:  // Danish
	case 13321:  // English (Philippines)
	case 15370:  // Spanish (Paraguay)
	case 9226:  // Spanish (Colombia)
	case 5130:  // Spanish (Costa Rica)
	case 7178:  // Spanish (Dominican Republic)
	case 12298:  // Spanish (Ecuador)
	case 17418:  // Spanish (El Salvador)
	case 4106:  // Spanish (Guatemala)
	case 18442:  // Spanish (Honduras)
	case 3082:  // Spanish (International Sort)
	case 13322:  // Spanish (Chile)
	case 19466:  // Spanish (Nicaragua)
	case 2058:  // Spanish (Mexico)
	case 10250:  // Spanish (Peru)
	case 20490:  // Spanish (Puerto Rico)
	case 1034:  // Spanish (Traditional Sort)
	case 14346:  // Spanish (Uruguay)
	case 8202:  // Spanish (Venezuela)
	case 1089:  // Swahili
	case 1053:  // Swedish
	case 2077:  // Swedish (Finland)
	case 5127:  // German (Liechtenstein)
	case 1078:  // Afrikaans
	case 6154:  // Spanish (Panama)
	case 4103:  // German (Luxembourg)
	case 16394:  // Spanish (Bolivia)
	case 2055:  // German (Switzerland)
	case 1039:  // Icelandic
	case 1057:  // Indonesian
	case 1040:  // Italian (Italy)
	case 2064:  // Italian (Switzerland)
	case 2068:  // Norwegian (Nynorsk)
	case 11274:  // Spanish (Argentina)
	case 1046:  // Portuguese (Brazil)
	case 1044:  // Norwegian (Bokmal)
	case 1086:  // Malay (Malaysia)
	case 2110:  // Malay (Brunei Darussalam)
	case 2070:  // Portuguese (Portugal)
		return 1252;
	case 1032:  // Greek
		return 1253;
	case 1091:  // Uzbek (Latin)
	case 1068:  // Azeri (Latin)
	case 1055:  // Turkish
		return 1254;
	case 1037:  // Hebrew
		return 1255;
	case 5121:  // Arabic (Algeria)
	case 15361:  // Arabic (Bahrain)
	case 9217:  // Arabic (Yemen)
	case 3073:  // Arabic (Egypt)
	case 2049:  // Arabic (Iraq)
	case 11265:  // Arabic (Jordan)
	case 13313:  // Arabic (Kuwait)
	case 12289:  // Arabic (Lebanon)
	case 4097:  // Arabic (Libya)
	case 6145:  // Arabic (Morocco)
	case 8193:  // Arabic (Oman)
	case 16385:  // Arabic (Qatar)
	case 1025:  // Arabic (Saudi Arabia)
	case 10241:  // Arabic (Syria)
	case 14337:  // Arabic (U.A.E.)
	case 1065:  // Farsi
	case 1056:  // Urdu
	case 7169:  // Arabic (Tunisia)
		return 1256;
	case 1061:  // Estonian
	case 1062:  // Latvian
	case 1063:  // Lithuanian
		return 1257;
	case 1066:  // Vietnamese
		return 1258;
	}
	return 65001;   // utf-8
}

//! Enumeration for all event types there are.
enum CORE_ITEM EventType
{
	/*
	UI events are created by the UI environment or the UI elements in response to mouse
	or keyboard events. When a UI element receives an event it will either process it and
	return true, or pass the event to its parent. If an event is not absorbed before it
	reaches the root element then it will then be passed to the user receiver.
	*/
	//! An event of the graphical user interface.
	ET_UI_EVENT = 0,

	/*
	Mouse events are created by the device and passed to Device::PostEvent in response to
	mouse input received from the operating system. Mouse events are first passed to the user
	receiver, then to the UI environment and its elements, then finally the input receiving
	scene manager where it is passed to the active camera.
	*/
	//! A mouse input event.
	ET_MOUSE_INPUT_EVENT,


	/*
	Like mouse events, keyboard events are created by the device and passed to Device::PostEvent.
	They take the same path as mouse events.
	*/
	//! A key input event.
	ET_KEY_INPUT_EVENT,

    //! A string input event.
    /** This event is created when multiple characters are sent at a time (e.g. using an IME). */
    ET_STRING_INPUT_EVENT,

    //! A touch input event.
    ET_TOUCH_INPUT_EVENT,

	/*
	Log events are only passed to the user receiver if there is one. If they are absorbed by
	the user receiver then no text will be sent to the console.
	*/
	//! A log event
	ET_LOG_TEXT_EVENT,

	/*
	This is not used by GameEngine and can be used to send user specific data though the system.
	The usage and behavior depends on the operating system:

	Windows: send a WM_USER message to the GameEngine Window; the wParam and lParam will be used
	to populate the UserData1 and UserData2 members of the struct UserEvent.

	Linux: send a ClientMessage via XSendEvent to the GameEngine window; the data.l[0] and
	data.l[1] members will be casted to signed int and used as UserData1 and UserData2.

	MacOS: Not yet implemented
	*/
	//! A user event with user data.
	ET_USER_EVENT,

    //! Pass on raw events from the OS
    ET_SYSTEM_EVENT,

    //! Application state events like a resume, pause etc.
    ET_APPLICATION_EVENT

};

//! Enumeration for all mouse input events
enum CORE_ITEM MouseInputEvent
{
	//! Left mouse button was pressed down.
	MIE_LMOUSE_PRESSED_DOWN = 0,

	//! Right mouse button was pressed down.
	MIE_RMOUSE_PRESSED_DOWN,

	//! Middle mouse button was pressed down.
	MIE_MMOUSE_PRESSED_DOWN,

	//! Left mouse button was left up.
	MIE_LMOUSE_LEFT_UP,

	//! Right mouse button was left up.
	MIE_RMOUSE_LEFT_UP,

	//! Middle mouse button was left up.
	MIE_MMOUSE_LEFT_UP,

	//! The mouse cursor changed its position.
	MIE_MOUSE_MOVED,

	//! The mouse wheel was moved. Use Wheel value in event data to find out
	//! in what direction and how fast.
	MIE_MOUSE_WHEEL,

	//! Left mouse button double click.
	//! This event is generated after the second MIE_LMOUSE_PRESSED_DOWN event.
	MIE_LMOUSE_DOUBLE_CLICK,

	//! Right mouse button double click.
	//! This event is generated after the second MIE_RMOUSE_PRESSED_DOWN event.
	MIE_RMOUSE_DOUBLE_CLICK,

	//! Middle mouse button double click.
	//! This event is generated after the second MIE_MMOUSE_PRESSED_DOWN event.
	MIE_MMOUSE_DOUBLE_CLICK,

	//! Left mouse button triple click.
	//! This event is generated after the third MIE_LMOUSE_PRESSED_DOWN event.
	MIE_LMOUSE_TRIPLE_CLICK,

	//! Right mouse button triple click.
	//! This event is generated after the third MIE_RMOUSE_PRESSED_DOWN event.
	MIE_RMOUSE_TRIPLE_CLICK,

	//! Middle mouse button triple click.
	//! This event is generated after the third MIE_MMOUSE_PRESSED_DOWN event.
	MIE_MMOUSE_TRIPLE_CLICK,

    //! Mouse enters canvas used for rendering.
    //! Only generated on emscripten
    MIE_MOUSE_ENTER_CANVAS,

    //! Mouse leaves canvas used for rendering.
    //! Only generated on emscripten
    MIE_MOUSE_LEAVE_CANVAS,

	//! No real event. Just for convenience to get number of events
	MIE_COUNT
};

//! Masks for mouse button states
enum CORE_ITEM MouseButtonStateMask
{
	MBSM_LEFT = 0x01,
	MBSM_RIGHT = 0x02,
	MBSM_MIDDLE = 0x04,

	//! currently only on windows
	MBSM_EXTRA1 = 0x08,

	//! currently only on windows
	MBSM_EXTRA2 = 0x10,

	MBSM_FORCE_32_BIT = 0x7fffffff
};

class BaseUIElement;

//! Enumeration for all touch input events
enum CORE_ITEM TouchInputEvent
{
    //! Touch was pressed down.
    TIE_PRESSED_DOWN = 0,

    //! Touch was left up.
    TIE_LEFT_UP,

    //! The touch changed its position.
    TIE_MOVED,

    //! No real event. Just for convenience to get number of events
    TIE_COUNT
};

//! Enumeration for all events which are sendable by the gui system
enum CORE_ITEM UIEventType
{
	//! A gui element has lost its focus.
	/** GUIEvent.Caller is losing the focus to GUIEvent.Element.
	If the event is absorbed then the focus will not be changed. */
	UIEVT_ELEMENT_FOCUS_LOST = 0,

	//! A gui element has got the focus.
	/** If the event is absorbed then the focus will not be changed. */
	UIEVT_ELEMENT_FOCUSED,

	//! The mouse cursor hovered over a gui element.
	/** If an element has sub-elements you also get this message for the subelements */
	UIEVT_ELEMENT_HOVERED,

	//! The mouse cursor left the hovered element.
	/** If an element has sub-elements you also get this message for the subelements */
	UIEVT_ELEMENT_LEFT,

	//! An element would like to close.
	/** Windows and context menus use this event when they would like to close,
	this can be cancelled by absorbing the event. */
	UIEVT_ELEMENT_CLOSED,

	//! A button was clicked.
	UIEVT_BUTTON_CLICKED,

	//! A scrollbar has changed its position.
	UIEVT_SCROLL_BAR_CHANGED,

	//! A checkbox has changed its check state.
	UIEVT_CHECKBOX_CHANGED,

	//! A new item in a listbox was selected.
	/** NOTE: You also get this event currently when the same item was clicked again after more than 500 ms. */
	UIEVT_LISTBOX_CHANGED,

	//! An item in the listbox was selected, which was already selected.
	/** NOTE: You get the event currently only if the item was clicked again within 500 ms or selected by "enter" or "space". */
	UIEVT_LISTBOX_SELECTED_AGAIN,

    //! A file open dialog has been closed without choosing a file
    UIEVT_FILE_CHOOSE_DIALOG_CANCELLED,

    //! 'Yes' was clicked on a messagebox
    UIEVT_MESSAGEBOX_YES,

    //! 'No' was clicked on a messagebox
    UIEVT_MESSAGEBOX_NO,

    //! 'OK' was clicked on a messagebox
    UIEVT_MESSAGEBOX_OK,

    //! 'Cancel' was clicked on a messagebox
    UIEVT_MESSAGEBOX_CANCEL,

	//! In an editbox 'ENTER' was pressed
	UIEVT_EDITBOX_ENTER,

	//! The text in an editbox was changed. This does not include automatic changes in text-breaking.
	UIEVT_EDITBOX_CHANGED,

	//! The marked area in an editbox was changed.
	UIEVT_EDITBOX_MARKING_CHANGED,

	//! The tab was changed in an tab control
	UIEVT_TAB_CHANGED,

	//! A menu item was selected in a (context) menu
	UIEVT_MENU_ITEM_SELECTED,

	//! The selection in a combo box has been changed
	UIEVT_COMBO_BOX_CHANGED,

    //! The value of a spin box has changed
    UIEVT_SPINBOX_CHANGED,

    //! A table has changed
    UIEVT_TABLE_CHANGED,
    UIEVT_TABLE_HEADER_CHANGED,
    UIEVT_TABLE_SELECTED_AGAIN,

	//! A tree view node lost selection. See IGUITreeView::getLastEventNode().
	UIEVT_TREEVIEW_NODE_DESELECT,

	//! A tree view node was selected. See IGUITreeView::getLastEventNode().
	UIEVT_TREEVIEW_NODE_SELECT,

	//! A tree view node was expanded. See IGUITreeView::getLastEventNode().
	UIEVT_TREEVIEW_NODE_EXPAND,

	//! A tree view node was collapsed. See IGUITreeView::getLastEventNode().
	UIEVT_TREEVIEW_NODE_COLLAPSE,

	//! deprecated - use EGET_TREEVIEW_NODE_COLLAPSE instead. This
	UIEVT_TREEVIEW_NODE_COLLAPS = UIEVT_TREEVIEW_NODE_COLLAPSE,

	//! No real event. Just for convenience to get number of events
	UIEVT_COUNT
};


//! Bitflags for defining the the focus behavior of the gui
// (all names start with SET as we might add REMOVE flags later to control that behavior as well)
enum UIFocusFlag
{
    //! When set the focus changes when the left mouse-button got clicked while over an element
    UIFF_SET_ON_LMOUSE_DOWN = 0x1,

    //! When set the focus changes when the right mouse-button got clicked while over an element
    //! Note that elements usually don't care about right-click and that won't change with this flag
    //! This is mostly to allow taking away focus from elements with right-mouse additionally.
    UIFF_SET_ON_RMOUSE_DOWN = 0x2,

    //! When set the focus changes when the mouse-cursor is over an element
    UIFF_SET_ON_MOUSE_OVER = 0x4,

    //! When set the focus can be changed with TAB-key combinations.
    UIFF_SET_ON_TAB = 0x8,

    //! When set it's possible to set the focus to disabled elements.
    UIFF_CAN_FOCUS_DISABLED = 0x16
};

//! Events hold information about an event. See EventReceiver for details on event handling.
struct Event
{
	//! Any kind of UI event.
	struct UIEvent
	{
		//! BaseUIElement who called the event
		BaseUIElement* mCaller;

		//! If the event has something to do with another element, it will be held here.
		BaseUIElement* mElement;

		//! Type of UI Event
		UIEventType mEventType;

	};

	//! Any kind of mouse event.
	struct MouseInput
	{
		//! X position of mouse cursor
		int X;

		//! Y position of mouse cursor
		int Y;

		/* Only valid if event was MIE_MOUSE_WHEEL */
		//! mouse wheel delta, often 1.0 or -1.0, but can have other values < 0.f or > 0.f;
		float mWheel;

		//! True if shift was also pressed
		bool mShift : 1;

		//! True if ctrl was also pressed
		bool mControl : 1;

		//! A bitmap of button states. You can use isButtonPressed() to determine
		//! if a button is pressed or not.
		//! Currently only valid if the event was MIE_MOUSE_MOVED
		unsigned int mButtonStates;

		//! Is the left button pressed down?
		bool IsLeftPressed() const { return 0 != (mButtonStates & MBSM_LEFT); }

		//! Is the right button pressed down?
		bool IsRightPressed() const { return 0 != (mButtonStates & MBSM_RIGHT); }

		//! Is the middle button pressed down?
		bool IsMiddlePressed() const { return 0 != (mButtonStates & MBSM_MIDDLE); }

		//! Type of mouse event
		MouseInputEvent mEvent;
	};

	//! Any kind of keyboard event.
	struct KeyInput
	{
		//! Character corresponding to the key (0, if not a character)
		wchar_t mChar;

		//! Key which has been pressed or released
        KeyCode mKey;

		//! If not true, then the key was left up
		bool mPressedDown : 1;

		//! True if shift was also pressed
		bool mShift : 1;

		//! True if ctrl was also pressed
		bool mControl : 1;
	};

    //! String input event.
    struct StringInput
    {
        //! The string that is entered
        wchar_t* mStr;
    };

    //! Any kind of touch event.
    struct TouchInput
    {
        // Touch ID.
        size_t mID;

        // X position of simple touch.
        int mX;

        // Y position of simple touch.
        int mY;

        // number of current touches
        int mTouchedCount;

        //! Type of touch event.
        TouchInputEvent mEvent;
    };


	//! Any kind of user event.
	struct UserEvent
	{
		//! Some user specified data as int
		int mUserData1;

		//! Another user specified data as int
		int mUserData2;
	};

	EventType mEventType;
	union
	{
		struct UIEvent		mUIEvent;
		struct MouseInput	mMouseInput;
		struct KeyInput		mKeyInput;
        struct TouchInput   mTouchInput;
        struct StringInput  mStringInput;
		struct UserEvent	mUserEvent;
	};

};

////////////////////////////////////////////////////
//
// BaseKeyboardHandler Description		- Chapter 9, page 242
// BaseMouseHandler Description		- Chapter 9, page 242
// BaseGamepadHandler Description		- Chapter 9, page 242
//
// These are the public APIs for any object that implements reactions
// to events sent by hardware user interface devices.
//
////////////////////////////////////////////////////

class BaseKeyboardHandler
{
public:
	virtual bool OnKeyDown(const Event::KeyInput& input) = 0;
	virtual bool OnKeyUp(const Event::KeyInput& input) = 0;
};

class BaseMouseHandler
{
public:
	virtual bool OnWheelRollUp() = 0;
	virtual bool OnWheelRollDown() = 0;

	virtual bool OnMouseMove(const Vector2<int> &pos, const int radius) = 0;
	virtual bool OnMouseButtonDown(
		const Vector2<int> &pos, const int radius, const std::string &buttonName) = 0;
	virtual bool OnMouseButtonUp(
		const Vector2<int> &pos, const int radius, const std::string &buttonName) = 0;
};

class BaseGamepadHandler
{
	virtual bool OnTrigger(const std::string &triggerName, float const pressure) = 0;
	virtual bool OnButtonDown(const std::string &buttonName, int const pressure) = 0;
	virtual bool OnButtonUp(const std::string &buttonName) = 0;
	virtual bool OnDirectionalPad(const std::string &direction) = 0;
	virtual bool OnThumbstick(const std::string &stickName, float const x, float const y) = 0;
};

/*
Supporting system for interactive application posses the capability to accept events. They can be
from different kind, such device input (mouse and keyboard) graphic user interface (gui) or user
events. Events usually start at a PostEvent function and are passed down through a chain of event 
receivers until OnEvent returns true. See EEVENT_TYPE for a description of where each type of event 
starts, and the path it takes through the system.
*/
//! Interface of an object which can listen events.
class EventListener
{
public:

	//! Destructor
	virtual ~EventListener() {}

	/*
	Please take care that the method only returns 'true' inr order to prevent the GameEngine
	from processing the event any further. So 'true' does mean that an event is completely done.
	Therefore your return value for all unprocessed events should be 'false'.
	\return True if the event was processed.
	*/
	//! Called if an event happened.
	virtual bool OnEvent(const Event& ev) = 0;
};

#endif //EVENTSYSTEM_H