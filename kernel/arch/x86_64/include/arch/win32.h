#ifndef ARCH_X86_64_WIN32_H
#define ARCH_X86_64_WIN32_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <arch/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HKEY
typedef void* HKEY;
#endif

#ifndef REGSAM
typedef uint32_t REGSAM;
#endif

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long ULONGLONG;
typedef signed long long LONGLONG;
typedef void* PVOID;
typedef PVOID LPVOID;
typedef const void* LPCVOID;
typedef DWORD* PDWORD;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef long LONG;
typedef int64_t LARGE_INTEGER;
typedef LARGE_INTEGER* PLARGE_INTEGER;
typedef uint32_t ACCESS_MASK;
typedef size_t SIZE_T;
typedef SIZE_T* PSIZE_T;
typedef unsigned char BOOLEAN;
typedef ULONG* PULONG;
typedef ULONGLONG DWORD_PTR;
typedef ULONGLONG UINT_PTR;
typedef LONGLONG INT_PTR;
typedef LONGLONG LONG_PTR;
typedef ULONGLONG ULONG_PTR;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef char* PCHAR;
typedef WCHAR* PWSTR;
typedef const WCHAR* PCWSTR;
typedef CHAR* LPSTR;
typedef const CHAR* LPCSTR;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
typedef void* HANDLE;
typedef int INT;
#define VOID void
typedef DWORD* LPDWORD;
typedef HANDLE* PHANDLE;
typedef BYTE* LPBYTE;
typedef HANDLE SC_HANDLE;
typedef UINT ALG_ID;
#ifndef _SID_NAME_USE_DEFINED
#define _SID_NAME_USE_DEFINED
typedef enum _SID_NAME_USE {
    SidTypeUser = 1,
    SidTypeGroup,
    SidTypeDomain,
    SidTypeAlias,
    SidTypeWellKnownGroup,
    SidTypeDeletedAccount,
    SidTypeInvalid,
    SidTypeUnknown,
    SidTypeComputer,
    SidTypeLabel,
    SidTypeLogonSession
} SID_NAME_USE, *PSID_NAME_USE;
#endif

#ifndef _LUID_DEFINED
#define _LUID_DEFINED
typedef struct _LUID {
    DWORD LowPart;
    LONG  HighPart;
} LUID, *PLUID;
#endif

#ifndef _LUID_AND_ATTRIBUTES_DEFINED
#define _LUID_AND_ATTRIBUTES_DEFINED
typedef struct _LUID_AND_ATTRIBUTES {
    LUID     Luid;
    DWORD    Attributes;
} LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;
#endif

typedef HANDLE HWND;
typedef HANDLE HMODULE;
typedef HANDLE HINSTANCE;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef HANDLE HGDIOBJ;
typedef HANDLE HBITMAP;
typedef HANDLE HBRUSH;
typedef HANDLE HPEN;
typedef HANDLE HFONT;
typedef HANDLE HRGN;
typedef HANDLE HMENU;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HDESK;
typedef HANDLE HWINSTA;
typedef HANDLE HDC;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef DWORD LCID;
typedef DWORD LANGID;
typedef DWORD COLORREF;
typedef uint64_t DWORDLONG;
typedef DWORDLONG* PDWORDLONG;
typedef DWORD_PTR* PDWORD_PTR;
typedef intptr_t (*FARPROC)();
typedef unsigned short ATOM;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID, IID, CLSID, *REFGUID, *REFCLSID, *REFIID;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagWINDOWINFO {
    DWORD cbSize;
    RECT  rcWindow;
    RECT  rcClient;
    DWORD dwStyle;
    DWORD dwExStyle;
    UINT  cxWindowBorders;
    UINT  cyWindowBorders;
    ATOM  atomWindowType;
    WORD  wCreatorVersion;
} WINDOWINFO, *PWINDOWINFO, *LPWINDOWINFO;

#define WM_NULL                        0x0000
#define WM_CREATE                      0x0001
#define WM_DESTROY                     0x0002
#define WM_MOVE                        0x0003
#define WM_SIZE                        0x0005
#define WM_ACTIVATE                    0x0006
#define WM_SETFOCUS                    0x0007
#define WM_KILLFOCUS                   0x0008
#define WM_ENABLE                      0x000A
#define WM_SETREDRAW                   0x000B
#define WM_SETTEXT                     0x000C
#define WM_GETTEXT                     0x000D
#define WM_GETTEXTLENGTH               0x000E
#define WM_PAINT                       0x000F
#define WM_CLOSE                       0x0010
#define WM_QUIT                        0x0012
#define WM_ERASEBKGND                  0x0014
#define WM_SYSCOLORCHANGE              0x0015
#define WM_SHOWWINDOW                  0x0018
#define WM_ACTIVATEAPP                 0x001C
#define WM_SETCURSOR                   0x0020
#define WM_MOUSEACTIVATE               0x0021
#define WM_GETMINMAXINFO               0x0024
#define WM_WINDOWPOSCHANGING           0x0046
#define WM_WINDOWPOSCHANGED            0x0047
#define WM_CONTEXTMENU                 0x007B
#define WM_STYLECHANGING               0x007C
#define WM_STYLECHANGED                0x007D
#define WM_DISPLAYCHANGE               0x007E
#define WM_GETICON                     0x007F
#define WM_SETICON                     0x0080
#define WM_NCCREATE                    0x0081
#define WM_NCDESTROY                   0x0082
#define WM_NCCALCSIZE                  0x0083
#define WM_NCHITTEST                   0x0084
#define WM_NCPAINT                     0x0085
#define WM_NCACTIVATE                  0x0086
#define WM_GETDLGCODE                  0x0087
#define WM_SYNCPAINT                   0x0088
#define WM_NCMOUSEMOVE                 0x00A0
#define WM_NCLBUTTONDOWN               0x00A1
#define WM_NCLBUTTONUP                 0x00A2
#define WM_NCLBUTTONDBLCLK             0x00A3
#define WM_NCRBUTTONDOWN               0x00A4
#define WM_NCRBUTTONUP                 0x00A5
#define WM_NCRBUTTONDBLCLK             0x00A6
#define WM_NCMBUTTONDOWN               0x00A7
#define WM_NCMBUTTONUP                 0x00A8
#define WM_NCMBUTTONDBLCLK             0x00A9
#define WM_KEYFIRST                    0x0100
#define WM_KEYDOWN                     0x0100
#define WM_KEYUP                       0x0101
#define WM_CHAR                        0x0102
#define WM_DEADCHAR                    0x0103
#define WM_SYSKEYDOWN                  0x0104
#define WM_SYSKEYUP                    0x0105
#define WM_SYSCHAR                     0x0106
#define WM_SYSDEADCHAR                 0x0107
#define WM_IME_STARTCOMPOSITION        0x010D
#define WM_IME_ENDCOMPOSITION          0x010E
#define WM_IME_COMPOSITION             0x010F
#define WM_INITDIALOG                  0x0110
#define WM_COMMAND                     0x0111
#define WM_SYSCOMMAND                  0x0112
#define WM_TIMER                       0x0113
#define WM_HSCROLL                     0x0114
#define WM_VSCROLL                     0x0115
#define WM_INITMENU                    0x0116
#define WM_INITMENUPOPUP               0x0117
#define WM_MENUSELECT                  0x011F
#define WM_MENUCHAR                    0x0120
#define WM_ENTERIDLE                   0x0121
#define WM_MENURBUTTONUP               0x0122
#define WM_MENUDRAG                    0x0123
#define WM_MENUGETOBJECT               0x0124
#define WM_UNINITMENUPOPUP             0x0125
#define WM_MENUCOMMAND                 0x0126
#define WM_CTLCOLORMSGBOX              0x0132
#define WM_CTLCOLOREDIT                0x0133
#define WM_CTLCOLORLISTBOX             0x0134
#define WM_CTLCOLORBTN                 0x0135
#define WM_CTLCOLORDLG                 0x0136
#define WM_CTLCOLORSCROLLBAR           0x0137
#define WM_CTLCOLORSTATIC              0x0138
#define WM_MOUSEFIRST                  0x0200
#define WM_MOUSEMOVE                   0x0200
#define WM_LBUTTONDOWN                 0x0201
#define WM_LBUTTONUP                   0x0202
#define WM_LBUTTONDBLCLK               0x0203
#define WM_RBUTTONDOWN                 0x0204
#define WM_RBUTTONUP                   0x0205
#define WM_RBUTTONDBLCLK               0x0206
#define WM_MBUTTONDOWN                 0x0207
#define WM_MBUTTONUP                   0x0208
#define WM_MBUTTONDBLCLK               0x0209
#define WM_MOUSEWHEEL                  0x020A
#define WM_MOUSEHWHEEL                 0x020E
#define WM_PARENTNOTIFY                0x0210
#define WM_ENTERMENULOOP               0x0211
#define WM_EXITMENULOOP                0x0212
#define WM_NEXTMENU                    0x0213
#define WM_SIZING                      0x0214
#define WM_CAPTURECHANGED              0x0215
#define WM_MOVING                      0x0216
#define WM_POWERBROADCAST              0x0218
#define WM_DEVICECHANGE                0x0219
#define WM_MDICREATE                   0x0220
#define WM_MDIDESTROY                  0x0221
#define WM_MDIACTIVATE                 0x0222
#define WM_MDIRESTORE                  0x0223
#define WM_MDINEXT                     0x0224
#define WM_MDIMAXIMIZE                 0x0225
#define WM_MDITILE                     0x0226
#define WM_MDICASCADE                  0x0227
#define WM_MDIICONARRANGE              0x0228
#define WM_MDIGETACTIVE                0x0229
#define WM_MDISETMENU                  0x0230
#define WM_ENTERSIZEMOVE               0x0231
#define WM_EXITSIZEMOVE                0x0232
#define WM_DROPFILES                   0x0233
#define WM_MDIREFRESHMENU              0x0234
#define WM_IME_SETCONTEXT              0x0281
#define WM_IME_NOTIFY                  0x0282
#define WM_IME_CONTROL                 0x0283
#define WM_IME_COMPOSITIONFULL         0x0284
#define WM_IME_SELECT                  0x0285
#define WM_IME_CHAR                    0x0286
#define WM_IME_REQUEST                 0x0288
#define WM_IME_KEYDOWN                 0x0290
#define WM_IME_KEYUP                   0x0291
#define WM_NCMOUSEHOVER                0x02A0
#define WM_MOUSEHOVER                  0x02A1
#define WM_NCMOUSELEAVE                0x02A2
#define WM_MOUSELEAVE                  0x02A3
#define WM_WTSSESSION_CHANGE           0x02B1
#define WM_DPICHANGED                  0x02E0
#define WM_DPICHANGED_BEFOREPARENT     0x02E2
#define WM_DPICHANGED_AFTERPARENT      0x02E3
#define WM_DPI_AWARENESS_CONTEXT_UNCHANGED 0x02E4
#define WM_GETDPISCALEDSIZE            0x02E4
#define WM_CUT                         0x0300
#define WM_COPY                        0x0301
#define WM_PASTE                       0x0302
#define WM_CLEAR                       0x0303
#define WM_UNDO                        0x0304
#define WM_RENDERFORMAT                0x0305
#define WM_RENDERALLFORMATS            0x0306
#define WM_DESTROYCLIPBOARD            0x0307
#define WM_DRAWCLIPBOARD               0x0308
#define WM_PAINTCLIPBOARD              0x0309
#define WM_VSCROLLCLIPBOARD            0x030A
#define WM_SIZECLIPBOARD               0x030B
#define WM_ASKCBFORMATNAME             0x030C
#define WM_CHANGECBCHAIN               0x030D
#define WM_HSCROLLCLIPBOARD            0x030E
#define WM_QUERYNEWPALETTE             0x030F
#define WM_PALETTEISCHANGING           0x0310
#define WM_PALETTECHANGED              0x0311
#define WM_HOTKEY                      0x0312
#define WM_PRINT                       0x0317
#define WM_PRINTCLIENT                 0x0318
#define WM_APPCOMMAND                  0x0319
#define WM_HANDHELDFIRST               0x0358
#define WM_HANDHELDLAST                0x035F
#define WM_AFXFIRST                    0x0360
#define WM_AFXLAST                     0x037F
#define WM_PENWINFIRST                 0x0380
#define WM_PENWINLAST                  0x038F
#define WM_COPYGLOBALDATA              0x0049

