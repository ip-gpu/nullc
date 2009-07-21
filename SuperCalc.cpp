#include "stdafx.h"
#include "SuperCalc.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "commctrl.h"
#include "richedit.h"
#pragma comment(lib, "comctl32.lib")
#include <windowsx.h>

#include <MMSystem.h>
#pragma comment(lib, "Winmm.lib")

#include <iostream>

#include "NULLC/nullc.h"
#include "NULLC/ParseClass.h"

#include "Colorer.h"

#include "UnitTests.h"

#define MAX_LOADSTRING 100

HINSTANCE hInst;
char szTitle[MAX_LOADSTRING];
char szWindowClass[MAX_LOADSTRING];

WORD				MyRegisterClass(HINSTANCE hInstance);
bool				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, unsigned int, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, unsigned int, WPARAM, LPARAM);

//Window handles
HWND hWnd;
HWND hButtonCalc;	//calculate button
HWND hButtonCalcX86;//calculate button
HWND hDoOptimize;	//optimization checkbox
HWND hTextArea;		//code text area (rich edit)
HWND hResult;		//label with execution result
HWND hCode;			//disabled text area for errors and asm-like code output
HWND hLog;			//disabled text area for log information of AST creation
HWND hVars;			//disabled text area that shows values of all variables in global scope

//colorer, compiler and executor
Colorer*	colorer;

//for text update
bool needTextUpdate;
DWORD lastUpdate;

char *variableData = NULL;
void FillComplexVariableInfo(TypeInfo* type, int address, HTREEITEM parent);
void FillArrayVariableInfo(TypeInfo* type, int address, HTREEITEM parent);

struct ArrayPtr{ char* ptr; int len; };

int myGetTime()
{
	LARGE_INTEGER freq, count;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&count);
	double temp = double(count.QuadPart) / double(freq.QuadPart);
	return int(temp*1000.0);
}

double myGetPreciseTime()
{
	LARGE_INTEGER freq, count;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&count);
	double temp = double(count.QuadPart) / double(freq.QuadPart);
	return temp*1000.0;
}

FILE* myFileOpen(ArrayPtr name, ArrayPtr access)
{
	variableData = (char*)nullcGetVariableData();
	return fopen(reinterpret_cast<long long>(name.ptr)+variableData, reinterpret_cast<long long>(access.ptr)+variableData);
}

void myFileWrite(FILE* file, ArrayPtr arr)
{
	variableData = (char*)nullcGetVariableData();
	fwrite(reinterpret_cast<long long>(arr.ptr)+variableData, 1, arr.len, file);
}

template<typename T>
void myFileWriteType(FILE* file, T val)
{
	fwrite(&val, sizeof(T), 1, file);
}

template<typename T>
void myFileWriteTypePtr(FILE* file, T* val)
{
	variableData = (char*)nullcGetVariableData();
	fwrite(reinterpret_cast<long long>(val)+variableData, sizeof(T), 1, file);
}

void myFileRead(FILE* file, ArrayPtr arr)
{
	variableData = (char*)nullcGetVariableData();
	fread(reinterpret_cast<long long>(arr.ptr)+variableData, 1, arr.len, file);
}

template<typename T>
void myFileReadTypePtr(FILE* file, T* val)
{
	variableData = (char*)nullcGetVariableData();
	fread(reinterpret_cast<long long>(val)+variableData, sizeof(T), 1, file);
}

void myFileClose(FILE* file)
{
	fclose(file);
}

bool	consoleActive = false;
HANDLE	conStdIn;
HANDLE	conStdOut;

void InitConsole();
void DeInitConsole();

// Does nothing at this point
int __stdcall ConsoleEvent(DWORD eventType)
{
	switch(eventType)
	{
	case CTRL_C_EVENT:
		return 1;
	case CTRL_BREAK_EVENT:
		return 1;
	case CTRL_CLOSE_EVENT:
		return 1;
	default:
		return 0;
	}
}

void InitConsole()
{
	if(consoleActive)
		return;
	AllocConsole();
	consoleActive = true;
	conStdIn = GetStdHandle(STD_INPUT_HANDLE);
	conStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD fdwMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT; 
    SetConsoleMode(conStdIn, fdwMode);
	SetConsoleCtrlHandler(ConsoleEvent, 1);
}

