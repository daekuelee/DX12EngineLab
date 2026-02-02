// DX12EngineLab.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DX12EngineLab.h"
#include "Engine/App.h"
#include "Renderer/DX12/ToggleSystem.h"
#include "Renderer/DX12/ImGuiLayer.h"
#include "Input/InputRouter.h"
#include "Scene/SceneContract.h"
#include <cstdio>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM

/******************************************************************************
 * FILE CONTRACT — DX12EngineLab.cpp (PR1: Input Routing Refactor)
 *
 * THREAD MODEL
 *   Single UI thread owns window, message pump, WndProc, and App::Tick.
 *
 * PUMP MODEL
 *   PeekMessage (non-blocking) -> TranslateAccelerator -> TranslateMessage -> DispatchMessage.
 *   When queue is empty, App::Tick is called for game logic and rendering.
 *
 * DISPATCH BOUNDARY
 *   WndProc must return quickly. No blocking I/O, no heavy computation.
 *
 * INPUT OWNERSHIP PRIORITY (highest to lowest)
 *   1. TranslateAccelerator — menu accelerators (Alt+F4, etc.)
 *   2. ImGui forwarding     — unconditional, before engine checks
 *   3. ImGui capture check  — blocks engine hotkeys if WantsKeyboard()
 *   4. InputRouter          — engine hotkeys (edge-gated), mouse forwarding
 *   5. DefWindowProc        — unhandled messages
 *
 * PR1 SCOPE
 *   Hotkey edge gating + mouse forward only.
 *   WASD movement remains polled in App::Tick via GetAsyncKeyState.
 *
 * PROOF POINTS
 *   [PROOF-KEYEDGE] — Hold T: one toggle, repeats show isRepeat=1
 *   [PROOF-IMGUI]   — Focus ImGui text, press T: blocked, log shows captured=1
 *   [PROOF-ACCEL]   — Alt+F4 closes window (TranslateAccelerator path)
 *   [PROOF-MOUSE-SMOKE] — Mouse look works in 3P mode
 ******************************************************************************/

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND g_hWnd = nullptr;                          // main window handle

// Application instance
static Engine::App g_app;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

/******************************************************************************
 * wWinMain CONTRACT
 *   - PeekMessage (non-blocking) allows Tick when queue is empty.
 *   - TranslateAccelerator called before DispatchMessage for menu shortcuts.
 *   - WM_QUIT terminates loop; exactly-once g_app.Shutdown() on exit.
 ******************************************************************************/
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DX12ENGINELAB, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DX12ENGINELAB));

    MSG msg = {};

    // Main game loop using PeekMessage for non-blocking message processing
    bool running = true;
    while (running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }

            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (running)
        {
            // Update and render when no messages are pending
            g_app.Tick();
        }
    }

    // Shutdown the application
    g_app.Shutdown();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DX12ENGINELAB));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DX12ENGINELAB);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   g_hWnd = hWnd; // Store window handle globally

   // Initialize the application
   if (!g_app.Initialize(hWnd))
   {
      return FALSE;
   }

   // Initialize InputRouter (PR1: table-driven hotkey routing)
   InputRouter::Initialize(&g_app);

   // Run Scene contract self-test (Debug-only)
   Scene::RunContractSelfTest();

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

/******************************************************************************
 * WndProc CONTRACT
 *   - ImGui forwarding FIRST and UNCONDITIONALLY.
 *   - Input messages (WM_KEYDOWN/UP, WM_MOUSEMOVE, WM_KILLFOCUS) -> InputRouter.
 *   - If InputRouter returns true, message is consumed; return 0.
 *   - If InputRouter returns false, fall through to DefWindowProc.
 *   - lParam bit30 = previous key state, used for edge detection.
 *   - FORBIDDEN: blocking I/O, heavy computation, frame-rate logic.
 ******************************************************************************/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // CONTRACT: ImGui forwarding FIRST, unconditionally
    Renderer::ImGuiLayer::WndProcHandler(hWnd, message, wParam, lParam);

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // Rendering is handled by Dx12Context, not GDI
            (void)hdc;
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_MOUSEMOVE:
    case WM_KILLFOCUS:
        // Delegate to InputRouter; correct fallthrough if not handled
        if (InputRouter::OnWin32Message(hWnd, message, wParam, lParam))
            return 0;
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
