/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute dumper                                                            *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

  switch (A->getKind()) {
  default:
    llvm_unreachable("Unknown attribute kind!");
    break;
  case attr::AcquiredAfter: {
    const AcquiredAfterAttr *SA = cast<AcquiredAfterAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (AcquiredAfterAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::AcquiredBefore: {
    const AcquiredBeforeAttr *SA = cast<AcquiredBeforeAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (AcquiredBeforeAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Alias: {
    const AliasAttr *SA = cast<AliasAttr>(A);
    OS << " \"" << SA->getAliasee() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::AlignMac68k: {
    break;
  }
  case attr::Aligned: {
    const AlignedAttr *SA = cast<AlignedAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    if (SA->isAlignmentExpr()) {
      lastChild();
      dumpStmt(SA->getAlignmentExpr());
    } else
      dumpType(SA->getAlignmentType()->getType());
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::AllocSize: {
    const AllocSizeAttr *SA = cast<AllocSizeAttr>(A);
    for (AllocSizeAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I)
      OS << " " << *I;
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::AlwaysInline: {
    break;
  }
  case attr::AnalyzerNoReturn: {
    break;
  }
  case attr::Annotate: {
    const AnnotateAttr *SA = cast<AnnotateAttr>(A);
    OS << " \"" << SA->getAnnotation() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ArcWeakrefUnavailable: {
    break;
  }
  case attr::ArgumentWithTypeTag: {
    const ArgumentWithTypeTagAttr *SA = cast<ArgumentWithTypeTagAttr>(A);
    OS << " " << SA->getArgumentKind()->getName();
    OS << " " << SA->getArgumentIdx();
    OS << " " << SA->getTypeTagIdx();
    if (SA->getIsPointer()) OS << " IsPointer";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::AsmLabel: {
    const AsmLabelAttr *SA = cast<AsmLabelAttr>(A);
    OS << " \"" << SA->getLabel() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Availability: {
    const AvailabilityAttr *SA = cast<AvailabilityAttr>(A);
    OS << " " << SA->getPlatform()->getName();
    OS << " " << SA->getIntroduced();
    OS << " " << SA->getDeprecated();
    OS << " " << SA->getObsoleted();
    if (SA->getUnavailable()) OS << " Unavailable";
    OS << " \"" << SA->getMessage() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false || false || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false || false || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Blocks: {
    const BlocksAttr *SA = cast<BlocksAttr>(A);
    switch(SA->getType()) {
    case BlocksAttr::ByRef:
      OS << " ByRef";
      break;
    }
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::C11NoReturn: {
    break;
  }
  case attr::CDecl: {
    break;
  }
  case attr::CFAuditedTransfer: {
    break;
  }
  case attr::CFConsumed: {
    break;
  }
  case attr::CFReturnsNotRetained: {
    break;
  }
  case attr::CFReturnsRetained: {
    break;
  }
  case attr::CFUnknownTransfer: {
    break;
  }
  case attr::CUDAConstant: {
    break;
  }
  case attr::CUDADevice: {
    break;
  }
  case attr::CUDAGlobal: {
    break;
  }
  case attr::CUDAHost: {
    break;
  }
  case attr::CUDALaunchBounds: {
    const CUDALaunchBoundsAttr *SA = cast<CUDALaunchBoundsAttr>(A);
    OS << " " << SA->getMaxThreads();
    OS << " " << SA->getMinBlocks();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::CUDAShared: {
    break;
  }
  case attr::CXX11NoReturn: {
    break;
  }
  case attr::CarriesDependency: {
    break;
  }
  case attr::Cleanup: {
    const CleanupAttr *SA = cast<CleanupAttr>(A);
    OS << " ";
    dumpBareDeclRef(SA->getFunctionDecl());
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Cold: {
    break;
  }
  case attr::Common: {
    break;
  }
  case attr::Const: {
    break;
  }
  case attr::Constructor: {
    const ConstructorAttr *SA = cast<ConstructorAttr>(A);
    OS << " " << SA->getPriority();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::DLLExport: {
    break;
  }
  case attr::DLLImport: {
    break;
  }
  case attr::Deprecated: {
    const DeprecatedAttr *SA = cast<DeprecatedAttr>(A);
    OS << " \"" << SA->getMessage() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Destructor: {
    const DestructorAttr *SA = cast<DestructorAttr>(A);
    OS << " " << SA->getPriority();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Endian: {
    const EndianAttr *SA = cast<EndianAttr>(A);
    OS << " " << SA->getPlatform()->getName();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ExclusiveLockFunction: {
    const ExclusiveLockFunctionAttr *SA = cast<ExclusiveLockFunctionAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (ExclusiveLockFunctionAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ExclusiveLocksRequired: {
    const ExclusiveLocksRequiredAttr *SA = cast<ExclusiveLocksRequiredAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (ExclusiveLocksRequiredAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ExclusiveTrylockFunction: {
    const ExclusiveTrylockFunctionAttr *SA = cast<ExclusiveTrylockFunctionAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || SA->args_begin() != SA->args_end();
    setMoreChildren(MoreChildren);
    lastChild();
    dumpStmt(SA->getSuccessValue());
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (ExclusiveTrylockFunctionAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::FallThrough: {
    break;
  }
  case attr::FastCall: {
    break;
  }
  case attr::Final: {
    break;
  }
  case attr::ForceInline: {
    break;
  }
  case attr::Format: {
    const FormatAttr *SA = cast<FormatAttr>(A);
    OS << " \"" << SA->getType() << "\"";
    OS << " " << SA->getFormatIdx();
    OS << " " << SA->getFirstArg();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::FormatArg: {
    const FormatArgAttr *SA = cast<FormatArgAttr>(A);
    OS << " " << SA->getFormatIdx();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::GNUInline: {
    break;
  }
  case attr::GuardedBy: {
    const GuardedByAttr *SA = cast<GuardedByAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    lastChild();
    dumpStmt(SA->getArg());
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::GuardedVar: {
    break;
  }
  case attr::Hot: {
    break;
  }
  case attr::IBAction: {
    break;
  }
  case attr::IBOutlet: {
    break;
  }
  case attr::IBOutletCollection: {
    const IBOutletCollectionAttr *SA = cast<IBOutletCollectionAttr>(A);
    OS << " " << SA->getInterface().getAsString();
    OS << " ";
    SA->getInterfaceLoc().print(OS, *SM);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::InitPriority: {
    const InitPriorityAttr *SA = cast<InitPriorityAttr>(A);
    OS << " " << SA->getPriority();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::IntelOclBicc: {
    break;
  }
  case attr::LockReturned: {
    const LockReturnedAttr *SA = cast<LockReturnedAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    lastChild();
    dumpStmt(SA->getArg());
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Lockable: {
    break;
  }
  case attr::LocksExcluded: {
    const LocksExcludedAttr *SA = cast<LocksExcludedAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (LocksExcludedAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::MBlazeInterruptHandler: {
    break;
  }
  case attr::MBlazeSaveVolatiles: {
    break;
  }
  case attr::MSP430Interrupt: {
    const MSP430InterruptAttr *SA = cast<MSP430InterruptAttr>(A);
    OS << " " << SA->getNumber();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Malloc: {
    break;
  }
  case attr::MaxFieldAlignment: {
    const MaxFieldAlignmentAttr *SA = cast<MaxFieldAlignmentAttr>(A);
    OS << " " << SA->getAlignment();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::MayAlias: {
    break;
  }
  case attr::MinSize: {
    break;
  }
  case attr::Mips16: {
    break;
  }
  case attr::MsProperty: {
    break;
  }
  case attr::MsStruct: {
    break;
  }
  case attr::MultipleInheritance: {
    break;
  }
  case attr::NSBridged: {
    const NSBridgedAttr *SA = cast<NSBridgedAttr>(A);
    OS << " " << SA->getBridgedType()->getName();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::NSConsumed: {
    break;
  }
  case attr::NSConsumesSelf: {
    break;
  }
  case attr::NSReturnsAutoreleased: {
    break;
  }
  case attr::NSReturnsNotRetained: {
    break;
  }
  case attr::NSReturnsRetained: {
    break;
  }
  case attr::Naked: {
    break;
  }
  case attr::NoCommon: {
    break;
  }
  case attr::NoDebug: {
    break;
  }
  case attr::NoInline: {
    break;
  }
  case attr::NoInstrumentFunction: {
    break;
  }
  case attr::NoMips16: {
    break;
  }
  case attr::NoReturn: {
    break;
  }
  case attr::NoSanitizeAddress: {
    break;
  }
  case attr::NoSanitizeMemory: {
    break;
  }
  case attr::NoSanitizeThread: {
    break;
  }
  case attr::NoThreadSafetyAnalysis: {
    break;
  }
  case attr::NoThrow: {
    break;
  }
  case attr::NonNull: {
    const NonNullAttr *SA = cast<NonNullAttr>(A);
    for (NonNullAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I)
      OS << " " << *I;
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ObjCException: {
    break;
  }
  case attr::ObjCMethodFamily: {
    const ObjCMethodFamilyAttr *SA = cast<ObjCMethodFamilyAttr>(A);
    switch(SA->getFamily()) {
    case ObjCMethodFamilyAttr::OMF_None:
      OS << " OMF_None";
      break;
    case ObjCMethodFamilyAttr::OMF_alloc:
      OS << " OMF_alloc";
      break;
    case ObjCMethodFamilyAttr::OMF_copy:
      OS << " OMF_copy";
      break;
    case ObjCMethodFamilyAttr::OMF_init:
      OS << " OMF_init";
      break;
    case ObjCMethodFamilyAttr::OMF_mutableCopy:
      OS << " OMF_mutableCopy";
      break;
    case ObjCMethodFamilyAttr::OMF_new:
      OS << " OMF_new";
      break;
    }
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ObjCNSObject: {
    break;
  }
  case attr::ObjCPreciseLifetime: {
    break;
  }
  case attr::ObjCRequiresPropertyDefs: {
    break;
  }
  case attr::ObjCRequiresSuper: {
    break;
  }
  case attr::ObjCReturnsInnerPointer: {
    break;
  }
  case attr::ObjCRootClass: {
    break;
  }
  case attr::OpenCLImageAccess: {
    const OpenCLImageAccessAttr *SA = cast<OpenCLImageAccessAttr>(A);
    OS << " " << SA->getAccess();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::OpenCLKernel: {
    break;
  }
  case attr::Overloadable: {
    break;
  }
  case attr::Override: {
    break;
  }
  case attr::Ownership: {
    const OwnershipAttr *SA = cast<OwnershipAttr>(A);
    switch(SA->getOwnKind()) {
    case OwnershipAttr::Holds:
      OS << " Holds";
      break;
    case OwnershipAttr::Returns:
      OS << " Returns";
      break;
    case OwnershipAttr::Takes:
      OS << " Takes";
      break;
    }
    OS << " \"" << SA->getModule() << "\"";
    for (OwnershipAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I)
      OS << " " << *I;
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Packed: {
    break;
  }
  case attr::Pascal: {
    break;
  }
  case attr::Pcs: {
    const PcsAttr *SA = cast<PcsAttr>(A);
    switch(SA->getPCS()) {
    case PcsAttr::AAPCS:
      OS << " AAPCS";
      break;
    case PcsAttr::AAPCS_VFP:
      OS << " AAPCS_VFP";
      break;
    }
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::PnaclCall: {
    break;
  }
  case attr::PtGuardedBy: {
    const PtGuardedByAttr *SA = cast<PtGuardedByAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    lastChild();
    dumpStmt(SA->getArg());
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::PtGuardedVar: {
    break;
  }
  case attr::Ptr32: {
    break;
  }
  case attr::Ptr64: {
    break;
  }
  case attr::Pure: {
    break;
  }
  case attr::Regparm: {
    const RegparmAttr *SA = cast<RegparmAttr>(A);
    OS << " " << SA->getNumParams();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ReqdWorkGroupSize: {
    const ReqdWorkGroupSizeAttr *SA = cast<ReqdWorkGroupSizeAttr>(A);
    OS << " " << SA->getXDim();
    OS << " " << SA->getYDim();
    OS << " " << SA->getZDim();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ReturnsTwice: {
    break;
  }
  case attr::ScopedLockable: {
    break;
  }
  case attr::Section: {
    const SectionAttr *SA = cast<SectionAttr>(A);
    OS << " \"" << SA->getName() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Sentinel: {
    const SentinelAttr *SA = cast<SentinelAttr>(A);
    OS << " " << SA->getSentinel();
    OS << " " << SA->getNullPos();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::SharedLockFunction: {
    const SharedLockFunctionAttr *SA = cast<SharedLockFunctionAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (SharedLockFunctionAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::SharedLocksRequired: {
    const SharedLocksRequiredAttr *SA = cast<SharedLocksRequiredAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (SharedLocksRequiredAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::SharedTrylockFunction: {
    const SharedTrylockFunctionAttr *SA = cast<SharedTrylockFunctionAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || SA->args_begin() != SA->args_end();
    setMoreChildren(MoreChildren);
    lastChild();
    dumpStmt(SA->getSuccessValue());
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (SharedTrylockFunctionAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::SingleInheritance: {
    break;
  }
  case attr::StdCall: {
    break;
  }
  case attr::TLSModel: {
    const TLSModelAttr *SA = cast<TLSModelAttr>(A);
    OS << " \"" << SA->getModel() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::ThisCall: {
    break;
  }
  case attr::TransparentUnion: {
    break;
  }
  case attr::TypeTagForDatatype: {
    const TypeTagForDatatypeAttr *SA = cast<TypeTagForDatatypeAttr>(A);
    OS << " " << SA->getArgumentKind()->getName();
    OS << " " << SA->getMatchingCType().getAsString();
    if (SA->getLayoutCompatible()) OS << " LayoutCompatible";
    if (SA->getMustBeNull()) OS << " MustBeNull";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::TypeVisibility: {
    const TypeVisibilityAttr *SA = cast<TypeVisibilityAttr>(A);
    switch(SA->getVisibility()) {
    case TypeVisibilityAttr::Default:
      OS << " Default";
      break;
    case TypeVisibilityAttr::Hidden:
      OS << " Hidden";
      break;
    case TypeVisibilityAttr::Protected:
      OS << " Protected";
      break;
    }
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::Unavailable: {
    const UnavailableAttr *SA = cast<UnavailableAttr>(A);
    OS << " \"" << SA->getMessage() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::UnlockFunction: {
    const UnlockFunctionAttr *SA = cast<UnlockFunctionAttr>(A);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    for (UnlockFunctionAttr::args_iterator I = SA->args_begin(), E = SA->args_end(); I != E; ++I) {
      if (I + 1 == E)
        lastChild();
      dumpStmt(*I);
    }
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::UnspecifiedInheritance: {
    break;
  }
  case attr::Unused: {
    break;
  }
  case attr::Used: {
    break;
  }
  case attr::Uuid: {
    const UuidAttr *SA = cast<UuidAttr>(A);
    OS << " \"" << SA->getGuid() << "\"";
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::VecReturn: {
    break;
  }
  case attr::VecTypeHint: {
    const VecTypeHintAttr *SA = cast<VecTypeHintAttr>(A);
    OS << " " << SA->getTypeHint().getAsString();
    OS << " ";
    SA->getTypeLoc().print(OS, *SM);
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::VirtualInheritance: {
    break;
  }
  case attr::Visibility: {
    const VisibilityAttr *SA = cast<VisibilityAttr>(A);
    switch(SA->getVisibility()) {
    case VisibilityAttr::Default:
      OS << " Default";
      break;
    case VisibilityAttr::Hidden:
      OS << " Hidden";
      break;
    case VisibilityAttr::Protected:
      OS << " Protected";
      break;
    }
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::WarnUnusedResult: {
    break;
  }
  case attr::Weak: {
    break;
  }
  case attr::WeakImport: {
    break;
  }
  case attr::WeakRef: {
    break;
  }
  case attr::Win64: {
    break;
  }
  case attr::WorkGroupSizeHint: {
    const WorkGroupSizeHintAttr *SA = cast<WorkGroupSizeHintAttr>(A);
    OS << " " << SA->getXDim();
    OS << " " << SA->getYDim();
    OS << " " << SA->getZDim();
    bool OldMoreChildren = hasMoreChildren();
    bool MoreChildren = OldMoreChildren;
    MoreChildren = OldMoreChildren || false || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren || false;
    setMoreChildren(MoreChildren);
    MoreChildren = OldMoreChildren;
    setMoreChildren(MoreChildren);
    setMoreChildren(OldMoreChildren);
    break;
  }
  case attr::X86ForceAlignArgPointer: {
    break;
  }
  }
