
// win/windows.h : windows definitions

// https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types

#if WINDOWS

//------------------------------------------------------------------------------
#begin unsafe
//------------------------------------------------------------------------------

typedef char  CHAR;
typedef wchar WCHAR;

typedef byte VOID;
typedef byte BYTE;
typedef byte UINT8;
typedef uint2 WORD;
typedef uint2 USHORT;
typedef uint4 DWORD;
typedef uint4 ULONG;
typedef uint4 ULONG32;
typedef uint4 UINT;
typedef uint  UINT32;
typedef int8  ULONGLONG;
typedef int8  UINT64;
typedef int8  DWORD64;

typedef int2  SHORT;
typedef int4  INT;
typedef int4  LONG;
typedef int8  DWORDLONG;
typedef int8  LONGLONG;
typedef int8  INT64;
typedef int8  ULARGE_INTEGER;

typedef float  FLOAT;
typedef double DOUBLE;

#if MEM32
  typedef int4 INT_PTR;
  typedef int4 LONG_PTR;
  typedef uint4 UINT_PTR;
  typedef uint4 ULONG_PTR;
#else
  typedef int8 INT_PTR;
  typedef int8 LONG_PTR;
  typedef int8 UINT_PTR;
  typedef int8 ULONG_PTR;
#endif

typedef INT_PTR HANDLE;   // HANDLE is signed integer, not pointer like in other langages

//------------------------------------------------------------------------------

typedef LONG_PTR LRESULT;
typedef ULONG_PTR SIZE_T;

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef ULONG_PTR DWORD_PTR;

typedef VOID* PVOID;
typedef VOID* LPVOID;

typedef CHAR* LPSTR;
typedef CHAR* LPCSTR;

typedef WCHAR* LPCWSTR;
typedef WCHAR* PCWSTR;
typedef WCHAR* LPWSTR;
typedef WCHAR* LPCOLESTR;
typedef WCHAR* LPOLESTR;

typedef CHAR TCHAR;     // can be unicode too so WCHAR*
typedef CHAR* LPCTSTR;  // can be unicode too so WCHAR*
typedef LPSTR LPTSTR;   // can be unicode too so WCHAR*

typedef BYTE* LPBYTE;
typedef DWORD* LPDWORD;

typedef WORD  ATOM;
typedef INT   BOOL;
typedef BYTE  BOOLEAN;
typedef DWORD COLORREF;
typedef LONGLONG MFTIME;    // hundred nanosecond unit

typedef HANDLE HACCEL;
typedef HANDLE HBITMAP;
typedef HANDLE HBRUSH;
typedef HANDLE HDC;
typedef HANDLE HICON;
typedef HANDLE HFONT;
typedef HANDLE HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef HANDLE HINSTANCE;
typedef HANDLE HKEY;
typedef HANDLE HMENU;
typedef HANDLE HPEN;
typedef HANDLE HCRYPTPROV;
typedef HANDLE HRGN;
typedef HANDLE HRSRC;
typedef HANDLE HMONITOR;
typedef HANDLE SC_HANDLE;
typedef HANDLE HMETAFILEPICT;
typedef HANDLE HENHMETAFILE;

typedef HANDLE HWND;

typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef INT HFILE;

typedef LONG HRESULT;

const HANDLE INVALID_HANDLE_VALUE = -1;

packed struct REFCLSID
{
  uint4 a;
  uint2 b;
  uint2 c;
  uint1 r1, r2, r3, r4, r5, r6, r7, r8;
}

typedef REFCLSID REFIID;
typedef REFCLSID GUID;
typedef REFCLSID IID;
typedef REFCLSID LPCLSID;
typedef GUID*    REFGUID;



const int MAX_MODULE_NAME32 = 255;
const int MAX_PATH = 260;

struct MODULEENTRY32
{
  DWORD   dwSize;
  DWORD   th32ModuleID;
  DWORD   th32ProcessID;
  DWORD   GlblcntUsage;
  DWORD   ProccntUsage;
  BYTE    *modBaseAddr;
  DWORD   modBaseSize;
  HMODULE hModule;
  char    szModule[MAX_MODULE_NAME32 + 1];
  char    szExePath[MAX_PATH];
}

[extern "KERNEL32.DLL"]
BOOL Module32First(
  HANDLE          hSnapshot,
  MODULEENTRY32*  lpme);

[extern "KERNEL32.DLL"]
BOOL Module32Next(
  HANDLE          hSnapshot,
  MODULEENTRY32* lpme);

struct THREADENTRY32
{
  DWORD dwSize;
  DWORD cntUsage;
  DWORD th32ThreadID;
  DWORD th32OwnerProcessID;
  LONG  tpBasePri;
  LONG  tpDeltaPri;
  DWORD dwFlags;
}

[extern "KERNEL32.DLL"]
BOOL Thread32First (HANDLE hSnapshot, THREADENTRY32* lpte);

[extern "KERNEL32.DLL"]
BOOL Thread32Next (HANDLE hSnapshot, THREADENTRY32* lpte);

// -----------------------------

[extern "KERNEL32.DLL"]
HANDLE OpenThread (DWORD dwDesiredAccess, BOOL  bInheritHandle, DWORD dwThreadId);

[extern "KERNEL32.DLL"]
DWORD SuspendThread (HANDLE hThread);

[extern "KERNEL32.DLL"]
DWORD ResumeThread (HANDLE hThread);

// -----------------------------

// calendar

struct SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
}

[extern "KERNEL32.DLL"]
void GetLocalTime (out SYSTEMTIME lpSystemTime);

[extern "KERNEL32.DLL"]
void GetSystemTime (out SYSTEMTIME lpSystemTime);

struct TIME_ZONE_INFORMATION
{
  LONG       Bias;
  WCHAR      StandardName[32];
  SYSTEMTIME StandardDate;
  LONG       StandardBias;
  WCHAR      DaylightName[32];
  SYSTEMTIME DaylightDate;
  LONG       DaylightBias;
}

[extern "KERNEL32.DLL"]
DWORD GetTimeZoneInformation (out TIME_ZONE_INFORMATION TimeZoneInformation);

// -----------------------------

// files

// access
const uint GENERIC_READ  = 0x80000000;
const uint GENERIC_WRITE = 0x40000000;

// share
const uint FILE_SHARE_READ   = 0x00000001;
const uint FILE_SHARE_WRITE  = 0x00000002;
const uint FILE_SHARE_DELETE = 0x00000004;

// creation disposition
const uint CREATE_NEW = 1;
const uint CREATE_ALWAYS = 2;
const uint OPEN_EXISTING = 3;

// attr
const uint FILE_ATTRIBUTE_HIDDEN = 2;
const uint FILE_ATTRIBUTE_TEMPORARY = 256;
const uint FILE_FLAG_DELETE_ON_CLOSE = 0x04000000;


const uint FILE_ATTRIBUTE_NORMAL     = 0x00000080;
const uint FILE_FLAG_RANDOM_ACCESS   = 0x10000000;
const uint FILE_FLAG_SEQUENTIAL_SCAN = 0x08000000;

packed struct FILETIME
{
  uint dwLowDateTime;
  uint dwHighDateTime;
}

struct WIN32_FIND_DATAA
{
  uint     dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  uint     nFileSizeHigh;
  uint     nFileSizeLow;
  uint     dwReserved0;
  uint     dwReserved1;
  char     cFileName[MAX_PATH];
  char     cAlternateFileName[14];
}

struct WIN32_FIND_DATAW
{
  uint     dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  uint     nFileSizeHigh;
  uint     nFileSizeLow;
  uint     dwReserved0;
  uint     dwReserved1;
  wchar    cFileName[MAX_PATH];
  wchar    cAlternateFileName[14];
}

struct LARGE_INTEGER
{
  uint low;
  int  high;
}

struct OVERLAPPED
{
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  DWORD Offset;
  DWORD OffsetHigh;
  HANDLE hEvent;
}

[extern "KERNEL32.DLL"]
BOOL FileTimeToLocalFileTime (FILETIME lpFileTime, out FILETIME lpLocalFileTime);

[extern "KERNEL32.DLL"]
BOOL LocalFileTimeToFileTime (FILETIME ft1, out FILETIME ft2);

[extern "KERNEL32.DLL"]
BOOL FileTimeToSystemTime (FILETIME lpFileTime, out SYSTEMTIME lpSystemTime);

[extern "KERNEL32.DLL"]
BOOL SystemTimeToFileTime (SYSTEMTIME lpSystemTime, out FILETIME lpFileTime);

[extern "KERNEL32.DLL"]
BOOL GetFileTime (HANDLE handle, FILETIME *CreationTime, FILETIME *LastAccessTime, FILETIME *LastWriteTime);

[extern "KERNEL32.DLL"]
BOOL SetFileTime (HANDLE handle, FILETIME *CreationTime, FILETIME *LastAccessTime, FILETIME *LastWriteTime);

[extern "KERNEL32.DLL"]
BOOL SystemTimeToTzSpecificLocalTime
  (    TIME_ZONE_INFORMATION *p,   // can be null
       SYSTEMTIME lpSystemTime,
   out SYSTEMTIME lpSystemTimeB);

[extern "KERNEL32.DLL"]
DWORD GetLastError ();

[extern "KERNEL32.DLL"]
DWORD GetFileSize (HANDLE hFile, DWORD *high);

[extern "KERNEL32.DLL"]
BOOL GetFileSizeEx (HANDLE hFile, int8 *lpFileSize);

[extern "KERNEL32.DLL"]
DWORD SetFilePointer (
  HANDLE hFile,
  ULONG  lDistanceToMove,  // should be unsigned
  LONG*  lDistanceToMoveHigh,
  DWORD  dwMoveMethod);

[extern "KERNEL32.DLL"]
BOOL SetFilePointerEx (HANDLE hFile, int8 liDistanceToMove, int8* lpNewFilePointer, DWORD dwMoveMethod);

[extern "KERNEL32.DLL"]
HANDLE CreateFileA (
  LPCSTR lpFileName,
  DWORD  dwDesiredAccess,
  DWORD  dwShareMode,
  byte* lpSecurityAttributes,
  DWORD  dwCreationDisposition,
  DWORD  dwFlagsAndAttributes,
  HANDLE hTemplateFile);

[extern "KERNEL32.DLL"]
HANDLE CreateFileW (
  LPCWSTR lpFileName,
  DWORD  dwDesiredAccess,
  DWORD  dwShareMode,
  byte* lpSecurityAttributes,
  DWORD  dwCreationDisposition,
  DWORD  dwFlagsAndAttributes,
  HANDLE hTemplateFile);

[extern "KERNEL32.DLL"]
BOOL WriteFile (
  HANDLE hFile,
  byte*  buffer,
  DWORD  size,
  DWORD* bytesWritten,
  byte*  lpOverlapped);

[extern "KERNEL32.DLL"]
BOOL ReadFile (
  HANDLE hFile,
  byte*  buffer,
  DWORD  size,
  DWORD* BytesRead,
  byte*  lpOverlapped);

[extern "KERNEL32.DLL"]
BOOL LockFileEx (
  HANDLE  hFile,
  DWORD dwFlags,
  DWORD dwReserved,
  DWORD nNumberOfBytesToLockLow,
  DWORD nNumberOfBytesToLockHigh,
  OVERLAPPED* lpOverlapped);

[extern "KERNEL32.DLL"]
BOOL UnlockFileEx (
  HANDLE  hFile,
  DWORD dwReserved,
  DWORD nNumberOfBytesToUnlockLow,
  DWORD nNumberOfBytesToUnlockHigh,
  OVERLAPPED* lpOverlapped);

[extern "KERNEL32.DLL"]
BOOL FlushFileBuffers (HANDLE hFile);

[extern "KERNEL32.DLL"]
BOOL CloseHandle (HANDLE hObject);

[extern "KERNEL32.DLL"]
BOOL MoveFileA (char *lpExistingFileName, char *lpNewFileName);

[extern "KERNEL32.DLL"]
BOOL MoveFileW (wchar *lpExistingFileName, wchar *lpNewFileName);

[extern "KERNEL32.DLL"]
BOOL CopyFileA (char *lpExistingFileName, char *lpNewFileName, BOOL bFailIfExists);

[extern "KERNEL32.DLL"]
BOOL CopyFileW (wchar *lpExistingFileName, wchar *lpNewFileName, BOOL bFailIfExists);

[extern "KERNEL32.DLL"]
BOOL DeleteFileA (char *lpFileName);

[extern "KERNEL32.DLL"]
BOOL DeleteFileW (wchar *lpFileName);

[extern "KERNEL32.DLL"]
BOOL GetFileAttributesExA (     LPCTSTR      lpFileName,
                                uint         fInfoLevelId,
                           WIN32_FIND_DATAA* lpFileInformation);

[extern "KERNEL32.DLL"]
BOOL GetFileAttributesExW (     LPCWSTR      lpFileName,
                                uint         fInfoLevelId,
                           WIN32_FIND_DATAW* lpFileInformation);

[extern "KERNEL32.DLL"]
HANDLE FindFirstFileA (char *filename, WIN32_FIND_DATAA* lpFindFileData);

[extern "KERNEL32.DLL"]
HANDLE FindFirstFileW (wchar *filename, WIN32_FIND_DATAW* lpFindFileData);

[extern "KERNEL32.DLL"]
BOOL FindNextFileA (HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData);

[extern "KERNEL32.DLL"]
BOOL FindNextFileW (HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData);

[extern "KERNEL32.DLL"]
BOOL FindClose (HANDLE hFindFile);


[extern "KERNEL32.DLL"]
BOOL CreateDirectoryA (char *lpPathName, byte *lpSecurityAttributes);

[extern "KERNEL32.DLL"]
BOOL CreateDirectoryW (wchar *lpPathName, byte *lpSecurityAttributes);

[extern "KERNEL32.DLL"]
BOOL RemoveDirectoryA (char *lpPathName);

[extern "KERNEL32.DLL"]
BOOL RemoveDirectoryW (wchar *lpPathName);

[extern "KERNEL32.DLL"]
DWORD GetCurrentDirectoryA (DWORD nBufferLength, char *Buffer);

[extern "KERNEL32.DLL"]
DWORD GetCurrentDirectoryW (DWORD nBufferLength, wchar *Buffer);

[extern "KERNEL32.DLL"]
BOOL SetCurrentDirectoryA (char *PathName);

[extern "KERNEL32.DLL"]
BOOL SetCurrentDirectoryW (wchar *PathName);

[extern "KERNEL32.DLL"]
BOOL GetDiskFreeSpaceExA (
  char          *DirectoryName,
  LARGE_INTEGER *lpFreeBytesAvailable,
  LARGE_INTEGER *lpTotalNumberOfBytes,
  LARGE_INTEGER *lpTotalNumberOfFreeBytes);

[extern "KERNEL32.DLL"]
BOOL GetDiskFreeSpaceExW (
  wchar         *DirectoryName,
  LARGE_INTEGER *lpFreeBytesAvailable,
  LARGE_INTEGER *lpTotalNumberOfBytes,
  LARGE_INTEGER *lpTotalNumberOfFreeBytes);

[extern "KERNEL32.DLL"]
DWORD GetFinalPathNameByHandleA
  (HANDLE hFile,
   LPSTR lpszFilePath,
   DWORD cchFilePath,
   DWORD dwFlags);

[extern "KERNEL32.DLL"]
DWORD GetFinalPathNameByHandleW
  (HANDLE hFile,
   LPWSTR lpszFilePath,
   DWORD  cchFilePath,
   DWORD  dwFlags);

// -----------------------------

// console

[extern "KERNEL32.DLL"]
HANDLE GetStdHandle (int nStdHandle);


[extern "KERNEL32.DLL"]
int MultiByteToWideChar(
  UINT   CodePage,
  DWORD  dwFlags,
  char*  lpMultiByteStr,
  int    cbMultiByte,
  wchar* lpWideCharStr,
  int    cchWideChar);

const DWORD MB_PRECOMPOSED = 0x00000001;
const DWORD MB_COMPOSITE   = 0x00000002;

//------------------------------------------------------------------------------

// exception

struct EXCEPTION_RECORD
{
  DWORD            ExceptionCode;
  DWORD            ExceptionFlags;
  EXCEPTION_RECORD *ExceptionRecord;
  INT_PTR          ExceptionAddress;
  DWORD            NumberParameters;
  ULONG_PTR        ExceptionInformation[15];
}

struct M128A
{
  ULONGLONG Low;
  LONGLONG High;
}


#if MEM32
struct CONTEXT
{
  uint ContextFlags;
  uint Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;

  // This section is returned if the ContextFlags word contains the flag CONTEXT_FLOATING_POINT.
  uint ControlWord, StatusWord, TagWord, ErrorOffset, ErrorSelector, DataOffset, DataSelector;
  byte RegisterArea[80], Cr0NpxState;

  // This section is returned if the ContextFlags word contains the flag CONTEXT_SEGMENTS.
  uint SegGs, SegFs, SegEs, SegDs;

  // This section is returned if the ContextFlags word contains the flag CONTEXT_INTEGER.
  int Rdi, Rsi, Rbx, Rdx, Rcx, Rax;

  // This section is returned if the ContextFlags word contains the flag CONTEXT_CONTROL.
  int Rbp, Rip, SegCs, EFlags, Rsp, SegSs;

  BYTE    ExtendedRegisters[512];
}
#endif

#if MEM64
struct CONTEXT
{
    DWORD64 P1Home;
    DWORD64 P2Home;
    DWORD64 P3Home;
    DWORD64 P4Home;
    DWORD64 P5Home;
    DWORD64 P6Home;

    // Control flags.
    DWORD ContextFlags;
    DWORD MxCsr;

    // Segment Registers and processor flags.
    WORD   SegCs;
    WORD   SegDs;
    WORD   SegEs;
    WORD   SegFs;
    WORD   SegGs;
    WORD   SegSs;
    DWORD EFlags;

    // Debug registers
    DWORD64 Dr0;
    DWORD64 Dr1;
    DWORD64 Dr2;
    DWORD64 Dr3;
    DWORD64 Dr6;
    DWORD64 Dr7;

    // Integer registers.
    DWORD64 Rax;
    DWORD64 Rcx;
    DWORD64 Rdx;
    DWORD64 Rbx;
    DWORD64 Rsp;
    DWORD64 Rbp;
    DWORD64 Rsi;
    DWORD64 Rdi;
    DWORD64 R8;
    DWORD64 R9;
    DWORD64 R10;
    DWORD64 R11;
    DWORD64 R12;
    DWORD64 R13;
    DWORD64 R14;
    DWORD64 R15;

    // Program counter.
    DWORD64 Rip;

    // Floating point state.
    byte xmm[512];
/*
    union {
        XMM_SAVE_AREA32 FltSave;  // 512 bytes
        struct {  // 26 x 16 = 416
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0;
            M128A Xmm1;
            M128A Xmm2;
            M128A Xmm3;
            M128A Xmm4;
            M128A Xmm5;
            M128A Xmm6;
            M128A Xmm7;
            M128A Xmm8;
            M128A Xmm9;
            M128A Xmm10;
            M128A Xmm11;
            M128A Xmm12;
            M128A Xmm13;
            M128A Xmm14;
            M128A Xmm15;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
*/

    // Vector registers.
    M128A VectorRegister[26];
    DWORD64 VectorControl;

    // Special debug control registers.
    DWORD64 DebugControl;
    DWORD64 LastBranchToRip;
    DWORD64 LastBranchFromRip;
    DWORD64 LastExceptionToRip;
    DWORD64 LastExceptionFromRip;
}
#endif

struct EXCEPTION_POINTERS
{
  EXCEPTION_RECORD *ExceptionRecord;
  CONTEXT          *ContextRecord;
}

typedef [callback] void TOPLEVELEXCEPTIONFILTER (EXCEPTION_POINTERS e);

[extern "KERNEL32.DLL"]
TOPLEVELEXCEPTIONFILTER* SetUnhandledExceptionFilter (TOPLEVELEXCEPTIONFILTER f);

typedef [callback] LONG PVECTORED_EXCEPTION_HANDLER (EXCEPTION_POINTERS ExceptionInfo);

[extern "KERNEL32.DLL"]
PVOID AddVectoredExceptionHandler (ULONG First, PVECTORED_EXCEPTION_HANDLER Handler);

[extern "KERNEL32.DLL"]
void FatalAppExitA (UINT uAction, char *MessageText);

[extern "KERNEL32.DLL"]
void FatalAppExitW (UINT uAction, wchar *MessageText);

[extern "KERNEL32.DLL"]
void ExitProcess (UINT retcode);

typedef [callback] BOOL FEnumProcessModulesEx(
  HANDLE  hProcess,
  HMODULE *lphModule,
  DWORD   cb,
  LPDWORD lpcbNeeded,
  DWORD   dwFilterFlag);

[extern "KERNEL32.DLL"]
HANDLE CreateToolhelp32Snapshot(
  DWORD dwFlags,
  DWORD th32ProcessID);

const DWORD TH32CS_SNAPHEAPLIST = 1;
const DWORD TH32CS_SNAPPROCESS = 2;
const DWORD TH32CS_SNAPTHREAD = 4;
const DWORD TH32CS_SNAPMODULE = 8;
const DWORD TH32CS_SNAPMODULE32 = 16;

struct PROCESSENTRY32
{
  DWORD     dwSize;
  DWORD     cntUsage;
  DWORD     th32ProcessID;
  ULONG_PTR th32DefaultHeapID;
  DWORD     th32ModuleID;
  DWORD     cntThreads;
  DWORD     th32ParentProcessID;
  LONG      pcPriClassBase;
  DWORD     dwFlags;
  CHAR      szExeFile[MAX_PATH];
}

[extern "KERNEL32.DLL"]
BOOL Process32First (HANDLE hSnapshot, PROCESSENTRY32* lppe);

[extern "KERNEL32.DLL"]
BOOL Process32Next (HANDLE hSnapshot, PROCESSENTRY32* lppe);

[extern "KERNEL32.DLL"]
BOOL GetThreadContext (HANDLE hThread, CONTEXT* lpContext);

//----------------------------------------------------------------------------------------

struct MEMORY_BASIC_INFORMATION
{
  INT_PTR BaseAddress;
  INT_PTR AllocationBase;
  DWORD   AllocationProtect;
#if MEM64
  WORD    PartitionId;
#endif
  SIZE_T  RegionSize;
  DWORD   State;
  DWORD   Protect;
  DWORD   Type;
}

[extern "KERNEL32.DLL"]
SIZE_T VirtualQuery (INT_PTR addr, MEMORY_BASIC_INFORMATION *lpBuffer, SIZE_T dwLength);

[extern "KERNEL32.DLL"]
CHAR *GetCommandLineA();

//----------------------------------------------------------------------------------------

// netcard

const int MAXLEN_INTERFACE_NAME = 256;
const int MAXLEN_PHYSADDR       =   8;
const int MAXLEN_DESCR          = 256;

const uint IF_TYPE_OTHER              =   1;
const uint IF_TYPE_ETHERNET_CSMACD    =   6; // An Ethernet network interface.
const uint IF_TYPE_ISO88025_TOKENRING =   9; // A token ring network interface.
const uint IF_TYPE_PPP                =  23; // A PPP network interface.
const uint IF_TYPE_SOFTWARE_LOOPBACK  =  24; // A software loopback network interface.
const uint IF_TYPE_ATM                =  37; // An ATM network interface.
const uint IF_TYPE_IEEE80211          =  71; // An IEEE 802.11 wireless network interface.
const uint IF_TYPE_TUNNEL             = 131; // A tunnel type encapsulation network interface.
const uint IF_TYPE_IEEE1394           = 144; // An IEEE 1394 (Firewire) high performance serial bus network interface.

