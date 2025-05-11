#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <shlwapi.h> // For PathRemoveFileSpec and PathRemoveExtension

#pragma comment(lib, "shlwapi.lib") // Link the necessary library for path functions

#define ID_RATE_INPUT 101
#define ID_TEXT_LABEL 102
#define ID_DROP_TARGET 103

HWND hWndRateInput, hWndLabel;
wchar_t rateValue[10] = L"1/6"; // Default rate value (wide string)
wchar_t filepath[MAX_PATH], folderpath[MAX_PATH], basename[MAX_PATH];

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void OnDropFile(HWND hwnd, HDROP hDrop);
void RunFFMPEG(wchar_t* filepath, wchar_t* rate, wchar_t* folderpath, wchar_t* basename);
void GetFileInfo(wchar_t* filepath, wchar_t* folderpath, wchar_t* basename);

// Replace the standard `main` with the `WinMain` entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"Vid2Screens"; // Use wide string class name
    wc.hInstance = hInstance;
    RegisterClass(&wc);

    // Create the window
    HWND hwnd = CreateWindowEx(0, L"Vid2Screens", L"Vid2Screens",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL);

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_RATE_INPUT) {
            // Handle rate input change if necessary
            return 0;
        }
        break;
    case WM_DROPFILES:
        OnDropFile(hwnd, (HDROP)wParam);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateControls(HWND hwnd) {
    // Create a label for "Rate"
    CreateWindow(L"STATIC", L"Rate", WS_VISIBLE | WS_CHILD,
        10, 10, 80, 20, hwnd, (HMENU)ID_TEXT_LABEL, NULL, NULL);

    // Create an input field for rate value
    hWndRateInput = CreateWindow(L"EDIT", rateValue, WS_VISIBLE | WS_CHILD | WS_BORDER,
        100, 10, 80, 20, hwnd, (HMENU)ID_RATE_INPUT, NULL, NULL);

    // Create a label explaining the rate
    CreateWindow(L"STATIC", L"1. Set Rate as decimal or fraction (smaller is faster)\n2. Drag and drop a video file anywhere\n3. Wait", WS_VISIBLE | WS_CHILD,
        10, 50, 380, 60, hwnd, NULL, NULL, NULL);

    // Allow drag-and-drop
    DragAcceptFiles(hwnd, TRUE);
}

void CleanBasename(wchar_t* basename) {
    // Clean invalid characters for Windows file paths
    for (int i = 0; basename[i] != L'\0'; ++i) {
        if (basename[i] == L'<' || basename[i] == L'>' || basename[i] == L':' ||
            basename[i] == L'"' || basename[i] == L'/' || basename[i] == L'\\' ||
            basename[i] == L'|' || basename[i] == L'?' || basename[i] == L'*') {
            basename[i] = L'_'; // Replace invalid characters with underscore
        }
    }
}

void OnDropFile(HWND hwnd, HDROP hDrop) {
    // Get the file path from the dropped item
    DragQueryFile(hDrop, 0, filepath, MAX_PATH);
    DragFinish(hDrop);

    // Get the basename and folderpath
    GetFileInfo(filepath, folderpath, basename);

    // Clean the basename to remove invalid characters
    CleanBasename(basename);

    // Read the rate value from the input field
    GetWindowText(hWndRateInput, rateValue, sizeof(rateValue) / sizeof(wchar_t));

    // Create the output folder path with a timestamp
    wchar_t outputFolder[MAX_PATH];

    // Get current time
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Format timestamp as yyyy-mm-dd_hh_mm_ss
    wchar_t timestamp[32];
    swprintf_s(timestamp, 32, L"%04d-%02d-%02d_%02d-%02d-%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // Combine base name and timestamp in the folder path
    swprintf_s(outputFolder, MAX_PATH, L"%s\\vid2screens-%s-%s", folderpath, basename, timestamp);


    // Debug output for folder path construction
    wprintf(L"Output folder path: %s\n", outputFolder);  // Debug: print the output folder path

    // Check if the folder exists, and if not, create it
    if (!PathFileExists(outputFolder)) {
        // Try to create the directory and check the result
        if (CreateDirectory(outputFolder, NULL) == 0) {
            // If folder creation failed, handle the error
            DWORD dwError = GetLastError();

            // Prepare the error message
            wchar_t errorMessage[512];
            swprintf(errorMessage, sizeof(errorMessage) / sizeof(wchar_t),
                L"Failed to create output folder: %s. Error code: %lu", outputFolder, dwError);

            // Get the error description from the system
            LPVOID lpMsgBuf;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, dwError, 0, (LPWSTR)&lpMsgBuf, 0, NULL);

            // Display the error message
            MessageBox(hwnd, (LPWSTR)lpMsgBuf, L"Error", MB_OK | MB_ICONERROR);

            // Free the buffer allocated by FormatMessage
            LocalFree(lpMsgBuf);
            return; // Exit early if the folder could not be created
        }
    }

    // Run ffmpeg command if folder was successfully created (or already exists)
    RunFFMPEG(filepath, rateValue, outputFolder, basename);
}


