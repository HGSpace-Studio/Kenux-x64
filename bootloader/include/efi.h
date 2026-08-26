#ifndef __EFI_H__
#define __EFI_H__

typedef unsigned int            UINT32;
typedef unsigned short          UINT16;
typedef unsigned char           UINT8;
typedef unsigned long long      UINT64;
typedef UINT64                  UINTN;
typedef signed char             INT8;
typedef signed short            INT16;
typedef signed int              INT32;
typedef signed long long        INT64;
typedef INT64                   INTN;
typedef UINTN                   EFI_HANDLE;
typedef INTN                    EFI_STATUS;
typedef UINT64                  EFI_PHYSICAL_ADDRESS;
typedef UINT64                  EFI_VIRTUAL_ADDRESS;
typedef void                    VOID;

#define EFI_SUCCESS             0
#define EFI_LOAD_ERROR          (INTN)(-1)
#define EFI_INVALID_PARAMETER   (INTN)(-2)
#define EFI_UNSUPPORTED         (INTN)(-3)
#define EFI_BAD_BUFFER_SIZE     (INTN)(-4)
#define EFI_BUFFER_TOO_SMALL    (INTN)(-5)
#define EFI_NOT_READY           (INTN)(-6)
#define EFI_DEVICE_ERROR        (INTN)(-7)
#define EFI_WRITE_PROTECTED     (INTN)(-8)
#define EFI_OUT_OF_RESOURCES    (INTN)(-9)
#define EFI_VOLUME_CORRUPTED    (INTN)(-10)
#define EFI_VOLUME_FULL         (INTN)(-11)
#define EFI_NO_MEDIA            (INTN)(-12)
#define EFI_MEDIA_CHANGED       (INTN)(-13)
#define EFI_NOT_FOUND           (INTN)(-14)
#define EFI_ACCESS_DENIED       (INTN)(-15)
#define EFI_NO_RESPONSE         (INTN)(-16)
#define EFI_NO_MAPPING          (INTN)(-17)
#define EFI_TIMEOUT             (INTN)(-18)
#define EFI_NOT_STARTED         (INTN)(-19)
#define EFI_ALREADY_STARTED     (INTN)(-20)
#define EFI_ABORTED             (INTN)(-21)
#define EFI_ICMP_ERROR          (INTN)(-22)
#define EFI_TFTP_ERROR          (INTN)(-23)
#define EFI_PROTOCOL_ERROR      (INTN)(-24)
#define EFI_INCOMPATIBLE_VERSION (INTN)(-25)
#define EFI_SECURITY_VIOLATION  (INTN)(-26)
#define EFI_CRC_ERROR           (INTN)(-27)
#define EFI_END_OF_MEDIA        (INTN)(-28)
#define EFI_END_OF_FILE         (INTN)(-31)

#define EFI_ERROR(a)            (((INTN)(a)) < 0)

typedef UINT16                  CHAR16;
typedef unsigned char           BOOLEAN;

#define TRUE                    1
#define FALSE                   0

#define EFIAPI                  __attribute__((ms_abi))

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

typedef UINT64 EFI_EVENT;

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct {
    UINT8 Type;
    UINT8 SubType;
    UINT16 Length;
} EFI_DEVICE_PATH_PROTOCOL;

typedef struct {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef enum {
    EfiLocateAllHandles,
    EfiLocateByRegisterNotify,
    EfiLocateByProtocol
} EFI_LOCATE_SEARCH_TYPE;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct EFI_FILE_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_LOADED_IMAGE_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_BOOT_SERVICES;
struct EFI_RUNTIME_SERVICES;
struct EFI_SYSTEM_TABLE;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFIAPI EFI_STATUS (*Reset)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFIAPI EFI_STATUS (*OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
    EFIAPI EFI_STATUS (*TestString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
    EFIAPI EFI_STATUS (*QueryMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber, UINTN *Columns, UINTN *Rows);
    EFIAPI EFI_STATUS (*SetMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber);
    EFIAPI EFI_STATUS (*SetAttribute)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute);
    EFIAPI EFI_STATUS (*ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    EFIAPI EFI_STATUS (*SetCursorPosition)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Column, UINTN Row);
    EFIAPI EFI_STATUS (*EnableCursor)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN Visible);
    UINTN Mode;
    UINTN Attribute;
    UINTN CursorColumn;
    UINTN CursorRow;
    BOOLEAN CursorVisible;
};

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFIAPI EFI_STATUS (*Reset)(struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFIAPI EFI_STATUS (*ReadKeyStroke)(struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);
    EFI_EVENT WaitForKey;
};

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    UINT64 CreateTime;
    UINT64 LastAccessTime;
    UINT64 ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[256];
} EFI_FILE_INFO;

struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFIAPI EFI_STATUS (*Open)(struct EFI_FILE_PROTOCOL *This, struct EFI_FILE_PROTOCOL **NewHandle, CHAR16 *FileName, UINT64 OpenMode, UINT64 Attributes);
    EFIAPI EFI_STATUS (*Close)(struct EFI_FILE_PROTOCOL *This);
    EFIAPI EFI_STATUS (*Delete)(struct EFI_FILE_PROTOCOL *This);
    EFIAPI EFI_STATUS (*Read)(struct EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer);
    EFIAPI EFI_STATUS (*Write)(struct EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer);
    EFIAPI EFI_STATUS (*GetPosition)(struct EFI_FILE_PROTOCOL *This, UINT64 *Position);
    EFIAPI EFI_STATUS (*SetPosition)(struct EFI_FILE_PROTOCOL *This, UINT64 Position);
    EFIAPI EFI_STATUS (*GetInfo)(struct EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType, UINTN *BufferSize, VOID *Buffer);
    EFIAPI EFI_STATUS (*SetInfo)(struct EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType, UINTN BufferSize, VOID *Buffer);
    EFIAPI EFI_STATUS (*Flush)(struct EFI_FILE_PROTOCOL *This);
};

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFIAPI EFI_STATUS (*OpenVolume)(struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, struct EFI_FILE_PROTOCOL **Root);
};

struct EFI_LOADED_IMAGE_PROTOCOL {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    struct EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    VOID *FilePath;
    UINT32 Reserved;
    UINTN LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINTN ImageSize;
    UINT32 ImageCodeType;
    UINT32 ImageDataType;
    EFIAPI EFI_STATUS (*Unload)(EFI_HANDLE ImageHandle);
};

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT8 PixelInformation[16];
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_MODE;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFIAPI EFI_STATUS (*QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber, UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFIAPI EFI_STATUS (*SetMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber);
    EFIAPI EFI_STATUS (*Blt)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, void *BltBuffer, UINTN BltOperation, UINTN SourceX, UINTN SourceY, UINTN DestinationX, UINTN DestinationY, UINTN Width, UINTN Height, UINTN Delta);
    EFI_GRAPHICS_OUTPUT_MODE *Mode;
};