const uint IF_OPER_STATUS_NON_OPERATIONAL = 0; // LAN adapter has been disabled, for example because of an address conflict.
const uint IF_OPER_STATUS_UNREACHABLE     = 1; // WAN adapter that is not connected.
const uint IF_OPER_STATUS_DISCONNECTED    = 2; // For LAN adapters: network cable disconnected. For WAN adapters: no carrier.
const uint IF_OPER_STATUS_CONNECTING      = 3; // WAN adapter that is in the process of connecting.
const uint IF_OPER_STATUS_CONNECTED       = 4; // WAN adapter that is connected to a remote peer.
const uint IF_OPER_STATUS_OPERATIONAL     = 5; // Default status for LAN adapters

packed struct NETCARD
{
  wchar name[MAXLEN_INTERFACE_NAME];
  uint  index;
  uint  type;        // see IF_TYPE_xx constants
  uint  mtu;
  uint  speed;       // bits per sec (incorrect for very fast 10Gb/s network cards)
  uint  PhysAddrLen;
  byte  PhysAddr[MAXLEN_PHYSADDR];
  uint  AdminStatus;
  uint  OperStatus;  // see IF_OPER_xx constants
  uint  LastChange;
  uint  InOctets;
  uint  InUcastPkts;
  uint  InNUcastPkts;
  uint  InDiscards;
  uint  InErrors;
  uint  InUnknownProtos;
  uint  OutOctets;
  uint  OutUcastPkts;
  uint  OutNUcastPkts;
  uint  OutDiscards;
  uint  OutErrors;
  uint  OutQLen;
  uint  DescrLen;
  char  Descr[MAXLEN_DESCR];
}

typedef [callback]
int GETIFTABLE (byte* pIfTable, ULONG* pdwSize, BOOL bOrder);

// -----------------------------

// odbc

typedef int  HDBC;
typedef int  HSTMT;
typedef int  HENV;
typedef int  SQLHDBC;
typedef int  SQLHSTMT;
typedef int  SQLHENV;
typedef int  SQLHANDLE;

typedef int   SDWORD;
typedef int2  SWORD;
typedef int2  SQLRETURN;
typedef int2  RETCODE;
typedef int2  SQLSMALLINT;
typedef uint2 SQLUSMALLINT;
typedef int   SQLINTEGER;
typedef char  SQLCHAR;
typedef int   SQLLEN;
typedef uint  SQLULEN;

typedef byte* SQLPOINTER;

const int2 SQL_HANDLE_ENV   = 1;
const int2 SQL_HANDLE_DBC   = 2;
const int2 SQL_HANDLE_STMT  = 3;
const int2 SQL_HANDLE_DESC  = 4;

const int SQL_ATTR_ODBC_VERSION = 200;
const int SQL_ATTR_QUERY_TIMEOUT = 0;
const int SQL_IS_UINTEGER = (-5);

const int  SQL_NO_DATA = 100;
const int  SQL_NO_DATA_FOUND = 100;

const int2 SQL_NTS = -3;

const int SQL_SUCCESS           = 0;
const int SQL_SUCCESS_WITH_INFO = 1;

const int SQL_AUTOCOMMIT  = 102;
const int SQL_ATTR_AUTOCOMMIT = SQL_AUTOCOMMIT;

const int SQL_AUTOCOMMIT_OFF = 0;
const int SQL_AUTOCOMMIT_ON  = 1;

const int SQL_LOGIN_TIMEOUT = 103;

const int2 SQL_COMMIT        = 0;
const int2 SQL_ROLLBACK      = 1;

const uint2 SQL_CLOSE        = 0;
const uint2 SQL_UNBIND       = 2;
const uint2 SQL_RESET_PARAMS = 3;

// -----------------------------

[extern "odbc32.dll"]
SQLRETURN SQLGetDiagRecA ( SQLSMALLINT     HandleType,
                           SQLHANDLE       Handle,
                           SQLSMALLINT     RecNumber,
                           SQLCHAR *       SQLState,
                           SQLINTEGER *    NativeErrorPtr,
                           SQLCHAR *       MessageText,
                           SQLSMALLINT     BufferLength,
                           SQLSMALLINT *   TextLengthPtr);

[extern "odbc32.dll"]
SQLRETURN SQLAllocHandle(
	SQLSMALLINT  fHandleType,
	SQLHANDLE		 hInput,
	SQLHANDLE	   *phOutput);

[extern "odbc32.dll"]
SQLRETURN SQLSetEnvAttr(
     SQLHENV      EnvironmentHandle,
     SQLINTEGER   Attribute,
     SQLPOINTER   ValuePtr,
     SQLINTEGER   StringLength);

[extern "odbc32.dll"]
SQLRETURN SQLSetStmtAttr(
     SQLHSTMT      StatementHandle,
     SQLINTEGER    Attribute,
     SQLPOINTER    ValuePtr,
     SQLINTEGER    StringLength);

[extern "odbc32.dll"]
SQLRETURN SQLConnectA(
   SQLHDBC        ConnectionHandle,
   SQLCHAR *      ServerName,
   SQLSMALLINT    NameLength1,
   SQLCHAR *      UserName,
   SQLSMALLINT    NameLength2,
   SQLCHAR *      Authentication,
   SQLSMALLINT    NameLength3);

[extern "odbc32.dll"]
SQLRETURN SQLSetConnectAttr(
   SQLHDBC       ConnectionHandle,
   SQLINTEGER    Attribute,
   SQLPOINTER    ValuePtr,
   SQLINTEGER    StringLength);

[extern "odbc32.dll"]
SQLRETURN SQLEndTran(
     SQLSMALLINT   HandleType,
     SQLHANDLE     Handle,
     SQLSMALLINT   CompletionType);

[extern "odbc32.dll"]
SQLRETURN SQLFreeHandle(
     SQLSMALLINT   HandleType,
     SQLHANDLE     Handle);

[extern "odbc32.dll"]
SQLRETURN SQLDisconnect(SQLHDBC ConnectionHandle);

[extern "odbc32.dll"]
SQLRETURN SQLExecDirect(
     SQLHSTMT     StatementHandle,
     SQLCHAR *    StatementText,
     SQLINTEGER   TextLength);

[extern "odbc32.dll"]
SQLRETURN SQLFreeStmt(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Option);

[extern "odbc32.dll"]
SQLRETURN SQLBindCol(
      SQLHSTMT       StatementHandle,
      SQLUSMALLINT   ColumnNumber,
      SQLSMALLINT    TargetType,
      SQLPOINTER     TargetValuePtr,
      SQLLEN         BufferLength,
      SQLLEN *       StrLen_or_Ind);

[extern "odbc32.dll"]
SQLRETURN SQLFetch(
     SQLHSTMT     StatementHandle);

[extern "odbc32.dll"]
SQLRETURN SQLRowCount(
      SQLHSTMT   StatementHandle,
      SQLLEN *   RowCountPtr);

// -----------------------------

// services

packed struct SERVICE_STATUS  // 28 bytes
{
  uint          dwServiceType;
  uint          dwCurrentState;
  uint          dwControlsAccepted;
  uint          dwWin32ExitCode;
  uint          dwServiceSpecificExitCode;
  uint          dwCheckPoint;
  uint          dwWaitHint;
}

const uint SERVICE_WIN32_OWN_PROCESS =  0x00000010;
const uint SERVICE_ACCEPT_STOP       =  0x00000001;
const uint SERVICE_ACCEPT_SHUTDOWN   =  0x00000004;

const uint SERVICE_CONTROL_STOP        = 0x00000001;
const uint SERVICE_CONTROL_PAUSE       = 0x00000002;
const uint SERVICE_CONTROL_CONTINUE    = 0x00000003;
const uint SERVICE_CONTROL_INTERROGATE = 0x00000004;
const uint SERVICE_CONTROL_SHUTDOWN    = 0x00000005;

[extern "ADVAPI32.DLL"]
BOOL SetServiceStatus
  (HANDLE hServiceStatus,
   SERVICE_STATUS* lpServiceStatus);

[extern "ADVAPI32.DLL"]
HANDLE OpenSCManagerA
  (char* lpMachineName,
   char* lpDatabaseName,
   DWORD dwDesiredAccess);

[extern "ADVAPI32.DLL"]
BOOL CloseServiceHandle (HANDLE hSCObject);

const uint STANDARD_RIGHTS_REQUIRED      = 0xF0000;
const uint SC_MANAGER_CONNECT            = 0x00001;
const uint SC_MANAGER_CREATE_SERVICE     = 0x00002;
const uint SC_MANAGER_ENUMERATE_SERVICE  = 0x00004;
const uint SC_MANAGER_LOCK               = 0x00008;
const uint SC_MANAGER_QUERY_LOCK_STATUS  = 0x00010;
const uint SC_MANAGER_MODIFY_BOOT_CONFIG = 0x00020;

const uint SC_MANAGER_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED      |
                                   SC_MANAGER_CONNECT            |
                                   SC_MANAGER_CREATE_SERVICE     |
                                   SC_MANAGER_ENUMERATE_SERVICE  |
                                   SC_MANAGER_LOCK               |
                                   SC_MANAGER_QUERY_LOCK_STATUS  |
                                   SC_MANAGER_MODIFY_BOOT_CONFIG;

typedef [callback] void SERVICE_MAIN_FUNCTION (DWORD argc, CHAR** argv);

struct SERVICE_TABLE_ENTRY
{
  char*                 lpServiceName;
  SERVICE_MAIN_FUNCTION lpServiceProc;
}

[extern "ADVAPI32.DLL"]
BOOL StartServiceCtrlDispatcherA (SERVICE_TABLE_ENTRY* lpServiceTable);

typedef [callback] void HANDLER_FUNCTION (DWORD control);

[extern "ADVAPI32.DLL"]
HANDLE RegisterServiceCtrlHandlerA
  (char* lpServiceName,
   HANDLER_FUNCTION lpHandlerProc);

[extern "ADVAPI32.DLL"]
HANDLE CreateServiceA
                  (HANDLE hSCManager,
                    char* lpServiceName,
                    char* lpDisplayName,
                    DWORD  dwDesiredAccess,
                    DWORD  dwServiceType,
                    DWORD  dwStartType,
                    DWORD  dwErrorControl,
                    char* lpBinaryPathName,
                    char* lpLoadOrderGroup,
                    char* lpdwTagId,
                    char* lpDependencies,
                    char* lpServiceStartName,
                    char* lpPassword);

packed struct ENUM_SERVICE_STATUS
{
  char*          lpServiceName;
  char*          lpDisplayName;
  SERVICE_STATUS ServiceStatus;  // 28 bytes
#if MEM64
  byte[4]        filler;
#endif
}

[extern "ADVAPI32.DLL"]
BOOL EnumServicesStatusA (HANDLE    hSCManager,
                          DWORD     dwServiceType,
                          DWORD     dwServiceState,
                          ENUM_SERVICE_STATUS*  lpServices,
                          DWORD     cbBufSize,
                          DWORD*    pcbBytesNeeded,
                          DWORD*    lpServicesReturned,
                          DWORD*    lpResumeHandle);

[extern "ADVAPI32.DLL"]
HANDLE OpenServiceA
   (HANDLE hSCManager,
    char*  lpServiceName,
    DWORD  dwDesiredAccess);

[extern "ADVAPI32.DLL"]
BOOL DeleteService (HANDLE hService);

[extern "ADVAPI32.DLL"]
BOOL StartServiceA (HANDLE hService,
                    DWORD  dwNumServiceArgs,
                    CHAR** lpServiceArgVectors);

[extern "ADVAPI32.DLL"]
BOOL ControlService
  (HANDLE hService,
   DWORD dwControl,
   SERVICE_STATUS* lpServiceStatus);

packed struct QUERY_SERVICE_CONFIGA
{
  uint  dwServiceType;
  uint  dwStartType;
  uint  dwErrorControl;
#if MEM64
  uint  filler1;
#endif
  char* lpBinaryPathName;
  char* lpLoadOrderGroup;
  uint  dwTagId;
#if MEM64
  uint  filler2;
#endif
  char* lpDependencies;
  char* lpServiceStartName;
  char* lpDisplayName;
}

[extern "ADVAPI32.DLL"]
BOOL QueryServiceConfigA (SC_HANDLE              hService,
                          QUERY_SERVICE_CONFIGA* lpServiceConfig,
                          DWORD                  cbBufSize,
                          DWORD*                 pcbBytesNeeded);

[extern "ADVAPI32.DLL"]
BOOL ChangeServiceConfigA (SC_HANDLE   hService,
                             DWORD  dwServiceType,
                             DWORD  dwStartType,
                             DWORD  dwErrorControl,
                             CHAR* lpBinaryPathName,
                             CHAR* lpLoadOrderGroup,
                             CHAR* lpdwTagId,
                             CHAR* lpDependencies,
                             CHAR* lpServiceStartName,
                             CHAR* lpPassword,
                             CHAR* lpDisplayName);

const uint SERVICE_NO_CHANGE = 0xffffffff;

const uint SERVICE_ERROR_NORMAL = 1;
const uint SERVICE_AUTO_START   = 2;

const uint SERVICE_QUERY_CONFIG          = 0x0001;
const uint SERVICE_CHANGE_CONFIG         = 0x0002;
const uint SERVICE_QUERY_STATUS          = 0x0004;
const uint SERVICE_ENUMERATE_DEPENDENTS  = 0x0008;
const uint SERVICE_START                 = 0x0010;
const uint SERVICE_STOP                  = 0x0020;
const uint SERVICE_PAUSE_CONTINUE        = 0x0040;
const uint SERVICE_INTERROGATE           = 0x0080;
const uint SERVICE_USER_DEFINED_CONTROL  = 0x0100;

const uint SERVICE_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED     |
                                SERVICE_QUERY_CONFIG         |
                                SERVICE_CHANGE_CONFIG        |
                                SERVICE_QUERY_STATUS         |
                                SERVICE_ENUMERATE_DEPENDENTS |
                                SERVICE_START                |
                                SERVICE_STOP                 |
                                SERVICE_PAUSE_CONTINUE       |
                                SERVICE_INTERROGATE          |
                                SERVICE_USER_DEFINED_CONTROL;

// -----------------------------

// sound

packed struct WAVEFORMATEX   // should be packed otherwise it allocates 2 bytes too much at the end
{
  WORD  wFormatTag;
  WORD  nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD  nBlockAlign;
  WORD  wBitsPerSample;
  WORD  cbSize;
}

const WORD WAVE_FORMAT_PCM = 1;

struct WAVEHDR
{
  byte*    lpData;
  DWORD    dwBufferLength;
  DWORD    dwBytesRecorded;
  byte*    dwUser;
  DWORD    dwFlags;
  DWORD    dwLoops;   // nb of repeats for play
  WAVEHDR* lpNext;
  byte*    reserved;
}

const uint WHDR_DONE = 1;
const uint WHDR_ENDLOOP = 8;

typedef UINT MMRESULT;
typedef HANDLE HWAVEIN;
typedef HANDLE HWAVEOUT;
typedef UINT MMVERSION;
const uint WAVE_MAPPER = uint'max;

[extern "WINMM.DLL"]
MMRESULT waveInOpen (HWAVEIN*     phwi,
                     UINT         uDeviceID,
                     WAVEFORMATEX pwfx,
                     DWORD_PTR    dwCallback,
                     DWORD_PTR    dwCallbackInstance,
                     DWORD_PTR    fdwOpen);

[extern "WINMM.DLL"]
MMRESULT waveInStart (HWAVEIN hwi);

[extern "WINMM.DLL"]
MMRESULT waveInStop (HWAVEIN hwi);

[extern "WINMM.DLL"]
MMRESULT waveInReset (HWAVEIN hwi);

[extern "WINMM.DLL"]
MMRESULT waveInClose (HWAVEIN hwi);

