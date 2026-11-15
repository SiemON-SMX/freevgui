// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2024-2026 Alibek Omarov

#include "support.h"

using namespace vgui;

void vgui::XashKey( VGUI_KeyAction action, VGUI_KeyCode code )
{
	if( !g_surface )
		return;

	App *app = App::getInstance();

	// VGUI_KeyCode is laid out to match vgui::KeyCode exactly
	switch( action )
	{
	case KA_PRESSED:
		app->internalKeyPressed( (KeyCode)code, g_surface );
		break;
	case KA_RELEASED:
		app->internalKeyReleased( (KeyCode)code, g_surface );
		break;
	case KA_TYPED:
		app->internalKeyTyped( (KeyCode)code, g_surface );
		break;
	}
}

void vgui::XashMouse( VGUI_MouseAction action, int code )
{
	if( !g_surface )
		return;

	App *app = App::getInstance();

	// VGUI_MouseCode is laid out to match vgui::MouseCode exactly
	switch( action )
	{
	case MA_PRESSED:
		{
			int x, y;

			g_surface->GetMousePos( x, y );
			app->internalCursorMoved( x, y, g_surface );
		}
		app->internalMousePressed( (MouseCode)code, g_surface );
		break;
	case MA_RELEASED:
		{
			int x, y;

			g_surface->GetMousePos( x, y );
			app->internalCursorMoved( x, y, g_surface );
		}
		app->internalMouseReleased( (MouseCode)code, g_surface );
		break;
	case MA_DOUBLE:
		app->internalMouseDoublePressed( (MouseCode)code, g_surface );
		break;
	case MA_WHEEL:
		app->internalMouseWheeled( code, g_surface ); // here the code is a signed wheel delta, not a button
		break;
	}
}

void vgui::XashEnableTextInput( bool enable, bool force )
{
	if( g_engine && g_engine->EnableTextInput )
		g_engine->EnableTextInput( enable ? 1 : 0, force ? 1 : 0 );
}

int vgui::g_lastCursorPos[2];
bool vgui::g_lastCursorPosValid = false;

void vgui::XashMouseMove( int x, int y )
{
	g_lastCursorPos[0] = x;
	g_lastCursorPos[1] = y;
	g_lastCursorPosValid = true;

	if( !g_surface )
		return;

	// already scaled out of physical pixels by the engine
	App::getInstance()->internalCursorMoved( x, y, g_surface );
}

// text from the android keyboard, turn it into normal key presses
void vgui::XashTextInput( const char *text )
{
	if( !text || !g_surface )
		return;

	App *app = App::getInstance();
	if( !app )
		return;

	for( const unsigned char *p = (const unsigned char *)text; *p; ++p )
	{
		if( *p == '\b' || *p == 0x7f )
		{
			app->internalKeyTyped( KEY_BACKSPACE, g_surface );
			continue;
		}

		if( *p == '\r' || *p == '\n' )
		{
			app->internalKeyTyped( KEY_ENTER, g_surface );
			continue;
		}

		if( *p < 0x20 || *p > 0x7e )
			continue;

		bool found = false;
		for( int key = KEY_0; key < KEY_LAST && !found; ++key )
		{
			for( int shift = 0; shift <= 1; ++shift )
			{
				if( app->getKeyCodeChar( (KeyCode)key, shift != 0 ) != (char)*p )
					continue;

				if( shift )
					app->internalKeyPressed( KEY_LSHIFT, g_surface );
				app->internalKeyPressed( (KeyCode)key, g_surface );
				app->internalKeyTyped( (KeyCode)key, g_surface );
				app->internalKeyReleased( (KeyCode)key, g_surface );
				if( shift )
					app->internalKeyReleased( KEY_LSHIFT, g_surface );
				found = true;
				break;
			}
		}
	}
}