void DeInitConsole()
{
	if(!consoleActive)
		return;
	FreeConsole();
	consoleActive = false;
}

void WriteToConsole(ArrayPtr data)
{
	InitConsole();
	DWORD written;
	WriteFile(conStdOut, reinterpret_cast<long long>(data.ptr)+variableData, data.len-1, &written, NULL); 
}

void ReadIntFromConsole(int* val)
{
	InitConsole();
	char temp[128];
	DWORD read;
	ReadFile(conStdIn, temp, 128, &read, NULL);
	*(int*)(reinterpret_cast<long long>(val)+variableData) = atoi(temp);

	DWORD written;
	WriteFile(conStdOut, "\r\n", 2, &written, NULL); 
}

int ReadTextFromConsole(ArrayPtr data)
{
	char buffer[2048];

	InitConsole();
	DWORD read;
	ReadFile(conStdIn, buffer, 2048, &read, NULL);
	buffer[read-1] = 0;
	char *target = reinterpret_cast<long long>(data.ptr) + variableData;
	int c = 0;
	for(unsigned int i = 0; i < read; i++)
	{
		buffer[c++] = buffer[i];
		if(buffer[i] == '\b')
			c -= 2;
		if(c < 0)
			c = 0;
	}
	if(c < data.len)
		buffer[c-1] = 0;
	else
		buffer[data.len-1] = 0;
	memcpy(target, buffer, data.len);

	DWORD written;
	WriteFile(conStdOut, "\r\n", 2, &written, NULL);
	return (c < data.len ? c : data.len);
}

struct float4c{ float x, y, z, w; };

void PrintFloat4(float4c n)
{
	InitConsole();
	DWORD written;
	char temp[128];
	sprintf(temp, "{%f, %f, %f, %f}\r\n", n.x, n.y, n.z, n.w);
	WriteFile(conStdOut, temp, (unsigned int)strlen(temp), &written, NULL); 
}

void PrintLong(long long lg)
{
	InitConsole();
	DWORD written;
	char temp[128];
	sprintf(temp, "{%I64d}\r\n", lg);
	WriteFile(conStdOut, temp, (unsigned int)strlen(temp), &written, NULL); 
}

void draw_rect(int x, int y, int width, int height, int color)
{
	//DWORD written;
	/*char buf[64];
	printf("%d %d %d %d %d\r\n", x, y, width, height, color);*/
	//fwrite(buf, strlen(buf), 1, zeuxOut);
}

char typeTest(int x, short y, char z, int d, long long u, float m, double k)
{
	InitConsole();
	DWORD written;
	char buf[64];
	sprintf(buf, "%d %d %d %d %I64d %f %f", x, y, z, d, u, m, k);
	WriteFile(conStdOut, buf, strlen(buf), &written, NULL); 
	return 12;
}

char* buf;

int APIENTRY WinMain(HINSTANCE	hInstance,
					HINSTANCE	hPrevInstance,
					LPTSTR		lpCmdLine,
					int			nCmdShow)
{
	(void)lpCmdLine;
	(void)hPrevInstance;
	buf = new char[100000];

	MSG msg;
	HACCEL hAccelTable;

	needTextUpdate = true;
	lastUpdate = GetTickCount();

	bool runUnitTests = true;
	if(runUnitTests)
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONIN$", "r", stdin);

		RunTests();
	}

	nullcInit();
