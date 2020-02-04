// Axel '0vercl0k' Souchet - January 25 2020
#include <ntifs.h>

//
// Bunch of useful resources I found useful you also might enjoy:
//   - ProcessHacker's pht project,
//   - Rekall's source-code,
//   - Windows Internals 7th edition
//
// Register the driver:
//   - sc create sic type=kernel binPath=c:\users\over\desktop\sic-drv.sys
//

//
// Declare a bunch of functions to satisfy the below pragmas.
//

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD SicDriverUnload;
DRIVER_DISPATCH_PAGED SicDispatchDeviceControl;

//
// Our code doesn't need to be allocated in non-paged memory.
// There is no functions running above APC_LEVEL as a result page faults
// are allowed.
// DriverEntry is in the INIT segment which gets discarded once the driver
// as been initialized.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SicDriverUnload)
#pragma alloc_text(PAGE, SicDispatchDeviceControl)
#endif

//
// Thanks to ProcessHacker's native library:
// https://github.com/processhacker/phnt
//

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation, // q: SYSTEM_BASIC_INFORMATION
    SystemProcessorInformation, // q: SYSTEM_PROCESSOR_INFORMATION
    SystemPerformanceInformation, // q: SYSTEM_PERFORMANCE_INFORMATION
    SystemTimeOfDayInformation, // q: SYSTEM_TIMEOFDAY_INFORMATION
    SystemPathInformation, // not implemented
    SystemProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
} SYSTEM_INFORMATION_CLASS;

typedef enum _KTHREAD_STATE
{
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting,
    Transition,
    DeferredReady,
    GateWaitObsolete,
    WaitingForProcessInSwap,
    MaximumThreadState
} KTHREAD_STATE, * PKTHREAD_STATE;

