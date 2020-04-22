/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute classes' member function definitions                              *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

AcquiredAfterAttr *AcquiredAfterAttr::clone(ASTContext &C) const {
  return new (C) AcquiredAfterAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void AcquiredAfterAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((acquired_after(";
  bool isFirst = true;
  for (AcquiredAfterAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

AcquiredBeforeAttr *AcquiredBeforeAttr::clone(ASTContext &C) const {
  return new (C) AcquiredBeforeAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void AcquiredBeforeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((acquired_before(";
  bool isFirst = true;
  for (AcquiredBeforeAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

AliasAttr *AliasAttr::clone(ASTContext &C) const {
  return new (C) AliasAttr(getLocation(), C, getAliasee(), getSpellingListIndex());
}

void AliasAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((alias(\"" << getAliasee() << "\")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::alias(\"" << getAliasee() << "\")]]";
    break;
  }
}
}

AlignMac68kAttr *AlignMac68kAttr::clone(ASTContext &C) const {
  return new (C) AlignMac68kAttr(getLocation(), C, getSpellingListIndex());
}

void AlignMac68kAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

bool AlignedAttr::isAlignmentDependent() const {
  if (isalignmentExpr)
    return alignmentExpr && (alignmentExpr->isValueDependent() || alignmentExpr->isTypeDependent());
  else
    return alignmentType->getType()->isDependentType();
}
unsigned AlignedAttr::getAlignment(ASTContext &Ctx) const {
  assert(!isAlignmentDependent());
  if (isalignmentExpr)
    return (alignmentExpr ? alignmentExpr->EvaluateKnownConstInt(Ctx).getZExtValue() : 16)* Ctx.getCharWidth();
  else
    return 0; // FIXME
}
AlignedAttr *AlignedAttr::clone(ASTContext &C) const {
  return new (C) AlignedAttr(getLocation(), C, isalignmentExpr, isalignmentExpr ? static_cast<void*>(alignmentExpr) : alignmentType, getSpellingListIndex());
}

void AlignedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((aligned(";
  alignmentExpr->printPretty(OS, 0, Policy);
  OS << ")))";
    break;
  }
  case 1 : {
    OS << " __declspec(align(";
  alignmentExpr->printPretty(OS, 0, Policy);
  OS << "))";
    break;
  }
  case 2 : {
    OS << " [[gnu::aligned(";
  alignmentExpr->printPretty(OS, 0, Policy);
  OS << ")]]";
    break;
  }
  case 3 : {
    OS << " alignas(";
  alignmentExpr->printPretty(OS, 0, Policy);
  OS << ")";
    break;
  }
  case 4 : {
    OS << " _Alignas(";
  alignmentExpr->printPretty(OS, 0, Policy);
  OS << ")";
    break;
  }
}
}

AllocSizeAttr *AllocSizeAttr::clone(ASTContext &C) const {
  return new (C) AllocSizeAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void AllocSizeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((alloc_size(";
  bool isFirst = true;
  for (AllocSizeAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::alloc_size(";
  bool isFirst = true;
  for (AllocSizeAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")]]";
    break;
  }
}
}

AlwaysInlineAttr *AlwaysInlineAttr::clone(ASTContext &C) const {
  return new (C) AlwaysInlineAttr(getLocation(), C, getSpellingListIndex());
}

void AlwaysInlineAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((always_inline))";
    break;
  }
  case 1 : {
    OS << " [[gnu::always_inline]]";
    break;
  }
}
}

AnalyzerNoReturnAttr *AnalyzerNoReturnAttr::clone(ASTContext &C) const {
  return new (C) AnalyzerNoReturnAttr(getLocation(), C, getSpellingListIndex());
}

void AnalyzerNoReturnAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((analyzer_noreturn))";
    break;
  }
}
}

AnnotateAttr *AnnotateAttr::clone(ASTContext &C) const {
  return new (C) AnnotateAttr(getLocation(), C, getAnnotation(), getSpellingListIndex());
}

void AnnotateAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((annotate(\"" << getAnnotation() << "\")))";
    break;
  }
}
}

ArcWeakrefUnavailableAttr *ArcWeakrefUnavailableAttr::clone(ASTContext &C) const {
  return new (C) ArcWeakrefUnavailableAttr(getLocation(), C, getSpellingListIndex());
}

void ArcWeakrefUnavailableAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_arc_weak_reference_unavailable))";
    break;
  }
}
}

ArgumentWithTypeTagAttr *ArgumentWithTypeTagAttr::clone(ASTContext &C) const {
  return new (C) ArgumentWithTypeTagAttr(getLocation(), C, argumentKind, argumentIdx, typeTagIdx, isPointer, getSpellingListIndex());
}

void ArgumentWithTypeTagAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((argument_with_type_tag(" << getArgumentKind()->getName() << ", " << getArgumentIdx() << ", " << getTypeTagIdx() << ", " << getIsPointer() << ")))";
    break;
  }
  case 1 : {
    OS << " __attribute__((pointer_with_type_tag(" << getArgumentKind()->getName() << ", " << getArgumentIdx() << ", " << getTypeTagIdx() << ", " << getIsPointer() << ")))";
    break;
  }
}
}

AsmLabelAttr *AsmLabelAttr::clone(ASTContext &C) const {
  return new (C) AsmLabelAttr(getLocation(), C, getLabel(), getSpellingListIndex());
}

void AsmLabelAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

AvailabilityAttr *AvailabilityAttr::clone(ASTContext &C) const {
  return new (C) AvailabilityAttr(getLocation(), C, platform, getIntroduced(), getDeprecated(), getObsoleted(), unavailable, getMessage(), getSpellingListIndex());
}

void AvailabilityAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((availability(" << getPlatform()->getName();
  if (!getIntroduced().empty()) OS << ", introduced=" << getIntroduced();
  if (!getDeprecated().empty()) OS << ", deprecated=" << getDeprecated();
  if (!getObsoleted().empty()) OS << ", obsoleted=" << getObsoleted();
  if (getUnavailable()) OS << ", unavailable";
  OS << ")))";
    break;
  }
}
}

BlocksAttr *BlocksAttr::clone(ASTContext &C) const {
  return new (C) BlocksAttr(getLocation(), C, type, getSpellingListIndex());
}

void BlocksAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((blocks(" << getType() << ")))";
    break;
  }
}
}

C11NoReturnAttr *C11NoReturnAttr::clone(ASTContext &C) const {
  return new (C) C11NoReturnAttr(getLocation(), C, getSpellingListIndex());
}

void C11NoReturnAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " _Noreturn";
    break;
  }
}
}

CDeclAttr *CDeclAttr::clone(ASTContext &C) const {
  return new (C) CDeclAttr(getLocation(), C, getSpellingListIndex());
}

void CDeclAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cdecl))";
    break;
  }
  case 1 : {
    OS << " [[gnu::cdecl]]";
    break;
  }
  case 2 : {
    OS << " __cdecl";
    break;
  }
  case 3 : {
    OS << " _cdecl";
    break;
  }
}
}

CFAuditedTransferAttr *CFAuditedTransferAttr::clone(ASTContext &C) const {
  return new (C) CFAuditedTransferAttr(getLocation(), C, getSpellingListIndex());
}

void CFAuditedTransferAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cf_audited_transfer))";
    break;
  }
}
}

CFConsumedAttr *CFConsumedAttr::clone(ASTContext &C) const {
  return new (C) CFConsumedAttr(getLocation(), C, getSpellingListIndex());
}

void CFConsumedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cf_consumed))";
    break;
  }
}
}

CFReturnsNotRetainedAttr *CFReturnsNotRetainedAttr::clone(ASTContext &C) const {
  return new (C) CFReturnsNotRetainedAttr(getLocation(), C, getSpellingListIndex());
}

void CFReturnsNotRetainedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cf_returns_not_retained))";
    break;
  }
}
}

CFReturnsRetainedAttr *CFReturnsRetainedAttr::clone(ASTContext &C) const {
  return new (C) CFReturnsRetainedAttr(getLocation(), C, getSpellingListIndex());
}

void CFReturnsRetainedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cf_returns_retained))";
    break;
  }
}
}

CFUnknownTransferAttr *CFUnknownTransferAttr::clone(ASTContext &C) const {
  return new (C) CFUnknownTransferAttr(getLocation(), C, getSpellingListIndex());
}

void CFUnknownTransferAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cf_unknown_transfer))";
    break;
  }
}
}

CUDAConstantAttr *CUDAConstantAttr::clone(ASTContext &C) const {
  return new (C) CUDAConstantAttr(getLocation(), C, getSpellingListIndex());
}

void CUDAConstantAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((constant))";
    break;
  }
}
}

CUDADeviceAttr *CUDADeviceAttr::clone(ASTContext &C) const {
  return new (C) CUDADeviceAttr(getLocation(), C, getSpellingListIndex());
}

void CUDADeviceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((device))";
    break;
  }
}
}

CUDAGlobalAttr *CUDAGlobalAttr::clone(ASTContext &C) const {
  return new (C) CUDAGlobalAttr(getLocation(), C, getSpellingListIndex());
}

void CUDAGlobalAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((global))";
    break;
  }
}
}

CUDAHostAttr *CUDAHostAttr::clone(ASTContext &C) const {
  return new (C) CUDAHostAttr(getLocation(), C, getSpellingListIndex());
}

void CUDAHostAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((host))";
    break;
  }
}
}

CUDALaunchBoundsAttr *CUDALaunchBoundsAttr::clone(ASTContext &C) const {
  return new (C) CUDALaunchBoundsAttr(getLocation(), C, maxThreads, minBlocks, getSpellingListIndex());
}

void CUDALaunchBoundsAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((launch_bounds(" << getMaxThreads() << ", " << getMinBlocks() << ")))";
    break;
  }
}
}

CUDASharedAttr *CUDASharedAttr::clone(ASTContext &C) const {
  return new (C) CUDASharedAttr(getLocation(), C, getSpellingListIndex());
}

void CUDASharedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((shared))";
    break;
  }
}
}

CXX11NoReturnAttr *CXX11NoReturnAttr::clone(ASTContext &C) const {
  return new (C) CXX11NoReturnAttr(getLocation(), C, getSpellingListIndex());
}

void CXX11NoReturnAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " [[noreturn]]";
    break;
  }
  case 1 : {
    OS << " [[std::noreturn]]";
    break;
  }
}
}

CarriesDependencyAttr *CarriesDependencyAttr::clone(ASTContext &C) const {
  return new (C) CarriesDependencyAttr(getLocation(), C, getSpellingListIndex());
}

void CarriesDependencyAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((carries_dependency))";
    break;
  }
  case 1 : {
    OS << " [[carries_dependency]]";
    break;
  }
  case 2 : {
    OS << " [[std::carries_dependency]]";
    break;
  }
}
}

CleanupAttr *CleanupAttr::clone(ASTContext &C) const {
  return new (C) CleanupAttr(getLocation(), C, functionDecl, getSpellingListIndex());
}

void CleanupAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cleanup(" << getFunctionDecl()->getNameInfo().getAsString() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::cleanup(" << getFunctionDecl()->getNameInfo().getAsString() << ")]]";
    break;
  }
}
}

ColdAttr *ColdAttr::clone(ASTContext &C) const {
  return new (C) ColdAttr(getLocation(), C, getSpellingListIndex());
}

void ColdAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((cold))";
    break;
  }
  case 1 : {
    OS << " [[gnu::cold]]";
    break;
  }
}
}