#define WS_OVERLAPPED        0x00000000
#define WS_POPUP             0x80000000
#define WS_CHILD             0x40000000
#define WS_MINIMIZE          0x20000000
#define WS_VISIBLE           0x10000000
#define WS_DISABLED          0x08000000
#define WS_CLIPSIBLINGS      0x04000000
#define WS_CLIPCHILDREN      0x02000000
#define WS_MAXIMIZE          0x01000000
#define WS_CAPTION           0x00C00000
#define WS_BORDER            0x00800000
#define WS_DLGFRAME          0x00400000
#define WS_VSCROLL           0x00200000
#define WS_HSCROLL           0x00100000
#define WS_SYSMENU           0x00080000
#define WS_THICKFRAME        0x00040000
#define WS_GROUP             0x00020000
#define WS_TABSTOP           0x00010000
#define WS_MINIMIZEBOX       0x00020000
#define WS_MAXIMIZEBOX       0x00010000
#define WS_TILED             WS_OVERLAPPED
#define WS_ICONIC            WS_MINIMIZE
#define WS_SIZEBOX           WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW  (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW       (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW       WS_CHILD

#define WS_EX_DLGMODALFRAME     0x00000001
#define WS_EX_NOPARENTNOTIFY    0x00000004
#define WS_EX_TOPMOST           0x00000008
#define WS_EX_ACCEPTFILES       0x00000010
#define WS_EX_TRANSPARENT       0x00000020
#define WS_EX_MDICHILD          0x00000040
#define WS_EX_TOOLWINDOW        0x00000080
#define WS_EX_WINDOWEDGE        0x00000100
#define WS_EX_PALETTEWINDOW     (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)
#define WS_EX_CLIENTEDGE        0x00000200
#define WS_EX_CONTEXTHELP       0x00000400
#define WS_EX_RIGHT             0x00001000
#define WS_EX_LEFT              0x00000000
#define WS_EX_RTLREADING        0x00002000
#define WS_EX_LTRREADING        0x00000000
#define WS_EX_LEFTSCROLLBAR     0x00004000
#define WS_EX_RIGHTSCROLLBAR    0x00000000
#define WS_EX_CONTROLPARENT     0x00010000
#define WS_EX_STATICEDGE        0x00020000
#define WS_EX_APPWINDOW         0x00040000
#define WS_EX_OVERLAPPEDWINDOW  (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_LAYERED           0x00080000
#define WS_EX_NOINHERITLAYOUT   0x00100000
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000
#define WS_EX_LAYOUTRTL         0x00400000
#define WS_EX_COMPOSITED        0x02000000
#define WS_EX_NOACTIVATE        0x08000000

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb)   ((BYTE)((rgb)&0xFF))
#define GetGValue(rgb)   ((BYTE)(((rgb)>>8)&0xFF))
#define GetBValue(rgb)   ((BYTE)(((rgb)>>16)&0xFF))

#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9
#define SW_SHOWDEFAULT      10
#define SW_FORCEMINIMIZE    11

#define INFINITE            0xFFFFFFFF
#define WAIT_FAILED         0xFFFFFFFF
#define WAIT_OBJECT_0       0x00000000
#define WAIT_TIMEOUT        0x00000102
#define WAIT_ABANDONED      0x00000080

#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

#define FILE_SHARE_READ     0x00000001
#define FILE_SHARE_WRITE    0x00000002
#define FILE_SHARE_DELETE   0x00000004

#define GENERIC_READ        0x80000000
#define GENERIC_WRITE       0x40000000
#define GENERIC_EXECUTE     0x20000000
#define GENERIC_ALL         0x10000000

#define FILE_ATTRIBUTE_READONLY     0x00000001
#define FILE_ATTRIBUTE_HIDDEN       0x00000002
#define FILE_ATTRIBUTE_SYSTEM       0x00000004
#define FILE_ATTRIBUTE_DIRECTORY    0x00000010
#define FILE_ATTRIBUTE_ARCHIVE      0x00000020
#define FILE_ATTRIBUTE_NORMAL       0x00000080
#define FILE_ATTRIBUTE_TEMPORARY    0x00000100

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

#define BEGIN_OF_FILE        0
#define CURRENT_FILE_POINT   1
#define END_OF_FILE          2

#define FILE_BEGIN           BEGIN_OF_FILE
#define FILE_CURRENT         CURRENT_FILE_POINT
#define FILE_END             END_OF_FILE

#define GMEM_FIXED          0x0000
#define GMEM_MOVEABLE       0x0002
#define GMEM_ZEROINIT       0x0040
#define GHND                (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR                (GMEM_FIXED | GMEM_ZEROINIT)

#define LMEM_FIXED          0x0000
#define LMEM_MOVEABLE       0x0002
#define LMEM_ZEROINIT       0x0040
#define LHND                (LMEM_MOVEABLE | LMEM_ZEROINIT)
#define LPTR                (LMEM_FIXED | LMEM_ZEROINIT)

#define MEM_COMMIT          0x00001000
#define MEM_RESERVE         0x00002000
#define MEM_DECOMMIT        0x00004000
#define MEM_RELEASE         0x00008000
#define MEM_RESET           0x00080000
#define MEM_LARGE_PAGES     0x20000000

#define PAGE_NOACCESS       0x01
#define PAGE_READONLY       0x02
#define PAGE_READWRITE      0x04
#define PAGE_WRITECOPY      0x08
#define PAGE_EXECUTE        0x10
#define PAGE_EXECUTE_READ   0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD          0x100
#define PAGE_NOCACHE        0x200
#define PAGE_WRITECOMBINE   0x400

#define HEAP_NO_SERIALIZE   0x00000001
#define HEAP_GROWABLE       0x00000002
#define HEAP_GENERATE_EXCEPTIONS 0x00000004
#define HEAP_ZERO_MEMORY    0x00000008
#define HEAP_REALLOC_IN_PLACE_ONLY 0x00000010

#define OPAQUE              2
#define TRANSPARENT         1

#define MM_TEXT             1
#define MM_LOMETRIC         2
#define MM_HIMETRIC         3
#define MM_LOENGLISH        4
#define MM_HIENGLISH        5
#define MM_TWIPS            6
#define MM_ISOTROPIC        7
#define MM_ANISOTROPIC      8

#define SRCCOPY             0x00CC0020
#define SRCPAINT            0x00EE0086
#define SRCAND              0x008800C6
#define SRCINVERT           0x00660046
#define SRCERASE            0x00440328
#define NOTSRCCOPY          0x00330008
#define NOTSRCERASE         0x001100A6
#define MERGECOPY           0x00C000CA
#define MERGEPAINT          0x00BB0226
#define PATCOPY             0x00F00021
#define PATPAINT            0x00FB0A09
#define PATINVERT           0x005A0049
#define DSTINVERT           0x00550009
#define BLACKNESS           0x00000042
#define WHITENESS           0x00FF0062

#define PS_SOLID            0
#define PS_DASH             1
#define PS_DOT              2
#define PS_DASHDOT          3
#define PS_DASHDOTDOT       4
#define PS_NULL             5
#define PS_INSIDEFRAME      6

#define DT_TOP              0x00000000
#define DT_LEFT             0x00000000
#define DT_CENTER           0x00000001
#define DT_RIGHT            0x00000002
#define DT_VCENTER          0x00000004
#define DT_BOTTOM           0x00000008
#define DT_WORDBREAK        0x00000010
#define DT_SINGLELINE       0x00000020
#define DT_EXPANDTABS       0x00000040
#define DT_TABSTOP          0x00000080
#define DT_NOCLIP           0x00000100
#define DT_EXTERNALLEADING  0x00000200
#define DT_CALCRECT         0x00000400
#define DT_NOPREFIX         0x00000800
#define DT_INTERNAL         0x00001000
#define DT_EDITCONTROL      0x00002000
#define DT_PATH_ELLIPSIS    0x00004000
#define DT_END_ELLIPSIS     0x00008000
#define DT_MODIFYSTRING     0x00010000
#define DT_HIDEPREFIX       0x00100000
#define DT_PREFIXONLY       0x00200000

#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_NOCOPYBITS      0x0100
#define SWP_NOOWNERZORDER   0x0200
#define SWP_NOSENDCHANGING  0x0400
#define SWP_DRAWFRAME       SWP_FRAMECHANGED
#define SWP_NOREPOSITION    SWP_NOOWNERZORDER
#define SWP_DEFERERASE      0x2000
#define SWP_ASYNCWINDOWPOS  0x4000

#define HWND_TOP        ((HWND)0)
#define HWND_BOTTOM     ((HWND)1)
#define HWND_TOPMOST    ((HWND)-1)
#define HWND_NOTOPMOST  ((HWND)-2)

#define GW_HWNDFIRST        0
#define GW_HWNDLAST         1
#define GW_HWNDNEXT         2
#define GW_HWNDPREV         3
#define GW_OWNER            4
#define GW_CHILD            5
#define GW_ENABLEDPOPUP     6

#define SM_CXSCREEN         0
#define SM_CYSCREEN         1
#define SM_CXVSCROLL        2
#define SM_CYHSCROLL        3
#define SM_CYCAPTION        4
#define SM_CXBORDER         5
#define SM_CYBORDER         6
#define SM_CXDLGFRAME       7
#define SM_CYDLGFRAME       8
#define SM_CYVTHUMB         9
#define SM_CXHTHUMB         10
#define SM_CXICON           11
#define SM_CYICON           12
#define SM_CXCURSOR         13
#define SM_CYCURSOR         14
#define SM_CYMENU           15
#define SM_CXFULLSCREEN     16
#define SM_CYFULLSCREEN     17
#define SM_CYKANJIWINDOW    18
#define SM_MOUSEPRESENT     19
#define SM_CYVSCROLL        20
#define SM_CXHSCROLL        21
#define SM_DEBUG            22
#define SM_SWAPBUTTON       23
#define SM_CXMIN            28
#define SM_CYMIN            29
#define SM_CXSIZE           30
#define SM_CYSIZE           31
#define SM_CXFRAME          32
#define SM_CYFRAME          33
#define SM_CXMINTRACK       34
#define SM_CYMINTRACK       35
#define SM_CXDOUBLECLK      36
#define SM_CYDOUBLECLK      37
#define SM_CXICONSPACING    38
#define SM_CYICONSPACING    39
#define SM_MENUDROPALIGNMENT 40
#define SM_PENWINDOWS       41
#define SM_DBCSENABLED      42
#define SM_CMOUSEBUTTONS    43
#define SM_CXFIXEDFRAME     SM_CXDLGFRAME
#define SM_CYFIXEDFRAME     SM_CYDLGFRAME
#define SM_CXSIZEFRAME      SM_CXFRAME
#define SM_CYSIZEFRAME      SM_CYFRAME
#define SM_SECURE           44
#define SM_CXEDGE           45
#define SM_CYEDGE           46
#define SM_CXMINSPACING     47
#define SM_CYMINSPACING     48
#define SM_CXSMICON         49
#define SM_CYSMICON         50
#define SM_CYSMCAPTION      51
#define SM_CXSMSIZE         52
#define SM_CYSMSIZE         53
#define SM_CXMENUSIZE       54
#define SM_CYMENUSIZE       55
#define SM_ARRANGE          56
#define SM_CXMINIMIZED      57
#define SM_CYMINIMIZED      58
#define SM_CXMAXTRACK       59
#define SM_CYMAXTRACK       60
#define SM_CXMAXIMIZED      61
#define SM_CYMAXIMIZED      62
#define SM_NETWORK          63
#define SM_CLEANBOOT        67
#define SM_CXDRAG           68
#define SM_CYDRAG           69
#define SM_CMONITORS        80
#define SM_CXTHUMB          SM_CXHTHUMB

