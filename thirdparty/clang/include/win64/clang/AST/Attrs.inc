/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute classes' definitions                                              *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef LLVM_CLANG_ATTR_CLASSES_INC
#define LLVM_CLANG_ATTR_CLASSES_INC

class AcquiredAfterAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  AcquiredAfterAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AcquiredAfter, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual AcquiredAfterAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::AcquiredAfter; }
  virtual bool isLateParsed() const { return 1; }
};

class AcquiredBeforeAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  AcquiredBeforeAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AcquiredBefore, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual AcquiredBeforeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::AcquiredBefore; }
  virtual bool isLateParsed() const { return 1; }
};

class AliasAttr : public InheritableAttr {
unsigned aliaseeLength;
char *aliasee;


 public:
  AliasAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Aliasee
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Alias, R, SI)
              , aliaseeLength(Aliasee.size()),aliasee(new (Ctx, 1) char[aliaseeLength])
  {
      std::memcpy(aliasee, Aliasee.data(), aliaseeLength);
  }

  virtual AliasAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getAliasee() const {
    return llvm::StringRef(aliasee, aliaseeLength);
  }
  unsigned getAliaseeLength() const {
    return aliaseeLength;
  }
  void setAliasee(ASTContext &C, llvm::StringRef S) {
    aliaseeLength = S.size();
    this->aliasee = new (C, 1) char [aliaseeLength];
    std::memcpy(this->aliasee, S.data(), aliaseeLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Alias; }
  virtual bool isLateParsed() const { return 0; }
};

class AlignMac68kAttr : public InheritableAttr {

 public:
  AlignMac68kAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AlignMac68k, R, SI)
  {
  }

  virtual AlignMac68kAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::AlignMac68k; }
  virtual bool isLateParsed() const { return 0; }
};

class AlignedAttr : public InheritableAttr {
bool isalignmentExpr;
union {
Expr *alignmentExpr;
TypeSourceInfo *alignmentType;
};


 public:
  AlignedAttr(SourceRange R, ASTContext &Ctx
              , bool IsAlignmentExpr, void *Alignment
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Aligned, R, SI)
              , isalignmentExpr(IsAlignmentExpr)
  {
    if (isalignmentExpr)
       alignmentExpr = reinterpret_cast<Expr *>(Alignment);
    else
       alignmentType = reinterpret_cast<TypeSourceInfo *>(Alignment);
  }

  virtual AlignedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  bool isGNU() const { return SpellingListIndex == 0 ||
    SpellingListIndex == 2; }
  bool isC11() const { return SpellingListIndex == 4; }
  bool isAlignas() const { return SpellingListIndex == 3 ||
    SpellingListIndex == 4; }
  bool isDeclspec() const { return SpellingListIndex == 1; }
  bool isAlignmentDependent() const;
  unsigned getAlignment(ASTContext &Ctx) const;
  bool isAlignmentExpr() const {
    return isalignmentExpr;
  }
  Expr *getAlignmentExpr() const {
    assert(isalignmentExpr);
    return alignmentExpr;
  }
  TypeSourceInfo *getAlignmentType() const {
    assert(!isalignmentExpr);
    return alignmentType;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Aligned; }
  virtual bool isLateParsed() const { return 0; }
};

class AllocSizeAttr : public Attr {
  unsigned argsSize;
  unsigned *args;


 public:
  AllocSizeAttr(SourceRange R, ASTContext &Ctx
              , unsigned *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : Attr(attr::AllocSize, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) unsigned[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(unsigned));

  }

  virtual AllocSizeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef unsigned* args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::AllocSize; }
  virtual bool isLateParsed() const { return 0; }
};

class AlwaysInlineAttr : public InheritableAttr {

 public:
  AlwaysInlineAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AlwaysInline, R, SI)
  {
  }

  virtual AlwaysInlineAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::AlwaysInline; }
  virtual bool isLateParsed() const { return 0; }
};

class AnalyzerNoReturnAttr : public InheritableAttr {

 public:
  AnalyzerNoReturnAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AnalyzerNoReturn, R, SI)
  {
  }

  virtual AnalyzerNoReturnAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::AnalyzerNoReturn; }
  virtual bool isLateParsed() const { return 0; }
};

class AnnotateAttr : public InheritableParamAttr {
unsigned annotationLength;
char *annotation;


 public:
  AnnotateAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Annotation
              , unsigned SI = 0
             )
    : InheritableParamAttr(attr::Annotate, R, SI)
              , annotationLength(Annotation.size()),annotation(new (Ctx, 1) char[annotationLength])
  {
      std::memcpy(annotation, Annotation.data(), annotationLength);
  }

  virtual AnnotateAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getAnnotation() const {
    return llvm::StringRef(annotation, annotationLength);
  }
  unsigned getAnnotationLength() const {
    return annotationLength;
  }
  void setAnnotation(ASTContext &C, llvm::StringRef S) {
    annotationLength = S.size();
    this->annotation = new (C, 1) char [annotationLength];
    std::memcpy(this->annotation, S.data(), annotationLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Annotate; }
  virtual bool isLateParsed() const { return 0; }
};

class ArcWeakrefUnavailableAttr : public InheritableAttr {

 public:
  ArcWeakrefUnavailableAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ArcWeakrefUnavailable, R, SI)
  {
  }

  virtual ArcWeakrefUnavailableAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ArcWeakrefUnavailable; }
  virtual bool isLateParsed() const { return 0; }
};

class ArgumentWithTypeTagAttr : public InheritableAttr {
IdentifierInfo * argumentKind;

unsigned argumentIdx;

unsigned typeTagIdx;

bool isPointer;


 public:
  ArgumentWithTypeTagAttr(SourceRange R, ASTContext &Ctx
              , IdentifierInfo * ArgumentKind
              , unsigned ArgumentIdx
              , unsigned TypeTagIdx
              , bool IsPointer
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ArgumentWithTypeTag, R, SI)
              , argumentKind(ArgumentKind)
              , argumentIdx(ArgumentIdx)
              , typeTagIdx(TypeTagIdx)
              , isPointer(IsPointer)
  {




  }

  virtual ArgumentWithTypeTagAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  IdentifierInfo * getArgumentKind() const {
    return argumentKind;
  }

  unsigned getArgumentIdx() const {
    return argumentIdx;
  }

  unsigned getTypeTagIdx() const {
    return typeTagIdx;
  }

  bool getIsPointer() const {
    return isPointer;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ArgumentWithTypeTag; }
  virtual bool isLateParsed() const { return 0; }
};

class AsmLabelAttr : public InheritableAttr {
unsigned labelLength;
char *label;


 public:
  AsmLabelAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Label
              , unsigned SI = 0
             )
    : InheritableAttr(attr::AsmLabel, R, SI)
              , labelLength(Label.size()),label(new (Ctx, 1) char[labelLength])
  {
      std::memcpy(label, Label.data(), labelLength);
  }

  virtual AsmLabelAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getLabel() const {
    return llvm::StringRef(label, labelLength);
  }
  unsigned getLabelLength() const {
    return labelLength;
  }
  void setLabel(ASTContext &C, llvm::StringRef S) {
    labelLength = S.size();
    this->label = new (C, 1) char [labelLength];
    std::memcpy(this->label, S.data(), labelLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::AsmLabel; }
  virtual bool isLateParsed() const { return 0; }
};