CommonAttr *CommonAttr::clone(ASTContext &C) const {
  return new (C) CommonAttr(getLocation(), C, getSpellingListIndex());
}

void CommonAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((common))";
    break;
  }
  case 1 : {
    OS << " [[gnu::common]]";
    break;
  }
}
}

ConstAttr *ConstAttr::clone(ASTContext &C) const {
  return new (C) ConstAttr(getLocation(), C, getSpellingListIndex());
}

void ConstAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((const))";
    break;
  }
  case 1 : {
    OS << " __attribute__((__const))";
    break;
  }
  case 2 : {
    OS << " [[gnu::const]]";
    break;
  }
}
}

ConstructorAttr *ConstructorAttr::clone(ASTContext &C) const {
  return new (C) ConstructorAttr(getLocation(), C, priority, getSpellingListIndex());
}

void ConstructorAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((constructor(" << getPriority() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::constructor(" << getPriority() << ")]]";
    break;
  }
}
}

DLLExportAttr *DLLExportAttr::clone(ASTContext &C) const {
  return new (C) DLLExportAttr(getLocation(), C, getSpellingListIndex());
}

void DLLExportAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __declspec(dllexport)";
    break;
  }
}
}

DLLImportAttr *DLLImportAttr::clone(ASTContext &C) const {
  return new (C) DLLImportAttr(getLocation(), C, getSpellingListIndex());
}

void DLLImportAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __declspec(dllimport)";
    break;
  }
}
}

DeprecatedAttr *DeprecatedAttr::clone(ASTContext &C) const {
  return new (C) DeprecatedAttr(getLocation(), C, getMessage(), getSpellingListIndex());
}

void DeprecatedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((deprecated(\"" << getMessage() << "\")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::deprecated(\"" << getMessage() << "\")]]";
    break;
  }
}
}

DestructorAttr *DestructorAttr::clone(ASTContext &C) const {
  return new (C) DestructorAttr(getLocation(), C, priority, getSpellingListIndex());
}

void DestructorAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((destructor(" << getPriority() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::destructor(" << getPriority() << ")]]";
    break;
  }
}
}

EndianAttr *EndianAttr::clone(ASTContext &C) const {
  return new (C) EndianAttr(getLocation(), C, platform, getSpellingListIndex());
}

void EndianAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((endian(" << getPlatform()->getName() << ")))";
    break;
  }
}
}

ExclusiveLockFunctionAttr *ExclusiveLockFunctionAttr::clone(ASTContext &C) const {
  return new (C) ExclusiveLockFunctionAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void ExclusiveLockFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((exclusive_lock_function(";
  bool isFirst = true;
  for (ExclusiveLockFunctionAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

ExclusiveLocksRequiredAttr *ExclusiveLocksRequiredAttr::clone(ASTContext &C) const {
  return new (C) ExclusiveLocksRequiredAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void ExclusiveLocksRequiredAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((exclusive_locks_required(";
  bool isFirst = true;
  for (ExclusiveLocksRequiredAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

ExclusiveTrylockFunctionAttr *ExclusiveTrylockFunctionAttr::clone(ASTContext &C) const {
  return new (C) ExclusiveTrylockFunctionAttr(getLocation(), C, successValue, args, argsSize, getSpellingListIndex());
}

void ExclusiveTrylockFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((exclusive_trylock_function(" << getSuccessValue() << ", ";
  bool isFirst = true;
  for (ExclusiveTrylockFunctionAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

FallThroughAttr *FallThroughAttr::clone(ASTContext &C) const {
  return new (C) FallThroughAttr(getLocation(), C, getSpellingListIndex());
}

void FallThroughAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " [[clang::fallthrough]]";
    break;
  }
}
}

FastCallAttr *FastCallAttr::clone(ASTContext &C) const {
  return new (C) FastCallAttr(getLocation(), C, getSpellingListIndex());
}

void FastCallAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((fastcall))";
    break;
  }
  case 1 : {
    OS << " [[gnu::fastcall]]";
    break;
  }
  case 2 : {
    OS << " __fastcall";
    break;
  }
  case 3 : {
    OS << " _fastcall";
    break;
  }
}
}

FinalAttr *FinalAttr::clone(ASTContext &C) const {
  return new (C) FinalAttr(getLocation(), C, getSpellingListIndex());
}

void FinalAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

ForceInlineAttr *ForceInlineAttr::clone(ASTContext &C) const {
  return new (C) ForceInlineAttr(getLocation(), C, getSpellingListIndex());
}

void ForceInlineAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __forceinline";
    break;
  }
}
}

FormatAttr *FormatAttr::clone(ASTContext &C) const {
  return new (C) FormatAttr(getLocation(), C, getType(), formatIdx, firstArg, getSpellingListIndex());
}

void FormatAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((format(\"" << getType() << "\", " << getFormatIdx() << ", " << getFirstArg() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::format(\"" << getType() << "\", " << getFormatIdx() << ", " << getFirstArg() << ")]]";
    break;
  }
}
}

FormatArgAttr *FormatArgAttr::clone(ASTContext &C) const {
  return new (C) FormatArgAttr(getLocation(), C, formatIdx, getSpellingListIndex());
}

void FormatArgAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((format_arg(" << getFormatIdx() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::format_arg(" << getFormatIdx() << ")]]";
    break;
  }
}
}

GNUInlineAttr *GNUInlineAttr::clone(ASTContext &C) const {
  return new (C) GNUInlineAttr(getLocation(), C, getSpellingListIndex());
}

void GNUInlineAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((gnu_inline))";
    break;
  }
  case 1 : {
    OS << " [[gnu::gnu_inline]]";
    break;
  }
}
}

GuardedByAttr *GuardedByAttr::clone(ASTContext &C) const {
  return new (C) GuardedByAttr(getLocation(), C, arg, getSpellingListIndex());
}

void GuardedByAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((guarded_by(" << getArg() << ")))";
    break;
  }
}
}