#define MAX_PATH            260

#define KEY_RETURN          0x0D

#define COLOR_SCROLLBAR         0
#define COLOR_BACKGROUND        1
#define COLOR_ACTIVECAPTION     2
#define COLOR_INACTIVECAPTION   3
#define COLOR_MENU              4
#define COLOR_WINDOW            5
#define COLOR_WINDOWFRAME       6
#define COLOR_MENUTEXT          7
#define COLOR_WINDOWTEXT        8
#define COLOR_CAPTIONTEXT       9
#define COLOR_ACTIVEBORDER      10
#define COLOR_INACTIVEBORDER    11
#define COLOR_APPWORKSPACE      12
#define COLOR_HIGHLIGHT         13
#define COLOR_HIGHLIGHTTEXT     14
#define COLOR_BTNFACE           15
#define COLOR_BTNSHADOW         16
#define COLOR_GRAYTEXT          17
#define COLOR_BTNTEXT           18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT      20
#define COLOR_3DDKSHADOW        21
#define COLOR_3DLIGHT           22
#define COLOR_INFOTEXT          23
#define COLOR_INFOBK            24

#define FW_DONTCARE         0
#define FW_THIN             100
#define FW_EXTRALIGHT       200
#define FW_LIGHT            300
#define FW_NORMAL           400
#define FW_MEDIUM           500
#define FW_SEMIBOLD         600
#define FW_BOLD             700
#define FW_EXTRABOLD        800
#define FW_HEAVY            900

#define ANSI_CHARSET        0
#define DEFAULT_CHARSET     1
#define SYMBOL_CHARSET      2
#define SHIFTJIS_CHARSET    128
#define HANGEUL_CHARSET     129
#define GB2312_CHARSET      134
#define CHINESEBIG5_CHARSET 136
#define OEM_CHARSET         255

#define OUT_DEFAULT_PRECIS      0
#define OUT_STRING_PRECIS       1
#define OUT_CHARACTER_PRECIS    2
#define OUT_STROKE_PRECIS       3
#define OUT_TT_PRECIS           4
#define OUT_DEVICE_PRECIS       5
#define OUT_RASTER_PRECIS       6
#define OUT_TT_ONLY_PRECIS      7
#define OUT_OUTLINE_PRECIS      8
#define OUT_SCREEN_OUTLINE_PRECIS 9
#define OUT_PS_ONLY_PRECIS      10

#define CLIP_DEFAULT_PRECIS     0
#define CLIP_CHARACTER_PRECIS   1
#define CLIP_STROKE_PRECIS      2
#define CLIP_MASK               0xF
#define CLIP_LH_ANGLES          (1<<4)
#define CLIP_TT_ALWAYS          (2<<4)
#define CLIP_EMBEDDED           (8<<4)

#define DEFAULT_QUALITY         0
#define DRAFT_QUALITY           1
#define PROOF_QUALITY           2
#define NONANTIALIASED_QUALITY  3
#define ANTIALIASED_QUALITY     4
#define CLEARTYPE_QUALITY       5

#define DEFAULT_PITCH           0
#define FIXED_PITCH             1
#define VARIABLE_PITCH          2
#define FF_DONTCARE             (0<<4)
#define FF_ROMAN                (1<<4)
#define FF_SWISS                (2<<4)
#define FF_MODERN               (3<<4)
#define FF_SCRIPT               (4<<4)
#define FF_DECORATIVE           (5<<4)

#define BDR_RAISEDOUTER     0x0001
#define BDR_SUNKENOUTER     0x0002
#define BDR_RAISEDINNER     0x0004
#define BDR_SUNKENINNER     0x0008
#define BDR_OUTER           (BDR_RAISEDOUTER | BDR_SUNKENOUTER)
#define BDR_INNER           (BDR_RAISEDINNER | BDR_SUNKENINNER)
#define BDR_RAISED          (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define BDR_SUNKEN          (BDR_SUNKENOUTER | BDR_SUNKENINNER)

#define EDGE_RAISED         (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN         (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED         (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP           (BDR_RAISEDOUTER | BDR_SUNKENINNER)

#define BF_LEFT             0x0001
#define BF_TOP              0x0002
#define BF_RIGHT            0x0004
#define BF_BOTTOM           0x0008
#define BF_TOPLEFT          (BF_TOP | BF_LEFT)
#define BF_TOPRIGHT         (BF_TOP | BF_RIGHT)
#define BF_BOTTOMLEFT       (BF_BOTTOM | BF_LEFT)
#define BF_BOTTOMRIGHT      (BF_BOTTOM | BF_RIGHT)
#define BF_RECT             (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)

#define DFC_BUTTON          0x0004
#define DFC_POPUPMENU       0x0005
#define DFC_CAPTION         0x0001
#define DFC_MENU            0x0002
#define DFC_SCROLLBAR       0x0003

#define DFCS_BUTTONPUSH     0x0001
#define DFCS_BUTTONCHECK    0x0002
#define DFCS_BUTTONRADIO    0x0004
#define DFCS_BUTTON3STATE   0x0008
#define DFCS_BUTTONRADIOIMAGE 0x0001
#define DFCS_BUTTONRADIOMASK 0x0002
#define DFCS_CAPTIONCLOSE   0x0001
#define DFCS_CAPTIONMIN     0x0002
#define DFCS_CAPTIONMAX     0x0004
#define DFCS_CAPTIONRESTORE 0x0008
#define DFCS_CAPTIONHELP    0x0010
#define DFCS_MENUARROW      0x0001
#define DFCS_MENUCHECK      0x0002
#define DFCS_MENUBULLET     0x0004
#define DFCS_MENUARROWRIGHT 0x0008
#define DFCS_UP             0x0001
#define DFCS_DOWN           0x0002
#define DFCS_UPLEFT         0x0003
#define DFCS_UPRIGHT        0x0004
#define DFCS_LEFT           0x0005
#define DFCS_RIGHT          0x0006
#define DFCS_DOWNLEFT       0x0007
#define DFCS_DOWNRIGHT      0x0008
#define DFCS_FLAT           0x0001
#define DFCS_MONO           0x0002
#define DFCS_HOT            0x1000
#define DFCS_ADJUSTRECT     0x2000
#define DFCS_PUSHED         0x0100
#define DFCS_CHECKED        0x0400
#define DFCS_TRANSPARENT    0x0800

#define DSS_NORMAL          0x0000
#define DSS_UNION           0x0010
#define DSS_DISABLED        0x0020
#define DSS_MONO            0x0080
#define DSS_RIGHT           0x0080

#define PM_NOREMOVE         0x0000
#define PM_REMOVE           0x0001
#define PM_NOYIELD          0x0002

#define SMTO_NORMAL         0x0000
#define SMTO_BLOCK          0x0001
#define SMTO_ABORTIFHUNG    0x0002
#define SMTO_NOTIMEOUTIFNOTHUNG 0x0008

#define GWLP_WNDPROC        (-4)
#define GWLP_HINSTANCE      (-6)
#define GWLP_HWNDPARENT     (-8)
#define GWLP_USERDATA       (-21)
#define GWLP_ID             (-12)
#define GWL_WNDPROC         GWLP_WNDPROC
#define GWL_HINSTANCE       GWLP_HINSTANCE
#define GWL_HWNDPARENT      GWLP_HWNDPARENT
#define GWL_USERDATA        GWLP_USERDATA
#define GWL_ID              GWLP_ID
#define GWL_STYLE           (-16)
#define GWL_EXSTYLE         (-20)

#define ERROR_SUCCESS               0L
#define ERROR_FILE_NOT_FOUND        2L
#define ERROR_PATH_NOT_FOUND        3L
#define ERROR_ACCESS_DENIED         5L
#define ERROR_INVALID_HANDLE        6L
#define ERROR_NOT_ENOUGH_MEMORY     8L
#define ERROR_INVALID_DATA          13L
#define ERROR_DRIVE_LOCKED          108L
#define ERROR_BROKEN_PIPE           109L
#define ERROR_DISK_FULL             112L
#define ERROR_INVALID_NAME          123L
#define ERROR_INVALID_PARAMETER     87L
#define ERROR_NOT_FOUND             1168L
#define ERROR_CALL_NOT_IMPLEMENTED  120L
#define ERROR_INSUFFICIENT_BUFFER   122L
#define ERROR_FAILED_SERVICE_CONTROLLER_CONNECT 1063L
#define ERROR_INVALID_FLAGS         1004L
#define ERROR_NO_DATA               232L
#define ERROR_NO_MORE_ITEMS         259L
#define ERROR_PROC_NOT_FOUND        127L
#define ERROR_ENVVAR_NOT_FOUND      203L
#define ERROR_BUFFER_OVERFLOW       111L
#define ERROR_NEGATIVE_SEEK         131L

#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_REMOTE      4
#define DRIVE_CDROM       5
#define DRIVE_RAMDISK     6

#define PF_FLOATING_POINT_PRECISION_ERRATA 0
#define PF_FLOATING_POINT_EMULATED        1
#define PF_COMPARE_EXCHANGE_DOUBLE        2
#define PF_MMX_INSTRUCTIONS_AVAILABLE     3
#define PF_XMMI_INSTRUCTIONS_AVAILABLE    6
#define PF_3DNOW_INSTRUCTIONS_AVAILABLE   7
#define PF_RDTSC_INSTRUCTION_AVAILABLE    8
#define PF_PAE_ENABLED                   9
#define PF_XMMI64_INSTRUCTIONS_AVAILABLE 10
#define PF_SSE3_INSTRUCTIONS_AVAILABLE   13
#define PF_COMPARE_EXCHANGE128          14
#define PF_COMPARE64EXCHANGE128         15
#define PF_CHANNELS_ENABLED             16
#define PF_XSAVE_ENABLED                17
#define PF_VIRT_FIRMWARE_ENABLED        18
#define PF_RDWRFSGSBASE_AVAILABLE       19
#define PF_FASTFAIL_AVAILABLE            20
#define PF_ARM_VFP_32_REGISTERS_AVAILABLE 21
#define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE 22
#define PF_SECOND_LEVEL_ADDRESS_TRANSLATION 23
#define PF_VIRT_FIRMWARE_ENFORCED       24
#define PF_RDSEED_INSTRUCTION_AVAILABLE 25
#define PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE 26
#define PF_ARM_64BIT_LOADSTORE_ATOMIC   27
#define PF_ARM_EXTERNAL_CACHE_AVAILABLE 28
#define PF_ARM_FPCRT_INSTANCES          29
#define PF_SSSE3_INSTRUCTIONS_AVAILABLE 30
#define PF_SSE41_INSTRUCTIONS_AVAILABLE 31
#define PF_SSE42_INSTRUCTIONS_AVAILABLE 32
#define PF_AVX_INSTRUCTIONS_AVAILABLE    33
#define PF_AVX2_AVAILABLE                34
#define PF_AVX512F_INSTRUCTIONS_AVAILABLE 35
#define PF_ARM_DMB_INSTRUCTIONS_AVAILABLE 36

typedef BOOL (*LOCALE_ENUMPROC)(LPSTR);
#define CSTR_LESS_THAN    1
#define CSTR_EQUAL        2
#define CSTR_GREATER_THAN 3

#define C1_UPPER          0x0001
#define C1_LOWER          0x0002
#define C1_DIGIT          0x0004
#define C1_SPACE          0x0008
#define C1_PUNCT          0x0010
#define C1_CNTRL          0x0020
#define C1_ALPHA          0x0080

#define COORD struct { SHORT X; SHORT Y; }
#define SMALL_RECT struct { SHORT Left; SHORT Top; SHORT Right; SHORT Bottom; }
#define CHAR_INFO union { CHAR UnicodeChar; CHAR AsciiChar; WORD Attributes; }