#define REGISTER(func, proto) nullcAddExternalFunction((void (*)())func, proto)
REGISTER(draw_rect, "void draw_rect(int x, int y, int width, int height, int color);");

	//nullcDeinit();
	//nullcInit();
	/*
	const char *code="return 2+2;"; nullcCompile(code);
	unsigned int time; nullcExecuteVM(&time, NULL);
	const char *res = nullcGetResult();

	nullcDeinit();
	return 0;
*/
	colorer = NULL;

	//typeTest(12, 14, 'c', 15, 5l, 5.0);

	nullcAddExternalFunction((void (*)())(typeTest), "char typeTest(int x, short y, char z, int d, long u, float m, double k);");

	nullcAddExternalFunction((void (*)())(PrintFloat4), "void TestEx(float4 test);");
	nullcAddExternalFunction((void (*)())(PrintLong), "void TestEx2(long test);");

	nullcAddExternalFunction((void (*)())(myGetTime), "int clock();");

	nullcAddExternalFunction((void (*)())(myFileOpen), "file FileOpen(char[] name, char[] access);");
	nullcAddExternalFunction((void (*)())(myFileClose), "void FileClose(file fID);");
	nullcAddExternalFunction((void (*)())(myFileWrite), "void FileWrite(file fID, char[] arr);");
	nullcAddExternalFunction((void (*)())(myFileWriteTypePtr<char>), "void FileWrite(file fID, char ref data);");
	nullcAddExternalFunction((void (*)())(myFileWriteTypePtr<short>), "void FileWrite(file fID, short ref data);");
	nullcAddExternalFunction((void (*)())(myFileWriteTypePtr<int>), "void FileWrite(file fID, int ref data);");
	nullcAddExternalFunction((void (*)())(myFileWriteTypePtr<long long>), "void FileWrite(file fID, long ref data);");
	nullcAddExternalFunction((void (*)())(myFileWriteType<char>), "void FileWrite(file fID, char data);");
	nullcAddExternalFunction((void (*)())(myFileWriteType<short>), "void FileWrite(file fID, short data);");
	nullcAddExternalFunction((void (*)())(myFileWriteType<int>), "void FileWrite(file fID, int data);");
	nullcAddExternalFunction((void (*)())(myFileWriteType<long long>), "void FileWrite(file fID, long data);");

	nullcAddExternalFunction((void (*)())(myFileRead), "void FileRead(file fID, char[] arr);");
	nullcAddExternalFunction((void (*)())(myFileReadTypePtr<char>), "void FileRead(file fID, char ref data);");
	nullcAddExternalFunction((void (*)())(myFileReadTypePtr<short>), "void FileRead(file fID, short ref data);");
	nullcAddExternalFunction((void (*)())(myFileReadTypePtr<int>), "void FileRead(file fID, int ref data);");
	nullcAddExternalFunction((void (*)())(myFileReadTypePtr<long long>), "void FileRead(file fID, long ref data);");

	nullcAddExternalFunction((void (*)())(WriteToConsole), "void Print(char[] text);");
	nullcAddExternalFunction((void (*)())(ReadIntFromConsole), "void Input(int ref num);");
	nullcAddExternalFunction((void (*)())(ReadTextFromConsole), "int Input(char[] buf);");

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SUPERCALC, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if(!InitInstance(hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_SUPERCALC);

	// Main message loop:
	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	delete colorer;

	nullcDeinit();

	delete[] buf;

	return (int) msg.wParam;
}

WORD MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_SUPERCALC);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)IDC_SUPERCALC;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

char* GetLastErrorDesc()
{
	char* msgBuf = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&msgBuf), 0, NULL);
	return msgBuf;
}

