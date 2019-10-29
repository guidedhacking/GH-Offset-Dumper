#pragma once
//Sources:
//https://github.com/x64dbg/x64dbg/blob/development/src/dbg/ntdll/ntdll.h
//this will conflict with winternl.h, need to modify everything to avoid conflict
//for each struct you import, prefix with RFW_, if it starts with _, make it _RFW_
//delete the redefinition that follows } and prefix the *P with RFW_

#define GDI_HANDLE_BUFFER_SIZE32	34
#define GDI_HANDLE_BUFFER_SIZE64	60

#ifndef _WIN64
#define GDI_HANDLE_BUFFER_SIZE GDI_HANDLE_BUFFER_SIZE32
#else
#define GDI_HANDLE_BUFFER_SIZE GDI_HANDLE_BUFFER_SIZE64
#endif

typedef ULONG GDI_HANDLE_BUFFER32[GDI_HANDLE_BUFFER_SIZE32];
typedef ULONG GDI_HANDLE_BUFFER64[GDI_HANDLE_BUFFER_SIZE64];
typedef ULONG GDI_HANDLE_BUFFER[GDI_HANDLE_BUFFER_SIZE];
typedef LONG KPRIORITY, * PKPRIORITY;
#define RTL_MAX_DRIVE_LETTERS 32
#define RTL_DRIVE_LETTER_VALID (USHORT)0x0001

typedef struct _RFW_UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} *RFW_P_RFW_UNICODE_STRING;

typedef const _RFW_UNICODE_STRING* RFW_PC_RFW_UNICODE_STRING;

typedef struct _RFW_CURDIR
{
	_RFW_UNICODE_STRING DosPath;
	HANDLE Handle;
} *RFW_PCURDIR;

typedef struct _RFW_RTL_DRIVE_LETTER_CURDIR
{
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	_RFW_UNICODE_STRING DosPath;
} *RFW_PRTL_DRIVE_LETTER_CURDIR;

typedef struct _RFW_RTL_USER_PROCESS_PARAMETERS
{
	ULONG MaximumLength;
	ULONG Length;

	ULONG Flags;
	ULONG DebugFlags;

	HANDLE ConsoleHandle;
	ULONG ConsoleFlags;
	HANDLE StandardInput;
	HANDLE StandardOutput;
	HANDLE StandardError;

	_RFW_CURDIR CurrentDirectory;
	_RFW_UNICODE_STRING DllPath;
	_RFW_UNICODE_STRING ImagePathName;
	_RFW_UNICODE_STRING CommandLine;
	PWCHAR Environment;

	ULONG StartingX;
	ULONG StartingY;
	ULONG CountX;
	ULONG CountY;
	ULONG CountCharsX;
	ULONG CountCharsY;
	ULONG FillAttribute;

	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	_RFW_UNICODE_STRING WindowTitle;
	_RFW_UNICODE_STRING DesktopInfo;
	_RFW_UNICODE_STRING ShellInfo;
	_RFW_UNICODE_STRING RuntimeData;
	_RFW_RTL_DRIVE_LETTER_CURDIR CurrentDirectories[RTL_MAX_DRIVE_LETTERS];

	ULONG_PTR EnvironmentSize;
	ULONG_PTR EnvironmentVersion;
	PVOID PackageDependencyData;
	ULONG ProcessGroupId;
	ULONG LoaderThreads;
} *RFW_PRTL_USER_PROCESS_PARAMETERS;

//typedef struct _RFW_PEB_LDR_DATA
typedef struct _RFW_PEB_LDR_DATA
{
	ULONG Length;
	BOOLEAN Initialized;
	HANDLE SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID EntryInProgress;
	BOOLEAN ShutdownInProgress;
	HANDLE ShutdownThreadId;
} *RFW_PPEB_LDR_DATA;