BOOL LockFile(HANDLE hFile, uint32_t dwFileOffsetLow,
              uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToLockLow,
              uint32_t nNumberOfBytesToLockHigh);
BOOL UnlockFile(HANDLE hFile, uint32_t dwFileOffsetLow,
                uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToUnlockLow,
                uint32_t nNumberOfBytesToUnlockHigh);
BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, uint32_t dwReserved,
                uint32_t nNumberOfBytesToLockLow, uint32_t nNumberOfBytesToLockHigh,
                void* lpOverlapped);
BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved,
                  uint32_t nNumberOfBytesToUnlockLow, uint32_t nNumberOfBytesToUnlockHigh,
                  void* lpOverlapped);
DWORD GetLogicalDriveStringsA(uint32_t nBufferLength, char* lpBuffer);
UINT GetDriveTypeA(const char* lpRootPathName);
BOOL GetDiskFreeSpaceA(const char* lpRootPathName, uint32_t* lpSectorsPerCluster,
                       uint32_t* lpBytesPerSector, uint32_t* lpNumberOfFreeClusters,
                       uint32_t* lpTotalNumberOfClusters);
BOOL GetDiskFreeSpaceExA(const char* lpDirectoryName,
                         uint64_t* lpFreeBytesAvailableToCaller,
                         uint64_t* lpTotalNumberOfBytes,
                         uint64_t* lpTotalNumberOfFreeBytes);
BOOL GetFileTime(HANDLE hFile, int64_t* lpCreationTime,
                 int64_t* lpLastAccessTime, int64_t* lpLastWriteTime);
BOOL SetFileTime(HANDLE hFile, const int64_t* lpCreationTime,
                 const int64_t* lpLastAccessTime,
                 const int64_t* lpLastWriteTime);
BOOL GetFileAttributesExA(const char* lpFileName, uint32_t fInfoLevelId,
                          void* lpFileInformation);
BOOL SetFileAttributesA(const char* lpFileName, uint32_t dwFileAttributes);
BOOL GetFullPathNameA(const char* lpFileName, uint32_t nBufferLength,
                      char* lpBuffer, char** lpFilePart);
BOOL GetShortPathNameA(const char* lpszLongPath, char* lpszShortPath,
                       uint32_t cchBuffer);
BOOL GetLongPathNameA(const char* lpszShortPath, char* lpszLongPath,
                      uint32_t cchBuffer);
DWORD SearchPathA(const char* lpPath, const char* lpFileName,
                  const char* lpExtension, uint32_t nBufferLength,
                  char* lpBuffer, char** lpFilePart);
HANDLE CreateFileMappingA(HANDLE hFile, void* lpFileMappingAttributes,
                          uint32_t flProtect, uint32_t dwMaximumSizeHigh,
                          uint32_t dwMaximumSizeLow, const char* lpName);
LPVOID MapViewOfFile(HANDLE hFileMappingObject, uint32_t dwDesiredAccess,
                     uint32_t dwFileOffsetHigh, uint32_t dwFileOffsetLow,
                     uint64_t dwNumberOfBytesToMap);
BOOL UnmapViewOfFile(LPCVOID lpBaseAddress);
BOOL QueryPerformanceCounter(int64_t* lpPerformanceCount);
BOOL QueryPerformanceFrequency(int64_t* lpFrequency);
void SleepEx(uint32_t dwMilliseconds, BOOL bAlertable);
BOOL IsProcessorFeaturePresent(uint32_t ProcessorFeature);
BOOL GetSystemPowerStatus(void* lpSystemPowerStatus);
int GetSystemDefaultLangID(void);
int GetUserDefaultLangID(void);
LCID GetSystemDefaultLCID(void);
LCID GetUserDefaultLCID(void);
BOOL IsValidLocale(uint32_t Locale, uint32_t dwFlags);
BOOL EnumSystemLocalesA(LOCALE_ENUMPROC lpLocaleEnumProc, uint32_t dwFlags);
int CompareStringA(uint32_t Locale, uint32_t dwCmpFlags,
                   const char* lpString1, int cchCount1,
                   const char* lpString2, int cchCount2);
int LCMapStringA(uint32_t Locale, uint32_t dwMapFlags,
                 const char* lpSrcStr, int cchSrc,
                 char* lpDestStr, int cchDest);
int GetStringTypeA(uint32_t Locale, uint32_t dwInfoType,
                   const char* lpSrcStr, int cchSrc,
                   uint16_t* lpCharType);
BOOL ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress,
                       LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
BOOL WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress,
                        LPCVOID lpBuffer, SIZE_T nSize,
                        SIZE_T* lpNumberOfBytesWritten);
HMODULE GetModuleHandleW(const wchar_t* lpModuleName);
DWORD GetModuleFileNameW(HMODULE hModule, wchar_t* lpFilename, DWORD nSize);
HMODULE LoadLibraryW(const wchar_t* lpLibFileName);
HMODULE LoadLibraryExA(const char* lpLibFileName, HANDLE hFile, uint32_t dwFlags);
HMODULE LoadLibraryExW(const wchar_t* lpLibFileName, HANDLE hFile, uint32_t dwFlags);
BOOL FreeLibraryAndExitThread(HMODULE hLibModule, uint32_t dwExitCode);
DWORD GetDllDirectoryA(uint32_t nBufferLength, char* lpBuffer);
BOOL SetDllDirectoryA(const char* lpPathName);
SIZE_T VirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress,
                      void* lpBuffer, SIZE_T dwLength);
LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, uint64_t dwBytes);
SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
BOOL HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
uint32_t HeapCompact(HANDLE hHeap, uint32_t dwFlags);
BOOL HeapSetInformation(HANDLE hHeap, uint32_t HeapInformationClass,
                         void* HeapInformation, SIZE_T HeapInformationLength);
BOOL HeapQueryInformation(HANDLE hHeap, uint32_t HeapInformationClass,
                           void* HeapInformation, SIZE_T HeapInformationLength,
                           SIZE_T* ReturnLength);
BOOL InitializeCriticalSection(void* lpCriticalSection);
void EnterCriticalSection(void* lpCriticalSection);
BOOL TryEnterCriticalSection(void* lpCriticalSection);
void LeaveCriticalSection(void* lpCriticalSection);
void DeleteCriticalSection(void* lpCriticalSection);
BOOL InitializeCriticalSectionAndSpinCount(void* lpCriticalSection, uint32_t dwSpinCount);
BOOL InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comparand);
LONG InterlockedIncrement(volatile LONG* Addend);
LONG InterlockedDecrement(volatile LONG* Addend);
LONG InterlockedExchange(volatile LONG* Target, LONG Value);
LONG InterlockedExchangeAdd(volatile LONG* Addend, LONG Value);
PVOID InterlockedCompareExchangePointer(volatile PVOID* Destination,
                                        PVOID Exchange, PVOID Comparand);
PVOID InterlockedExchangePointer(volatile PVOID* Target, PVOID Value);
void OutputDebugStringA(const char* lpOutputString);
void DebugBreak(void);
BOOL IsDebuggerPresent(void);
void FatalExit(int ExitCode);
void RaiseException(uint32_t dwExceptionCode, uint32_t dwExceptionFlags,
                    uint32_t nNumberOfArguments, const uint32_t* lpArguments);
uint32_t SetErrorMode(uint32_t uMode);
BOOL Beep(uint32_t dwFreq, uint32_t dwDuration);
BOOL GenerateConsoleCtrlEvent(uint32_t dwCtrlEvent, uint32_t dwProcessGroupId);
BOOL AllocConsole(void);
BOOL FreeConsole(void);
HWND GetConsoleWindow(void);
BOOL SetConsoleTitleA(const char* lpConsoleTitle);
DWORD GetConsoleTitleA(char* lpConsoleTitle, uint32_t nSize);
BOOL SetConsoleTextAttribute(HANDLE hConsoleOutput, uint16_t wAttributes);
BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput, void* lpConsoleScreenBufferInfo);
BOOL SetConsoleCursorPosition(HANDLE hConsoleOutput, COORD dwCursorPosition);
BOOL SetConsoleCursorInfo(HANDLE hConsoleOutput, const void* lpConsoleCursorInfo);
BOOL FillConsoleOutputCharacterA(HANDLE hConsoleOutput, CHAR cCharacter,
                                 DWORD nLength, COORD dwWriteCoord,
                                 DWORD* lpNumberOfCharsWritten);
BOOL FillConsoleOutputAttribute(HANDLE hConsoleOutput, WORD wAttribute,
                                DWORD nLength, COORD dwWriteCoord,
                                DWORD* lpNumberOfAttrsWritten);
BOOL ScrollConsoleScreenBufferA(HANDLE hConsoleOutput, const SMALL_RECT* lpScrollRectangle,
                               const SMALL_RECT* lpClipRectangle, COORD dwDestinationOrigin,
                               const CHAR_INFO* lpFill);
BOOL WriteConsoleA(HANDLE hConsoleOutput, const void* lpBuffer,
                   DWORD nNumberOfCharsToWrite, DWORD* lpNumberOfCharsWritten,
                   void* lpReserved);
BOOL ReadConsoleA(HANDLE hConsoleInput, void* lpBuffer,
                  DWORD nNumberOfCharsToRead, DWORD* lpNumberOfCharsRead,
                  void* lpReserved);

#define HKEY_CLASSES_ROOT       ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER       ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE      ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS              ((HKEY)(uintptr_t)0x80000003)
#define HKEY_PERFORMANCE_DATA   ((HKEY)(uintptr_t)0x80000004)
#define HKEY_CURRENT_CONFIG     ((HKEY)(uintptr_t)0x80000005)
#define HKEY_DYN_DATA           ((HKEY)(uintptr_t)0x80000006)

#define KEY_QUERY_VALUE         0x0001
#define KEY_SET_VALUE           0x0002
#define KEY_CREATE_SUB_KEY      0x0004
#define KEY_ENUMERATE_SUB_KEYS  0x0008
#define KEY_NOTIFY              0x0010
#define KEY_CREATE_LINK         0x0020
#define KEY_WOW64_32KEY         0x0200
#define KEY_WOW64_64KEY         0x0100
#define KEY_WOW64_RES           0x0300
#define KEY_READ                ((0x00020000L | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY) & (~0x00100000L))
#define KEY_WRITE               ((0x00020000L | KEY_SET_VALUE | KEY_CREATE_SUB_KEY) & (~0x00100000L))
#define KEY_EXECUTE             ((KEY_READ) & (~0x00100000L))
#define KEY_ALL_ACCESS          ((0x000F0000L | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | KEY_CREATE_LINK) & (~0x00100000L))

#define REG_NONE                    0
#define REG_SZ                      1
#define REG_EXPAND_SZ               2
#define REG_BINARY                  3
#define REG_DWORD                   4
#define REG_DWORD_BIG_ENDIAN        5
#define REG_LINK                    6
#define REG_MULTI_SZ                7
#define REG_RESOURCE_LIST           8
#define REG_FULL_RESOURCE_DESCRIPTOR 9
#define REG_RESOURCE_REQUIREMENTS_LIST 10
#define REG_QWORD                   11

#define REG_OPTION_RESERVED         0x00000000
#define REG_OPTION_NON_VOLATILE     0x00000000
#define REG_OPTION_VOLATILE         0x00000001
#define REG_OPTION_CREATE_LINK      0x00000002
#define REG_OPTION_BACKUP_RESTORE   0x00000004
#define REG_OPTION_OPEN_LINK        0x00000008

#define REG_CREATED_NEW_KEY         0x00000001
#define REG_OPENED_EXISTING_KEY     0x00000002

typedef uint32_t (*THREAD_START_ROUTINE)(void* lpThreadParameter);
typedef THREAD_START_ROUTINE* LPTHREAD_START_ROUTINE;
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (*WNDENUMPROC)(HWND, LPARAM);

