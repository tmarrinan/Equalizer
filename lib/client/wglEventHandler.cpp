/* Copyright (c) 2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "wglEventHandler.h"

#include "event.h"
#include "window.h"

#include <eq/base/debug.h>
#include <eq/base/perThread.h>

#include <algorithm>
#include <windowsx.h>

using namespace eq;
using namespace eqBase;
using namespace std;

typedef hash_map< HWND, WGLEventHandler* > HandlerMap
static PerThread< HandlerMap* > _handlers;

// Win32 defines to indentify special keys
#define SCANCODE_MASK     0xff0000
#define SCANCODE_SHIFT_L  0x2a0000
#define SCANCODE_SHIFT_R  0x360000
#define RIGHT_ALT_OR_CTRL 0x1000000

#ifndef MK_XBUTTON1
#  define MK_XBUTTON1  0x20
#endif
#ifndef MK_XBUTTON2
#  define MK_XBUTTON2  0x40
#endif

class HandlerMapCleaner : public ExecutionListener
{
    HandlerMapCleaner()
        {
            Thread::addListener( this );
        }

    void notifyExecutionStopping()
        {
            HandlerMap* map = _handlers.get();
            if( map ) 
            {
                EQWARN << map->size() 
                       << " WGL event handlers registered during thread exit"
                       << endl;
                map->clear();
                delete map;
            }
        }
};
static HandlerMapCleaner _cleaner;

static void registerHandler( HWND hWnd, WGLEventHandler* handler )
{
    if( _handlers == 0 )
        _handlers = new HandlerMap;

    HandlerMap* map = _handlers.get();
    EQASSERT( map->find( hWnd ) == map->end( ));

    (*map)[hWnd] = handler;
}

static void deregisterHandler( HWND hWnd )
{
    HandlerMap* map = _handlers.get();
    EQASSERT( map )
    EQASSERT( map->find( hWnd ) != map->end( ));

    map->erase( hWnd );
    if( map->empty( ))
    {
        delete map;
        _handlers = 0;
    }
}

static WGLEventHandler* getEventHandler( HWND hWnd )
{
    HandlerMap* map = _handlers.get();
    if( !map || map->find( hWnd ) == map->end( ))
        return 0;

    return (*map)[hWnd];
}

WGLEventHandler::WGLEventHandler( Window* window )
        : _window( window ),
          _hWnd( window->getWGLWindowHandle( ))
{
    if( !_hWnd )
    {
        EQWARN << "Window has no window handle" << endl;
        return;
    }

    registerHandler( _hWnd, this );
    SetWindowLongPtr( _hWnd, GWLP_WNDPROC, (LONG_PTR)wndProc );
}

WGLEventHandler::~WGLEventHandler
{
    deregisterHandler( _hWnd );
}

LRESULT CALLBACK WGLEventHandler::wndProc( HWND hWnd, UINT uMsg, WPARAM wParam, 
                                           LPARAM lParam )
{
    WGLEventHandler* handler = getEventHandler( hWnd );
    if( !handler )
    {
        EQERROR << "Message arrived before window was registered" << endl;
        return DefWindowProc( hWnd, uMsg, wParam, lParam );
    }

    return handler->_wndProc( hWnd, uMsg, wParam, lParam );
}

void WGLEventHandler::_syncButtonState( WPARAM wParam )
{
    uint32_t buttons = PTR_BUTTON_NONE;
    if( wParam & MK_LBUTTON )  buttons |= PTR_BUTTON1;
    if( wParam & MK_MBUTTON )  buttons |= PTR_BUTTON2;
    if( wParam & MK_RBUTTON )  buttons |= PTR_BUTTON3;
    if( wParam & MK_XBUTTON1 ) buttons |= PTR_BUTTON4;
    if( wParam & MK_XBUTTON2 ) buttons |= PTR_BUTTON5;

#ifndef NDEBUG
    if( _buttonStates != buttons )

        EQWARN << "WM_MOUSEMOVE reports button state " << buttons
               << ", but internal state is " << _buttonStates << endl;
#endif

    _buttonStates = buttons;
}

LRESULT CALLBACK WGLEventHandler::_wndProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                                            LPARAM lParam )
{
    WindowEvent event;
    event.hWnd   = hWnd;
    event.uMsg   = uMsg;
    event.wParam = wParam;
    event.lParam = lParam;
    event.window = _window

    LONG result = 0;
    switch( uMsg )
    {
        case WM_CREATE:
        case WM_SIZE:
        case WM_MOVE:
        case WM_SHOWWINDOW:
        case WM_WINDOWPOSCHANGED:
        {
            event.type = WindowEvent::RESIZE;

            RECT rect;
            GetClientRect( hWnd, &rect );
            event.resize.w = rect.right - rect.left;
            event.resize.h = rect.bottom - rect.top; 

            // Get window coordinates, the rect data is relative
            // to window parent, but we report pvp relative to screen.
            POINT point;
            point.x = rect.left;
            point.y = rect.top;
            ClientToScreen( hWnd, &point );
            event.resize.x = point.x;
            event.resize.y = point.y;

            break;
        }

        case WM_CLOSE:
            event.type = WindowEvent::CLOSE;
            break;

        case WM_PAINT:
        {
            if( GetUpdateRect( hWnd, 0, false ) == 0 ) // No 'expose'
                return DefWindowProc( hWnd, uMsg, wParam, lParam );

            PAINTSTRUCT    ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);

            event.type = WindowEvent::EXPOSE;
            break;
        }

        case WM_MOUSEMOVE:
        {
            _syncButtonState( event.window, wParam );

            event.type = WindowEvent::POINTER_MOTION;
            event.pointerMotion.x = GET_X_LPARAM( lParam );
            event.pointerMotion.y = GET_Y_LPARAM( lParam );
            event.pointerMotion.buttons = _buttonStates[ event.window ];

            _computePointerDelta( event );
            break;
        }

        case WM_LBUTTONDOWN:
            _buttonStates[event.window] |= PTR_BUTTON1;
            event.type = WindowEvent::POINTER_BUTTON_PRESS;
            event.pointerButtonPress.x       = GET_X_LPARAM( lParam );
            event.pointerButtonPress.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonPress.buttons = _buttonStates[event.window];
            event.pointerButtonPress.button  = PTR_BUTTON1;

            _computePointerDelta( event );
            break;

        case WM_MBUTTONDOWN:
            _buttonStates[event.window] |= PTR_BUTTON2;
            event.type = WindowEvent::POINTER_BUTTON_PRESS;
            event.pointerButtonPress.x       = GET_X_LPARAM( lParam );
            event.pointerButtonPress.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonPress.buttons = _buttonStates[event.window];
            event.pointerButtonPress.button  = PTR_BUTTON2;

            _computePointerDelta( event );
            break;

        case WM_RBUTTONDOWN:
            _buttonStates[event.window] |= PTR_BUTTON3;
            event.type = WindowEvent::POINTER_BUTTON_PRESS;
            event.pointerButtonPress.x       = GET_X_LPARAM( lParam );
            event.pointerButtonPress.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonPress.buttons = _buttonStates[event.window];
            event.pointerButtonPress.button  = PTR_BUTTON3;

            _computePointerDelta( event );
            break;

        case WM_XBUTTONDOWN:
            event.type = WindowEvent::POINTER_BUTTON_PRESS;
            event.pointerButtonPress.x       = GET_X_LPARAM( lParam );
            event.pointerButtonPress.y       = GET_Y_LPARAM( lParam );

            if( GET_XBUTTON_WPARAM( wParam ) & XBUTTON1 )
                event.pointerButtonPress.button = PTR_BUTTON4;
            else
                event.pointerButtonPress.button = PTR_BUTTON5;

            _buttonStates[event.window] |= event.pointerButtonPress.button;
            _syncButtonState( event.window, GET_KEYSTATE_WPARAM( wParam ));
            event.pointerButtonPress.buttons = _buttonStates[event.window];

            _computePointerDelta( event );
            result = TRUE;
            break;

        case WM_LBUTTONUP:
            _buttonStates[event.window] &= ~PTR_BUTTON1;
            event.type = WindowEvent::POINTER_BUTTON_RELEASE;
            event.pointerButtonRelease.x       = GET_X_LPARAM( lParam );
            event.pointerButtonRelease.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonRelease.buttons = _buttonStates[event.window];
            event.pointerButtonRelease.button  = PTR_BUTTON1;

            _computePointerDelta( event );
            break;

        case WM_MBUTTONUP:
            _buttonStates[event.window] &= ~PTR_BUTTON2;
            event.type = WindowEvent::POINTER_BUTTON_RELEASE;
            event.pointerButtonRelease.x       = GET_X_LPARAM( lParam );
            event.pointerButtonRelease.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonRelease.buttons = _buttonStates[event.window];
            event.pointerButtonRelease.button  = PTR_BUTTON2;

            _computePointerDelta( event );
            break;

        case WM_RBUTTONUP:
            _buttonStates[event.window] &= ~PTR_BUTTON3;
            event.type = WindowEvent::POINTER_BUTTON_RELEASE;
            event.pointerButtonRelease.x       = GET_X_LPARAM( lParam );
            event.pointerButtonRelease.y       = GET_Y_LPARAM( lParam );
            event.pointerButtonRelease.buttons = _buttonStates[event.window];
            event.pointerButtonRelease.button  = PTR_BUTTON3;

            _computePointerDelta( event );
            break;

        case WM_XBUTTONUP:
            event.type = WindowEvent::POINTER_BUTTON_RELEASE;
            event.pointerButtonRelease.x       = GET_X_LPARAM( lParam );
            event.pointerButtonRelease.y       = GET_Y_LPARAM( lParam );

            if( GET_XBUTTON_WPARAM( wParam ) & XBUTTON1 )
                event.pointerButtonRelease.button = PTR_BUTTON4;
            else
                event.pointerButtonRelease.button = PTR_BUTTON5;

            _buttonStates[event.window]&=~event.pointerButtonRelease.button;
            _syncButtonState( event.window, GET_KEYSTATE_WPARAM( wParam ));
            event.pointerButtonRelease.buttons =_buttonStates[event.window];

            _computePointerDelta( event );
            result = TRUE;
            break;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            event.type = WindowEvent::KEY_PRESS;
            event.keyPress.key = _getKey( lParam, wParam );
            break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
            event.type = WindowEvent::KEY_RELEASE;
            event.keyRelease.key = _getKey( lParam, wParam );
            break;

        default:
            event.type = WindowEvent::UNHANDLED;
            EQWARN << "Unhandled message " << uMsg << endl;
            result = !0;
            break;
    }

    if( !event.window->processEvent( event ))
        return DefWindowProc( hWnd, uMsg, wParam, lParam );

    return result;
}

uint32_t WGLEventHandler::_getKey( LPARAM lParam, WPARAM wParam )
{
    switch( wParam )
    {
        case VK_ESCAPE:    return KC_ESCAPE;    
        case VK_BACK:      return KC_BACKSPACE; 
        case VK_RETURN:    return KC_RETURN;    
        case VK_TAB:       return KC_TAB;       
        case VK_HOME:      return KC_HOME;       
        case VK_LEFT:      return KC_LEFT;       
        case VK_UP:        return KC_UP;         
        case VK_RIGHT:     return KC_RIGHT;      
        case VK_DOWN:      return KC_DOWN;       
        case VK_PRIOR:     return KC_PAGE_UP;    
        case VK_NEXT:      return KC_PAGE_DOWN;  
        case VK_END:       return KC_END;        
        case VK_F1:        return KC_F1;         
        case VK_F2:        return KC_F2;         
        case VK_F3:        return KC_F3;         
        case VK_F4:        return KC_F4;         
        case VK_F5:        return KC_F5;         
        case VK_F6:        return KC_F6;         
        case VK_F7:        return KC_F7;         
        case VK_F8:        return KC_F8;         
        case VK_F9:        return KC_F9;         
        case VK_F10:       return KC_F10;        
        case VK_F11:       return KC_F11;        
        case VK_F12:       return KC_F12;        
        case VK_F13:       return KC_F13;        
        case VK_F14:       return KC_F14;        
        case VK_F15:       return KC_F15;        
        case VK_F16:       return KC_F16;        
        case VK_F17:       return KC_F17;        
        case VK_F18:       return KC_F18;        
        case VK_F19:       return KC_F19;        
        case VK_F20:       return KC_F20;        
        case VK_F21:       return KC_F21;        
        case VK_F22:       return KC_F22;        
        case VK_F23:       return KC_F23;        
        case VK_F24:       return KC_F24;  
        case VK_SHIFT:
            switch( lParam & SCANCODE_MASK )
            {
                case SCANCODE_SHIFT_L: return KC_SHIFT_L;
                case SCANCODE_SHIFT_R: return KC_SHIFT_R;
            }
            break;

        case VK_CONTROL:
            if( lParam & RIGHT_ALT_OR_CTRL )
                return KC_CONTROL_R;
            return KC_CONTROL_L;

        case VK_MENU:
            if( lParam & RIGHT_ALT_OR_CTRL )
                return KC_ALT_R;
            return KC_ALT_L;

        default: 
            // 'Useful' Latin1 characters
            if( (wParam >= '0' && wParam <= '9' ) ||
                (wParam >= 'A' && wParam <= 'Z' ))

                return wParam;
            break;
    }
    EQWARN << "Unrecognized virtual key code " << wParam << endl;
    return KC_VOID;
}
