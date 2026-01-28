// DX12EngineLab.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DX12EngineLab.h"
#include "Engine/App.h"
#include "Renderer/DX12/ToggleSystem.h"
#include "Renderer/DX12/ImGuiLayer.h"
#include <cstdio>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM

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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

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

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Forward all messages to ImGui (ignore return value, do not early-return)
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
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_KEYDOWN:
        {
            // Skip engine key handling if ImGui wants keyboard
            if (Renderer::ImGuiLayer::WantsKeyboard())
            {
                break;
            }

            // 'T' key toggles draw mode (instanced <-> naive)
            if (wParam == 'T')
            {
                Renderer::ToggleSystem::ToggleDrawMode();
                Renderer::ToggleSystem::RequestDiagnosticLog(); // Trigger diagnostic log on next frame
                OutputDebugStringA("Draw mode toggled\n");
            }
            // F1 key toggles sentinel_Instance0 proof (moved from '1' to avoid camera preset collision)
            else if (wParam == VK_F1)
            {
                bool current = Renderer::ToggleSystem::IsSentinelInstance0Enabled();
                Renderer::ToggleSystem::SetSentinelInstance0(!current);
                OutputDebugStringA(current ? "sentinel_Instance0: OFF\n" : "sentinel_Instance0: ON\n");
            }
            // F2 key toggles stomp_Lifetime proof (moved from '2' to avoid camera preset collision)
            else if (wParam == VK_F2)
            {
                bool current = Renderer::ToggleSystem::IsStompLifetimeEnabled();
                Renderer::ToggleSystem::SetStompLifetime(!current);
                OutputDebugStringA(current ? "stomp_Lifetime: OFF\n" : "stomp_Lifetime: ON\n");
            }
            // 'G' key toggles grid (cubes) visibility for floor debugging
            else if (wParam == 'G')
            {
                Renderer::ToggleSystem::ToggleGrid();
                OutputDebugStringA(Renderer::ToggleSystem::IsGridEnabled() ? "Grid: ON\n" : "Grid: OFF\n");
            }
            // 'C' key cycles color mode (FaceDebug -> InstanceID -> Lambert)
            else if (wParam == 'C')
            {
                Renderer::ToggleSystem::CycleColorMode();
                char buf[64];
                sprintf_s(buf, "ColorMode = %s\n", Renderer::ToggleSystem::GetColorModeName());
                OutputDebugStringA(buf);
            }
            // 'U' key toggles upload diagnostic mode (Day2)
            else if (wParam == 'U')
            {
                Renderer::ToggleSystem::ToggleUploadDiag();
                OutputDebugStringA(Renderer::ToggleSystem::IsUploadDiagEnabled()
                    ? "UploadDiag: ON\n" : "UploadDiag: OFF\n");
            }
            // 'V' key toggles camera mode (Day3)
            else if (wParam == 'V')
            {
                Renderer::ToggleSystem::ToggleCameraMode();
                char buf[64];
                sprintf_s(buf, "CameraMode: %s\n", Renderer::ToggleSystem::GetCameraModeName());
                OutputDebugStringA(buf);
            }
            // F9 key toggles debug single instance mode (MT2)
            else if (wParam == VK_F9)
            {
                Renderer::ToggleSystem::ToggleDebugSingleInstance();
                char buf[64];
                sprintf_s(buf, "DebugSingleInstance: %s (idx=%u)\n",
                    Renderer::ToggleSystem::IsDebugSingleInstanceEnabled() ? "ON" : "OFF",
                    Renderer::ToggleSystem::GetDebugInstanceIndex());
                OutputDebugStringA(buf);
            }
            // F6 key toggles controller mode (Day3.11)
            else if (wParam == VK_F6)
            {
                g_app.ToggleControllerMode();
            }
            // F7 key toggles step-up grid test (Day3.12+)
            else if (wParam == VK_F7)
            {
                g_app.ToggleStepUpGridTest();
            }
            // F8 key toggles HUD verbose mode (Day3.12+)
            else if (wParam == VK_F8)
            {
                Renderer::ToggleSystem::ToggleHudVerbose();
                OutputDebugStringA(Renderer::ToggleSystem::IsHudVerboseEnabled()
                    ? "[HUD] Verbose: ON\n" : "[HUD] Verbose: OFF\n");
            }
            // 'O' key toggles opaque PSO (Task B: blend/depth sanity test)
            else if (wParam == 'O')
            {
                Renderer::ToggleSystem::ToggleOpaquePSO();
                OutputDebugStringA(Renderer::ToggleSystem::IsOpaquePSOEnabled()
                    ? "OpaquePSO: ON\n" : "OpaquePSO: OFF\n");
            }
        }
        break;
    case WM_MOUSEMOVE:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            g_app.OnMouseMove(xPos, yPos);
        }
        break;
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
