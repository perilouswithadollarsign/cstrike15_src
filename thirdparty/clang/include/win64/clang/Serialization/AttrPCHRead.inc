/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute deserialization code                                              *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

  switch (Kind) {
  default:
    assert(0 && "Unknown attribute!");
    break;
  case attr::AcquiredAfter: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) AcquiredAfterAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::AcquiredBefore: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) AcquiredBeforeAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Alias: {
    bool isInherited = Record[Idx++];
    std::string aliasee= ReadString(Record, Idx);
    New = new (Context) AliasAttr(Range, Context, aliasee);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::AlignMac68k: {
    bool isInherited = Record[Idx++];
    New = new (Context) AlignMac68kAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Aligned: {
    bool isInherited = Record[Idx++];
    bool isalignmentExpr = Record[Idx++];
    void *alignmentPtr;
    if (isalignmentExpr)
      alignmentPtr = ReadExpr(F);
    else
      alignmentPtr = GetTypeSourceInfo(F, Record, Idx);
    New = new (Context) AlignedAttr(Range, Context, isalignmentExpr, alignmentPtr);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::AllocSize: {
  unsigned argsSize = Record[Idx++];
  SmallVector<unsigned, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(Record[Idx++]);
    New = new (Context) AllocSizeAttr(Range, Context, args.data(), argsSize);
    break;
  }
  case attr::AlwaysInline: {
    bool isInherited = Record[Idx++];
    New = new (Context) AlwaysInlineAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::AnalyzerNoReturn: {
    bool isInherited = Record[Idx++];
    New = new (Context) AnalyzerNoReturnAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Annotate: {
    bool isInherited = Record[Idx++];
    std::string annotation= ReadString(Record, Idx);
    New = new (Context) AnnotateAttr(Range, Context, annotation);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ArcWeakrefUnavailable: {
    bool isInherited = Record[Idx++];
    New = new (Context) ArcWeakrefUnavailableAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ArgumentWithTypeTag: {
    bool isInherited = Record[Idx++];
    IdentifierInfo * argumentKind = GetIdentifierInfo(F, Record, Idx);
    unsigned argumentIdx = Record[Idx++];
    unsigned typeTagIdx = Record[Idx++];
    bool isPointer = Record[Idx++];
    New = new (Context) ArgumentWithTypeTagAttr(Range, Context, argumentKind, argumentIdx, typeTagIdx, isPointer);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::AsmLabel: {
    bool isInherited = Record[Idx++];
    std::string label= ReadString(Record, Idx);
    New = new (Context) AsmLabelAttr(Range, Context, label);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Availability: {
    bool isInherited = Record[Idx++];
    IdentifierInfo * platform = GetIdentifierInfo(F, Record, Idx);
    VersionTuple introduced= ReadVersionTuple(Record, Idx);
    VersionTuple deprecated= ReadVersionTuple(Record, Idx);
    VersionTuple obsoleted= ReadVersionTuple(Record, Idx);
    bool unavailable = Record[Idx++];
    std::string message= ReadString(Record, Idx);
    New = new (Context) AvailabilityAttr(Range, Context, platform, introduced, deprecated, obsoleted, unavailable, message);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Blocks: {
    bool isInherited = Record[Idx++];
    BlocksAttr::BlockType type(static_cast<BlocksAttr::BlockType>(Record[Idx++]));
    New = new (Context) BlocksAttr(Range, Context, type);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::C11NoReturn: {
    bool isInherited = Record[Idx++];
    New = new (Context) C11NoReturnAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CDecl: {
    bool isInherited = Record[Idx++];
    New = new (Context) CDeclAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CFAuditedTransfer: {
    bool isInherited = Record[Idx++];
    New = new (Context) CFAuditedTransferAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CFConsumed: {
    bool isInherited = Record[Idx++];
    New = new (Context) CFConsumedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CFReturnsNotRetained: {
    bool isInherited = Record[Idx++];
    New = new (Context) CFReturnsNotRetainedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CFReturnsRetained: {
    bool isInherited = Record[Idx++];
    New = new (Context) CFReturnsRetainedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CFUnknownTransfer: {
    bool isInherited = Record[Idx++];
    New = new (Context) CFUnknownTransferAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDAConstant: {
    bool isInherited = Record[Idx++];
    New = new (Context) CUDAConstantAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDADevice: {
    bool isInherited = Record[Idx++];
    New = new (Context) CUDADeviceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDAGlobal: {
    bool isInherited = Record[Idx++];
    New = new (Context) CUDAGlobalAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDAHost: {
    bool isInherited = Record[Idx++];
    New = new (Context) CUDAHostAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDALaunchBounds: {
    bool isInherited = Record[Idx++];
    int maxThreads = Record[Idx++];
    int minBlocks = Record[Idx++];
    New = new (Context) CUDALaunchBoundsAttr(Range, Context, maxThreads, minBlocks);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CUDAShared: {
    bool isInherited = Record[Idx++];
    New = new (Context) CUDASharedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CXX11NoReturn: {
    bool isInherited = Record[Idx++];
    New = new (Context) CXX11NoReturnAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::CarriesDependency: {
    bool isInherited = Record[Idx++];
    New = new (Context) CarriesDependencyAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Cleanup: {
    bool isInherited = Record[Idx++];
    FunctionDecl * functionDecl = GetLocalDeclAs<FunctionDecl >(F, Record[Idx++]);
    New = new (Context) CleanupAttr(Range, Context, functionDecl);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Cold: {
    bool isInherited = Record[Idx++];
    New = new (Context) ColdAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Common: {
    bool isInherited = Record[Idx++];
    New = new (Context) CommonAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Const: {
    bool isInherited = Record[Idx++];
    New = new (Context) ConstAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Constructor: {
    bool isInherited = Record[Idx++];
    int priority = Record[Idx++];
    New = new (Context) ConstructorAttr(Range, Context, priority);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::DLLExport: {
    bool isInherited = Record[Idx++];
    New = new (Context) DLLExportAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::DLLImport: {
    bool isInherited = Record[Idx++];
    New = new (Context) DLLImportAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Deprecated: {
    bool isInherited = Record[Idx++];
    std::string message= ReadString(Record, Idx);
    New = new (Context) DeprecatedAttr(Range, Context, message);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Destructor: {
    bool isInherited = Record[Idx++];
    int priority = Record[Idx++];
    New = new (Context) DestructorAttr(Range, Context, priority);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Endian: {
    bool isInherited = Record[Idx++];
    IdentifierInfo * platform = GetIdentifierInfo(F, Record, Idx);
    New = new (Context) EndianAttr(Range, Context, platform);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ExclusiveLockFunction: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) ExclusiveLockFunctionAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ExclusiveLocksRequired: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) ExclusiveLocksRequiredAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ExclusiveTrylockFunction: {
    bool isInherited = Record[Idx++];
    Expr * successValue = ReadExpr(F);
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) ExclusiveTrylockFunctionAttr(Range, Context, successValue, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::FallThrough: {
    New = new (Context) FallThroughAttr(Range, Context);
    break;
  }
  case attr::FastCall: {
    bool isInherited = Record[Idx++];
    New = new (Context) FastCallAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Final: {
    bool isInherited = Record[Idx++];
    New = new (Context) FinalAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ForceInline: {
    bool isInherited = Record[Idx++];
    New = new (Context) ForceInlineAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Format: {
    bool isInherited = Record[Idx++];
    std::string type= ReadString(Record, Idx);
    int formatIdx = Record[Idx++];
    int firstArg = Record[Idx++];
    New = new (Context) FormatAttr(Range, Context, type, formatIdx, firstArg);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::FormatArg: {
    bool isInherited = Record[Idx++];
    int formatIdx = Record[Idx++];
    New = new (Context) FormatArgAttr(Range, Context, formatIdx);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::GNUInline: {
    bool isInherited = Record[Idx++];
    New = new (Context) GNUInlineAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::GuardedBy: {
    bool isInherited = Record[Idx++];
    Expr * arg = ReadExpr(F);
    New = new (Context) GuardedByAttr(Range, Context, arg);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::GuardedVar: {
    bool isInherited = Record[Idx++];
    New = new (Context) GuardedVarAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Hot: {
    bool isInherited = Record[Idx++];
    New = new (Context) HotAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::IBAction: {
    bool isInherited = Record[Idx++];
    New = new (Context) IBActionAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::IBOutlet: {
    bool isInherited = Record[Idx++];
    New = new (Context) IBOutletAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::IBOutletCollection: {
    bool isInherited = Record[Idx++];
    QualType interface = getLocalType(F, Record[Idx++]);
    SourceLocation interfaceLoc = ReadSourceLocation(F, Record, Idx);
    New = new (Context) IBOutletCollectionAttr(Range, Context, interface, interfaceLoc);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::InitPriority: {
    bool isInherited = Record[Idx++];
    unsigned priority = Record[Idx++];
    New = new (Context) InitPriorityAttr(Range, Context, priority);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::IntelOclBicc: {
    bool isInherited = Record[Idx++];
    New = new (Context) IntelOclBiccAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::LockReturned: {
    bool isInherited = Record[Idx++];
    Expr * arg = ReadExpr(F);
    New = new (Context) LockReturnedAttr(Range, Context, arg);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Lockable: {
    bool isInherited = Record[Idx++];
    New = new (Context) LockableAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::LocksExcluded: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) LocksExcludedAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MBlazeInterruptHandler: {
    bool isInherited = Record[Idx++];
    New = new (Context) MBlazeInterruptHandlerAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MBlazeSaveVolatiles: {
    bool isInherited = Record[Idx++];
    New = new (Context) MBlazeSaveVolatilesAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MSP430Interrupt: {
    bool isInherited = Record[Idx++];
    unsigned number = Record[Idx++];
    New = new (Context) MSP430InterruptAttr(Range, Context, number);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Malloc: {
    bool isInherited = Record[Idx++];
    New = new (Context) MallocAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MaxFieldAlignment: {
    bool isInherited = Record[Idx++];
    unsigned alignment = Record[Idx++];
    New = new (Context) MaxFieldAlignmentAttr(Range, Context, alignment);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MayAlias: {
    bool isInherited = Record[Idx++];
    New = new (Context) MayAliasAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MinSize: {
    bool isInherited = Record[Idx++];
    New = new (Context) MinSizeAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Mips16: {
    bool isInherited = Record[Idx++];
    New = new (Context) Mips16Attr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MsProperty: {
    New = new (Context) MsPropertyAttr(Range, Context);
    break;
  }
  case attr::MsStruct: {
    bool isInherited = Record[Idx++];
    New = new (Context) MsStructAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::MultipleInheritance: {
    bool isInherited = Record[Idx++];
    New = new (Context) MultipleInheritanceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSBridged: {
    bool isInherited = Record[Idx++];
    IdentifierInfo * bridgedType = GetIdentifierInfo(F, Record, Idx);
    New = new (Context) NSBridgedAttr(Range, Context, bridgedType);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSConsumed: {
    bool isInherited = Record[Idx++];
    New = new (Context) NSConsumedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSConsumesSelf: {
    bool isInherited = Record[Idx++];
    New = new (Context) NSConsumesSelfAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSReturnsAutoreleased: {
    bool isInherited = Record[Idx++];
    New = new (Context) NSReturnsAutoreleasedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSReturnsNotRetained: {
    bool isInherited = Record[Idx++];
    New = new (Context) NSReturnsNotRetainedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NSReturnsRetained: {
    bool isInherited = Record[Idx++];
    New = new (Context) NSReturnsRetainedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Naked: {
    bool isInherited = Record[Idx++];
    New = new (Context) NakedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoCommon: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoCommonAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoDebug: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoDebugAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoInline: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoInlineAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoInstrumentFunction: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoInstrumentFunctionAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoMips16: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoMips16Attr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoReturn: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoReturnAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoSanitizeAddress: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoSanitizeAddressAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoSanitizeMemory: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoSanitizeMemoryAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoSanitizeThread: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoSanitizeThreadAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoThreadSafetyAnalysis: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoThreadSafetyAnalysisAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NoThrow: {
    bool isInherited = Record[Idx++];
    New = new (Context) NoThrowAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::NonNull: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<unsigned, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(Record[Idx++]);
    New = new (Context) NonNullAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCException: {
    bool isInherited = Record[Idx++];
    New = new (Context) ObjCExceptionAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCMethodFamily: {
    bool isInherited = Record[Idx++];
    ObjCMethodFamilyAttr::FamilyKind family(static_cast<ObjCMethodFamilyAttr::FamilyKind>(Record[Idx++]));
    New = new (Context) ObjCMethodFamilyAttr(Range, Context, family);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCNSObject: {
    bool isInherited = Record[Idx++];
    New = new (Context) ObjCNSObjectAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCPreciseLifetime: {
    New = new (Context) ObjCPreciseLifetimeAttr(Range, Context);
    break;
  }
  case attr::ObjCRequiresPropertyDefs: {
    bool isInherited = Record[Idx++];
    New = new (Context) ObjCRequiresPropertyDefsAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCRequiresSuper: {
    bool isInherited = Record[Idx++];
    New = new (Context) ObjCRequiresSuperAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ObjCReturnsInnerPointer: {
    New = new (Context) ObjCReturnsInnerPointerAttr(Range, Context);
    break;
  }
  case attr::ObjCRootClass: {
    New = new (Context) ObjCRootClassAttr(Range, Context);
    break;
  }
  case attr::OpenCLImageAccess: {
    int access = Record[Idx++];
    New = new (Context) OpenCLImageAccessAttr(Range, Context, access);
    break;
  }
  case attr::OpenCLKernel: {
    New = new (Context) OpenCLKernelAttr(Range, Context);
    break;
  }
  case attr::Overloadable: {
    New = new (Context) OverloadableAttr(Range, Context);
    break;
  }
  case attr::Override: {
    bool isInherited = Record[Idx++];
    New = new (Context) OverrideAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Ownership: {
    bool isInherited = Record[Idx++];
    OwnershipAttr::OwnershipKind ownKind(static_cast<OwnershipAttr::OwnershipKind>(Record[Idx++]));
    std::string module= ReadString(Record, Idx);
  unsigned argsSize = Record[Idx++];
  SmallVector<unsigned, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(Record[Idx++]);
    New = new (Context) OwnershipAttr(Range, Context, ownKind, module, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Packed: {
    bool isInherited = Record[Idx++];
    New = new (Context) PackedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Pascal: {
    bool isInherited = Record[Idx++];
    New = new (Context) PascalAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Pcs: {
    bool isInherited = Record[Idx++];
    PcsAttr::PCSType pCS(static_cast<PcsAttr::PCSType>(Record[Idx++]));
    New = new (Context) PcsAttr(Range, Context, pCS);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::PnaclCall: {
    bool isInherited = Record[Idx++];
    New = new (Context) PnaclCallAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::PtGuardedBy: {
    bool isInherited = Record[Idx++];
    Expr * arg = ReadExpr(F);
    New = new (Context) PtGuardedByAttr(Range, Context, arg);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::PtGuardedVar: {
    bool isInherited = Record[Idx++];
    New = new (Context) PtGuardedVarAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Ptr32: {
    bool isInherited = Record[Idx++];
    New = new (Context) Ptr32Attr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Ptr64: {
    bool isInherited = Record[Idx++];
    New = new (Context) Ptr64Attr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Pure: {
    bool isInherited = Record[Idx++];
    New = new (Context) PureAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Regparm: {
    bool isInherited = Record[Idx++];
    unsigned numParams = Record[Idx++];
    New = new (Context) RegparmAttr(Range, Context, numParams);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ReqdWorkGroupSize: {
    bool isInherited = Record[Idx++];
    unsigned xDim = Record[Idx++];
    unsigned yDim = Record[Idx++];
    unsigned zDim = Record[Idx++];
    New = new (Context) ReqdWorkGroupSizeAttr(Range, Context, xDim, yDim, zDim);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ReturnsTwice: {
    bool isInherited = Record[Idx++];
    New = new (Context) ReturnsTwiceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ScopedLockable: {
    bool isInherited = Record[Idx++];
    New = new (Context) ScopedLockableAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Section: {
    bool isInherited = Record[Idx++];
    std::string name= ReadString(Record, Idx);
    New = new (Context) SectionAttr(Range, Context, name);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Sentinel: {
    bool isInherited = Record[Idx++];
    int sentinel = Record[Idx++];
    int nullPos = Record[Idx++];
    New = new (Context) SentinelAttr(Range, Context, sentinel, nullPos);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::SharedLockFunction: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) SharedLockFunctionAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::SharedLocksRequired: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) SharedLocksRequiredAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::SharedTrylockFunction: {
    bool isInherited = Record[Idx++];
    Expr * successValue = ReadExpr(F);
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) SharedTrylockFunctionAttr(Range, Context, successValue, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::SingleInheritance: {
    bool isInherited = Record[Idx++];
    New = new (Context) SingleInheritanceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::StdCall: {
    bool isInherited = Record[Idx++];
    New = new (Context) StdCallAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::TLSModel: {
    bool isInherited = Record[Idx++];
    std::string model= ReadString(Record, Idx);
    New = new (Context) TLSModelAttr(Range, Context, model);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::ThisCall: {
    bool isInherited = Record[Idx++];
    New = new (Context) ThisCallAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::TransparentUnion: {
    bool isInherited = Record[Idx++];
    New = new (Context) TransparentUnionAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::TypeTagForDatatype: {
    bool isInherited = Record[Idx++];
    IdentifierInfo * argumentKind = GetIdentifierInfo(F, Record, Idx);
    QualType matchingCType = getLocalType(F, Record[Idx++]);
    bool layoutCompatible = Record[Idx++];
    bool mustBeNull = Record[Idx++];
    New = new (Context) TypeTagForDatatypeAttr(Range, Context, argumentKind, matchingCType, layoutCompatible, mustBeNull);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::TypeVisibility: {
    bool isInherited = Record[Idx++];
    TypeVisibilityAttr::VisibilityType visibility(static_cast<TypeVisibilityAttr::VisibilityType>(Record[Idx++]));
    New = new (Context) TypeVisibilityAttr(Range, Context, visibility);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Unavailable: {
    bool isInherited = Record[Idx++];
    std::string message= ReadString(Record, Idx);
    New = new (Context) UnavailableAttr(Range, Context, message);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::UnlockFunction: {
    bool isInherited = Record[Idx++];
  unsigned argsSize = Record[Idx++];
  SmallVector<Expr *, 4> args;
  args.reserve(argsSize);
  for (unsigned i = argsSize; i; --i)
    args.push_back(ReadExpr(F));
    New = new (Context) UnlockFunctionAttr(Range, Context, args.data(), argsSize);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::UnspecifiedInheritance: {
    bool isInherited = Record[Idx++];
    New = new (Context) UnspecifiedInheritanceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Unused: {
    bool isInherited = Record[Idx++];
    New = new (Context) UnusedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Used: {
    bool isInherited = Record[Idx++];
    New = new (Context) UsedAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Uuid: {
    bool isInherited = Record[Idx++];
    std::string guid= ReadString(Record, Idx);
    New = new (Context) UuidAttr(Range, Context, guid);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::VecReturn: {
    bool isInherited = Record[Idx++];
    New = new (Context) VecReturnAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::VecTypeHint: {
    bool isInherited = Record[Idx++];
    QualType typeHint = getLocalType(F, Record[Idx++]);
    SourceLocation typeLoc = ReadSourceLocation(F, Record, Idx);
    New = new (Context) VecTypeHintAttr(Range, Context, typeHint, typeLoc);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::VirtualInheritance: {
    bool isInherited = Record[Idx++];
    New = new (Context) VirtualInheritanceAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Visibility: {
    bool isInherited = Record[Idx++];
    VisibilityAttr::VisibilityType visibility(static_cast<VisibilityAttr::VisibilityType>(Record[Idx++]));
    New = new (Context) VisibilityAttr(Range, Context, visibility);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::WarnUnusedResult: {
    bool isInherited = Record[Idx++];
    New = new (Context) WarnUnusedResultAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Weak: {
    bool isInherited = Record[Idx++];
    New = new (Context) WeakAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::WeakImport: {
    bool isInherited = Record[Idx++];
    New = new (Context) WeakImportAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::WeakRef: {
    bool isInherited = Record[Idx++];
    New = new (Context) WeakRefAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::Win64: {
    bool isInherited = Record[Idx++];
    New = new (Context) Win64Attr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::WorkGroupSizeHint: {
    bool isInherited = Record[Idx++];
    unsigned xDim = Record[Idx++];
    unsigned yDim = Record[Idx++];
    unsigned zDim = Record[Idx++];
    New = new (Context) WorkGroupSizeHintAttr(Range, Context, xDim, yDim, zDim);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  case attr::X86ForceAlignArgPointer: {
    bool isInherited = Record[Idx++];
    New = new (Context) X86ForceAlignArgPointerAttr(Range, Context);
    cast<InheritableAttr>(New)->setInherited(isInherited);
    break;
  }
  }