typedef struct _SYSTEM_THREAD_INFORMATION
{
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    CLIENT_ID ClientId;
    KPRIORITY Priority;
    LONG BasePriority;
    ULONG ContextSwitches;
    KTHREAD_STATE ThreadState;
    KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
    ULONG HardFaultCount; // since WIN7
    ULONG NumberOfThreadsHighWatermark; // since WIN7
    ULONGLONG CycleTime; // since WIN7
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    SYSTEM_THREAD_INFORMATION Threads[1]; // SystemProcessInformation
    // SYSTEM_EXTENDED_THREAD_INFORMATION Threads[1]; // SystemExtendedProcessinformation
    // SYSTEM_EXTENDED_THREAD_INFORMATION + SYSTEM_PROCESS_INFORMATION_EXTENSION // SystemFullProcessInformation
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

extern
NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQuerySystemInformation(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength
);

//
// Thanks to ntdiff.github.io / pdbex.
//

typedef struct _MMVAD_SHORT
{
    union
    {
        /* 0x0000 */ struct _RTL_BALANCED_NODE VadNode;
        /* 0x0000 */ struct _MMVAD_SHORT* NextVad;
    } u; /* size: 0x0018 */
    /* 0x0018 */ unsigned long StartingVpn;
    /* 0x001c */ unsigned long EndingVpn;
    /* 0x0020 */ unsigned char StartingVpnHigh;
    /* 0x0021 */ unsigned char EndingVpnHigh;
    /* 0x0022 */ unsigned char CommitChargeHigh;
    /* 0x0023 */ unsigned char SpareNT64VadUChar;
    /* 0x0024 */ long ReferenceCount;
    /* 0x0028 */ /*struct _EX_PUSH_LOCK*/ PVOID PushLock;
    union
    {
        union
        {
            /* 0x0030 */ unsigned long LongFlags;
            // /* 0x0030 */ struct _MMVAD_FLAGS VadFlags;
        } u; /* size: 0x0004 */
    } /* size: 0x0004 */ u2;
    union
    {
        union
        {
            /* 0x0034 */ unsigned long LongFlags1;
            // /* 0x0034 */ struct _MMVAD_FLAGS1 VadFlags1;
        } u; /* size: 0x0004 */
    } /* size: 0x0004 */ u1;
    /* 0x0038 */ struct _MI_VAD_EVENT_BLOCK* EventList;
} MMVAD_SHORT, * PMMVAD_SHORT; /* size: 0x0040 */

typedef struct _MMVAD
{
    /* 0x0000 */ struct _MMVAD_SHORT Core;
    union
    {
        union
        {
            /* 0x0040 */ unsigned long LongFlags2;
            // /* 0x0040 */ struct _MMVAD_FLAGS2 VadFlags2;
        } u; /* size: 0x0004 */
    } /* size: 0x0004 */ u2;
    /* 0x0048 */ struct _SUBSECTION* Subsection;
    /* 0x0050 */ struct _MMPTE* FirstPrototypePte;
    /* 0x0058 */ struct _MMPTE* LastContiguousPte;
    /* 0x0060 */ struct _LIST_ENTRY ViewLinks;
    /* 0x0070 */ struct _EPROCESS* VadsProcess;
    union
    {
        union
        {
            // /* 0x0078 */ struct _MI_VAD_SEQUENTIAL_INFO SequentialVa;
            /* 0x0078 */ struct _MMEXTEND_INFO* ExtendedInfo;
        } u; /* size: 0x0008 */
    } /* size: 0x0008 */ u4;
    /* 0x0080 */ struct _FILE_OBJECT* FileObject;
} MMVAD, * PMMVAD; /* size: 0x0088 */

//
// Some Sic constants / structures.
//

#define SIC_MEMORY_TAG ' ciS'
#define SIC_MEMORY_TAG_AVL_ENTRY 'AciS'
#define SIC_MEMORY_TAG_SLIST_ENTRY 'SciS'

#ifdef DBG
#define DebugPrint(_fmt_, ...) {  \
    DbgPrintEx(                   \
        DPFLTR_IHVDRIVER_ID,      \
        0xffffffff,               \
        (_fmt_),                  \
        __VA_ARGS__               \
    );                            \
}
#ifdef SIC_VERBOSE
#define DebugPrintVerbose(_fmt_, ...) DebugPrint((_fmt_), __VA_ARGS__)
#else
#define DebugPrintVerbose(_fmt_, ...) /* Nuthin. */
#endif // SIC_VERBOSE
#else
#define DebugPrint(...) /* Nuthin. */
#endif // DBG

typedef struct _SICK_LOOKUP_NODE_OWNERS
{
    SLIST_ENTRY SList;
    PEPROCESS Process;
    ULONG_PTR StartingVirtualAddress;
    ULONG_PTR EndingVirtualAddress;
} SICK_LOOKUP_NODE_OWNER, *PSICK_LOOKUP_NODE_OWNER;

typedef struct _SIC_LOOKUP_VAD_NODE
{
    struct _MMPTE *FirstPrototypePte;
    SLIST_HEADER Owners;
} SIC_LOOKUP_VAD_NODE, *PSIC_LOOKUP_VAD_NODE;

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
typedef
NTSTATUS
(*SIC_WALK_VAD_ROUTINE)(
    _In_ const PMMVAD Vad,
    _Inout_opt_ PVOID Context
);

typedef struct _SIC_WALK_VAD_CTX
{
    PRTL_AVL_TABLE LookupTable;
    PEPROCESS Process;
} SIC_WALK_VAD_CTX, *PSIC_WALK_VAD_CTX;

//
// Time to do some work I suppose.
//

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
SicDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    Unloads the driver. Pretty much empty for now.

Arguments:

    DriverObject - The driver object getting unloaded.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(DriverObject);
    PAGED_CODE();
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
SicGetProcessList(
    _Out_ PSYSTEM_PROCESS_INFORMATION *ProcessList
    )

/*++

Routine Description:

    Gets a list of running process on the system. If successful,
    *ProcessList holds a pointer to the beginning of the list. Also,
    the callers are expected to release the memory allocated for the list.

Arguments:

    ProcessList - Pointer to the beginning of the list.

Return Value:

    STATUS_SUCCESS if successful, STATUS_INVALID_PARAMETER if ProcessList is
    NULL and STATUS_UNSUCCESSFUL if failed.

--*/

