// KinectV2Recorder.cpp
//
// Author: Po-Chen Wu (pcwu0329@gmail.com)
//
// These codes are written mainly based on codes from Kinect for Windows SDK 2.0
// https://www.microsoft.com/en-us/download/details.aspx?id=44561


#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "KinectV2Recorder.h"
#include <algorithm>
#include <vector>
#include <queue>

#include <ctime>
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"



#include "mscl/Communication/Connection.h"
#include "mscl/MicroStrain/Inertial/InertialNode.h"
#include "mscl/Exceptions.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <fstream>

#include <shlobj.h>//getting Document path under windows
#pragma comment(lib, "shell32.lib")

#ifdef USE_IPP
#include <ippi.h>
#endif

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
    )
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CKinectV2Recorder application;
    application.Run(hInstance, nShowCmd);
}

/// <summary>
/// Constructor
/// </summary>
CKinectV2Recorder::CKinectV2Recorder() :
m_hWnd(NULL),
m_nStartTime(0),
m_nInfraredLastCounter(0),
m_nDepthLastCounter(0),
m_nColorLastCounter(0),
m_nInfraredFramesSinceUpdate(0),
m_nDepthFramesSinceUpdate(0),
m_nColorFramesSinceUpdate(0),
m_fFreq(0),
m_nNextStatusTime(0LL),
m_bRecord(false),
m_bShot(false),
m_bShotReady(false),
m_bSelect2D(true),
m_pKinectSensor(NULL),
m_pInfraredFrameReader(NULL),
m_pDepthFrameReader(NULL),
m_pColorFrameReader(NULL),
m_pD2DFactory(NULL),
m_pDrawInfrared(NULL),
m_pDrawDepth(NULL),
m_pDrawColor(NULL),
m_pInfraredRGBX(NULL),
m_pDepthRGBX(NULL),
m_pColorRGBX(NULL),
m_nInfraredIndex(0),
m_nDepthIndex(0),
m_nColorIndex(0),
m_nModel2DIndex(0),
m_nModel3DIndex(0),
m_nTypeIndex(0),
m_nLevelIndex(0),
m_nSideIndex(0),
m_tSaveThread(),
m_bStopThread(false),
m_tSaveThreadIMU(),
m_bStopThreadIMU(false),
m_myfile(NULL),
m_bIMUEnable(true)
{
    LARGE_INTEGER qpf = { 0 };
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }

    // create heap storage for infrared pixel data in RGBX format
    m_pInfraredRGBX = new RGBQUAD[cInfraredWidth * cInfraredHeight];

    // create heap storage for depth pixel data in RGBX  & UINT16 format
    m_pDepthRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];

    // create heap storage for color pixel data in RGBX format
    m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];

    for (int i = 0; i < BufferSize; ++i)
    {
        // create heap storage for infrared pixel data in UINT16 format
        m_pInfraredUINT16[i] = new UINT16[cInfraredWidth * cInfraredHeight];

        // create heap storage for depth pixel data in UINT16 format
        m_pDepthUINT16[i] = new UINT16[cDepthWidth * cDepthHeight];

        // create heap storage for color pixel data in RGB format
        m_pColorRGB[i] = new RGBTRIPLE[cColorWidth * cColorHeight];
    }
    
    // create heap storage for file lists
    m_vInfraredList.reserve(1800);
    m_vDepthList.reserve(1800);
    m_vColorList.reserve(1800);

	std::string strWidth = getIni("setting.ini", "KINECT", "rgb_width");
	std::string strHeight = getIni("setting.ini", "KINECT", "rgb_height");
	
	m_iRGBwidth = std::stoi(strWidth);
	m_iRGBheight = std::stoi(strHeight);


	// Get user specified path
	m_strMyDocumentPath = getIni("setting.ini", "SYSTEM", "save_path");
	if (m_strMyDocumentPath == "")
	{
		// Get user Document folder path
		WCHAR my_documents[MAX_PATH];
		HRESULT result = SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, my_documents);
		m_strMyDocumentPath = WCHARtoString(my_documents);
	}

}


/// <summary>
/// Destructor
/// </summary>
CKinectV2Recorder::~CKinectV2Recorder()
{
    // clean up Direct2D renderer
    if (m_pDrawInfrared)
    {
        delete m_pDrawInfrared;
        m_pDrawInfrared = NULL;
    }

    if (m_pDrawDepth)
    {
        delete m_pDrawDepth;
        m_pDrawDepth = NULL;
    }

    if (m_pDrawColor)
    {
        delete m_pDrawColor;
        m_pDrawColor = NULL;
    }

    if (m_pInfraredRGBX)
    {
        delete[] m_pInfraredRGBX;
        m_pInfraredRGBX = NULL;
    }

    if (m_pDepthRGBX)
    {
        delete[] m_pDepthRGBX;
        m_pDepthRGBX = NULL;
    }

    if (m_pColorRGBX)
    {
        delete[] m_pColorRGBX;
        m_pColorRGBX = NULL;
    }

    for (int i = 0; i < BufferSize; ++i)
    {
        if (m_pInfraredUINT16[i])
        {
            delete[] m_pInfraredUINT16[i];
            m_pInfraredUINT16[i] = NULL;
        }

        if (m_pDepthUINT16[i])
        {
            delete[] m_pDepthUINT16[i];
            m_pDepthUINT16[i] = NULL;
        }

        if (m_pColorRGB[i])
        {
            delete[] m_pColorRGB[i];
            m_pColorRGB[i] = NULL;
        }
    }

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    // done with infrared frame reader
    SafeRelease(m_pInfraredFrameReader);

    // done with depth frame reader
    SafeRelease(m_pDepthFrameReader);

    // done with color frame reader
    SafeRelease(m_pColorFrameReader);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);

    m_bStopThread = true;
    if (m_tSaveThread.joinable()) m_tSaveThread.join();

	m_bStopThreadIMU = true;
	if (m_tSaveThreadIMU.joinable()) m_tSaveThreadIMU.join();
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CKinectV2Recorder::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = { 0 };
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra = DLGWINDOWEXTRA;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc = DefDlgProcW;
    wc.lpszClassName = L"KinectV2RecorderAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CKinectV2Recorder::MessageRouter,
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

	std::string strEnable = getIni("setting.ini", "IMU", "enable");
	if (strEnable == "1")
	{
		m_bIMUEnable = true;
	}
	else
	{
		m_bIMUEnable = false;
		WCHAR a[128];
		GetWindowText(hWndApp, a, sizeof(a));
		wchar_t b[] = L" (IMU disabled)";
		wchar_t* c;
		c = wcscat(a, b);
		SetWindowText(hWndApp, c);
	}

	if (m_bIMUEnable)
	{
		StartThreadIMU();
	}


    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

