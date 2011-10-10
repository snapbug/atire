/*
	CANVAS.C
	--------
*/
#include <windows.h>
#include "resources/resource.h"
#include "canvas.h"

#define VK_SCROLLLOCK (0x91)

/*
	ANT_CANVAS::ANT_CANVAS()
	------------------------
*/
ANT_canvas::ANT_canvas(HINSTANCE hInstance)
{
this->hInstance = hInstance;
}

/*
	ANT_CANVAS::MAKE_CANVAS()
	-------------------------
*/
void ANT_canvas::make_canvas(void)
{
HGDIOBJ bmp;
BITMAPINFO256 info;

info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
info.bmiHeader.biWidth = WIDTH_IN_PIXELS;
info.bmiHeader.biHeight = -HEIGHT_IN_PIXELS;			// -ve for a top-down DIB
info.bmiHeader.biPlanes = 1;
info.bmiHeader.biBitCount = 8;
info.bmiHeader.biCompression = BI_RGB;		// not compressed
info.bmiHeader.biSizeImage = 0;
info.bmiHeader.biXPelsPerMeter = 0;
info.bmiHeader.biYPelsPerMeter = 0;
info.bmiHeader.biClrUsed = 0;
info.bmiHeader.biClrImportant = 0;
memset(info.bmiColors, 0, sizeof(info.bmiColors));
memcpy(info.bmiColors, pallette, sizeof(pallette));		// copy the Poly's colour pallette
bitmap = CreateCompatibleDC(NULL);
bmp = CreateDIBSection(bitmap, (BITMAPINFO *)&info, DIB_RGB_COLORS, (void **)&canvas, NULL, 0);
SelectObject(bitmap, bmp);
}

/*
	ANT_CANVAS::MENU()
	------------------
*/
void ANT_canvas::menu(WORD clicked)
{
switch (clicked)
	{
	/*
		FILE MENU
	*/
	case ID_FILE_RESET:
		client->reset();
		break;

	case ID_FILE_EXIT:
		PostQuitMessage(0);
		break;
	}
}

/*
	ANT_CANVAS::WINDOWS_CALLBACK()
	------------------------------
*/
LRESULT ANT_canvas::windows_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
long long cycles_spent, cycles_to_spend, cycles_per_key_event;
HDC hDC; 
RECT client_size, window_size;
PAINTSTRUCT paintStruct;
unsigned char poly_key;
unsigned long display_context;
poly **current_client;