class AvailabilityAttr : public InheritableAttr {
IdentifierInfo * platform;

VersionTuple introduced;


VersionTuple deprecated;


VersionTuple obsoleted;


bool unavailable;

unsigned messageLength;
char *message;


 public:
  AvailabilityAttr(SourceRange R, ASTContext &Ctx
              , IdentifierInfo * Platform
              , VersionTuple Introduced
              , VersionTuple Deprecated
              , VersionTuple Obsoleted
              , bool Unavailable
              , llvm::StringRef Message
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Availability, R, SI)
              , platform(Platform)
              , introduced(Introduced)
              , deprecated(Deprecated)
              , obsoleted(Obsoleted)
              , unavailable(Unavailable)
              , messageLength(Message.size()),message(new (Ctx, 1) char[messageLength])
  {





      std::memcpy(message, Message.data(), messageLength);
  }

  virtual AvailabilityAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  IdentifierInfo * getPlatform() const {
    return platform;
  }

  VersionTuple getIntroduced() const {
    return introduced;
  }
  void setIntroduced(ASTContext &C, VersionTuple V) {
    introduced = V;
  }

  VersionTuple getDeprecated() const {
    return deprecated;
  }
  void setDeprecated(ASTContext &C, VersionTuple V) {
    deprecated = V;
  }

  VersionTuple getObsoleted() const {
    return obsoleted;
  }
  void setObsoleted(ASTContext &C, VersionTuple V) {
    obsoleted = V;
  }

  bool getUnavailable() const {
    return unavailable;
  }

  llvm::StringRef getMessage() const {
    return llvm::StringRef(message, messageLength);
  }
  unsigned getMessageLength() const {
    return messageLength;
  }
  void setMessage(ASTContext &C, llvm::StringRef S) {
    messageLength = S.size();
    this->message = new (C, 1) char [messageLength];
    std::memcpy(this->message, S.data(), messageLength);
  }

static llvm::StringRef getPrettyPlatformName(llvm::StringRef Platform) {
    return llvm::StringSwitch<llvm::StringRef>(Platform)
             .Case("ios", "iOS")
             .Case("macosx", "OS X")
             .Default(llvm::StringRef());
} 

  static bool classof(const Attr *A) { return A->getKind() == attr::Availability; }
  virtual bool isLateParsed() const { return 0; }
};

class BlocksAttr : public InheritableAttr {
public:
  enum BlockType {
    ByRef
  };
private:
  BlockType type;


 public:
  BlocksAttr(SourceRange R, ASTContext &Ctx
              , BlockType Type
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Blocks, R, SI)
              , type(Type)
  {

  }

  virtual BlocksAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  BlockType getType() const {
    return type;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Blocks; }
  virtual bool isLateParsed() const { return 0; }
};

class C11NoReturnAttr : public InheritableAttr {

 public:
  C11NoReturnAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::C11NoReturn, R, SI)
  {
  }

  virtual C11NoReturnAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::C11NoReturn; }
  virtual bool isLateParsed() const { return 0; }
};

class CDeclAttr : public InheritableAttr {

 public:
  CDeclAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CDecl, R, SI)
  {
  }

  virtual CDeclAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CDecl; }
  virtual bool isLateParsed() const { return 0; }
};

class CFAuditedTransferAttr : public InheritableAttr {

 public:
  CFAuditedTransferAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CFAuditedTransfer, R, SI)
  {
  }

  virtual CFAuditedTransferAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CFAuditedTransfer; }
  virtual bool isLateParsed() const { return 0; }
};

class CFConsumedAttr : public InheritableParamAttr {

 public:
  CFConsumedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableParamAttr(attr::CFConsumed, R, SI)
  {
  }

  virtual CFConsumedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CFConsumed; }
  virtual bool isLateParsed() const { return 0; }
};

class CFReturnsNotRetainedAttr : public InheritableAttr {

 public:
  CFReturnsNotRetainedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CFReturnsNotRetained, R, SI)
  {
  }

  virtual CFReturnsNotRetainedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CFReturnsNotRetained; }
  virtual bool isLateParsed() const { return 0; }
};

class CFReturnsRetainedAttr : public InheritableAttr {

 public:
  CFReturnsRetainedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CFReturnsRetained, R, SI)
  {
  }

  virtual CFReturnsRetainedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CFReturnsRetained; }
  virtual bool isLateParsed() const { return 0; }
};

class CFUnknownTransferAttr : public InheritableAttr {

 public:
  CFUnknownTransferAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CFUnknownTransfer, R, SI)
  {
  }

  virtual CFUnknownTransferAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CFUnknownTransfer; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDAConstantAttr : public InheritableAttr {

 public:
  CUDAConstantAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDAConstant, R, SI)
  {
  }

  virtual CUDAConstantAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CUDAConstant; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDADeviceAttr : public InheritableAttr {

 public:
  CUDADeviceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDADevice, R, SI)
  {
  }

  virtual CUDADeviceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CUDADevice; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDAGlobalAttr : public InheritableAttr {

 public:
  CUDAGlobalAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDAGlobal, R, SI)
  {
  }

  virtual CUDAGlobalAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CUDAGlobal; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDAHostAttr : public InheritableAttr {

 public:
  CUDAHostAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDAHost, R, SI)
  {
  }

  virtual CUDAHostAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CUDAHost; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDALaunchBoundsAttr : public InheritableAttr {
int maxThreads;

int minBlocks;


 public:
  CUDALaunchBoundsAttr(SourceRange R, ASTContext &Ctx
              , int MaxThreads
              , int MinBlocks
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDALaunchBounds, R, SI)
              , maxThreads(MaxThreads)
              , minBlocks(MinBlocks)
  {


  }

  virtual CUDALaunchBoundsAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getMaxThreads() const {
    return maxThreads;
  }

  int getMinBlocks() const {
    return minBlocks;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::CUDALaunchBounds; }
  virtual bool isLateParsed() const { return 0; }
};

class CUDASharedAttr : public InheritableAttr {

 public:
  CUDASharedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CUDAShared, R, SI)
  {
  }

  virtual CUDASharedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CUDAShared; }
  virtual bool isLateParsed() const { return 0; }
};

class CXX11NoReturnAttr : public InheritableAttr {

 public:
  CXX11NoReturnAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::CXX11NoReturn, R, SI)
  {
  }

  virtual CXX11NoReturnAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CXX11NoReturn; }
  virtual bool isLateParsed() const { return 0; }
};

class CarriesDependencyAttr : public InheritableParamAttr {

 public:
  CarriesDependencyAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableParamAttr(attr::CarriesDependency, R, SI)
  {
  }

  virtual CarriesDependencyAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::CarriesDependency; }
  virtual bool isLateParsed() const { return 0; }
};