/// <summary>
/// Start multithreading
/// </summary>
void CKinectV2Recorder::StartMultithreading()
{
    m_tSaveThread = std::thread(&CKinectV2Recorder::SaveRecordImages, this);
}

void CKinectV2Recorder::StartThreadIMU()
{
	m_tSaveThreadIMU = std::thread(&CKinectV2Recorder::GetIMUData, this);
}

/// <summary>
/// Main processing function
/// </summary>
void CKinectV2Recorder::Update()
{
    if (!m_pInfraredFrameReader | !m_pDepthFrameReader | !m_pColorFrameReader)
    {
        return;
    }

    INT64 currentInfraredFrameTime = 0;
    INT64 currentDepthFrameTime = 0;
    INT64 currentColorFrameTime = 0;

    IInfraredFrame* pInfraredFrame = NULL;
    IDepthFrame* pDepthFrame = NULL;
    IColorFrame* pColorFrame = NULL;


    // Get an infrared frame from Kinect
    HRESULT hrInfrared = m_pInfraredFrameReader->AcquireLatestFrame(&pInfraredFrame);
    // Get a depth frame from Kinect
    HRESULT hrDepth = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
    // Get a color frame from Kinect
    HRESULT hrColor = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

    if (SUCCEEDED(hrInfrared))
    {
        INT64 currentInfraredFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pInfraredFrame->get_RelativeTime(&currentInfraredFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
        }

        if (SUCCEEDED(hr))
        {
            ProcessInfrared(currentInfraredFrameTime, pBuffer, nWidth, nHeight);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pInfraredFrame);

    if (SUCCEEDED(hrDepth))
    {
        INT64 currentDepthFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        USHORT nDepthMinReliableDistance = 0;
        USHORT nDepthMaxDistance = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pDepthFrame->get_RelativeTime(&currentDepthFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
        }

        if (SUCCEEDED(hr))
        {
            ProcessDepth(currentDepthFrameTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
        }

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pDepthFrame);

    if (SUCCEEDED(hrColor))
    {
        INT64 currentColorFrameTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        ColorImageFormat imageFormat = ColorImageFormat_None;
        UINT nBufferSize = 0;
        RGBQUAD *pBuffer = NULL;

        // Unit: 100 ns
        HRESULT hr = pColorFrame->get_RelativeTime(&currentColorFrameTime);

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
        }

        if (SUCCEEDED(hr))
        {
            if (imageFormat == ColorImageFormat_Bgra)
            {
                hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
            }
            else if (m_pColorRGBX)
            {
                pBuffer = m_pColorRGBX;
                nBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
                hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Bgra);
            }
            else
            {
                hr = E_FAIL;
            }
        }

        if (SUCCEEDED(hr))
        {
            ProcessColor(currentColorFrameTime, pBuffer, nWidth, nHeight);
        }
        SafeRelease(pFrameDescription);
    }

    SafeRelease(pColorFrame);
}

/// <summary>
/// Initialize the UI
/// </summary>
void CKinectV2Recorder::InitializeUIControls()
{
    const wchar_t *Models[] = { L"Wing", L"Duck", L"City", L"Beach", L"Firework", L"Maple" };
    const wchar_t *Types[] = { L"Translation", L"Zoom", L"In-plane Rotation", L"Out-of-plane Rotation",
        L"Flashing Light", L"Moving Light", L"Free Movement" };
    const wchar_t *Levels[] = { L"1", L"2", L"3", L"4", L"5" };
    const wchar_t *Sides[] = { L"Front", L"Left", L"Back", L"Right" };

    // Set the radio button for selection between 2D and 3D
    if (m_bSelect2D)
    {
        CheckDlgButton(m_hWnd, IDC_2D, BST_CHECKED);
    }
    else
    {
        CheckDlgButton(m_hWnd, IDC_3D, BST_CHECKED);
    }

    for (int i = 0; i < 6; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Models[i]);
    }

    for (int i = 0; i < 7; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_ADDSTRING, 0, (LPARAM)Types[i]);
    }

    for (int i = 0; i < 5; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_ADDSTRING, 0, (LPARAM)Levels[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_ADDSTRING, 0, (LPARAM)Sides[i]);
    }

    // Set combo box index
    SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel2DIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_SETCURSEL, m_nTypeIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_SETCURSEL, m_nLevelIndex, 0);
    SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_SETCURSEL, m_nSideIndex, 0);

    // Disable the side text & combo box
    EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), false);
    EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), false);

    // Set button icons
    m_hRecord = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_RECORD), IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
    m_hStop = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_STOP), IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
    m_hShot = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SHOT), IMAGE_ICON, 27, 18, LR_DEFAULTCOLOR);
    SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);
    SendDlgItemMessage(m_hWnd, IDC_BUTTON_SHOT, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hShot);

    // Set sfolder
	getTimeString(m_cModelFolder);
	wcscpy(m_cSaveFolder, m_cModelFolder);
    //StringCchPrintf(m_cModelFolder, _countof(m_cModelFolder), L"2D");

    //StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D//wi_tr_1");
}