void GetFileInfo(wchar_t* filepath, wchar_t* folderpath, wchar_t* basename) {
    // Get the folder path
    wcscpy_s(folderpath, MAX_PATH, filepath);  // Safe copy with bounds checking
    PathRemoveFileSpec(folderpath);  // Removes the file name part, leaving the folder path

    // Extract the filename (basename) without the path, but don't modify the original filepath
    wchar_t tempFilePath[MAX_PATH];
    wcscpy_s(tempFilePath, MAX_PATH, filepath);  // Create a temporary copy of filepath
    PathStripPath(tempFilePath);  // Modify tempFilePath to just the filename with extension

    // Copy the filename (including extension) to basename
    wcscpy_s(basename, MAX_PATH, tempFilePath);

    // Remove the file extension from the basename
    PathRemoveExtension(basename);  // Remove the extension from the basename
}

#include <windows.h>
#include <shlwapi.h>   // For PathRemoveFileSpec
#include <shellapi.h>  // For ShellExecute if needed
#include <stdio.h>

void RunFFMPEG(wchar_t* filepath, wchar_t* rate, wchar_t* folderpath, wchar_t* basename) {
    // Get the path of this executable
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath); // Remove the filename

    // Build full path to ffmpeg.exe
    wchar_t ffmpegPath[MAX_PATH];
    swprintf_s(ffmpegPath, MAX_PATH, L"%s\\ffmpeg.exe", exePath);

    // Construct ffmpeg command (quoted)
    wchar_t ffmpegCommand[1024];
    swprintf_s(ffmpegCommand, 1024, L"\"%s\" -i \"%s\" -r %s \"%s\\%%05d.jpg\" & timeout 3 & exit",
        ffmpegPath, filepath, rate, folderpath);

    // Show the command that will run
    wchar_t preview[1200];
    swprintf_s(preview, 1200, L"The following ffmpeg command will be executed in a console window:\n\n%s", ffmpegCommand);
    MessageBox(NULL, preview, L"FFmpeg Command Preview", MB_OK | MB_ICONINFORMATION);

    // Create full cmd line: cmd.exe /k "ffmpeg ..."
    wchar_t cmdLine[1300];
    swprintf_s(cmdLine, 1300, L"cmd.exe /k \"%s\"", ffmpegCommand);

    // STARTUPINFO and PROCESS_INFORMATION
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Create the process
    BOOL success = CreateProcess(
        NULL,         // App name (NULL because in command line)
        cmdLine,      // Command line
        NULL,         // Process handle not inheritable
        NULL,         // Thread handle not inheritable
        FALSE,        // Do not inherit handles
        0,            // No creation flags
        NULL,         // Use parent's environment block
        NULL,         // Use parent's starting directory 
        &si,          // Pointer to STARTUPINFO structure
        &pi);         // Pointer to PROCESS_INFORMATION structure

    if (!success) {
        DWORD err = GetLastError();
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&lpMsgBuf,
            0, NULL);

        wchar_t errMsg[512];
        swprintf_s(errMsg, 512, L"Failed to run ffmpeg. Error %lu:\n\n%s", err, (LPWSTR)lpMsgBuf);
        MessageBox(NULL, errMsg, L"Error", MB_OK | MB_ICONERROR);

        LocalFree(lpMsgBuf);
    }
    else {
        // Close handles to avoid leaks
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