typedef struct _RFW_PEB
{
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	union
	{
		BOOLEAN BitField;
		struct
		{
			BOOLEAN ImageUsesLargePages : 1;
			BOOLEAN IsProtectedProcess : 1;
			BOOLEAN IsImageDynamicallyRelocated : 1;
			BOOLEAN SkipPatchingUser32Forwarders : 1;
			BOOLEAN IsPackagedProcess : 1;
			BOOLEAN IsAppContainer : 1;
			BOOLEAN IsProtectedProcessLight : 1;
			BOOLEAN IsLongPathAwareProcess : 1;
		} s1;
	} u1;

	HANDLE Mutant;

	PVOID ImageBaseAddress;
	RFW_PPEB_LDR_DATA Ldr;
	RFW_PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PRTL_CRITICAL_SECTION FastPebLock;
	PVOID AtlThunkSListPtr;
	PVOID IFEOKey;
	union
	{
		ULONG CrossProcessFlags;
		struct
		{
			ULONG ProcessInJob : 1;
			ULONG ProcessInitializing : 1;
			ULONG ProcessUsingVEH : 1;
			ULONG ProcessUsingVCH : 1;
			ULONG ProcessUsingFTH : 1;
			ULONG ProcessPreviouslyThrottled : 1;
			ULONG ProcessCurrentlyThrottled : 1;
			ULONG ReservedBits0 : 25;
		} s2;
	} u2;
	union
	{
		PVOID KernelCallbackTable;
		PVOID UserSharedInfoPtr;
	} u3;
	ULONG SystemReserved[1];
	ULONG AtlThunkSListPtr32;
	PVOID ApiSetMap;
	ULONG TlsExpansionCounter;
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];

	PVOID ReadOnlySharedMemoryBase;
	PVOID SharedData; // HotpatchInformation
	PVOID* ReadOnlyStaticServerData;

	PVOID AnsiCodePageData; // PCPTABLEINFO
	PVOID OemCodePageData; // PCPTABLEINFO
	PVOID UnicodeCaseTableData; // PNLSTABLEINFO

	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;

	LARGE_INTEGER CriticalSectionTimeout;
	SIZE_T HeapSegmentReserve;
	SIZE_T HeapSegmentCommit;
	SIZE_T HeapDeCommitTotalFreeThreshold;
	SIZE_T HeapDeCommitFreeBlockThreshold;

	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PVOID* ProcessHeaps; // PHEAP

	PVOID GdiSharedHandleTable;
	PVOID ProcessStarterHelper;
	ULONG GdiDCAttributeList;

	PRTL_CRITICAL_SECTION LoaderLock;

	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	USHORT OSBuildNumber;
	USHORT OSCSDVersion;
	ULONG OSPlatformId;
	ULONG ImageSubsystem;
	ULONG ImageSubsystemMajorVersion;
	ULONG ImageSubsystemMinorVersion;
	ULONG_PTR ActiveProcessAffinityMask;
	GDI_HANDLE_BUFFER GdiHandleBuffer;
	PVOID PostProcessInitRoutine;

	PVOID TlsExpansionBitmap;
	ULONG TlsExpansionBitmapBits[32];

	ULONG SessionId;

	ULARGE_INTEGER AppCompatFlags;
	ULARGE_INTEGER AppCompatFlagsUser;
	PVOID pShimData;
	PVOID AppCompatInfo; // APPCOMPAT_EXE_DATA

	_RFW_UNICODE_STRING CSDVersion;

	PVOID ActivationContextData; // ACTIVATION_CONTEXT_DATA
	PVOID ProcessAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP
	PVOID SystemDefaultActivationContextData; // ACTIVATION_CONTEXT_DATA
	PVOID SystemAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP

	SIZE_T MinimumStackCommit;

	PVOID* FlsCallback;
	LIST_ENTRY FlsListHead;
	PVOID FlsBitmap;
	ULONG FlsBitmapBits[FLS_MAXIMUM_AVAILABLE / (sizeof(ULONG) * 8)];
	ULONG FlsHighIndex;

	PVOID WerRegistrationData;
	PVOID WerShipAssertPtr;
	PVOID pUnused; // pContextData
	PVOID pImageHeaderHash;
	union
	{
		ULONG TracingFlags;
		struct
		{
			ULONG HeapTracingEnabled : 1;
			ULONG CritSecTracingEnabled : 1;
			ULONG LibLoaderTracingEnabled : 1;
			ULONG SpareTracingBits : 29;
		} s3;
	} u4;
	ULONGLONG CsrServerReadOnlySharedMemoryBase;
	PVOID TppWorkerpListLock;
	LIST_ENTRY TppWorkerpList;
	PVOID WaitOnAddressHashTable[128];
	PVOID TelemetryCoverageHeader; // REDSTONE3
	ULONG CloudFileFlags;
}*RFW_PPEB;

#define RTL_BALANCED_NODE_RESERVED_PARENT_MASK 3

enum _RFW_LDR_DDAG_STATE
{
	LdrModulesMerged = -5,
	LdrModulesInitError = -4,
	LdrModulesSnapError = -3,
	LdrModulesUnloaded = -2,
	LdrModulesUnloading = -1,
	LdrModulesPlaceHolder = 0,
	LdrModulesMapping = 1,
	LdrModulesMapped = 2,
	LdrModulesWaitingForDependencies = 3,
	LdrModulesSnapping = 4,
	LdrModulesSnapped = 5,
	LdrModulesCondensed = 6,
	LdrModulesReadyToInit = 7,
	LdrModulesInitializing = 8,
	LdrModulesReadyToRun = 9
};

typedef struct _RFW_RTL_BALANCED_NODE
{
	union
	{
		struct _RFW_RTL_BALANCED_NODE* Children[2];
		struct
		{
			struct _RFW_RTL_BALANCED_NODE* Left;
			struct _RFW_RTL_BALANCED_NODE* Right;
		} s;
	};
	union
	{
		UCHAR Red : 1;
		UCHAR Balance : 2;
		ULONG_PTR ParentValue;
	} u;
} *RFW_PRTL_BALANCED_NODE;

typedef struct _RFW_LDR_SERVICE_TAG_RECORD
{
	struct _RFW_LDR_SERVICE_TAG_RECORD* Next;
	ULONG ServiceTag;
} *RFW_PLDR_SERVICE_TAG_RECORD;