/// <summary>
/// Process the UI inputs
/// </summary>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
void CKinectV2Recorder::ProcessUI(WPARAM wParam, LPARAM)
{
    const wchar_t *Model2D[] = { L"Wing", L"Duck", L"City", L"Beach", L"Firework", L"Maple" };
    const wchar_t *Model3D[] = { L"Soda", L"Chest", L"Ironman", L"House", L"Bike", L"Jet" };

    // Select 2D Model
    if (IDC_2D == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        m_bSelect2D = true;
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 6; i++)
        {
            SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Model2D[i]);
        }

        // Setup combo boxes
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel2DIndex, 0);

        // Disable the side text & combo box
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), false);
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), false);
    }
    // Select 3D Model
    if (IDC_3D == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        m_bSelect2D = false;
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 6; i++)
        {
            SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_ADDSTRING, 0, (LPARAM)Model3D[i]);
        }

        // Setup combo boxes
        SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_SETCURSEL, m_nModel3DIndex, 0);

        // Enable the side text & combo box
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_TEXT), true);
        EnableWindow(GetDlgItem(m_hWnd, IDC_SIDE_CBO), true);
    }
    // Model selection
    if (IDC_MODEL_CBO == LOWORD(wParam))
    {
        UINT index = (UINT)SendDlgItemMessage(m_hWnd, IDC_MODEL_CBO, CB_GETCURSEL, 0, 0);
        if (m_bSelect2D) m_nModel2DIndex = index;
        else m_nModel3DIndex = index;
    }
    // Motion type selection
    if (IDC_TYPE_CBO == LOWORD(wParam))
    {
        m_nTypeIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_TYPE_CBO, CB_GETCURSEL, 0, 0);
        if (m_nTypeIndex > 3)
        {
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_TEXT), false);
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_CBO), false);
        }
        else
        {
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_TEXT), true);
            EnableWindow(GetDlgItem(m_hWnd, IDC_LEVEL_CBO), true);
        }
    }
    // Motion digradation level selection
    if (IDC_LEVEL_CBO == LOWORD(wParam))
    {
        m_nLevelIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_LEVEL_CBO, CB_GETCURSEL, 0, 0);
    }
    // Model side selection
    if (IDC_SIDE_CBO == LOWORD(wParam))
    {
        m_nSideIndex = (UINT)SendDlgItemMessage(m_hWnd, IDC_SIDE_CBO, CB_GETCURSEL, 0, 0);
    }
    // Set save folder

 /*   if (m_bSelect2D)
    {
        StringCchPrintf(m_cModelFolder, _countof(m_cModelFolder), L"2D");
        switch (m_nModel2DIndex)
        {
        case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\wi"); break;
        case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\du"); break;
        case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\ci"); break;
        case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\be"); break;
        case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\fi"); break;
        case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"2D\\ma"); break;
        }
    }
    else{
        StringCchPrintf(m_cModelFolder, _countof(m_cModelFolder), L"3D");
        switch (m_nModel3DIndex)
        {
        case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\so"); break;
        case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ch"); break;
        case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ir"); break;
        case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\ho"); break;
        case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\bi"); break;
        case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"3D\\je"); break;
        }
    }
    switch (m_nTypeIndex)
    {
    case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_tr", m_cSaveFolder); break;
    case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_zo", m_cSaveFolder); break;
    case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_ir", m_cSaveFolder); break;
    case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_or", m_cSaveFolder); break;
    case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_fl", m_cSaveFolder); break;
    case 5: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_ml", m_cSaveFolder); break;
    case 6: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_fm", m_cSaveFolder); break;
    }
    if (m_nTypeIndex < 4)
    {
        switch (m_nLevelIndex)
        {
        case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_1", m_cSaveFolder); break;
        case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_2", m_cSaveFolder); break;
        case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_3", m_cSaveFolder); break;
        case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_4", m_cSaveFolder); break;
        case 4: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_5", m_cSaveFolder); break;
        }
    }
    if (!m_bSelect2D)
    {
        switch (m_nSideIndex)
        {
        case 0: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_f", m_cSaveFolder); break;
        case 1: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_l", m_cSaveFolder); break;
        case 2: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_b", m_cSaveFolder); break;
        case 3: StringCchPrintf(m_cSaveFolder, _countof(m_cSaveFolder), L"%s_r", m_cSaveFolder); break;
        }
    }*/
    WCHAR szStatusMessage[128];
    StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS(Infrared, Depth, Color) = (%0.2f,  %0.2f,  %0.2f)", m_cSaveFolder, m_fInfraredFPS, m_fDepthFPS, m_fColorFPS);
    SetStatusMessage(szStatusMessage, 500, true);

    // If it is a record control and a button clicked event, save the video sequences
    if (IDC_BUTTON_RECORD == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        if (m_bRecord)
        {
#ifdef VERBOSE
            CheckImages();
#endif
            ResetRecordParameters();

			if (m_bIMUEnable == true)
			{
				fclose(m_myfile);
				m_myfile = NULL;
			}
        }
        else
        {
            m_bRecord = true;
            SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hStop);
        }
    }

    // If it is a shot control and a button clicked event, save the camera images
    if (IDC_BUTTON_SHOT == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
    {
        m_bShot = true;
    }
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CKinectV2Recorder::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CKinectV2Recorder* pThis = NULL;

    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CKinectV2Recorder*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CKinectV2Recorder*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CKinectV2Recorder::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Bind application window handle
        m_hWnd = hWnd;

        InitializeUIControls();

        // Init Direct2D
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

        // Create and initialize a new Direct2D image renderer (take a look at ImageRenderer.h)
        // We'll use this to draw the data we receive from the Kinect to the screen 
        m_pDrawInfrared = new ImageRenderer();
        HRESULT hr = m_pDrawInfrared->Initialize(GetDlgItem(m_hWnd, IDC_INFRAREDVIEW), m_pD2DFactory, cInfraredWidth, cInfraredHeight, cInfraredWidth * sizeof(RGBQUAD));
        if (FAILED(hr))
        {
            SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
        }

        m_pDrawDepth = new ImageRenderer();
        hr = m_pDrawDepth->Initialize(GetDlgItem(m_hWnd, IDC_DEPTHVIEW), m_pD2DFactory, cDepthWidth, cDepthHeight, cDepthWidth * sizeof(RGBQUAD));
        if (FAILED(hr))
        {
            SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
        }

        m_pDrawColor = new ImageRenderer();
        hr = m_pDrawColor->Initialize(GetDlgItem(m_hWnd, IDC_COLORVIEW), m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD));
        if (FAILED(hr))
        {
            SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
        }

        // Get and initialize the default Kinect sensor
        InitializeDefaultSensor();

        StartMultithreading();
    }
    break;

    // If the titlebar X is clicked, destroy app
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        // Quit the main message pump
        PostQuitMessage(0);
        break;

    // Handle button press
    case WM_COMMAND:
        ProcessUI(wParam, lParam);
        break;
    }

    return FALSE;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize the Kinect and get the readers
        IInfraredFrameSource* pInfraredFrameSource = NULL;
        IDepthFrameSource* pDepthFrameSource = NULL;
        IColorFrameSource* pColorFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_InfraredFrameSource(&pInfraredFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pInfraredFrameSource->OpenReader(&m_pInfraredFrameReader);
        }
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
        }
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
        }

        SafeRelease(pInfraredFrameSource);
        SafeRelease(pDepthFrameSource);
        SafeRelease(pColorFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new infrared data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// </summary>
void CKinectV2Recorder::ProcessInfrared(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight)
{
    if (m_hWnd)
    {
        double fps = 0.0;

        LARGE_INTEGER qpcNow = { 0 };
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nInfraredLastCounter)
                {
                    m_nInfraredFramesSinceUpdate++;
                    fps = m_fFreq * m_nInfraredFramesSinceUpdate / double(qpcNow.QuadPart - m_nInfraredLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[128];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS(Infrared, Depth, Color) = (%0.2f,  %0.2f,  %0.2f)", m_cSaveFolder, m_fInfraredFPS, m_fDepthFPS, m_fColorFPS);

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nInfraredLastCounter = qpcNow.QuadPart;
            m_nInfraredFramesSinceUpdate = 0;
            m_fInfraredFPS = fps;
#ifdef VERBOSE
            if (m_bRecord && m_fInfraredFPS < 29.5)
            {
                ResetRecordParameters();
                MessageBox(NULL,
                    L"Infrared frame dropping occured...\n",
                    L"No Good",
                    MB_OK | MB_ICONERROR
                    );
                return;
            }
#endif
        }
    }

    if (m_pInfraredRGBX && pBuffer && (nWidth == cInfraredWidth) && (nHeight == cInfraredHeight))
    {
        INT64 index = m_nInfraredIndex % BufferSize;
        RGBQUAD* pRGBX = m_pInfraredRGBX;
        UINT16* pUINT16 = m_pInfraredUINT16[index];
        pBuffer += cInfraredWidth - 1;

        for (int i = 0; i < cInfraredHeight; ++i)
        {
            for (int j = 0; j < cInfraredWidth; ++j)
            {
                // normalize the incoming infrared data (ushort) to a float ranging from 
                // [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
                // 1. dividing the incoming value by the source maximum value
                float intensityRatio = static_cast<float>(*pBuffer) / InfraredSourceValueMaximum;

                // 2. dividing by the (average scene value * standard deviations)
                intensityRatio /= InfraredSceneValueAverage * InfraredSceneStandardDeviations;

                // 3. limiting the value to InfraredOutputValueMaximum
                intensityRatio = cv::min(InfraredOutputValueMaximum, intensityRatio);

                // 4. limiting the lower value InfraredOutputValueMinimym
                intensityRatio = cv::max(InfraredOutputValueMinimum, intensityRatio);

                // 5. converting the normalized value to a byte and using the result
                // as the RGB components required by the image
                byte intensity = static_cast<byte>(intensityRatio * 255.0f);
                pRGBX->rgbRed = intensity;
                pRGBX->rgbGreen = intensity;
                pRGBX->rgbBlue = intensity;

                // convert UINT16 to Big-Endian format
                (*pUINT16) = ((*pBuffer) >> 8) | ((*pBuffer) << 8);

                --pBuffer;
                ++pRGBX;
                ++pUINT16;
            }
            pBuffer += (cInfraredWidth << 1);
        }

        // Draw the data with Direct2D
        m_pDrawInfrared->Draw(reinterpret_cast<BYTE*>(m_pInfraredRGBX), cInfraredWidth * cInfraredHeight * sizeof(RGBQUAD));

        if (m_bRecord)
        {
            if (!m_nStartTime)
            {
                if (IsDirectoryExists(m_cSaveFolder))
                {
                    MessageBox(NULL,
                        L"The related folder is not emtpy!\n",
                        L"Frames already existed",
                        MB_OK | MB_ICONERROR
                        );

					MessageBox(NULL,
						m_cSaveFolder,
						L"Frames already existed",
						MB_OK | MB_ICONERROR
					);
					
                    m_bRecord = false;
                    SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);
                    return;
                }
                m_nStartTime = nTime;
            }

            // Write out the bitmap to disk (enqeue)
            m_qInfraredTimeQueue.push(nTime - m_nStartTime);
            m_qInfraredFrameQueue.push(m_pInfraredUINT16[index]);

            ++m_nInfraredIndex;
        }

        if (m_bShot)
        {
            m_nInfraredShotTime = nTime;
            m_bShotReady = true;
        }
    }
}

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>
void CKinectV2Recorder::ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth)
{
    if (m_hWnd)
    {
        double fps = 0.0;

        LARGE_INTEGER qpcNow = { 0 };
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nDepthLastCounter)
                {
                    m_nDepthFramesSinceUpdate++;
                    fps = m_fFreq * m_nDepthFramesSinceUpdate / double(qpcNow.QuadPart - m_nDepthLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[128];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS(Infrared, Depth, Color) = (%0.2f,  %0.2f,  %0.2f)", m_cSaveFolder, m_fInfraredFPS, m_fDepthFPS, m_fColorFPS);

        if (m_nDepthFramesSinceUpdate % 30 == 0)
        {
            m_nDepthLastCounter = qpcNow.QuadPart;
            m_nDepthFramesSinceUpdate = 0;
            m_fDepthFPS = fps;
#ifdef VERBOSE
            if (m_bRecord && m_fDepthFPS < 29.5)
            {
                ResetRecordParameters();
                MessageBox(NULL,
                    L"Depth frame dropping occured...\n",
                    L"No Good",
                    MB_OK | MB_ICONERROR
                    );
                return;
            }
#endif
        }
    }

    // Make sure we've received valid data
    if (m_pDepthRGBX && pBuffer && (nWidth == cDepthWidth) && (nHeight == cDepthHeight))
    {
        INT64 index = m_nDepthIndex % BufferSize;
        RGBQUAD* pRGBX = m_pDepthRGBX;
        UINT16* pUINT16 = m_pDepthUINT16[index];
        pBuffer += cDepthWidth - 1;

        for (int i = 0; i < cDepthHeight; ++i)
        {
            for (int j = 0; j < cDepthWidth; ++j)
            {
                USHORT depth = *pBuffer;

                // To convert to a byte, we're discarding the most-significant
                // rather than least-significant bits.
                // We're preserving detail, although the intensity will "wrap."
                // Values outside the reliable depth range are mapped to 0 (black).

                // Note: Using conditionals in this loop could degrade performance.
                // Consider using a lookup table instead when writing production code.
                if ((depth < nMinDepth) || (depth > nMaxDepth))
                {
                    depth = 0;
                    pRGBX->rgbRed = 34;
                    pRGBX->rgbGreen = 132;
                    pRGBX->rgbBlue = 212;
                }
                else
                {
                    BYTE intensity = static_cast<BYTE>(depth % 256);
                    pRGBX->rgbRed = intensity;
                    pRGBX->rgbGreen = intensity;
                    pRGBX->rgbBlue = intensity;
                }

                // convert UINT16 to Big-Endian format
                (*pUINT16) = ((depth) >> 8) | ((depth) << 8);

                --pBuffer;
                ++pRGBX;
                ++pUINT16;
            }
            pBuffer += (cDepthWidth << 1);
        }

        // Draw the data with Direct2D
        m_pDrawDepth->Draw(reinterpret_cast<BYTE*>(m_pDepthRGBX), cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

        if (m_bRecord && m_nStartTime)
        {
            // Write out the bitmap to disk (enqeue)
            m_qDepthTimeQueue.push(nTime - m_nStartTime);
            m_qDepthFrameQueue.push(m_pDepthUINT16[index]);

            ++m_nDepthIndex;
        }

        if (m_bShotReady)
        {
            m_nDepthShotTime = nTime;
        }
    }
}

/// <summary>
/// Handle new color data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// </summary>
void CKinectV2Recorder::ProcessColor(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight)
{
    if (m_hWnd)
    {
        double fps = 0.0;

        LARGE_INTEGER qpcNow = { 0 };
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nColorLastCounter)
                {
                    m_nColorFramesSinceUpdate++;
                    fps = m_fFreq * m_nColorFramesSinceUpdate / double(qpcNow.QuadPart - m_nColorLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[128];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" Save Folder: %s    FPS(Infrared, Depth, Color) = (%0.2f,  %0.2f,  %0.2f)", m_cSaveFolder, m_fInfraredFPS, m_fDepthFPS, m_fColorFPS);

        if (m_nColorFramesSinceUpdate % 30 == 0)
        {
            m_nColorLastCounter = qpcNow.QuadPart;
            m_nColorFramesSinceUpdate = 0;
            m_fColorFPS = fps;
#ifdef VERBOSE
            if (m_bRecord && m_fColorFPS < 29.5)
            {
                ResetRecordParameters();
                MessageBox(NULL,
                    L"Color frame dropping occured...\n",
                    L"No Good",
                    MB_OK | MB_ICONERROR
                    );
                return;
            }
#endif
        }
    }

    // Make sure we've received valid data
    if (pBuffer && (nWidth == cColorWidth) && (nHeight == cColorHeight))
    {
        INT64 index = m_nColorIndex % BufferSize;
        RGBQUAD* pRGBX = pBuffer;
        RGBTRIPLE* pRGB = m_pColorRGB[index];

#ifdef USE_IPP
        const IppiSize roiSize = { cColorWidth, cColorHeight };
        ippiMirror_8u_C4IR((Ipp8u*)pRGBX, cColorWidth * 4, roiSize, ippAxsVertical);
#ifdef COLOR_BMP
        ippiCopy_8u_AC4C3R((Ipp8u*)pBuffer, cColorWidth * 4, (Ipp8u*)pRGB, cColorWidth * 3, roiSize);  // BGRA to BGR
#else // COLOR_BMP
        const int dstOrder[3] = {2, 1, 0};
        ippiSwapChannels_8u_C4C3R((Ipp8u*)pRGBX, cColorWidth * 4, (Ipp8u*)pRGB, cColorWidth * 3, roiSize, dstOrder); // BGRA to RGB
#endif // COLOR_BMP
#else // USE_IPP
        RGBQUAD* pBegin = pBuffer;
        RGBQUAD* pEnd = pBuffer + cColorWidth;
        for (int i = 0; i < cColorHeight; ++i)
        {
            std::reverse(pBegin, pEnd);
            pBegin += cColorWidth;
            pEnd += cColorWidth;
        }

        // end pixel is start + width*height - 1
        const RGBQUAD* pBufferEnd = pBuffer + (nWidth * nHeight);

        while (pRGBX < pBufferEnd)
        {
#ifdef COLOR_BMP
            pRGB->rgbtRed = pRGBX->rgbRed;
            pRGB->rgbtGreen = pRGBX->rgbGreen;
            pRGB->rgbtBlue = pRGBX->rgbBlue;
#else // COLOR_BMP
            pRGB->rgbtRed = pRGBX->rgbBlue;
            pRGB->rgbtGreen = pRGBX->rgbGreen;
            pRGB->rgbtBlue = pRGBX->rgbRed;
#endif // COLOR_BMP
            ++pRGBX;
            ++pRGB;
        }
#endif // USE_IPP

        // Draw the data with Direct2D
        m_pDrawColor->Draw(reinterpret_cast<BYTE*>(pBuffer), cColorWidth * cColorHeight * sizeof(RGBQUAD));

        if (m_bRecord && m_nStartTime)
        {
            // Write out the bitmap to disk (enqeue)
            m_qColorTimeQueue.push(nTime - m_nStartTime);
            m_qColorFrameQueue.push(m_pColorRGB[index]);

            ++m_nColorIndex;
        }

        if (m_bShotReady)
        {
            m_nColorShotTime = nTime;
            if (m_nInfraredShotTime == m_nDepthShotTime || abs(m_nColorShotTime - m_nDepthShotTime) < 100000)
            {
                SaveShotImages();
                m_bShot = false;
                m_bShotReady = false;
            }
        }
    }
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
bool CKinectV2Recorder::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    // Unit: 1 ms
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

/// <summary>
/// Save passed in image data to disk as a bitmap
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::SaveToBMP(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    BITMAPINFOHEADER bmpInfoHeader = { 0 };

    bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);  // Size of the header
    bmpInfoHeader.biBitCount = wBitsPerPixel;             // Bit count
    bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
    bmpInfoHeader.biWidth = lWidth;                    // Width in pixels
    bmpInfoHeader.biHeight = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
    bmpInfoHeader.biPlanes = 1;                         // Default
    bmpInfoHeader.biSizeImage = dwByteCount;               // Image size in bytes

    BITMAPFILEHEADER bfh = { 0 };

    bfh.bfType = 0x4D42;                                           // 'M''B', indicates bitmap
    bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
    bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile)
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;

    // Write the bitmap file header
    if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Write the bitmap info header
    if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Write the RGB Data
    if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}

HRESULT CKinectV2Recorder::SaveToPNG(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
	// Convert LPCWSTR lpszFilePath to char
	size_t origsize = wcslen(lpszFilePath) + 1;
	size_t convertedChars = 0;
	char gszFile[100] = { 0 };
	wcstombs_s(&convertedChars, gszFile, origsize, lpszFilePath, _TRUNCATE); //from wchar_t to char*
	// Convert char to std::string
	std::string strFilePath(gszFile);

	cv::Mat image_ori = cv::Mat(lHeight, lWidth, CV_8UC3, pBitmapBits);
	//Mat image;
	//cv::resize(image_ori, image, cv::Size(640, 480));
	
	//cv::Rect myROI((cColorWidth / 2) - (640/2), (cColorHeight/2)-(480/2), 640, 480);
	cv::Rect myROI((cColorWidth / 2) - (m_iRGBwidth / 2), (cColorHeight / 2) - (m_iRGBheight / 2), m_iRGBwidth, m_iRGBheight);

	cv::Mat croppedImage = image_ori(myROI);

	cv::Mat image;
	cv::cvtColor(croppedImage, image, CV_BGR2RGB);

	imwrite(strFilePath, image);

	/*
	DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

	BITMAPINFOHEADER bmpInfoHeader = { 0 };

	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);  // Size of the header
	bmpInfoHeader.biBitCount = wBitsPerPixel;             // Bit count
	bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
	bmpInfoHeader.biWidth = lWidth;                    // Width in pixels
	bmpInfoHeader.biHeight = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
	bmpInfoHeader.biPlanes = 1;                         // Default
	bmpInfoHeader.biSizeImage = dwByteCount;               // Image size in bytes

	BITMAPFILEHEADER bfh = { 0 };

	bfh.bfType = 0x4D42;                                           // 'M''B', indicates bitmap
	bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
	bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

																   // Create the file on disk to write to
	HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// Return if error opening file
	if (NULL == hFile)
	{
		return E_ACCESSDENIED;
	}

	DWORD dwBytesWritten = 0;

	// Write the bitmap file header
	if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the bitmap info header
	if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the RGB Data
	if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Close the file
	CloseHandle(hFile);*/
	return S_OK;
}


/// <summary>
/// Save passed in image data to disk as a pgm file
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lMaxPixel">max value of a pixel</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::SaveToPGM(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LONG lMaxPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    // Set save folder
    CHAR szHeader[256];
    sprintf_s(szHeader, _countof(szHeader), "P5\n%d %d\n%d\n", lWidth, lHeight, lMaxPixel);

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile)
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;

    // Write the pgm file header
    if (!WriteFile(hFile, szHeader, strlen(szHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Write the grayscale data
    if (!WriteFile(hFile, pBitmapBits, dwByteCount, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}

HRESULT CKinectV2Recorder::SaveToPNG_depth(
	BYTE* pBitmapBits, 
	LONG lWidth, 
	LONG lHeight, 
	WORD wBitsPerPixel, 
	LONG lMaxPixel, 
	LPCWSTR lpszFilePath)
{
	// Convert LPCWSTR lpszFilePath to char
	size_t origsize = wcslen(lpszFilePath) + 1;
	size_t convertedChars = 0;
	char gszFile[100] = { 0 };
	wcstombs_s(&convertedChars, gszFile, origsize, lpszFilePath, _TRUNCATE); //from wchar_t to char*
																			 // Convert char to std::string
	std::string strFilePath(gszFile);

	cv::Mat image_ori = cv::Mat(lHeight, lWidth, CV_16UC1, pBitmapBits);
	//Mat image;
	//cv::resize(image_ori, image, cv::Size(640, 480));

	cv::imwrite(strFilePath, image_ori);
	return S_OK;
}

/// <summary>
/// Save passed in image data to disk as a PPM file
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lMaxPixel">max value of a pixel</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CKinectV2Recorder::SaveToPPM(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LONG lMaxPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    // Set save folder
    CHAR szHeader[256];
    sprintf_s(szHeader, _countof(szHeader), "P6\n%d %d\n%d\n", lWidth, lHeight, lMaxPixel);

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile)
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;

    // Write the pgm file header
    if (!WriteFile(hFile, szHeader, strlen(szHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Write the grayscale data
    if (!WriteFile(hFile, pBitmapBits, dwByteCount, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}

/// <summary>
/// Check if the directory exists
/// </summary>
/// <param name="szDirName">directory</param>
/// <returns>indicates exists or not</returns>
bool CKinectV2Recorder::IsDirectoryExists(WCHAR* szDirName) {
    DWORD attribs = ::GetFileAttributes(szDirName);
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attribs & FILE_ATTRIBUTE_DIRECTORY);
}

/// <summary>
/// Save record images
/// </summary>
void CKinectV2Recorder::SaveRecordImages()
{
    while (!m_bStopThread)
    {
		bool bInfraredWrite = !m_qInfraredFrameQueue.empty();
		bool bDepthWrite = !m_qDepthFrameQueue.empty();
		bool bColorWrite = !m_qColorFrameQueue.empty();

		

        // Check if the necessary directories exist
        if ((bInfraredWrite || bDepthWrite || bColorWrite))
        {
            if (!IsDirectoryExists(m_cModelFolder))
            {
                CreateDirectory(m_cModelFolder, NULL);
            }
            if (!IsDirectoryExists(m_cSaveFolder))
            {
                CreateDirectory(m_cSaveFolder, NULL);
            }
        }

        if (bInfraredWrite)
        {
            WCHAR szSavePath[MAX_PATH];
            StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\ir", m_cSaveFolder);

            if (!IsDirectoryExists(szSavePath))
            {
                CreateDirectory(szSavePath, NULL);
            }

            INT64 nTime = m_qInfraredTimeQueue.front();
            //StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.pgm", szSavePath, nTime / 10000000.);
			StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.png", szSavePath, nTime / 10000000.);
            //SaveToPGM(reinterpret_cast<BYTE*>(m_qInfraredFrameQueue.front()), cInfraredWidth, cInfraredHeight, sizeof(UINT16)* 8, 65535, szSavePath);
			SaveToPNG_depth(reinterpret_cast<BYTE*>(m_qInfraredFrameQueue.front()), cInfraredWidth, cInfraredHeight, sizeof(UINT16) * 8, 65535, szSavePath);
            m_vInfraredList.push_back(nTime);

            m_qInfraredTimeQueue.pop();
            m_qInfraredFrameQueue.pop();
        }

        if (bDepthWrite)
        {
            WCHAR szSavePath[MAX_PATH];
            StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\depth", m_cSaveFolder);

            if (!IsDirectoryExists(szSavePath))
            {
                CreateDirectory(szSavePath, NULL);
            }

            INT64 nTime = m_qDepthTimeQueue.front();
            //StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.pgm", szSavePath, nTime / 10000000.);
			StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.png", szSavePath, nTime / 10000000.);
            //SaveToPGM(reinterpret_cast<BYTE*>(m_qDepthFrameQueue.front()), cDepthWidth, cDepthHeight, sizeof(UINT16)* 8, 65535, szSavePath);
			SaveToPNG_depth(reinterpret_cast<BYTE*>(m_qDepthFrameQueue.front()), cDepthWidth, cDepthHeight, sizeof(UINT16) * 8, 65535, szSavePath);

            m_vDepthList.push_back(nTime);

            m_qDepthTimeQueue.pop();
            m_qDepthFrameQueue.pop();
        }


		

        if (bColorWrite)
        {
			if (m_bIMUEnable == true)
			{
				// Write IMU data
				WCHAR szSavePathIMU[MAX_PATH];
				StringCchPrintfW(szSavePathIMU, _countof(szSavePathIMU), L"%s\\imu", m_cSaveFolder);
				if (!IsDirectoryExists(szSavePathIMU))
				{
					CreateDirectory(szSavePathIMU, NULL);
				}
				std::wstring ws(szSavePathIMU);
				std::string strPath(ws.begin(), ws.end());
				if (m_myfile == NULL)
				{
					std::string imuFilePath = strPath + "\\imu.txt";
					m_myfile = fopen(imuFilePath.c_str(), "w+");
				}
			}

            WCHAR szSavePath[MAX_PATH];
            StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\color", m_cSaveFolder);

            if (!IsDirectoryExists(szSavePath))
            {
                CreateDirectory(szSavePath, NULL);
            }

            INT64 nTime = m_qColorTimeQueue.front();
			char buffer[50];
			sprintf(buffer, "%011.6f", nTime / 10000000.);
			std::string strTime(buffer);
			
			if (m_bIMUEnable == true)
			{
				std::vector<double> IMUdata;
				mtx.lock();
				IMUdata.assign(m_IMUdata.begin(), m_IMUdata.end());
				mtx.unlock();
			
			
				assert(IMUdata.size() == 6);
				if (IMUdata.size() != 6)
				{
					MessageBox(NULL,
						L"IMU incoming data incorrect.  Try to setup MIP monitor and press the blue 'play' button.",
						L"Error",
						MB_OK | MB_ICONERROR
					);
					exit(1);
				}
				writeCSV(m_myfile, strTime, IMUdata);
			}
#ifdef COLOR_BMP
            StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.bmp", szSavePath, nTime / 10000000.);
            SaveToBMP(reinterpret_cast<BYTE*>(m_qColorFrameQueue.front()), cColorWidth, cColorHeight, sizeof(RGBTRIPLE)* 8, szSavePath);
#else
            //StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.ppm", szSavePath, nTime / 10000000.);
            //SaveToPPM(reinterpret_cast<BYTE*>(m_qColorFrameQueue.front()), cColorWidth, cColorHeight, sizeof(RGBTRIPLE)* 8, 255, szSavePath);
			StringCchPrintfW(szSavePath, _countof(szSavePath), L"%s\\%011.6f.png", szSavePath, nTime / 10000000.);
			SaveToPNG(reinterpret_cast<BYTE*>(m_qColorFrameQueue.front()), cColorWidth, cColorHeight, sizeof(RGBTRIPLE) * 8, szSavePath);
#endif
            m_vColorList.push_back(nTime);

            m_qColorTimeQueue.pop();
            m_qColorFrameQueue.pop();
        }

		

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

/// <summary>
/// Get IMU Data
/// </summary>
void CKinectV2Recorder::GetIMUData()
{
	std::string strEnable = getIni("setting.ini", "IMU", "enable");
	if (strEnable == "1")
	{
		m_bIMUEnable = true;
	}
	else
	{
		m_bIMUEnable = false;
		return;
	}
	//TODO: change these constants to match your setup
	//std::string COM_PORT = "COM3";
	std::string strCOM_Port = getIni("setting.ini", "IMU", "comport");

	// Print out message
	std::wstring stemp = s2ws(strCOM_Port);
	LPCWSTR iniContent = stemp.c_str();
	MessageBox(NULL,
		iniContent,//L"Frame dropping occured...\n",
		L"IMU info load from setting.ini",
		MB_OK
	);

	if (strCOM_Port == "")
	{
		m_bIMUEnable = false;
		return;
	}

	try
	{
		//create a SerialConnection with the COM port
		mscl::Connection connection = mscl::Connection::Serial(strCOM_Port);

		//create an InertialNode with the connection
		mscl::InertialNode node(connection);

		//Put the Inertial Node into its idle state 
		//  (This is not required but reduces the parsing 
		//  burden during initialization and makes visual 
		//  confirmation of the commands easier.)
		node.setToIdle();

		//build up the channels to set
		mscl::InertialChannels sensorChs;

		sensorChs.push_back(mscl::InertialChannel(mscl::InertialTypes::CH_FIELD_SENSOR_SCALED_ACCEL_VEC, mscl::SampleRate::Hertz(100)));

		sensorChs.push_back(mscl::InertialChannel(mscl::InertialTypes::CH_FIELD_SENSOR_SCALED_GYRO_VEC, mscl::SampleRate::Hertz(100)));

		//set the active channels for the Sensor category on the Node
		node.setActiveChannelFields(mscl::InertialTypes::CATEGORY_SENSOR, sensorChs);

		//start sampling on the Sensor category of the Node
		node.enableDataStream(mscl::InertialTypes::CATEGORY_SENSOR);

		while (!m_bStopThreadIMU)
		{
			mscl::InertialDataPoints data; //typedef std::vector<InertialDataPoint> InertialDataPoints;
			//get all the data packets from the node, with a timeout of 500 milliseconds
			mscl::InertialDataPackets packets = node.getDataPackets(500);
			
			for (mscl::InertialDataPacket packet : packets)
			{
				data = packet.data();
				

				assert(data.size() == 6);
				if (data.size() != 6)
				{
					MessageBox(NULL,
						L"IMU incoming data incorrect.  Try to setup MIP monitor and press the blue 'play' button.",
						L"Error",
						MB_OK | MB_ICONERROR
					);
					exit(1);
				}


				mtx.lock();// #########################lock
				m_IMUdata.clear();
				for(int i = 0; i < 6; i++)
				{
					m_IMUdata.push_back(data[i].as_double());
				}
				mtx.unlock();// #########################unlock
			}
		}
	}
	catch (mscl::Error& e)
	{
		std::string tmpstr(e.what());
		std::string errmsg = "Error:" + tmpstr;

		std::wstring stemp = s2ws(errmsg);
		LPCWSTR result = stemp.c_str();

		MessageBox(NULL,
			result,//L"Frame dropping occured...\n",
			L"Error",
			MB_OK | MB_ICONERROR
		);
	}
}

/// <summary>
/// Save shot images
/// </summary>
void CKinectV2Recorder::SaveShotImages()
{
    WCHAR* szPicturesFolder = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &szPicturesFolder);

    if (SUCCEEDED(hr))
    {
        WCHAR szCalibrationFolder[MAX_PATH];
        WCHAR szInfraredFolder[MAX_PATH];
        WCHAR szDepthFolder[MAX_PATH];
        WCHAR szColorFolder[MAX_PATH];
        WCHAR FileName[MAX_PATH];
        StringCchPrintfW(szCalibrationFolder, _countof(szCalibrationFolder), L"%s\\calibration", szPicturesFolder);
        if (!IsDirectoryExists(szCalibrationFolder))
        {
            CreateDirectory(szCalibrationFolder, NULL);
        }
        GetTimeFormatEx(NULL, 0, NULL, L"HH'-'mm'-'ss", FileName, _countof(FileName));

        // Save infrared image
        StringCchPrintfW(szInfraredFolder, _countof(szInfraredFolder), L"%s\\ir", szCalibrationFolder);
        if (!IsDirectoryExists(szInfraredFolder))
        {
            CreateDirectory(szInfraredFolder, NULL);
        }
        WCHAR szInfraredPath[MAX_PATH];
        //StringCchPrintfW(szInfraredPath, _countof(szInfraredPath), L"%s\\%s.pgm", szInfraredFolder, FileName);
		StringCchPrintfW(szInfraredPath, _countof(szInfraredPath), L"%s\\%s.png", szInfraredFolder, FileName);
        //SaveToPGM(reinterpret_cast<BYTE*>(m_pInfraredUINT16[0]), cInfraredWidth, cInfraredHeight, sizeof(UINT16)* 8, 65535, szInfraredPath);
		SaveToPNG_depth(reinterpret_cast<BYTE*>(m_pInfraredUINT16[0]), cInfraredWidth, cInfraredHeight, sizeof(UINT16) * 8, 65535, szInfraredPath);

        // Save depth image
        StringCchPrintfW(szDepthFolder, _countof(szDepthFolder), L"%s\\depth", szCalibrationFolder);
        if (!IsDirectoryExists(szDepthFolder))
        {
            CreateDirectory(szDepthFolder, NULL);
        }
        WCHAR szDepthPath[MAX_PATH];
        //StringCchPrintfW(szDepthPath, _countof(szDepthPath), L"%s\\%s.pgm", szDepthFolder, FileName);
		StringCchPrintfW(szDepthPath, _countof(szDepthPath), L"%s\\%s.png", szDepthFolder, FileName);
        //SaveToPGM(reinterpret_cast<BYTE*>(m_pDepthUINT16[0]), cDepthWidth, cDepthHeight, sizeof(UINT16)* 8, 65535, szDepthPath);
		SaveToPNG_depth(reinterpret_cast<BYTE*>(m_pDepthUINT16[0]), cDepthWidth, cDepthHeight, sizeof(UINT16) * 8, 65535, szDepthPath);
    
        // Save Color image
        StringCchPrintfW(szColorFolder, _countof(szColorFolder), L"%s\\color", szCalibrationFolder);
        if (!IsDirectoryExists(szColorFolder))
        {
            CreateDirectory(szColorFolder, NULL);
        }
        WCHAR szColorPath[MAX_PATH];
        StringCchPrintfW(szColorPath, _countof(szColorPath), L"%s\\%s.bmp", szColorFolder, FileName);
#ifndef COLOR_BMP
        RGBTRIPLE* pBuffer = m_pColorRGB[0];
        // end pixel is start + width*height - 1
        const RGBTRIPLE* pBufferEnd = pBuffer + (cColorWidth * cColorHeight);

        while (pBuffer < pBufferEnd)
        {
            std::swap(pBuffer->rgbtRed, pBuffer->rgbtBlue);
            ++pBuffer;
        }
#endif
        SaveToBMP(reinterpret_cast<BYTE*>(m_pColorRGB[0]), cColorWidth, cColorHeight, sizeof(RGBTRIPLE)* 8, szColorPath);

        WCHAR szStatusMessage[128];
        StringCchPrintfW(szStatusMessage, _countof(szStatusMessage), L"Take a shot   [%s\\xxx\\%s.xxx]", szCalibrationFolder, FileName);
        SetStatusMessage(szStatusMessage, 3000, true);
    }
}

/// <summary>
/// Check if we have stored all the necessary images (no frame dropping)
/// </summary>
void CKinectV2Recorder::CheckImages()
{
    m_bRecord = false;
    while (!m_qInfraredFrameQueue.empty() || !m_qDepthFrameQueue.empty() || !m_qColorFrameQueue.empty())
    {
        std::this_thread::sleep_for(std::chrono::microseconds(33));
    }
    int nInfraredFrameNumber = m_vInfraredList.size();
    int nDepthFrameNumber = m_vDepthList.size();
    int nColorFrameNumber = m_vColorList.size();

    if (!(nInfraredFrameNumber == nDepthFrameNumber && nDepthFrameNumber == nColorFrameNumber))
    {
        MessageBox(NULL,
            L"Frame dropping occured...\n",
            L"No Good",
            MB_OK | MB_ICONERROR
            );
        return;
    }

    for (int i = 0; i < nInfraredFrameNumber; ++i)
    {
        if (m_vInfraredList[i] != m_vDepthList[i] || abs(m_vColorList[i] - m_vInfraredList[i]) > 100000)
        {
            MessageBox(NULL,
                L"Frame dropping occured...\n",
                L"No Good",
                MB_OK | MB_ICONERROR
                );
            return;
        }
    }
}

/// <summary>
/// Reset record parameters
/// </summary>
void CKinectV2Recorder::ResetRecordParameters()
{
    m_bRecord = false;
    while (!m_qInfraredFrameQueue.empty() || !m_qDepthFrameQueue.empty() || !m_qColorFrameQueue.empty())
    {
        std::this_thread::sleep_for(std::chrono::microseconds(33));
    }
    m_nInfraredIndex = 0;
    m_nDepthIndex = 0;
    m_nColorIndex = 0;
    m_vInfraredList.resize(0);
    m_vDepthList.resize(0);
    m_vColorList.resize(0);
    m_nStartTime = 0;
    
	getTimeString(m_cModelFolder);
	wcscpy(m_cSaveFolder, m_cModelFolder);
    SendDlgItemMessage(m_hWnd, IDC_BUTTON_RECORD, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)m_hRecord);
}




void CKinectV2Recorder::getTimeString(WCHAR *wstr)
{
	time_t now = time(0);
	tm *ltm = localtime(&now);
	char buffer[200];
	int year = 1900 + ltm->tm_year;
	int month = ltm->tm_mon + 1;
	int day = ltm->tm_mday;
	int hour = ltm->tm_hour;
	int minute = ltm->tm_min;
	int sec = ltm->tm_sec;
	size_t n = sprintf(buffer, "%s\\KinectV2Recorder\\%d_%02d_%02d_%02d%02d%02d", m_strMyDocumentPath.c_str(), year, month, day, hour, minute, sec);


	const size_t cSize = strlen(buffer) + 1;
	//wchar_t* wc = new wchar_t[cSize];
	size_t tmp = 0;
	mbstowcs_s(&tmp, wstr, cSize, buffer, cSize);
}

std::wstring CKinectV2Recorder::s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

std::string CKinectV2Recorder::WCHARtoString(WCHAR* c)
{
	std::wstring ws(c);
	std::string strPath(ws.begin(), ws.end());
	return strPath;
}

std::string CKinectV2Recorder::getIni(
	const std::string iniPath,
	const std::string section,
	const std::string key)
{
	// Get current working directory with execution file full path
	WCHAR path[1000];
	GetModuleFileNameW(NULL, path, 1000);

	// Convert WCHAR to string
	std::wstring ws(path);
	std::string strPath;
	strPath.assign(ws.begin(), ws.end());

	// Get rid of execution file 
	std::string::size_type pos = std::string(strPath).find_last_of("\\/");
	strPath = std::string(strPath).substr(0, pos);

	// Parse ini
	boost::property_tree::ptree pt;
	//boost::property_tree::ini_parser::read_ini(strPath + "\\setting.ini", pt);
	try
	{
		boost::property_tree::ini_parser::read_ini(strPath + "\\" + iniPath, pt);
	}
	catch (const boost::property_tree::ptree_error &e)
	{
		std::wstring stemp = s2ws(e.what());
		LPCWSTR error_msg = stemp.c_str();

		MessageBox(NULL,
			error_msg,//L"Frame dropping occured...\n",
			L"IMU info load from setting.ini",
			MB_OK | MB_ICONERROR
		);
		exit(1);
	}
	return pt.get<std::string>(section + "." + key);
}

void CKinectV2Recorder::writeCSV(
	FILE* f, 
	std::string time, 
	std::vector<double> content)
{
	fprintf(f, "%s,%0.4f,%0.4f,%0.4f,%0.4f,%0.4f,%0.4f\n", 
		time.c_str(), 
		content.at(0),
		content.at(1), 
		content.at(2), 
		content.at(3), 
		content.at(4), 
		content.at(5) );
}