class CleanupAttr : public InheritableAttr {
FunctionDecl * functionDecl;


 public:
  CleanupAttr(SourceRange R, ASTContext &Ctx
              , FunctionDecl * FunctionDecl
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Cleanup, R, SI)
              , functionDecl(FunctionDecl)
  {

  }

  virtual CleanupAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  FunctionDecl * getFunctionDecl() const {
    return functionDecl;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Cleanup; }
  virtual bool isLateParsed() const { return 0; }
};

class ColdAttr : public InheritableAttr {

 public:
  ColdAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Cold, R, SI)
  {
  }

  virtual ColdAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Cold; }
  virtual bool isLateParsed() const { return 0; }
};

class CommonAttr : public InheritableAttr {

 public:
  CommonAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Common, R, SI)
  {
  }

  virtual CommonAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Common; }
  virtual bool isLateParsed() const { return 0; }
};

class ConstAttr : public InheritableAttr {

 public:
  ConstAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Const, R, SI)
  {
  }

  virtual ConstAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Const; }
  virtual bool isLateParsed() const { return 0; }
};

class ConstructorAttr : public InheritableAttr {
int priority;


 public:
  ConstructorAttr(SourceRange R, ASTContext &Ctx
              , int Priority
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Constructor, R, SI)
              , priority(Priority)
  {

  }

  virtual ConstructorAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getPriority() const {
    return priority;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Constructor; }
  virtual bool isLateParsed() const { return 0; }
};

class DLLExportAttr : public InheritableAttr {

 public:
  DLLExportAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::DLLExport, R, SI)
  {
  }

  virtual DLLExportAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::DLLExport; }
  virtual bool isLateParsed() const { return 0; }
};

class DLLImportAttr : public InheritableAttr {

 public:
  DLLImportAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::DLLImport, R, SI)
  {
  }

  virtual DLLImportAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::DLLImport; }
  virtual bool isLateParsed() const { return 0; }
};

class DeprecatedAttr : public InheritableAttr {
unsigned messageLength;
char *message;


 public:
  DeprecatedAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Message
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Deprecated, R, SI)
              , messageLength(Message.size()),message(new (Ctx, 1) char[messageLength])
  {
      std::memcpy(message, Message.data(), messageLength);
  }

  virtual DeprecatedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getMessage() const {
    return llvm::StringRef(message, messageLength);
  }
  unsigned getMessageLength() const {
    return messageLength;
  }
  void setMessage(ASTContext &C, llvm::StringRef S) {
    messageLength = S.size();
    this->message = new (C, 1) char [messageLength];
    std::memcpy(this->message, S.data(), messageLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Deprecated; }
  virtual bool isLateParsed() const { return 0; }
};

class DestructorAttr : public InheritableAttr {
int priority;


 public:
  DestructorAttr(SourceRange R, ASTContext &Ctx
              , int Priority
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Destructor, R, SI)
              , priority(Priority)
  {

  }

  virtual DestructorAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getPriority() const {
    return priority;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Destructor; }
  virtual bool isLateParsed() const { return 0; }
};

class EndianAttr : public InheritableAttr {
IdentifierInfo * platform;


 public:
  EndianAttr(SourceRange R, ASTContext &Ctx
              , IdentifierInfo * Platform
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Endian, R, SI)
              , platform(Platform)
  {

  }

  virtual EndianAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  IdentifierInfo * getPlatform() const {
    return platform;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Endian; }
  virtual bool isLateParsed() const { return 0; }
};

class ExclusiveLockFunctionAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  ExclusiveLockFunctionAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ExclusiveLockFunction, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual ExclusiveLockFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ExclusiveLockFunction; }
  virtual bool isLateParsed() const { return 1; }
};

class ExclusiveLocksRequiredAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  ExclusiveLocksRequiredAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ExclusiveLocksRequired, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual ExclusiveLocksRequiredAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ExclusiveLocksRequired; }
  virtual bool isLateParsed() const { return 1; }
};

class ExclusiveTrylockFunctionAttr : public InheritableAttr {
Expr * successValue;

  unsigned argsSize;
  Expr * *args;


 public:
  ExclusiveTrylockFunctionAttr(SourceRange R, ASTContext &Ctx
              , Expr * SuccessValue
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ExclusiveTrylockFunction, R, SI)
              , successValue(SuccessValue)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {

    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual ExclusiveTrylockFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  Expr * getSuccessValue() const {
    return successValue;
  }

  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ExclusiveTrylockFunction; }
  virtual bool isLateParsed() const { return 1; }
};

class FallThroughAttr : public Attr {

 public:
  FallThroughAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::FallThrough, R, SI)
  {
  }

  virtual FallThroughAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::FallThrough; }
  virtual bool isLateParsed() const { return 0; }
};

class FastCallAttr : public InheritableAttr {

 public:
  FastCallAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::FastCall, R, SI)
  {
  }

  virtual FastCallAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::FastCall; }
  virtual bool isLateParsed() const { return 0; }
};

class FinalAttr : public InheritableAttr {

 public:
  FinalAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Final, R, SI)
  {
  }

  virtual FinalAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Final; }
  virtual bool isLateParsed() const { return 0; }
};

class ForceInlineAttr : public InheritableAttr {

 public:
  ForceInlineAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ForceInline, R, SI)
  {
  }

  virtual ForceInlineAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ForceInline; }
  virtual bool isLateParsed() const { return 0; }
};

class FormatAttr : public InheritableAttr {
unsigned typeLength;
char *type;

int formatIdx;

int firstArg;


 public:
  FormatAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Type
              , int FormatIdx
              , int FirstArg
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Format, R, SI)
              , typeLength(Type.size()),type(new (Ctx, 1) char[typeLength])
              , formatIdx(FormatIdx)
              , firstArg(FirstArg)
  {
      std::memcpy(type, Type.data(), typeLength);


  }

  virtual FormatAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getType() const {
    return llvm::StringRef(type, typeLength);
  }
  unsigned getTypeLength() const {
    return typeLength;
  }
  void setType(ASTContext &C, llvm::StringRef S) {
    typeLength = S.size();
    this->type = new (C, 1) char [typeLength];
    std::memcpy(this->type, S.data(), typeLength);
  }

  int getFormatIdx() const {
    return formatIdx;
  }

  int getFirstArg() const {
    return firstArg;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Format; }
  virtual bool isLateParsed() const { return 0; }
};

class FormatArgAttr : public InheritableAttr {
int formatIdx;


 public:
  FormatArgAttr(SourceRange R, ASTContext &Ctx
              , int FormatIdx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::FormatArg, R, SI)
              , formatIdx(FormatIdx)
  {

  }

  virtual FormatArgAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getFormatIdx() const {
    return formatIdx;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::FormatArg; }
  virtual bool isLateParsed() const { return 0; }
};

class GNUInlineAttr : public InheritableAttr {

 public:
  GNUInlineAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::GNUInline, R, SI)
  {
  }

  virtual GNUInlineAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::GNUInline; }
  virtual bool isLateParsed() const { return 0; }
};