GuardedVarAttr *GuardedVarAttr::clone(ASTContext &C) const {
  return new (C) GuardedVarAttr(getLocation(), C, getSpellingListIndex());
}

void GuardedVarAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((guarded_var))";
    break;
  }
}
}

HotAttr *HotAttr::clone(ASTContext &C) const {
  return new (C) HotAttr(getLocation(), C, getSpellingListIndex());
}

void HotAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((hot))";
    break;
  }
  case 1 : {
    OS << " [[gnu::hot]]";
    break;
  }
}
}

IBActionAttr *IBActionAttr::clone(ASTContext &C) const {
  return new (C) IBActionAttr(getLocation(), C, getSpellingListIndex());
}

void IBActionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ibaction))";
    break;
  }
}
}

IBOutletAttr *IBOutletAttr::clone(ASTContext &C) const {
  return new (C) IBOutletAttr(getLocation(), C, getSpellingListIndex());
}

void IBOutletAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((iboutlet))";
    break;
  }
}
}

IBOutletCollectionAttr *IBOutletCollectionAttr::clone(ASTContext &C) const {
  return new (C) IBOutletCollectionAttr(getLocation(), C, interface, interfaceLoc, getSpellingListIndex());
}

void IBOutletCollectionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((iboutletcollection(" << getInterface().getAsString() << ", " << getInterfaceLoc().getRawEncoding() << ")))";
    break;
  }
}
}

InitPriorityAttr *InitPriorityAttr::clone(ASTContext &C) const {
  return new (C) InitPriorityAttr(getLocation(), C, priority, getSpellingListIndex());
}

void InitPriorityAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((init_priority(" << getPriority() << ")))";
    break;
  }
}
}

IntelOclBiccAttr *IntelOclBiccAttr::clone(ASTContext &C) const {
  return new (C) IntelOclBiccAttr(getLocation(), C, getSpellingListIndex());
}

void IntelOclBiccAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((intel_ocl_bicc))";
    break;
  }
}
}

LockReturnedAttr *LockReturnedAttr::clone(ASTContext &C) const {
  return new (C) LockReturnedAttr(getLocation(), C, arg, getSpellingListIndex());
}

void LockReturnedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((lock_returned(" << getArg() << ")))";
    break;
  }
}
}

LockableAttr *LockableAttr::clone(ASTContext &C) const {
  return new (C) LockableAttr(getLocation(), C, getSpellingListIndex());
}

void LockableAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((lockable))";
    break;
  }
}
}

LocksExcludedAttr *LocksExcludedAttr::clone(ASTContext &C) const {
  return new (C) LocksExcludedAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void LocksExcludedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((locks_excluded(";
  bool isFirst = true;
  for (LocksExcludedAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

MBlazeInterruptHandlerAttr *MBlazeInterruptHandlerAttr::clone(ASTContext &C) const {
  return new (C) MBlazeInterruptHandlerAttr(getLocation(), C, getSpellingListIndex());
}

void MBlazeInterruptHandlerAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

MBlazeSaveVolatilesAttr *MBlazeSaveVolatilesAttr::clone(ASTContext &C) const {
  return new (C) MBlazeSaveVolatilesAttr(getLocation(), C, getSpellingListIndex());
}

void MBlazeSaveVolatilesAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

MSP430InterruptAttr *MSP430InterruptAttr::clone(ASTContext &C) const {
  return new (C) MSP430InterruptAttr(getLocation(), C, number, getSpellingListIndex());
}

void MSP430InterruptAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

MallocAttr *MallocAttr::clone(ASTContext &C) const {
  return new (C) MallocAttr(getLocation(), C, getSpellingListIndex());
}

void MallocAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((malloc))";
    break;
  }
  case 1 : {
    OS << " [[gnu::malloc]]";
    break;
  }
}
}

MaxFieldAlignmentAttr *MaxFieldAlignmentAttr::clone(ASTContext &C) const {
  return new (C) MaxFieldAlignmentAttr(getLocation(), C, alignment, getSpellingListIndex());
}

void MaxFieldAlignmentAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

MayAliasAttr *MayAliasAttr::clone(ASTContext &C) const {
  return new (C) MayAliasAttr(getLocation(), C, getSpellingListIndex());
}

void MayAliasAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((may_alias))";
    break;
  }
  case 1 : {
    OS << " [[gnu::may_alias]]";
    break;
  }
}
}

MinSizeAttr *MinSizeAttr::clone(ASTContext &C) const {
  return new (C) MinSizeAttr(getLocation(), C, getSpellingListIndex());
}

void MinSizeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((minsize))";
    break;
  }
}
}

Mips16Attr *Mips16Attr::clone(ASTContext &C) const {
  return new (C) Mips16Attr(getLocation(), C, getSpellingListIndex());
}

void Mips16Attr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((mips16))";
    break;
  }
  case 1 : {
    OS << " [[gnu::mips16]]";
    break;
  }
}
}

MsPropertyAttr *MsPropertyAttr::clone(ASTContext &C) const {
  return new (C) MsPropertyAttr(getLocation(), C, getSpellingListIndex());
}

void MsPropertyAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __declspec(property)";
    break;
  }
}
}

MsStructAttr *MsStructAttr::clone(ASTContext &C) const {
  return new (C) MsStructAttr(getLocation(), C, getSpellingListIndex());
}

void MsStructAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __declspec(ms_struct)";
    break;
  }
}
}

MultipleInheritanceAttr *MultipleInheritanceAttr::clone(ASTContext &C) const {
  return new (C) MultipleInheritanceAttr(getLocation(), C, getSpellingListIndex());
}

void MultipleInheritanceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __multiple_inheritance";
    break;
  }
}
}

NSBridgedAttr *NSBridgedAttr::clone(ASTContext &C) const {
  return new (C) NSBridgedAttr(getLocation(), C, bridgedType, getSpellingListIndex());
}

void NSBridgedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_bridged(" << getBridgedType()->getName() << ")))";
    break;
  }
}
}

NSConsumedAttr *NSConsumedAttr::clone(ASTContext &C) const {
  return new (C) NSConsumedAttr(getLocation(), C, getSpellingListIndex());
}

void NSConsumedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_consumed))";
    break;
  }
}
}

NSConsumesSelfAttr *NSConsumesSelfAttr::clone(ASTContext &C) const {
  return new (C) NSConsumesSelfAttr(getLocation(), C, getSpellingListIndex());
}

void NSConsumesSelfAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_consumes_self))";
    break;
  }
}
}

NSReturnsAutoreleasedAttr *NSReturnsAutoreleasedAttr::clone(ASTContext &C) const {
  return new (C) NSReturnsAutoreleasedAttr(getLocation(), C, getSpellingListIndex());
}

void NSReturnsAutoreleasedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_returns_autoreleased))";
    break;
  }
}
}

NSReturnsNotRetainedAttr *NSReturnsNotRetainedAttr::clone(ASTContext &C) const {
  return new (C) NSReturnsNotRetainedAttr(getLocation(), C, getSpellingListIndex());
}

void NSReturnsNotRetainedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_returns_not_retained))";
    break;
  }
}
}

NSReturnsRetainedAttr *NSReturnsRetainedAttr::clone(ASTContext &C) const {
  return new (C) NSReturnsRetainedAttr(getLocation(), C, getSpellingListIndex());
}

void NSReturnsRetainedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ns_returns_retained))";
    break;
  }
}
}

NakedAttr *NakedAttr::clone(ASTContext &C) const {
  return new (C) NakedAttr(getLocation(), C, getSpellingListIndex());
}

void NakedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((naked))";
    break;
  }
  case 1 : {
    OS << " [[gnu::naked]]";
    break;
  }
}
}

NoCommonAttr *NoCommonAttr::clone(ASTContext &C) const {
  return new (C) NoCommonAttr(getLocation(), C, getSpellingListIndex());
}

void NoCommonAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((nocommon))";
    break;
  }
  case 1 : {
    OS << " [[gnu::nocommon]]";
    break;
  }
}
}

NoDebugAttr *NoDebugAttr::clone(ASTContext &C) const {
  return new (C) NoDebugAttr(getLocation(), C, getSpellingListIndex());
}

void NoDebugAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((nodebug))";
    break;
  }
}
}

NoInlineAttr *NoInlineAttr::clone(ASTContext &C) const {
  return new (C) NoInlineAttr(getLocation(), C, getSpellingListIndex());
}

void NoInlineAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((noinline))";
    break;
  }
  case 1 : {
    OS << " [[gnu::noinline]]";
    break;
  }
}
}

NoInstrumentFunctionAttr *NoInstrumentFunctionAttr::clone(ASTContext &C) const {
  return new (C) NoInstrumentFunctionAttr(getLocation(), C, getSpellingListIndex());
}

void NoInstrumentFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((no_instrument_function))";
    break;
  }
  case 1 : {
    OS << " [[gnu::no_instrument_function]]";
    break;
  }
}
}

NoMips16Attr *NoMips16Attr::clone(ASTContext &C) const {
  return new (C) NoMips16Attr(getLocation(), C, getSpellingListIndex());
}

void NoMips16Attr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((nomips16))";
    break;
  }
  case 1 : {
    OS << " [[gnu::nomips16]]";
    break;
  }
}
}

NoReturnAttr *NoReturnAttr::clone(ASTContext &C) const {
  return new (C) NoReturnAttr(getLocation(), C, getSpellingListIndex());
}

void NoReturnAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((noreturn))";
    break;
  }
  case 1 : {
    OS << " [[gnu::noreturn]]";
    break;
  }
}
}

NoSanitizeAddressAttr *NoSanitizeAddressAttr::clone(ASTContext &C) const {
  return new (C) NoSanitizeAddressAttr(getLocation(), C, getSpellingListIndex());
}

void NoSanitizeAddressAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((no_address_safety_analysis))";
    break;
  }
  case 1 : {
    OS << " __attribute__((no_sanitize_address))";
    break;
  }
}
}

NoSanitizeMemoryAttr *NoSanitizeMemoryAttr::clone(ASTContext &C) const {
  return new (C) NoSanitizeMemoryAttr(getLocation(), C, getSpellingListIndex());
}

void NoSanitizeMemoryAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((no_sanitize_memory))";
    break;
  }
}
}

NoSanitizeThreadAttr *NoSanitizeThreadAttr::clone(ASTContext &C) const {
  return new (C) NoSanitizeThreadAttr(getLocation(), C, getSpellingListIndex());
}

void NoSanitizeThreadAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((no_sanitize_thread))";
    break;
  }
}
}

NoThreadSafetyAnalysisAttr *NoThreadSafetyAnalysisAttr::clone(ASTContext &C) const {
  return new (C) NoThreadSafetyAnalysisAttr(getLocation(), C, getSpellingListIndex());
}

void NoThreadSafetyAnalysisAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((no_thread_safety_analysis))";
    break;
  }
}
}

NoThrowAttr *NoThrowAttr::clone(ASTContext &C) const {
  return new (C) NoThrowAttr(getLocation(), C, getSpellingListIndex());
}

void NoThrowAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((nothrow))";
    break;
  }
  case 1 : {
    OS << " [[gnu::nothrow]]";
    break;
  }
}
}

NonNullAttr *NonNullAttr::clone(ASTContext &C) const {
  return new (C) NonNullAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void NonNullAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((nonnull(";
  bool isFirst = true;
  for (NonNullAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::nonnull(";
  bool isFirst = true;
  for (NonNullAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")]]";
    break;
  }
}
}

ObjCExceptionAttr *ObjCExceptionAttr::clone(ASTContext &C) const {
  return new (C) ObjCExceptionAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCExceptionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_exception))";
    break;
  }
}
}

