#include "..\\win32.cpp"
#include <gl/gl.h>
#include <math.h>
#include <windows.h>

Window window;

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_SYSCOMMAND: {
    // handle alt
    if (wParam == SC_KEYMENU) {
      return 0;
    }
    break;
  }
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (wParam == VK_F11) {
      window.isFullscreen = !window.isFullscreen;
      SetFullscreen(window.windowHandle, window.isFullscreen);
    }
    if (wParam == 'Q') {
      PostQuitMessage(0);
      window.isTerminated = 1;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    window.isTerminated = 1;
    break;
  case WM_SIZE:
    OnSize(&window, lParam);
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

float t;
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  PreventWindowsDPIScaling();

  window = {.width = 1400,
            .height = 2100,
            .bg = 0x222222,
            .title = "Editor",
            .isFullscreen = 0,
            .onEvent = OnEvent};

  OpenWindow(&window);
  Win32InitOpenGL(window);

  while (!window.isTerminated) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    t += 0.01;
    float r = (sinf(t) + 1.0f) / 2.0f;

    glClearColor(r, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers(window.dc);
  }
  return 0;
}