#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL       0x00000002
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL      0x00000004
#define EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER 0x00000008
#define EFI_OPEN_PROTOCOL_BY_DRIVER          0x00000010
#define EFI_OPEN_PROTOCOL_EXCLUSIVE          0x00000020

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    EFIAPI EFI_STATUS (*RaiseTPL)(UINTN NewTpl);
    EFIAPI UINTN (*RestoreTPL)(UINTN OldTpl);
    EFIAPI EFI_STATUS (*AllocatePages)(EFI_ALLOCATE_TYPE Type, EFI_MEMORY_TYPE MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory);
    EFIAPI EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFIAPI EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize, EFI_MEMORY_DESCRIPTOR *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
    EFIAPI EFI_STATUS (*AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size, VOID **Buffer);
    EFIAPI EFI_STATUS (*FreePool)(VOID *Buffer);
    EFIAPI EFI_STATUS (*CreateEvent)(UINT32 Type, UINTN NotifyTpl, void (*NotifyFunction)(void *), void *NotifyContext, EFI_EVENT *Event);
    EFIAPI EFI_STATUS (*SetTimer)(EFI_EVENT Event, UINT64 Type, UINT64 TriggerTime);
    EFIAPI EFI_STATUS (*WaitForEvent)(UINTN NumberOfEvents, EFI_EVENT *Event, UINTN *Index);
    EFIAPI EFI_STATUS (*SignalEvent)(EFI_EVENT Event);
    EFIAPI EFI_STATUS (*CloseEvent)(EFI_EVENT Event);
    EFIAPI EFI_STATUS (*CheckEvent)(EFI_EVENT Event);
    EFIAPI EFI_STATUS (*InstallProtocolInterface)(EFI_HANDLE *Handle, EFI_GUID *Protocol, UINTN InterfaceType, void *Interface);
    EFIAPI EFI_STATUS (*ReinstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol, void *OldInterface, void *NewInterface);
    EFIAPI EFI_STATUS (*UninstallProtocolInterface)(EFI_HANDLE Handle, EFI_GUID *Protocol, void *Interface);
    EFIAPI EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface);
    VOID *Reserved;
    EFIAPI EFI_STATUS (*RegisterProtocolNotify)(EFI_GUID *Protocol, EFI_EVENT Event, void **Registration);
    EFIAPI EFI_STATUS (*LocateHandle)(EFI_LOCATE_SEARCH_TYPE SearchType, EFI_GUID *Protocol, void *SearchKey, UINTN *BufferSize, EFI_HANDLE *Buffer);
    EFIAPI EFI_STATUS (*LocateDevicePath)(EFI_GUID *Protocol, EFI_DEVICE_PATH_PROTOCOL **DevicePath, EFI_HANDLE *Device);
    EFIAPI EFI_STATUS (*InstallConfigurationTable)(EFI_GUID *Guid, void *Table);
    EFIAPI EFI_STATUS (*LoadImage)(BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle, EFI_DEVICE_PATH_PROTOCOL *DevicePath, VOID *SourceBuffer, UINTN SourceSize, EFI_HANDLE *ImageHandle);
    EFIAPI EFI_STATUS (*StartImage)(EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData);
    EFIAPI EFI_STATUS (*Exit)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus, UINTN ExitDataSize, CHAR16 *ExitData);
    EFIAPI EFI_STATUS (*UnloadImage)(EFI_HANDLE ImageHandle);
    EFIAPI EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    EFIAPI EFI_STATUS (*GetNextMonotonicCount)(UINT64 *Count);
    EFIAPI EFI_STATUS (*Stall)(UINTN Microseconds);
    EFIAPI EFI_STATUS (*SetWatchdogTimer)(UINTN Timeout, UINT64 WatchdogCode, UINTN DataSize, CHAR16 *WatchdogData);
    EFIAPI EFI_STATUS (*ConnectController)(EFI_HANDLE ControllerHandle, EFI_HANDLE *DriverImageHandle, EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath, UINTN Recursive);
    EFIAPI EFI_STATUS (*DisconnectController)(EFI_HANDLE ControllerHandle, EFI_HANDLE DriverImageHandle, EFI_HANDLE ChildHandle);
    EFIAPI EFI_STATUS (*OpenProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface, EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle, UINT32 Attributes);
    EFIAPI EFI_STATUS (*CloseProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle);
    EFIAPI EFI_STATUS (*OpenProtocolInformation)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **EntryBuffer, UINTN *EntryCount);
    EFIAPI EFI_STATUS (*ProtocolsPerHandle)(EFI_HANDLE Handle, EFI_GUID ***ProtocolBuffer, UINTN *ProtocolCount);
    EFIAPI EFI_STATUS (*LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE SearchType, EFI_GUID *Protocol, void *SearchKey, UINTN *NoHandles, EFI_HANDLE **Buffer);
    EFIAPI EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
    EFIAPI EFI_STATUS (*InstallMultipleProtocolInterfaces)(EFI_HANDLE *Handle, ...);
    EFIAPI EFI_STATUS (*UninstallMultipleProtocolInterfaces)(EFI_HANDLE Handle, ...);
    EFIAPI EFI_STATUS (*CalculateCrc32)(void *Data, UINTN DataSize, UINT32 *CrcOut);
    EFIAPI void (*CopyMem)(void *Destination, const void *Source, UINTN Length);
    EFIAPI void (*SetMem)(void *Buffer, UINTN Size, UINT8 Value);
    EFIAPI EFI_STATUS (*CreateEventEx)(UINT32 Type, UINTN NotifyTpl, void (*NotifyFunction)(void *), void *NotifyContext, EFI_GUID *EventGroup, EFI_EVENT *Event);
};

struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    struct EFI_RUNTIME_SERVICES *RuntimeServices;
    struct EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
};

struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;
};

#endif