ObjCMethodFamilyAttr *ObjCMethodFamilyAttr::clone(ASTContext &C) const {
  return new (C) ObjCMethodFamilyAttr(getLocation(), C, family, getSpellingListIndex());
}

void ObjCMethodFamilyAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_method_family(" << getFamily() << ")))";
    break;
  }
}
}

ObjCNSObjectAttr *ObjCNSObjectAttr::clone(ASTContext &C) const {
  return new (C) ObjCNSObjectAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCNSObjectAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((NSObject))";
    break;
  }
}
}

ObjCPreciseLifetimeAttr *ObjCPreciseLifetimeAttr::clone(ASTContext &C) const {
  return new (C) ObjCPreciseLifetimeAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCPreciseLifetimeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_precise_lifetime))";
    break;
  }
}
}

ObjCRequiresPropertyDefsAttr *ObjCRequiresPropertyDefsAttr::clone(ASTContext &C) const {
  return new (C) ObjCRequiresPropertyDefsAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCRequiresPropertyDefsAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_requires_property_definitions))";
    break;
  }
}
}

ObjCRequiresSuperAttr *ObjCRequiresSuperAttr::clone(ASTContext &C) const {
  return new (C) ObjCRequiresSuperAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCRequiresSuperAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_requires_super))";
    break;
  }
}
}

ObjCReturnsInnerPointerAttr *ObjCReturnsInnerPointerAttr::clone(ASTContext &C) const {
  return new (C) ObjCReturnsInnerPointerAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCReturnsInnerPointerAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_returns_inner_pointer))";
    break;
  }
}
}

ObjCRootClassAttr *ObjCRootClassAttr::clone(ASTContext &C) const {
  return new (C) ObjCRootClassAttr(getLocation(), C, getSpellingListIndex());
}

void ObjCRootClassAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((objc_root_class))";
    break;
  }
}
}

OpenCLImageAccessAttr *OpenCLImageAccessAttr::clone(ASTContext &C) const {
  return new (C) OpenCLImageAccessAttr(getLocation(), C, access, getSpellingListIndex());
}

void OpenCLImageAccessAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((opencl_image_access(" << getAccess() << ")))";
    break;
  }
}
}

OpenCLKernelAttr *OpenCLKernelAttr::clone(ASTContext &C) const {
  return new (C) OpenCLKernelAttr(getLocation(), C, getSpellingListIndex());
}

void OpenCLKernelAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __kernel";
    break;
  }
  case 1 : {
    OS << " kernel";
    break;
  }
}
}

OverloadableAttr *OverloadableAttr::clone(ASTContext &C) const {
  return new (C) OverloadableAttr(getLocation(), C, getSpellingListIndex());
}

void OverloadableAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((overloadable))";
    break;
  }
}
}

OverrideAttr *OverrideAttr::clone(ASTContext &C) const {
  return new (C) OverrideAttr(getLocation(), C, getSpellingListIndex());
}

void OverrideAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

OwnershipAttr *OwnershipAttr::clone(ASTContext &C) const {
  return new (C) OwnershipAttr(getLocation(), C, ownKind, getModule(), args, argsSize, getSpellingListIndex());
}

void OwnershipAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((ownership_holds(" << getOwnKind() << ", \"" << getModule() << "\", ";
  bool isFirst = true;
  for (OwnershipAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
  case 1 : {
    OS << " __attribute__((ownership_returns(" << getOwnKind() << ", \"" << getModule() << "\", ";
  bool isFirst = true;
  for (OwnershipAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
  case 2 : {
    OS << " __attribute__((ownership_takes(" << getOwnKind() << ", \"" << getModule() << "\", ";
  bool isFirst = true;
  for (OwnershipAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

PackedAttr *PackedAttr::clone(ASTContext &C) const {
  return new (C) PackedAttr(getLocation(), C, getSpellingListIndex());
}

void PackedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((packed))";
    break;
  }
  case 1 : {
    OS << " [[gnu::packed]]";
    break;
  }
}
}

PascalAttr *PascalAttr::clone(ASTContext &C) const {
  return new (C) PascalAttr(getLocation(), C, getSpellingListIndex());
}

void PascalAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pascal))";
    break;
  }
  case 1 : {
    OS << " __pascal";
    break;
  }
  case 2 : {
    OS << " _pascal";
    break;
  }
}
}

PcsAttr *PcsAttr::clone(ASTContext &C) const {
  return new (C) PcsAttr(getLocation(), C, pCS, getSpellingListIndex());
}

void PcsAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pcs(" << getPCS() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::pcs(" << getPCS() << ")]]";
    break;
  }
}
}

PnaclCallAttr *PnaclCallAttr::clone(ASTContext &C) const {
  return new (C) PnaclCallAttr(getLocation(), C, getSpellingListIndex());
}

void PnaclCallAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pnaclcall))";
    break;
  }
}
}

PtGuardedByAttr *PtGuardedByAttr::clone(ASTContext &C) const {
  return new (C) PtGuardedByAttr(getLocation(), C, arg, getSpellingListIndex());
}

void PtGuardedByAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pt_guarded_by(" << getArg() << ")))";
    break;
  }
}
}

PtGuardedVarAttr *PtGuardedVarAttr::clone(ASTContext &C) const {
  return new (C) PtGuardedVarAttr(getLocation(), C, getSpellingListIndex());
}

void PtGuardedVarAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pt_guarded_var))";
    break;
  }
}
}

Ptr32Attr *Ptr32Attr::clone(ASTContext &C) const {
  return new (C) Ptr32Attr(getLocation(), C, getSpellingListIndex());
}

void Ptr32Attr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __ptr32";
    break;
  }
}
}

