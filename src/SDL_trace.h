/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#ifndef _SDL_trace_h
#define _SDL_trace_h

#ifndef NDEBUG
#define SDL_TRACE(msg, ...) fprintf(stderr, "SDL: " msg, ##__VA_ARGS__)
#else
#define SDL_TRACE(...)
#endif

#define SDL_log(msg, ...) printf("SDL: " msg, ##__VA_ARGS__)

#endif
