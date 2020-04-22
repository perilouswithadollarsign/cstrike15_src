/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Attribute name matcher                                                      *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

static AttributeList::Kind getAttrKind(StringRef Name) {
  switch (Name.size()) {
  default: break;
  case 3:	 // 2 strings to match.
    switch (Name[0]) {
    default: break;
    case 'h':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ot", 2))
        break;
      return AttributeList::AT_Hot;	 // "hot"
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "cs", 2))
        break;
      return AttributeList::AT_Pcs;	 // "pcs"
    }
    break;
  case 4:	 // 7 strings to match.
    switch (Name[0]) {
    default: break;
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "old", 3))
        break;
      return AttributeList::AT_Cold;	 // "cold"
    case 'h':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ost", 3))
        break;
      return AttributeList::AT_CUDAHost;	 // "host"
    case 'm':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ode", 3))
        break;
      return AttributeList::AT_Mode;	 // "mode"
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ure", 3))
        break;
      return AttributeList::AT_Pure;	 // "pure"
    case 'u':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ed", 2))
          break;
        return AttributeList::AT_Used;	 // "used"
      case 'u':	 // 1 string to match.
        if (memcmp(Name.data()+2, "id", 2))
          break;
        return AttributeList::AT_Uuid;	 // "uuid"
      }
      break;
    case 'w':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eak", 3))
        break;
      return AttributeList::AT_Weak;	 // "weak"
    }
    break;
  case 5:	 // 6 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_w64", 4))
        break;
      return AttributeList::AT_Win64;	 // "__w64"
    case 'a':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "li", 2))
        break;
      switch (Name[3]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (Name[4] != 's')
          break;
        return AttributeList::AT_Alias;	 // "alias"
      case 'g':	 // 1 string to match.
        if (Name[4] != 'n')
          break;
        return AttributeList::AT_Aligned;	 // "align"
      }
      break;
    case 'c':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'd':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ecl", 3))
          break;
        return AttributeList::AT_CDecl;	 // "cdecl"
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+2, "nst", 3))
          break;
        return AttributeList::AT_Const;	 // "const"
      }
      break;
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "aked", 4))
        break;
      return AttributeList::AT_Naked;	 // "naked"
    }
    break;
  case 6:	 // 14 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "cdecl", 5))
        break;
      return AttributeList::AT_CDecl;	 // "_cdecl"
    case 'b':	 // 1 string to match.
      if (memcmp(Name.data()+1, "locks", 5))
        break;
      return AttributeList::AT_Blocks;	 // "blocks"
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ommon", 5))
        break;
      return AttributeList::AT_Common;	 // "common"
    case 'd':	 // 1 string to match.
      if (memcmp(Name.data()+1, "evice", 5))
        break;
      return AttributeList::AT_CUDADevice;	 // "device"
    case 'e':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ndian", 5))
        break;
      return AttributeList::AT_Endian;	 // "endian"
    case 'f':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ormat", 5))
        break;
      return AttributeList::AT_Format;	 // "format"
    case 'g':	 // 1 string to match.
      if (memcmp(Name.data()+1, "lobal", 5))
        break;
      return AttributeList::AT_CUDAGlobal;	 // "global"
    case 'k':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ernel", 5))
        break;
      return AttributeList::AT_OpenCLKernel;	 // "kernel"
    case 'm':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+2, "lloc", 4))
          break;
        return AttributeList::AT_Malloc;	 // "malloc"
      case 'i':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ps16", 4))
          break;
        return AttributeList::AT_Mips16;	 // "mips16"
      }
      break;
    case 'p':	 // 2 strings to match.
      if (Name[1] != 'a')
        break;
      switch (Name[2]) {
      default: break;
      case 'c':	 // 1 string to match.
        if (memcmp(Name.data()+3, "ked", 3))
          break;
        return AttributeList::AT_Packed;	 // "packed"
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+3, "cal", 3))
          break;
        return AttributeList::AT_Pascal;	 // "pascal"
      }
      break;
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "hared", 5))
        break;
      return AttributeList::AT_CUDAShared;	 // "shared"
    case 'u':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nused", 5))
        break;
      return AttributeList::AT_Unused;	 // "unused"
    }
    break;
  case 7:	 // 18 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 5 strings to match.
      switch (Name[1]) {
      default: break;
      case '_':	 // 4 strings to match.
        switch (Name[2]) {
        default: break;
        case 'c':	 // 2 strings to match.
          switch (Name[3]) {
          default: break;
          case 'd':	 // 1 string to match.
            if (memcmp(Name.data()+4, "ecl", 3))
              break;
            return AttributeList::AT_CDecl;	 // "__cdecl"
          case 'o':	 // 1 string to match.
            if (memcmp(Name.data()+4, "nst", 3))
              break;
            return AttributeList::AT_Const;	 // "__const"
          }
          break;
        case 'p':	 // 2 strings to match.
          if (memcmp(Name.data()+3, "tr", 2))
            break;
          switch (Name[5]) {
          default: break;
          case '3':	 // 1 string to match.
            if (Name[6] != '2')
              break;
            return AttributeList::AT_Ptr32;	 // "__ptr32"
          case '6':	 // 1 string to match.
            if (Name[6] != '4')
              break;
            return AttributeList::AT_Ptr64;	 // "__ptr64"
          }
          break;
        }
        break;
      case 'p':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ascal", 5))
          break;
        return AttributeList::AT_Pascal;	 // "_pascal"
      }
      break;
    case 'a':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "lign", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (Name[6] != 's')
          break;
        return AttributeList::AT_Aligned;	 // "alignas"
      case 'e':	 // 1 string to match.
        if (Name[6] != 'd')
          break;
        return AttributeList::AT_Aligned;	 // "aligned"
      }
      break;
    case 'b':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ounded", 6))
        break;
      return AttributeList::IgnoredAttribute;	 // "bounded"
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "leanup", 6))
        break;
      return AttributeList::AT_Cleanup;	 // "cleanup"
    case 'm':	 // 1 string to match.
      if (memcmp(Name.data()+1, "insize", 6))
        break;
      return AttributeList::AT_MinSize;	 // "minsize"
    case 'n':	 // 3 strings to match.
      if (Name[1] != 'o')
        break;
      switch (Name[2]) {
      default: break;
      case 'd':	 // 1 string to match.
        if (memcmp(Name.data()+3, "ebug", 4))
          break;
        return AttributeList::AT_NoDebug;	 // "nodebug"
      case 'n':	 // 1 string to match.
        if (memcmp(Name.data()+3, "null", 4))
          break;
        return AttributeList::AT_NonNull;	 // "nonnull"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+3, "hrow", 4))
          break;
        return AttributeList::AT_NoThrow;	 // "nothrow"
      }
      break;
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "bjc_gc", 6))
        break;
      return AttributeList::AT_ObjCGC;	 // "objc_gc"
    case 'r':	 // 1 string to match.
      if (memcmp(Name.data()+1, "egparm", 6))
        break;
      return AttributeList::AT_Regparm;	 // "regparm"
    case 's':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'e':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ction", 5))
          break;
        return AttributeList::AT_Section;	 // "section"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+2, "dcall", 5))
          break;
        return AttributeList::AT_StdCall;	 // "stdcall"
      }
      break;
    case 'w':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eakref", 6))
        break;
      return AttributeList::AT_WeakRef;	 // "weakref"
    }
    break;
  case 8:	 // 20 strings to match.
    switch (Name[0]) {
    default: break;
    case 'N':	 // 1 string to match.
      if (memcmp(Name.data()+1, "SObject", 7))
        break;
      return AttributeList::AT_ObjCNSObject;	 // "NSObject"
    case '_':	 // 4 strings to match.
      switch (Name[1]) {
      default: break;
      case 'A':	 // 1 string to match.
        if (memcmp(Name.data()+2, "lignas", 6))
          break;
        return AttributeList::AT_Aligned;	 // "_Alignas"
      case '_':	 // 2 strings to match.
        switch (Name[2]) {
        default: break;
        case 'k':	 // 1 string to match.
          if (memcmp(Name.data()+3, "ernel", 5))
            break;
          return AttributeList::AT_OpenCLKernel;	 // "__kernel"
        case 'p':	 // 1 string to match.
          if (memcmp(Name.data()+3, "ascal", 5))
            break;
          return AttributeList::AT_Pascal;	 // "__pascal"
        }
        break;
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+2, "tdcall", 6))
          break;
        return AttributeList::AT_StdCall;	 // "_stdcall"
      }
      break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nnotate", 7))
        break;
      return AttributeList::AT_Annotate;	 // "annotate"
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "onstant", 7))
        break;
      return AttributeList::AT_CUDAConstant;	 // "constant"
    case 'f':	 // 1 string to match.
      if (memcmp(Name.data()+1, "astcall", 7))
        break;
      return AttributeList::AT_FastCall;	 // "fastcall"
    case 'g':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'h':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ot", 2))
          break;
        return AttributeList::AT_Hot;	 // "gnu::hot"
      case 'p':	 // 1 string to match.
        if (memcmp(Name.data()+6, "cs", 2))
          break;
        return AttributeList::AT_Pcs;	 // "gnu::pcs"
      }
      break;
    case 'i':	 // 2 strings to match.
      if (Name[1] != 'b')
        break;
      switch (Name[2]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+3, "ction", 5))
          break;
        return AttributeList::AT_IBAction;	 // "ibaction"
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+3, "utlet", 5))
          break;
        return AttributeList::AT_IBOutlet;	 // "iboutlet"
      }
      break;
    case 'l':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ockable", 7))
        break;
      return AttributeList::AT_Lockable;	 // "lockable"
    case 'n':	 // 4 strings to match.
      if (Name[1] != 'o')
        break;
      switch (Name[2]) {
      default: break;
      case 'c':	 // 1 string to match.
        if (memcmp(Name.data()+3, "ommon", 5))
          break;
        return AttributeList::AT_NoCommon;	 // "nocommon"
      case 'i':	 // 1 string to match.
        if (memcmp(Name.data()+3, "nline", 5))
          break;
        return AttributeList::AT_NoInline;	 // "noinline"
      case 'm':	 // 1 string to match.
        if (memcmp(Name.data()+3, "ips16", 5))
          break;
        return AttributeList::AT_NoMips16;	 // "nomips16"
      case 'r':	 // 1 string to match.
        if (memcmp(Name.data()+3, "eturn", 5))
          break;
        return AttributeList::AT_NoReturn;	 // "noreturn"
      }
      break;
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "roperty", 7))
        break;
      return AttributeList::AT_MsProperty;	 // "property"
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "entinel", 7))
        break;
      return AttributeList::AT_Sentinel;	 // "sentinel"
    case 't':	 // 1 string to match.
      if (memcmp(Name.data()+1, "hiscall", 7))
        break;
      return AttributeList::AT_ThisCall;	 // "thiscall"
    }
    break;
  case 9:	 // 15 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 3 strings to match.
      switch (Name[1]) {
      default: break;
      case '_':	 // 1 string to match.
        if (memcmp(Name.data()+2, "stdcall", 7))
          break;
        return AttributeList::AT_StdCall;	 // "__stdcall"
      case 'f':	 // 1 string to match.
        if (memcmp(Name.data()+2, "astcall", 7))
          break;
        return AttributeList::AT_FastCall;	 // "_fastcall"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+2, "hiscall", 7))
          break;
        return AttributeList::AT_ThisCall;	 // "_thiscall"
      }
      break;
    case 'd':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "ll", 2))
        break;
      switch (Name[3]) {
      default: break;
      case 'e':	 // 1 string to match.
        if (memcmp(Name.data()+4, "xport", 5))
          break;
        return AttributeList::AT_DLLExport;	 // "dllexport"
      case 'i':	 // 1 string to match.
        if (memcmp(Name.data()+4, "mport", 5))
          break;
        return AttributeList::AT_DLLImport;	 // "dllimport"
      }
      break;
    case 'g':	 // 5 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'c':	 // 1 string to match.
        if (memcmp(Name.data()+6, "old", 3))
          break;
        return AttributeList::AT_Cold;	 // "gnu::cold"
      case 'm':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ode", 3))
          break;
        return AttributeList::AT_Mode;	 // "gnu::mode"
      case 'p':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ure", 3))
          break;
        return AttributeList::AT_Pure;	 // "gnu::pure"
      case 'u':	 // 1 string to match.
        if (memcmp(Name.data()+6, "sed", 3))
          break;
        return AttributeList::AT_Used;	 // "gnu::used"
      case 'w':	 // 1 string to match.
        if (memcmp(Name.data()+6, "eak", 3))
          break;
        return AttributeList::AT_Weak;	 // "gnu::weak"
      }
      break;
    case 'm':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+2, "y_alias", 7))
          break;
        return AttributeList::AT_MayAlias;	 // "may_alias"
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+2, "_struct", 7))
          break;
        return AttributeList::AT_MsStruct;	 // "ms_struct"
      }
      break;
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "naclcall", 8))
        break;
      return AttributeList::AT_PnaclCall;	 // "pnaclcall"
    case 't':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ls_model", 8))
        break;
      return AttributeList::AT_TLSModel;	 // "tls_model"
    case 'v':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ecreturn", 8))
        break;
      return AttributeList::AT_VecReturn;	 // "vecreturn"
    }
    break;
  case 10:	 // 15 strings to match.
    switch (Name[0]) {
    default: break;
    case ':':	 // 1 string to match.
      if (memcmp(Name.data()+1, ":noreturn", 9))
        break;
      return AttributeList::AT_CXX11NoReturn;	 // "::noreturn"
    case '_':	 // 2 strings to match.
      if (Name[1] != '_')
        break;
      switch (Name[2]) {
      default: break;
      case 'f':	 // 1 string to match.
        if (memcmp(Name.data()+3, "astcall", 7))
          break;
        return AttributeList::AT_FastCall;	 // "__fastcall"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+3, "hiscall", 7))
          break;
        return AttributeList::AT_ThisCall;	 // "__thiscall"
      }
      break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "lloc_size", 9))
        break;
      return AttributeList::AT_AllocSize;	 // "alloc_size"
    case 'd':	 // 2 strings to match.
      if (Name[1] != 'e')
        break;
      switch (Name[2]) {
      default: break;
      case 'p':	 // 1 string to match.
        if (memcmp(Name.data()+3, "recated", 7))
          break;
        return AttributeList::AT_Deprecated;	 // "deprecated"
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+3, "tructor", 7))
          break;
        return AttributeList::AT_Destructor;	 // "destructor"
      }
      break;
    case 'f':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ormat_arg", 9))
        break;
      return AttributeList::AT_FormatArg;	 // "format_arg"
    case 'g':	 // 6 strings to match.
      switch (Name[1]) {
      default: break;
      case 'n':	 // 5 strings to match.
        if (Name[2] != 'u')
          break;
        switch (Name[3]) {
        default: break;
        case ':':	 // 4 strings to match.
          if (Name[4] != ':')
            break;
          switch (Name[5]) {
          default: break;
          case 'a':	 // 1 string to match.
            if (memcmp(Name.data()+6, "lias", 4))
              break;
            return AttributeList::AT_Alias;	 // "gnu::alias"
          case 'c':	 // 2 strings to match.
            switch (Name[6]) {
            default: break;
            case 'd':	 // 1 string to match.
              if (memcmp(Name.data()+7, "ecl", 3))
                break;
              return AttributeList::AT_CDecl;	 // "gnu::cdecl"
            case 'o':	 // 1 string to match.
              if (memcmp(Name.data()+7, "nst", 3))
                break;
              return AttributeList::AT_Const;	 // "gnu::const"
            }
            break;
          case 'n':	 // 1 string to match.
            if (memcmp(Name.data()+6, "aked", 4))
              break;
            return AttributeList::AT_Naked;	 // "gnu::naked"
          }
          break;
        case '_':	 // 1 string to match.
          if (memcmp(Name.data()+4, "inline", 6))
            break;
          return AttributeList::AT_GNUInline;	 // "gnu_inline"
        }
        break;
      case 'u':	 // 1 string to match.
        if (memcmp(Name.data()+2, "arded_by", 8))
          break;
        return AttributeList::AT_GuardedBy;	 // "guarded_by"
      }
      break;
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "s_bridged", 9))
        break;
      return AttributeList::AT_NSBridged;	 // "ns_bridged"
    case 'v':	 // 1 string to match.
      if (memcmp(Name.data()+1, "isibility", 9))
        break;
      return AttributeList::AT_Visibility;	 // "visibility"
    }
    break;
  case 11:	 // 14 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_unaligned", 10))
        break;
      return AttributeList::IgnoredAttribute;	 // "__unaligned"
    case 'c':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'f':	 // 1 string to match.
        if (memcmp(Name.data()+2, "_consumed", 9))
          break;
        return AttributeList::AT_CFConsumed;	 // "cf_consumed"
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+2, "nstructor", 9))
          break;
        return AttributeList::AT_Constructor;	 // "constructor"
      }
      break;
    case 'g':	 // 7 strings to match.
      switch (Name[1]) {
      default: break;
      case 'n':	 // 6 strings to match.
        if (memcmp(Name.data()+2, "u::", 3))
          break;
        switch (Name[5]) {
        default: break;
        case 'c':	 // 1 string to match.
          if (memcmp(Name.data()+6, "ommon", 5))
            break;
          return AttributeList::AT_Common;	 // "gnu::common"
        case 'f':	 // 1 string to match.
          if (memcmp(Name.data()+6, "ormat", 5))
            break;
          return AttributeList::AT_Format;	 // "gnu::format"
        case 'm':	 // 2 strings to match.
          switch (Name[6]) {
          default: break;
          case 'a':	 // 1 string to match.
            if (memcmp(Name.data()+7, "lloc", 4))
              break;
            return AttributeList::AT_Malloc;	 // "gnu::malloc"
          case 'i':	 // 1 string to match.
            if (memcmp(Name.data()+7, "ps16", 4))
              break;
            return AttributeList::AT_Mips16;	 // "gnu::mips16"
          }
          break;
        case 'p':	 // 1 string to match.
          if (memcmp(Name.data()+6, "acked", 5))
            break;
          return AttributeList::AT_Packed;	 // "gnu::packed"
        case 'u':	 // 1 string to match.
          if (memcmp(Name.data()+6, "nused", 5))
            break;
          return AttributeList::AT_Unused;	 // "gnu::unused"
        }
        break;
      case 'u':	 // 1 string to match.
        if (memcmp(Name.data()+2, "arded_var", 9))
          break;
        return AttributeList::AT_GuardedVar;	 // "guarded_var"
      }
      break;
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "s_consumed", 10))
        break;
      return AttributeList::AT_NSConsumed;	 // "ns_consumed"
    case 'u':	 // 1 string to match.
      if (memcmp(Name.data()+1, "navailable", 10))
        break;
      return AttributeList::AT_Unavailable;	 // "unavailable"
    case 'v':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ector_size", 10))
        break;
      return AttributeList::AT_VectorSize;	 // "vector_size"
    case 'w':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eak_import", 10))
        break;
      return AttributeList::AT_WeakImport;	 // "weak_import"
    }
    break;
  case 12:	 // 10 strings to match.
    switch (Name[0]) {
    default: break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "vailability", 11))
        break;
      return AttributeList::AT_Availability;	 // "availability"
    case 'g':	 // 8 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ligned", 6))
          break;
        return AttributeList::AT_Aligned;	 // "gnu::aligned"
      case 'c':	 // 1 string to match.
        if (memcmp(Name.data()+6, "leanup", 6))
          break;
        return AttributeList::AT_Cleanup;	 // "gnu::cleanup"
      case 'n':	 // 2 strings to match.
        if (Name[6] != 'o')
          break;
        switch (Name[7]) {
        default: break;
        case 'n':	 // 1 string to match.
          if (memcmp(Name.data()+8, "null", 4))
            break;
          return AttributeList::AT_NonNull;	 // "gnu::nonnull"
        case 't':	 // 1 string to match.
          if (memcmp(Name.data()+8, "hrow", 4))
            break;
          return AttributeList::AT_NoThrow;	 // "gnu::nothrow"
        }
        break;
      case 'r':	 // 1 string to match.
        if (memcmp(Name.data()+6, "egparm", 6))
          break;
        return AttributeList::AT_Regparm;	 // "gnu::regparm"
      case 's':	 // 2 strings to match.
        switch (Name[6]) {
        default: break;
        case 'e':	 // 1 string to match.
          if (memcmp(Name.data()+7, "ction", 5))
            break;
          return AttributeList::AT_Section;	 // "gnu::section"
        case 't':	 // 1 string to match.
          if (memcmp(Name.data()+7, "dcall", 5))
            break;
          return AttributeList::AT_StdCall;	 // "gnu::stdcall"
        }
        break;
      case 'w':	 // 1 string to match.
        if (memcmp(Name.data()+6, "eakref", 6))
          break;
        return AttributeList::AT_WeakRef;	 // "gnu::weakref"
      }
      break;
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "verloadable", 11))
        break;
      return AttributeList::AT_Overloadable;	 // "overloadable"
    }
    break;
  case 13:	 // 17 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_forceinline", 12))
        break;
      return AttributeList::AT_ForceInline;	 // "__forceinline"
    case 'a':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'd':	 // 1 string to match.
        if (memcmp(Name.data()+2, "dress_space", 11))
          break;
        return AttributeList::AT_AddressSpace;	 // "address_space"
      case 'l':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ways_inline", 11))
          break;
        return AttributeList::AT_AlwaysInline;	 // "always_inline"
      }
      break;
    case 'g':	 // 7 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'f':	 // 1 string to match.
        if (memcmp(Name.data()+6, "astcall", 7))
          break;
        return AttributeList::AT_FastCall;	 // "gnu::fastcall"
      case 'n':	 // 4 strings to match.
        if (Name[6] != 'o')
          break;
        switch (Name[7]) {
        default: break;
        case 'c':	 // 1 string to match.
          if (memcmp(Name.data()+8, "ommon", 5))
            break;
          return AttributeList::AT_NoCommon;	 // "gnu::nocommon"
        case 'i':	 // 1 string to match.
          if (memcmp(Name.data()+8, "nline", 5))
            break;
          return AttributeList::AT_NoInline;	 // "gnu::noinline"
        case 'm':	 // 1 string to match.
          if (memcmp(Name.data()+8, "ips16", 5))
            break;
          return AttributeList::AT_NoMips16;	 // "gnu::nomips16"
        case 'r':	 // 1 string to match.
          if (memcmp(Name.data()+8, "eturn", 5))
            break;
          return AttributeList::AT_NoReturn;	 // "gnu::noreturn"
        }
        break;
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+6, "entinel", 7))
          break;
        return AttributeList::AT_Sentinel;	 // "gnu::sentinel"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+6, "hiscall", 7))
          break;
        return AttributeList::AT_ThisCall;	 // "gnu::thiscall"
      }
      break;
    case 'i':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nit_priority", 12))
        break;
      return AttributeList::AT_InitPriority;	 // "init_priority"
    case 'l':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+2, "unch_bounds", 11))
          break;
        return AttributeList::AT_CUDALaunchBounds;	 // "launch_bounds"
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ck_returned", 11))
          break;
        return AttributeList::AT_LockReturned;	 // "lock_returned"
      }
      break;
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "t_guarded_by", 12))
        break;
      return AttributeList::AT_PtGuardedBy;	 // "pt_guarded_by"
    case 'r':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eturns_twice", 12))
        break;
      return AttributeList::AT_ReturnsTwice;	 // "returns_twice"
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "td::noreturn", 12))
        break;
      return AttributeList::AT_CXX11NoReturn;	 // "std::noreturn"
    case 'v':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ec_type_hint", 12))
        break;
      return AttributeList::AT_VecTypeHint;	 // "vec_type_hint"
    }
    break;
  case 14:	 // 8 strings to match.
    switch (Name[0]) {
    default: break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "cquired_after", 13))
        break;
      return AttributeList::AT_AcquiredAfter;	 // "acquired_after"
    case 'g':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'm':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ay_alias", 8))
          break;
        return AttributeList::AT_MayAlias;	 // "gnu::may_alias"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ls_model", 8))
          break;
        return AttributeList::AT_TLSModel;	 // "gnu::tls_model"
      }
      break;
    case 'i':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ntel_ocl_bicc", 13))
        break;
      return AttributeList::AT_IntelOclBicc;	 // "intel_ocl_bicc"
    case 'l':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ocks_excluded", 13))
        break;
      return AttributeList::AT_LocksExcluded;	 // "locks_excluded"
    case 'o':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "bjc_", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'e':	 // 1 string to match.
        if (memcmp(Name.data()+6, "xception", 8))
          break;
        return AttributeList::AT_ObjCException;	 // "objc_exception"
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+6, "wnership", 8))
          break;
        return AttributeList::AT_ObjCOwnership;	 // "objc_ownership"
      }
      break;
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "t_guarded_var", 13))
        break;
      return AttributeList::AT_PtGuardedVar;	 // "pt_guarded_var"
    }
    break;
  case 15:	 // 14 strings to match.
    switch (Name[0]) {
    default: break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "cquired_before", 14))
        break;
      return AttributeList::AT_AcquiredBefore;	 // "acquired_before"
    case 'e':	 // 1 string to match.
      if (memcmp(Name.data()+1, "xt_vector_type", 14))
        break;
      return AttributeList::AT_ExtVectorType;	 // "ext_vector_type"
    case 'g':	 // 6 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+6, "lloc_size", 9))
          break;
        return AttributeList::AT_AllocSize;	 // "gnu::alloc_size"
      case 'd':	 // 2 strings to match.
        if (Name[6] != 'e')
          break;
        switch (Name[7]) {
        default: break;
        case 'p':	 // 1 string to match.
          if (memcmp(Name.data()+8, "recated", 7))
            break;
          return AttributeList::AT_Deprecated;	 // "gnu::deprecated"
        case 's':	 // 1 string to match.
          if (memcmp(Name.data()+8, "tructor", 7))
            break;
          return AttributeList::AT_Destructor;	 // "gnu::destructor"
        }
        break;
      case 'f':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ormat_arg", 9))
          break;
        return AttributeList::AT_FormatArg;	 // "gnu::format_arg"
      case 'g':	 // 1 string to match.
        if (memcmp(Name.data()+6, "nu_inline", 9))
          break;
        return AttributeList::AT_GNUInline;	 // "gnu::gnu_inline"
      case 'v':	 // 1 string to match.
        if (memcmp(Name.data()+6, "isibility", 9))
          break;
        return AttributeList::AT_Visibility;	 // "gnu::visibility"
      }
      break;
    case 'o':	 // 3 strings to match.
      switch (Name[1]) {
      default: break;
      case 'b':	 // 1 string to match.
        if (memcmp(Name.data()+2, "jc_root_class", 13))
          break;
        return AttributeList::AT_ObjCRootClass;	 // "objc_root_class"
      case 'w':	 // 2 strings to match.
        if (memcmp(Name.data()+2, "nership_", 8))
          break;
        switch (Name[10]) {
        default: break;
        case 'h':	 // 1 string to match.
          if (memcmp(Name.data()+11, "olds", 4))
            break;
          return AttributeList::AT_ownership_holds;	 // "ownership_holds"
        case 't':	 // 1 string to match.
          if (memcmp(Name.data()+11, "akes", 4))
            break;
          return AttributeList::AT_ownership_takes;	 // "ownership_takes"
        }
        break;
      }
      break;
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "coped_lockable", 14))
        break;
      return AttributeList::AT_ScopedLockable;	 // "scoped_lockable"
    case 't':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ype_visibility", 14))
        break;
      return AttributeList::AT_TypeVisibility;	 // "type_visibility"
    case 'u':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nlock_function", 14))
        break;
      return AttributeList::AT_UnlockFunction;	 // "unlock_function"
    }
    break;
  case 16:	 // 4 strings to match.
    switch (Name[0]) {
    default: break;
    case 'g':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'c':	 // 1 string to match.
        if (memcmp(Name.data()+6, "onstructor", 10))
          break;
        return AttributeList::AT_Constructor;	 // "gnu::constructor"
      case 'v':	 // 1 string to match.
        if (memcmp(Name.data()+6, "ector_size", 10))
          break;
        return AttributeList::AT_VectorSize;	 // "gnu::vector_size"
      }
      break;
    case 'n':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'e':	 // 1 string to match.
        if (memcmp(Name.data()+2, "on_vector_type", 14))
          break;
        return AttributeList::AT_NeonVectorType;	 // "neon_vector_type"
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+2, "_consumes_self", 14))
          break;
        return AttributeList::AT_NSConsumesSelf;	 // "ns_consumes_self"
      }
      break;
    }
    break;
  case 17:	 // 3 strings to match.
    switch (Name[0]) {
    default: break;
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nalyzer_noreturn", 16))
        break;
      return AttributeList::AT_AnalyzerNoReturn;	 // "analyzer_noreturn"
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "wnership_returns", 16))
        break;
      return AttributeList::AT_ownership_returns;	 // "ownership_returns"
    case 't':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ransparent_union", 16))
        break;
      return AttributeList::AT_TransparentUnion;	 // "transparent_union"
    }
    break;
  case 18:	 // 9 strings to match.
    switch (Name[0]) {
    default: break;
    case 'c':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+2, "rries_dependency", 16))
          break;
        return AttributeList::AT_CarriesDependency;	 // "carries_dependency"
      case 'l':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ang::fallthrough", 16))
          break;
        return AttributeList::AT_FallThrough;	 // "clang::fallthrough"
      }
      break;
    case 'g':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "nu::", 4))
        break;
      switch (Name[5]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+6, "lways_inline", 12))
          break;
        return AttributeList::AT_AlwaysInline;	 // "gnu::always_inline"
      case 'r':	 // 1 string to match.
        if (memcmp(Name.data()+6, "eturns_twice", 12))
          break;
        return AttributeList::AT_ReturnsTwice;	 // "gnu::returns_twice"
      }
      break;
    case 'i':	 // 1 string to match.
      if (memcmp(Name.data()+1, "boutletcollection", 17))
        break;
      return AttributeList::AT_IBOutletCollection;	 // "iboutletcollection"
    case 'n':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "o_sanitize_", 11))
        break;
      switch (Name[12]) {
      default: break;
      case 'm':	 // 1 string to match.
        if (memcmp(Name.data()+13, "emory", 5))
          break;
        return AttributeList::AT_NoSanitizeMemory;	 // "no_sanitize_memory"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+13, "hread", 5))
          break;
        return AttributeList::AT_NoSanitizeThread;	 // "no_sanitize_thread"
      }
      break;
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "bjc_method_family", 17))
        break;
      return AttributeList::AT_ObjCMethodFamily;	 // "objc_method_family"
    case 'w':	 // 1 string to match.
      if (memcmp(Name.data()+1, "arn_unused_result", 17))
        break;
      return AttributeList::AT_WarnUnusedResult;	 // "warn_unused_result"
    }
    break;
  case 19:	 // 7 strings to match.
    switch (Name[0]) {
    default: break;
    case 'c':	 // 3 strings to match.
      if (memcmp(Name.data()+1, "f_", 2))
        break;
      switch (Name[3]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+4, "udited_transfer", 15))
          break;
        return AttributeList::AT_CFAuditedTransfer;	 // "cf_audited_transfer"
      case 'r':	 // 1 string to match.
        if (memcmp(Name.data()+4, "eturns_retained", 15))
          break;
        return AttributeList::AT_CFReturnsRetained;	 // "cf_returns_retained"
      case 'u':	 // 1 string to match.
        if (memcmp(Name.data()+4, "nknown_transfer", 15))
          break;
        return AttributeList::AT_CFUnknownTransfer;	 // "cf_unknown_transfer"
      }
      break;
    case 'n':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'o':	 // 1 string to match.
        if (memcmp(Name.data()+2, "_sanitize_address", 17))
          break;
        return AttributeList::AT_NoSanitizeAddress;	 // "no_sanitize_address"
      case 's':	 // 1 string to match.
        if (memcmp(Name.data()+2, "_returns_retained", 17))
          break;
        return AttributeList::AT_NSReturnsRetained;	 // "ns_returns_retained"
      }
      break;
    case 'o':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'b':	 // 1 string to match.
        if (memcmp(Name.data()+2, "jc_requires_super", 17))
          break;
        return AttributeList::AT_ObjCRequiresSuper;	 // "objc_requires_super"
      case 'p':	 // 1 string to match.
        if (memcmp(Name.data()+2, "encl_image_access", 17))
          break;
        return AttributeList::AT_OpenCLImageAccess;	 // "opencl_image_access"
      }
      break;
    }
    break;
  case 20:	 // 6 strings to match.
    switch (Name[0]) {
    default: break;
    case ':':	 // 1 string to match.
      if (memcmp(Name.data()+1, ":carries_dependency", 19))
        break;
      return AttributeList::AT_CarriesDependency;	 // "::carries_dependency"
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_single_inheritance", 19))
        break;
      return AttributeList::AT_SingleInheritance;	 // "__single_inheritance"
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eon_polyvector_type", 19))
        break;
      return AttributeList::AT_NeonPolyVectorType;	 // "neon_polyvector_type"
    case 'r':	 // 1 string to match.
      if (memcmp(Name.data()+1, "eqd_work_group_size", 19))
        break;
      return AttributeList::AT_ReqdWorkGroupSize;	 // "reqd_work_group_size"
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "hared_lock_function", 19))
        break;
      return AttributeList::AT_SharedLockFunction;	 // "shared_lock_function"
    case 'w':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ork_group_size_hint", 19))
        break;
      return AttributeList::AT_WorkGroupSizeHint;	 // "work_group_size_hint"
    }
    break;
  case 21:	 // 5 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_virtual_inheritance", 20))
        break;
      return AttributeList::AT_VirtualInheritance;	 // "__virtual_inheritance"
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "bjc_precise_lifetime", 20))
        break;
      return AttributeList::AT_ObjCPreciseLifetime;	 // "objc_precise_lifetime"
    case 'p':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ointer_with_type_tag", 20))
        break;
      return AttributeList::AT_ArgumentWithTypeTag;	 // "pointer_with_type_tag"
    case 's':	 // 1 string to match.
      if (memcmp(Name.data()+1, "hared_locks_required", 20))
        break;
      return AttributeList::AT_SharedLocksRequired;	 // "shared_locks_required"
    case 't':	 // 1 string to match.
      if (memcmp(Name.data()+1, "ype_tag_for_datatype", 20))
        break;
      return AttributeList::AT_TypeTagForDatatype;	 // "type_tag_for_datatype"
    }
    break;
  case 22:	 // 5 strings to match.
    switch (Name[0]) {
    default: break;
    case '_':	 // 1 string to match.
      if (memcmp(Name.data()+1, "_multiple_inheritance", 21))
        break;
      return AttributeList::AT_MultipleInheritance;	 // "__multiple_inheritance"
    case 'a':	 // 1 string to match.
      if (memcmp(Name.data()+1, "rgument_with_type_tag", 21))
        break;
      return AttributeList::AT_ArgumentWithTypeTag;	 // "argument_with_type_tag"
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "lang::type_visibility", 21))
        break;
      return AttributeList::AT_TypeVisibility;	 // "clang::type_visibility"
    case 'g':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nu::transparent_union", 21))
        break;
      return AttributeList::AT_TransparentUnion;	 // "gnu::transparent_union"
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "o_instrument_function", 21))
        break;
      return AttributeList::AT_NoInstrumentFunction;	 // "no_instrument_function"
    }
    break;
  case 23:	 // 7 strings to match.
    switch (Name[0]) {
    default: break;
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "f_returns_not_retained", 22))
        break;
      return AttributeList::AT_CFReturnsNotRetained;	 // "cf_returns_not_retained"
    case 'e':	 // 1 string to match.
      if (memcmp(Name.data()+1, "xclusive_lock_function", 22))
        break;
      return AttributeList::AT_ExclusiveLockFunction;	 // "exclusive_lock_function"
    case 'g':	 // 1 string to match.
      if (memcmp(Name.data()+1, "nu::warn_unused_result", 22))
        break;
      return AttributeList::AT_WarnUnusedResult;	 // "gnu::warn_unused_result"
    case 'n':	 // 2 strings to match.
      if (memcmp(Name.data()+1, "s_returns_", 10))
        break;
      switch (Name[11]) {
      default: break;
      case 'a':	 // 1 string to match.
        if (memcmp(Name.data()+12, "utoreleased", 11))
          break;
        return AttributeList::AT_NSReturnsAutoreleased;	 // "ns_returns_autoreleased"
      case 'n':	 // 1 string to match.
        if (memcmp(Name.data()+12, "ot_retained", 11))
          break;
        return AttributeList::AT_NSReturnsNotRetained;	 // "ns_returns_not_retained"
      }
      break;
    case 's':	 // 2 strings to match.
      switch (Name[1]) {
      default: break;
      case 'h':	 // 1 string to match.
        if (memcmp(Name.data()+2, "ared_trylock_function", 21))
          break;
        return AttributeList::AT_SharedTrylockFunction;	 // "shared_trylock_function"
      case 't':	 // 1 string to match.
        if (memcmp(Name.data()+2, "d::carries_dependency", 21))
          break;
        return AttributeList::AT_CarriesDependency;	 // "std::carries_dependency"
      }
      break;
    }
    break;
  case 24:	 // 1 string to match.
    if (memcmp(Name.data()+0, "exclusive_locks_required", 24))
      break;
    return AttributeList::AT_ExclusiveLocksRequired;	 // "exclusive_locks_required"
  case 25:	 // 2 strings to match.
    switch (Name[0]) {
    default: break;
    case 'c':	 // 1 string to match.
      if (memcmp(Name.data()+1, "lang::warn_unused_result", 24))
        break;
      return AttributeList::AT_WarnUnusedResult;	 // "clang::warn_unused_result"
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "o_thread_safety_analysis", 24))
        break;
      return AttributeList::AT_NoThreadSafetyAnalysis;	 // "no_thread_safety_analysis"
    }
    break;
  case 26:	 // 3 strings to match.
    switch (Name[0]) {
    default: break;
    case 'e':	 // 1 string to match.
      if (memcmp(Name.data()+1, "xclusive_trylock_function", 25))
        break;
      return AttributeList::AT_ExclusiveTrylockFunction;	 // "exclusive_trylock_function"
    case 'n':	 // 1 string to match.
      if (memcmp(Name.data()+1, "o_address_safety_analysis", 25))
        break;
      return AttributeList::AT_NoSanitizeAddress;	 // "no_address_safety_analysis"
    case 'o':	 // 1 string to match.
      if (memcmp(Name.data()+1, "bjc_returns_inner_pointer", 25))
        break;
      return AttributeList::AT_ObjCReturnsInnerPointer;	 // "objc_returns_inner_pointer"
    }
    break;
  case 27:	 // 1 string to match.
    if (memcmp(Name.data()+0, "gnu::no_instrument_function", 27))
      break;
    return AttributeList::AT_NoInstrumentFunction;	 // "gnu::no_instrument_function"
  case 34:	 // 1 string to match.
    if (memcmp(Name.data()+0, "objc_requires_property_definitions", 34))
      break;
    return AttributeList::AT_ObjCRequiresPropertyDefs;	 // "objc_requires_property_definitions"
  case 35:	 // 1 string to match.
    if (memcmp(Name.data()+0, "objc_arc_weak_reference_unavailable", 35))
      break;
    return AttributeList::AT_ArcWeakrefUnavailable;	 // "objc_arc_weak_reference_unavailable"
  }
return AttributeList::UnknownAttribute;
}