Ptr64Attr *Ptr64Attr::clone(ASTContext &C) const {
  return new (C) Ptr64Attr(getLocation(), C, getSpellingListIndex());
}

void Ptr64Attr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __ptr64";
    break;
  }
}
}

PureAttr *PureAttr::clone(ASTContext &C) const {
  return new (C) PureAttr(getLocation(), C, getSpellingListIndex());
}

void PureAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((pure))";
    break;
  }
  case 1 : {
    OS << " [[gnu::pure]]";
    break;
  }
}
}

RegparmAttr *RegparmAttr::clone(ASTContext &C) const {
  return new (C) RegparmAttr(getLocation(), C, numParams, getSpellingListIndex());
}

void RegparmAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((regparm(" << getNumParams() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::regparm(" << getNumParams() << ")]]";
    break;
  }
}
}

ReqdWorkGroupSizeAttr *ReqdWorkGroupSizeAttr::clone(ASTContext &C) const {
  return new (C) ReqdWorkGroupSizeAttr(getLocation(), C, xDim, yDim, zDim, getSpellingListIndex());
}

void ReqdWorkGroupSizeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((reqd_work_group_size(" << getXDim() << ", " << getYDim() << ", " << getZDim() << ")))";
    break;
  }
}
}

ReturnsTwiceAttr *ReturnsTwiceAttr::clone(ASTContext &C) const {
  return new (C) ReturnsTwiceAttr(getLocation(), C, getSpellingListIndex());
}

void ReturnsTwiceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((returns_twice))";
    break;
  }
  case 1 : {
    OS << " [[gnu::returns_twice]]";
    break;
  }
}
}

ScopedLockableAttr *ScopedLockableAttr::clone(ASTContext &C) const {
  return new (C) ScopedLockableAttr(getLocation(), C, getSpellingListIndex());
}

void ScopedLockableAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((scoped_lockable))";
    break;
  }
}
}

SectionAttr *SectionAttr::clone(ASTContext &C) const {
  return new (C) SectionAttr(getLocation(), C, getName(), getSpellingListIndex());
}

void SectionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((section(\"" << getName() << "\")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::section(\"" << getName() << "\")]]";
    break;
  }
}
}

SentinelAttr *SentinelAttr::clone(ASTContext &C) const {
  return new (C) SentinelAttr(getLocation(), C, sentinel, nullPos, getSpellingListIndex());
}

void SentinelAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((sentinel(" << getSentinel() << ", " << getNullPos() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::sentinel(" << getSentinel() << ", " << getNullPos() << ")]]";
    break;
  }
}
}

SharedLockFunctionAttr *SharedLockFunctionAttr::clone(ASTContext &C) const {
  return new (C) SharedLockFunctionAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void SharedLockFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((shared_lock_function(";
  bool isFirst = true;
  for (SharedLockFunctionAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

SharedLocksRequiredAttr *SharedLocksRequiredAttr::clone(ASTContext &C) const {
  return new (C) SharedLocksRequiredAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void SharedLocksRequiredAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((shared_locks_required(";
  bool isFirst = true;
  for (SharedLocksRequiredAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

SharedTrylockFunctionAttr *SharedTrylockFunctionAttr::clone(ASTContext &C) const {
  return new (C) SharedTrylockFunctionAttr(getLocation(), C, successValue, args, argsSize, getSpellingListIndex());
}

void SharedTrylockFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((shared_trylock_function(" << getSuccessValue() << ", ";
  bool isFirst = true;
  for (SharedTrylockFunctionAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

SingleInheritanceAttr *SingleInheritanceAttr::clone(ASTContext &C) const {
  return new (C) SingleInheritanceAttr(getLocation(), C, getSpellingListIndex());
}

void SingleInheritanceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __single_inheritance";
    break;
  }
}
}

StdCallAttr *StdCallAttr::clone(ASTContext &C) const {
  return new (C) StdCallAttr(getLocation(), C, getSpellingListIndex());
}

void StdCallAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((stdcall))";
    break;
  }
  case 1 : {
    OS << " [[gnu::stdcall]]";
    break;
  }
  case 2 : {
    OS << " __stdcall";
    break;
  }
  case 3 : {
    OS << " _stdcall";
    break;
  }
}
}

TLSModelAttr *TLSModelAttr::clone(ASTContext &C) const {
  return new (C) TLSModelAttr(getLocation(), C, getModel(), getSpellingListIndex());
}

void TLSModelAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((tls_model(\"" << getModel() << "\")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::tls_model(\"" << getModel() << "\")]]";
    break;
  }
}
}

ThisCallAttr *ThisCallAttr::clone(ASTContext &C) const {
  return new (C) ThisCallAttr(getLocation(), C, getSpellingListIndex());
}

void ThisCallAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((thiscall))";
    break;
  }
  case 1 : {
    OS << " [[gnu::thiscall]]";
    break;
  }
  case 2 : {
    OS << " __thiscall";
    break;
  }
  case 3 : {
    OS << " _thiscall";
    break;
  }
}
}

TransparentUnionAttr *TransparentUnionAttr::clone(ASTContext &C) const {
  return new (C) TransparentUnionAttr(getLocation(), C, getSpellingListIndex());
}

void TransparentUnionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((transparent_union))";
    break;
  }
  case 1 : {
    OS << " [[gnu::transparent_union]]";
    break;
  }
}
}

TypeTagForDatatypeAttr *TypeTagForDatatypeAttr::clone(ASTContext &C) const {
  return new (C) TypeTagForDatatypeAttr(getLocation(), C, argumentKind, matchingCType, layoutCompatible, mustBeNull, getSpellingListIndex());
}

void TypeTagForDatatypeAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((type_tag_for_datatype(" << getArgumentKind()->getName() << ", " << getMatchingCType().getAsString() << ", " << getLayoutCompatible() << ", " << getMustBeNull() << ")))";
    break;
  }
}
}