bool InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		100, 100, 900, 450, NULL, NULL, hInstance, NULL);
	if(!hWnd)
		return 0;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	hButtonCalc = CreateWindow("BUTTON", "Calculate", WS_CHILD,
		5, 185, 100, 30, hWnd, NULL, hInstance, NULL);
	if(!hButtonCalc)
		return 0;
	ShowWindow(hButtonCalc, nCmdShow);
	UpdateWindow(hButtonCalc);

	hButtonCalcX86 = CreateWindow("BUTTON", "Run Native X86", WS_CHILD,
		800-140, 185, 130, 30, hWnd, NULL, hInstance, NULL);
	if(!hButtonCalcX86)
		return 0;
	ShowWindow(hButtonCalcX86, nCmdShow);
	UpdateWindow(hButtonCalcX86);

	hDoOptimize = CreateWindow("BUTTON", "Optimize", BS_AUTOCHECKBOX | WS_CHILD,
		800-240, 185, 90, 30, hWnd, NULL, hInstance, NULL);
	if(!hDoOptimize)
		return 0;
	ShowWindow(hDoOptimize, nCmdShow);
	UpdateWindow(hDoOptimize);

	INITCOMMONCONTROLSEX commControlTypes;
	commControlTypes.dwSize = sizeof(INITCOMMONCONTROLSEX);
	commControlTypes.dwICC = ICC_TREEVIEW_CLASSES;
	int commControlsAvailable = InitCommonControlsEx(&commControlTypes);
	if(!commControlsAvailable)
		return 0;

	/*HMODULE sss = */LoadLibrary("RICHED32.dll");

	FILE *startText = fopen("code.txt", "rb");
	char *fileContent = NULL;
	if(startText)
	{
		fseek(startText, 0, SEEK_END);
		unsigned int textSize = ftell(startText);
		fseek(startText, 0, SEEK_SET);
		fileContent = new char[textSize+1];
		fread(fileContent, 1, textSize, startText);
		fileContent[textSize] = 0;
		fclose(startText);
	}
	hTextArea = CreateWindow("RICHEDIT", fileContent ? fileContent : "int a = 5;\r\nint ref b = &a;\r\nreturn 1;",
		WS_CHILD | WS_BORDER |  WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
		5, 5, 780, 175, hWnd, NULL, hInstance, NULL);
	delete fileContent;
	fileContent = NULL;
	if(!hTextArea)
		return 0;
	ShowWindow(hTextArea, nCmdShow);
	UpdateWindow(hTextArea);

	colorer = new Colorer(hTextArea);

	SendMessage(hTextArea, EM_SETEVENTMASK, 0, ENM_CHANGE);
	unsigned int widt = (800-25)/4;

	hCode = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER |  WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY,
		5, 225, widt*2, 165, hWnd, NULL, hInstance, NULL);
	if(!hCode)
		return 0;
	ShowWindow(hCode, nCmdShow);
	UpdateWindow(hCode);
	SendMessage(hCode, WM_SETFONT, (WPARAM)CreateFont(15,0,0,0,0,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FF_DONTCARE,"Courier New"), 0);

	hLog = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER |  WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY,
		2*widt+10, 200, widt-100, 165, hWnd, NULL, hInstance, NULL);
	if(!hLog)
		return 0;
	ShowWindow(hLog, nCmdShow);
	UpdateWindow(hLog);

	hVars = CreateWindow(WC_TREEVIEW, "", WS_CHILD | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_EDITLABELS,
		3*widt+15, 225, widt, 165, hWnd, NULL, hInstance, NULL);
	if(!hVars)
		return 0;
	ShowWindow(hVars, nCmdShow);
	UpdateWindow(hVars);

	hResult = CreateWindow("STATIC", "The result will be here", WS_CHILD,
		110, 185, 300, 30, hWnd, NULL, hInstance, NULL);
	if(!hResult)
		return 0;
	ShowWindow(hResult, nCmdShow);
	UpdateWindow(hResult);

	PostMessage(hWnd, WM_SIZE, 0, (394<<16)+(900-16));

	SetTimer(hWnd, 1, 500, 0);
	return TRUE;
}

nullres RunCallback(unsigned int cmdNum)
{
	std::string str;
	char num[32];
	str = std::string("SuperCalc [") + _itoa(cmdNum, num, 10) + "]";
	SetWindowText(hWnd, str.c_str());
	UpdateWindow(hWnd);
	static int ignore = false;
	if(cmdNum % 300000000 == 0 && !ignore)
	{
		int butSel = MessageBox(hWnd, "Code execution can take a long time. Do you wish to continue?\r\nPress Cancel if you don't want to see this warning again", "Warning: long execution time", MB_YESNOCANCEL);
		if(butSel == IDYES)
			return true;
		else if(butSel == IDNO)
			return false;
		else
			ignore = true;
	}
	return true;
}

const char* GetSimpleVariableValue(TypeInfo* type, int address)
{
	static char val[256];
	if(type->type == TypeInfo::TYPE_INT)
	{
		sprintf(val, "%d", *((int*)&variableData[address]));
	}else if(type->type == TypeInfo::TYPE_SHORT){
		sprintf(val, "%d", *((short*)&variableData[address]));
	}else if(type->type == TypeInfo::TYPE_CHAR){
		if(*((unsigned char*)&variableData[address]))
			sprintf(val, "'%c' (%d)", *((unsigned char*)&variableData[address]), (int)(*((unsigned char*)&variableData[address])));
		else
			sprintf(val, "0");
	}else if(type->type == TypeInfo::TYPE_FLOAT){
		sprintf(val, "%f", *((float*)&variableData[address]));
	}else if(type->type == TypeInfo::TYPE_LONG){
		sprintf(val, "%I64d", *((long long*)&variableData[address]));
	}else if(type->type == TypeInfo::TYPE_DOUBLE){
		sprintf(val, "%f", *((double*)&variableData[address]));
	}else{
		sprintf(val, "not basic type");
	}
	return val;
}