class GuardedByAttr : public InheritableAttr {
Expr * arg;


 public:
  GuardedByAttr(SourceRange R, ASTContext &Ctx
              , Expr * Arg
              , unsigned SI = 0
             )
    : InheritableAttr(attr::GuardedBy, R, SI)
              , arg(Arg)
  {

  }

  virtual GuardedByAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  Expr * getArg() const {
    return arg;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::GuardedBy; }
  virtual bool isLateParsed() const { return 1; }
};

class GuardedVarAttr : public InheritableAttr {

 public:
  GuardedVarAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::GuardedVar, R, SI)
  {
  }

  virtual GuardedVarAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::GuardedVar; }
  virtual bool isLateParsed() const { return 0; }
};

class HotAttr : public InheritableAttr {

 public:
  HotAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Hot, R, SI)
  {
  }

  virtual HotAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Hot; }
  virtual bool isLateParsed() const { return 0; }
};

class IBActionAttr : public InheritableAttr {

 public:
  IBActionAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::IBAction, R, SI)
  {
  }

  virtual IBActionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::IBAction; }
  virtual bool isLateParsed() const { return 0; }
};

class IBOutletAttr : public InheritableAttr {

 public:
  IBOutletAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::IBOutlet, R, SI)
  {
  }

  virtual IBOutletAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::IBOutlet; }
  virtual bool isLateParsed() const { return 0; }
};

class IBOutletCollectionAttr : public InheritableAttr {
QualType interface;

SourceLocation interfaceLoc;


 public:
  IBOutletCollectionAttr(SourceRange R, ASTContext &Ctx
              , QualType Interface
              , SourceLocation InterfaceLoc
              , unsigned SI = 0
             )
    : InheritableAttr(attr::IBOutletCollection, R, SI)
              , interface(Interface)
              , interfaceLoc(InterfaceLoc)
  {


  }

  virtual IBOutletCollectionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  QualType getInterface() const {
    return interface;
  }

  SourceLocation getInterfaceLoc() const {
    return interfaceLoc;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::IBOutletCollection; }
  virtual bool isLateParsed() const { return 0; }
};

class InitPriorityAttr : public InheritableAttr {
unsigned priority;


 public:
  InitPriorityAttr(SourceRange R, ASTContext &Ctx
              , unsigned Priority
              , unsigned SI = 0
             )
    : InheritableAttr(attr::InitPriority, R, SI)
              , priority(Priority)
  {

  }

  virtual InitPriorityAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getPriority() const {
    return priority;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::InitPriority; }
  virtual bool isLateParsed() const { return 0; }
};

class IntelOclBiccAttr : public InheritableAttr {

 public:
  IntelOclBiccAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::IntelOclBicc, R, SI)
  {
  }

  virtual IntelOclBiccAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::IntelOclBicc; }
  virtual bool isLateParsed() const { return 0; }
};

class LockReturnedAttr : public InheritableAttr {
Expr * arg;


 public:
  LockReturnedAttr(SourceRange R, ASTContext &Ctx
              , Expr * Arg
              , unsigned SI = 0
             )
    : InheritableAttr(attr::LockReturned, R, SI)
              , arg(Arg)
  {

  }

  virtual LockReturnedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  Expr * getArg() const {
    return arg;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::LockReturned; }
  virtual bool isLateParsed() const { return 1; }
};

class LockableAttr : public InheritableAttr {

 public:
  LockableAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Lockable, R, SI)
  {
  }

  virtual LockableAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Lockable; }
  virtual bool isLateParsed() const { return 0; }
};

class LocksExcludedAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  LocksExcludedAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::LocksExcluded, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual LocksExcludedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::LocksExcluded; }
  virtual bool isLateParsed() const { return 1; }
};

class MBlazeInterruptHandlerAttr : public InheritableAttr {

 public:
  MBlazeInterruptHandlerAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MBlazeInterruptHandler, R, SI)
  {
  }

  virtual MBlazeInterruptHandlerAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MBlazeInterruptHandler; }
  virtual bool isLateParsed() const { return 0; }
};

class MBlazeSaveVolatilesAttr : public InheritableAttr {

 public:
  MBlazeSaveVolatilesAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MBlazeSaveVolatiles, R, SI)
  {
  }

  virtual MBlazeSaveVolatilesAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MBlazeSaveVolatiles; }
  virtual bool isLateParsed() const { return 0; }
};

class MSP430InterruptAttr : public InheritableAttr {
unsigned number;


 public:
  MSP430InterruptAttr(SourceRange R, ASTContext &Ctx
              , unsigned Number
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MSP430Interrupt, R, SI)
              , number(Number)
  {

  }

  virtual MSP430InterruptAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getNumber() const {
    return number;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::MSP430Interrupt; }
  virtual bool isLateParsed() const { return 0; }
};

class MallocAttr : public InheritableAttr {

 public:
  MallocAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Malloc, R, SI)
  {
  }

  virtual MallocAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Malloc; }
  virtual bool isLateParsed() const { return 0; }
};

class MaxFieldAlignmentAttr : public InheritableAttr {
unsigned alignment;


 public:
  MaxFieldAlignmentAttr(SourceRange R, ASTContext &Ctx
              , unsigned Alignment
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MaxFieldAlignment, R, SI)
              , alignment(Alignment)
  {

  }

  virtual MaxFieldAlignmentAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getAlignment() const {
    return alignment;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::MaxFieldAlignment; }
  virtual bool isLateParsed() const { return 0; }
};

class MayAliasAttr : public InheritableAttr {

 public:
  MayAliasAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MayAlias, R, SI)
  {
  }

  virtual MayAliasAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MayAlias; }
  virtual bool isLateParsed() const { return 0; }
};

class MinSizeAttr : public InheritableAttr {

 public:
  MinSizeAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MinSize, R, SI)
  {
  }

  virtual MinSizeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MinSize; }
  virtual bool isLateParsed() const { return 0; }
};

class Mips16Attr : public InheritableAttr {

 public:
  Mips16Attr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Mips16, R, SI)
  {
  }

  virtual Mips16Attr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Mips16; }
  virtual bool isLateParsed() const { return 0; }
};

class MsPropertyAttr : public Attr {

 public:
  MsPropertyAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::MsProperty, R, SI)
  {
  }

  virtual MsPropertyAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MsProperty; }
  virtual bool isLateParsed() const { return 0; }
};

class MsStructAttr : public InheritableAttr {

 public:
  MsStructAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::MsStruct, R, SI)
  {
  }

  virtual MsStructAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MsStruct; }
  virtual bool isLateParsed() const { return 0; }
};

class MultipleInheritanceAttr : public MSInheritanceAttr {

 public:
  MultipleInheritanceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : MSInheritanceAttr(attr::MultipleInheritance, R, SI)
  {
  }

  virtual MultipleInheritanceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::MultipleInheritance; }
  virtual bool isLateParsed() const { return 0; }
};

class NSBridgedAttr : public InheritableAttr {
IdentifierInfo * bridgedType;


