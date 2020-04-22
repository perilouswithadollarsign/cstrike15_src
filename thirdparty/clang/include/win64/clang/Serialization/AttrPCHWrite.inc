/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute serialization code                                                *|
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
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (AcquiredAfterAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::AcquiredBefore: {
    const AcquiredBeforeAttr *SA = cast<AcquiredBeforeAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (AcquiredBeforeAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::Alias: {
    const AliasAttr *SA = cast<AliasAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getAliasee(), Record);
    break;
  }
  case attr::AlignMac68k: {
    const AlignMac68kAttr *SA = cast<AlignMac68kAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Aligned: {
    const AlignedAttr *SA = cast<AlignedAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->isAlignmentExpr());
    if (SA->isAlignmentExpr())
      AddStmt(SA->getAlignmentExpr());
    else
      AddTypeSourceInfo(SA->getAlignmentType(), Record);
    break;
  }
  case attr::AllocSize: {
    const AllocSizeAttr *SA = cast<AllocSizeAttr>(A);
    Record.push_back(SA->args_size());
    for (AllocSizeAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      Record.push_back((*i));
    break;
  }
  case attr::AlwaysInline: {
    const AlwaysInlineAttr *SA = cast<AlwaysInlineAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::AnalyzerNoReturn: {
    const AnalyzerNoReturnAttr *SA = cast<AnalyzerNoReturnAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Annotate: {
    const AnnotateAttr *SA = cast<AnnotateAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getAnnotation(), Record);
    break;
  }
  case attr::ArcWeakrefUnavailable: {
    const ArcWeakrefUnavailableAttr *SA = cast<ArcWeakrefUnavailableAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ArgumentWithTypeTag: {
    const ArgumentWithTypeTagAttr *SA = cast<ArgumentWithTypeTagAttr>(A);
    Record.push_back(SA->isInherited());
    AddIdentifierRef(SA->getArgumentKind(), Record);
    Record.push_back(SA->getArgumentIdx());
    Record.push_back(SA->getTypeTagIdx());
    Record.push_back(SA->getIsPointer());
    break;
  }
  case attr::AsmLabel: {
    const AsmLabelAttr *SA = cast<AsmLabelAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getLabel(), Record);
    break;
  }
  case attr::Availability: {
    const AvailabilityAttr *SA = cast<AvailabilityAttr>(A);
    Record.push_back(SA->isInherited());
    AddIdentifierRef(SA->getPlatform(), Record);
    AddVersionTuple(SA->getIntroduced(), Record);
    AddVersionTuple(SA->getDeprecated(), Record);
    AddVersionTuple(SA->getObsoleted(), Record);
    Record.push_back(SA->getUnavailable());
    AddString(SA->getMessage(), Record);
    break;
  }
  case attr::Blocks: {
    const BlocksAttr *SA = cast<BlocksAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getType());
    break;
  }
  case attr::C11NoReturn: {
    const C11NoReturnAttr *SA = cast<C11NoReturnAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CDecl: {
    const CDeclAttr *SA = cast<CDeclAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CFAuditedTransfer: {
    const CFAuditedTransferAttr *SA = cast<CFAuditedTransferAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CFConsumed: {
    const CFConsumedAttr *SA = cast<CFConsumedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CFReturnsNotRetained: {
    const CFReturnsNotRetainedAttr *SA = cast<CFReturnsNotRetainedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CFReturnsRetained: {
    const CFReturnsRetainedAttr *SA = cast<CFReturnsRetainedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CFUnknownTransfer: {
    const CFUnknownTransferAttr *SA = cast<CFUnknownTransferAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CUDAConstant: {
    const CUDAConstantAttr *SA = cast<CUDAConstantAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CUDADevice: {
    const CUDADeviceAttr *SA = cast<CUDADeviceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CUDAGlobal: {
    const CUDAGlobalAttr *SA = cast<CUDAGlobalAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CUDAHost: {
    const CUDAHostAttr *SA = cast<CUDAHostAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CUDALaunchBounds: {
    const CUDALaunchBoundsAttr *SA = cast<CUDALaunchBoundsAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getMaxThreads());
    Record.push_back(SA->getMinBlocks());
    break;
  }
  case attr::CUDAShared: {
    const CUDASharedAttr *SA = cast<CUDASharedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CXX11NoReturn: {
    const CXX11NoReturnAttr *SA = cast<CXX11NoReturnAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::CarriesDependency: {
    const CarriesDependencyAttr *SA = cast<CarriesDependencyAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Cleanup: {
    const CleanupAttr *SA = cast<CleanupAttr>(A);
    Record.push_back(SA->isInherited());
    AddDeclRef(SA->getFunctionDecl(), Record);
    break;
  }
  case attr::Cold: {
    const ColdAttr *SA = cast<ColdAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Common: {
    const CommonAttr *SA = cast<CommonAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Const: {
    const ConstAttr *SA = cast<ConstAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Constructor: {
    const ConstructorAttr *SA = cast<ConstructorAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getPriority());
    break;
  }
  case attr::DLLExport: {
    const DLLExportAttr *SA = cast<DLLExportAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::DLLImport: {
    const DLLImportAttr *SA = cast<DLLImportAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Deprecated: {
    const DeprecatedAttr *SA = cast<DeprecatedAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getMessage(), Record);
    break;
  }
  case attr::Destructor: {
    const DestructorAttr *SA = cast<DestructorAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getPriority());
    break;
  }
  case attr::Endian: {
    const EndianAttr *SA = cast<EndianAttr>(A);
    Record.push_back(SA->isInherited());
    AddIdentifierRef(SA->getPlatform(), Record);
    break;
  }
  case attr::ExclusiveLockFunction: {
    const ExclusiveLockFunctionAttr *SA = cast<ExclusiveLockFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (ExclusiveLockFunctionAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::ExclusiveLocksRequired: {
    const ExclusiveLocksRequiredAttr *SA = cast<ExclusiveLocksRequiredAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (ExclusiveLocksRequiredAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::ExclusiveTrylockFunction: {
    const ExclusiveTrylockFunctionAttr *SA = cast<ExclusiveTrylockFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    AddStmt(SA->getSuccessValue());
    Record.push_back(SA->args_size());
    for (ExclusiveTrylockFunctionAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::FallThrough: {
    break;
  }
  case attr::FastCall: {
    const FastCallAttr *SA = cast<FastCallAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Final: {
    const FinalAttr *SA = cast<FinalAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ForceInline: {
    const ForceInlineAttr *SA = cast<ForceInlineAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Format: {
    const FormatAttr *SA = cast<FormatAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getType(), Record);
    Record.push_back(SA->getFormatIdx());
    Record.push_back(SA->getFirstArg());
    break;
  }
  case attr::FormatArg: {
    const FormatArgAttr *SA = cast<FormatArgAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getFormatIdx());
    break;
  }
  case attr::GNUInline: {
    const GNUInlineAttr *SA = cast<GNUInlineAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::GuardedBy: {
    const GuardedByAttr *SA = cast<GuardedByAttr>(A);
    Record.push_back(SA->isInherited());
    AddStmt(SA->getArg());
    break;
  }
  case attr::GuardedVar: {
    const GuardedVarAttr *SA = cast<GuardedVarAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Hot: {
    const HotAttr *SA = cast<HotAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::IBAction: {
    const IBActionAttr *SA = cast<IBActionAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::IBOutlet: {
    const IBOutletAttr *SA = cast<IBOutletAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::IBOutletCollection: {
    const IBOutletCollectionAttr *SA = cast<IBOutletCollectionAttr>(A);
    Record.push_back(SA->isInherited());
    AddTypeRef(SA->getInterface(), Record);
    AddSourceLocation(SA->getInterfaceLoc(), Record);
    break;
  }
  case attr::InitPriority: {
    const InitPriorityAttr *SA = cast<InitPriorityAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getPriority());
    break;
  }
  case attr::IntelOclBicc: {
    const IntelOclBiccAttr *SA = cast<IntelOclBiccAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::LockReturned: {
    const LockReturnedAttr *SA = cast<LockReturnedAttr>(A);
    Record.push_back(SA->isInherited());
    AddStmt(SA->getArg());
    break;
  }
  case attr::Lockable: {
    const LockableAttr *SA = cast<LockableAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::LocksExcluded: {
    const LocksExcludedAttr *SA = cast<LocksExcludedAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (LocksExcludedAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::MBlazeInterruptHandler: {
    const MBlazeInterruptHandlerAttr *SA = cast<MBlazeInterruptHandlerAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MBlazeSaveVolatiles: {
    const MBlazeSaveVolatilesAttr *SA = cast<MBlazeSaveVolatilesAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MSP430Interrupt: {
    const MSP430InterruptAttr *SA = cast<MSP430InterruptAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getNumber());
    break;
  }
  case attr::Malloc: {
    const MallocAttr *SA = cast<MallocAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MaxFieldAlignment: {
    const MaxFieldAlignmentAttr *SA = cast<MaxFieldAlignmentAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getAlignment());
    break;
  }
  case attr::MayAlias: {
    const MayAliasAttr *SA = cast<MayAliasAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MinSize: {
    const MinSizeAttr *SA = cast<MinSizeAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Mips16: {
    const Mips16Attr *SA = cast<Mips16Attr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MsProperty: {
    break;
  }
  case attr::MsStruct: {
    const MsStructAttr *SA = cast<MsStructAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::MultipleInheritance: {
    const MultipleInheritanceAttr *SA = cast<MultipleInheritanceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NSBridged: {
    const NSBridgedAttr *SA = cast<NSBridgedAttr>(A);
    Record.push_back(SA->isInherited());
    AddIdentifierRef(SA->getBridgedType(), Record);
    break;
  }
  case attr::NSConsumed: {
    const NSConsumedAttr *SA = cast<NSConsumedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NSConsumesSelf: {
    const NSConsumesSelfAttr *SA = cast<NSConsumesSelfAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NSReturnsAutoreleased: {
    const NSReturnsAutoreleasedAttr *SA = cast<NSReturnsAutoreleasedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NSReturnsNotRetained: {
    const NSReturnsNotRetainedAttr *SA = cast<NSReturnsNotRetainedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NSReturnsRetained: {
    const NSReturnsRetainedAttr *SA = cast<NSReturnsRetainedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Naked: {
    const NakedAttr *SA = cast<NakedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoCommon: {
    const NoCommonAttr *SA = cast<NoCommonAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoDebug: {
    const NoDebugAttr *SA = cast<NoDebugAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoInline: {
    const NoInlineAttr *SA = cast<NoInlineAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoInstrumentFunction: {
    const NoInstrumentFunctionAttr *SA = cast<NoInstrumentFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoMips16: {
    const NoMips16Attr *SA = cast<NoMips16Attr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoReturn: {
    const NoReturnAttr *SA = cast<NoReturnAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoSanitizeAddress: {
    const NoSanitizeAddressAttr *SA = cast<NoSanitizeAddressAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoSanitizeMemory: {
    const NoSanitizeMemoryAttr *SA = cast<NoSanitizeMemoryAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoSanitizeThread: {
    const NoSanitizeThreadAttr *SA = cast<NoSanitizeThreadAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoThreadSafetyAnalysis: {
    const NoThreadSafetyAnalysisAttr *SA = cast<NoThreadSafetyAnalysisAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NoThrow: {
    const NoThrowAttr *SA = cast<NoThrowAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::NonNull: {
    const NonNullAttr *SA = cast<NonNullAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (NonNullAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      Record.push_back((*i));
    break;
  }
  case attr::ObjCException: {
    const ObjCExceptionAttr *SA = cast<ObjCExceptionAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ObjCMethodFamily: {
    const ObjCMethodFamilyAttr *SA = cast<ObjCMethodFamilyAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getFamily());
    break;
  }
  case attr::ObjCNSObject: {
    const ObjCNSObjectAttr *SA = cast<ObjCNSObjectAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ObjCPreciseLifetime: {
    break;
  }
  case attr::ObjCRequiresPropertyDefs: {
    const ObjCRequiresPropertyDefsAttr *SA = cast<ObjCRequiresPropertyDefsAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ObjCRequiresSuper: {
    const ObjCRequiresSuperAttr *SA = cast<ObjCRequiresSuperAttr>(A);
    Record.push_back(SA->isInherited());
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
    Record.push_back(SA->getAccess());
    break;
  }
  case attr::OpenCLKernel: {
    break;
  }
  case attr::Overloadable: {
    break;
  }
  case attr::Override: {
    const OverrideAttr *SA = cast<OverrideAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Ownership: {
    const OwnershipAttr *SA = cast<OwnershipAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getOwnKind());
    AddString(SA->getModule(), Record);
    Record.push_back(SA->args_size());
    for (OwnershipAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      Record.push_back((*i));
    break;
  }
  case attr::Packed: {
    const PackedAttr *SA = cast<PackedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Pascal: {
    const PascalAttr *SA = cast<PascalAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Pcs: {
    const PcsAttr *SA = cast<PcsAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getPCS());
    break;
  }
  case attr::PnaclCall: {
    const PnaclCallAttr *SA = cast<PnaclCallAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::PtGuardedBy: {
    const PtGuardedByAttr *SA = cast<PtGuardedByAttr>(A);
    Record.push_back(SA->isInherited());
    AddStmt(SA->getArg());
    break;
  }
  case attr::PtGuardedVar: {
    const PtGuardedVarAttr *SA = cast<PtGuardedVarAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Ptr32: {
    const Ptr32Attr *SA = cast<Ptr32Attr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Ptr64: {
    const Ptr64Attr *SA = cast<Ptr64Attr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Pure: {
    const PureAttr *SA = cast<PureAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Regparm: {
    const RegparmAttr *SA = cast<RegparmAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getNumParams());
    break;
  }
  case attr::ReqdWorkGroupSize: {
    const ReqdWorkGroupSizeAttr *SA = cast<ReqdWorkGroupSizeAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getXDim());
    Record.push_back(SA->getYDim());
    Record.push_back(SA->getZDim());
    break;
  }
  case attr::ReturnsTwice: {
    const ReturnsTwiceAttr *SA = cast<ReturnsTwiceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::ScopedLockable: {
    const ScopedLockableAttr *SA = cast<ScopedLockableAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Section: {
    const SectionAttr *SA = cast<SectionAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getName(), Record);
    break;
  }
  case attr::Sentinel: {
    const SentinelAttr *SA = cast<SentinelAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getSentinel());
    Record.push_back(SA->getNullPos());
    break;
  }
  case attr::SharedLockFunction: {
    const SharedLockFunctionAttr *SA = cast<SharedLockFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (SharedLockFunctionAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::SharedLocksRequired: {
    const SharedLocksRequiredAttr *SA = cast<SharedLocksRequiredAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (SharedLocksRequiredAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::SharedTrylockFunction: {
    const SharedTrylockFunctionAttr *SA = cast<SharedTrylockFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    AddStmt(SA->getSuccessValue());
    Record.push_back(SA->args_size());
    for (SharedTrylockFunctionAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::SingleInheritance: {
    const SingleInheritanceAttr *SA = cast<SingleInheritanceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::StdCall: {
    const StdCallAttr *SA = cast<StdCallAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::TLSModel: {
    const TLSModelAttr *SA = cast<TLSModelAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getModel(), Record);
    break;
  }
  case attr::ThisCall: {
    const ThisCallAttr *SA = cast<ThisCallAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::TransparentUnion: {
    const TransparentUnionAttr *SA = cast<TransparentUnionAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::TypeTagForDatatype: {
    const TypeTagForDatatypeAttr *SA = cast<TypeTagForDatatypeAttr>(A);
    Record.push_back(SA->isInherited());
    AddIdentifierRef(SA->getArgumentKind(), Record);
    AddTypeRef(SA->getMatchingCType(), Record);
    Record.push_back(SA->getLayoutCompatible());
    Record.push_back(SA->getMustBeNull());
    break;
  }
  case attr::TypeVisibility: {
    const TypeVisibilityAttr *SA = cast<TypeVisibilityAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getVisibility());
    break;
  }
  case attr::Unavailable: {
    const UnavailableAttr *SA = cast<UnavailableAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getMessage(), Record);
    break;
  }
  case attr::UnlockFunction: {
    const UnlockFunctionAttr *SA = cast<UnlockFunctionAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->args_size());
    for (UnlockFunctionAttr::args_iterator i = SA->args_begin(), e = SA->args_end(); i != e; ++i)
      AddStmt((*i));
    break;
  }
  case attr::UnspecifiedInheritance: {
    const UnspecifiedInheritanceAttr *SA = cast<UnspecifiedInheritanceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Unused: {
    const UnusedAttr *SA = cast<UnusedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Used: {
    const UsedAttr *SA = cast<UsedAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Uuid: {
    const UuidAttr *SA = cast<UuidAttr>(A);
    Record.push_back(SA->isInherited());
    AddString(SA->getGuid(), Record);
    break;
  }
  case attr::VecReturn: {
    const VecReturnAttr *SA = cast<VecReturnAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::VecTypeHint: {
    const VecTypeHintAttr *SA = cast<VecTypeHintAttr>(A);
    Record.push_back(SA->isInherited());
    AddTypeRef(SA->getTypeHint(), Record);
    AddSourceLocation(SA->getTypeLoc(), Record);
    break;
  }
  case attr::VirtualInheritance: {
    const VirtualInheritanceAttr *SA = cast<VirtualInheritanceAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Visibility: {
    const VisibilityAttr *SA = cast<VisibilityAttr>(A);
    Record.push_back(SA->isInherited());
Record.push_back(SA->getVisibility());
    break;
  }
  case attr::WarnUnusedResult: {
    const WarnUnusedResultAttr *SA = cast<WarnUnusedResultAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Weak: {
    const WeakAttr *SA = cast<WeakAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::WeakImport: {
    const WeakImportAttr *SA = cast<WeakImportAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::WeakRef: {
    const WeakRefAttr *SA = cast<WeakRefAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::Win64: {
    const Win64Attr *SA = cast<Win64Attr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  case attr::WorkGroupSizeHint: {
    const WorkGroupSizeHintAttr *SA = cast<WorkGroupSizeHintAttr>(A);
    Record.push_back(SA->isInherited());
    Record.push_back(SA->getXDim());
    Record.push_back(SA->getYDim());
    Record.push_back(SA->getZDim());
    break;
  }
  case attr::X86ForceAlignArgPointer: {
    const X86ForceAlignArgPointerAttr *SA = cast<X86ForceAlignArgPointerAttr>(A);
    Record.push_back(SA->isInherited());
    break;
  }
  }