switch(message)
	{
	case WM_CREATE:
		GetClientRect(hwnd, &client_size);
		GetWindowRect(hwnd, &window_size);
		SetWindowPos(hwnd, HWND_TOP, window_size.top, window_size.left, WIDTH_IN_PIXELS + (WIDTH_IN_PIXELS - client_size.right), 2 * HEIGHT_IN_PIXELS + (2 * HEIGHT_IN_PIXELS - client_size.bottom), 0);
		make_canvas();
		set_rom_version(ID_ROMS_BASIC_23);
		return 0;

	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if ((poly_key = translate_key(wParam)) != 0)
			client->putch(poly_key, 1);
		return 0;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		if ((poly_key = translate_key(wParam)) != 0)
			client->putch(poly_key, 0);

	case WM_PAINT:
		memset(canvas, PALLETTE_BACKGROUND_BASE + ((client->pia->in_b() >> 4) & 0x07), WIDTH_IN_PIXELS * HEIGHT_IN_PIXELS);
		display_context = client->pia->in_b() << 8 | client->pia->in_a();
		
		if ((display_context & 0x0210) == 0x0200)			// screen 4 (no 480)
			client->graphics_screen->render_page(canvas, client->memory + 0x8000, (client->pia->in_b() & 0x0C) >> 2);

		if ((display_context & 0x0100) == 0x0100)			// screen 3
			client->screen_3->paint_text_page(canvas, text_flash_state);

		if ((display_context & 0x0030) == 0x0030)			// screen 5
			client->graphics_screen->render_combined_pages(canvas, client->memory + 0x4000, client->memory + 0x8000);

		if ((display_context & 0x0030) == 0x0020)			// screen 2
			client->graphics_screen->render_page(canvas, client->memory + 0x4000, (client->pia->in_a() & 0x06) >> 1);

		if ((display_context & 0x0270) == 0x0260)			// screens 2 and 4 colour mixed (the routine below only draws mixed pixels)
			client->graphics_screen->render_mixed_pages(canvas, client->memory + 0x4000, client->memory + 0x8000);

		if ((display_context & 0x0008) == 0x0008)			// screen 1
			client->screen_1->paint_text_page(canvas, text_flash_state);

		hDC = BeginPaint(hwnd, &paintStruct);
		StretchBlt(hDC, 0, 0, WIDTH_IN_PIXELS, 2 * HEIGHT_IN_PIXELS, bitmap, 0, 0, WIDTH_IN_PIXELS, HEIGHT_IN_PIXELS, SRCCOPY);
		EndPaint(hwnd, &paintStruct);
		return 0;

	case WM_TIMER:
		if (wParam == TIMER_TEXT_FLASH)
			{
			text_flash_state = !text_flash_state;
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_INTERNALPAINT);
			}
		else if (wParam == TIMER_DISPLAY_REFRESH)
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_INTERNALPAINT);
		else if (wParam == TIMER_CPU_TICK)
			{
			/*
				Run for a 10th of an emulated second (clock speed / 10)
			*/
			cycles_spent = client->cycles; 
			cycles_to_spend = client->cpu_frequency / 10;
			cycles_per_key_event = cycles_to_spend / 100;
			while (client->cycles < cycles_spent + cycles_to_spend)
				{
				if (client->cycles % cycles_per_key_event == 0)
					{
					client->process_next_keyboard_event();
					if (clients != NULL)
						for (current_client = clients; *current_client != NULL; current_client++)
							(*current_client)->process_next_keyboard_event();
					}
				client->step();
				if (clients != NULL)
					for (current_client = clients; *current_client != NULL; current_client++)
						(*current_client)->step();
				if (server != NULL)
					server->step();
				}

			/*
				Now refresh the screen
			*/
			if (client->changes != 0)
				{
				RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_INTERNALPAINT);
				client->changes = 0;
				}
			}
		return 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
			{
			/*
				Look, see, I didn't write windows!  the ESCAPE key is encoded as an IDCANCEL command!
			*/
			if ((poly_key = translate_key(VK_ESCAPE)) != 0)
				client->putch(poly_key, 1);
			}
		else
			menu(LOWORD(wParam));
		return 0;
	}
return (DefWindowProc(hwnd,message,wParam,lParam));
}

/*
	WINDOWS_CALLBACK()
	------------------
*/
static LRESULT CALLBACK windows_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
ANT_canvas *object = NULL;

if (message == WM_CREATE)
	{
	object = (ANT_canvas *)((CREATESTRUCT *)lParam)->lpCreateParams;
	object->window = hwnd;
	return object->windows_callback(hwnd, message, wParam, lParam);
	}
else if ((object = (poly_unit *)GetWindowLong(hwnd, GWL_USERDATA)) != NULL)
	return object->windows_callback(hwnd, message, wParam, lParam);
else
	return (DefWindowProc(hwnd,message,wParam,lParam));
}

/*
	ANT_CANVAS::CREATE_WINDOW()
	---------------------------
*/
long ANT_canvas::create_window(char *window_title)
{
HWND window;
WNDCLASSEX windowClass;

this->hInstance = hInstance;
windowClass.cbSize = sizeof(WNDCLASSEX);
windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
windowClass.lpfnWndProc = ::windows_callback;
windowClass.cbClsExtra = 0;
windowClass.cbWndExtra = 0;
windowClass.hInstance = 0;
windowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_POLY));
windowClass.hCursor = NULL;
windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
windowClass.lpszMenuName = NULL;
windowClass.lpszClassName = "ATIRE-Edit";
windowClass.hIconSm = NULL;

RegisterClassEx(&windowClass);

window = CreateWindowEx(NULL,			// extended style
	"Edit",					// class name
	window_title,					// window name
	WS_EX_OVERLAPPEDWINDOW | WS_BORDER| WS_SYSMENU | WS_VISIBLE,
	120, 120,			// x/y coords
	WIDTH_IN_PIXELS,	// width
	2 * HEIGHT_IN_PIXELS,	// height
	NULL,				// handle to parent
	LoadMenu(0, MAKEINTRESOURCE(IDR_MENU_POLY)),				// Menu
	0,					// application instance
	this);				// no extra parameter's

SetWindowLongPtr(window, GWL_USERDATA, this);

UpdateWindow(window);

return 0;
}