TypeVisibilityAttr *TypeVisibilityAttr::clone(ASTContext &C) const {
  return new (C) TypeVisibilityAttr(getLocation(), C, visibility, getSpellingListIndex());
}

void TypeVisibilityAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((type_visibility(" << getVisibility() << ")))";
    break;
  }
  case 1 : {
    OS << " [[clang::type_visibility(" << getVisibility() << ")]]";
    break;
  }
}
}

UnavailableAttr *UnavailableAttr::clone(ASTContext &C) const {
  return new (C) UnavailableAttr(getLocation(), C, getMessage(), getSpellingListIndex());
}

void UnavailableAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((unavailable(\"" << getMessage() << "\")))";
    break;
  }
}
}

UnlockFunctionAttr *UnlockFunctionAttr::clone(ASTContext &C) const {
  return new (C) UnlockFunctionAttr(getLocation(), C, args, argsSize, getSpellingListIndex());
}

void UnlockFunctionAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((unlock_function(";
  bool isFirst = true;
  for (UnlockFunctionAttr::args_iterator i = args_begin(), e = args_end(); i != e; ++i) {
    if (isFirst) isFirst = false;
    else OS << ", ";
    OS << *i;
  }
  OS << ")))";
    break;
  }
}
}

UnspecifiedInheritanceAttr *UnspecifiedInheritanceAttr::clone(ASTContext &C) const {
  return new (C) UnspecifiedInheritanceAttr(getLocation(), C, getSpellingListIndex());
}

void UnspecifiedInheritanceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

UnusedAttr *UnusedAttr::clone(ASTContext &C) const {
  return new (C) UnusedAttr(getLocation(), C, getSpellingListIndex());
}

void UnusedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((unused))";
    break;
  }
  case 1 : {
    OS << " [[gnu::unused]]";
    break;
  }
}
}

UsedAttr *UsedAttr::clone(ASTContext &C) const {
  return new (C) UsedAttr(getLocation(), C, getSpellingListIndex());
}

void UsedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((used))";
    break;
  }
  case 1 : {
    OS << " [[gnu::used]]";
    break;
  }
}
}

UuidAttr *UuidAttr::clone(ASTContext &C) const {
  return new (C) UuidAttr(getLocation(), C, getGuid(), getSpellingListIndex());
}

void UuidAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((uuid(\"" << getGuid() << "\")))";
    break;
  }
}
}

VecReturnAttr *VecReturnAttr::clone(ASTContext &C) const {
  return new (C) VecReturnAttr(getLocation(), C, getSpellingListIndex());
}

void VecReturnAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((vecreturn))";
    break;
  }
}
}

VecTypeHintAttr *VecTypeHintAttr::clone(ASTContext &C) const {
  return new (C) VecTypeHintAttr(getLocation(), C, typeHint, typeLoc, getSpellingListIndex());
}

void VecTypeHintAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((vec_type_hint(" << getTypeHint().getAsString() << ", " << getTypeLoc().getRawEncoding() << ")))";
    break;
  }
}
}

VirtualInheritanceAttr *VirtualInheritanceAttr::clone(ASTContext &C) const {
  return new (C) VirtualInheritanceAttr(getLocation(), C, getSpellingListIndex());
}

void VirtualInheritanceAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __virtual_inheritance";
    break;
  }
}
}

VisibilityAttr *VisibilityAttr::clone(ASTContext &C) const {
  return new (C) VisibilityAttr(getLocation(), C, visibility, getSpellingListIndex());
}

void VisibilityAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((visibility(" << getVisibility() << ")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::visibility(" << getVisibility() << ")]]";
    break;
  }
}
}

WarnUnusedResultAttr *WarnUnusedResultAttr::clone(ASTContext &C) const {
  return new (C) WarnUnusedResultAttr(getLocation(), C, getSpellingListIndex());
}

void WarnUnusedResultAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((warn_unused_result))";
    break;
  }
  case 1 : {
    OS << " [[clang::warn_unused_result]]";
    break;
  }
  case 2 : {
    OS << " [[gnu::warn_unused_result]]";
    break;
  }
}
}

WeakAttr *WeakAttr::clone(ASTContext &C) const {
  return new (C) WeakAttr(getLocation(), C, getSpellingListIndex());
}

void WeakAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((weak))";
    break;
  }
  case 1 : {
    OS << " [[gnu::weak]]";
    break;
  }
}
}

WeakImportAttr *WeakImportAttr::clone(ASTContext &C) const {
  return new (C) WeakImportAttr(getLocation(), C, getSpellingListIndex());
}

void WeakImportAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((weak_import))";
    break;
  }
}
}

WeakRefAttr *WeakRefAttr::clone(ASTContext &C) const {
  return new (C) WeakRefAttr(getLocation(), C, getSpellingListIndex());
}

void WeakRefAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((weakref))";
    break;
  }
  case 1 : {
    OS << " [[gnu::weakref]]";
    break;
  }
}
}

Win64Attr *Win64Attr::clone(ASTContext &C) const {
  return new (C) Win64Attr(getLocation(), C, getSpellingListIndex());
}

void Win64Attr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __w64";
    break;
  }
}
}

WorkGroupSizeHintAttr *WorkGroupSizeHintAttr::clone(ASTContext &C) const {
  return new (C) WorkGroupSizeHintAttr(getLocation(), C, xDim, yDim, zDim, getSpellingListIndex());
}

void WorkGroupSizeHintAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  switch (SpellingListIndex) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((work_group_size_hint(" << getXDim() << ", " << getYDim() << ", " << getZDim() << ")))";
    break;
  }
}
}

X86ForceAlignArgPointerAttr *X86ForceAlignArgPointerAttr::clone(ASTContext &C) const {
  return new (C) X86ForceAlignArgPointerAttr(getLocation(), C, getSpellingListIndex());
}

void X86ForceAlignArgPointerAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
}