 public:
  NSBridgedAttr(SourceRange R, ASTContext &Ctx
              , IdentifierInfo * BridgedType
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NSBridged, R, SI)
              , bridgedType(BridgedType)
  {

  }

  virtual NSBridgedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  IdentifierInfo * getBridgedType() const {
    return bridgedType;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::NSBridged; }
  virtual bool isLateParsed() const { return 0; }
};

class NSConsumedAttr : public InheritableParamAttr {

 public:
  NSConsumedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableParamAttr(attr::NSConsumed, R, SI)
  {
  }

  virtual NSConsumedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NSConsumed; }
  virtual bool isLateParsed() const { return 0; }
};

class NSConsumesSelfAttr : public InheritableAttr {

 public:
  NSConsumesSelfAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NSConsumesSelf, R, SI)
  {
  }

  virtual NSConsumesSelfAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NSConsumesSelf; }
  virtual bool isLateParsed() const { return 0; }
};

class NSReturnsAutoreleasedAttr : public InheritableAttr {

 public:
  NSReturnsAutoreleasedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NSReturnsAutoreleased, R, SI)
  {
  }

  virtual NSReturnsAutoreleasedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NSReturnsAutoreleased; }
  virtual bool isLateParsed() const { return 0; }
};

class NSReturnsNotRetainedAttr : public InheritableAttr {

 public:
  NSReturnsNotRetainedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NSReturnsNotRetained, R, SI)
  {
  }

  virtual NSReturnsNotRetainedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NSReturnsNotRetained; }
  virtual bool isLateParsed() const { return 0; }
};

class NSReturnsRetainedAttr : public InheritableAttr {

 public:
  NSReturnsRetainedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NSReturnsRetained, R, SI)
  {
  }

  virtual NSReturnsRetainedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NSReturnsRetained; }
  virtual bool isLateParsed() const { return 0; }
};

class NakedAttr : public InheritableAttr {

 public:
  NakedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Naked, R, SI)
  {
  }

  virtual NakedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Naked; }
  virtual bool isLateParsed() const { return 0; }
};

class NoCommonAttr : public InheritableAttr {

 public:
  NoCommonAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoCommon, R, SI)
  {
  }

  virtual NoCommonAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoCommon; }
  virtual bool isLateParsed() const { return 0; }
};

class NoDebugAttr : public InheritableAttr {

 public:
  NoDebugAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoDebug, R, SI)
  {
  }

  virtual NoDebugAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoDebug; }
  virtual bool isLateParsed() const { return 0; }
};

class NoInlineAttr : public InheritableAttr {

 public:
  NoInlineAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoInline, R, SI)
  {
  }

  virtual NoInlineAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoInline; }
  virtual bool isLateParsed() const { return 0; }
};

class NoInstrumentFunctionAttr : public InheritableAttr {

 public:
  NoInstrumentFunctionAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoInstrumentFunction, R, SI)
  {
  }

  virtual NoInstrumentFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoInstrumentFunction; }
  virtual bool isLateParsed() const { return 0; }
};

class NoMips16Attr : public InheritableAttr {

 public:
  NoMips16Attr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoMips16, R, SI)
  {
  }

  virtual NoMips16Attr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoMips16; }
  virtual bool isLateParsed() const { return 0; }
};

class NoReturnAttr : public InheritableAttr {

 public:
  NoReturnAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoReturn, R, SI)
  {
  }

  virtual NoReturnAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoReturn; }
  virtual bool isLateParsed() const { return 0; }
};

class NoSanitizeAddressAttr : public InheritableAttr {

 public:
  NoSanitizeAddressAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoSanitizeAddress, R, SI)
  {
  }

  virtual NoSanitizeAddressAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoSanitizeAddress; }
  virtual bool isLateParsed() const { return 0; }
};

class NoSanitizeMemoryAttr : public InheritableAttr {

 public:
  NoSanitizeMemoryAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoSanitizeMemory, R, SI)
  {
  }

  virtual NoSanitizeMemoryAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoSanitizeMemory; }
  virtual bool isLateParsed() const { return 0; }
};

class NoSanitizeThreadAttr : public InheritableAttr {

 public:
  NoSanitizeThreadAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoSanitizeThread, R, SI)
  {
  }

  virtual NoSanitizeThreadAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoSanitizeThread; }
  virtual bool isLateParsed() const { return 0; }
};

class NoThreadSafetyAnalysisAttr : public InheritableAttr {

 public:
  NoThreadSafetyAnalysisAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoThreadSafetyAnalysis, R, SI)
  {
  }

  virtual NoThreadSafetyAnalysisAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoThreadSafetyAnalysis; }
  virtual bool isLateParsed() const { return 0; }
};

class NoThrowAttr : public InheritableAttr {

 public:
  NoThrowAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NoThrow, R, SI)
  {
  }

  virtual NoThrowAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::NoThrow; }
  virtual bool isLateParsed() const { return 0; }
};

class NonNullAttr : public InheritableAttr {
  unsigned argsSize;
  unsigned *args;


 public:
  NonNullAttr(SourceRange R, ASTContext &Ctx
              , unsigned *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::NonNull, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) unsigned[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(unsigned));

  }

  virtual NonNullAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef unsigned* args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }

bool isNonNull(unsigned idx) const {
    for (args_iterator i = args_begin(), e = args_end();
         i != e; ++i)
      if (*i == idx)
        return true;
    return false;
  } 

  static bool classof(const Attr *A) { return A->getKind() == attr::NonNull; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCExceptionAttr : public InheritableAttr {

 public:
  ObjCExceptionAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ObjCException, R, SI)
  {
  }

  virtual ObjCExceptionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCException; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCMethodFamilyAttr : public InheritableAttr {
public:
  enum FamilyKind {
    OMF_None,
    OMF_alloc,
    OMF_copy,
    OMF_init,
    OMF_mutableCopy,
    OMF_new
  };
private:
  FamilyKind family;


 public:
  ObjCMethodFamilyAttr(SourceRange R, ASTContext &Ctx
              , FamilyKind Family
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ObjCMethodFamily, R, SI)
              , family(Family)
  {

  }

  virtual ObjCMethodFamilyAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  FamilyKind getFamily() const {
    return family;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCMethodFamily; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCNSObjectAttr : public InheritableAttr {

 public:
  ObjCNSObjectAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ObjCNSObject, R, SI)
  {
  }

  virtual ObjCNSObjectAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCNSObject; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCPreciseLifetimeAttr : public Attr {

 public:
  ObjCPreciseLifetimeAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::ObjCPreciseLifetime, R, SI)
  {
  }

  virtual ObjCPreciseLifetimeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCPreciseLifetime; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCRequiresPropertyDefsAttr : public InheritableAttr {

 public:
  ObjCRequiresPropertyDefsAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ObjCRequiresPropertyDefs, R, SI)
  {
  }

  virtual ObjCRequiresPropertyDefsAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCRequiresPropertyDefs; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCRequiresSuperAttr : public InheritableAttr {

 public:
  ObjCRequiresSuperAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ObjCRequiresSuper, R, SI)
  {
  }

  virtual ObjCRequiresSuperAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCRequiresSuper; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCReturnsInnerPointerAttr : public Attr {

 public:
  ObjCReturnsInnerPointerAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::ObjCReturnsInnerPointer, R, SI)
  {
  }

  virtual ObjCReturnsInnerPointerAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCReturnsInnerPointer; }
  virtual bool isLateParsed() const { return 0; }
};

class ObjCRootClassAttr : public Attr {

 public:
  ObjCRootClassAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::ObjCRootClass, R, SI)
  {
  }

  virtual ObjCRootClassAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ObjCRootClass; }
  virtual bool isLateParsed() const { return 0; }
};

