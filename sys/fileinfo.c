/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2017 - 2020 Google, Inc.
  Copyright (C) 2015 - 2019 Adrien J. <liryna.stark@gmail.com> and Maxime C. <maxime@islog.com>
  Copyright (C) 2007 - 2011 Hiroki Asakawa <info@dokan-dev.net>

  http://dokan-dev.github.io

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "dokan.h"
#include "util/irp_buffer_helper.h"
#include "util/str.h"

NTSTATUS
DokanDispatchQueryInformation(__in PDEVICE_OBJECT DeviceObject, __in PIRP Irp) {
  NTSTATUS status = STATUS_NOT_IMPLEMENTED;
  PIO_STACK_LOCATION irpSp;
  PFILE_OBJECT fileObject;
  FILE_INFORMATION_CLASS infoClass;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  PDokanVCB vcb;
  ULONG eventLength;
  PEVENT_CONTEXT eventContext;

  // PAGED_CODE();

  __try {
    Irp->IoStatus.Information = 0;
    DDbgPrint("==> DokanQueryInformation\n");

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpSp->FileObject;
    infoClass = irpSp->Parameters.QueryFile.FileInformationClass;

    DDbgPrint("  FileInfoClass %d\n", infoClass);
    DDbgPrint("  ProcessId %lu\n", IoGetRequestorProcessId(Irp));

    if (fileObject == NULL) {
      DDbgPrint("  fileObject == NULL\n");
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    DokanPrintFileName(fileObject);

    vcb = DeviceObject->DeviceExtension;
    if (GetIdentifierType(vcb) != VCB ||
        !DokanCheckCCB(vcb->Dcb, fileObject->FsContext2)) {
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    ccb = (PDokanCCB)fileObject->FsContext2;
    ASSERT(ccb != NULL);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);

    OplockDebugRecordMajorFunction(fcb, IRP_MJ_QUERY_INFORMATION);    
    DokanFCBLockRO(fcb);
    switch (infoClass) {
    case FileBasicInformation:
      DDbgPrint("  FileBasicInformation\n");
      break;
    case FileInternalInformation:
      DDbgPrint("  FileInternalInformation\n");
      break;
    case FileEaInformation:
      DDbgPrint("  FileEaInformation\n");
      break;
    case FileStandardInformation:
      DDbgPrint("  FileStandardInformation\n");
      break;
    case FileAllInformation:
      DDbgPrint("  FileAllInformation\n");
      break;
    case FileAlternateNameInformation:
      DDbgPrint("  FileAlternateNameInformation\n");
      break;
    case FileAttributeTagInformation:
      DDbgPrint("  FileAttributeTagInformation\n");
      break;
    case FileCompressionInformation:
      DDbgPrint("  FileCompressionInformation\n");
      break;
    case FileNormalizedNameInformation:
      DDbgPrint("  FileNormalizedNameInformation\n");
    case FileNameInformation: {
      DDbgPrint("  FileNameInformation\n");

      PFILE_NAME_INFORMATION nameInfo;
      if (!PREPARE_OUTPUT(Irp, nameInfo, /*SetInformationOnFailure=*/FALSE)) {
        status = STATUS_BUFFER_OVERFLOW;
        __leave;
      }

      PUNICODE_STRING fileName = &fcb->FileName;
      PCHAR dest = (PCHAR)&nameInfo->FileName;
      nameInfo->FileNameLength = fileName->Length;

      BOOLEAN isNetworkDevice =
          (vcb->Dcb->VolumeDeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM);
      if (isNetworkDevice) {
        PUNICODE_STRING devicePath = vcb->Dcb->UNCName->Length
                                         ? vcb->Dcb->UNCName
                                         : vcb->Dcb->DiskDeviceName;
        nameInfo->FileNameLength += devicePath->Length;

        if (!AppendVarSizeOutputString(Irp, dest, devicePath,
                                       /*UpdateInformationOnFailure=*/FALSE,
                                       /*FillSpaceWithPartialString=*/TRUE)) {
          status = STATUS_BUFFER_OVERFLOW;
          __leave;
        }
        dest += devicePath->Length;
      }

      if (!AppendVarSizeOutputString(Irp, dest, fileName,
                                     /*UpdateInformationOnFailure=*/FALSE,
                                     /*FillSpaceWithPartialString=*/TRUE)) {
        status = STATUS_BUFFER_OVERFLOW;
        __leave;
      }
      status = STATUS_SUCCESS;
      __leave;
    } break;
    case FileNetworkOpenInformation:
      DDbgPrint("  FileNetworkOpenInformation\n");
      break;
    case FilePositionInformation: {
      DDbgPrint("  FilePositionInformation\n");

      PFILE_POSITION_INFORMATION posInfo;
      if (!PREPARE_OUTPUT(Irp, posInfo, /*SetInformationOnFailure=*/FALSE)) {
        status = STATUS_INFO_LENGTH_MISMATCH;
        __leave;
      }

      if (fileObject->CurrentByteOffset.QuadPart < 0) {
        status = STATUS_INVALID_PARAMETER;
        __leave;
      }

      // set the current file offset
      posInfo->CurrentByteOffset = fileObject->CurrentByteOffset;
      status = STATUS_SUCCESS;
      __leave;
    } break;
    case FileStreamInformation:
      DDbgPrint("  FileStreamInformation\n");
      if (!vcb->Dcb->UseAltStream) {
        DDbgPrint("    alternate stream disabled\n");
        status = STATUS_NOT_IMPLEMENTED;
        __leave;
      }
      break;
    case FileStandardLinkInformation:
      DDbgPrint("  FileStandardLinkInformation\n");
      break;
    case FileNetworkPhysicalNameInformation: {
      DDbgPrint("  FileNetworkPhysicalNameInformation\n");
      // This info class is generally not worth passing to the DLL. It will be
      // filled in with info that is accessible to the driver.

      PFILE_NETWORK_PHYSICAL_NAME_INFORMATION netInfo;
      if (!PREPARE_OUTPUT(Irp, netInfo, /*SetInformationOnFailure=*/FALSE)) {
        status = STATUS_BUFFER_OVERFLOW;
        __leave;
      }

      if (!AppendVarSizeOutputString(Irp, &netInfo->FileName, &fcb->FileName,
                                     /*UpdateInformationOnFailure=*/FALSE,
                                     /*FillSpaceWithPartialString=*/FALSE)) {
        status = STATUS_BUFFER_OVERFLOW;
        __leave;
      }
      status = STATUS_SUCCESS;
      __leave;
    }
    case FileRemoteProtocolInformation:
      DDbgPrint("  FileRemoteProtocolInformation\n");
      break;
    default:
      DDbgPrint("  unknown type:%d\n", infoClass);
      break;
    }

    if (fcb->BlockUserModeDispatch) {
      status = STATUS_SUCCESS;
      __leave;
    }

    // if it is not treadted in swich case

    // calculate the length of EVENT_CONTEXT
    // sum of it's size and file name length
    eventLength = sizeof(EVENT_CONTEXT) + fcb->FileName.Length;

    eventContext = AllocateEventContext(vcb->Dcb, Irp, eventLength, ccb);

    if (eventContext == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      __leave;
    }

    eventContext->Context = ccb->UserContext;
    // DDbgPrint("   get Context %X\n", (ULONG)ccb->UserContext);

    eventContext->Operation.File.FileInformationClass = infoClass;

    // bytes length which is able to be returned
    eventContext->Operation.File.BufferLength =
        irpSp->Parameters.QueryFile.Length;

    // copy file name to EventContext from FCB
    eventContext->Operation.File.FileNameLength = fcb->FileName.Length;
    RtlCopyMemory(eventContext->Operation.File.FileName, fcb->FileName.Buffer,
                  fcb->FileName.Length);

    // register this IRP to pending IRP list
    status = DokanRegisterPendingIrp(DeviceObject, Irp, eventContext, 0);

  } __finally {
    if (fcb)
      DokanFCBUnlock(fcb);

    // Warning: there seems to be a verifier failure about using freed memory if
    // we de-reference Irp in here when the status is pending. We are not sure
    // why this would be.
    DokanCompleteDispatchRoutine(Irp, status);

    DDbgPrint("<== DokanQueryInformation\n");
  }

  return status;
}

VOID DokanCompleteQueryInformation(__in PIRP_ENTRY IrpEntry,
                                   __in PEVENT_INFORMATION EventInfo) {
  PIRP irp;
  PIO_STACK_LOCATION irpSp;
  NTSTATUS status = STATUS_SUCCESS;
  ULONG info = 0;
  ULONG bufferLen = 0;
  PVOID buffer = NULL;
  PDokanCCB ccb;

  DDbgPrint("==> DokanCompleteQueryInformation\n");

  irp = IrpEntry->Irp;
  irpSp = IrpEntry->IrpSp;

  ccb = IrpEntry->FileObject->FsContext2;

  ASSERT(ccb != NULL);

  ccb->UserContext = EventInfo->Context;
  // DDbgPrint("   set Context %X\n", (ULONG)ccb->UserContext);

  // where we shold copy FileInfo to
  buffer = irp->AssociatedIrp.SystemBuffer;

  // available buffer size
  bufferLen = irpSp->Parameters.QueryFile.Length;

  // buffer is not specified or short of size
  if (bufferLen == 0 || buffer == NULL || bufferLen < EventInfo->BufferLength) {
    info = 0;
    status = STATUS_INSUFFICIENT_RESOURCES;

  } else {

    //
    // we write FileInfo from user mode
    //
    ASSERT(buffer != NULL);

    RtlZeroMemory(buffer, bufferLen);
    RtlCopyMemory(buffer, EventInfo->Buffer, EventInfo->BufferLength);

    // written bytes
    info = EventInfo->BufferLength;
    status = EventInfo->Status;

    //Update file size to FCB
    if (NT_SUCCESS(status) &&
            irpSp->Parameters.QueryFile.FileInformationClass ==
                FileAllInformation ||
        irpSp->Parameters.QueryFile.FileInformationClass ==
            FileStandardInformation ||
        irpSp->Parameters.QueryFile.FileInformationClass ==
            FileNetworkOpenInformation) {

      FSRTL_ADVANCED_FCB_HEADER *header = IrpEntry->FileObject->FsContext;
      LONGLONG allocationSize = 0;
      LONGLONG fileSize = 0;

      ASSERT(header != NULL);

      if (irpSp->Parameters.QueryFile.FileInformationClass ==
          FileAllInformation) {

        PFILE_ALL_INFORMATION allInfo = (PFILE_ALL_INFORMATION)buffer;
        allocationSize = allInfo->StandardInformation.AllocationSize.QuadPart;
        fileSize = allInfo->StandardInformation.EndOfFile.QuadPart;

        allInfo->PositionInformation.CurrentByteOffset =
            IrpEntry->FileObject->CurrentByteOffset;

      } else if (irpSp->Parameters.QueryFile.FileInformationClass ==
                 FileStandardInformation) {

        PFILE_STANDARD_INFORMATION standardInfo =
            (PFILE_STANDARD_INFORMATION)buffer;
        allocationSize = standardInfo->AllocationSize.QuadPart;
        fileSize = standardInfo->EndOfFile.QuadPart;

      } else if (irpSp->Parameters.QueryFile.FileInformationClass ==
                 FileNetworkOpenInformation) {

        PFILE_NETWORK_OPEN_INFORMATION networkInfo =
            (PFILE_NETWORK_OPEN_INFORMATION)buffer;
        allocationSize = networkInfo->AllocationSize.QuadPart;
        fileSize = networkInfo->EndOfFile.QuadPart;
      }

      InterlockedExchange64(&header->AllocationSize.QuadPart, allocationSize);
      InterlockedExchange64(&header->FileSize.QuadPart, fileSize);

      DDbgPrint("  AllocationSize: %llu, EndOfFile: %llu\n", allocationSize,
                fileSize);
    }
  }

  DokanCompleteIrpRequest(irp, status, info);

  DDbgPrint("<== DokanCompleteQueryInformation\n");
}

VOID FlushFcb(__in PDokanFCB fcb, __in_opt PFILE_OBJECT fileObject) {

  if (fcb == NULL) {
    return;
  }

  if (fcb->SectionObjectPointers.ImageSectionObject != NULL) {
    DDbgPrint("  MmFlushImageSection FileName: %wZ FileCount: %lu.\n",
              &fcb->FileName, fcb->FileCount);
    MmFlushImageSection(&fcb->SectionObjectPointers, MmFlushForWrite);
    DDbgPrint("  MmFlushImageSection done FileName: %wZ FileCount: %lu.\n",
              &fcb->FileName, fcb->FileCount);
  }

  if (fcb->SectionObjectPointers.DataSectionObject != NULL) {
    DDbgPrint("  CcFlushCache FileName: %wZ FileCount: %lu.\n", &fcb->FileName,
              fcb->FileCount);

    CcFlushCache(&fcb->SectionObjectPointers, NULL, 0, NULL);

    DokanPagingIoLockRW(fcb);
    DokanPagingIoUnlock(fcb);

    CcPurgeCacheSection(&fcb->SectionObjectPointers, NULL, 0, FALSE);
    if (fileObject != NULL) {
      CcUninitializeCacheMap(fileObject, NULL, NULL);
    }

    DDbgPrint("  CcFlushCache done FileName: %wZ FileCount: %lu.\n",
              &fcb->FileName, fcb->FileCount);
  }
}

VOID FlushAllCachedFcb(__in PDokanFCB fcbRelatedTo,
                       __in_opt PFILE_OBJECT fileObject) {
  PLIST_ENTRY thisEntry, nextEntry, listHead;
  PDokanFCB fcb = NULL;

  if (fcbRelatedTo == NULL) {
    return;
  }

  DDbgPrint("  FlushAllCachedFcb\n");

  if (!DokanFCBFlagsIsSet(fcbRelatedTo, DOKAN_FILE_DIRECTORY)) {
    DDbgPrint("  FlushAllCachedFcb file passed in. Flush only this file %wZ.\n",
              &fcbRelatedTo->FileName);
    FlushFcb(fcbRelatedTo, fileObject);
    return;
  }

  DokanVCBLockRW(fcbRelatedTo->Vcb);

  listHead = &fcbRelatedTo->Vcb->NextFCB;

  for (thisEntry = listHead->Flink; thisEntry != listHead;
       thisEntry = nextEntry) {

    nextEntry = thisEntry->Flink;

    fcb = CONTAINING_RECORD(thisEntry, DokanFCB, NextFCB);

    if (DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)) {
      DDbgPrint("  FlushAllCachedFcb %wZ is directory so skip it.\n",
                &fcb->FileName);
      continue;
    }

    DDbgPrint("  FlushAllCachedFcb check %wZ if is related to %wZ\n",
              &fcb->FileName, &fcbRelatedTo->FileName);

    if (StartsWith(&fcb->FileName, &fcbRelatedTo->FileName)) {
      DDbgPrint("  FlushAllCachedFcb flush %wZ if flush is possible.\n",
                &fcb->FileName);
      FlushFcb(fcb, NULL);
    }

    fcb = NULL;
  }

  DokanVCBUnlock(fcbRelatedTo->Vcb);

  DDbgPrint("  FlushAllCachedFcb finished\n");
}