[extern "WINMM.DLL"]
MMRESULT waveInPrepareHeader (HWAVEIN hwi, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveInUnprepareHeader (HWAVEIN hwi, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveInAddBuffer (HWAVEIN hwi, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveOutOpen (HWAVEOUT*     phwo,
                      UINT          uDeviceID,
                      WAVEFORMATEX* pwfx,
                      DWORD_PTR     dwCallback,
                      DWORD_PTR     dwCallbackInstance,
                      DWORD_PTR     fdwOpen);

[extern "WINMM.DLL"]
MMRESULT waveOutPrepareHeader (HWAVEOUT hwo, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveOutUnprepareHeader (HWAVEOUT hwo, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveOutWrite (HWAVEOUT hwo, WAVEHDR* pwh, UINT cbwh);

[extern "WINMM.DLL"]
MMRESULT waveOutPause (HWAVEOUT hwo);

[extern "WINMM.DLL"]
MMRESULT waveOutRestart (HWAVEOUT hwo);

[extern "WINMM.DLL"]
MMRESULT waveOutReset (HWAVEOUT hwo);

[extern "WINMM.DLL"]
MMRESULT waveOutClose (HWAVEOUT hwo);

[extern "WINMM.DLL"]
UINT waveInGetNumDevs();

[extern "WINMM.DLL"]
UINT waveOutGetNumDevs();


const int MAXPNAMELEN = 32;   /* max product name length (including NULL) */

struct WAVEINCAPSW
{
  WORD      wMid;
  WORD      wPid;
  MMVERSION vDriverVersion;
  WCHAR     szPname[MAXPNAMELEN];
  DWORD     dwFormats;
  WORD      wChannels;
  WORD      wReserved1;
}

[extern "WINMM.DLL"]
MMRESULT waveInGetDevCapsW (UINT uDeviceID, WAVEINCAPSW* pwic, UINT cbwic);


struct WAVEOUTCAPSW
{
  WORD      wMid;
  WORD      wPid;
  MMVERSION vDriverVersion;
  WCHAR     szPname[MAXPNAMELEN];
  DWORD     dwFormats;
  WORD      wChannels;
  WORD      wReserved1;
  DWORD     dwSupport;
}

[extern "WINMM.DLL"]
MMRESULT waveOutGetDevCapsW (UINT uDeviceID, WAVEOUTCAPSW* pwoc, UINT cbwoc);

// -----------------------------

// system

[extern "KERNEL32.DLL"]
BOOL GetComputerNameA (LPSTR lpBuffer, LPDWORD nSize);

[extern "KERNEL32.DLL"]
BOOL GetComputerNameW (LPWSTR lpBuffer, LPDWORD nSize);

[extern "Advapi32.dll"]
BOOL GetUserNameA (LPSTR lpBuffer, LPDWORD pcbBuffer);

[extern "Advapi32.dll"]
BOOL GetUserNameW (LPWSTR lpBuffer, LPDWORD pcbBuffer);

struct MEMORYSTATUSEX
{
  uint dwLength;
  uint dwMemoryLoad;
  long ullTotalPhys;
  long ullAvailPhys;
  long ullTotalPageFile;
  long ullAvailPageFile;
  long ullTotalVirtual;
  long ullAvailVirtual;
  long ullAvailExtendedVirtual;
}

[extern "KERNEL32.DLL"]
BOOL GlobalMemoryStatusEx (MEMORYSTATUSEX* lpBuffer);

struct SYSTEM_INFO
{
  uint2 wProcessorArchitecture;
  uint2 wReserved;
  uint  dwPageSize;
  byte* lpMinimumApplicationAddress;
  byte* lpMaximumApplicationAddress;
  byte* dwActiveProcessorMask;
  uint  dwNumberOfProcessors;
  uint  dwProcessorType;
  uint  dwAllocationGranularity;
  uint2 wProcessorLevel;
  uint2 wProcessorRevision;
}

[extern "KERNEL32.DLL"]
void GetSystemInfo (SYSTEM_INFO* lpSystemInfo);

[extern "KERNEL32.DLL"]
WORD GetUserDefaultLangID ();

[extern "KERNEL32.DLL"]
UINT GetWindowsDirectoryA (CHAR* lpBuffer, UINT uSize);


typedef [callback] int GetUserDefaultGeoName (wchar* geoName, int geoNameCount);


struct LASTINPUTINFO
{
  UINT  cbSize;
  DWORD dwTime;
}

[extern "User32.dll"]
BOOL GetLastInputInfo (LASTINPUTINFO* p);

// -----------------------------

// tcpip

struct WSADATA
{
  WORD  wVersion;
  WORD  wHighVersion;
#if MEM64
  uint2  iMaxSockets;
  uint2  iMaxUdpDg;
  CHAR   *lpVendorInfo;
  char   szDescription[256+1];
  char   szSystemStatus[128+1];
#else
  char   szDescription[256+1];
  char   szSystemStatus[128+1];
  uint2  iMaxSockets;
  uint2  iMaxUdpDg;
  CHAR   *lpVendorInfo;
#endif
}

[extern "ws2_32.dll"]
int WSAStartup (WORD version, WSADATA* data);

const int2 AF_INET   =  2; // IPV4
const int2 AF_INET6  = 23; // IPV6

const int SOCK_STREAM = 1;  // STREAM
const int SOCK_DGRAM  = 2;  // DATAGRAM

// level
const int IPPROTO_IP = 0; 
const int IPPROTO_IPV6 = 41;

const int IPPROTO_TCP = 6;  // TCPIP
const int IPPROTO_UDP = 17; // UDP

const int IP_PKTINFO   = 19; // enable return of packet information by WSARecvMsg() on IPv4
const int IPV6_PKTINFO = 19; // enable return of packet information by WSARecvMsg() on IPv6

typedef INT_PTR SOCKET;

[extern "ws2_32.dll"]
SOCKET socket (int af, int type, int protocol);

const int SOL_SOCKET   = 0xffff;
const int SO_SNDBUF    = 0x1001;
const int SO_RCVBUF    = 0x1002;
const int SO_KEEPALIVE = 0x0008;
const int TCP_NODELAY  = 1;

[extern "ws2_32.dll"]
int setsockopt (SOCKET s, int level, int optname, byte *val, uint len);

[extern "ws2_32.dll"]
int closesocket (SOCKET s);

packed struct sockaddr_in
{
  int2    sin_family;  // AF_INET
  uint2   sin_port;
  byte[4] sin_addr;
  byte[8] sin_zero;
}

packed struct sockaddr_in6
{
  int2     sin6_family;  // AF_INET6
  uint2    sin6_port;
  uint4    sin6_flowinfo;
  byte[16] sin6_addr;
  uint4    sin6_scope_id;
}

const int EINVAL        = 10022;
const int EWOULDBLOCK   = 10035;
const int EDESTADDRREQ  = 10039;
const int EADDRINUSE    = 10048;
const int EADDRNOTAVAIL = 10049;
const int ENETDOWN      = 10050;
const int ENETUNREACH   = 10051;
const int ENETRESET     = 10052;
const int ECONNABORTED  = 10053;
const int ECONNRESET    = 10054;
const int ETIMEDOUT     = 10060;
const int ECONNREFUSED  = 10061;
const int EHOSTDOWN     = 10064;
const int EHOSTUNREACH  = 10065;

[extern "ws2_32.dll"]
int connect (SOCKET s, byte *addr, uint len);

[extern "ws2_32.dll"]
int send (SOCKET s, byte *buf, uint len, int flags);

[extern "ws2_32.dll"]
int sendto (SOCKET s, byte *buf, uint len, int flags, byte *to, uint *tolen);

[extern "ws2_32.dll"]
int recv (SOCKET s, byte *buf, uint len, int flags);

[extern "ws2_32.dll"]
int recvfrom (SOCKET s, byte *buf, uint len, int flags, byte *frm, uint *fromlen);

[extern "ws2_32.dll"]
int bind (SOCKET s, byte *addr, uint len);

[extern "ws2_32.dll"]
int getsockname (SOCKET s, byte *sockaddr, uint *len);

[extern "ws2_32.dll"]
int getpeername (SOCKET s, byte *sockaddr, uint *len);

[extern "ws2_32.dll"]
int listen (SOCKET s, int backlog);

[extern "ws2_32.dll"]
SOCKET accept (SOCKET s, byte *addr, uint *len);

const int ENOMEM = 12;

typedef HANDLE WSAEVENT;

[extern "ws2_32.dll"]
WSAEVENT WSACreateEvent();

const uint FD_READ    = (1 << 0);
const uint FD_WRITE   = (1 << 1);
const uint FD_ACCEPT  = (1 << 3);
const uint FD_CONNECT = (1 << 4);
const uint FD_CLOSE   = (1 << 5);

[extern "ws2_32.dll"]
int WSAEventSelect (SOCKET s, WSAEVENT v, uint mask);

[extern "ws2_32.dll"]
BOOL WSASetEvent (WSAEVENT v);

[extern "ws2_32.dll"]
BOOL WSAResetEvent (WSAEVENT v);

[extern "ws2_32.dll"]
DWORD WSAWaitForMultipleEvents
  (DWORD cEvents,
   WSAEVENT *hEvents,
   BOOL fWaitAll,
   DWORD dwTimeout,
   BOOL fAlertable);

const int WSA_WAIT_TIMEOUT = 0x102;
const int FD_MAX_EVENTS = 10;

struct WSANETWORKEVENTS
{
  int lNetworkEvents;
  int iErrorCode[FD_MAX_EVENTS];
}

[extern "ws2_32.dll"]
int WSAEnumNetworkEvents (SOCKET s, WSAEVENT v, WSANETWORKEVENTS* lpNetworkEvents);

[extern "ws2_32.dll"]
BOOL WSACloseEvent (WSAEVENT v);

struct ADDRINFOA
{
  int       ai_flags;
  int       ai_family;
  int       ai_socktype;
  int       ai_protocol;
  SIZE_T    ai_addrlen;
  char      *ai_canonname;
  byte      *ai_addr;
  ADDRINFOA *ai_next;
}

// for function "getaddrinfo"
typedef [callback] INT GETADDRINFO (CHAR *NodeName, CHAR *ServiceName, ADDRINFOA *pHints, ADDRINFOA **Result);
typedef [callback] void FREEADDRINFO (ADDRINFOA *p);

struct hostent
{
  char  *h_name;
  char  **h_aliases;
  int2  h_addrtype;
  uint2 h_length;
  byte  **h_addr_list;
}

[extern "ws2_32.dll"]
hostent* gethostbyname (char* namez);

[extern "ws2_32.dll"]
uint inet_addr (char *ipstr);

typedef int socklen_t;

typedef [callback] INT GETNAMEINFO
  (byte *addr,
   socklen_t addr_len,
   CHAR *host,
   DWORD hostlen,
   CHAR* serv,
   DWORD servlen,
   INT flags);

[extern "ws2_32.dll"]
hostent* gethostbyaddr (byte *addr, uint len, int type);


[extern "Ws2_32.dll"]
int WSAIoctl
 (SOCKET   s,
  DWORD    dwIoControlCode,
  LPVOID   lpvInBuffer,
  DWORD    cbInBuffer,
  LPVOID   lpvOutBuffer,
  DWORD    cbOutBuffer,
  LPDWORD  lpcbBytesReturned,
  byte*    lpOverlapped = null,
  byte*    lpCompletionRoutine = null);

const DWORD SIO_GET_EXTENSION_FUNCTION_POINTER = 0xC8000006;

const REFIID WSAID_WSARECVMSG = {0xf689d7c8,0x6f1f,0x436b,0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22};
const REFIID WSAID_WSASENDMSG = {0xa441e712,0x754f,0x43ca,0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d};


packed struct WSABUF   // 8 or 16 bytes, align 4 or 8
{
  uint  len;   // the length of the buffer
#if MEM64
  uint  filler;
#endif
  byte* buf;   // the pointer to the buffer
}

packed struct WSAMSG  // 28 or 56 bytes
{
  byte*    name;           // Remote address (SOCKADDR  128 bytes)
  uint     namelen;        // Remote address length
#if MEM64
  uint     filler;
#endif
  WSABUF*  lpBuffers;      // Data buffer array
  uint     dwBufferCount;  // Number of elements in the array
#if MEM64
  uint     filler2;
#endif
  WSABUF   Control;        // Control buffer (40 bytes is enough for IP_PKTINFO or IPV6_PKTINFO)
  uint     dwFlags;        // Flags
#if MEM64
  uint     filler3;
#endif
}

packed struct WSACMSGHDR  // 12 or 16, align 4 or 8
{
  SIZE_T      cmsg_len;   // 4 or 8 bytes
  int         cmsg_level;
  int         cmsg_type;
  // followed by UCHAR cmsg_data[]
}


typedef [callback] 
int WSARecvmsg 
 (SOCKET   s,
  WSAMSG*  lpMsg,
  uint4*   lpdwNumberOfBytesRecvd,
  byte*    lpOverlapped = null,
  byte*    lpCompletionRoutine = null);


typedef [callback] 
int WSASendMsg
 (SOCKET   s,
  WSAMSG*  lpMsg,
  uint4    dwFlags,
  uint4*   lpNumberOfBytesSent,
  byte*    lpOverlapped = null,
  byte*    lpCompletionRoutine = null);

//-----------------------------------------------------

// thread

[extern "kernel32.dll"]
HMODULE GetModuleHandleA (LPCSTR lpModuleName);

[extern "KERNEL32.dll"]
HMODULE GetModuleHandleW (LPCWSTR lpModuleName);

[extern "KERNEL32.DLL"]
DWORD GetModuleFileNameA (HMODULE hModule, LPSTR Filename, DWORD nSize);

[extern "KERNEL32.DLL"]
DWORD GetModuleFileNameW (HMODULE hModule, LPWSTR Filename, DWORD nSize);

[extern "KERNEL32.DLL"]
HMODULE LoadLibraryA (char* lpFileName);

[extern "KERNEL32.DLL"]
HMODULE LoadLibraryExA (char* lpLibFileName, HANDLE hFile, DWORD dwFlags);

[extern "KERNEL32.DLL"]
BOOL FreeLibrary (HMODULE hModule);

[extern "KERNEL32.DLL"]
LPVOID GetProcAddress (HMODULE hModule, LPCSTR lpProcName);

[extern "KERNEL32.DLL"]
DWORD GetTickCount ();

struct CRITICAL_SECTION
{
#if MEM32
  byte[24] inside;
#else
  byte[40] inside;
#endif
}

[extern "KERNEL32.DLL"]
void InitializeCriticalSection (CRITICAL_SECTION* lpCriticalSection);

[extern "KERNEL32.DLL"]
void EnterCriticalSection (CRITICAL_SECTION* lpCriticalSection);

[extern "KERNEL32.DLL"]
void LeaveCriticalSection (CRITICAL_SECTION* lpCriticalSection);

[extern "KERNEL32.DLL"]
void DeleteCriticalSection (CRITICAL_SECTION* lpCriticalSection);

[extern "KERNEL32.dll"]
HANDLE GetCurrentThread();

[extern "KERNEL32.DLL"]
DWORD GetCurrentThreadId ();

[extern "KERNEL32.dll"]
HANDLE GetCurrentProcess ();

[extern "KERNEL32.DLL"]
DWORD GetCurrentProcessId ();

enum PROCESS_INFORMATION_CLASS { ProcessMemoryPriority,  ProcessMemoryExhaustionInfo,  ProcessAppMemoryInfo,
  ProcessInPrivateInfo,  ProcessPowerThrottling,  ProcessReservedValue1,  ProcessTelemetryCoverageInfo,
  ProcessProtectionLevelInfo,  ProcessLeapSecondInfo,  ProcessMachineTypeInfo,  ProcessOverrideSubsequentPrefetchParameter,
  ProcessMaxOverridePrefetchParameter,  ProcessInformationClassMax };

struct PROCESS_POWER_THROTTLING_STATE 
{
  ULONG Version;
  ULONG ControlMask;
  ULONG StateMask;
}

const uint PROCESS_POWER_THROTTLING_CURRENT_VERSION = 1;
const uint PROCESS_POWER_THROTTLING_EXECUTION_SPEED = 1;
const uint PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION = 2;

typedef [callback]
BOOL FSetProcessInformation(
  HANDLE                    hProcess,
  PROCESS_INFORMATION_CLASS ProcessInformationClass,
  LPVOID                    ProcessInformation,
  DWORD                     ProcessInformationSize);

[extern "KERNEL32.DLL"]
void GetSystemTimeAsFileTime (int8* lpSystemTimeAsFileTime);

[extern "KERNEL32.dll"]
BOOL QueryPerformanceFrequency (int8* lpFrequency);

[extern "KERNEL32.dll"]
BOOL QueryPerformanceCounter (int8* lpPerformanceCount);

[extern "KERNEL32.DLL"]
void Sleep (DWORD milliseconds);

[extern "KERNEL32.DLL"]
int GetThreadPriority (HANDLE nThread);

[extern "KERNEL32.DLL"]
BOOL SetThreadPriority (HANDLE nThread, BOOL priority);  // returns non-zero if success

[extern "kernel32.dll"]
BOOL TerminateThread (HANDLE hThread, DWORD dwExitCode);

[extern "kernel32.dll"]
BOOL DuplicateHandle (HANDLE   hSourceProcessHandle,
                      HANDLE   hSourceHandle,
                      HANDLE   hTargetProcessHandle,
                      HANDLE*  lpTargetHandle,
                      DWORD    dwDesiredAccess,
                      BOOL     bInheritHandle,
                      DWORD    dwOptions);

const DWORD DUPLICATE_SAME_ACCESS = 0x00000002;


typedef byte* LPSECURITY_ATTRIBUTES;

const BOOL FALSE = 0;
const BOOL TRUE = 1;

const WORD SW_HIDE            = 0;
const WORD SW_SHOWNORMAL      = 1;
const WORD SW_NORMAL          = 1;
const WORD SW_SHOWMINIMIZED   = 2;
const WORD SW_SHOWMAXIMIZED   = 3;
const WORD SW_MAXIMIZE        = 3;
const WORD SW_SHOWNOACTIVATE  = 4;
const WORD SW_SHOW            = 5;
const WORD SW_MINIMIZE        = 6;
const WORD SW_SHOWMINNOACTIVE = 7;
const WORD SW_SHOWNA          = 8;
const WORD SW_RESTORE         = 9;
const WORD SW_SHOWDEFAULT     = 10;
const WORD SW_FORCEMINIMIZE   = 11;

struct STARTUPINFOA
{
  DWORD   cb;
  LPSTR   lpReserved;
  LPSTR   lpDesktop;
  LPSTR   lpTitle;
  DWORD   dwX;
  DWORD   dwY;
  DWORD   dwXSize;
  DWORD   dwYSize;
  DWORD   dwXCountChars;
  DWORD   dwYCountChars;
  DWORD   dwFillAttribute;
  DWORD   dwFlags;
  WORD    wShowWindow;
  WORD    cbReserved2;
  LPBYTE  lpReserved2;
  HANDLE  hStdInput;
  HANDLE  hStdOutput;
  HANDLE  hStdError;
}

struct PROCESS_INFORMATION
{
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
}

[extern "KERNEL32.DLL"]
BOOL CreateProcessA (LPCSTR lpApplicationName,
                     LPCSTR lpCommandLine,
                     LPSECURITY_ATTRIBUTES lpProcessAttributes,
                     LPSECURITY_ATTRIBUTES lpThreadAttributes,
                     BOOL bInheritHandles,
                     DWORD dwCreationFlags,
                     LPVOID lpEnvironment,
                     LPCSTR lpCurrentDirectory,
                     STARTUPINFOA* lpStartupInfo,
                     PROCESS_INFORMATION* lpProcessInformation);

[extern "KERNEL32.DLL"]
BOOL CreateProcessW (LPCWSTR lpApplicationName,
                     LPCWSTR lpCommandLine,
                     LPSECURITY_ATTRIBUTES lpProcessAttributes,
                     LPSECURITY_ATTRIBUTES lpThreadAttributes,
                     BOOL bInheritHandles,
                     DWORD dwCreationFlags,
                     LPVOID lpEnvironment,
                     LPCWSTR lpCurrentDirectory,
                     STARTUPINFOA* lpStartupInfo,
                     PROCESS_INFORMATION* lpProcessInformation);

[extern "USER32.DLL"]
DWORD WaitForInputIdle (HANDLE hProcess, DWORD dwMilliseconds);


[extern "KERNEL32.DLL"]
HANDLE CreateSemaphoreA
  (LPSECURITY_ATTRIBUTES EventAttributes,
   LONG lInitialCount,
   LONG lMaximumCount,
   CHAR* name);

[extern "KERNEL32.DLL"]
BOOL ReleaseSemaphore
  (HANDLE hSemaphore,
   LONG lReleaseCount,
   LONG* lpPreviousCount);

[extern "KERNEL32.DLL"]
DWORD WaitForMultipleObjectsEx
  (DWORD   count,
   HANDLE* handles,
   BOOL fWaitAll,
   DWORD dwTimeout,
   BOOL alertable);

[extern "Winmm.dll"]
MMRESULT timeBeginPeriod (UINT msecs);

[extern "Winmm.dll"]
MMRESULT timeEndPeriod (UINT msecs);

// -----------------------------

const DWORD DWMWA_CLOAK = 13;

[extern "Dwmapi.DLL"]
HRESULT DwmSetWindowAttribute (HWND hwnd, DWORD dwAttribute, byte* pvAttribute, DWORD cbAttribute);

typedef [callback] HRESULT DWMSETWINDOWATTRIBUTE (HWND hwnd, DWORD dwAttribute, byte* pvAttribute, DWORD cbAttribute);

// -----------------------------

// registry

typedef LONG LSTATUS;

[extern "Advapi32.dll"]
LSTATUS RegCreateKeyExA
    (HKEY hKey,
     char* lpSubKey,
     DWORD Reserved,
     char* lpClass,
     DWORD dwOptions,
     DWORD samDesired,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
     HKEY* phkResult,
     DWORD* lpdwDisposition);

[extern "Advapi32.dll"]
LSTATUS RegOpenKeyExA
     (HKEY hKey,
      CHAR* lpSubKey,
      DWORD ulOptions,
      DWORD samDesired,
      HKEY* phkResult);

[extern "Advapi32.dll"]
LSTATUS RegSetValueExA
   (HKEY hKey,
    CHAR* lpValueName,
    DWORD Reserved,
    DWORD dwType,
    BYTE* lpData,
    DWORD cbData);

[extern "Advapi32.dll"]
LSTATUS RegQueryValueExA
   (HKEY hKey,
    CHAR* lpValueName,
    DWORD* lpReserved,
    DWORD* lpType,
    BYTE* lpData,
    DWORD* lpcbData);

[extern "Advapi32.dll"]
LSTATUS RegCloseKey (HKEY hKey);

[extern "Advapi32.dll"]
LSTATUS RegEnumKeyExA
   (HKEY hKey,
    DWORD dwIndex,
    LPSTR lpName,
    LPDWORD lpcchName,
    LPDWORD lpReserved,
    LPSTR lpClass,
    LPDWORD lpcchClass,
    FILETIME* lpftLastWriteTime);

// -----------------------------

// memory

[extern "kernel32.dll"]
HANDLE GetProcessHeap();   // returns heap

[extern "kernel32.dll"]
byte* HeapAlloc (HANDLE heap, DWORD flags, SIZE_T size);   // flags = 4 == generate exceptions

[extern "kernel32.dll"]
BOOL HeapFree (HANDLE heap, DWORD flags, byte* mem);

[extern "kernel32.dll"]
SIZE_T HeapSize (HANDLE heap, DWORD dwFlags, byte* mem);

// -----------------------------

typedef [callback]
void TIMERPROC (HWND hwnd, UINT msg, UINT_PTR wparam, DWORD lparam);

// dummy declarations
typedef byte* MENUTEMPLATEA;
typedef byte* MENUTEMPLATEW;
typedef byte* RGNDATA;


const HRESULT NOERROR       = 0;
const HRESULT E_NOINTERFACE = 0x80004002 - 0x100000000;
const HRESULT E_POINTER     = 0x80004003 - 0x100000000;
const HRESULT E_FAIL        = 0x80004005 - 0x100000000;
const HRESULT E_NOTIMPL     = 0x80004001 - 0x100000000;

const REFIID IID_IUnknown = {0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

typedef [callback] HRESULT FQueryInterface (LPVOID This,  REFIID riid,  out LPVOID ppvObject);
typedef [callback] ULONG   FAddRef         (LPVOID This);
typedef [callback] ULONG   FRelease        (LPVOID This);
typedef [callback] ULONG   FDummy          (LPVOID This);

struct IUnknownVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
}

struct IUnknown
{
  IUnknownVtbl* lpVtbl;
}

void SafeRelease (LPVOID **ppT);

typedef byte* LPUNKNOWN;

[extern "OLE32.DLL"]
HRESULT CoCreateInstance (REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, out LPVOID ppv);

const DWORD CLSCTX_INPROC_SERVER  = 1;
const DWORD CLSCTX_INPROC_HANDLER = 2;
const DWORD CLSCTX_LOCAL_SERVER   = 4;
const DWORD CLSCTX_REMOTE_SERVER  = 16;
const DWORD CLSCTX_ALL            = (CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER|CLSCTX_LOCAL_SERVER|CLSCTX_REMOTE_SERVER);
const DWORD CLSCTX_INPROC         = (CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER);

[extern "OLE32.DLL"]
HRESULT CoInitializeEx (LPVOID ptr, DWORD mode);

const DWORD COINIT_MULTITHREADED     = 0x0;
const DWORD COINIT_APARTMENTTHREADED = 0x2; // Apartment model
const DWORD COINIT_DISABLE_OLE1DDE   = 0x4; // Don't use DDE for Ole1 support
const DWORD COINIT_SPEED_OVER_MEMORY = 0x8; // Trade memory for speed

const int S_OK = 0;
const int S_FALSE = 1;

[extern "OLE32.DLL"]
void CoUninitialize ();

[extern "OLE32.DLL"]
void CoTaskMemFree (LPVOID pv);



typedef [callback]
LRESULT WNDPROC (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

const int IDC_ARROW = 32512;
const int IDC_IBEAM = 32513;
const int IDC_WAIT  = 32514;
const int IDC_CROSS = 32515;
const int IDC_NO    = 32648;
const int IDC_HAND  = 32649;

const INT_PTR IDI_APPLICATION = 32512;

const uint WS_OVERLAPPED      = 0x00000000;
const uint WS_POPUP           = 0x80000000;
const uint WS_CHILD           = 0x40000000;
const uint WS_MINIMIZE        = 0x20000000;
const uint WS_VISIBLE         = 0x10000000;
const uint WS_DISABLED        = 0x08000000;
const uint WS_CLIPSIBLINGS    = 0x04000000;
const uint WS_CLIPCHILDREN    = 0x02000000;
const uint WS_MAXIMIZE        = 0x01000000;
const uint WS_CAPTION         = 0x00C00000;
const uint WS_BORDER          = 0x00800000;
const uint WS_DLGFRAME        = 0x00400000;
const uint WS_VSCROLL         = 0x00200000;
const uint WS_HSCROLL         = 0x00100000;
const uint WS_SYSMENU         = 0x00080000;
const uint WS_THICKFRAME      = 0x00040000;
const uint WS_GROUP           = 0x00020000;
const uint WS_TABSTOP         = 0x00010000;
const uint WS_MINIMIZEBOX     = 0x00020000;
const uint WS_MAXIMIZEBOX     = 0x00010000;
const uint WS_TILED           = WS_OVERLAPPED;
const uint WS_ICONIC          = WS_MINIMIZE;
const uint WS_SIZEBOX         = WS_THICKFRAME;
const uint WS_OVERLAPPEDWINDOW= (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
const uint WS_TILEDWINDOW     = WS_OVERLAPPEDWINDOW;
const uint WS_POPUPWINDOW     = (WS_POPUP | WS_BORDER | WS_SYSMENU);
const uint WS_CHILDWINDOW     = (WS_CHILD);

const int CW_USEDEFAULT      = int'min;

[extern "user32.dll"]
HCURSOR LoadCursorA (HINSTANCE hinstance, INT_PTR lpCursorName);

[extern "user32.dll"]
HCURSOR LoadCursorW (HINSTANCE hinstance, INT_PTR lpCursorName);

[extern "user32.dll"]
HICON LoadIconA (HINSTANCE hinstance, INT_PTR lpCursorName);

[extern "user32.dll"]
HICON LoadIconW (HINSTANCE hinstance, INT_PTR lpCursorName);

[extern "comctl32.dll"]
void InitCommonControls();

packed struct INITCOMMONCONTROLSEX
{
  DWORD dwSize;
  DWORD dwICC;
}

const DWORD ICC_LISTVIEW_CLASSES = 0x00000001; // listview, header
const DWORD ICC_TREEVIEW_CLASSES = 0x00000002; // treeview, tooltips
const DWORD ICC_BAR_CLASSES      = 0x00000004; // toolbar, statusbar, trackbar, tooltips
const DWORD ICC_TAB_CLASSES      = 0x00000008; // tab, tooltips
const DWORD ICC_UPDOWN_CLASS     = 0x00000010; // updown
const DWORD ICC_PROGRESS_CLASS   = 0x00000020; // progress
const DWORD ICC_HOTKEY_CLASS     = 0x00000040; // hotkey
const DWORD ICC_ANIMATE_CLASS    = 0x00000080; // animate
const DWORD ICC_WIN95_CLASSES    = 0x000000FF;
const DWORD ICC_DATE_CLASSES     = 0x00000100; // month picker, date picker, time picker, updown
const DWORD ICC_USEREX_CLASSES   = 0x00000200; // comboex
const DWORD ICC_COOL_CLASSES     = 0x00000400; // rebar (coolbar) control
const DWORD ICC_INTERNET_CLASSES = 0x00000800;
const DWORD ICC_PAGESCROLLER_CLASS = 0x00001000;   // page scroller
const DWORD ICC_NATIVEFNTCTL_CLASS = 0x00002000;   // native font control

[extern "comctl32.dll"]
BOOL InitCommonControlsEx (INITCOMMONCONTROLSEX* lpInitCtrls);

struct WNDCLASSA
{
  uint      style;
  WNDPROC   lpfnWndProc;
  int       cbClsExtra;
  int       cbWndExtra;
  HINSTANCE hInstance;
  HICON     hIcon;
  HCURSOR   hCursor;
  HBRUSH    hbrBackground;
  LPCSTR    lpszMenuName;
  LPCSTR    lpszClassName;
}

[extern "user32.dll"]
ATOM RegisterClassA (WNDCLASSA* wc);

struct WNDCLASSEXA
{
  UINT        cbSize;
  UINT        style;
  WNDPROC     lpfnWndProc;
  int         cbClsExtra;
  int         cbWndExtra;
  HINSTANCE   hInstance;
  HICON       hIcon;
  HCURSOR     hCursor;
  HBRUSH      hbrBackground;
  LPCSTR      lpszMenuName;
  LPCSTR      lpszClassName;
  HICON       hIconSm;
}

[extern "user32.dll"]
ATOM RegisterClassExA (WNDCLASSEXA* wc);

struct WNDCLASSEXW
{
    UINT        cbSize;
    UINT        style;
    WNDPROC     lpfnWndProc;
    int         cbClsExtra;
    int         cbWndExtra;
    HINSTANCE   hInstance;
    HICON       hIcon;
    HCURSOR     hCursor;
    HBRUSH      hbrBackground;
    LPCWSTR     lpszMenuName;
    LPCWSTR     lpszClassName;
    HICON       hIconSm;
}

[extern "user32.dll"]
ATOM RegisterClassExW (WNDCLASSEXW* wc);

[extern "user32.dll"]
HWND CreateWindowExA
  (DWORD exstyles,
   CHAR* class,
   CHAR* title,
   DWORD style,
   int x,
   int y,
   int width,
   int height,
   HWND parent,
   HMENU menu,
   HINSTANCE hinstance,
   LPVOID lparam);

[extern "user32.dll"]
HWND CreateWindowExW
  (DWORD exstyles,
   wchar *class,
   wchar *title,
   DWORD style,
   int x,
   int y,
   int width,
   int height,
   HWND parent,
   HMENU menu,
   HINSTANCE hinstance,
   LPVOID lparam);

[extern "user32.dll"]
BOOL ShowWindow (HWND hwnd, int nCmdShow);

[extern "user32.dll"]
BOOL EnableWindow (HWND hWnd, BOOL bEnable);

[extern "user32.dll"]
BOOL SetForegroundWindow (HWND hwnd);

[extern "user32.dll"]
HWND SetFocus(HWND hWnd);

[extern "user32.dll"]
HWND SetActiveWindow(HWND hWnd);

packed struct RECT
{
  int left;
  int top;
  int right;
  int bottom;
}

packed struct COLORKEY
{
  DWORD KeyType;
  DWORD PaletteIndex;
  COLORREF LowColorValue;
  COLORREF HighColorValue;
}

[extern "user32.dll"]
BOOL InvalidateRgn (HWND hWnd, HRGN hRgn, BOOL bErase);

[extern "user32.dll"]
BOOL ValidateRgn (HWND hWnd, HRGN hRgn);

[extern "user32.dll"]
BOOL RedrawWindow (HWND hWnd, RECT *lprcUpdate, HRGN hrgnUpdate, UINT flags);


const uint RDW_INVALIDATE        =  0x0001;
const uint RDW_INTERNALPAINT     =  0x0002;
const uint RDW_ERASE             =  0x0004;
const uint RDW_VALIDATE          =  0x0008;
const uint RDW_NOINTERNALPAINT   =  0x0010;
const uint RDW_NOERASE           =  0x0020;
const uint RDW_NOCHILDREN        =  0x0040;
const uint RDW_ALLCHILDREN       =  0x0080;
const uint RDW_UPDATENOW         =  0x0100;
const uint RDW_ERASENOW          =  0x0200;
const uint RDW_FRAME             =  0x0400;
const uint RDW_NOFRAME           =  0x0800;


[extern "user32.dll"]
LRESULT DefWindowProcA (HWND hWnd, UINT message, WPARAM wParam, LPARAM lPparam);

[extern "user32.dll"]
LRESULT DefWindowProcW (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

const int GWL_WNDPROC       =  (-4);
const int GWL_HINSTANCE     =  (-6);
const int GWL_HWNDPARENT    =  (-8);
const int GWL_STYLE         =  (-16);
const int GWL_EXSTYLE       =  (-20);
const int GWL_USERDATA      =  (-21);
const int GWL_ID            =  (-12);

[extern "user32.dll"]
LONG GetWindowLongA (HWND hWnd, int nIndex);

[extern "user32.dll"]
LONG GetWindowLongW (HWND hWnd, int nIndex);

[extern "user32.dll"]
LONG SetWindowLongA (HWND hWnd, int nIndex, LONG dwNewLong);

[extern "user32.dll"]
LONG SetWindowLongW (HWND hWnd, int nIndex, LONG dwNewLong);

#if MEM64
  [extern "user32.dll"]
  PVOID GetWindowLongPtrA (HWND hWnd, int nIndex);

  [extern "user32.dll"]
  PVOID SetWindowLongPtrA (HWND hWnd, int nIndex, PVOID dwNewLong);
#else
  PVOID GetWindowLongPtrA (HWND hWnd, int nIndex);
  PVOID SetWindowLongPtrA (HWND hWnd, int nIndex, PVOID dwNewLong);
#endif

[extern "user32.dll"]
HWND GetForegroundWindow();

[extern "user32.dll"]
HWND GetActiveWindow();

const uint LWA_COLORKEY       =     0x00000001;
const uint LWA_ALPHA          =     0x00000002;

const uint ULW_COLORKEY       =     0x00000001;
const uint ULW_ALPHA          =     0x00000002;
const uint ULW_OPAQUE         =     0x00000004;

const uint WS_EX_DLGMODALFRAME    = 0x00000001;
const uint WS_EX_NOPARENTNOTIFY   = 0x00000004;
const uint WS_EX_TOPMOST          = 0x00000008;
const uint WS_EX_ACCEPTFILES      = 0x00000010;
const uint WS_EX_TRANSPARENT      = 0x00000020;
const uint WS_EX_MDICHILD         = 0x00000040;
const uint WS_EX_TOOLWINDOW       = 0x00000080;
const uint WS_EX_WINDOWEDGE       = 0x00000100;
const uint WS_EX_CLIENTEDGE       = 0x00000200;
const uint WS_EX_CONTEXTHELP      = 0x00000400;
const uint WS_EX_RIGHT            = 0x00001000;
const uint WS_EX_LEFT             = 0x00000000;
const uint WS_EX_RTLREADING       = 0x00002000;
const uint WS_EX_LTRREADING       = 0x00000000;
const uint WS_EX_LEFTSCROLLBAR    = 0x00004000;
const uint WS_EX_RIGHTSCROLLBAR   = 0x00000000;
const uint WS_EX_CONTROLPARENT    = 0x00010000;
const uint WS_EX_STATICEDGE       = 0x00020000;
const uint WS_EX_APPWINDOW        = 0x00040000;
const uint WS_EX_OVERLAPPEDWINDOW = (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE);
const uint WS_EX_PALETTEWINDOW    = (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
const uint WS_EX_LAYERED          = 0x00080000;
const uint WS_EX_COMPOSITED       = 0x02000000;


[extern "user32.dll"]
BOOL SetLayeredWindowAttributes
  (HWND     hwnd,
   COLORREF crKey,
   BYTE     bAlpha,
   DWORD    dwFlags);

packed struct POINT
{
  int x;
  int y;
}

packed struct SIZE
{
  LONG cx;
  LONG cy;
}

struct MSG
{
  HWND   hwnd;
  UINT   message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD  time;
  POINT  pt;
}

[extern "user32.dll"]
LRESULT SendMessageA (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

[extern "user32.dll"]
LRESULT SendMessageW (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

LRESULT SendMessagePtrA (HWND hwnd, UINT message, WPARAM wParam, char* lParam);
LRESULT SendMessagePtrW (HWND hwnd, UINT message, WPARAM wParam, wchar* lParam);

[extern "user32.dll"]
BOOL PostMessageA (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

[extern "user32.dll"]
BOOL PostMessageW (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

[extern "user32.dll"]
BOOL GetMessageA (MSG* msg, HWND hwnd, UINT min, UINT max);

[extern "user32.dll"]
BOOL GetMessageW (MSG* msg, HWND hwnd, UINT min, UINT max);

[extern "user32.dll"]
BOOL PeekMessageA (MSG* msg, HWND hwnd, UINT min, UINT max, UINT remove);

[extern "user32.dll"]
BOOL PeekMessageW (MSG* msg, HWND hwnd, UINT min, UINT max, UINT remove);

const uint PM_NOREMOVE = 0;
const uint PM_REMOVE   = 1;
const uint PM_NOYIELD  = 2;

[extern "user32.dll"]
BOOL TranslateMessage (MSG* msg);

[extern "user32.dll"]
LRESULT DispatchMessageA (MSG* msg);

[extern "user32.dll"]
LRESULT DispatchMessageW (MSG* msg);

[extern "user32.dll"]
BOOL DestroyWindow (HWND hwnd);

[extern "user32.dll"]
void PostQuitMessage (int exitcode);

[extern "user32.dll"]
BOOL PostThreadMessageA
   (DWORD thread_id,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

[extern "user32.dll"]
BOOL PostThreadMessageW
   (DWORD thread_id,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

const uint WM_CREATE                    =  0x0001;
const uint WM_DESTROY                   =  0x0002;
const uint WM_MOVE                      =  0x0003;
const uint WM_SIZE                      =  0x0005;
const uint WM_ACTIVATE                  =  0x0006;
const uint WM_SETFOCUS                  =  0x0007;
const uint WM_KILLFOCUS                 =  0x0008;
const uint WM_ENABLE                    =  0x000A;
const uint WM_SETREDRAW                 =  0x000B;
const uint WM_SETTEXT                   =  0x000C;
const uint WM_GETTEXT                   =  0x000D;
const uint WM_GETTEXTLENGTH             =  0x000E;
const uint WM_PAINT                     =  0x000F;
const uint WM_CLOSE                     =  0x0010;
const uint WM_QUERYENDSESSION           =  0x0011;
const uint WM_QUIT                      =  0x0012;
const uint WM_QUERYOPEN                 =  0x0013;
const uint WM_ERASEBKGND                =  0x0014;
const uint WM_SYSCOLORCHANGE            =  0x0015;
const uint WM_ENDSESSION                =  0x0016;
const uint WM_SHOWWINDOW                =  0x0018;
const uint WM_WININICHANGE              =  0x001A;
const uint WM_DEVMODECHANGE             =  0x001B;
const uint WM_ACTIVATEAPP               =  0x001C;
const uint WM_FONTCHANGE                =  0x001D;
const uint WM_TIMECHANGE                =  0x001E;
const uint WM_CANCELMODE                =  0x001F;
const uint WM_SETCURSOR                 =  0x0020;
const uint WM_MOUSEACTIVATE             =  0x0021;
const uint WM_CHILDACTIVATE             =  0x0022;
const uint WM_QUEUESYNC                 =  0x0023;
const uint WM_GETMINMAXINFO             =  0x0024;
const uint WM_PAINTICON                 =  0x0026;
const uint WM_ICONERASEBKGND            =  0x0027;
const uint WM_NEXTDLGCTL                =  0x0028;
const uint WM_SPOOLERSTATUS             =  0x002A;
const uint WM_DRAWITEM                  =  0x002B;
const uint WM_MEASUREITEM               =  0x002C;
const uint WM_DELETEITEM                =  0x002D;
const uint WM_VKEYTOITEM                =  0x002E;
const uint WM_CHARTOITEM                =  0x002F;
const uint WM_SETFONT                   =  0x0030;
const uint WM_GETFONT                   =  0x0031;
const uint WM_SETHOTKEY                 =  0x0032;
const uint WM_GETHOTKEY                 =  0x0033;
const uint WM_QUERYDRAGICON             =  0x0037;
const uint WM_COMPAREITEM               =  0x0039;
const uint WM_GETOBJECT                 =  0x003D;
const uint WM_COMPACTING                =  0x0041;
const uint WM_COMMNOTIFY                =  0x0044;
const uint WM_WINDOWPOSCHANGING         =  0x0046;
const uint WM_WINDOWPOSCHANGED          =  0x0047;
const uint WM_POWER                     =  0x0048;
const uint WM_COPYDATA                  =  0x004A;
const uint WM_CANCELJOURNAL             =  0x004B;
const uint WM_NOTIFY                    =  0x004E;
const uint WM_INPUTLANGCHANGEREQUEST    =  0x0050;
const uint WM_INPUTLANGCHANGE           =  0x0051;
const uint WM_TCARD                     =  0x0052;
const uint WM_HELP                      =  0x0053;
const uint WM_USERCHANGED               =  0x0054;
const uint WM_NOTIFYFORMAT              =  0x0055;
const uint WM_CONTEXTMENU               =  0x007B;
const uint WM_STYLECHANGING             =  0x007C;
const uint WM_STYLECHANGED              =  0x007D;
const uint WM_DISPLAYCHANGE             =  0x007E;
const uint WM_GETICON                   =  0x007F;
const uint WM_SETICON                   =  0x0080;
const uint WM_NCCREATE                  =  0x0081;
const uint WM_NCDESTROY                 =  0x0082;
const uint WM_NCCALCSIZE                =  0x0083;
const uint WM_NCHITTEST                 =  0x0084;
const uint WM_NCPAINT                   =  0x0085;
const uint WM_NCACTIVATE                =  0x0086;
const uint WM_GETDLGCODE                =  0x0087;
const uint WM_SYNCPAINT                 =  0x0088;
const uint WM_NCMOUSEMOVE               =  0x00A0;
const uint WM_NCLBUTTONDOWN             =  0x00A1;
const uint WM_NCLBUTTONUP               =  0x00A2;
const uint WM_NCLBUTTONDBLCLK           =  0x00A3;
const uint WM_NCRBUTTONDOWN             =  0x00A4;
const uint WM_NCRBUTTONUP               =  0x00A5;
const uint WM_NCRBUTTONDBLCLK           =  0x00A6;
const uint WM_NCMBUTTONDOWN             =  0x00A7;
const uint WM_NCMBUTTONUP               =  0x00A8;
const uint WM_NCMBUTTONDBLCLK           =  0x00A9;
const uint WM_KEYFIRST                  =  0x0100;
const uint WM_KEYDOWN                   =  0x0100;
const uint WM_KEYUP                     =  0x0101;
const uint WM_CHAR                      =  0x0102;
const uint WM_DEADCHAR                  =  0x0103;
const uint WM_SYSKEYDOWN                =  0x0104;
const uint WM_SYSKEYUP                  =  0x0105;
const uint WM_SYSCHAR                   =  0x0106;
const uint WM_SYSDEADCHAR               =  0x0107;
const uint WM_KEYLAST                   =  0x0108;
const uint WM_IME_STARTCOMPOSITION      =  0x010D;
const uint WM_IME_ENDCOMPOSITION        =  0x010E;
const uint WM_IME_COMPOSITION           =  0x010F;
const uint WM_IME_KEYLAST               =  0x010F;
const uint WM_INITDIALOG                =  0x0110;
const uint WM_COMMAND                   =  0x0111;
const uint WM_SYSCOMMAND                =  0x0112;
const uint WM_TIMER                     =  0x0113;
const uint WM_HSCROLL                   =  0x0114;
const uint WM_VSCROLL                   =  0x0115;
const uint WM_INITMENU                  =  0x0116;
const uint WM_INITMENUPOPUP             =  0x0117;
const uint WM_MENUSELECT                =  0x011F;
const uint WM_MENUCHAR                  =  0x0120;
const uint WM_ENTERIDLE                 =  0x0121;
const uint WM_MENURBUTTONUP             =  0x0122;
const uint WM_MENUDRAG                  =  0x0123;
const uint WM_MENUGETOBJECT             =  0x0124;
const uint WM_UNINITMENUPOPUP           =  0x0125;
const uint WM_MENUCOMMAND               =  0x0126;
const uint WM_CHANGEUISTATE             =  0x0127;
const uint WM_CTLCOLORMSGBOX            =  0x0132;
const uint WM_CTLCOLOREDIT              =  0x0133;
const uint WM_CTLCOLORLISTBOX           =  0x0134;
const uint WM_CTLCOLORBTN               =  0x0135;
const uint WM_CTLCOLORDLG               =  0x0136;
const uint WM_CTLCOLORSCROLLBAR         =  0x0137;
const uint WM_CTLCOLORSTATIC            =  0x0138;
const uint WM_MOUSEFIRST                =  0x0200;
const uint WM_MOUSEMOVE                 =  0x0200;
const uint WM_LBUTTONDOWN               =  0x0201;
const uint WM_LBUTTONUP                 =  0x0202;
const uint WM_LBUTTONDBLCLK             =  0x0203;
const uint WM_RBUTTONDOWN               =  0x0204;
const uint WM_RBUTTONUP                 =  0x0205;
const uint WM_RBUTTONDBLCLK             =  0x0206;
const uint WM_MBUTTONDOWN               =  0x0207;
const uint WM_MBUTTONUP                 =  0x0208;
const uint WM_MBUTTONDBLCLK             =  0x0209;
const uint WM_MOUSEWHEEL                =  0x020A;
const uint WM_PARENTNOTIFY              =  0x0210;
const uint WM_ENTERMENULOOP             =  0x0211;
const uint WM_EXITMENULOOP              =  0x0212;
const uint WM_NEXTMENU                  =  0x0213;
const uint WM_SIZING                    =  0x0214;
const uint WM_CAPTURECHANGED            =  0x0215;
const uint WM_MOVING                    =  0x0216;
const uint WM_POWERBROADCAST            =  0x0218;
const uint WM_DEVICECHANGE              =  0x0219;
const uint WM_MDICREATE                 =  0x0220;
const uint WM_MDIDESTROY                =  0x0221;
const uint WM_MDIACTIVATE               =  0x0222;
const uint WM_MDIRESTORE                =  0x0223;
const uint WM_MDINEXT                   =  0x0224;
const uint WM_MDIMAXIMIZE               =  0x0225;
const uint WM_MDITILE                   =  0x0226;
const uint WM_MDICASCADE                =  0x0227;
const uint WM_MDIICONARRANGE            =  0x0228;
const uint WM_MDIGETACTIVE              =  0x0229;
const uint WM_MDISETMENU                =  0x0230;
const uint WM_ENTERSIZEMOVE             =  0x0231;
const uint WM_EXITSIZEMOVE              =  0x0232;
const uint WM_DROPFILES                 =  0x0233;
const uint WM_MDIREFRESHMENU            =  0x0234;
const uint WM_IME_SETCONTEXT            =  0x0281;
const uint WM_IME_NOTIFY                =  0x0282;
const uint WM_IME_CONTROL               =  0x0283;
const uint WM_IME_COMPOSITIONFULL       =  0x0284;
const uint WM_IME_SELECT                =  0x0285;
const uint WM_IME_CHAR                  =  0x0286;
const uint WM_IME_REQUEST               =  0x0288;
const uint WM_IME_KEYDOWN               =  0x0290;
const uint WM_IME_KEYUP                 =  0x0291;
const uint WM_MOUSEHOVER                =  0x02A1;
const uint WM_MOUSELEAVE                =  0x02A3;
const uint WM_CUT                       =  0x0300;
const uint WM_COPY                      =  0x0301;
const uint WM_PASTE                     =  0x0302;
const uint WM_CLEAR                     =  0x0303;
const uint WM_UNDO                      =  0x0304;
const uint WM_RENDERFORMAT              =  0x0305;
const uint WM_RENDERALLFORMATS          =  0x0306;
const uint WM_DESTROYCLIPBOARD          =  0x0307;
const uint WM_DRAWCLIPBOARD             =  0x0308;
const uint WM_PAINTCLIPBOARD            =  0x0309;
const uint WM_VSCROLLCLIPBOARD          =  0x030A;
const uint WM_SIZECLIPBOARD             =  0x030B;
const uint WM_ASKCBFORMATNAME           =  0x030C;
const uint WM_CHANGECBCHAIN             =  0x030D;
const uint WM_HSCROLLCLIPBOARD          =  0x030E;
const uint WM_QUERYNEWPALETTE           =  0x030F;
const uint WM_PALETTEISCHANGING         =  0x0310;
const uint WM_PALETTECHANGED            =  0x0311;
const uint WM_HOTKEY                    =  0x0312;
const uint WM_PRINT                     =  0x0317;
const uint WM_PRINTCLIENT               =  0x0318;
const uint WM_HANDHELDFIRST             =  0x0358;
const uint WM_HANDHELDLAST              =  0x035F;
const uint WM_AFXFIRST                  =  0x0360;
const uint WM_AFXLAST                   =  0x037F;
const uint WM_PENWINFIRST               =  0x0380;
const uint WM_PENWINLAST                =  0x038F;

const uint WM_USER                      = 0x0400;



const int HTERROR          =   (-2);
const int HTTRANSPARENT    =   (-1);
const int HTNOWHERE        =   0;
const int HTCLIENT         =   1;
const int HTCAPTION        =   2;
const int HTSYSMENU        =   3;
const int HTGROWBOX        =   4;
const int HTSIZE           =   HTGROWBOX;
const int HTMENU           =   5;
const int HTHSCROLL        =   6;
const int HTVSCROLL        =   7;
const int HTMINBUTTON      =   8;
const int HTMAXBUTTON      =   9;
const int HTLEFT           =   10;
const int HTRIGHT          =   11;
const int HTTOP            =   12;
const int HTTOPLEFT        =   13;
const int HTTOPRIGHT       =   14;
const int HTBOTTOM         =   15;
const int HTBOTTOMLEFT     =   16;
const int HTBOTTOMRIGHT    =   17;
const int HTBORDER         =   18;
const int HTREDUCE         =   HTMINBUTTON;
const int HTZOOM           =   HTMAXBUTTON;
const int HTSIZEFIRST      =   HTLEFT;
const int HTSIZELAST       =   HTBOTTOMRIGHT;
const int HTOBJECT         =   19;
const int HTCLOSE          =   20;
const int HTHELP           =   21;


packed struct MINMAXINFO
{
  POINT ptReserved;
  POINT ptMaxSize;
  POINT ptMaxPosition;
  POINT ptMinTrackSize;
  POINT ptMaxTrackSize;
}

struct PAINTSTRUCT
{
  HDC  hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  BYTE rgbReserved[32];
}

[extern "user32.dll"]
HDC BeginPaint (HWND hWnd, PAINTSTRUCT* lpPaint);

[extern "user32.dll"]
BOOL EndPaint (HWND hWnd, PAINTSTRUCT* lpPaint);

[extern "user32.dll"]
BOOL InvalidateRect (HWND hWnd, RECT* rect, BOOL bErase);

[extern "user32.dll"]
BOOL UpdateWindow (HWND hWnd);

[extern "user32.dll"]
BOOL MoveWindow (HWND hWnd, int x, int y, int nWidth, int nHeight, BOOL brepaint);

[extern "user32.dll"]
BOOL SetWindowPos (HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

const uint SWP_NOSIZE         = 0x0001;
const uint SWP_NOMOVE         = 0x0002;
const uint SWP_NOZORDER       = 0x0004;
const uint SWP_NOREDRAW       = 0x0008;
const uint SWP_NOACTIVATE     = 0x0010;
const uint SWP_FRAMECHANGED   = 0x0020;
const uint SWP_SHOWWINDOW     = 0x0040;
const uint SWP_HIDEWINDOW     = 0x0080;
const uint SWP_NOCOPYBITS     = 0x0100;
const uint SWP_NOOWNERZORDER  = 0x0200;  /* Don't do owner Z ordering */
const uint SWP_NOSENDCHANGING = 0x0400;  /* Don't send WM_WINDOWPOSCHANGING */
const uint SWP_DRAWFRAME      = SWP_FRAMECHANGED;
const uint SWP_NOREPOSITION   = SWP_NOOWNERZORDER;
const uint SWP_DEFERERASE     = 0x2000;
const uint SWP_ASYNCWINDOWPOS = 0x4000;

const int HWND_TOP       = 0;
const int HWND_BOTTOM    = 1;
const int HWND_TOPMOST   = -1;
const int HWND_NOTOPMOST = -2;

struct WINDOWPOS
{
  HWND hwnd;
  HWND hwndInsertAfter;
  int  x;
  int  y;
  int  cx;
  int  cy;
  UINT flags;
}

[extern "user32.dll"]
BOOL SetWindowTextA (HWND hWnd, char *title);

[extern "user32.dll"]
BOOL SetWindowTextW (HWND hWnd, wchar *title);

[extern "user32.dll"]
int GetWindowTextA (HWND hWnd, char *lpString, uint nMaxCount);

[extern "user32.dll"]
int GetWindowTextW (HWND hWnd, wchar *lpString, uint nMaxCount);

const uint SC_SIZE        = 0xF000;
const uint SC_MOVE        = 0xF010;
const uint SC_MINIMIZE    = 0xF020;
const uint SC_MAXIMIZE    = 0xF030;
const uint SC_NEXTWINDOW  = 0xF040;
const uint SC_PREVWINDOW  = 0xF050;
const uint SC_CLOSE       = 0xF060;
const uint SC_VSCROLL     = 0xF070;
const uint SC_HSCROLL     = 0xF080;
const uint SC_MOUSEMENU   = 0xF090;
const uint SC_KEYMENU     = 0xF100;
const uint SC_ARRANGE     = 0xF110;
const uint SC_RESTORE     = 0xF120;

const uint CS_VREDRAW        = 0x0001;
const uint CS_HREDRAW        = 0x0002;
const uint CS_DBLCLKS        = 0x0008;
const uint CS_OWNDC          = 0x0020;
const uint CS_CLASSDC        = 0x0040;
const uint CS_PARENTDC       = 0x0080;
const uint CS_NOCLOSE        = 0x0200;
const uint CS_SAVEBITS       = 0x0800;
const uint CS_BYTEALIGNCLIENT= 0x1000;
const uint CS_BYTEALIGNWINDOW= 0x2000;
const uint CS_GLOBALCLASS    = 0x4000;
const uint CS_IME           = 0x10000;

[extern "user32.dll"]
BOOL IsIconic (HWND hWnd);

[extern "user32.dll"]
BOOL GetWindowRect (HWND hWnd, RECT *r);

[extern "user32.dll"]
BOOL GetClientRect (HWND hWnd, RECT* lpRect);

[extern "user32.dll"]
DWORD SetClassLongA (HWND hwnd, int index, LONG dwNewLong);

[extern "user32.dll"]
DWORD SetClassLongW (HWND hwnd, int index, LONG dwNewLong);

const int GCL_HCURSOR  = (-12);

[extern "user32.dll"]
BOOL GetCursorPos (POINT* lpPoint);

[extern "user32.dll"]
BOOL SetCursorPos (int X, int Y);

[extern "user32.dll"]
HWND SetCapture (HWND hWnd);

[extern "user32.dll"]
BOOL ReleaseCapture ();

[extern "user32.dll"]
BOOL ClipCursor (RECT *lpRect);

[extern "user32.dll"]
HCURSOR SetCursor (HCURSOR hCursor);

[extern "user32.dll"]
int ShowCursor (BOOL bShow);

[extern "user32.dll"]
BOOL CreateCaret (HWND hWnd, HBITMAP hBitmap, int nWidth, int nHeight);

[extern "user32.dll"]
BOOL SetCaretPos (int X, int Y);

[extern "user32.dll"]
BOOL ShowCaret (HWND hWnd);

[extern "user32.dll"]
BOOL HideCaret (HWND hWnd);

[extern "user32.dll"]
BOOL DestroyCaret ();

[extern "user32.dll"]
int GetSystemMetrics (int nIndex);

const int SM_CXSCREEN          =   0;
const int SM_CYSCREEN          =   1;
const int SM_CXVSCROLL         =   2;
const int SM_CYHSCROLL         =   3;
const int SM_CYCAPTION         =   4;
const int SM_CXBORDER          =   5;
const int SM_CYBORDER          =   6;
const int SM_CXDLGFRAME        =   7;
const int SM_CYDLGFRAME        =   8;
const int SM_CYVTHUMB          =   9;
const int SM_CXHTHUMB          =   10;
const int SM_CXICON            =   11;
const int SM_CYICON            =   12;
const int SM_CXCURSOR          =   13;
const int SM_CYCURSOR          =   14;
const int SM_CYMENU            =   15;
const int SM_CXFULLSCREEN      =   16;
const int SM_CYFULLSCREEN      =   17;
const int SM_CYKANJIWINDOW     =   18;
const int SM_MOUSEPRESENT      =   19;
const int SM_CYVSCROLL         =   20;
const int SM_CXHSCROLL         =   21;
const int SM_DEBUG             =   22;
const int SM_SWAPBUTTON        =   23;
const int SM_RESERVED1         =   24;
const int SM_RESERVED2         =   25;
const int SM_RESERVED3         =   26;
const int SM_RESERVED4         =   27;
const int SM_CXMIN             =   28;
const int SM_CYMIN             =   29;
const int SM_CXSIZE            =   30;
const int SM_CYSIZE            =   31;
const int SM_CXFRAME           =   32;
const int SM_CYFRAME           =   33;
const int SM_CXMINTRACK        =   34;
const int SM_CYMINTRACK        =   35;
const int SM_CXDOUBLECLK       =   36;
const int SM_CYDOUBLECLK       =   37;
const int SM_CXICONSPACING     =   38;
const int SM_CYICONSPACING     =   39;
const int SM_MENUDROPALIGNMENT =   40;
const int SM_PENWINDOWS        =   41;
const int SM_DBCSENABLED       =   42;
const int SM_CMOUSEBUTTONS     =   43;
const int SM_CXFIXEDFRAME      =     SM_CXDLGFRAME;
const int SM_CYFIXEDFRAME      =     SM_CYDLGFRAME;
const int SM_CXSIZEFRAME       =     SM_CXFRAME;
const int SM_CYSIZEFRAME       =     SM_CYFRAME;
const int SM_SECURE            =   44;
const int SM_CXEDGE            =   45;
const int SM_CYEDGE            =   46;
const int SM_CXMINSPACING      =   47;
const int SM_CYMINSPACING      =   48;
const int SM_CXSMICON          =   49;
const int SM_CYSMICON          =   50;
const int SM_CYSMCAPTION       =   51;
const int SM_CXSMSIZE          =   52;
const int SM_CYSMSIZE          =   53;
const int SM_CXMENUSIZE        =   54;
const int SM_CYMENUSIZE        =   55;
const int SM_ARRANGE           =   56;
const int SM_CXMINIMIZED       =   57;
const int SM_CYMINIMIZED       =   58;
const int SM_CXMAXTRACK        =   59;
const int SM_CYMAXTRACK        =   60;
const int SM_CXMAXIMIZED       =   61;
const int SM_CYMAXIMIZED       =   62;
const int SM_NETWORK           =   63;
const int SM_CLEANBOOT         =   67;
const int SM_CXDRAG            =   68;
const int SM_CYDRAG            =   69;
const int SM_SHOWSOUNDS        =   70;
const int SM_CXMENUCHECK       =   71;
const int SM_CYMENUCHECK       =   72;
const int SM_SLOWMACHINE       =   73;
const int SM_MIDEASTENABLED    =   74;
const int SM_MOUSEWHEELPRESENT =   75;
const int SM_XVIRTUALSCREEN    =   76;
const int SM_YVIRTUALSCREEN    =   77;
const int SM_CXVIRTUALSCREEN   =   78;
const int SM_CYVIRTUALSCREEN   =   79;
const int SM_CMONITORS         =   80;
const int SM_SAMEDISPLAYFORMAT =   81;
const int SM_CXPADDEDBORDER    =   92;

[extern "user32.dll"]
int MessageBoxA (HWND hWnd, char *lpText, char *lpCaption, UINT uType);

[extern "user32.dll"]
int MessageBoxW (HWND hWnd, wchar *lpText, wchar *lpCaption, UINT uType);

const uint MB_OK                     = 0;
const uint MB_OKCANCEL               = 0x00000001;
const uint MB_ABORTRETRYIGNORE       = 0x00000002;
const uint MB_YESNOCANCEL            = 0x00000003;
const uint MB_YESNO                  = 0x00000004;
const uint MB_RETRYCANCEL            = 0x00000005;
const uint MB_ICONHAND               = 0x00000010;
const uint MB_ICONQUESTION           = 0x00000020;
const uint MB_ICONEXCLAMATION        = 0x00000030;
const uint MB_ICONASTERISK           = 0x00000040;
const uint MB_USERICON               = 0x00000080;
const uint MB_ICONWARNING            = MB_ICONEXCLAMATION;
const uint MB_ICONERROR              = MB_ICONHAND;
const uint MB_ICONINFORMATION        = MB_ICONASTERISK;
const uint MB_ICONSTOP               = MB_ICONHAND;


[extern "user32.dll"]
BOOL SystemParametersInfoA (UINT uiAction, UINT uiParam, LPVOID pvParam, UINT fWinIni);

[extern "user32.dll"]
BOOL SystemParametersInfoW (UINT uiAction, UINT uiParam, LPVOID pvParam, UINT fWinIni);

const uint SPI_GETWORKAREA  = 48;


const DWORD MONITOR_DEFAULTTONULL = 0;
const DWORD MONITOR_DEFAULTTOPRIMARY = 1;
const DWORD MONITOR_DEFAULTTONEAREST = 2;


[extern "user32.dll"]
HMONITOR MonitorFromPoint (POINT* pt, DWORD dwFlags);

[extern "user32.dll"]
HMONITOR MonitorFromRect (RECT* rect, DWORD dwFlags);

[extern "user32.dll"]
HMONITOR MonitorFromWindow (HWND hwnd, DWORD dwFlags);

struct MONITORINFO
{
  DWORD cbSize;
  RECT  rcMonitor;
  RECT  rcWork;
  DWORD dwFlags;
}

[extern "user32.dll"]
BOOL GetMonitorInfoA (HMONITOR hMonitor, MONITORINFO* lpmi);


typedef [callback] BOOL MONITORENUMPROC (HMONITOR Param1, HDC Param2, RECT* Param3, byte* Param4);

[extern "user32.dll"]
BOOL EnumDisplayMonitors (HDC hdc, RECT* lprcClip, MONITORENUMPROC lpfnEnum, byte* dwData);


const UINT SIF_RANGE           = 0x0001;
const UINT SIF_PAGE            = 0x0002;
const UINT SIF_POS             = 0x0004;
const UINT SIF_DISABLENOSCROLL = 0x0008;
const UINT SIF_TRACKPOS        = 0x0010;
const UINT SIF_ALL             = (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS);

const int SB_HORZ            = 0;
const int SB_VERT            = 1;
const int SB_CTL             = 2;
const int SB_BOTH            = 3;

const int SB_LINEUP          = 0;
const int SB_LINELEFT        = 0;
const int SB_LINEDOWN        = 1;
const int SB_LINERIGHT       = 1;
const int SB_PAGEUP          = 2;
const int SB_PAGELEFT        = 2;
const int SB_PAGEDOWN        = 3;
const int SB_PAGERIGHT       = 3;
const int SB_THUMBPOSITION   = 4;
const int SB_THUMBTRACK      = 5;
const int SB_TOP             = 6;
const int SB_LEFT            = 6;
const int SB_BOTTOM          = 7;
const int SB_RIGHT           = 7;
const int SB_ENDSCROLL       = 8;


struct SCROLLINFO
{
   UINT    cbSize;
   UINT    fMask;
   int     nMin;
   int     nMax;
   UINT    nPage;
   int     nPos;
   int     nTrackPos;
}

[extern "user32.dll"]
int SetScrollInfo (HWND hwnd, int flag, SCROLLINFO *info, BOOL b);

[extern "user32.dll"]
BOOL GetScrollInfo (HWND hwnd, int flag, SCROLLINFO *info);

const uint ES_LEFT           =  0x0000;
const uint ES_CENTER         =  0x0001;
const uint ES_RIGHT          =  0x0002;
const uint ES_MULTILINE      =  0x0004;
const uint ES_UPPERCASE      =  0x0008;
const uint ES_LOWERCASE      =  0x0010;
const uint ES_PASSWORD       =  0x0020;
const uint ES_AUTOVSCROLL    =  0x0040;
const uint ES_AUTOHSCROLL    =  0x0080;
const uint ES_NOHIDESEL      =  0x0100;
const uint ES_OEMCONVERT     =  0x0400;
const uint ES_READONLY       =  0x0800;
const uint ES_WANTRETURN     =  0x1000;
const uint ES_NUMBER         =  0x2000;

const uint EM_GETSEL              = 0x00B0;
const uint EM_SETSEL              = 0x00B1;
const uint EM_GETRECT             = 0x00B2;
const uint EM_SETRECT             = 0x00B3;
const uint EM_SETRECTNP           = 0x00B4;
const uint EM_SCROLL              = 0x00B5;
const uint EM_LINESCROLL          = 0x00B6;
const uint EM_SCROLLCARET         = 0x00B7;
const uint EM_GETMODIFY           = 0x00B8;
const uint EM_SETMODIFY           = 0x00B9;
const uint EM_GETLINECOUNT        = 0x00BA;
const uint EM_LINEINDEX           = 0x00BB;
const uint EM_SETHANDLE           = 0x00BC;
const uint EM_GETHANDLE           = 0x00BD;
const uint EM_GETTHUMB            = 0x00BE;
const uint EM_LINELENGTH          = 0x00C1;
const uint EM_REPLACESEL          = 0x00C2;
const uint EM_GETLINE             = 0x00C4;
const uint EM_LIMITTEXT           = 0x00C5;
const uint EM_CANUNDO             = 0x00C6;
const uint EM_UNDO                = 0x00C7;
const uint EM_FMTLINES            = 0x00C8;
const uint EM_LINEFROMCHAR        = 0x00C9;
const uint EM_SETTABSTOPS         = 0x00CB;
const uint EM_SETPASSWORDCHAR     = 0x00CC;
const uint EM_EMPTYUNDOBUFFER     = 0x00CD;
const uint EM_GETFIRSTVISIBLELINE = 0x00CE;
const uint EM_SETREADONLY         = 0x00CF;
const uint EM_SETWORDBREAKPROC    = 0x00D0;
const uint EM_GETWORDBREAKPROC    = 0x00D1;
const uint EM_GETPASSWORDCHAR     = 0x00D2;
const uint EM_SETMARGINS          = 0x00D3;
const uint EM_GETMARGINS          = 0x00D4;
const uint EM_SETLIMITTEXT        = EM_LIMITTEXT;
const uint EM_GETLIMITTEXT        = 0x00D5;
const uint EM_POSFROMCHAR         = 0x00D6;
const uint EM_CHARFROMPOS         = 0x00D7;
const uint EM_SETIMESTATUS        = 0x00D8;
const uint EM_GETIMESTATUS        = 0x00D9;

const uint EM_SETBKGNDCOLOR = (WM_USER + 67);
const uint EM_AUTOURLDETECT = (WM_USER + 91);
const uint AURL_ENABLEEAURLS = 8;


// VIRTUALKEYCODES

const int VK_LBUTTON      = 0x01;
const int VK_RBUTTON      = 0x02;
const int VK_CANCEL       = 0x03;
const int VK_MBUTTON      = 0x04;    /* NOT contiguous with L & RBUTTON */
const int VK_BACK         = 0x08;
const int VK_TAB          = 0x09;
const int VK_CLEAR        = 0x0C;
const int VK_RETURN       = 0x0D;
const int VK_SHIFT        = 0x10;
const int VK_CONTROL      = 0x11;
const int VK_MENU         = 0x12;
const int VK_PAUSE        = 0x13;
const int VK_CAPITAL      = 0x14;
const int VK_KANA         = 0x15;
const int VK_JUNJA        = 0x17;
const int VK_FINAL        = 0x18;
const int VK_HANJA        = 0x19;
const int VK_KANJI        = 0x19;
const int VK_ESCAPE       = 0x1B;
const int VK_CONVERT      = 0x1C;
const int VK_NONCONVERT   = 0x1D;
const int VK_ACCEPT       = 0x1E;
const int VK_MODECHANGE   = 0x1F;
const int VK_SPACE        = 0x20;
const int VK_PRIOR        = 0x21;
const int VK_NEXT         = 0x22;
const int VK_END          = 0x23;
const int VK_HOME         = 0x24;
const int VK_LEFT         = 0x25;
const int VK_UP           = 0x26;
const int VK_RIGHT        = 0x27;
const int VK_DOWN         = 0x28;
const int VK_SELECT       = 0x29;
const int VK_PRINT        = 0x2A;
const int VK_EXECUTE      = 0x2B;
const int VK_SNAPSHOT     = 0x2C;
const int VK_INSERT       = 0x2D;
const int VK_DELETE       = 0x2E;
const int VK_HELP         = 0x2F;
const int VK_LWIN         = 0x5B;
const int VK_RWIN         = 0x5C;
const int VK_APPS         = 0x5D;
const int VK_NUMPAD0      = 0x60;
const int VK_NUMPAD1      = 0x61;
const int VK_NUMPAD2      = 0x62;
const int VK_NUMPAD3      = 0x63;
const int VK_NUMPAD4      = 0x64;
const int VK_NUMPAD5      = 0x65;
const int VK_NUMPAD6      = 0x66;
const int VK_NUMPAD7      = 0x67;
const int VK_NUMPAD8      = 0x68;
const int VK_NUMPAD9      = 0x69;
const int VK_MULTIPLY     = 0x6A;
const int VK_ADD          = 0x6B;
const int VK_SEPARATOR    = 0x6C;
const int VK_SUBTRACT     = 0x6D;
const int VK_DECIMAL      = 0x6E;
const int VK_DIVIDE       = 0x6F;
const int VK_F1           = 0x70;
const int VK_F2           = 0x71;
const int VK_F3           = 0x72;
const int VK_F4           = 0x73;
const int VK_F5           = 0x74;
const int VK_F6           = 0x75;
const int VK_F7           = 0x76;
const int VK_F8           = 0x77;
const int VK_F9           = 0x78;
const int VK_F10          = 0x79;
const int VK_F11          = 0x7A;
const int VK_F12          = 0x7B;
const int VK_F13          = 0x7C;
const int VK_F14          = 0x7D;
const int VK_F15          = 0x7E;
const int VK_F16          = 0x7F;
const int VK_F17          = 0x80;
const int VK_F18          = 0x81;
const int VK_F19          = 0x82;
const int VK_F20          = 0x83;
const int VK_F21          = 0x84;
const int VK_F22          = 0x85;
const int VK_F23          = 0x86;
const int VK_F24          = 0x87;
const int VK_NUMLOCK      = 0x90;
const int VK_SCROLL       = 0x91;
const int VK_LSHIFT       = 0xA0;
const int VK_RSHIFT       = 0xA1;
const int VK_LCONTROL     = 0xA2;
const int VK_RCONTROL     = 0xA3;
const int VK_LMENU        = 0xA4;
const int VK_RMENU        = 0xA5;



/*
 * Key State Masks for Mouse Messages
 */
const int MK_LBUTTON          = 0x0001;
const int MK_RBUTTON          = 0x0002;
const int MK_SHIFT            = 0x0004;
const int MK_CONTROL          = 0x0008;
const int MK_MBUTTON          = 0x0010;

const uint TME_HOVER       = 0x00000001;
const uint TME_LEAVE       = 0x00000002;
const uint TME_QUERY       = 0x40000000;
const uint TME_CANCEL      = 0x80000000;



struct TRACKMOUSEEVENT
{
  DWORD cbSize;
  DWORD dwFlags;
  HWND  hwndTrack;
  DWORD dwHoverTime;
}

[extern "user32.dll"]
BOOL TrackMouseEvent (TRACKMOUSEEVENT* lpEventTrack);


[extern "user32.dll"]
int FillRect (HDC hDC, RECT* lprc, HBRUSH hbr);

[extern "user32.dll"]
HDC GetDC (HWND hWnd);

[extern "user32.dll"]
HDC GetDCEx (HWND hwnd, HRGN hrgn, DWORD flags);

[extern "user32.dll"]
int ReleaseDC (HWND hWnd, HDC hDC);

[extern "user32.dll"]
HDC GetWindowDC (HWND hwnd);

const uint DCX_WINDOW         =  0x00000001;
const uint DCX_CACHE          =  0x00000002;
const uint DCX_NORESETATTRS   =  0x00000004;
const uint DCX_CLIPCHILDREN   =  0x00000008;
const uint DCX_CLIPSIBLINGS   =  0x00000010;
const uint DCX_PARENTCLIP     =  0x00000020;
const uint DCX_EXCLUDERGN     =  0x00000040;
const uint DCX_INTERSECTRGN   =  0x00000080;
const uint DCX_EXCLUDEUPDATE  =  0x00000100;
const uint DCX_INTERSECTUPDATE=  0x00000200;
const uint DCX_LOCKWINDOWUPDATE= 0x00000400;
const uint DCX_VALIDATE       =  0x00200000;

//------------------------------------------------------------------------------

[extern "gdi32.dll"]
int GetObjectA (HANDLE h, uint c, LPVOID pv);

[extern "gdi32.dll"]
BOOL GetTextExtentPoint32A (HDC hdc, char *lpString, int c, SIZE* lpSize);

[extern "gdi32.dll"]
BOOL GetTextExtentPoint32W (HDC hdc, wchar *lpString, int c, SIZE* lpSize);

[extern "gdi32.dll"]
int ExcludeClipRect (HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);

const int NULLREGION  = 1;

[extern "gdi32.dll"]
HGDIOBJ SelectObject (HDC hdc, HGDIOBJ hgdiobj);

[extern "gdi32.dll"]
BOOL DeleteObject (HGDIOBJ hObject);

[extern "gdi32.dll"]
BOOL DeleteDC (HDC hdc);

[extern "gdi32.dll"]
HBITMAP CreateCompatibleBitmap (HDC hdc, int nWidth, int nHeight);

[extern "gdi32.dll"]
HDC CreateCompatibleDC (HDC hdc);

[extern "gdi32.dll"]
BOOL SetPixelV (HDC hdc, int X, int Y, COLORREF crColor);

[extern "gdi32.dll"]
HPEN CreatePen (int fnPenStyle, int nWidth, COLORREF crColor);

[extern "gdi32.dll"]
BOOL Polyline (HDC hdc, POINT *lppt, int cPoints);

[extern "gdi32.dll"]
HBRUSH CreateSolidBrush (COLORREF crColor);

[extern "gdi32.dll"]
COLORREF SetTextColor (HDC hdc, COLORREF crColor);

[extern "gdi32.dll"]
COLORREF SetBkColor (HDC hdc, COLORREF crColor);

[extern "gdi32.dll"]
int SetBkMode (HDC hdc, int iBkMode);

const int TRANSPARENT = 1;
const int OPAQUE      = 2;


[extern "gdi32.dll"]
HFONT CreateFontA (
  int nHeight,
  int nWidth,
  int nEscapement,
  int nOrientation,
  int fnWeight,
  DWORD fdwItalic,
  DWORD fdwUnderline,
  DWORD fdwStrikeOut,
  DWORD fdwCharSet,
  DWORD fdwOutputPrecision,
  DWORD fdwClipPrecision,
  DWORD fdwQuality,
  DWORD fdwPitchAndFamily,
  char  *lpszFace);

[extern "gdi32.dll"]
HFONT CreateFontW (
  int nHeight,
  int nWidth,
  int nEscapement,
  int nOrientation,
  int fnWeight,
  DWORD fdwItalic,
  DWORD fdwUnderline,
  DWORD fdwStrikeOut,
  DWORD fdwCharSet,
  DWORD fdwOutputPrecision,
  DWORD fdwClipPrecision,
  DWORD fdwQuality,
  DWORD fdwPitchAndFamily,
  wchar  *lpszFace);

const int  FW_BOLD           =  700;
const int  FW_DONTCARE       =  0;

const uint ANSI_CHARSET = 0;
const uint OUT_DEFAULT_PRECIS = 0;
const uint CLIP_DEFAULT_PRECIS = 0;
const uint DRAFT_QUALITY = 1;
const uint DEFAULT_PITCH = 0;

[extern "gdi32.dll"]
DWORD SetMapperFlags (HDC hdc, DWORD flags);

[extern "gdi32.dll"]
BOOL Rectangle (HDC hdc, int left, int top, int right, int bottom);

[extern "gdi32.dll"]
BOOL Ellipse (HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);

[extern "gdi32.dll"]
BOOL Arc
  (HDC hdc,
   int nLeftRect, int nTopRect,
   int nRightRect, int nBottomRect,
   int nXStartArc, int nYStartArc,
   int nXEndArc, int nYEndArc);

[extern "gdi32.dll"]
BOOL TextOutA (HDC hdc, int nXStart, int nYStart, LPCSTR lpString, int cchString);

[extern "gdi32.dll"]
BOOL TextOutW (HDC hdc, int nXStart, int nYStart, LPCWSTR lpString, int cchString);

[extern "gdi32.dll"]
BOOL ExtTextOutA (HDC hdc, int X, int Y, UINT fuOptions, RECT* lprc, LPCSTR lpString,
                  UINT cbCount, INT *lpDx);

[extern "gdi32.dll"]
BOOL ExtTextOutW (HDC hdc, int X, int Y, UINT fuOptions, RECT* lprc, LPCWSTR lpString,
                  UINT cbCount, INT *lpDx);

const UINT ETO_CLIPPED = 0x0004;
const UINT ETO_OPAQUE  = 0x0002;


[extern "gdi32.dll"]
BOOL BitBlt (HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight,
             HDC hdcSrc, int nXSrc, int nYSrc, DWORD dwRop);

[extern "gdi32.dll"]
BOOL StretchBlt (HDC hdcDest, int xDest, int yDest, int wDest, int hDest,
                 HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc,
                 DWORD rop);

const DWORD SRCCOPY = 0x00CC0020;

const uint BI_RGB  = 0;
const uint BI_BITFIELDS = 3;
const uint BI_JPEG = 4;
const uint BI_PNG  = 5;

packed struct BITMAPINFOHEADER
{
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
}

[extern "gdi32.dll"]
HBITMAP CreateDIBSection
   (HDC hdc,
    byte *pbmi,
    UINT iUsage,
    byte **ppvBits,
    HANDLE hSection,
    DWORD dwOffset);

const uint DIB_RGB_COLORS =  0;

[extern "gdi32.dll"]
int SetStretchBltMode (HDC hdc, int mode);
const int BLACKONWHITE = 1;
const int WHITEONBLACK = 2;
const int COLORONCOLOR = 3;
const int HALFTONE     = 4;

[extern "gdi32.dll"]
BOOL SetBrushOrgEx (HDC hdc, int x, int y, POINT* lppt);

typedef uint4 BLENDFUNCTION;
const BLENDFUNCTION BLDFCT = (255 << 16) + (1 << 24);

[extern "Msimg32.dll"]
BOOL AlphaBlend(
  HDC           hdcDest,
  int           xoriginDest,
  int           yoriginDest,
  int           wDest,
  int           hDest,
  HDC           hdcSrc,
  int           xoriginSrc,
  int           yoriginSrc,
  int           wSrc,
  int           hSrc,
  BLENDFUNCTION ftn);


const int CTLCOLOR_MSGBOX        = 0;
const int CTLCOLOR_EDIT          = 1;
const int CTLCOLOR_LISTBOX       = 2;
const int CTLCOLOR_BTN           = 3;
const int CTLCOLOR_DLG           = 4;
const int CTLCOLOR_SCROLLBAR     = 5;
const int CTLCOLOR_STATIC        = 6;
const int CTLCOLOR_MAX           = 7;

const int COLOR_SCROLLBAR        = 0;
const int COLOR_BACKGROUND       = 1;
const int COLOR_ACTIVECAPTION    = 2;
const int COLOR_INACTIVECAPTION  = 3;
const int COLOR_MENU             = 4;
const int COLOR_WINDOW           = 5;
const int COLOR_WINDOWFRAME      = 6;
const int COLOR_MENUTEXT         = 7;
const int COLOR_WINDOWTEXT       = 8;
const int COLOR_CAPTIONTEXT      = 9;
const int COLOR_ACTIVEBORDER     = 10;
const int COLOR_INACTIVEBORDER   = 11;
const int COLOR_APPWORKSPACE     = 12;
const int COLOR_HIGHLIGHT        = 13;
const int COLOR_HIGHLIGHTTEXT    = 14;
const int COLOR_BTNFACE          = 15;
const int COLOR_BTNSHADOW        = 16;
const int COLOR_GRAYTEXT         = 17;
const int COLOR_BTNTEXT          = 18;
const int COLOR_INACTIVECAPTIONTEXT= 19;
const int COLOR_BTNHIGHLIGHT     = 20;
const int COLOR_3DDKSHADOW       = 21;
const int COLOR_3DLIGHT          = 22;
const int COLOR_INFOTEXT         = 23;
const int COLOR_INFOBK           = 24;
const int COLOR_HOTLIGHT                 = 26;
const int COLOR_GRADIENTACTIVECAPTION    = 27;
const int COLOR_GRADIENTINACTIVECAPTION  = 28;

const int COLOR_DESKTOP         =  COLOR_BACKGROUND;
const int COLOR_3DFACE          =  COLOR_BTNFACE;
const int COLOR_3DSHADOW        =  COLOR_BTNSHADOW;
const int COLOR_3DHIGHLIGHT     =  COLOR_BTNHIGHLIGHT;
const int COLOR_3DHILIGHT       =  COLOR_BTNHIGHLIGHT;
const int COLOR_BTNHILIGHT      =  COLOR_BTNHIGHLIGHT;

[extern "gdi32.dll"]
HGDIOBJ GetStockObject (int n);

const int WHITE_BRUSH        = 0;
const int LTGRAY_BRUSH       = 1;
const int GRAY_BRUSH         = 2;
const int DKGRAY_BRUSH       = 3;
const int BLACK_BRUSH        = 4;
const int NULL_BRUSH         = 5;
const int HOLLOW_BRUSH       = NULL_BRUSH;
const int WHITE_PEN          = 6;
const int BLACK_PEN          = 7;
const int NULL_PEN           = 8;
const int OEM_FIXED_FONT     = 10;
const int ANSI_FIXED_FONT    = 11;
const int ANSI_VAR_FONT      = 12;
const int SYSTEM_FONT        = 13;
const int DEVICE_DEFAULT_FONT= 14;
const int DEFAULT_PALETTE    = 15;
const int SYSTEM_FIXED_FONT  = 16;
const int DEFAULT_GUI_FONT   = 17;
const int DC_BRUSH           = 18;
const int DC_PEN             = 19;

const int BS_SOLID           = 0;
const int BS_NULL            = 1;
const int BS_HOLLOW          = BS_NULL;
const int BS_HATCHED         = 2;
const int BS_PATTERN         = 3;
const int BS_INDEXED         = 4;
const int BS_DIBPATTERN      = 5;
const int BS_DIBPATTERNPT    = 6;
const int BS_PATTERN8X8      = 7;
const int BS_DIBPATTERN8X8   = 8;
const int BS_MONOPATTERN     = 9;

const int HS_HORIZONTAL      = 0;
const int HS_VERTICAL        = 1;
const int HS_FDIAGONAL       = 2;
const int HS_BDIAGONAL       = 3;
const int HS_CROSS           = 4;
const int HS_DIAGCROSS       = 5;

const int PS_SOLID           = 0;
const int PS_DASH            = 1;
const int PS_DOT             = 2;
const int PS_DASHDOT         = 3;
const int PS_DASHDOTDOT      = 4;
const int PS_NULL            = 5;
const int PS_INSIDEFRAME     = 6;
const int PS_USERSTYLE       = 7;
const int PS_ALTERNATE       = 8;
const int PS_STYLE_MASK      = 0x0000000F;

const int PS_ENDCAP_ROUND    = 0x00000000;
const int PS_ENDCAP_SQUARE   = 0x00000100;
const int PS_ENDCAP_FLAT     = 0x00000200;
const int PS_ENDCAP_MASK     = 0x00000F00;

const int PS_JOIN_ROUND      = 0x00000000;
const int PS_JOIN_BEVEL      = 0x00001000;
const int PS_JOIN_MITER      = 0x00002000;
const int PS_JOIN_MASK       = 0x0000F000;

const int PS_COSMETIC        = 0x00000000;
const int PS_GEOMETRIC       = 0x00010000;
const int PS_TYPE_MASK       = 0x000F0000;

[extern "Gdi32.dll"]
int GetDeviceCaps (HDC hdc, int nIndex);

const int HORZRES = 8;
const int VERTRES = 10;
const int PHYSICALOFFSETY = 113;
const int PHYSICALOFFSETX = 112;
const int PHYSICALHEIGHT  = 111;
const int PHYSICALWIDTH   = 110;

struct DEVMODE
{
  char dmDeviceName[32];
  WORD  dmSpecVersion;
  WORD  dmDriverVersion;
  WORD  dmSize;
  WORD  dmDriverExtra;
  DWORD dmFields;
  short dmOrientation;
  short dmPaperSize;
  short dmPaperLength;
  short dmPaperWidth;
  short dmScale;
  short dmCopies;
  short dmDefaultSource;
  short dmPrintQuality;
  short dmColor;
  short dmDuplex;
  short dmYResolution;
  short dmTTOption;
  short dmCollate;
  char  dmFormName[32];
  WORD  dmLogPixels;
  DWORD dmBitsPerPel;
  DWORD dmPelsWidth;
  DWORD dmPelsHeight;
  DWORD dmDisplayFlags;
  DWORD dmDisplayFrequency;
  DWORD dmICMMethod;
  DWORD dmICMIntent;
  DWORD dmMediaType;
  DWORD dmDitherType;
  DWORD dmReserved1;
  DWORD dmReserved2;
  DWORD dmPanningWidth;
  DWORD dmPanningHeight;
}

struct DOCINFO
{
  uint    cbSize;
  LPCTSTR lpszDocName;
  LPCTSTR lpszOutput;
  LPCTSTR lpszDatatype;
  DWORD   fwType;
}

// dummy declarations
typedef byte* LPPRINTHOOKPROC;
typedef byte* LPSETUPHOOKPROC;
typedef byte* LPPRINTER_DEFAULTS;
typedef byte* PSECURITY_DESCRIPTOR;

#if MEM32
packed
#endif
struct PRINTDLG
{
  DWORD           lStructSize;
  HWND            hwndOwner;
  HGLOBAL         hDevMode;
  HGLOBAL         hDevNames;
  HDC             hDC;
  DWORD           Flags;
  WORD            nFromPage;
  WORD            nToPage;
  WORD            nMinPage;
  WORD            nMaxPage;
  WORD            nCopies;
  HINSTANCE       hInstance;
  LPARAM          lCustData;
  LPPRINTHOOKPROC lpfnPrintHook;
  LPSETUPHOOKPROC lpfnSetupHook;
  LPCTSTR         lpPrintTemplateName;
  LPCTSTR         lpSetupTemplateName;
  HGLOBAL         hPrintTemplate;
  HGLOBAL         hSetupTemplate;
}

const uint PD_RETURNDEFAULT = 0x00000400;
const uint PD_RETURNDC      = 0x00000100;
const uint PD_NOWARNING     = 0x00000080;

[extern "Comdlg32.dll"]
BOOL PrintDlgA (PRINTDLG* lppd);

typedef DEVMODE* LPDEVMODE;

[extern "winspool.drv"]
BOOL GetDefaultPrinterA (LPTSTR pszBuffer, LPDWORD pcchBuffer);

[extern "Winspool.drv"]
BOOL OpenPrinterA (LPTSTR pPrinterName, HANDLE* phPrinter, LPPRINTER_DEFAULTS pDefault);

struct PRINTER_INFO_2
{
  LPTSTR               pServerName;
  LPTSTR               pPrinterName;
  LPTSTR               pShareName;
  LPTSTR               pPortName;
  LPTSTR               pDriverName;
  LPTSTR               pComment;
  LPTSTR               pLocation;
  LPDEVMODE            pDevMode;
  LPTSTR               pSepFile;
  LPTSTR               pPrintProcessor;
  LPTSTR               pDatatype;
  LPTSTR               pParameters;
  PSECURITY_DESCRIPTOR pSecurityDescriptor;
  DWORD                Attributes;
  DWORD                Priority;
  DWORD                DefaultPriority;
  DWORD                StartTime;
  DWORD                UntilTime;
  DWORD                Status;
  DWORD                cJobs;
  DWORD                AveragePPM;
}

const int SP_ERROR = -1;
const uint DM_TTOPTION = 0x00004000;
const short DMTT_BITMAP = 1;       // force mode bitmap

[extern "Winspool.drv"]
BOOL GetPrinterA (HANDLE hPrinter, DWORD Level, LPBYTE pPrinter, DWORD cbBuf, LPDWORD pcbNeeded);

[extern "Spoolss.dll"]
BOOL ClosePrinter (HANDLE hPrinter);

[extern "Gdi32.dll"]
int StartDocA (HDC hdc, DOCINFO *lpdi);

[extern "Gdi32.dll"]
int AbortDoc (HDC hdc);

[extern "Gdi32.dll"]
int EndDoc (HDC hdc);

[extern "Gdi32.dll"]
int StartPage (HDC hDC);

[extern "Gdi32.dll"]
int EndPage (HDC hDC);

[extern "Gdi32.dll"]
HDC ResetDCA (HDC hdc, DEVMODE *lpInitData);

[extern "Gdi32.dll"]
HDC CreateDCA (LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, DEVMODE* lpInitData);

const int LF_FACESIZE = 32;
const int LF_FULLFACESIZE = 64;

struct LOGFONT
{
  LONG  lfHeight;
  LONG  lfWidth;
  LONG  lfEscapement;
  LONG  lfOrientation;
  LONG  lfWeight;
  BYTE  lfItalic;
  BYTE  lfUnderline;
  BYTE  lfStrikeOut;
  BYTE  lfCharSet;
  BYTE  lfOutPrecision;
  BYTE  lfClipPrecision;
  BYTE  lfQuality;
  BYTE  lfPitchAndFamily;
  TCHAR lfFaceName[LF_FACESIZE];
}

struct ENUMLOGFONT
{
  LOGFONT elfLogFont;
  TCHAR   elfFullName[LF_FULLFACESIZE];
  TCHAR   elfStyle[LF_FACESIZE];
}

struct NEWTEXTMETRIC
{
  LONG  tmHeight;
  LONG  tmAscent;
  LONG  tmDescent;
  LONG  tmInternalLeading;
  LONG  tmExternalLeading;
  LONG  tmAveCharWidth;
  LONG  tmMaxCharWidth;
  LONG  tmWeight;
  LONG  tmOverhang;
  LONG  tmDigitizedAspectX;
  LONG  tmDigitizedAspectY;
  TCHAR tmFirstChar;
  TCHAR tmLastChar;
  TCHAR tmDefaultChar;
  TCHAR tmBreakChar;
  BYTE  tmItalic;
  BYTE  tmUnderlined;
  BYTE  tmStruckOut;
  BYTE  tmPitchAndFamily;
  BYTE  tmCharSet;
  DWORD ntmFlags;
  UINT  ntmSizeEM;
  UINT  ntmCellHeight;
  UINT  ntmAvgWidth;
}

packed struct RGBQUAD
{
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
}

packed struct BITMAPINFO
{
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD          bmiColors[1];
}

typedef [callback]
int FONTENUMPROC (ENUMLOGFONT *lpelf,  NEWTEXTMETRIC *lpntm, DWORD FontType, LPARAM lParam);

[extern "Gdi32.dll"]
int EnumFontFamilies (HDC hdc, LPCTSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);

[extern "Gdi32.dll"]
int StretchDIBits (HDC hdc, int XDest, int YDest, int nDestWidth, int nDestHeight, int XSrc, int YSrc, int nSrcWidth, int nSrcHeight,
                   VOID *lpBits, BITMAPINFO *lpBitsInfo, UINT iUsage, DWORD dwRop);

const uint FF_MODERN = (3<<4);

const uint DM_COLOR = 0x00000800;
const short DMCOLOR_COLOR = 2;

const int GDI_ERROR = -1;

//------------------------------------------------------------------------------

[extern "user32.dll"]
UINT_PTR SetTimer (HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);

[extern "user32.dll"]
BOOL KillTimer (HWND hWnd, UINT_PTR uIDEvent);

[extern "user32.dll"]
SHORT GetKeyState (int nVirtKey);

[extern "user32.dll"]
LONG GetDialogBaseUnits ();

[extern "user32.dll"]
HWND GetWindow (HWND hWnd, UINT uCmd);

const uint GW_HWNDFIRST = 0;
const uint GW_HWNDLAST  = 1;
const uint GW_HWNDNEXT  = 2;
const uint GW_HWNDPREV  = 3;
const uint GW_OWNER     = 4;
const uint GW_CHILD     = 5;
const uint GW_ENABLEDPOPUP = 6;

packed struct LOGBRUSH
{
  UINT      lbStyle;
  COLORREF  lbColor;
  ULONG_PTR lbHatch;
}

[extern "gdi32.dll"]
HBRUSH CreateBrushIndirect (LOGBRUSH *lplb);

//------------------------------------------------------------------------------

[extern "user32.dll"]
BOOL OpenClipboard (HWND hWndNewOwner);

[extern "user32.dll"]
BOOL EmptyClipboard ();

[extern "user32.dll"]
HANDLE GetClipboardData (UINT uFormat);

[extern "user32.dll"]
HANDLE SetClipboardData (UINT uFormat, HANDLE hMem);

[extern "user32.dll"]
BOOL CloseClipboard ();

//------------------------------------------------------------------------------

struct NOTIFYICONDATA
{
  DWORD cbSize;
  HWND  hWnd;
  UINT  uID;
  UINT  uFlags;
  UINT  uCallbackMessage;
  HICON hIcon;
  char  szTip[128];
  DWORD dwState;
  DWORD dwStateMask;
  char  szInfo[256];
  UINT  uTimeout;
  char  szInfoTitle[64];
  DWORD dwInfoFlags;
  byte[16] guidItem;
  HICON hBalloonIcon;
}

const uint NIF_MESSAGE = 0x00000001;
const uint NIF_ICON    = 0x00000002;
const uint NIF_TIP     = 0x00000004;

[extern "shell32.dll"]
BOOL Shell_NotifyIconA (DWORD dwMessage, NOTIFYICONDATA* lpdata);

[extern "Shell32.dll"]
HINSTANCE ShellExecuteW
    (HWND hwnd,
     wchar* lpOperation,
     wchar* lpFile,
     wchar* lpParameters,
     wchar* lpDirectory,
     INT nShowCmd);

//----------------------------------------------------------------------------------------------------

const DWORD NIM_ADD    = 0x00000000;
const DWORD NIM_DELETE = 0x00000002;

[extern "user32.dll"]
UINT RegisterWindowMessageA (LPCSTR lpString);

//------------------------------------------------------------------------------

[extern "kernel32.dll"]
HGLOBAL GlobalAlloc (UINT uFlags, SIZE_T dwBytes);

const UINT GMEM_MOVEABLE = 0x0002;

[extern "kernel32.dll"]
HGLOBAL GlobalFree (HGLOBAL hMem);

[extern "kernel32.dll"]
LPVOID GlobalLock (HGLOBAL hMem);

[extern "kernel32.dll"]
SIZE_T GlobalSize (HGLOBAL hMem);

[extern "kernel32.dll"]
BOOL GlobalUnlock (HGLOBAL hMem);

[extern "kernel32.dll"]
HANDLE CreateEventA
  (LPSECURITY_ATTRIBUTES lpEventAttributes,
   BOOL bManualReset,
   BOOL bInitialState,
   LPCSTR lpName);

[extern "kernel32.dll"]
BOOL SetEvent (HANDLE hEvent);

[extern "kernel32.dll"]
DWORD WaitForSingleObject (HANDLE hHandle, DWORD dwMilliseconds);

const DWORD WAIT_TIMEOUT   = 0x00000102;
const DWORD WAIT_FAILED    = 0xFFFFFFFF;
const DWORD WAIT_ABANDONED = 0x00000080;  // thread was killed





[extern "kernel32.dll"]
HRSRC FindResourceA (HMODULE hModule, INT_PTR lpName, INT_PTR lpType);

[extern "kernel32.dll"]
HRSRC FindResourceExA (HMODULE hModule, INT_PTR lpType, INT_PTR lpName, WORD wLanguage);

[extern "kernel32.dll"]
HGLOBAL LoadResource (HMODULE hModule, HRSRC hResInfo);

[extern "kernel32.dll"]
LPVOID LockResource (HGLOBAL hResData);

[extern "kernel32.dll"]
DWORD SizeofResource (HMODULE hModule, HRSRC hResInfo);

typedef [callback]
BOOL ENUMRESLANGPROCA (HMODULE hModule, INT_PTR lpszType, INT_PTR lpszName, WORD wIDLanguage, DWORD lParam);

[extern "kernel32.dll"]
BOOL EnumResourceLanguagesA(
  HMODULE          hModule,
  INT_PTR          lpType,
  INT_PTR          lpName,
  ENUMRESLANGPROCA lpEnumFunc,
  LONG_PTR         lParam);

typedef WORD LANGID;

[extern "kernel32.dll"]
BOOL EnumResourceLanguagesExA(
   HMODULE          hModule,
   INT_PTR          lpType,
   INT_PTR          lpName,
   ENUMRESLANGPROCA lpEnumFunc,
   LONG_PTR         lParam,
   DWORD            dwFlags,
   LANGID           LangId);

typedef [callback]
BOOL ENUMRESNAMEPROCA (HMODULE hModule, INT_PTR lpType, INT_PTR lpName, LONG_PTR lParam);

[extern "kernel32.dll"]
BOOL EnumResourceNamesA(
   HMODULE          hModule,
   INT_PTR          lpType,
   ENUMRESNAMEPROCA lpEnumFunc,
   LONG_PTR         lParam);

typedef [callback]
BOOL ENUMRESTYPEPROCA (
  HMODULE hModule,
  INT_PTR lpType,
  LONG_PTR lParam);

[extern "kernel32.dll"]
BOOL EnumResourceTypesA (
  HMODULE          hModule,
  ENUMRESTYPEPROCA lpEnumFunc,
  LONG_PTR         lParam);


uint RGB (uint r, uint g, uint b);

[extern "user32.dll"]
HMENU LoadMenuA (HINSTANCE hInstance, LPCSTR lpMenuName);

[extern "user32.dll"]
HMENU LoadMenuW (HINSTANCE hInstance, LPCWSTR lpMenuName);

[extern "user32.dll"]
HMENU LoadMenuIndirectA (MENUTEMPLATEA *lpMenuTemplate);

[extern "user32.dll"]
HMENU LoadMenuIndirectW (MENUTEMPLATEW *lpMenuTemplate);

[extern "user32.dll"]
HMENU GetMenu (HWND hWnd);

[extern "user32.dll"]
BOOL SetMenu (HWND hWnd, HMENU hMenu);

[extern "user32.dll"]
BOOL HiliteMenuItem (HWND hWnd, HMENU hMenu, UINT uIDHiliteItem, UINT uHilite);

[extern "user32.dll"]
int GetMenuStringA (HMENU hMenu, UINT uIDItem, LPSTR lpString, int nMaxCount, UINT uFlag);

[extern "user32.dll"]
int GetMenuStringW (HMENU hMenu, UINT uIDItem, LPWSTR lpString, int nMaxCount, UINT uFlag);

[extern "user32.dll"]
UINT GetMenuState (HMENU hMenu, UINT uId, UINT uFlags);

[extern "user32.dll"]
BOOL DrawMenuBar (HWND hWnd);

[extern "user32.dll"]
HMENU GetSystemMenu (HWND hWnd, BOOL bRevert);

[extern "user32.dll"]
HMENU CreateMenu ();

[extern "user32.dll"]
HMENU CreatePopupMenu ();

[extern "user32.dll"]
BOOL DestroyMenu (HMENU hMenu);

[extern "user32.dll"]
DWORD CheckMenuItem (HMENU hMenu, UINT uIDCheckItem, UINT uCheck);

[extern "user32.dll"]
BOOL EnableMenuItem (HMENU hMenu, UINT uIDEnableItem, UINT uEnable);

[extern "user32.dll"]
HMENU GetSubMenu (HMENU hMenu, int nPos);

[extern "user32.dll"]
UINT GetMenuItemID (HMENU hMenu, int nPos);

[extern "user32.dll"]
int GetMenuItemCount (HMENU hMenu);

[extern "user32.dll"]
BOOL InsertMenuA (HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem);

[extern "user32.dll"]
BOOL InsertMenuW (HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);

[extern "user32.dll"]
BOOL AppendMenuA (HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem);

[extern "user32.dll"]
BOOL AppendMenuW (HMENU hMenu, UINT uFlags, HMENU uIDNewItem, LPCWSTR lpNewItem);

[extern "user32.dll"]
BOOL ModifyMenuA (HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem);

[extern "user32.dll"]
BOOL ModifyMenuW (HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);

[extern "user32.dll"]
BOOL RemoveMenu (HMENU hMenu, UINT uPosition, UINT uFlags);

[extern "user32.dll"]
BOOL DeleteMenu (HMENU hMenu, UINT uPosition, UINT uFlags);

[extern "user32.dll"]
BOOL SetMenuItemBitmaps (HMENU hMenu, UINT uPosition, UINT uFlags, HBITMAP hBitmapUnchecked, HBITMAP hBitmapChecked);

[extern "user32.dll"]
LONG GetMenuCheckMarkDimensions ();

[extern "user32.dll"]
BOOL TrackPopupMenu (HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, RECT *prcRect);

const int MNC_IGNORE  =0;
const int MNC_CLOSE   =1;
const int MNC_EXECUTE =2;
const int MNC_SELECT  =3;

struct TPMPARAMS
{
  UINT  cbSize;
  RECT  rcExclude;
}
typedef TPMPARAMS *LPTPMPARAMS;

[extern "user32.dll"]
BOOL TrackPopupMenuEx (HMENU hmenu, UINT m, int a, int b, HWND h, LPTPMPARAMS p);

const uint MNS_NOCHECK       =  0x80000000;
const uint MNS_MODELESS      =  0x40000000;
const uint MNS_DRAGDROP      =  0x20000000;
const uint MNS_AUTODISMISS   =  0x10000000;
const uint MNS_NOTIFYBYPOS   =  0x08000000;
const uint MNS_CHECKORBMP    =  0x04000000;

const uint MIM_MAXHEIGHT            =   0x00000001;
const uint MIM_BACKGROUND           =   0x00000002;
const uint MIM_HELPID               =   0x00000004;
const uint MIM_MENUDATA             =   0x00000008;
const uint MIM_STYLE                =   0x00000010;
const uint MIM_APPLYTOSUBMENUS      =   0x80000000;

struct MENUINFO
{
  DWORD   cbSize;
  DWORD   fMask;
  DWORD   dwStyle;
  UINT    cyMax;
  HBRUSH  hbrBack;
  DWORD   dwContextHelpID;
  INT_PTR dwMenuData;
}

typedef MENUINFO *LPCMENUINFO;

[extern "user32.dll"]
BOOL GetMenuInfo (HMENU hmenu, LPCMENUINFO m);

[extern "user32.dll"]
BOOL SetMenuInfo (HMENU hmenu, LPCMENUINFO m);

[extern "user32.dll"]
BOOL EndMenu ();

const int MND_CONTINUE      = 0;
const int MND_ENDMENU       = 1;

struct MENUGETOBJECTINFO
{
    DWORD dwFlags;
    UINT uPos;
    HMENU hmenu;
    PVOID riid;
    PVOID pvObj;
}

const uint MNGOF_GAP          =  0x00000003;

const uint MNGO_NOINTERFACE   =  0x00000000;
const uint MNGO_NOERROR       =  0x00000001;

const uint MIIM_STATE      = 0x00000001;
const uint MIIM_ID         = 0x00000002;
const uint MIIM_SUBMENU    = 0x00000004;
const uint MIIM_CHECKMARKS = 0x00000008;
const uint MIIM_TYPE       = 0x00000010;
const uint MIIM_DATA       = 0x00000020;
const uint MIIM_STRING     = 0x00000040;
const uint MIIM_BITMAP     = 0x00000080;
const uint MIIM_FTYPE      = 0x00000100;

const int HBMMENU_CALLBACK          =  ( -1);
const int HBMMENU_SYSTEM            =  (  1);
const int HBMMENU_MBAR_RESTORE      =  (  2);
const int HBMMENU_MBAR_MINIMIZE     =  (  3);
const int HBMMENU_MBAR_CLOSE        =  (  5);
const int HBMMENU_MBAR_CLOSE_D      =  (  6);
const int HBMMENU_MBAR_MINIMIZE_D   =  (  7);
const int HBMMENU_POPUP_CLOSE       =  (  8);
const int HBMMENU_POPUP_RESTORE     =  (  9);
const int HBMMENU_POPUP_MAXIMIZE    =  ( 10);
const int HBMMENU_POPUP_MINIMIZE    =  ( 11);

struct MENUITEMINFOA
{
    UINT    cbSize;
    UINT    fMask;
    UINT    fType;
    UINT    fState;
    UINT    wID;
    HMENU   hSubMenu;
    HBITMAP hbmpChecked;
    HBITMAP hbmpUnchecked;
    DWORD   dwItemData;
    LPSTR   dwTypeData;
    UINT    cch;
    HBITMAP hbmpItem;
}

typedef MENUITEMINFOA* LPMENUITEMINFOA;

struct MENUITEMINFOW
{
    UINT    cbSize;
    UINT    fMask;
    UINT    fType;
    UINT    fState;
    UINT    wID;
    HMENU   hSubMenu;
    HBITMAP hbmpChecked;
    HBITMAP hbmpUnchecked;
    DWORD   dwItemData;
    LPWSTR  dwTypeData;
    UINT    cch;
    HBITMAP hbmpItem;
}

typedef MENUITEMINFOW* LPMENUITEMINFOW;

[extern "user32.dll"]
BOOL InsertMenuItemA (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOA m);

[extern "user32.dll"]
BOOL InsertMenuItemW (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOW m);

[extern "user32.dll"]
BOOL GetMenuItemInfoA (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOA m);

[extern "user32.dll"]
BOOL GetMenuItemInfoW (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOW m);

[extern "user32.dll"]
BOOL SetMenuItemInfoA (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOA m);

[extern "user32.dll"]
BOOL SetMenuItemInfoW (HMENU hmenu, UINT u, BOOL b, LPMENUITEMINFOW m);

const int GMDI_USEDISABLED   = 0x0001;
const int GMDI_GOINTOPOPUPS  = 0x0002;

[extern "user32.dll"]
UINT GetMenuDefaultItem (HMENU hMenu, UINT fByPos, UINT gmdiFlags);

[extern "user32.dll"]
BOOL SetMenuDefaultItem (HMENU hMenu, UINT uItem, UINT fByPos);

[extern "user32.dll"]
BOOL GetMenuItemRect (HWND hWnd, HMENU hMenu, UINT uItem, RECT* lprcItem);

[extern "user32.dll"]
int MenuItemFromPoint (HWND hWnd, HMENU hMenu, POINT* ptScreen);

const uint TPM_LEFTBUTTON  = 0x0000;
const uint TPM_RIGHTBUTTON = 0x0002;
const uint TPM_LEFTALIGN   = 0x0000;
const uint TPM_CENTERALIGN = 0x0004;
const uint TPM_RIGHTALIGN  = 0x0008;

const uint TPM_TOPALIGN      =  0x0000;
const uint TPM_VCENTERALIGN  =  0x0010;
const uint TPM_BOTTOMALIGN   =  0x0020;

const uint TPM_HORIZONTAL    =  0x0000;
const uint TPM_VERTICAL      =  0x0040;
const uint TPM_NONOTIFY      =  0x0080;
const uint TPM_RETURNCMD     =  0x0100;
const uint TPM_RECURSE       =  0x0001;

const uint MF_INSERT          = 0x00000000;
const uint MF_CHANGE          = 0x00000080;
const uint MF_APPEND          = 0x00000100;
const uint MF_DELETE          = 0x00000200;
const uint MF_REMOVE          = 0x00001000;

const uint MF_BYCOMMAND       = 0x00000000;
const uint MF_BYPOSITION      = 0x00000400;
const uint MF_SEPARATOR       = 0x00000800;

const uint MF_ENABLED         = 0x00000000;
const uint MF_GRAYED          = 0x00000001;
const uint MF_DISABLED        = 0x00000002;

const uint MF_UNCHECKED       = 0x00000000;
const uint MF_CHECKED         = 0x00000008;
const uint MF_USECHECKBITMAPS = 0x00000200;

const uint MF_STRING         =  0x00000000;
const uint MF_BITMAP         =  0x00000004;
const uint MF_OWNERDRAW      =  0x00000100;

const uint MF_POPUP          =  0x00000010;
const uint MF_MENUBARBREAK   =  0x00000020;
const uint MF_MENUBREAK      =  0x00000040;

const uint MF_UNHILITE       =  0x00000000;
const uint MF_HILITE         =  0x00000080;

const uint MF_DEFAULT        =  0x00001000;
const uint MF_SYSMENU        =  0x00002000;
const uint MF_HELP           =  0x00004000;
const uint MF_RIGHTJUSTIFY   =  0x00004000;
const uint MF_MOUSESELECT    =  0x00008000;
const uint MF_END            =  0x00000080;

const uint MFT_STRING        =  MF_STRING;
const uint MFT_BITMAP        =  MF_BITMAP;
const uint MFT_MENUBARBREAK  =  MF_MENUBARBREAK;
const uint MFT_MENUBREAK     =  MF_MENUBREAK;
const uint MFT_OWNERDRAW     =  MF_OWNERDRAW;
const uint MFT_RADIOCHECK    =  0x00000200;
const uint MFT_SEPARATOR     =  MF_SEPARATOR;
const uint MFT_RIGHTORDER    =  0x00002000;
const uint MFT_RIGHTJUSTIFY  =  MF_RIGHTJUSTIFY;

const uint MFS_GRAYED        =  0x00000003;
const uint MFS_DISABLED      =  MFS_GRAYED;
const uint MFS_CHECKED       =  MF_CHECKED;
const uint MFS_HILITE        =  MF_HILITE;
const uint MFS_ENABLED       =  MF_ENABLED;
const uint MFS_UNCHECKED     =  MF_UNCHECKED;
const uint MFS_UNHILITE      =  MF_UNHILITE;
const uint MFS_DEFAULT       =  MF_DEFAULT;
const uint MFS_MASK          =  0x0000108B;
const uint MFS_HOTTRACKDRAWN =  0x10000000;
const uint MFS_CACHEDBMP     =  0x20000000;
const uint MFS_BOTTOMGAPDROP =  0x40000000;
const uint MFS_TOPGAPDROP    =  0x80000000;
const uint MFS_GAPDROP       =  0xC0000000;

const uint EN_SETFOCUS        = 0x0100;
const uint EN_KILLFOCUS       = 0x0200;
const uint EN_CHANGE          = 0x0300;
const uint EN_UPDATE          = 0x0400;
const uint EN_ERRSPACE        = 0x0500;
const uint EN_MAXTEXT         = 0x0501;
const uint EN_HSCROLL         = 0x0601;
const uint EN_VSCROLL         = 0x0602;

const uint EC_LEFTMARGIN      = 0x0001;
const uint EC_RIGHTMARGIN     = 0x0002;
const uint EC_USEFONTINFO     = 0xffff;

const uint EMSIS_COMPOSITIONSTRING        =0x0001;
const uint EIMES_GETCOMPSTRATONCE         =0x0001;
const uint EIMES_CANCELCOMPSTRINFOCUS     =0x0002;
const uint EIMES_COMPLETECOMPSTRKILLFOCUS =0x0004;

const uint WB_LEFT           = 0;
const uint WB_RIGHT          = 1;
const uint WB_ISDELIMITER    = 2;

const uint BS_PUSHBUTTON      = 0x00000000;
const uint BS_DEFPUSHBUTTON   = 0x00000001;
const uint BS_CHECKBOX        = 0x00000002;
const uint BS_AUTOCHECKBOX    = 0x00000003;
const uint BS_RADIOBUTTON     = 0x00000004;
const uint BS_3STATE          = 0x00000005;
const uint BS_AUTO3STATE      = 0x00000006;
const uint BS_GROUPBOX        = 0x00000007;
const uint BS_USERBUTTON      = 0x00000008;
const uint BS_AUTORADIOBUTTON = 0x00000009;
const uint BS_OWNERDRAW       = 0x0000000B;
const uint BS_LEFTTEXT        = 0x00000020;
const uint BS_TEXT            = 0x00000000;
const uint BS_ICON            = 0x00000040;
const uint BS_BITMAP          = 0x00000080;
const uint BS_LEFT            = 0x00000100;
const uint BS_RIGHT           = 0x00000200;
const uint BS_CENTER          = 0x00000300;
const uint BS_TOP             = 0x00000400;
const uint BS_BOTTOM          = 0x00000800;
const uint BS_VCENTER         = 0x00000C00;
const uint BS_PUSHLIKE        = 0x00001000;
const uint BS_MULTILINE       = 0x00002000;
const uint BS_NOTIFY          = 0x00004000;
const uint BS_FLAT            = 0x00008000;
const uint BS_RIGHTBUTTON     = BS_LEFTTEXT;

const uint BN_CLICKED         = 0;
const uint BN_PAINT           = 1;
const uint BN_HILITE          = 2;
const uint BN_UNHILITE        = 3;
const uint BN_DISABLE         = 4;
const uint BN_DOUBLECLICKED   = 5;
const uint BN_PUSHED          = BN_HILITE;
const uint BN_UNPUSHED        = BN_UNHILITE;
const uint BN_DBLCLK          = BN_DOUBLECLICKED;
const uint BN_SETFOCUS        = 6;
const uint BN_KILLFOCUS       = 7;

const uint BM_GETCHECK      =  0x00F0;
const uint BM_SETCHECK      =  0x00F1;
const uint BM_GETSTATE      =  0x00F2;
const uint BM_SETSTATE      =  0x00F3;
const uint BM_SETSTYLE      =  0x00F4;
const uint BM_CLICK         =  0x00F5;
const uint BM_GETIMAGE      =  0x00F6;
const uint BM_SETIMAGE      =  0x00F7;

const uint BST_UNCHECKED     = 0x0000;
const uint BST_CHECKED       = 0x0001;
const uint BST_INDETERMINATE = 0x0002;
const uint BST_PUSHED        = 0x0004;
const uint BST_FOCUS         = 0x0008;

const uint SS_LEFT           =  0x00000000;
const uint SS_CENTER         =  0x00000001;
const uint SS_RIGHT          =  0x00000002;
const uint SS_ICON           =  0x00000003;
const uint SS_BLACKRECT      =  0x00000004;
const uint SS_GRAYRECT       =  0x00000005;
const uint SS_WHITERECT      =  0x00000006;
const uint SS_BLACKFRAME     =  0x00000007;
const uint SS_GRAYFRAME      =  0x00000008;
const uint SS_WHITEFRAME     =  0x00000009;
const uint SS_USERITEM       =  0x0000000A;
const uint SS_SIMPLE         =  0x0000000B;
const uint SS_LEFTNOWORDWRAP =  0x0000000C;

const uint SS_OWNERDRAW       = 0x0000000D;
const uint SS_BITMAP          = 0x0000000E;
const uint SS_ENHMETAFILE     = 0x0000000F;
const uint SS_ETCHEDHORZ      = 0x00000010;
const uint SS_ETCHEDVERT      = 0x00000011;
const uint SS_ETCHEDFRAME     = 0x00000012;
const uint SS_TYPEMASK        = 0x0000001F;
const uint SS_NOPREFIX        = 0x00000080;
const uint SS_NOTIFY          = 0x00000100;
const uint SS_CENTERIMAGE     = 0x00000200;
const uint SS_RIGHTJUST       = 0x00000400;
const uint SS_REALSIZEIMAGE   = 0x00000800;
const uint SS_SUNKEN          = 0x00001000;
const uint SS_ENDELLIPSIS     = 0x00004000;
const uint SS_PATHELLIPSIS    = 0x00008000;
const uint SS_WORDELLIPSIS    = 0x0000C000;
const uint SS_ELLIPSISMASK    = 0x0000C000;
const uint STM_SETICON        = 0x0170;
const uint STM_GETICON        = 0x0171;
const uint STM_SETIMAGE       = 0x0172;
const uint STM_GETIMAGE       = 0x0173;
const uint STN_CLICKED        = 0;
const uint STN_DBLCLK         = 1;
const uint STN_ENABLE         = 2;
const uint STN_DISABLE        = 3;

const int LB_OKAY            = 0;
const int LB_ERR             = (-1);
const int LBN_ERRSPACE       = (-2);
const int LBN_SELCHANGE      = 1;
const int LBN_DBLCLK         = 2;
const int LBN_SELCANCEL      = 3;
const int LBN_SETFOCUS       = 4;
const int LBN_KILLFOCUS      = 5;

const uint LB_ADDSTRING          =  0x0180;
const uint LB_INSERTSTRING       =  0x0181;
const uint LB_DELETESTRING       =  0x0182;
const uint LB_SELITEMRANGEEX     =  0x0183;
const uint LB_RESETCONTENT       =  0x0184;
const uint LB_SETSEL             =  0x0185;
const uint LB_SETCURSEL          =  0x0186;
const uint LB_GETSEL             =  0x0187;
const uint LB_GETCURSEL          =  0x0188;
const uint LB_GETTEXT            =  0x0189;
const uint LB_GETTEXTLEN         =  0x018A;
const uint LB_GETCOUNT           =  0x018B;
const uint LB_SELECTSTRING       =  0x018C;
const uint LB_DIR                =  0x018D;
const uint LB_GETTOPINDEX        =  0x018E;
const uint LB_FINDSTRING         =  0x018F;
const uint LB_GETSELCOUNT        =  0x0190;
const uint LB_GETSELITEMS        =  0x0191;
const uint LB_SETTABSTOPS        =  0x0192;
const uint LB_GETHORIZONTALEXTENT=  0x0193;
const uint LB_SETHORIZONTALEXTENT=  0x0194;
const uint LB_SETCOLUMNWIDTH     =  0x0195;
const uint LB_ADDFILE            =  0x0196;
const uint LB_SETTOPINDEX        =  0x0197;
const uint LB_GETITEMRECT        =  0x0198;
const uint LB_GETITEMDATA        =  0x0199;
const uint LB_SETITEMDATA        =  0x019A;
const uint LB_SELITEMRANGE       =  0x019B;
const uint LB_SETANCHORINDEX     =  0x019C;
const uint LB_GETANCHORINDEX     =  0x019D;
const uint LB_SETCARETINDEX      =  0x019E;
const uint LB_GETCARETINDEX      =  0x019F;
const uint LB_SETITEMHEIGHT      =  0x01A0;
const uint LB_GETITEMHEIGHT      =  0x01A1;
const uint LB_FINDSTRINGEXACT    =  0x01A2;
const uint LB_SETLOCALE          =  0x01A5;
const uint LB_GETLOCALE          =  0x01A6;
const uint LB_SETCOUNT           =  0x01A7;
const uint LB_INITSTORAGE        =  0x01A8;
const uint LB_ITEMFROMPOINT      =  0x01A9;

const uint LBS_NOTIFY           = 0x0001;
const uint LBS_SORT             = 0x0002;
const uint LBS_NOREDRAW         = 0x0004;
const uint LBS_MULTIPLESEL      = 0x0008;
const uint LBS_OWNERDRAWFIXED   = 0x0010;
const uint LBS_OWNERDRAWVARIABLE= 0x0020;
const uint LBS_HASSTRINGS       = 0x0040;
const uint LBS_USETABSTOPS      = 0x0080;
const uint LBS_NOINTEGRALHEIGHT = 0x0100;
const uint LBS_MULTICOLUMN      = 0x0200;
const uint LBS_WANTKEYBOARDINPUT= 0x0400;
const uint LBS_EXTENDEDSEL      = 0x0800;
const uint LBS_DISABLENOSCROLL  = 0x1000;
const uint LBS_NODATA           = 0x2000;
const uint LBS_NOSEL            = 0x4000;
const uint LBS_STANDARD         = (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER);

const int CB_OKAY            = 0;
const int CB_ERR             = (-1);
const int CB_ERRSPACE        = (-2);
const int CBN_ERRSPACE       = (-1);
const int CBN_SELCHANGE      = 1;
const int CBN_DBLCLK         = 2;
const int CBN_SETFOCUS       = 3;
const int CBN_KILLFOCUS      = 4;
const int CBN_EDITCHANGE     = 5;
const int CBN_EDITUPDATE     = 6;
const int CBN_DROPDOWN       = 7;
const int CBN_CLOSEUP        = 8;
const int CBN_SELENDOK       = 9;
const int CBN_SELENDCANCEL   = 10;

const uint CBS_SIMPLE           = 0x0001;
const uint CBS_DROPDOWN         = 0x0002;
const uint CBS_DROPDOWNLIST     = 0x0003;
const uint CBS_OWNERDRAWFIXED   = 0x0010;
const uint CBS_OWNERDRAWVARIABLE= 0x0020;
const uint CBS_AUTOHSCROLL      = 0x0040;
const uint CBS_OEMCONVERT       = 0x0080;
const uint CBS_SORT             = 0x0100;
const uint CBS_HASSTRINGS       = 0x0200;
const uint CBS_NOINTEGRALHEIGHT = 0x0400;
const uint CBS_DISABLENOSCROLL  = 0x0800;
const uint CBS_UPPERCASE        = 0x2000;
const uint CBS_LOWERCASE        = 0x4000;

const uint CB_GETEDITSEL             =  0x0140;
const uint CB_LIMITTEXT              =  0x0141;
const uint CB_SETEDITSEL             =  0x0142;
const uint CB_ADDSTRING              =  0x0143;
const uint CB_DELETESTRING           =  0x0144;
const uint CB_DIR                    =  0x0145;
const uint CB_GETCOUNT               =  0x0146;
const uint CB_GETCURSEL              =  0x0147;
const uint CB_GETLBTEXT              =  0x0148;
const uint CB_GETLBTEXTLEN           =  0x0149;
const uint CB_INSERTSTRING           =  0x014A;
const uint CB_RESETCONTENT           =  0x014B;
const uint CB_FINDSTRING             =  0x014C;
const uint CB_SELECTSTRING           =  0x014D;
const uint CB_SETCURSEL              =  0x014E;
const uint CB_SHOWDROPDOWN           =  0x014F;
const uint CB_GETITEMDATA            =  0x0150;
const uint CB_SETITEMDATA            =  0x0151;
const uint CB_GETDROPPEDCONTROLRECT  =  0x0152;
const uint CB_SETITEMHEIGHT          =  0x0153;
const uint CB_GETITEMHEIGHT          =  0x0154;
const uint CB_SETEXTENDEDUI          =  0x0155;
const uint CB_GETEXTENDEDUI          =  0x0156;
const uint CB_GETDROPPEDSTATE        =  0x0157;
const uint CB_FINDSTRINGEXACT        =  0x0158;
const uint CB_SETLOCALE              =  0x0159;
const uint CB_GETLOCALE              =  0x015A;
const uint CB_GETTOPINDEX            =  0x015b;
const uint CB_SETTOPINDEX            =  0x015c;
const uint CB_GETHORIZONTALEXTENT    =  0x015d;
const uint CB_SETHORIZONTALEXTENT    =  0x015e;
const uint CB_GETDROPPEDWIDTH        =  0x015f;
const uint CB_SETDROPPEDWIDTH        =  0x0160;
const uint CB_INITSTORAGE            =  0x0161;


const DWORD PROV_RSA_FULL       = 1;
const DWORD CRYPT_VERIFYCONTEXT = 0xF0000000;
const DWORD CRYPT_SILENT        = 64;  // don't ever display a UI to the user

[extern "advapi32.dll"]
BOOL CryptAcquireContextW (
  HCRYPTPROV *phProv,
  LPCTSTR pszContainer,
  LPCTSTR pszProvider,
  DWORD dwProvType,
  DWORD dwFlags);

[extern "advapi32.dll"]
BOOL CryptGenRandom (
  HCRYPTPROV hProv,
  DWORD dwLen,
  BYTE *pbBuffer);

[extern "advapi32.dll"]
BOOL CryptReleaseContext (HCRYPTPROV hProv, DWORD dwFlags);

[extern "advapi32.dll"]
BOOLEAN SystemFunction036 (PVOID RandomBuffer, ULONG RandomBufferLength); // RtlGenRandom

//------------------------------------------------------------------------------

// IID_IDataObject    "0000010e-0000-0000-C000-000000000046"
// IID_IEnumFORMATETC "00000103-0000-0000-C000-000000000046"
// IID_IDropSource    "00000121-0000-0000-C000-000000000046"

const REFIID IID_IDropTarget = {0x00000122, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

typedef WORD CLIPFORMAT;

const CLIPFORMAT CF_TEXT = 1;
const CLIPFORMAT CF_BITMAP = 2;
const CLIPFORMAT CF_METAFILEPICT = 3;
const CLIPFORMAT CF_SYLK = 4;
const CLIPFORMAT CF_DIF = 5;
const CLIPFORMAT CF_TIFF = 6;
const CLIPFORMAT CF_OEMTEXT = 7;
const CLIPFORMAT CF_DIB = 8;
const CLIPFORMAT CF_PALETTE = 9;
const CLIPFORMAT CF_PENDATA = 10;
const CLIPFORMAT CF_RIFF = 11;
const CLIPFORMAT CF_WAVE = 12;
const CLIPFORMAT CF_UNICODETEXT = 13;
const CLIPFORMAT CF_ENHMETAFILE = 14;
const CLIPFORMAT CF_HDROP = 15;
const CLIPFORMAT CF_LOCALE = 16;
const CLIPFORMAT CF_MAX = 17;
const CLIPFORMAT CF_OWNERDISPLAY = 0x80;
const CLIPFORMAT CF_DSPTEXT = 0x81;
const CLIPFORMAT CF_DSPBITMAP = 0x82;
const CLIPFORMAT CF_DSPMETAFILEPICT = 0x83;
const CLIPFORMAT CF_DSPENHMETAFILE = 0x8E;

struct DVTARGETDEVICE
{
  DWORD tdSize;
  WORD tdDriverNameOffset;
  WORD tdDeviceNameOffset;
  WORD tdPortNameOffset;
  WORD tdExtDevmodeOffset;
  BYTE tdData[1];
}

const DWORD DVASPECT_CONTENT = 0;
const DWORD DVASPECT_THUMBNAIL = 1;
const DWORD DVASPECT_ICON = 2;
const DWORD DVASPECT_DOCPRINT = 3;

const DWORD TYMED_NULL	= 0;
const DWORD TYMED_HGLOBAL	= 1;
const DWORD TYMED_FILE	= 2;
const DWORD TYMED_ISTREAM	= 4;
const DWORD TYMED_ISTORAGE	= 8;
const DWORD TYMED_GDI	= 16;
const DWORD TYMED_MFPICT	= 32;
const DWORD TYMED_ENHMF	= 64;

struct FORMATETC
{
  CLIPFORMAT cfFormat;
  DVTARGETDEVICE *ptd;
  DWORD dwAspect;
  LONG lindex;
  DWORD tymed;
}

// dummy declarations
typedef LPVOID IStream;
typedef LPVOID IStorage;

union STGMEDIUM_UNION
{
  HBITMAP hBitmap;
  HMETAFILEPICT hMetaFilePict;
  HENHMETAFILE hEnhMetaFile;
  HGLOBAL hGlobal;
  LPOLESTR lpszFileName;
  IStream *pstm;
  IStorage *pstg;
}

struct STGMEDIUM
{
  DWORD tymed;
  STGMEDIUM_UNION u;
  IUnknown *pUnkForRelease;
}

const DWORD DATADIR_GET	= 1;
const DWORD DATADIR_SET	= 2;

typedef IEnumFORMATETC;

typedef PVOID IAdviseSink;  // temporary
typedef PVOID IEnumSTATDATA; // temporary

typedef [callback] HRESULT FGetData      (LPVOID * This, FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
typedef [callback] HRESULT FGetDataHere  (LPVOID * This, FORMATETC *pformatetc, STGMEDIUM *pmedium);
typedef [callback] HRESULT FQueryGetData (LPVOID * This, FORMATETC *pformatetc);
typedef [callback] HRESULT FGetCanonicalFormatEtc (LPVOID * This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
typedef [callback] HRESULT FSetData      (LPVOID * This, FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
typedef [callback] HRESULT FEnumFormatEtc(LPVOID * This, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
typedef [callback] HRESULT FDAdvise      (LPVOID * This, FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
typedef [callback] HRESULT FDUnadvise    (LPVOID * This, DWORD dwConnection);
typedef [callback] HRESULT FEnumDAdvise  (LPVOID * This, IEnumSTATDATA **ppenumAdvise);

struct IDataObjectVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetData               GetData;
  FGetDataHere           GetDataHere;
  FQueryGetData          QueryGetData;
  FGetCanonicalFormatEtc GetCanonicalFormatEtc;
  FSetData               SetData;
  FEnumFormatEtc         EnumFormatEtc;
  FDAdvise               DAdvise;
  FDUnadvise             DUnadvise;
  FEnumDAdvise           EnumDAdvise;
}

struct IDataObject
{
  IDataObjectVtbl *lpVtbl;
}


typedef [callback] HRESULT FNext (LPVOID * This, ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
typedef [callback] HRESULT FSkip (LPVOID * This, ULONG celt);
typedef [callback] HRESULT FReset (LPVOID * This);
typedef [callback] HRESULT FClone (LPVOID * This, IEnumFORMATETC **ppenum);

struct IEnumFORMATETCVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FNext Next;
  FSkip Skip;
  FReset Reset;
  FClone Clone;
}

struct IEnumFORMATETC
{
  IEnumFORMATETCVtbl *lpVtbl;
}


typedef [callback] HRESULT FQueryContinueDrag (LPVOID * This, BOOL fEscapePressed, DWORD grfKeyState);
typedef [callback] HRESULT FGiveFeedback      (LPVOID * This, DWORD dwEffect);

struct IDropSourceVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FQueryContinueDrag     QueryContinueDrag;
  FGiveFeedback          GiveFeedback;
}

struct IDropSource
{
  IDropSourceVtbl *lpVtbl;
}

const DWORD	DROPEFFECT_NONE	= 0;
const DWORD DROPEFFECT_COPY	= 1;
const DWORD	DROPEFFECT_MOVE	= 2;
const DWORD	DROPEFFECT_LINK	= 4;
const DWORD	DROPEFFECT_SCROLL	= 0x80000000;

typedef [callback] HRESULT FDragEnter (LPVOID * This, IDataObject *pDataObj, DWORD grfKeyState, long xy, DWORD *pdwEffect);
typedef [callback] HRESULT FDragOver  (LPVOID * This, DWORD grfKeyState, long xy, DWORD *pdwEffect);
typedef [callback] HRESULT FDragLeave (LPVOID * This);
typedef [callback] HRESULT FDrop      (LPVOID * This, IDataObject *pDataObj, DWORD grfKeyState, long xy, DWORD *pdwEffect);

struct IDropTargetVtbl
{
  FQueryInterface  QueryInterface;
  FAddRef          AddRef;
  FRelease         Release;
  FDragEnter       DragEnter;
  FDragOver        DragOver;
  FDragLeave       DragLeave;
  FDrop            Drop;
}

struct IDropTarget
{
  IDropTargetVtbl *lpVtbl;
}

//--------------------------------------------------------------------------

[extern "Shell32.dll"]
UINT DragQueryFileA (byte* hDrop, UINT iFile, LPSTR lpszFile, UINT cch);

[extern "Shell32.dll"]
UINT DragQueryFileW (byte* hDrop, UINT iFile, LPWSTR lpszFile, UINT cch);

[extern "Ole32.dll"]
HRESULT OleInitialize (LPVOID pvReserved);

[extern "Ole32.dll"]
void OleUninitialize();

[extern "Ole32.dll"]
HRESULT CoLockObjectExternal (LPUNKNOWN pUnk, BOOL fLock, BOOL fLastUnlockReleases);

[extern "Ole32.dll"]
HRESULT RegisterDragDrop (HWND hwnd, IDropTarget* pDropTarget);

[extern "Ole32.dll"]
HRESULT RevokeDragDrop (HWND hwnd);

[extern "Ole32.dll"]
void ReleaseStgMedium (STGMEDIUM* pmedium);

//--------------------------------------------------------------------------

typedef [callback] BOOL WNDENUMPROC (HWND hwnd, LPARAM lParam);

[extern "user32.dll"]
BOOL EnumWindows (WNDENUMPROC lpEnumFunc, LPARAM lParam);

[extern "user32.dll"]
BOOL EnumChildWindows (HWND hWndParent, WNDENUMPROC lpEnumFunc, LPARAM lParam);

[extern "gdi32.dll"]
BOOL SetViewportOrgEx (HDC hdc, int X, int Y, POINT* lpPoint);

//------------------------------------------------------------------------------

struct OPENFILENAMEA
{
  DWORD        lStructSize;
  HWND         hwndOwner;
  HINSTANCE    hInstance;
  LPCSTR       lpstrFilter;
  LPSTR        lpstrCustomFilter;
  DWORD        nMaxCustFilter;
  DWORD        nFilterIndex;
  LPSTR        lpstrFile;
  DWORD        nMaxFile;
  LPSTR        lpstrFileTitle;
  DWORD        nMaxFileTitle;
  LPCSTR       lpstrInitialDir;
  LPCSTR       lpstrTitle;
  DWORD        Flags;
  WORD         nFileOffset;
  WORD         nFileExtension;
  LPCSTR       lpstrDefExt;
  LPARAM       lCustData;
  byte*        lpfnHook;
  LPCSTR       lpTemplateName;
  byte*        pvReserved;
  DWORD        dwReserved;
  DWORD        FlagsEx;
}

struct OPENFILENAMEW
{
  DWORD        lStructSize;
  HWND         hwndOwner;
  HINSTANCE    hInstance;
  LPCWSTR      lpstrFilter;
  LPWSTR       lpstrCustomFilter;
  DWORD        nMaxCustFilter;
  DWORD        nFilterIndex;
  LPWSTR       lpstrFile;
  DWORD        nMaxFile;
  LPWSTR       lpstrFileTitle;
  DWORD        nMaxFileTitle;
  LPCWSTR      lpstrInitialDir;
  LPCWSTR      lpstrTitle;
  DWORD        Flags;
  WORD         nFileOffset;
  WORD         nFileExtension;
  LPCWSTR      lpstrDefExt;
  LPARAM       lCustData;
  byte*        lpfnHook;
  LPCWSTR      lpTemplateName;
  byte*        pvReserved;
  DWORD        dwReserved;
  DWORD        FlagsEx;
}

[extern "Comdlg32.dll"]
BOOL GetOpenFileNameA (OPENFILENAMEA* lpofn);

[extern "Comdlg32.dll"]
BOOL GetOpenFileNameW (OPENFILENAMEW* lpofn);

[extern "Comdlg32.dll"]
BOOL GetSaveFileNameA (OPENFILENAMEA* lpofn);

[extern "Comdlg32.dll"]
BOOL GetSaveFileNameW (OPENFILENAMEW* lpofn);

const DWORD OFN_READONLY         = 1;
const DWORD OFN_OVERWRITEPROMPT  = 2;
const DWORD OFN_HIDEREADONLY     = 4;
const DWORD OFN_PATHMUSTEXIST    = 0x0800;
const DWORD OFN_SHAREAWARE       = 0x4000;
const DWORD OFN_FILEMUSTEXIST    = 0x1000;
const DWORD OFN_NOTESTFILECREATE = 0x10000;

[extern "Comdlg32.dll"]
DWORD CommDlgExtendedError();

//------------------------------------------------------------------------------

typedef uint2    VARTYPE;
typedef WORD     PROPVAR_PAD1;
typedef WORD     PROPVAR_PAD2;
typedef WORD     PROPVAR_PAD3;

struct PROPVARIANT
{
  VARTYPE      vt;
  PROPVAR_PAD1 wReserved1;
  PROPVAR_PAD2 wReserved2;
  PROPVAR_PAD3 wReserved3;
  byte[8]      Val;
#if MEM64
  byte[8]      extra;
#endif
}

[extern "Ole32.dll"]
HRESULT PropVariantClear (PROPVARIANT* pvar);

//------------------------------------------------------------------------------

const REFCLSID CLSID_ShellLink = {0x00021401,0,0,0xC0,0,0,0,0,0,0,0x46};
const REFIID IID_IShellLinkA = {0x000214EE, 0, 0, 0xC0,0,0,0,0,0,0,0x46};
const REFIID IID_IPersistFile = {0x0000010b,0,0,0xC0,0,0,0,0,0,0,0x46};

//---------------------------------------------------------------------------------------------------------

typedef IShellLink;

typedef [callback] HRESULT FGetPath (IShellLink *This, LPSTR pszFile, int cchMaxPath, byte*pfd, DWORD fFlags);
typedef [callback] HRESULT FSetDescription (IShellLink *This, LPCSTR pszName);
typedef [callback] HRESULT FSetWorkingDirectory (IShellLink *This, LPCSTR pszDir);
typedef [callback] HRESULT FSetPath (IShellLink *This, LPCSTR pszFile);

struct IShellLinkAVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetPath               GetPath;
  FQueryInterface   f1;  //  STDMETHOD(GetIDList)(THIS_ LPITEMIDLIST * ppidl) PURE;
  FQueryInterface   f2;  // STDMETHOD(SetIDList)(THIS_ LPCITEMIDLIST pidl) PURE;
  FQueryInterface   f3;  // STDMETHOD(GetDescription)(THIS_ LPSTR pszName, int cchMaxName) PURE;
  FSetDescription  SetDescription;
  FQueryInterface   f4;  //  STDMETHOD(GetWorkingDirectory)(THIS_ LPSTR pszDir, int cchMaxPath) PURE;
  FSetWorkingDirectory SetWorkingDirectory;
  FQueryInterface   f5;  //  STDMETHOD(GetArguments)(THIS_ LPSTR pszArgs, int cchMaxPath) PURE;
  FQueryInterface   f6;  //  STDMETHOD(SetArguments)(THIS_ LPCSTR pszArgs) PURE;
  FQueryInterface   f7;  //  STDMETHOD(GetHotkey)(THIS_ WORD *pwHotkey) PURE;
  FQueryInterface   f8;  //  STDMETHOD(SetHotkey)(THIS_ WORD wHotkey) PURE;
  FQueryInterface   f9;  //  STDMETHOD(GetShowCmd)(THIS_ int *piShowCmd) PURE;
  FQueryInterface   f10; //  STDMETHOD(SetShowCmd)(THIS_ int iShowCmd) PURE;
  FQueryInterface   f11; //  STDMETHOD(GetIconLocation)(THIS_ LPSTR pszIconPath, int cchIconPath, int *piIcon) PURE;
  FQueryInterface   f12; //  STDMETHOD(SetIconLocation)(THIS_ LPCSTR pszIconPath, int iIcon) PURE;
  FQueryInterface   f13; //  STDMETHOD(SetRelativePath)(THIS_ LPCSTR pszPathRel, DWORD dwReserved) PURE;
  FQueryInterface   f14; //  STDMETHOD(Resolve)(THIS_ HWND hwnd, DWORD fFlags) PURE;
  FSetPath  SetPath;
}

struct IShellLink
{
  IShellLinkAVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef IPersistFile;
typedef [callback] HRESULT FSave (IPersistFile *This, /* [unique][in] */ LPCOLESTR pszFileName,    /* [in] */ BOOL fRemember);

struct IPersistFileVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FQueryInterface   f1; //  HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetClassID )(  IPersistFile __RPC_FAR * This,      /* [out] */ CLSID __RPC_FAR *pClassID);
  FQueryInterface   f2; //  HRESULT ( STDMETHODCALLTYPE __RPC_FAR *IsDirty )(  IPersistFile __RPC_FAR * This);
  FQueryInterface   f3; //  HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Load )(  IPersistFile __RPC_FAR * This,  /* [in] */ LPCOLESTR pszFileName, /* [in] */ DWORD dwMode);
  FSave             Save;
  FQueryInterface   f4; //  HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SaveCompleted )( IPersistFile __RPC_FAR * This, /* [unique][in] */ LPCOLESTR pszFileName);
  FQueryInterface   f5; //  HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetCurFile )( IPersistFile __RPC_FAR * This,  /* [out] */ LPOLESTR __RPC_FAR *ppszFileName);
}

struct IPersistFile
{
  IPersistFileVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

const DWORD RIDEV_REMOVE       = 0x00000001;  // de-register
const DWORD RIDEV_NOLEGACY     = 0x00000030;  // do not send old messages to windows
const DWORD RIDEV_INPUTSINK    = 0x00000100;  // receive messages even when the window is not in the foreground.
const DWORD RIDEV_CAPTUREMOUSE = 0x00000200;  // does not activate other windows

packed struct RAWINPUTHEADER
{
  DWORD  dwType;
  DWORD  dwSize;
  HANDLE hDevice;
  WPARAM wParam;
}

const DWORD RIM_TYPEMOUSE = 0;

const USHORT HID_USAGE_PAGE_GENERIC   = 0x01;
const USHORT HID_USAGE_GENERIC_MOUSE  = 0x02;

packed struct RAWINPUTDEVICE
{
  USHORT usUsagePage;
  USHORT usUsage;
  DWORD  dwFlags;
  HWND   hwndTarget;
}

[extern "user32.dll"]
BOOL RegisterRawInputDevices
 (RAWINPUTDEVICE*  pRawInputDevices,
  UINT             uiNumDevices,
  UINT             cbSize);

const UINT RID_INPUT = 0x10000003;
typedef HANDLE HRAWINPUT;

packed struct RAWMOUSEus   // 4 bytes
{
  USHORT usButtonFlags;
  USHORT usButtonData;
}

union RAWMOUSEu   // 4 bytes
{
  ULONG      ulButtons;
  RAWMOUSEus s;
}

packed struct RAWMOUSE
{
  USHORT    usFlags;       // 2 bytes
  uint2     filler;
  RAWMOUSEu u;             // 4 bytes
  ULONG     ulRawButtons;  // 4 bytes
  LONG      lLastX;
  LONG      lLastY;
  ULONG     ulExtraInformation;
}

union RAWINPUTdata
{
  RAWMOUSE    mouse;
//  RAWKEYBOARD keyboard;
//  RAWHID      hid;
}

packed struct RAWINPUT
{
  RAWINPUTHEADER header;
  RAWINPUTdata   data;
}

[extern "user32.dll"]
UINT GetRawInputData
 (HRAWINPUT hRawInput,
  UINT      uiCommand,
  LPVOID    pData,
  UINT*     pcbSize,
  UINT      cbSizeHeader);

const UINT WM_INPUT = 0x00FF;

//------------------------------------------------------------------------------

[extern "user32.dll"]
SHORT GetAsyncKeyState (int vKey);

//------------------------------------------------------------------------------

[extern "kernel32.dll"]
DWORD TlsAlloc ();          // returns TLS slot, -1 if error

[extern "kernel32.dll"]
BOOL TlsFree (DWORD tlsIndex);   // 0 means failure, !=0 means OK

[extern "kernel32.dll"]
LPVOID TlsGetValue (DWORD tlsIndex);

[extern "kernel32.dll"]
BOOL TlsSetValue (DWORD tlsIndex, LPVOID value);   // 0 means failure, !=0 means OK

//------------------------------------------------------------------------------

const DWORD DBT_DEVTYP_VOLUME = 0x00000002;
const WORD  DBTF_MEDIA = 0x0001;

const WPARAM DBT_DEVICEARRIVAL = 0x8000;
const WPARAM DBT_DEVICEREMOVECOMPLETE = 0x8004;

struct DEV_BROADCAST_HDR
{
  DWORD dbch_size;
  DWORD dbch_devicetype;
  DWORD dbch_reserved;
}

struct DEV_BROADCAST_VOLUME
{
  DWORD dbcv_size;
  DWORD dbcv_devicetype;
  DWORD dbcv_reserved;
  DWORD dbcv_unitmask;
  WORD  dbcv_flags;
}

//------------------------------------------------------------------------------

HWND main_hWnd;   // handle of main window

// returns last windows error, always a negative value.
int negative_last_windows_error();

//------------------------------------------------------------------------------
#end unsafe
//------------------------------------------------------------------------------

#else
  !error // this file must not be included
#endif