class OpenCLImageAccessAttr : public Attr {
int access;


 public:
  OpenCLImageAccessAttr(SourceRange R, ASTContext &Ctx
              , int Access
              , unsigned SI = 0
             )
    : Attr(attr::OpenCLImageAccess, R, SI)
              , access(Access)
  {

  }

  virtual OpenCLImageAccessAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getAccess() const {
    return access;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::OpenCLImageAccess; }
  virtual bool isLateParsed() const { return 0; }
};

class OpenCLKernelAttr : public Attr {

 public:
  OpenCLKernelAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::OpenCLKernel, R, SI)
  {
  }

  virtual OpenCLKernelAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::OpenCLKernel; }
  virtual bool isLateParsed() const { return 0; }
};

class OverloadableAttr : public Attr {

 public:
  OverloadableAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : Attr(attr::Overloadable, R, SI)
  {
  }

  virtual OverloadableAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Overloadable; }
  virtual bool isLateParsed() const { return 0; }
};

class OverrideAttr : public InheritableAttr {

 public:
  OverrideAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Override, R, SI)
  {
  }

  virtual OverrideAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Override; }
  virtual bool isLateParsed() const { return 0; }
};

class OwnershipAttr : public InheritableAttr {
public:
  enum OwnershipKind {
    Holds,
    Returns,
    Takes
  };
private:
  OwnershipKind ownKind;

unsigned moduleLength;
char *module;

  unsigned argsSize;
  unsigned *args;


 public:
  OwnershipAttr(SourceRange R, ASTContext &Ctx
              , OwnershipKind OwnKind
              , llvm::StringRef Module
              , unsigned *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Ownership, R, SI)
              , ownKind(OwnKind)
              , moduleLength(Module.size()),module(new (Ctx, 1) char[moduleLength])
              , argsSize(ArgsSize), args(new (Ctx, 16) unsigned[argsSize])
  {

      std::memcpy(module, Module.data(), moduleLength);
    std::memcpy(args, Args, argsSize * sizeof(unsigned));

  }

  virtual OwnershipAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  OwnershipKind getOwnKind() const {
    return ownKind;
  }

  llvm::StringRef getModule() const {
    return llvm::StringRef(module, moduleLength);
  }
  unsigned getModuleLength() const {
    return moduleLength;
  }
  void setModule(ASTContext &C, llvm::StringRef S) {
    moduleLength = S.size();
    this->module = new (C, 1) char [moduleLength];
    std::memcpy(this->module, S.data(), moduleLength);
  }

  typedef unsigned* args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Ownership; }
  virtual bool isLateParsed() const { return 0; }
};

class PackedAttr : public InheritableAttr {

 public:
  PackedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Packed, R, SI)
  {
  }

  virtual PackedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Packed; }
  virtual bool isLateParsed() const { return 0; }
};

class PascalAttr : public InheritableAttr {

 public:
  PascalAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Pascal, R, SI)
  {
  }

  virtual PascalAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Pascal; }
  virtual bool isLateParsed() const { return 0; }
};

class PcsAttr : public InheritableAttr {
public:
  enum PCSType {
    AAPCS,
    AAPCS_VFP
  };
private:
  PCSType pCS;


 public:
  PcsAttr(SourceRange R, ASTContext &Ctx
              , PCSType PCS
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Pcs, R, SI)
              , pCS(PCS)
  {

  }

  virtual PcsAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  PCSType getPCS() const {
    return pCS;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Pcs; }
  virtual bool isLateParsed() const { return 0; }
};

class PnaclCallAttr : public InheritableAttr {

 public:
  PnaclCallAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::PnaclCall, R, SI)
  {
  }

  virtual PnaclCallAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::PnaclCall; }
  virtual bool isLateParsed() const { return 0; }
};

class PtGuardedByAttr : public InheritableAttr {
Expr * arg;


 public:
  PtGuardedByAttr(SourceRange R, ASTContext &Ctx
              , Expr * Arg
              , unsigned SI = 0
             )
    : InheritableAttr(attr::PtGuardedBy, R, SI)
              , arg(Arg)
  {

  }

  virtual PtGuardedByAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  Expr * getArg() const {
    return arg;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::PtGuardedBy; }
  virtual bool isLateParsed() const { return 1; }
};

class PtGuardedVarAttr : public InheritableAttr {

 public:
  PtGuardedVarAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::PtGuardedVar, R, SI)
  {
  }

  virtual PtGuardedVarAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::PtGuardedVar; }
  virtual bool isLateParsed() const { return 0; }
};

class Ptr32Attr : public InheritableAttr {

 public:
  Ptr32Attr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Ptr32, R, SI)
  {
  }

  virtual Ptr32Attr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Ptr32; }
  virtual bool isLateParsed() const { return 0; }
};

class Ptr64Attr : public InheritableAttr {

 public:
  Ptr64Attr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Ptr64, R, SI)
  {
  }

  virtual Ptr64Attr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Ptr64; }
  virtual bool isLateParsed() const { return 0; }
};

class PureAttr : public InheritableAttr {

 public:
  PureAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Pure, R, SI)
  {
  }

  virtual PureAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Pure; }
  virtual bool isLateParsed() const { return 0; }
};

class RegparmAttr : public InheritableAttr {
unsigned numParams;


 public:
  RegparmAttr(SourceRange R, ASTContext &Ctx
              , unsigned NumParams
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Regparm, R, SI)
              , numParams(NumParams)
  {

  }

  virtual RegparmAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getNumParams() const {
    return numParams;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Regparm; }
  virtual bool isLateParsed() const { return 0; }
};

class ReqdWorkGroupSizeAttr : public InheritableAttr {
unsigned xDim;

unsigned yDim;

unsigned zDim;


 public:
  ReqdWorkGroupSizeAttr(SourceRange R, ASTContext &Ctx
              , unsigned XDim
              , unsigned YDim
              , unsigned ZDim
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ReqdWorkGroupSize, R, SI)
              , xDim(XDim)
              , yDim(YDim)
              , zDim(ZDim)
  {



  }

  virtual ReqdWorkGroupSizeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getXDim() const {
    return xDim;
  }

  unsigned getYDim() const {
    return yDim;
  }

  unsigned getZDim() const {
    return zDim;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::ReqdWorkGroupSize; }
  virtual bool isLateParsed() const { return 0; }
};