uint32_t GetTickCount(void);
uint64_t GetTickCount64(void);
void Sleep(uint32_t dwMilliseconds);
BOOL GetSystemTimeAsFileTime(uint64_t* lpSystemTimeAsFileTime);
HANDLE CreateThread(void* lpThreadAttributes, uint64_t dwStackSize, uint32_t (*lpStartAddress)(void*), void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId);
BOOL TerminateThread(HANDLE hThread, uint32_t dwExitCode);
BOOL CloseHandle(HANDLE hObject);
uint32_t WaitForSingleObject(HANDLE hHandle, uint32_t dwMilliseconds);
uint32_t GetCurrentProcessId(void);
uint32_t GetCurrentThreadId(void);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
BOOL SetProcessPriorityBoost(HANDLE hProcess, BOOL bDisablePriorityBoost);
BOOL GetExitCodeProcess(HANDLE hProcess, uint32_t* lpExitCode);

HANDLE CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode, void* lpSecurityAttributes, uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL ReadFile(HANDLE hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead, uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
BOOL WriteFile(HANDLE hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
BOOL SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, int32_t* lpDistanceToMoveHigh, uint32_t dwMoveMethod);
BOOL SetEndOfFile(HANDLE hFile);
BOOL FlushFileBuffers(HANDLE hFile);
HANDLE FindFirstFileA(const char* lpFileName, void* lpFindFileData);
BOOL FindNextFileA(HANDLE hFindFile, void* lpFindFileData);
BOOL FindClose(HANDLE hFindFile);
BOOL CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes);
BOOL RemoveDirectoryA(const char* lpPathName);
BOOL DeleteFileA(const char* lpFileName);
BOOL MoveFileA(const char* lpExistingFileName, const char* lpNewFileName);
BOOL CopyFileA(const char* lpExistingFileName, const char* lpNewFileName, BOOL bFailIfExists);
uint32_t GetFileAttributesA(const char* lpFileName);
BOOL GetFileSizeEx(HANDLE hFile, int64_t* lpFileSize);
uint32_t GetLastError(void);
void SetLastError(uint32_t dwErrCode);

HWND CreateWindowExA(uint32_t dwExStyle, const char* lpClassName, const char* lpWindowName, uint32_t dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, void* lpParam);
HWND CreateWindowA(const char* lpClassName, const char* lpWindowName, uint32_t dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, void* lpParam);
BOOL DestroyWindow(HWND hWnd);
BOOL ShowWindow(HWND hWnd, int nCmdShow);
BOOL UpdateWindow(HWND hWnd);
BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL ValidateRect(HWND hWnd, const RECT* lpRect);
BOOL GetWindowRect(HWND hWnd, RECT* lpRect);
BOOL GetClientRect(HWND hWnd, RECT* lpRect);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, uint32_t uFlags);
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
BOOL IsWindowVisible(HWND hWnd);
BOOL IsWindow(HWND hWnd);
BOOL IsWindowEnabled(HWND hWnd);
HWND GetParent(HWND hWnd);
HWND GetWindow(HWND hWnd, uint32_t uCmd);
HWND GetDesktopWindow(void);
HWND GetForegroundWindow(void);
BOOL SetForegroundWindow(HWND hWnd);
HWND SetActiveWindow(HWND hWnd);
BOOL SetFocus(HWND hWnd);
HWND GetFocus(void);
HWND FindWindowA(const char* lpClassName, const char* lpWindowName);
HWND FindWindowExA(HWND hWndParent, HWND hWndChildAfter, const char* lpszClass, const char* lpszWindow);
BOOL SetWindowTextA(HWND hWnd, const char* lpString);
int GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount);
int GetWindowTextLengthA(HWND hWnd);
BOOL SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
LONG_PTR GetWindowLongPtrA(HWND hWnd, int nIndex);
BOOL EnumWindows(BOOL (*lpEnumFunc)(HWND, LPARAM), LPARAM lParam);
BOOL EnumChildWindows(HWND hWndParent, BOOL (*lpEnumFunc)(HWND, LPARAM), LPARAM lParam);
BOOL GetWindowInfo(HWND hWnd, void* pwi);
BOOL SetWindowCompositionAttribute(HWND hwnd, void* data);
HINSTANCE GetWindowInstance(HWND hWnd);
HWND GetTopWindow(HWND hWnd);
HWND GetLastActivePopup(HWND hWnd);
BOOL AnyPopup(void);
BOOL BringWindowToTop(HWND hWnd);
HWND SetTopLevel(HWND hWnd, DWORD fNoTopLevel);
BOOL ShowOwnedPopups(HWND hWnd, BOOL fShow);
BOOL FlashWindow(HWND hWnd, BOOL bInvert);

HDC BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
int GetSystemMetrics(int nIndex);
HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);
HDC GetWindowDC(HWND hWnd);
int GetDpiForWindow(HWND hwnd);
int GetDpiForSystem(void);
HBRUSH GetSysColorBrush(int nIndex);
DWORD GetSysColor(int nIndex);
BOOL SetSysColors(int cElements, const int* lpaElements, const COLORREF* lpaRgbValues);
COLORREF GetPixel(HDC hdc, int X, int Y);
COLORREF SetPixel(HDC hdc, int X, int Y, COLORREF crColor);
BOOL LineTo(HDC hdc, int X, int Y);
BOOL MoveToEx(HDC hdc, int X, int Y, POINT* lpPoint);
BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom);
BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int width, int height);
BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom);
BOOL Polygon(HDC hdc, const POINT* lpPoints, int iCount);
BOOL Polyline(HDC hdc, const POINT* lppt, int cPoints);
HBRUSH CreateSolidBrush(COLORREF color);
HPEN CreatePen(int iStyle, int cWidth, COLORREF color);
HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily, const char* pszFaceName);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ hgdiobj);
BOOL DeleteObject(HGDIOBJ ho);
COLORREF SetBkColor(HDC hdc, COLORREF color);
COLORREF SetTextColor(HDC hdc, COLORREF crColor);
int SetBkMode(HDC hdc, int mode);
int SetMapMode(HDC hdc, int fnMapMode);
BOOL SetViewportOrgEx(HDC hdc, int X, int Y, POINT* lpPoint);
BOOL SetWindowOrgEx(HDC hdc, int X, int Y, POINT* lpPoint);
BOOL SetViewportExtEx(HDC hdc, int xExt, int yExt, SIZE* lpSize);
BOOL SetWindowExtEx(HDC hdc, int xExt, int yExt, SIZE* lpSize);
BOOL FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr);
BOOL FrameRect(HDC hdc, const RECT* lprc, HBRUSH hbr);
BOOL InvertRect(HDC hdc, const RECT* lprc);
int DrawTextA(HDC hdc, const char* lpchText, int cchText, RECT* lprc, UINT format);
BOOL TextOutA(HDC hdc, int X, int Y, const char* lpString, int c);
BOOL PatBlt(HDC hdc, int X, int Y, int W, int H, DWORD rop);
BOOL BitBlt(HDC hdc, int X, int Y, int W, int H, HDC hdcSrc, int XSrc, int YSrc, DWORD dwRop);
BOOL StretchBlt(HDC hdc, int XDest, int YDest, int WDest, int HDest, HDC hdcSrc, int XSrc, int YSrc, int WSrc, int HSrc, DWORD dwRop);
BOOL ExtTextOutA(HDC hdc, int X, int Y, UINT fuOptions, const RECT* lprc, const char* lpString, UINT cbCount, const int* lpDx);
BOOL DrawEdge(HDC hdc, RECT* qrc, UINT edge, UINT grfFlags);
BOOL DrawFrameControl(HDC hdc, RECT* lpRect, UINT uType, UINT uState);
BOOL DrawFocusRect(HDC hdc, const RECT* lprc);
BOOL DrawStateA(HDC hdc, HBRUSH hbr, void* lpOutputFunc, LPARAM lData, WPARAM wData, int x, int y, int cx, int cy, UINT fuFlags);