void FillComplexVariableInfo(TypeInfo* type, int address, HTREEITEM parent)
{
	TVINSERTSTRUCT helpInsert;
	helpInsert.hParent = parent;
	helpInsert.hInsertAfter = TVI_LAST;
	helpInsert.item.mask = TVIF_TEXT;
	helpInsert.item.cchTextMax = 0;

	char name[256];
	HTREEITEM lastItem;

	for(TypeInfo::MemberVariable *curr = type->firstVariable; curr; curr = curr->next)//for(unsigned int mn = 0; mn < type->memberData.size(); mn++)
	{
		TypeInfo::MemberVariable &mInfo = *curr;//type->memberData[mn];

		sprintf(name, "%s %s = ", mInfo.type->GetFullTypeName(), mInfo.name);

		if(mInfo.type->type != TypeInfo::TYPE_COMPLEX && mInfo.type->arrLevel == 0)
			strcat(name, GetSimpleVariableValue(mInfo.type, address + mInfo.offset));

		if(mInfo.type->arrLevel == 1 && mInfo.type->subType->type == TypeInfo::TYPE_CHAR)
			sprintf(name+strlen(name), "\"%s\"", (char*)(variableData + address + mInfo.offset));

		helpInsert.item.pszText = name;
		lastItem = TreeView_InsertItem(hVars, &helpInsert);

		if(mInfo.type->arrLevel != 0)
		{
			FillArrayVariableInfo(mInfo.type, address + mInfo.offset, lastItem);
		}else if(mInfo.type->type == TypeInfo::TYPE_COMPLEX){
			FillComplexVariableInfo(mInfo.type, address + mInfo.offset, lastItem);
		}
	}
}

void FillArrayVariableInfo(TypeInfo* type, int address, HTREEITEM parent)
{
	TVINSERTSTRUCT helpInsert;
	helpInsert.hParent = parent;
	helpInsert.hInsertAfter = TVI_LAST;
	helpInsert.item.mask = TVIF_TEXT;
	helpInsert.item.cchTextMax = 0;

	TypeInfo* subType = type->subType;
	char name[256];
	HTREEITEM lastItem;

	unsigned int arrSize = type->arrSize;
	if(arrSize == -1)
	{
		arrSize = *((int*)&variableData[address+4]);
		address = *((int*)&variableData[address]);
	}
	for(unsigned int n = 0; n < arrSize; n++, address += subType->size)
	{
		if(n > 100)
		{
			sprintf(name, "[%d]-[%d]...", n, type->arrSize);
			helpInsert.item.pszText = name;
			lastItem = TreeView_InsertItem(hVars, &helpInsert);
			break;
		}
		sprintf(name, "[%d]: ", n);

		if(subType->arrLevel == 1 && subType->subType->type == TypeInfo::TYPE_CHAR)
			sprintf(name+strlen(name), "\"%s\"", (char*)(variableData+address));

		if(subType->type != TypeInfo::TYPE_COMPLEX && subType->arrLevel == 0)
			strcat(name, GetSimpleVariableValue(subType, address));

		helpInsert.item.pszText = name;
		lastItem = TreeView_InsertItem(hVars, &helpInsert);

		if(subType->arrLevel != 0)
		{
			FillArrayVariableInfo(subType, address, lastItem);
		}else if(subType->type == TypeInfo::TYPE_COMPLEX){
			FillComplexVariableInfo(subType, address, lastItem);
		}
	}
}