class ReturnsTwiceAttr : public InheritableAttr {

 public:
  ReturnsTwiceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ReturnsTwice, R, SI)
  {
  }

  virtual ReturnsTwiceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ReturnsTwice; }
  virtual bool isLateParsed() const { return 0; }
};

class ScopedLockableAttr : public InheritableAttr {

 public:
  ScopedLockableAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ScopedLockable, R, SI)
  {
  }

  virtual ScopedLockableAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ScopedLockable; }
  virtual bool isLateParsed() const { return 0; }
};

class SectionAttr : public InheritableAttr {
unsigned nameLength;
char *name;


 public:
  SectionAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Name
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Section, R, SI)
              , nameLength(Name.size()),name(new (Ctx, 1) char[nameLength])
  {
      std::memcpy(name, Name.data(), nameLength);
  }

  virtual SectionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getName() const {
    return llvm::StringRef(name, nameLength);
  }
  unsigned getNameLength() const {
    return nameLength;
  }
  void setName(ASTContext &C, llvm::StringRef S) {
    nameLength = S.size();
    this->name = new (C, 1) char [nameLength];
    std::memcpy(this->name, S.data(), nameLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Section; }
  virtual bool isLateParsed() const { return 0; }
};

class SentinelAttr : public InheritableAttr {
int sentinel;

int nullPos;


 public:
  SentinelAttr(SourceRange R, ASTContext &Ctx
              , int Sentinel
              , int NullPos
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Sentinel, R, SI)
              , sentinel(Sentinel)
              , nullPos(NullPos)
  {


  }

  virtual SentinelAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  int getSentinel() const {
    return sentinel;
  }

  int getNullPos() const {
    return nullPos;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Sentinel; }
  virtual bool isLateParsed() const { return 0; }
};

class SharedLockFunctionAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  SharedLockFunctionAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::SharedLockFunction, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual SharedLockFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::SharedLockFunction; }
  virtual bool isLateParsed() const { return 1; }
};

class SharedLocksRequiredAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  SharedLocksRequiredAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::SharedLocksRequired, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual SharedLocksRequiredAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::SharedLocksRequired; }
  virtual bool isLateParsed() const { return 1; }
};

class SharedTrylockFunctionAttr : public InheritableAttr {
Expr * successValue;

  unsigned argsSize;
  Expr * *args;


 public:
  SharedTrylockFunctionAttr(SourceRange R, ASTContext &Ctx
              , Expr * SuccessValue
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::SharedTrylockFunction, R, SI)
              , successValue(SuccessValue)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {

    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual SharedTrylockFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  Expr * getSuccessValue() const {
    return successValue;
  }

  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::SharedTrylockFunction; }
  virtual bool isLateParsed() const { return 1; }
};

class SingleInheritanceAttr : public MSInheritanceAttr {

 public:
  SingleInheritanceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : MSInheritanceAttr(attr::SingleInheritance, R, SI)
  {
  }

  virtual SingleInheritanceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::SingleInheritance; }
  virtual bool isLateParsed() const { return 0; }
};

class StdCallAttr : public InheritableAttr {

 public:
  StdCallAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::StdCall, R, SI)
  {
  }

  virtual StdCallAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::StdCall; }
  virtual bool isLateParsed() const { return 0; }
};

class TLSModelAttr : public InheritableAttr {
unsigned modelLength;
char *model;


 public:
  TLSModelAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Model
              , unsigned SI = 0
             )
    : InheritableAttr(attr::TLSModel, R, SI)
              , modelLength(Model.size()),model(new (Ctx, 1) char[modelLength])
  {
      std::memcpy(model, Model.data(), modelLength);
  }

  virtual TLSModelAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getModel() const {
    return llvm::StringRef(model, modelLength);
  }
  unsigned getModelLength() const {
    return modelLength;
  }
  void setModel(ASTContext &C, llvm::StringRef S) {
    modelLength = S.size();
    this->model = new (C, 1) char [modelLength];
    std::memcpy(this->model, S.data(), modelLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::TLSModel; }
  virtual bool isLateParsed() const { return 0; }
};

class ThisCallAttr : public InheritableAttr {

 public:
  ThisCallAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::ThisCall, R, SI)
  {
  }

  virtual ThisCallAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::ThisCall; }
  virtual bool isLateParsed() const { return 0; }
};

class TransparentUnionAttr : public InheritableAttr {

 public:
  TransparentUnionAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::TransparentUnion, R, SI)
  {
  }

  virtual TransparentUnionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::TransparentUnion; }
  virtual bool isLateParsed() const { return 0; }
};

class TypeTagForDatatypeAttr : public InheritableAttr {
IdentifierInfo * argumentKind;

QualType matchingCType;

bool layoutCompatible;

bool mustBeNull;


 public:
  TypeTagForDatatypeAttr(SourceRange R, ASTContext &Ctx
              , IdentifierInfo * ArgumentKind
              , QualType MatchingCType
              , bool LayoutCompatible
              , bool MustBeNull
              , unsigned SI = 0
             )
    : InheritableAttr(attr::TypeTagForDatatype, R, SI)
              , argumentKind(ArgumentKind)
              , matchingCType(MatchingCType)
              , layoutCompatible(LayoutCompatible)
              , mustBeNull(MustBeNull)
  {




  }

  virtual TypeTagForDatatypeAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  IdentifierInfo * getArgumentKind() const {
    return argumentKind;
  }

  QualType getMatchingCType() const {
    return matchingCType;
  }

  bool getLayoutCompatible() const {
    return layoutCompatible;
  }

  bool getMustBeNull() const {
    return mustBeNull;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::TypeTagForDatatype; }
  virtual bool isLateParsed() const { return 0; }
};

class TypeVisibilityAttr : public InheritableAttr {
public:
  enum VisibilityType {
    Default,
    Hidden,
    Protected
  };
private:
  VisibilityType visibility;


 public:
  TypeVisibilityAttr(SourceRange R, ASTContext &Ctx
              , VisibilityType Visibility
              , unsigned SI = 0
             )
    : InheritableAttr(attr::TypeVisibility, R, SI)
              , visibility(Visibility)
  {

  }

  virtual TypeVisibilityAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  VisibilityType getVisibility() const {
    return visibility;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::TypeVisibility; }
  virtual bool isLateParsed() const { return 0; }
};

class UnavailableAttr : public InheritableAttr {
unsigned messageLength;
char *message;


 public:
  UnavailableAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Message
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Unavailable, R, SI)
              , messageLength(Message.size()),message(new (Ctx, 1) char[messageLength])
  {
      std::memcpy(message, Message.data(), messageLength);
  }

  virtual UnavailableAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getMessage() const {
    return llvm::StringRef(message, messageLength);
  }
  unsigned getMessageLength() const {
    return messageLength;
  }
  void setMessage(ASTContext &C, llvm::StringRef S) {
    messageLength = S.size();
    this->message = new (C, 1) char [messageLength];
    std::memcpy(this->message, S.data(), messageLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Unavailable; }
  virtual bool isLateParsed() const { return 0; }
};