{
    const UINT32 MaxAttempts = 10;
    NTSTATUS Status = STATUS_SUCCESS;

    //
    // If we didn't receive a ProcessList, we fail the call as we
    // expect one.
    //

    if(!ARGUMENT_PRESENT(ProcessList)) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Initialize the output buffer to NULL.
    //

    *ProcessList = NULL;

    //
    // Try out to get a process list in a maximum number of attempts.
    // We do this because we can encounter racy behavior where the world
    // changes in between the two ZwQuerySystemInformation.. sigh.
    //

    for(UINT32 Attempt = 0; Attempt < MaxAttempts; Attempt++) {
        ULONG ReturnLength = 0;
        PVOID LocalProcessList = NULL;

        //
        // How much space do we need?
        //

        Status = ZwQuerySystemInformation(
            SystemProcessInformation,
            NULL,
            0,
            &ReturnLength
        );

        //
        // Allocate memory to receive the process list.
        //

        LocalProcessList = ExAllocatePoolWithTag(
            PagedPool,
            ReturnLength,
            SIC_MEMORY_TAG
        );

        if(LocalProcessList == NULL) {
            continue;
        }

        //
        // Get a list of the processes running on the system.
        //

        Status = ZwQuerySystemInformation(
            SystemProcessInformation,
            LocalProcessList,
            ReturnLength,
            &ReturnLength
        );

        //
        // If we fail, let's clean up behind ourselves, and give it another try!
        //

        if(!NT_SUCCESS(Status)) {
            ExFreePoolWithTag(
                LocalProcessList,
                SIC_MEMORY_TAG
            );

            LocalProcessList = NULL;
            continue;
        }

        //
        // If we make it here, it means that we have our list and we are done.
        //

        *ProcessList = LocalProcessList;
        break;
    }

    //
    // If we managed to get the list, it's all good otherwise it's a failure.
    //

    return (*ProcessList != NULL) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
SicDumpVad(
    _In_ const PMMVAD Vad,
    _Inout_ PVOID Context
    )

/*++

Routine Description:

    Dumps a VAD entry into a lookup table. The goal of this routine
    is to keep track of every VAD with a PrototypePTE non NULL

Arguments:

    Vad - Pointer to the beginning of the list.

    Context - The Context is a PSIC_WALK_VAD_CTX.

Return Value:

    STATUS_SUCCESS if successful and STATUS_INSUFFICIENT_RESOURCES if
    an allocation failed.

--*/

{
    const PSIC_WALK_VAD_CTX WalkVadContext = Context;
    SIC_LOOKUP_VAD_NODE VadNode;

    PAGED_CODE();

    DebugPrintVerbose("    VAD: %p\n", Vad);

    if(Vad->FirstPrototypePte == NULL) {
        return STATUS_SUCCESS;
    }

    const ULONG_PTR StartingVpn = Vad->Core.StartingVpn | (
        (ULONG_PTR)Vad->Core.StartingVpnHigh << 32
    );

    const ULONG_PTR EndingVpn = Vad->Core.EndingVpn | (
        (ULONG_PTR)Vad->Core.EndingVpnHigh << 32
    );

    const ULONG_PTR StartingVirtualAddress = StartingVpn * PAGE_SIZE;
    const ULONG_PTR EndingVirtualAddress = EndingVpn * PAGE_SIZE;

    DebugPrintVerbose("      StartingVirtualAddress: %zx\n", StartingVirtualAddress);
    DebugPrintVerbose("      EndingVirtualAddress: %zx\n", EndingVirtualAddress);
    DebugPrintVerbose("      ProtoPTE: %p\n", Vad->FirstPrototypePte);

    //
    // Populate the node before adding it to the lookup table.
    //

    RtlZeroMemory(&VadNode, sizeof(VadNode));
    VadNode.FirstPrototypePte = Vad->FirstPrototypePte;

    BOOLEAN NewElement = FALSE;
    PSIC_LOOKUP_VAD_NODE InsertedNode = RtlInsertElementGenericTableAvl(
        WalkVadContext->LookupTable,
        &VadNode,
        sizeof(VadNode),
        &NewElement
    );

    //
    // If this is an existing node, we simply add the process to the list of
    // owners of this PrototypePTE.
    // To do that we allocate memory for an entry and push it down the SLIST.
    //

    PSICK_LOOKUP_NODE_OWNER Owner = ExAllocatePoolWithTag(
        PagedPool,
        sizeof(SICK_LOOKUP_NODE_OWNER),
        SIC_MEMORY_TAG_SLIST_ENTRY
    );

    if(Owner == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Owner->Process = WalkVadContext->Process;
    Owner->StartingVirtualAddress = StartingVirtualAddress;
    Owner->EndingVirtualAddress = EndingVirtualAddress;

    ExInterlockedPushEntrySList(
        &InsertedNode->Owners,
        &Owner->SList,
        NULL
    );

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
SicWalkVadTreeInOrder(
    _In_ const PMMVAD Root,
    _In_ SIC_WALK_VAD_ROUTINE Routine,
    _Inout_opt_ PVOID Context
    )

/*++

Routine Description:

    Walks the VAD AVL tree starting from a Root. For every node
    encountered, the user provided callback: Routine(Node, Context)
    is invoked.

Arguments:

    Root - Pointer to the root of the VAD tree.

    Routine - Callback routine provided by the user.

    Context - Pointer provided by the user that will get
    passed to the Routine during invokation.

Return Value:

    STATUS_SUCCESS if successful and STATUS_INSUFFICIENT_RESOURCES if
    an allocation failed.

--*/

{
    typedef struct _NODE {
        LIST_ENTRY List;
        PMMVAD Vad;
    } VAD_NODE, *PVAD_NODE;

    NTSTATUS Status = STATUS_SUCCESS;
    PMMVAD CurrentVad = NULL;
    LIST_ENTRY VadNodeStackHead;

    PAGED_CODE();

    //
    // Initialize the head of our stack.
    //

    InitializeListHead(&VadNodeStackHead);

    //
    // We keep iterating as long as we have either a non-null VAD
    // or if we have any remaining nodes to visit.
    //

    CurrentVad = Root;

    while(CurrentVad != NULL || !IsListEmpty(&VadNodeStackHead)) {

        //
        // We first go down as deep as possible in the tree using
        // the left child.
        //

        while(CurrentVad != NULL) {

            //
            // As we go down, we keep track of the nodes in the list.
            //

            PVAD_NODE VisitedVadNode = ExAllocatePoolWithTag(
                PagedPool,
                sizeof(VAD_NODE),
                SIC_MEMORY_TAG
            );

            if(VisitedVadNode == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto clean;
            }

            //
            // Insert the node on top of the stack.
            //

            VisitedVadNode->Vad = CurrentVad;
            InsertHeadList(&VadNodeStackHead, &VisitedVadNode->List);

            //
            // Let's keep going down.
            //

            CurrentVad = (PMMVAD)CurrentVad->Core.u.VadNode.Left;
        }

        //
        // At this point we are on a node that doesn't have a left child.
        // We can pop it off the stack and display it.
        //

        PVAD_NODE DisplayVadNode = (PVAD_NODE)RemoveHeadList(&VadNodeStackHead);

        if(&DisplayVadNode->List == &VadNodeStackHead) {
            break;
        }

        //
        // Invoke the user-provided callback for them to do whatever they want with the
        // node.
        //

        Status = Routine(
            DisplayVadNode->Vad,
            Context
        );

        //
        // Now let's explore its right tree as we have explored the left one already.
        //

        CurrentVad = (PMMVAD)DisplayVadNode->Vad->Core.u.VadNode.Right;

        ExFreePoolWithTag(
            DisplayVadNode,
            SIC_MEMORY_TAG
        );

        DisplayVadNode = NULL;

        //
        // If the callback fails, we abort the whole thing.
        //

        if(!NT_SUCCESS(Status)) {
            goto clean;
        }
    }

    clean:

    //
    // Ensure we have cleaned up every node in the list.
    //

    while(TRUE) {

        //
        // We pop the nodes one by one to clean them up.
        //

        PVAD_NODE VadNode = (PVAD_NODE)RemoveHeadList(&VadNodeStackHead);

        if(&VadNode->List == &VadNodeStackHead) {
            break;
        }

        ExFreePoolWithTag(
            VadNode,
            SIC_MEMORY_TAG
        );

        VadNode = NULL;
    }

    return Status;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
RTL_GENERIC_COMPARE_RESULTS
SicCompareRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID FirstStruct,
    _In_ PVOID SecondStruct
    )

/*++

Routine Description:

    Compares one SIC_LOOKUP_VAD_NODE to another.

Arguments:

    Table - Pointer to the AVL table.

    FirstStruct - First node.

    SecondStruct - Second node.

Return Value:

    GenericEqual if the nodes are equal, GenericLessThan if the first node
    is less than the second node or GenericGreaterThan if the first node
    is greater than the second node.

--*/

{
    UNREFERENCED_PARAMETER(Table);
    const PSIC_LOOKUP_VAD_NODE First = FirstStruct;
    const PSIC_LOOKUP_VAD_NODE Second = SecondStruct;
    const BOOLEAN Equal = First->FirstPrototypePte == Second->FirstPrototypePte;

    if(Equal) {
        return GenericEqual;
    }

    const BOOLEAN LessThan = First->FirstPrototypePte < Second->FirstPrototypePte;
    return LessThan ? GenericLessThan : GenericGreaterThan;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
PVOID SicAllocateRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ ULONG ByteSize
    )

/*++

Routine Description:

    Allocates a SIC_LOOKUP_VAD_NODE for our lookup table.

Arguments:

    Table - Pointer to the AVL table.

    ByteSize - Size to allocate.

Return Value:

    NULL if the allocation failed or a pointer to the node.

--*/

{
    UNREFERENCED_PARAMETER(Table);

    //
    // Allocate memory for the node.
    //

    PVOID AvlNode =  ExAllocatePoolWithTag(
        PagedPool,
        ByteSize,
        SIC_MEMORY_TAG_AVL_ENTRY
    );

    if(AvlNode == NULL) {
        return NULL;
    }

    //
    // From the MSDN:
    // '''
    // For each new element, the AllocateRoutine is called to allocate memory for
    // caller-supplied data plus some additional memory for use by the Rtl...GenericTableAvl
    // routines. Note that because of this "additional memory," caller-supplied routines must
    // not access the first sizeof(RTL_BALANCED_LINKS) bytes of any element in the generic table.
    // '''
    //

    PSIC_LOOKUP_VAD_NODE Node = (PSIC_LOOKUP_VAD_NODE)(
        (ULONG_PTR)AvlNode + sizeof(RTL_BALANCED_LINKS)
    );

    //
    // Initialize the SLIST of Owners.
    //

    InitializeSListHead(&Node->Owners);
    return AvlNode;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID SicFreeRoutine(
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID Buffer
    )

/*++

Routine Description:

    Frees a node from the lookup table.

Arguments:

    Table - Pointer to the AVL table.

    Buffer - Pointer to the node to release.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(Table);

    //
    // From the MSDN:
    // '''
    // For each new element, the AllocateRoutine is called to allocate memory for
    // caller-supplied data plus some additional memory for use by the Rtl...GenericTableAvl
    // routines. Note that because of this "additional memory," caller-supplied routines must
    // not access the first sizeof(RTL_BALANCED_LINKS) bytes of any element in the generic table.
    // '''
    //

    PSIC_LOOKUP_VAD_NODE Node = (PSIC_LOOKUP_VAD_NODE)(
        (ULONG_PTR)Buffer + sizeof(RTL_BALANCED_LINKS)
    );

    //
    // Let's clear the Owners SLIST.
    //

    while(TRUE) {

        //
        // Pop, pop, pop.
        //

        PSICK_LOOKUP_NODE_OWNER Owner = (PSICK_LOOKUP_NODE_OWNER)ExInterlockedPopEntrySList(
            &Node->Owners,
            NULL
        );

        //
        // All right, the SLIST is empty let's break out of the loop.
        //

        if(Owner == NULL) {
            break;
        }

        //
        // Clean-up the memory that we allocated for the SLIST entry.
        //

        ExFreePoolWithTag(
            Owner,
            SIC_MEMORY_TAG_SLIST_ENTRY
        );

        Owner = NULL;
    }

    //
    // The list should be empty now.
    //

    NT_ASSERT(ExQueryDepthSList(&Node->Owners) == 0);

    //
    // Free the actual node.
    //

    ExFreePoolWithTag(
        Buffer,
        SIC_MEMORY_TAG_AVL_ENTRY
    );

    Buffer = NULL;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
SicDude(
    )

/*++

Routine Description:

    Do the job.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS if successful or STATUS_* otherwise.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSYSTEM_PROCESS_INFORMATION ProcessList = NULL;
    PSYSTEM_PROCESS_INFORMATION CurrentProcess = NULL;
    RTL_AVL_TABLE LookupTable;
    SIC_WALK_VAD_CTX WalkVadContext;

    PAGED_CODE();

    //
    // Initialize the look-up table.
    //

    RtlInitializeGenericTableAvl(
        &LookupTable,
        SicCompareRoutine,
        SicAllocateRoutine,
        SicFreeRoutine,
        NULL
    );

    WalkVadContext.LookupTable = &LookupTable;

    //
    // Get a list of processes.
    //

    Status = SicGetProcessList(&ProcessList);

    if(!NT_SUCCESS(Status)) {
        goto clean;
    }

    //
    // We need to walk each of them and walk through their VAD trees.
    //

    CurrentProcess = ProcessList;
    while(CurrentProcess->NextEntryOffset != 0) {
        PEPROCESS Process = NULL;

        //
        // Display the process name.
        //

        const PUNICODE_STRING ProcessName = &CurrentProcess->ImageName;
        DebugPrint("Process: %wZ\n", ProcessName);

        //
        // Reference the process to not have it die under us.
        //

        Status = PsLookupProcessByProcessId(
            CurrentProcess->UniqueProcessId,
            &Process
        );

        if(!NT_SUCCESS(Status)) {
            goto next;
        }

        DebugPrint("  EPROCESS: %p\n", Process);

        //
        // TODO: Check if EPROCESS.AddressCreationLock can be used to lock the
        // address space of a process to not have it change under us.
        // TODO: Also check nt!MiLockWorkingSetShared
        //

        //
        // Prepare the context structure that walking callback will receive.
        //

        WalkVadContext.Process = Process;

        //
        // Grab the VadRoot and walk the tree.
        //

        // ntdll!_EPROCESS
        //    + 0x658 VadRoot : _RTL_AVL_TREE
        const UINT32 EprocessToVadRoot = 0x658;
        const PMMVAD VadRoot = *(PMMVAD*)((ULONG_PTR)Process + EprocessToVadRoot);
        DebugPrint("  VadRoot: %p\n", VadRoot);

        Status = SicWalkVadTreeInOrder(
            VadRoot,
            SicDumpVad,
            &WalkVadContext
        );

        //
        // Don't forget to de-reference the EPROCESS object.
        //

        ObDereferenceObject(Process);

        Process = NULL;

        next:

        //
        // We are done with the current process, move to the next.
        //

        CurrentProcess = (PSYSTEM_PROCESS_INFORMATION)(
            (ULONG_PTR)CurrentProcess + CurrentProcess->NextEntryOffset
        );
    }

    //
    // Once we walked all the processes, let's walk the lookup table
    // to find the entries that have more than one owners.
    //

    for(PSIC_LOOKUP_VAD_NODE Node = RtlEnumerateGenericTableAvl(&LookupTable, TRUE);
        Node != NULL;
        Node = RtlEnumerateGenericTableAvl(&LookupTable, FALSE)
        ) {

        //
        // We are interested only in PrototypePTE with more than an owner.
        //

        const USHORT NumberOwners = ExQueryDepthSList(&Node->Owners);
        if(NumberOwners <= 1) {
            continue;
        }

        DebugPrint(
            "PrototypePTE: %p shared with:\n",
            Node->FirstPrototypePte
        );

        //
        // Now let's walk the owners one by one.
        //

        while(TRUE) {

            //
            // Pop, pop, pop.
            //

            PSICK_LOOKUP_NODE_OWNER Owner = (PSICK_LOOKUP_NODE_OWNER)ExInterlockedPopEntrySList(
                &Node->Owners,
                NULL
            );

            //
            // All right, the SLIST is empty let's break out of the loop.
            //

            if(Owner == NULL) {
                break;
            }

            //
            // Display the owning process as well as the virtual addresses of the mapping.
            //

            DebugPrint(
                "  EPROCESS %p at %zx-%zx\n",
                Owner->Process,
                Owner->StartingVirtualAddress,
                Owner->EndingVirtualAddress
            );

            //
            // Clean-up the memory that we allocated for the SLIST entry.
            //

            ExFreePoolWithTag(
                Owner,
                SIC_MEMORY_TAG_SLIST_ENTRY
            );

            Owner = NULL;
        }

        //
        // The list should be empty now.
        //

        NT_ASSERT(ExQueryDepthSList(&Node->Owners) == 0);
    }

    clean:

    //
    // Clear the table.
    //

    while(!RtlIsGenericTableEmptyAvl(&LookupTable)) {

        //
        // Get the entry at index 0.
        //

        PVOID Entry = RtlGetElementGenericTableAvl(
            &LookupTable,
            0
        );

        //
        // And delete it! Note that the SicFreeRoutine is called
        // on every node (and so it will also clean up the Owners).
        //

        RtlDeleteElementGenericTableAvl(
            &LookupTable,
            Entry
        );

        Entry = NULL;
    }

    //
    // The lookup table should also be empty now.
    //

    NT_ASSERT(RtlIsGenericTableEmptyAvl(&LookupTable));

    //
    // Don't forget to clean the process list.
    //

    if(ProcessList != NULL) {
        ExFreePoolWithTag(
            ProcessList,
            SIC_MEMORY_TAG
        );

        ProcessList = NULL;
    }

    return Status;
}

_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
SicDispatchDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )

/*++

Routine Description:

    Handles IOCTL requests coming from usermode.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the Interrupt Request Packet.

Return Value:

    STATUS_SUCCESS if successful or STATUS_* otherwise.

--*/

{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS Status = STATUS_SUCCESS;
    const PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    const ULONG IoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    PAGED_CODE();

    switch(IoControlCode) {
        default: {
            break;
        }
    }

    //
    // We are done with this IRP, so we fill in the IoStatus part.
    //

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;

    //
    // As well as completing it!
    //

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the main of the driver.

Arguments:

    DriverObject - Pointer to the driver object.

    RegistryPath - According to MSDN:
    """
    A pointer to a UNICODE_STRING structure that
    specifies the path to the driver's Parameters
    key in the registry.
    """

Return Value:

    STATUS_SUCCESS if successful or STATUS_* otherwise.

--*/

{
    UNREFERENCED_PARAMETER(RegistryPath);

    PAGED_CODE();

    DriverObject->DriverUnload = SicDriverUnload;

    SicDude();

    //
    // Set-up the Unload / I/O callbacks.
    //

    // DriverObject->DriverUnload = SicDriverUnload;
    // DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SicDispatchDeviceControl;
    return STATUS_FAILED_DRIVER_ENTRY;
}
