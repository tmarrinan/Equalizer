/* Copyright (c) 2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQ_WGLEVENTHANDLER_H
#define EQ_WGLEVENTHANDLER_H

#include <eq/client/eventHandler.h>

#include <eq/client/event.h>
#include <eq/client/windowEvent.h>

namespace eq
{
    /**
     * The event processing for WGL.
     *
     * The WGL implementation does not use a thread, since messages are handled
     * by a 'wndproc' callback in the thread which created the window. Each
     * window has its own WGLEventHandler
     */
    class EQ_EXPORT WGLEventHandler : public EventHandler
    {
    public:
        /** Constructs a new wgl event thread. */
        WGLEventHandler( Window* window );

        /** Destructs the wgl event thread. */
        virtual ~WGLEventHandler(){}
        
        static LRESULT CALLBACK wndProc( HWND hWnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam );
    private:
        const Window* _window;
        const HWND    _hWnd;
        uint32_t      _buttonStates;

        LRESULT CALLBACK _wndProc( HWND hWnd, UINT uMsg, WPARAM wParam, 
                                   LPARAM lParam );

        void      _syncButtonState( WPARAM wParam );
        uint32_t  _getKey( LPARAM lParam, WPARAM wParam );
    };
}

#endif // EQ_WGLEVENTHANDLER_H