BOOL GetMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL PeekMessageA(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL TranslateMessage(const MSG* lpMsg);
LRESULT DispatchMessageA(const MSG* lpMsg);
void PostQuitMessage(int nExitCode);
BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostThreadMessageA(uint32_t idThread, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageTimeoutA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
BOOL WaitMessage(void);
BOOL ReplyMessage(LRESULT lResult);
BOOL SetMessageQueue(uint32_t cMessagesMax);
BOOL AttachThreadInput(uint32_t idAttach, uint32_t idAttachTo, BOOL fAttach);
BOOL GetInputState(void);
BOOL GetQueueStatus(UINT flags);
BOOL WaitForInputIdle(HANDLE hProcess, uint32_t dwMilliseconds);
UINT RegisterWindowMessageA(LPCSTR lpString);

HMODULE GetModuleHandleA(const char* lpModuleName);
HMODULE LoadLibraryA(const char* lpLibFileName);
BOOL FreeLibrary(HMODULE hLibModule);
FARPROC GetProcAddress(HMODULE hModule, const char* lpProcName);
uint32_t GetModuleFileNameA(HMODULE hModule, char* lpFilename, uint32_t nSize);

void GetSystemInfo(void* lpSystemInfo);
void GetNativeSystemInfo(void* lpSystemInfo);
DWORD GetVersion(void);
BOOL GetVersionExA(void* lpVersionInformation);
BOOL VerifyVersionInfoA(void* lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask);
UINT GetSystemDirectoryA(char* lpBuffer, UINT uSize);
UINT GetWindowsDirectoryA(char* lpBuffer, UINT uSize);
UINT GetTempPathA(uint32_t nBufferLength, char* lpBuffer);
DWORD GetEnvironmentVariableA(const char* lpName, char* lpBuffer, DWORD nSize);
BOOL SetEnvironmentVariableA(const char* lpName, const char* lpValue);
DWORD ExpandEnvironmentStringsA(const char* lpSrc, char* lpDst, DWORD nSize);
BOOL GetComputerNameA(char* lpBuffer, uint32_t* nSize);
BOOL SetComputerNameA(const char* lpComputerName);
BOOL GetUserNameA(char* lpBuffer, uint32_t* pcbBuffer);

HANDLE GlobalAlloc(UINT uFlags, uint64_t dwBytes);
HGLOBAL GlobalFree(HGLOBAL hMem);
LPVOID GlobalLock(HGLOBAL hMem);
BOOL GlobalUnlock(HGLOBAL hMem);
uint64_t GlobalSize(HGLOBAL hMem);
HGLOBAL GlobalReAlloc(HGLOBAL hMem, uint64_t dwBytes, UINT uFlags);
HANDLE LocalAlloc(UINT uFlags, uint64_t uBytes);
HLOCAL LocalFree(HLOCAL hMem);
LPVOID LocalLock(HLOCAL hMem);
BOOL LocalUnlock(HLOCAL hMem);
uint64_t LocalSize(HLOCAL hMem);
LPVOID VirtualAlloc(LPVOID lpAddress, uint64_t dwSize, DWORD flAllocationType, DWORD flProtect);
BOOL VirtualFree(LPVOID lpAddress, uint64_t dwSize, DWORD dwFreeType);
BOOL VirtualProtect(LPVOID lpAddress, uint64_t dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
BOOL VirtualQuery(LPCVOID lpAddress, void* lpBuffer, uint64_t dwLength);
BOOL HeapDestroy(HANDLE hHeap);
HANDLE HeapCreate(DWORD flOptions, uint64_t dwInitialSize, uint64_t dwMaximumSize);
LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, uint64_t dwBytes);
BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
HANDLE GetProcessHeap(void);

int lstrlenA(const char* lpString);
char* lstrcpyA(char* lpString1, const char* lpString2);
char* lstrcpynA(char* lpString1, const char* lpString2, int iMaxLength);
int lstrcmpA(const char* lpString1, const char* lpString2);
int lstrcmpiA(const char* lpString1, const char* lpString2);
char* lstrcatA(char* lpString1, const char* lpString2);
LPSTR CharNextA(LPCSTR pch);
LPSTR CharPrevA(LPCSTR lpStart, LPCSTR lpCurrent);
BOOL IsCharAlphaA(CHAR ch);
BOOL IsCharAlphaNumericA(CHAR ch);
BOOL IsCharUpperA(CHAR ch);
BOOL IsCharLowerA(CHAR ch);
CHAR CharUpperA(CHAR ch);
CHAR CharLowerA(CHAR ch);
int wvsprintfA(char* lpOutput, const char* lpFmt, void* argptr);

int RegOpenKeyExA(HKEY hKey, const char* lpSubKey, DWORD ulOptions, REGSAM samDesired, HKEY* phkResult);
int RegCreateKeyExA(HKEY hKey, const char* lpSubKey, DWORD Reserved, char* lpClass, DWORD dwOptions, REGSAM samDesired, void* lpSecurityAttributes, HKEY* phkResult, DWORD* lpdwDisposition);
int RegCloseKey(HKEY hKey);
int RegQueryValueExA(HKEY hKey, const char* lpValueName, DWORD* lpReserved, DWORD* lpType, BYTE* lpData, DWORD* lpcbData);
int RegSetValueExA(HKEY hKey, const char* lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
int RegEnumKeyExA(HKEY hKey, DWORD dwIndex, char* lpName, DWORD* lpcchName, DWORD* lpReserved, char* lpClass, DWORD* lpcchClass, uint64_t* lpftLastWriteTime);
int RegEnumValueA(HKEY hKey, DWORD dwIndex, char* lpValueName, DWORD* lpcchValueName, DWORD* lpReserved, DWORD* lpType, BYTE* lpData, DWORD* lpcbData);
int RegDeleteKeyA(HKEY hKey, const char* lpSubKey);
int RegDeleteValueA(HKEY hKey, const char* lpValueName);
int RegFlushKey(HKEY hKey);

/* ================================================================
 * MSVCRT (Microsoft Visual C Runtime) - libc-like API
 * ================================================================ */
#ifndef _MSVCRT_TYPES
#define _MSVCRT_TYPES
typedef struct _FILE {
    int  fd;
    int  mode;
    int  eof;
    int  err;
    char ungetbuf[4];
    int  ungot;
} FILE;
#define stdin  ((FILE*)(uintptr_t)0)
#define stdout ((FILE*)(uintptr_t)1)
#define stderr ((FILE*)(uintptr_t)2)
#define EOF    (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define RAND_MAX 0x7fff
#endif

/* Memory */
void* malloc(size_t size);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);
void* _aligned_malloc(size_t size, size_t alignment);
void  _aligned_free(void* ptr);

/* String */
size_t strlen(const char* s);
char*  strcpy(char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t n);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strcat(char* dst, const char* src);
char*  strncat(char* dst, const char* src, size_t n);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);
char*  strstr(const char* haystack, const char* needle);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char*  strtok(char* str, const char* delim);
int    atoi(const char* s);
long   atol(const char* s);
long long atoll(const char* s);
double atof(const char* s);
char*  itoa(int value, char* str, int base);
char*  ltoa(long value, char* str, int base);
char*  lltoa(long long value, char* str, int base);
void*  memset(void* s, int c, size_t n);
void*  memcpy(void* dst, const void* src, size_t n);
void*  memmove(void* dst, const void* src, size_t n);
int    memcmp(const void* a, const void* b, size_t n);
void*  memchr(const void* s, int c, size_t n);

/* Stdio */
int printf(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t n, const char* fmt, ...);
int fprintf(FILE* f, const char* fmt, ...);
int fflush(FILE* f);
int vprintf(const char* fmt, va_list argptr);
int vsprintf(char* buf, const char* fmt, va_list argptr);
int vsnprintf(char* buf, size_t n, const char* fmt, va_list argptr);
int sscanf(const char* s, const char* fmt, ...);
int puts(const char* s);
int fputs(const char* s, FILE* f);
int fputc(int c, FILE* f);
int putchar(int c);
int fgetc(FILE* f);
int getchar(void);
char* fgets(char* s, int n, FILE* f);
char* gets_s(char* s, size_t n);
FILE* fopen(const char* path, const char* mode);
int   fclose(FILE* f);
size_t fread(void* buf, size_t sz, size_t n, FILE* f);
size_t fwrite(const void* buf, size_t sz, size_t n, FILE* f);
int   fseek(FILE* f, long offset, int origin);
long  ftell(FILE* f);
void  rewind(FILE* f);
int   feof(FILE* f);
int   ferror(FILE* f);
int   remove(const char* path);
int   rename(const char* oldp, const char* newp);
int   _mkdir(const char* path);
int   _rmdir(const char* path);
int   _chdir(const char* path);
char* _getcwd(char* buf, size_t n);
int   _access(const char* path, int mode);
long  _filelength(int fd);

/* Math / stdlib */
int   rand(void);
void  srand(unsigned int seed);
void  qsort(void* base, size_t num, size_t size,
            int (*cmp)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t num,
              size_t size, int (*cmp)(const void*, const void*));
void  abort(void);
void  exit(int code);
void  _exit(int code);
int   atexit(void (*fn)(void));
char* getenv(const char* name);
int   system(const char* cmd);
int   _setmode(int fd, int mode);

/* Wide-char stubs */
size_t wcslen(const unsigned short* s);
unsigned short* wcscpy(unsigned short* d, const unsigned short* s);
int    wcscmp(const unsigned short* a, const unsigned short* b);
int    _wcsicmp(const unsigned short* a, const unsigned short* b);
int    MultiByteToWideChar(uint32_t cp, uint32_t flags,
                            const char* mb, int mb_len,
                            unsigned short* wc, int wc_len);
int    WideCharToMultiByte(uint32_t cp, uint32_t flags,
                            const unsigned short* wc, int wc_len,
                            char* mb, int mb_len,
                            const char* def, int* used);
#define CP_ACP        0
#define CP_UTF8       65001
#define MB_PRECOMPOSED 0x01

/* ================================================================
 * NTDLL.DLL - Native system interfaces (Rtl, Nt, Zw)
 * ================================================================ */

/* NTSTATUS */
typedef LONG NTSTATUS;
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL             ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED          ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_INFO_CLASS       ((NTSTATUS)0xC0000003L)
#define STATUS_INFO_LENGTH_MISMATCH     ((NTSTATUS)0xC0000004L)
#define STATUS_ACCESS_VIOLATION         ((NTSTATUS)0xC0000005L)
#define STATUS_IN_PAGE_ERROR            ((NTSTATUS)0xC0000006L)
#define STATUS_INVALID_HANDLE           ((NTSTATUS)0xC0000008L)
#define STATUS_INVALID_PARAMETER        ((NTSTATUS)0xC000000DL)
#define STATUS_NO_SUCH_FILE             ((NTSTATUS)0xC000000FL)
#define STATUS_NO_MEMORY                ((NTSTATUS)0xC0000017L)
#define STATUS_ACCESS_DENIED            ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL         ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_TYPE_MISMATCH     ((NTSTATUS)0xC0000024L)
#define STATUS_OBJECT_NAME_NOT_FOUND    ((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_NAME_COLLISION    ((NTSTATUS)0xC0000035L)
#define STATUS_OBJECT_PATH_NOT_FOUND    ((NTSTATUS)0xC000003AL)
#define STATUS_EAS_NOT_SUPPORTED        ((NTSTATUS)0xC000004FL)
#define STATUS_EA_CORRUPT_ERROR         ((NTSTATUS)0xC0000050L)
#define STATUS_NONEXISTENT_EA_ENTRY     ((NTSTATUS)0xC0000051L)
#define STATUS_FILE_FORCE_CLOSED        ((NTSTATUS)0xC0000080L)
#define STATUS_PIPE_BROKEN              ((NTSTATUS)0xC000014BL)
#define STATUS_PIPE_DISCONNECTED        ((NTSTATUS)0xC000014CL)
#define STATUS_PIPE_CLOSING             ((NTSTATUS)0xC000014DL)
#define STATUS_THREAD_IS_TERMINATING    ((NTSTATUS)0xC000014FL)
#define STATUS_REPARSE_POINT_ENCOUNTERED ((NTSTATUS)0x80000025L)
#define STATUS_NOT_FOUND                ((NTSTATUS)0xC0000225L)

/* OBJECT_ATTRIBUTES */
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
#define OBJ_INHERIT              0x00000002L
#define OBJ_PERMANENT            0x00000010L
#define OBJ_EXCLUSIVE            0x00000020L
#define OBJ_CASE_INSENSITIVE     0x00000040L
#define OBJ_OPENIF               0x00000080L
#define OBJ_KERNEL_HANDLE        0x00000200L
#define OBJ_FORCE_ACCESS_CHECK   0x00000400L

/* IO_STATUS_BLOCK */
typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID    Pointer;
    } DUMMYUNIONNAME;
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

/* RtlXXX - Runtime Library */
void RtlInitUnicodeString(PUNICODE_STRING s, PCWSTR p);
NTSTATUS RtlAnsiStringToUnicodeString(PUNICODE_STRING dst,
                                       const void* src, BOOL alloc);
NTSTATUS RtlUnicodeStringToAnsiString(void* dst,
                                       PUNICODE_STRING src, BOOL alloc);
LONG RtlCompareUnicodeString(PUNICODE_STRING a, PUNICODE_STRING b, BOOL cs);
VOID RtlCopyUnicodeString(PUNICODE_STRING dst, PUNICODE_STRING src);
VOID RtlFreeUnicodeString(PUNICODE_STRING s);
NTSTATUS RtlIntegerToUnicodeString(ULONG val, ULONG base, PUNICODE_STRING s);
NTSTATUS RtlUnicodeStringToInteger(PUNICODE_STRING s, ULONG base, PULONG val);
ULONG RtlHashUnicodeString(PUNICODE_STRING s, BOOLEAN cs, ULONG hc);

BOOLEAN RtlEqualMemory(const void* a, const void* b, SIZE_T n);
VOID    RtlCopyMemory(PVOID dst, const VOID* src, SIZE_T n);
VOID    RtlFillMemory(PVOID dst, SIZE_T n, INT c);
VOID    RtlZeroMemory(PVOID dst, SIZE_T n);
SIZE_T  RtlCompareMemory(const VOID* a, const VOID* b, SIZE_T n);
VOID    RtlMoveMemory(PVOID dst, const VOID* src, SIZE_T n);

ULONG RtlUniform(PULONG seed);
ULONG RtlRandomEx(PULONG seed);

NTSTATUS RtlCreateUnicodeStringFromAsciiz(PUNICODE_STRING dst, const char* s);

/* NtXXX / ZwXXX - Native syscall stubs */
NTSTATUS NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                      POBJECT_ATTRIBUTES ObjectAttributes,
                      PIO_STATUS_BLOCK IoStatusBlock,
                      PLARGE_INTEGER AllocationSize,
                      ULONG FileAttributes, ULONG ShareAccess,
                      ULONG CreateDisposition, ULONG CreateOptions,
                      PVOID EaBuffer, ULONG EaLength);
NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                    POBJECT_ATTRIBUTES ObjectAttributes,
                    PIO_STATUS_BLOCK IoStatusBlock,
                    ULONG ShareAccess, ULONG OpenOptions);
NTSTATUS NtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
                    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
                    PVOID Buffer, ULONG Length,
                    PLARGE_INTEGER ByteOffset, PULONG Key);
NTSTATUS NtWriteFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
                     PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
                     const VOID* Buffer, ULONG Length,
                     PLARGE_INTEGER ByteOffset, PULONG Key);
NTSTATUS NtClose(HANDLE Handle);
NTSTATUS NtQueryInformationFile(HANDLE FileHandle,
                                PIO_STATUS_BLOCK IoStatusBlock,
                                PVOID FileInformation,
                                ULONG Length, ULONG FileInformationClass);