NTSTATUS
DokanDispatchSetInformation(__in PDEVICE_OBJECT DeviceObject, __in PIRP Irp) {

  NTSTATUS status = STATUS_NOT_IMPLEMENTED;
  PIO_STACK_LOCATION irpSp;
  PVOID buffer;
  PFILE_OBJECT fileObject;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  PDokanVCB vcb;
  ULONG eventLength;
  PFILE_OBJECT targetFileObject;
  PEVENT_CONTEXT eventContext;
  BOOLEAN isPagingIo = FALSE;
  BOOLEAN fcbLocked = FALSE;

  vcb = DeviceObject->DeviceExtension;

  __try {
    DDbgPrint("==> DokanSetInformationn\n");

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpSp->FileObject;

    if (fileObject == NULL) {
      DDbgPrint("  fileObject == NULL\n");
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    if (GetIdentifierType(vcb) != VCB ||
        !DokanCheckCCB(vcb->Dcb, fileObject->FsContext2)) {
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    ccb = (PDokanCCB)fileObject->FsContext2;
    ASSERT(ccb != NULL);

    DDbgPrint("  ProcessId %lu\n", IoGetRequestorProcessId(Irp));
    DokanPrintFileName(fileObject);

    buffer = Irp->AssociatedIrp.SystemBuffer;

    isPagingIo = (Irp->Flags & IRP_PAGING_IO);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);
    OplockDebugRecordMajorFunction(fcb, IRP_MJ_SET_INFORMATION);
    switch (irpSp->Parameters.SetFile.FileInformationClass) {
    case FileAllocationInformation: {
      if ((fileObject->SectionObjectPointer != NULL) &&
          (fileObject->SectionObjectPointer->DataSectionObject != NULL)) {

        LARGE_INTEGER AllocationSize =
            ((PFILE_ALLOCATION_INFORMATION)buffer)->AllocationSize;
        if (AllocationSize.QuadPart <
                fcb->AdvancedFCBHeader.AllocationSize.QuadPart &&
            !MmCanFileBeTruncated(fileObject->SectionObjectPointer,
                                  &AllocationSize)) {
          status = STATUS_USER_MAPPED_FILE;
          __leave;
        }
      }
      DDbgPrint(
          "  FileAllocationInformation %lld\n",
          ((PFILE_ALLOCATION_INFORMATION)buffer)->AllocationSize.QuadPart);
    } break;
    case FileBasicInformation:
      DDbgPrint("  FileBasicInformation\n");
      break;
    case FileDispositionInformation:
      DDbgPrint("  FileDispositionInformation\n");
    case FileDispositionInformationEx:
      DDbgPrint("  FileDispositionInformationEx\n");
      break;
    case FileEndOfFileInformation: {
      if ((fileObject->SectionObjectPointer != NULL) &&
          (fileObject->SectionObjectPointer->DataSectionObject != NULL)) {

        PFILE_END_OF_FILE_INFORMATION pInfoEoF =
            (PFILE_END_OF_FILE_INFORMATION)buffer;

        if (pInfoEoF->EndOfFile.QuadPart <
                fcb->AdvancedFCBHeader.FileSize.QuadPart &&
            !MmCanFileBeTruncated(fileObject->SectionObjectPointer,
                                  &pInfoEoF->EndOfFile)) {
          status = STATUS_USER_MAPPED_FILE;
          __leave;
        }

        if (!isPagingIo) {

          CcFlushCache(&fcb->SectionObjectPointers, NULL, 0, NULL);

          DokanPagingIoLockRW(fcb);
          DokanPagingIoUnlock(fcb);

          CcPurgeCacheSection(&fcb->SectionObjectPointers, NULL, 0, FALSE);
        }
      }
      DDbgPrint("  FileEndOfFileInformation %lld\n",
                ((PFILE_END_OF_FILE_INFORMATION)buffer)->EndOfFile.QuadPart);
    } break;
    case FileLinkInformation:
      DDbgPrint("  FileLinkInformation\n");
      break;
    case FilePositionInformation: {
      PFILE_POSITION_INFORMATION posInfo;

      posInfo = (PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
      ASSERT(posInfo != NULL);

      DDbgPrint("  FilePositionInformation %lld\n",
                posInfo->CurrentByteOffset.QuadPart);
      fileObject->CurrentByteOffset = posInfo->CurrentByteOffset;

      status = STATUS_SUCCESS;

      __leave;
    } break;
    case FileRenameInformation:
    case FileRenameInformationEx:
      DDbgPrint("  FileRenameInformation\n");
      /* Flush any opened files before doing a rename
       * of the parent directory or the specific file
       */
      targetFileObject = irpSp->Parameters.SetFile.FileObject;
      if (targetFileObject) {
        DDbgPrint("  FileRenameInformation targetFileObject specified so "
                  "perform flush\n");
        PDokanCCB targetCcb = (PDokanCCB)targetFileObject->FsContext2;
        ASSERT(targetCcb != NULL);
        FlushAllCachedFcb(targetCcb->Fcb, targetFileObject);
      }
      FlushAllCachedFcb(fcb, fileObject);
      break;
    case FileValidDataLengthInformation:
      DDbgPrint("  FileValidDataLengthInformation\n");
      break;
    default:
      DDbgPrint("  unknown type:%d\n",
                irpSp->Parameters.SetFile.FileInformationClass);
      break;
    }

    //
    // when this IRP is not handled in swich case
    //

    // calcurate the size of EVENT_CONTEXT
    // it is sum of file name length and size of FileInformation
    DokanFCBLockRW(fcb);
    fcbLocked = TRUE;

    eventLength = sizeof(EVENT_CONTEXT) + fcb->FileName.Length;
    if (irpSp->Parameters.SetFile.Length > MAXULONG - eventLength) {
      DDbgPrint("  Invalid SetFile Length received\n");
      status = STATUS_INSUFFICIENT_RESOURCES;
      __leave;
    }
    eventLength += irpSp->Parameters.SetFile.Length;

    targetFileObject = irpSp->Parameters.SetFile.FileObject;
    if (targetFileObject) {
      DDbgPrint("  FileObject Specified %wZ\n", &(targetFileObject->FileName));
      if (targetFileObject->FileName.Length > MAXULONG - eventLength) {
        DDbgPrint("  Invalid FileObject FileName Length received\n");
        status = STATUS_INSUFFICIENT_RESOURCES;
        __leave;
      }
      eventLength += targetFileObject->FileName.Length;
    }

    eventContext = AllocateEventContext(vcb->Dcb, Irp, eventLength, ccb);
    if (eventContext == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      __leave;
    }

    eventContext->Context = ccb->UserContext;

    eventContext->Operation.SetFile.FileInformationClass =
        irpSp->Parameters.SetFile.FileInformationClass;

    // the size of FileInformation
    eventContext->Operation.SetFile.BufferLength =
        irpSp->Parameters.SetFile.Length;

    // the offset from begining of structure to fill FileInfo
    eventContext->Operation.SetFile.BufferOffset =
        FIELD_OFFSET(EVENT_CONTEXT, Operation.SetFile.FileName[0]) +
        fcb->FileName.Length + sizeof(WCHAR); // the last null char

    BOOLEAN isRenameOrLink =
        irpSp->Parameters.SetFile.FileInformationClass == FileRenameInformation
		|| irpSp->Parameters.SetFile.FileInformationClass == FileLinkInformation
		|| irpSp->Parameters.SetFile.FileInformationClass == FileRenameInformationEx;

    if (!isRenameOrLink) {
      // copy FileInformation
      RtlCopyMemory(
          (PCHAR)eventContext + eventContext->Operation.SetFile.BufferOffset,
          Irp->AssociatedIrp.SystemBuffer, irpSp->Parameters.SetFile.Length);
    }

    if (isRenameOrLink) {
      // We need to hanle FileRenameInformation separetly because the structure
      // of FILE_RENAME_INFORMATION
      // has HANDLE type field, which size is different in 32 bit and 64 bit
      // environment.
      // This cases problems when driver is 64 bit and user mode library is 32
      // bit.
      PFILE_RENAME_INFORMATION renameInfo =
          (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
      PDOKAN_RENAME_INFORMATION renameContext = (PDOKAN_RENAME_INFORMATION)(
          (PCHAR)eventContext + eventContext->Operation.SetFile.BufferOffset);

      // This code assumes FILE_RENAME_INFORMATION and FILE_LINK_INFORMATION
      // have
      // the same typse and fields.
      ASSERT(sizeof(FILE_RENAME_INFORMATION) == sizeof(FILE_LINK_INFORMATION));

      renameContext->ReplaceIfExists = renameInfo->ReplaceIfExists;
      renameContext->FileNameLength = renameInfo->FileNameLength;
      RtlCopyMemory(renameContext->FileName, renameInfo->FileName,
                    renameInfo->FileNameLength);

      if (targetFileObject != NULL) {
        // if Parameters.SetFile.FileObject is specified, replace
        // FILE_RENAME_INFO's file name by
        // FileObject's file name. The buffer size is already adjusted.

        DDbgPrint("  renameContext->FileNameLength %d\n",
                  renameContext->FileNameLength);
        DDbgPrint("  renameContext->FileName %ws\n", renameContext->FileName);
        RtlZeroMemory(renameContext->FileName, renameContext->FileNameLength);

        PFILE_OBJECT parentFileObject = targetFileObject->RelatedFileObject;
        if (parentFileObject != NULL) {
          RtlCopyMemory(renameContext->FileName,
                        parentFileObject->FileName.Buffer,
                        parentFileObject->FileName.Length);

          RtlStringCchCatW(renameContext->FileName, NTSTRSAFE_MAX_CCH, L"\\");
          RtlStringCchCatW(renameContext->FileName, NTSTRSAFE_MAX_CCH,
                           targetFileObject->FileName.Buffer);
          renameContext->FileNameLength = targetFileObject->FileName.Length +
                                          parentFileObject->FileName.Length +
                                          sizeof(WCHAR);
        } else {
          RtlCopyMemory(renameContext->FileName,
                        targetFileObject->FileName.Buffer,
                        targetFileObject->FileName.Length);
          renameContext->FileNameLength = targetFileObject->FileName.Length;
        }
      }

      if (irpSp->Parameters.SetFile.FileInformationClass == FileRenameInformation
		  || irpSp->Parameters.SetFile.FileInformationClass == FileRenameInformationEx) {
        DDbgPrint("   rename: %wZ => %ls, FileCount = %u\n", fcb->FileName,
                  renameContext->FileName, (ULONG)fcb->FileCount);
      }
    }

    // copy the file name
    eventContext->Operation.SetFile.FileNameLength = fcb->FileName.Length;
    RtlCopyMemory(eventContext->Operation.SetFile.FileName,
                  fcb->FileName.Buffer, fcb->FileName.Length);

    // FsRtlCheckOpLock is called with non-NULL completion routine - not blocking.
    status = DokanCheckOplock(fcb, Irp, eventContext, DokanOplockComplete,
                              DokanPrePostIrp);
    //
    //  if FsRtlCheckOplock returns STATUS_PENDING the IRP has been posted
    //  to service an oplock break and we need to leave now.
    //
    if (status != STATUS_SUCCESS) {
      if (status == STATUS_PENDING) {
        DDbgPrint("   FsRtlCheckOplock returned STATUS_PENDING\n");
      } else {
        DokanFreeEventContext(eventContext);
      }
      __leave;
    }

    // register this IRP to waiting IRP list and make it pending status
    status = DokanRegisterPendingIrp(DeviceObject, Irp, eventContext, 0);

  } __finally {
    if (fcbLocked)
      DokanFCBUnlock(fcb);

    DokanCompleteIrpRequest(Irp, status, 0);

    DDbgPrint("<== DokanSetInformation\n");
  }

  return status;
}

// Returns the last index, |i|, so that [0, i] represents the range of the path
// to the parent directory. For example, if |fileName| is |C:\temp\text.txt|,
// returns 7 (the index of |\| right before |text.txt|).
//
// Returns -1 if no '\\' is found,
LONG GetParentDirectoryEndingIndex(PUNICODE_STRING fileName) {
  if (fileName->Length == 0) {
    return -1;
  }
  // If the path ends with L'\\' (in which case, this is a directory, that last
  // '\\' character can be ignored.)
  USHORT lastIndex = fileName->Length / sizeof(WCHAR) - 1;
  if (fileName->Buffer[lastIndex] == L'\\') {
    lastIndex--;
  }
  for (LONG index = lastIndex; index >= 0; index--) {
    if (fileName->Buffer[index] == L'\\') {
      return index;
    }
  }
  // There is no '\\' found.
  return -1;
}

// Returns |TRUE| if |fileName1| and |fileName2| represent paths to two
// files/folders that are in the same directory.
BOOLEAN IsInSameDirectory(PUNICODE_STRING fileName1,
                          PUNICODE_STRING fileName2) {
  LONG parentEndingIndex = GetParentDirectoryEndingIndex(fileName1);
  if (parentEndingIndex != GetParentDirectoryEndingIndex(fileName2)) {
    return FALSE;
  }
  for (LONG i = 0; i < parentEndingIndex; i++) {
    // TODO(ttdinhtrong): This code assumes case sensitive, which is not always
    // true. As of now we do not know if the user is in case sensitive or case
    // insensitive mode.
    if (fileName1->Buffer[i] != fileName2->Buffer[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

VOID DokanCompleteSetInformation(__in PIRP_ENTRY IrpEntry,
                                 __in PEVENT_INFORMATION EventInfo) {
  PIRP irp;
  PIO_STACK_LOCATION irpSp;
  NTSTATUS status;
  ULONG info = 0;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  UNICODE_STRING oldFileName;
  BOOLEAN fcbLocked = FALSE;
  BOOLEAN vcbLocked = FALSE;

  FILE_INFORMATION_CLASS infoClass;
  irp = IrpEntry->Irp;
  status = EventInfo->Status;

  __try {

    DDbgPrint("==> DokanCompleteSetInformation\n");

    irpSp = IrpEntry->IrpSp;

    ccb = IrpEntry->FileObject->FsContext2;
    ASSERT(ccb != NULL);

    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(&ccb->Resource, TRUE);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);

    info = EventInfo->BufferLength;

    infoClass = irpSp->Parameters.SetFile.FileInformationClass;

    // Note that we do not acquire the resource for paging file
    // operations in order to avoid deadlock with Mm
    if (!(irp->Flags & IRP_PAGING_IO)) {
      // If we are going to change the FileName on the FCB, then we want the VCB
      // locked so that we don't race with the loop in create.c that searches
      // currently open FCBs for a matching name. However, we need to lock that
      // before the FCB so that the lock order is consistent everywhere.
      if (NT_SUCCESS(status) && infoClass == FileRenameInformation) {
        DokanVCBLockRW(fcb->Vcb);
        vcbLocked = TRUE;
      }
      DokanFCBLockRW(fcb);
      fcbLocked = TRUE;
    }

    ccb->UserContext = EventInfo->Context;

    RtlZeroMemory(&oldFileName, sizeof(UNICODE_STRING));

    if (NT_SUCCESS(status)) {

      if (infoClass == FileDispositionInformation ||
          infoClass == FileDispositionInformationEx) {
        if (EventInfo->Operation.Delete.DeleteOnClose) {

          if (!MmFlushImageSection(&fcb->SectionObjectPointers,
                                   MmFlushForDelete)) {
            DDbgPrint("  Cannot delete user mapped image\n");
            status = STATUS_CANNOT_DELETE;
          } else {
            DokanCCBFlagsSetBit(ccb, DOKAN_DELETE_ON_CLOSE);
            DokanFCBFlagsSetBit(fcb, DOKAN_DELETE_ON_CLOSE);
            DDbgPrint("   FileObject->DeletePending = TRUE\n");
            IrpEntry->FileObject->DeletePending = TRUE;
          }

        } else {
          DokanCCBFlagsClearBit(ccb, DOKAN_DELETE_ON_CLOSE);
          DokanFCBFlagsClearBit(fcb, DOKAN_DELETE_ON_CLOSE);
          DDbgPrint("   FileObject->DeletePending = FALSE\n");
          IrpEntry->FileObject->DeletePending = FALSE;
        }
      }

      // if rename is executed, reassign the file name
      if (infoClass == FileRenameInformation ||
          infoClass == FileRenameInformationEx) {
        PVOID buffer = NULL;

        // this is used to inform rename in the bellow switch case
        oldFileName.Buffer = fcb->FileName.Buffer;
        oldFileName.Length = (USHORT)fcb->FileName.Length;
        oldFileName.MaximumLength = (USHORT)fcb->FileName.Length;

        // copy new file name
        buffer = DokanAllocZero(EventInfo->BufferLength + sizeof(WCHAR));
        if (buffer == NULL) {
          status = STATUS_INSUFFICIENT_RESOURCES;
          ExReleaseResourceLite(&ccb->Resource);
          KeLeaveCriticalRegion();
          __leave;
        }

        fcb->FileName.Buffer = buffer;
        ASSERT(fcb->FileName.Buffer != NULL);

        RtlCopyMemory(fcb->FileName.Buffer, EventInfo->Buffer,
                      EventInfo->BufferLength);

        fcb->FileName.Length = (USHORT)EventInfo->BufferLength;
        fcb->FileName.MaximumLength = (USHORT)EventInfo->BufferLength;
        DDbgPrint("   rename also done on fcb %wZ \n", &fcb->FileName);
      }
    }

    ExReleaseResourceLite(&ccb->Resource);
    KeLeaveCriticalRegion();

    if (NT_SUCCESS(status)) {
      switch (irpSp->Parameters.SetFile.FileInformationClass) {
      case FileAllocationInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      case FileBasicInformation:
        DokanNotifyReportChange(
            fcb,
            FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION,
            FILE_ACTION_MODIFIED);
        break;
      case FileDispositionInformation:
      case FileDispositionInformationEx:
        if (IrpEntry->FileObject->DeletePending) {
          if (DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)) {
            DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_DIR_NAME,
                                    FILE_ACTION_REMOVED);
          } else {
            DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_FILE_NAME,
                                    FILE_ACTION_REMOVED);
          }
        }
        break;
      case FileEndOfFileInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      case FileLinkInformation:
        // TODO: should check whether this is a directory
        // TODO: should notify new link name
        // DokanNotifyReportChange(vcb, ccb, FILE_NOTIFY_CHANGE_FILE_NAME,
        // FILE_ACTION_ADDED);
        break;
      case FilePositionInformation:
        // this is never used
        break;
      case FileRenameInformationEx:
      case FileRenameInformation: {
        DDbgPrint("  DokanCompleteSetInformation Report FileRenameInformation");

        if (IsInSameDirectory(&oldFileName, &fcb->FileName)) {
          DokanNotifyReportChange0(fcb, &oldFileName,
                                   DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)
                                       ? FILE_NOTIFY_CHANGE_DIR_NAME
                                       : FILE_NOTIFY_CHANGE_FILE_NAME,
                                   FILE_ACTION_RENAMED_OLD_NAME);
          DokanNotifyReportChange(fcb,
                                  DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)
                                      ? FILE_NOTIFY_CHANGE_DIR_NAME
                                      : FILE_NOTIFY_CHANGE_FILE_NAME,
                                  FILE_ACTION_RENAMED_NEW_NAME);
        } else {
          DokanNotifyReportChange0(fcb, &oldFileName,
                                   DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)
                                       ? FILE_NOTIFY_CHANGE_DIR_NAME
                                       : FILE_NOTIFY_CHANGE_FILE_NAME,
                                   FILE_ACTION_REMOVED);
          DokanNotifyReportChange(fcb,
                                  DokanFCBFlagsIsSet(fcb, DOKAN_FILE_DIRECTORY)
                                      ? FILE_NOTIFY_CHANGE_DIR_NAME
                                      : FILE_NOTIFY_CHANGE_FILE_NAME,
                                  FILE_ACTION_ADDED);
        }
        // free old file name
        ExFreePool(oldFileName.Buffer);
      } break;
      case FileValidDataLengthInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      default:
        DDbgPrint("  unknown type:%d\n",
                  irpSp->Parameters.SetFile.FileInformationClass);
        break;
      }
    }

  } __finally {
    if (fcbLocked) {
      DokanFCBUnlock(fcb);
    }
    if (vcbLocked) {
      DokanVCBUnlock(fcb->Vcb);
    }

    DokanCompleteIrpRequest(irp, status, info);

    DDbgPrint("<== DokanCompleteSetInformation\n");
  }
}