class UnlockFunctionAttr : public InheritableAttr {
  unsigned argsSize;
  Expr * *args;


 public:
  UnlockFunctionAttr(SourceRange R, ASTContext &Ctx
              , Expr * *Args, unsigned ArgsSize
              , unsigned SI = 0
             )
    : InheritableAttr(attr::UnlockFunction, R, SI)
              , argsSize(ArgsSize), args(new (Ctx, 16) Expr *[argsSize])
  {
    std::memcpy(args, Args, argsSize * sizeof(Expr *));

  }

  virtual UnlockFunctionAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  typedef Expr ** args_iterator;
  args_iterator args_begin() const {
    return args;
  }
  args_iterator args_end() const {
    return args + argsSize;
  }
  unsigned args_size() const {
    return argsSize;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::UnlockFunction; }
  virtual bool isLateParsed() const { return 1; }
};

class UnspecifiedInheritanceAttr : public MSInheritanceAttr {

 public:
  UnspecifiedInheritanceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : MSInheritanceAttr(attr::UnspecifiedInheritance, R, SI)
  {
  }

  virtual UnspecifiedInheritanceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::UnspecifiedInheritance; }
  virtual bool isLateParsed() const { return 0; }
};

class UnusedAttr : public InheritableAttr {

 public:
  UnusedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Unused, R, SI)
  {
  }

  virtual UnusedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Unused; }
  virtual bool isLateParsed() const { return 0; }
};

class UsedAttr : public InheritableAttr {

 public:
  UsedAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Used, R, SI)
  {
  }

  virtual UsedAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Used; }
  virtual bool isLateParsed() const { return 0; }
};

class UuidAttr : public InheritableAttr {
unsigned guidLength;
char *guid;


 public:
  UuidAttr(SourceRange R, ASTContext &Ctx
              , llvm::StringRef Guid
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Uuid, R, SI)
              , guidLength(Guid.size()),guid(new (Ctx, 1) char[guidLength])
  {
      std::memcpy(guid, Guid.data(), guidLength);
  }

  virtual UuidAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  llvm::StringRef getGuid() const {
    return llvm::StringRef(guid, guidLength);
  }
  unsigned getGuidLength() const {
    return guidLength;
  }
  void setGuid(ASTContext &C, llvm::StringRef S) {
    guidLength = S.size();
    this->guid = new (C, 1) char [guidLength];
    std::memcpy(this->guid, S.data(), guidLength);
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Uuid; }
  virtual bool isLateParsed() const { return 0; }
};

class VecReturnAttr : public InheritableAttr {

 public:
  VecReturnAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::VecReturn, R, SI)
  {
  }

  virtual VecReturnAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::VecReturn; }
  virtual bool isLateParsed() const { return 0; }
};

class VecTypeHintAttr : public InheritableAttr {
QualType typeHint;

SourceLocation typeLoc;


 public:
  VecTypeHintAttr(SourceRange R, ASTContext &Ctx
              , QualType TypeHint
              , SourceLocation TypeLoc
              , unsigned SI = 0
             )
    : InheritableAttr(attr::VecTypeHint, R, SI)
              , typeHint(TypeHint)
              , typeLoc(TypeLoc)
  {


  }

  virtual VecTypeHintAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  QualType getTypeHint() const {
    return typeHint;
  }

  SourceLocation getTypeLoc() const {
    return typeLoc;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::VecTypeHint; }
  virtual bool isLateParsed() const { return 0; }
};

class VirtualInheritanceAttr : public MSInheritanceAttr {

 public:
  VirtualInheritanceAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : MSInheritanceAttr(attr::VirtualInheritance, R, SI)
  {
  }

  virtual VirtualInheritanceAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::VirtualInheritance; }
  virtual bool isLateParsed() const { return 0; }
};

class VisibilityAttr : public InheritableAttr {
public:
  enum VisibilityType {
    Default,
    Hidden,
    Protected
  };
private:
  VisibilityType visibility;


 public:
  VisibilityAttr(SourceRange R, ASTContext &Ctx
              , VisibilityType Visibility
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Visibility, R, SI)
              , visibility(Visibility)
  {

  }

  virtual VisibilityAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  VisibilityType getVisibility() const {
    return visibility;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::Visibility; }
  virtual bool isLateParsed() const { return 0; }
};

class WarnUnusedResultAttr : public InheritableAttr {

 public:
  WarnUnusedResultAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::WarnUnusedResult, R, SI)
  {
  }

  virtual WarnUnusedResultAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::WarnUnusedResult; }
  virtual bool isLateParsed() const { return 0; }
};

class WeakAttr : public InheritableAttr {

 public:
  WeakAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Weak, R, SI)
  {
  }

  virtual WeakAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Weak; }
  virtual bool isLateParsed() const { return 0; }
};

class WeakImportAttr : public InheritableAttr {

 public:
  WeakImportAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::WeakImport, R, SI)
  {
  }

  virtual WeakImportAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::WeakImport; }
  virtual bool isLateParsed() const { return 0; }
};

class WeakRefAttr : public InheritableAttr {

 public:
  WeakRefAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::WeakRef, R, SI)
  {
  }

  virtual WeakRefAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::WeakRef; }
  virtual bool isLateParsed() const { return 0; }
};

class Win64Attr : public InheritableAttr {

 public:
  Win64Attr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::Win64, R, SI)
  {
  }

  virtual Win64Attr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::Win64; }
  virtual bool isLateParsed() const { return 0; }
};

class WorkGroupSizeHintAttr : public InheritableAttr {
unsigned xDim;

unsigned yDim;

unsigned zDim;


 public:
  WorkGroupSizeHintAttr(SourceRange R, ASTContext &Ctx
              , unsigned XDim
              , unsigned YDim
              , unsigned ZDim
              , unsigned SI = 0
             )
    : InheritableAttr(attr::WorkGroupSizeHint, R, SI)
              , xDim(XDim)
              , yDim(YDim)
              , zDim(ZDim)
  {



  }

  virtual WorkGroupSizeHintAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;
  unsigned getXDim() const {
    return xDim;
  }

  unsigned getYDim() const {
    return yDim;
  }

  unsigned getZDim() const {
    return zDim;
  }



  static bool classof(const Attr *A) { return A->getKind() == attr::WorkGroupSizeHint; }
  virtual bool isLateParsed() const { return 0; }
};

class X86ForceAlignArgPointerAttr : public InheritableAttr {

 public:
  X86ForceAlignArgPointerAttr(SourceRange R, ASTContext &Ctx
              , unsigned SI = 0
             )
    : InheritableAttr(attr::X86ForceAlignArgPointer, R, SI)
  {
  }

  virtual X86ForceAlignArgPointerAttr *clone (ASTContext &C) const;
  virtual void printPretty(raw_ostream &OS,
                           const PrintingPolicy &Policy) const;


  static bool classof(const Attr *A) { return A->getKind() == attr::X86ForceAlignArgPointer; }
  virtual bool isLateParsed() const { return 0; }
};

#endif