NTSTATUS NtSetInformationFile(HANDLE FileHandle,
                              PIO_STATUS_BLOCK IoStatusBlock,
                              PVOID FileInformation,
                              ULONG Length, ULONG FileInformationClass);
NTSTATUS NtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess,
                         POBJECT_ATTRIBUTES ObjectAttributes,
                         PLARGE_INTEGER MaximumSize,
                         ULONG SectionPageProtection,
                         ULONG AllocationAttributes,
                         HANDLE FileHandle);
NTSTATUS NtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle,
                            PVOID* BaseAddress,
                            ULONG_PTR ZeroBits,
                            SIZE_T CommitSize,
                            PLARGE_INTEGER SectionOffset,
                            PSIZE_T ViewSize,
                            ULONG InheritDisposition,
                            ULONG AllocationType,
                            ULONG Win32Protect);
NTSTATUS NtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress);
NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                                 ULONG_PTR ZeroBits, PSIZE_T RegionSize,
                                 ULONG AllocationType, ULONG Protect);
NTSTATUS NtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                             PSIZE_T RegionSize, ULONG FreeType);
NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                                PSIZE_T RegionSize, ULONG NewProtect,
                                PULONG OldProtect);
NTSTATUS NtQueryVirtualMemory(HANDLE ProcessHandle, LPCVOID Address,
                              ULONG MemoryInformationClass,
                              PVOID Buffer, SIZE_T Length, PSIZE_T Result);
NTSTATUS NtCreateProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess,
                         POBJECT_ATTRIBUTES ObjectAttributes,
                         HANDLE ParentProcess,
                         BOOLEAN InheritObjectTable,
                         HANDLE SectionHandle,
                         HANDLE DebugPort, HANDLE ExceptionPort);
NTSTATUS NtCreateThreadEx(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
                          POBJECT_ATTRIBUTES ObjectAttributes,
                          HANDLE ProcessHandle, PVOID StartRoutine,
                          PVOID Argument, ULONG CreateFlags,
                          ULONG_PTR ZeroBits, SIZE_T StackSize,
                          SIZE_T MaximumStackSize, PVOID AttributeList);
NTSTATUS NtTerminateProcess(HANDLE ProcessHandle, NTSTATUS ExitStatus);
NTSTATUS NtTerminateThread(HANDLE ThreadHandle, NTSTATUS ExitStatus);
NTSTATUS NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable,
                               const LARGE_INTEGER* Timeout);
NTSTATUS NtWaitForMultipleObjects(ULONG Count, const HANDLE* Handles,
                                  ULONG WaitType, BOOLEAN Alertable,
                                  const LARGE_INTEGER* Timeout);
NTSTATUS NtQuerySystemInformation(ULONG SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   PULONG ReturnLength);
NTSTATUS NtSetSystemInformation(ULONG SystemInformationClass,
                                 PVOID SystemInformation,
                                 ULONG SystemInformationLength);
NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle,
                                   ULONG ProcessInformationClass,
                                   PVOID ProcessInformation,
                                   ULONG ProcessInformationLength,
                                   PULONG ReturnLength);
NTSTATUS NtQueryInformationThread(HANDLE ThreadHandle,
                                  ULONG ThreadInformationClass,
                                  PVOID ThreadInformation,
                                  ULONG ThreadInformationLength,
                                  PULONG ReturnLength);
NTSTATUS NtDuplicateObject(HANDLE SourceProcessHandle, HANDLE SourceHandle,
                           HANDLE TargetProcessHandle, PHANDLE TargetHandle,
                           ACCESS_MASK DesiredAccess, ULONG HandleAttributes,
                           ULONG Options);
NTSTATUS NtLoadDriver(PUNICODE_STRING DriverServiceName);
NTSTATUS NtUnloadDriver(PUNICODE_STRING DriverServiceName);

/* ZwXXX = NtXXX for kernel-mode usage (same stubs) */
#define ZwCreateFile           NtCreateFile
#define ZwOpenFile             NtOpenFile
#define ZwReadFile             NtReadFile
#define ZwWriteFile            NtWriteFile
#define ZwClose                NtClose
#define ZwQueryInformationFile NtQueryInformationFile
#define ZwSetInformationFile   NtSetInformationFile
#define ZwCreateSection        NtCreateSection
#define ZwMapViewOfSection     NtMapViewOfSection
#define ZwUnmapViewOfSection   NtUnmapViewOfSection
#define ZwAllocateVirtualMemory NtAllocateVirtualMemory
#define ZwFreeVirtualMemory    NtFreeVirtualMemory
#define ZwProtectVirtualMemory NtProtectVirtualMemory
#define ZwQueryVirtualMemory   NtQueryVirtualMemory
#define ZwCreateProcess        NtCreateProcess
#define ZwCreateThreadEx       NtCreateThreadEx
#define ZwTerminateProcess     NtTerminateProcess
#define ZwTerminateThread      NtTerminateThread
#define ZwWaitForSingleObject  NtWaitForSingleObject
#define ZwWaitForMultipleObjects NtWaitForMultipleObjects
#define ZwQuerySystemInformation NtQuerySystemInformation
#define ZwSetSystemInformation   NtSetSystemInformation
#define ZwDuplicateObject      NtDuplicateObject
#define ZwLoadDriver           NtLoadDriver
#define ZwUnloadDriver         NtUnloadDriver

/* ================================================================
 * ADVAPI32.DLL - Advanced services (Registry+, Crypto, EventLog, Auth)
 * ================================================================ */

/* Event Logging */
#define EVENTLOG_SUCCESS          0x0000
#define EVENTLOG_ERROR_TYPE       0x0001
#define EVENTLOG_WARNING_TYPE     0x0002
#define EVENTLOG_INFORMATION_TYPE 0x0004
#define EVENTLOG_AUDIT_SUCCESS    0x0008
#define EVENTLOG_AUDIT_FAILURE    0x0010

HANDLE RegisterEventSourceA(const char* lpUNCServerName,
                            const char* lpSourceName);
BOOL   DeregisterEventSource(HANDLE hEventLog);
BOOL   ReportEventA(HANDLE hEventLog, WORD wType, WORD wCategory,
                    DWORD dwEventID, void* lpUserSid,
                    WORD wNumStrings, DWORD dwDataSize,
                    const char** lpStrings, LPVOID lpRawData);
HANDLE OpenEventLogA(const char* lpUNCServerName, const char* lpSourceName);
BOOL   CloseEventLog(HANDLE hEventLog);
BOOL   ReadEventLogA(HANDLE hEventLog, DWORD dwReadFlags,
                     DWORD dwRecordOffset, LPVOID lpBuffer,
                     DWORD nNumberOfBytesToRead,
                     DWORD* pnBytesRead, DWORD* pnMinNumberOfBytesNeeded);
BOOL   ClearEventLogA(HANDLE hEventLog, const char* lpBackupFileName);
BOOL   BackupEventLogA(HANDLE hEventLog, const char* lpBackupFileName);

/* Service API extensions (complement scm.c) */
BOOL QueryServiceConfigA(SC_HANDLE hService, void* lpServiceConfig,
                         DWORD cbBufSize, LPDWORD pcbBytesNeeded);
BOOL ChangeServiceConfigA(SC_HANDLE hService, DWORD dwServiceType,
                          DWORD dwStartType, DWORD dwErrorControl,
                          const char* lpBinaryPathName,
                          const char* lpLoadOrderGroup,
                          LPDWORD lpdwTagId,
                          const char* lpDependencies,
                          const char* lpServiceStartName,
                          const char* lpPassword,
                          const char* lpDisplayName);
BOOL QueryServiceConfig2A(SC_HANDLE hService, DWORD dwInfoLevel,
                          LPBYTE lpBuffer, DWORD cbBufSize,
                          LPDWORD pcbBytesNeeded);
BOOL ChangeServiceConfig2A(SC_HANDLE hService, DWORD dwInfoLevel,
                           LPVOID lpInfo);
BOOL EnumDependentServicesA(SC_HANDLE hService, DWORD dwServiceState,
                            void* lpServices, DWORD cbBufSize,
                            LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned);
BOOL StartServiceCtrlDispatcherA(const void* lpServiceStartTable);
BOOL SetServiceStatus(SC_HANDLE hServiceStatus,
                      const void* lpServiceStatus);
SC_HANDLE RegisterServiceCtrlHandlerA(const char* lpServiceName,
                                       void* lpHandlerProc);

/* Crypto API (Cryptography API: Next Generation stubs) */
typedef ULONG_PTR HCRYPTPROV;
typedef ULONG_PTR HCRYPTKEY;
typedef ULONG_PTR HCRYPTHASH;
#define PROV_RSA_FULL         1
#define PROV_RSA_SIG          2
#define PROV_DSS              3
#define PROV_FORTEZZA         4
#define PROV_MS_EXCHANGE      5
#define PROV_SSL              6
#define PROV_RSA_SCHANNEL     12
#define PROV_DSS_DH           13
#define PROV_EC_ECDSA_SIG     14
#define PROV_EC_ECNRA_SIG     15
#define PROV_EC_ECDSA_FULL    16
#define PROV_EC_ECNRA_FULL    17
#define PROV_DH_SCHANNEL      18
#define PROV_SPYKIT           20
#define PROV_RNG_TEST         21
#define CRYPT_VERIFYCONTEXT   0xF0000000
#define CRYPT_NEWKEYSET       0x00000008
#define CRYPT_DELETEKEYSET    0x00000010
#define CRYPT_MACHINE_KEYSET  0x00000020
#define CRYPT_SILENT          0x00000040
#define CRYPT_EXPORTABLE      0x00000001
#define CRYPT_USER_PROTECTED  0x00000002
#define CALG_MD5              0x00008003
#define CALG_SHA1             0x00008004
#define CALG_SHA_256          0x0000800c
#define CALG_SHA_512          0x0000800e
#define CALG_RSA_KEYX         0x0000a400
#define CALG_RSA_SIGN         0x00002400
#define CALG_AES_128          0x0000660e
#define CALG_AES_256          0x00006610
#define CALG_3DES             0x00006603
#define CALG_RC4              0x00006801

BOOL CryptAcquireContextA(HCRYPTPROV* phProv, const char* pszContainer,
                          const char* pszProvider, DWORD dwProvType,
                          DWORD dwFlags);
BOOL CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags);
BOOL CryptGenKey(HCRYPTPROV hProv, ALG_ID Algid, DWORD dwFlags,
                 HCRYPTKEY* phKey);
BOOL CryptDeriveKey(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTHASH hBaseData,
                    DWORD dwFlags, HCRYPTKEY* phKey);
BOOL CryptDestroyKey(HCRYPTKEY hKey);
BOOL CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType,
                    DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen);
BOOL CryptImportKey(HCRYPTPROV hProv, const BYTE* pbData, DWORD dwDataLen,
                    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey);
BOOL CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
                  DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen,
                  DWORD dwBufLen);
BOOL CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
                  DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen);
BOOL CryptCreateHash(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey,
                     DWORD dwFlags, HCRYPTHASH* phHash);
BOOL CryptHashData(HCRYPTHASH hHash, const BYTE* pbData,
                   DWORD dwDataLen, DWORD dwFlags);
BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam,
                       BYTE* pbData, DWORD* pdwDataLen, DWORD dwFlags);
BOOL CryptDestroyHash(HCRYPTHASH hHash);
BOOL CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer);
BOOL CryptProtectData(const void* pDataIn, const char* szDataDescr,
                      const void* pOptionalEntropy, PVOID pvReserved,
                      const void* pPromptStruct, DWORD dwFlags,
                      void* pDataOut);
BOOL CryptUnprotectData(const void* pDataIn, char** ppszDataDescr,
                        void* pOptionalEntropy, PVOID pvReserved,
                        void* pPromptStruct, DWORD dwFlags,
                        void* pDataOut);

/* Credential management */
BOOL CredReadA(const char* TargetName, DWORD Type, DWORD Flags,
               void** Credential);