enum _LDR_DLL_LOAD_REASON
{
	LoadReasonStaticDependency,
	LoadReasonStaticForwarderDependency,
	LoadReasonDynamicForwarderDependency,
	LoadReasonDelayloadDependency,
	LoadReasonDynamicLoad,
	LoadReasonAsImageLoad,
	LoadReasonAsDataLoad,
	LoadReasonUnknown = -1
}; //*RFW_PLDR_DLL_LOAD_REASON;

typedef struct _RFW_LDRP_CSLIST
{
	PSINGLE_LIST_ENTRY Tail;
} *RFW_PLDRP_CSLIST;

typedef struct _RFW_LDR_DDAG_NODE
{
	LIST_ENTRY Modules;
	RFW_PLDR_SERVICE_TAG_RECORD ServiceTagList;
	ULONG LoadCount;
	ULONG LoadWhileUnloadingCount;
	ULONG LowestLink;
	union
	{
		_RFW_LDRP_CSLIST Dependencies;
		SINGLE_LIST_ENTRY RemovalLink;
	};
	_RFW_LDRP_CSLIST IncomingDependencies;
	_RFW_LDR_DDAG_STATE State;
	SINGLE_LIST_ENTRY CondenseLink;
	ULONG PreorderNumber;
} *RFW_PLDR_DDAG_NODE;

enum class _RFW_LDR_DLL_LOAD_REASON
{
	LoadReasonStaticDependency,
	LoadReasonStaticForwarderDependency,
	LoadReasonDynamicForwarderDependency,
	LoadReasonDelayloadDependency,
	LoadReasonDynamicLoad,
	LoadReasonAsImageLoad,
	LoadReasonAsDataLoad,
	LoadReasonUnknown = -1
}; //*RFW_PLDR_DLL_LOAD_REASON;

typedef struct RFW_LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	union
	{
		LIST_ENTRY InInitializationOrderLinks;
		LIST_ENTRY InProgressLinks;
	};
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	_RFW_UNICODE_STRING FullDllName;
	_RFW_UNICODE_STRING BaseDllName;
	union
	{
		UCHAR FlagGroup[4];
		ULONG Flags;
		struct
		{
			ULONG PackagedBinary : 1;
			ULONG MarkedForRemoval : 1;
			ULONG ImageDll : 1;
			ULONG LoadNotificationsSent : 1;
			ULONG TelemetryEntryProcessed : 1;
			ULONG ProcessStaticImport : 1;
			ULONG InLegacyLists : 1;
			ULONG InIndexes : 1;
			ULONG ShimDll : 1;
			ULONG InExceptionTable : 1;
			ULONG ReservedFlags1 : 2;
			ULONG LoadInProgress : 1;
			ULONG LoadConfigProcessed : 1;
			ULONG EntryProcessed : 1;
			ULONG ProtectDelayLoad : 1;
			ULONG ReservedFlags3 : 2;
			ULONG DontCallForThreads : 1;
			ULONG ProcessAttachCalled : 1;
			ULONG ProcessAttachFailed : 1;
			ULONG CorDeferredValidate : 1;
			ULONG CorImage : 1;
			ULONG DontRelocate : 1;
			ULONG CorILOnly : 1;
			ULONG ReservedFlags5 : 3;
			ULONG Redirected : 1;
			ULONG ReservedFlags6 : 2;
			ULONG CompatDatabaseProcessed : 1;
		} s;
	} u;
	USHORT ObsoleteLoadCount;
	USHORT TlsIndex;
	LIST_ENTRY HashLinks;
	ULONG TimeDateStamp;
	struct _ACTIVATION_CONTEXT* EntryPointActivationContext;
	PVOID Lock;
	RFW_PLDR_DDAG_NODE DdagNode;
	LIST_ENTRY NodeModuleLink;
	struct _LDRP_LOAD_CONTEXT* LoadContext;
	PVOID ParentDllBase;
	PVOID SwitchBackContext;
	_RFW_RTL_BALANCED_NODE BaseAddressIndexNode;
	_RFW_RTL_BALANCED_NODE MappingInfoIndexNode;
	ULONG_PTR OriginalBase;
	LARGE_INTEGER LoadTime;
	ULONG BaseNameHashValue;
	_RFW_LDR_DLL_LOAD_REASON LoadReason;
	ULONG ImplicitPathOptions;
	ULONG ReferenceCount;
	ULONG DependentLoadFlags;
	UCHAR SigningLevel; // Since Windows 10 RS2
}*RFW_PLDR_DATA_TABLE_ENTRY;

union RFW_UNION_LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY list;
	RFW_LDR_DATA_TABLE_ENTRY dataEntry;
};

typedef struct RFW_PROCESS_BASIC_INFORMATION
{
	NTSTATUS ExitStatus;
	PPEB PebBaseAddress;
	ULONG_PTR AffinityMask;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
} *RFW_PPROCESS_BASIC_INFORMATION;