void FillVariableInfoTree()
{
	unsigned int varCount = 0;
	VariableInfo **varInfo = (VariableInfo**)nullcGetVariableInfo(&varCount);
	TreeView_DeleteAllItems(hVars);

	TVINSERTSTRUCT helpInsert;
	helpInsert.hParent = NULL;
	helpInsert.hInsertAfter = TVI_ROOT;
	helpInsert.item.mask = TVIF_TEXT;
	helpInsert.item.cchTextMax = 0;

	unsigned int address = 0;
	char name[256];
	HTREEITEM lastItem;
	for(unsigned int i = 0; i < varCount; i++)
	{
		VariableInfo &currVar = *(*(varInfo+i));
		address = currVar.pos;
		sprintf(name, "%d: %s%s %s = ", address, (currVar.isConst ? "const " : ""), (*currVar.varType).GetFullTypeName(), currVar.name);

		if(currVar.varType->type != TypeInfo::TYPE_COMPLEX && currVar.varType->arrLevel == 0)
			strcat(name, GetSimpleVariableValue(currVar.varType, address));

		if(currVar.varType->arrLevel == 1 && currVar.varType->subType->type == TypeInfo::TYPE_CHAR)
			sprintf(name+strlen(name), "\"%s\"", (char*)(variableData+address));

		if(currVar.varType->arrSize == -1)
			sprintf(name+strlen(name), " address: %d, size: %d", *((int*)&variableData[address]), *((int*)&variableData[address+4]));

		helpInsert.item.pszText = name;
		lastItem = TreeView_InsertItem(hVars, &helpInsert);

		if(currVar.varType->arrLevel != 0)
		{
			FillArrayVariableInfo(currVar.varType, address, lastItem);
		}else if(currVar.varType->type == TypeInfo::TYPE_COMPLEX){
			FillComplexVariableInfo(currVar.varType, address, lastItem);
		}
		address += currVar.varType->size;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
	
	case WM_COMMAND:
		wmId	= LOWORD(wParam); 
		wmEvent = HIWORD(wParam);

		if((HWND)lParam == hButtonCalc)
		{
		
			/*static*/ int callNum = -1;
			callNum++;
			GetWindowText(hTextArea, buf, 100000);

			DeInitConsole();

			char	result[128];

			nullcSetExecutor(NULLC_VM);
			nullcSetExecutorOptions(false);
			double compTime = 0.0, bytecodeTime = 0.0, linkTime = 0.0, execTime = 0.0;
			int kkk = 0;
			double time = myGetPreciseTime();
		//for(kkk = 0; kkk < 30000; kkk++)
		//{
			nullres good = nullcCompile(buf);
		//	nullcSaveListing("asm.txt");
		//}
			compTime += myGetPreciseTime()-time;
			time = myGetPreciseTime();

			char *bytecode;
		//for(kkk = 0; kkk < 300000; kkk++)
		//{
			unsigned int size = nullcGetBytecode(&bytecode);
		//	delete[] bytecode;
		//}
			bytecodeTime += myGetPreciseTime()-time;
		//	unsigned int size = nullcGetBytecode(&bytecode);
			time = myGetPreciseTime();
		//for(kkk = 0; kkk < 300000; kkk++)
		//{
			nullcClean();
			nullcLinkCode(bytecode, 1);
		//}
			linkTime += myGetPreciseTime()-time;

			delete[] bytecode;
			if(!good)
			{
				SetWindowText(hCode, nullcGetCompilationError());
			}else{
				variableData = (char*)nullcGetVariableData();
				
				double time = myGetPreciseTime();
				nullres goodRun = nullcRunFunction(callNum%2 ? "draw_progress_bar" : NULL);

				

				if(goodRun)
				{
					const char *val = nullcGetResult();
					execTime += myGetPreciseTime()-time;

					sprintf_s(result, 128, "The answer is: %s [in %f]", val, execTime/(kkk+1.0));

					variableData = (char*)nullcGetVariableData();
					//FillVariableInfoTree();
				}else{
					sprintf_s(result, 128, "%s [in %f]", nullcGetRuntimeError(), myGetPreciseTime()-time);
				}
				SetWindowText(hResult, result);
			}
			//SetWindowText(hLog, nullcGetCompilationLog());
		//}
			//linkTime += myGetPreciseTime()-time;
			//sprintf_s(result, 128, "compile: %f bytecode: %f link: %f", compTime, bytecodeTime, linkTime);
			SetWindowText(hResult, result);
		}
		if((HWND)lParam == hButtonCalcX86)
		{
			/*static*/ int callNum = -1;
			callNum++;
			GetWindowText(hTextArea, buf, 100000);

			DeInitConsole();

			nullcSetExecutor(NULLC_X86);
			nullcSetExecutorOptions(!!Button_GetCheck(hDoOptimize));

			char	result[128];

			nullres good = nullcCompile(buf);
			nullcSaveListing("asm.txt");

			char *bytecode;
			nullcGetBytecode(&bytecode);
			nullcClean();
			nullcLinkCode(bytecode, 1);
			delete[] bytecode;
			if(!good)
			{
				SetWindowText(hCode, nullcGetCompilationError());
			}else{
				variableData = (char*)nullcGetVariableData();
				
				double time = myGetPreciseTime();
				nullres goodRun = nullcRunFunction(callNum%2 ? "draw_progress_bar" : NULL);
				if(goodRun)
				{
					const char *val = nullcGetResult();
					double execTime = myGetPreciseTime()-time;

					sprintf_s(result, 128, "The answer is: %s [in %f]", val, execTime);

					variableData = (char*)nullcGetVariableData();
					FillVariableInfoTree();
				}else{
					sprintf_s(result, 128, "%s", nullcGetRuntimeError());
				}
				SetWindowText(hResult, result);
			}
			SetWindowText(hLog, nullcGetCompilationLog());
		}
		if((HWND)lParam == hTextArea)
		{
			if(wmEvent == EN_CHANGE)
			{
				needTextUpdate = true;
				lastUpdate = GetTickCount();
			}
		}
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			//DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_FILE_SAVE:
			break;
		case ID_FILE_LOAD:
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	
	case WM_PAINT:
		{
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_TIMER:
	{
		if(!needTextUpdate || (GetTickCount()-lastUpdate < 500))
			break;
		bool bRetFocus = false;
		CHARRANGE cr;
		if(GetFocus() == hTextArea)
		{
			bRetFocus = true;
			SendMessage(hTextArea, (unsigned int)EM_EXGETSEL, 0L, (LPARAM)&cr);  
			SetFocus(hWnd);
		}
		string str = "";
		SetWindowText(hCode, str.c_str());

		/*if(!colorer->ColorText())
		{
			SetWindowText(hCode, colorer->GetError().c_str());
		}*/
		if(bRetFocus)
		{
			SetFocus(hTextArea);
			Edit_SetSel(hTextArea, cr.cpMin, cr.cpMax);
		}
		needTextUpdate = false;
	}
		break;
	case WM_LBUTTONUP:
		break;
	case WM_SIZE:
	{
		SetWindowPos(hTextArea, HWND_TOP, 5,5,LOWORD(lParam)-10, (int)(5.0/9.0*HIWORD(lParam)), NULL);
		SetWindowPos(hButtonCalc, HWND_TOP, 5,7+(int)(5.0/9.0*HIWORD(lParam)),100, 30, NULL);
		SetWindowPos(hButtonCalcX86, HWND_TOP, (int)(LOWORD(lParam))-135,7+(int)(5.0/9.0*HIWORD(lParam)),130, 30, NULL);
		SetWindowPos(hDoOptimize, HWND_TOP, (int)(LOWORD(lParam))-235,7+(int)(5.0/9.0*HIWORD(lParam)),95, 30, NULL);
		SetWindowPos(hResult, HWND_TOP, 110,7+(int)(5.0/9.0*HIWORD(lParam)),(int)(LOWORD(lParam))-345, 30, NULL);
		unsigned int widt = (LOWORD(lParam)-20)/4;
		SetWindowPos(hCode, HWND_TOP, 5,40+(int)(5.0/9.0*HIWORD(lParam)),2*widt, (int)(3.0/9.0*HIWORD(lParam)), NULL);
		SetWindowPos(hLog, HWND_TOP, 2*widt+10,40+(int)(5.0/9.0*HIWORD(lParam)),widt, (int)(3.0/9.0*HIWORD(lParam)), NULL);
		SetWindowPos(hVars, HWND_TOP, 3*widt+15,40+(int)(5.0/9.0*HIWORD(lParam)),widt, (int)(3.0/9.0*HIWORD(lParam)), NULL);
	}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}