BOOL CredWriteA(const void* Credential, DWORD Flags);
BOOL CredDeleteA(const char* TargetName, DWORD Type, DWORD Flags);
BOOL CredFree(void* Buffer);
BOOL CredEnumerateA(const char* Filter, DWORD Flags,
                    DWORD* Count, void*** Credentials);

/* Power / shutdown */
BOOL InitiateSystemShutdownA(const char* lpMachineName,
                             const char* lpMessage,
                             DWORD dwTimeout, BOOL bForceAppsClosed,
                             BOOL bRebootAfterShutdown);
BOOL AbortSystemShutdownA(const char* lpMachineName);
BOOL ExitWindowsEx(UINT uFlags, DWORD dwReason);
#define EWX_LOGOFF   0x00000000
#define EWX_SHUTDOWN 0x00000001
#define EWX_REBOOT   0x00000002
#define EWX_FORCE    0x00000004
#define EWX_POWEROFF 0x00000008

/* Process tokens / auth helpers */
BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess,
                      PHANDLE TokenHandle);
BOOL OpenThreadToken(HANDLE ThreadHandle, DWORD DesiredAccess,
                     BOOL OpenAsSelf, PHANDLE TokenHandle);
BOOL GetTokenInformation(HANDLE TokenHandle, ULONG TokenInformationClass,
                         LPVOID TokenInformation, DWORD TokenInformationLength,
                         PDWORD ReturnLength);
BOOL SetTokenInformation(HANDLE TokenHandle, ULONG TokenInformationClass,
                         LPVOID TokenInformation, DWORD TokenInformationLength);
BOOL AdjustTokenPrivileges(HANDLE TokenHandle, BOOL DisableAllPrivileges,
                           void* NewState, DWORD BufferLength,
                           void* PreviousState, PDWORD ReturnLength);
BOOL LookupPrivilegeValueA(const char* lpSystemName, const char* lpName,
                           PLUID lpLuid);
BOOL LookupPrivilegeNameA(const char* lpSystemName, PLUID lpLuid,
                          char* lpName, LPDWORD cchName);
BOOL LookupAccountNameA(const char* lpSystemName, const char* lpAccountName,
                        void* Sid, LPDWORD cbSid,
                        char* ReferencedDomainName,
                        LPDWORD cchReferencedDomainName,
                        PSID_NAME_USE peUse);
BOOL LookupAccountSidA(const char* lpSystemName, void* Sid,
                       char* Name, LPDWORD cchName,
                       char* ReferencedDomainName,
                       LPDWORD cchReferencedDomainName,
                       PSID_NAME_USE peUse);

/* ================================================================
 * SHELL32 / CMD.EXE - Windows Shell & command interpreter
 * ================================================================ */

/* Shell file operations */
HINSTANCE ShellExecuteA(HWND hwnd, const char* lpOperation,
                        const char* lpFile, const char* lpParameters,
                        const char* lpDirectory, INT nShowCmd);
BOOL      ShellExecuteExA(void* pExecInfo);
UINT      DragQueryFileA(HANDLE hDrop, UINT iFile,
                         char* lpszFile, UINT cch);
BOOL      DragQueryPoint(HANDLE hDrop, void* lppt);
VOID      DragFinish(HANDLE hDrop);
BOOL      Dragging(void);

/* SHFileOperation */
#define FO_MOVE         0x0001
#define FO_COPY         0x0002
#define FO_DELETE       0x0003
#define FO_RENAME       0x0004
int  SHFileOperationA(void* lpFileOp);

/* Path helpers */
void PathAddBackslashA(char* path);
BOOL PathRemoveBackslashA(char* path);
void PathAddExtensionA(char* path, const char* ext);
BOOL PathRemoveExtensionA(char* path);
BOOL PathRenameExtensionA(char* dst, const char* src, const char* ext);
BOOL PathFileExistsA(const char* path);
BOOL PathDirectoryExistsA(const char* path);
char* PathFindFileNameA(const char* path);
char* PathFindExtensionA(const char* path);
char* PathFindNextComponentA(const char* path);
BOOL PathIsDirectoryA(const char* path);
BOOL PathIsFileSpecA(const char* path);
BOOL PathIsRootA(const char* path);
BOOL PathIsRelativeA(const char* path);
BOOL PathStripPathA(char* path);
BOOL PathStripToRootA(char* path);
BOOL PathQuoteSpacesA(char* path);
BOOL PathUnquoteSpacesA(char* path);
BOOL PathCanonicalizeA(char* dst, const char* src);
BOOL PathCombineA(char* dst, const char* dir, const char* file);
BOOL PathAppendA(char* path, const char* more);
void PathBuildRootA(char* root, int drive);
BOOL PathGetDriveNumberA(const char* path);
BOOL PathMatchSpecA(const char* path, const char* spec);

/* Shell folders (CSIDL) */
#define CSIDL_DESKTOP                0x0000
#define CSIDL_INTERNET               0x0001
#define CSIDL_PROGRAMS               0x0002
#define CSIDL_CONTROLS               0x0003
#define CSIDL_PRINTERS               0x0004
#define CSIDL_PERSONAL               0x0005
#define CSIDL_FAVORITES              0x0006
#define CSIDL_STARTUP                0x0007
#define CSIDL_RECENT                 0x0008
#define CSIDL_SENDTO                 0x0009
#define CSIDL_BITBUCKET              0x000a
#define CSIDL_STARTMENU              0x000b
#define CSIDL_MYDOCUMENTS            0x000c
#define CSIDL_MYMUSIC                0x000d
#define CSIDL_MYVIDEO                0x000e
#define CSIDL_DESKTOPDIRECTORY       0x0010
#define CSIDL_DRIVES                 0x0011
#define CSIDL_NETWORK                0x0012
#define CSIDL_NETHOOD                0x0013
#define CSIDL_FONTS                  0x0014
#define CSIDL_TEMPLATES              0x0015
#define CSIDL_COMMON_STARTMENU       0x0016
#define CSIDL_COMMON_PROGRAMS        0x0017
#define CSIDL_COMMON_STARTUP         0x0018
#define CSIDL_COMMON_DESKTOPDIRECTORY 0x0019
#define CSIDL_APPDATA                0x001a
#define CSIDL_PRINTHOOD              0x001b
#define CSIDL_LOCAL_APPDATA          0x001c
#define CSIDL_ALTSTARTUP             0x001d
#define CSIDL_COMMON_ALTSTARTUP      0x001e
#define CSIDL_COMMON_FAVORITES       0x001f
#define CSIDL_INTERNET_CACHE         0x0020
#define CSIDL_COOKIES                0x0021
#define CSIDL_HISTORY                0x0022
#define CSIDL_COMMON_APPDATA         0x0023
#define CSIDL_WINDOWS                0x0024
#define CSIDL_SYSTEM                 0x0025
#define CSIDL_PROGRAM_FILES          0x0026
#define CSIDL_MYPICTURES             0x0027
#define CSIDL_PROFILE                0x0028
#define CSIDL_SYSTEMX86              0x0029
#define CSIDL_PROGRAM_FILESX86       0x002a
#define CSIDL_PROGRAM_FILES_COMMON   0x002b
#define CSIDL_PROGRAM_FILES_COMMONX86 0x002c
#define CSIDL_COMMON_TEMPLATES       0x002d
#define CSIDL_COMMON_DOCUMENTS       0x002e
#define CSIDL_COMMON_ADMINTOOLS      0x002f
#define CSIDL_ADMINTOOLS             0x0030
#define CSIDL_FLAG_CREATE            0x8000
#define SHGFP_TYPE_CURRENT           0
#define SHGFP_TYPE_DEFAULT           1
int SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken,
                     DWORD dwFlags, char* pszPath);
int SHGetSpecialFolderPathA(HWND hwnd, char* pszPath,
                            int csidl, BOOL fCreate);

/* Notification icons */
#define NIM_ADD          0x00000000
#define NIM_MODIFY       0x00000001
#define NIM_DELETE       0x00000002
#define NIM_SETFOCUS     0x00000003
#define NIM_SETVERSION   0x00000004
#define NIF_MESSAGE      0x00000001
#define NIF_ICON         0x00000002
#define NIF_TIP          0x00000004
#define NIF_STATE        0x00000008
#define NIF_INFO         0x00000010
BOOL Shell_NotifyIconA(DWORD dwMessage, void* pnid);

/* ================================================================
 * CMD.EXE / Batch script interpreter
 * ================================================================ */
typedef struct cmd_state {
    char   cwd[MAX_PATH];
    char   path_env[8192];
    char*  vars[256];        /* "NAME=VALUE" entries */
    int    nvars;
    int    echo_on;
    int    interactive;
    int    errorlevel;
    int    recurse_depth;
    struct {
        int  enabled;
        char label[256];
        int  skip_flag;
    } if_state;
    struct {
        int  enabled;
        char varname[64];
        char list[4096];
        int  iter;
        int  start_line;
    } for_state;
    struct {
        int  enabled;
        char target_label[256];
    } goto_state;
    struct {
        int  enabled;
        char modules[16][64];
        int  count;
    } call_stack;
} cmd_state_t;

void cmd_init(cmd_state_t* cmd, int interactive);
void cmd_destroy(cmd_state_t* cmd);
int  cmd_set_var(cmd_state_t* cmd, const char* name, const char* value);
const char* cmd_get_var(cmd_state_t* cmd, const char* name);
int  cmd_unset_var(cmd_state_t* cmd, const char* name);
int  cmd_expand(cmd_state_t* cmd, const char* in, char* out, size_t outsz);
int  cmd_execute_line(cmd_state_t* cmd, const char* line);
int  cmd_execute_script(cmd_state_t* cmd, const char* path);
int  cmd_interactive(cmd_state_t* cmd);
void cmd_print_prompt(cmd_state_t* cmd);

/* Built-in cmd commands */
typedef int (*cmd_builtin_fn)(cmd_state_t* cmd, int argc, char** argv);
typedef struct {
    const char*    name;
    cmd_builtin_fn fn;
    const char*    help;
} cmd_builtin_t;

extern cmd_builtin_t cmd_builtins[];
extern int cmd_builtin_count;

/* Entry points (mimic cmd.exe) */
int cmd_main(int argc, char** argv);

/* ================================================================
 * Environment Block / Process Creation helpers
 * ================================================================ */
BOOL CreateProcessA(const char* lpApplicationName,
                    char* lpCommandLine,
                    void* lpProcessAttributes,
                    void* lpThreadAttributes,
                    BOOL bInheritHandles,
                    DWORD dwCreationFlags,
                    LPVOID lpEnvironment,
                    const char* lpCurrentDirectory,
                    void* lpStartupInfo,
                    void* lpProcessInformation);
BOOL CreateProcessAsUserA(HANDLE hToken,
                          const char* lpApplicationName,
                          char* lpCommandLine,
                          void* lpProcessAttributes,
                          void* lpThreadAttributes,
                          BOOL bInheritHandles,
                          DWORD dwCreationFlags,
                          LPVOID lpEnvironment,
                          const char* lpCurrentDirectory,
                          void* lpStartupInfo,
                          void* lpProcessInformation);
BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);
BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);
BOOL GetProcessTimes(HANDLE hProcess,
                     void* lpCreationTime,
                     void* lpExitTime,
                     void* lpKernelTime,
                     void* lpUserTime);
BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
                void* lpPipeAttributes, DWORD nSize);
BOOL DuplicateHandle(HANDLE hSourceProcessHandle,
                     HANDLE hSourceHandle,
                     HANDLE hTargetProcessHandle,
                     PHANDLE lpTargetHandle,
                     DWORD dwDesiredAccess,
                     BOOL bInheritHandle, DWORD dwOptions);

/* Subsystem init entry points (called from kernel.c) */
int     msvcrt_init(void);
int     ntdll_init(void);
int     advapi32_init(void);
int     shell32_init(void);
int     cmd_init_global(void);

#ifdef __cplusplus
}
#endif

#endif