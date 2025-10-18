
// elf0.h

// >>> see codebrowser.dev/gcc/include/elf.h.html

typedef uint2 Elf64_Half;
typedef uint2 Elf64_Section;
typedef uint4 Elf64_Word;
typedef int8  Elf64_Xword;
typedef int8  Elf64_Sxword;
typedef int8  Elf64_Addr;
typedef int8  Elf64_Off;

//---------------------------------------------------------------------
// Program segment header
//---------------------------------------------------------------------

packed struct Elf64_Phdr
{
  Elf64_Word    p_type;    /* Segment type */
  Elf64_Word    p_flags;   /* Segment flags */
  Elf64_Off     p_offset;  /* Segment file offset - Offset of the segment in the file image */
  Elf64_Addr    p_vaddr;   /* Segment virtual address - Virtual address of the segment in memory */
  Elf64_Addr    p_paddr;   /* Segment physical address - On systems where physical address is relevant */
  Elf64_Xword   p_filesz;  /* Segment size in file */
  Elf64_Xword   p_memsz;   /* Segment size in memory */
  Elf64_Xword   p_align;   /* Segment alignment - 0 and 1 specify no alignment. Otherwise should be a positive, integral power of 2, with p_vaddr equating p_offset modulus p_align.*/
}

/* Legal values for p_type (segment type) */
const uint4 PT_NULL           =  0;           /* Program header table entry unused */
const uint4 PT_LOAD           =  1;           /* Loadable program segment */
const uint4 PT_DYNAMIC        =  2;           /* Dynamic linking information */
const uint4 PT_INTERP         =  3;           /* Program interpreter */
const uint4 PT_NOTE           =  4;           /* Auxiliary information */
const uint4 PT_SHLIB          =  5;           /* Reserved */
const uint4 PT_PHDR           =  6;           /* Entry for Program segment header table itself */
const uint4 PT_TLS            =  7;           /* Thread-local storage segment */
const uint4 PT_GNU_EH_FRAME   =  0x6474e550;  /* GCC .eh_frame_hdr segment */
const uint4 PT_GNU_STACK      =  0x6474e551;  /* Indicates stack executability */
const uint4 PT_GNU_RELRO      =  0x6474e552;  /* Read-only after relocation */
const uint4 PT_GNU_PROPERTY   =  0x6474e553;  /* GNU property */
const uint4 PT_LOSUNW         =  0x6ffffffa;
const uint4 PT_SUNWBSS        =  0x6ffffffa;  /* Sun Specific segment */
const uint4 PT_SUNWSTACK      =  0x6ffffffb;  /* Stack segment */
const uint4 PT_HISUNW         =  0x6fffffff;

/* Legal values for p_flags (segment flags).  */
const uint4 PF_X        =  (1 << 0);        /* Segment is executable (1) */
const uint4 PF_W        =  (1 << 1);        /* Segment is writable (2) */
const uint4 PF_R        =  (1 << 2);        /* Segment is readable (4) */

//---------------------------------------------------------------------
// Section Header
//---------------------------------------------------------------------

packed struct Elf64_Shdr  // 64 bytes
{
  Elf64_Word     sh_name;        /* Section name (string tbl index) */
  Elf64_Word     sh_type;        /* Section type */
  Elf64_Xword    sh_flags;       /* Section flags */
  Elf64_Addr     sh_addr;        /* Section virtual addr at execution */
  Elf64_Off      sh_offset;      /* Section file offset */
  Elf64_Xword    sh_size;        /* Section size in the file, in bytes. May be 0. */
  Elf64_Word     sh_link;        /* Link to another section */
  Elf64_Word     sh_info;        /* Additional section information (low=type, high=bind) */
  Elf64_Xword    sh_addralign;   /* Section alignment. This field must be a power of two. */
  Elf64_Xword    sh_entsize;     /* Entry size if section holds table. Otherwise zero. */
}

// values for .sh_type
const Elf64_Word SHT_NULL         =  0;                /* Section header table entry unused */
const Elf64_Word SHT_PROGBITS     =  1;                /* Program data */
const Elf64_Word SHT_SYMTAB       =  2;                /* Symbol table */
const Elf64_Word SHT_STRTAB       =  3;                /* String table */
const Elf64_Word SHT_RELA         =  4;                /* Relocation entries with addends */
const Elf64_Word SHT_HASH         =  5;                /* Symbol hash table */
const Elf64_Word SHT_DYNAMIC      =  6;                /* Dynamic linking information */
const Elf64_Word SHT_NOTE         =  7;                /* Notes */
const Elf64_Word SHT_NOBITS       =  8;                /* Program space with no data (bss) */
const Elf64_Word SHT_REL          =  9;                /* Relocation entries, no addends */
const Elf64_Word SHT_SHLIB        = 10;                /* Reserved */
const Elf64_Word SHT_DYNSYM       = 11;                /* Dynamic linker symbol table */
const Elf64_Word SHT_INIT_ARRAY   = 14;                /* Array of constructors */
const Elf64_Word SHT_FINI_ARRAY   = 15;                /* Array of destructors */
const Elf64_Word SHT_PREINIT_ARRAY= 16;                /* Array of pre-constructors */
const Elf64_Word SHT_GROUP        = 17;                /* Section group */
const Elf64_Word SHT_SYMTAB_SHNDX = 18;                /* Extended section indices */
const Elf64_Word SHT_NUM          = 19;                /* Number of defined types.  */
const Elf64_Word SHT_LOOS           = 0x60000000;        /* Start OS-specific.  */
const Elf64_Word SHT_GNU_ATTRIBUTES = 0x6ffffff5;        /* Object attributes.  */
const Elf64_Word SHT_GNU_HASH       = 0x6ffffff6;        /* GNU-style hash table.  */
const Elf64_Word SHT_GNU_LIBLIST    = 0x6ffffff7;        /* Prelink library list */
const Elf64_Word SHT_CHECKSUM       = 0x6ffffff8;        /* Checksum for DSO content.  */
const Elf64_Word SHT_LOSUNW         = 0x6ffffffa;        /* Sun-specific low bound.  */
const Elf64_Word SHT_SUNW_move      = 0x6ffffffa;
const Elf64_Word SHT_SUNW_COMDAT    = 0x6ffffffb;
const Elf64_Word SHT_SUNW_syminfo   = 0x6ffffffc;
const Elf64_Word SHT_GNU_verdef     = 0x6ffffffd;        /* Version definition section.  */
const Elf64_Word SHT_GNU_verneed    = 0x6ffffffe;        /* Version needs section.  */
const Elf64_Word SHT_GNU_versym     = 0x6fffffff;        /* Version symbol table.  */

// values for .sh_flags
const Elf64_Xword SHF_WRITE            = (1 << 0);        /* Writable */
const Elf64_Xword SHF_ALLOC            = (1 << 1);        /* Occupies memory during execution */
const Elf64_Xword SHF_EXECINSTR        = (1 << 2);        /* Executable */
const Elf64_Xword SHF_MERGE            = (1 << 4);        /* Might be merged */
const Elf64_Xword SHF_STRINGS          = (1 << 5);        /* Contains nul-terminated strings */
const Elf64_Xword SHF_INFO_LINK        = (1 << 6);        /* `sh_info' contains SHT index */
const Elf64_Xword SHF_LINK_ORDER       = (1 << 7);        /* Preserve order after combining */
const Elf64_Xword SHF_OS_NONCONFORMING = (1 << 8);        /* Non-standard OS specific handling required */
const Elf64_Xword SHF_GROUP            = (1 << 9);        /* Section is member of a group.  */
const Elf64_Xword SHF_TLS              = (1 << 10);       /* Section hold thread-local data.  */
const Elf64_Xword SHF_COMPRESSED       = (1 << 11);       /* Section with compressed data. */
const Elf64_Xword SHF_MASKOS           =  0x0ff00000;     /* OS-specific.  */
const Elf64_Xword SHF_MASKPROC         =  0xf0000000;     /* Processor-specific */
const Elf64_Xword SHF_GNU_RETAIN       = (1 << 21);       /* Not to be GCed by linker.  */
const Elf64_Xword SHF_ORDERED          = (1 << 30);       /* Special ordering requirement (Solaris).  */
const Elf64_Xword SHF_EXCLUDE          = (1 << 31);      /* Section is excluded unless referenced or allocated (Solaris).*/

//---------------------------------------------------------------------
// ELF Header
//---------------------------------------------------------------------

packed struct Elf64_Ehdr
{
  byte[16]    e_ident;    /* Magic number and other info (7F 45 4C 46 02=64bit 01=LSB 01=version 00 00 00 00 00 00 00 00 00) */
  Elf64_Half  e_type;     /* Object file type (2=exe, 3=shared lib) */
  Elf64_Half  e_machine;  /* Architecture (android EM_AARCH64 = B7) */
  Elf64_Word  e_version;  /* Object file version (=1) */
  Elf64_Addr  e_entry;    /* Entry point virtual address (=0 for shared lib) */
  Elf64_Off   e_phoff;    /* Program header table file offset */
  Elf64_Off   e_shoff;    /* Section header table file offset */
  Elf64_Word  e_flags;    /* Processor-specific flags (=0) */
  Elf64_Half  e_ehsize;   /* ELF header size in bytes (=sizeof(Elf64_Ehdr) */
  Elf64_Half  e_phentsize;/* Program header table entry size (=sizeof(Elf64_Phdr)) */
  Elf64_Half  e_phnum;    /* Program header table entry count */
  Elf64_Half  e_shentsize;/* Section header table entry size (=sizeof(Elf64_Shdr)) */
  Elf64_Half  e_shnum;    /* Section header table entry count */
  Elf64_Half  e_shstrndx; /* Section header string table index */
}

//---------------------------------------------------------------------
// section 0x0B SHT_DYNSYM  Dynamic linker symbol table
//---------------------------------------------------------------------

// list of imported & exported symbols

packed struct Elf64_Sym  // 24 bytes
{
  Elf64_Word      st_name;   /* Symbol name (string tbl index) */  // 4 bytes
  byte            st_info;   /* Symbol type and binding */         // 1 (ST_BIND = high nybble, ST_TYPE = low nybble)
  byte            st_other;  /* Symbol visibility */               // 1
  Elf64_Section   st_shndx;  /* Section index */                   // 2 bytes
  Elf64_Addr      st_value;  /* Symbol value */                    // 8 bytes
  Elf64_Xword     st_size;   /* Symbol size */                     // 8 bytes
}

/* Legal values for ST_BIND subfield of st_info (symbol binding) (high nybble of .st_info) */
const byte STB_LOCAL      =  0;                /* Local symbol */
const byte STB_GLOBAL     =  1;                /* Global symbol */
const byte STB_WEAK       =  2;                /* Weak symbol */
const byte STB_GNU_UNIQUE = 10;                /* Unique symbol */

/* Legal values for ST_TYPE subfield of st_info (symbol type) (low nybble of .st_info) */
const byte STT_NOTYPE     =  0;                /* Symbol type is unspecified */
const byte STT_OBJECT     =  1;                /* Symbol is a data object */
const byte STT_FUNC       =  2;                /* Symbol is a code object */
const byte STT_SECTION    =  3;                /* Symbol associated with a section */
const byte STT_FILE       =  4;                /* Symbol's name is file name */
const byte STT_COMMON     =  5;                /* Symbol is a common data object */
const byte STT_TLS        =  6;                /* Symbol is thread-local data object*/
const byte STT_GNU_IFUNC  = 10;                /* Symbol is indirect code object */

//---------------------------------------------------------------------
// section 0x4 SHT_RELA Relocation entries with addends
//---------------------------------------------------------------------

packed struct Elf64_Rela
{
  Elf64_Addr    r_offset;  // Address
  Elf64_Word    r_type;    // relocation type
  Elf64_Word    r_sym;     // symbol index
  Elf64_Sxword  r_addend;  // Addend
}

/* .r_type */
const uint4 R_AARCH64_COPY       =  1024;    /* Copy symbol at runtime.  */
const uint4 R_AARCH64_GLOB_DAT   =  1025;    /* Create GOT entry. for exported global variables */
const uint4 R_AARCH64_JUMP_SLOT  =  1026;    /* Create PLT entry. for imported functions */
const uint4 R_AARCH64_RELATIVE   =  1027;    /* Adjust by program base.  */
const uint4 R_AARCH64_TLS_DTPMOD =  1028;    /* Module number, 64 bit.  */
const uint4 R_AARCH64_TLS_DTPREL =  1029;    /* Module-relative offset, 64 bit.  */
const uint4 R_AARCH64_TLS_TPREL  =  1030;    /* TP-relative offset, 64 bit.  */
const uint4 R_AARCH64_TLSDESC    =  1031;    /* TLS Descriptor.  */
const uint4 R_AARCH64_IRELATIVE  =  1032;    /* STT_GNU_IFUNC relocation.  */

//---------------------------------------------------------------------
// program header 5 0x00000002  PT_DYNAMIC  Dynamic linking information.
//---------------------------------------------------------------------

packed struct Elf64_Dyn
{
  Elf64_Sxword d_tag;   /* Dynamic entry type */
  Elf64_Xword  d_val;   /* value */
}

// Legal values for d_tag
const Elf64_Sxword DT_NULL         =  0;                /* Marks end of dynamic section */
const Elf64_Sxword DT_NEEDED       =  1;                /* Name of needed library */
const Elf64_Sxword DT_PLTRELSZ     =  2;                /* Size in bytes of PLT relocs */
const Elf64_Sxword DT_PLTGOT       =  3;                /* Processor defined value */
const Elf64_Sxword DT_HASH         =  4;                /* Address of symbol hash table */
const Elf64_Sxword DT_STRTAB       =  5;                /* Address of string table */
const Elf64_Sxword DT_SYMTAB       =  6;                /* Address of symbol table */
const Elf64_Sxword DT_RELA         =  7;                /* Address of Rela relocs */
const Elf64_Sxword DT_RELASZ       =  8;                /* Total size of Rela relocs */
const Elf64_Sxword DT_RELAENT      =  9;                /* Size of one Rela reloc */
const Elf64_Sxword DT_STRSZ        = 10;                /* Size of string table */
const Elf64_Sxword DT_SYMENT       = 11;                /* Size of one symbol table entry */
const Elf64_Sxword DT_INIT         = 12;                /* Address of init function */
const Elf64_Sxword DT_FINI         = 13;                /* Address of termination function */
const Elf64_Sxword DT_SONAME       = 14;                /* Name of shared object */
const Elf64_Sxword DT_RPATH        = 15;                /* Library search path (deprecated) */
const Elf64_Sxword DT_SYMBOLIC     = 16;                /* Start symbol search here */
const Elf64_Sxword DT_REL          = 17;                /* Address of Rel relocs */
const Elf64_Sxword DT_RELSZ        = 18;                /* Total size of Rel relocs */
const Elf64_Sxword DT_RELENT       = 19;                /* Size of one Rel reloc */
const Elf64_Sxword DT_PLTREL       = 20;                /* Type of reloc in PLT */
const Elf64_Sxword DT_DEBUG        = 21;                /* For debugging; unspecified */
const Elf64_Sxword DT_TEXTREL      = 22;                /* Reloc might modify .text */
const Elf64_Sxword DT_JMPREL       = 23;                /* Address of PLT relocs */
const Elf64_Sxword DT_BIND_NOW     = 24;                /* Process relocations of object */
const Elf64_Sxword DT_INIT_ARRAY   = 25;                /* Array with addresses of init fct */
const Elf64_Sxword DT_FINI_ARRAY   = 26;                /* Array with addresses of fini fct */
const Elf64_Sxword DT_INIT_ARRAYSZ = 27;                /* Size in bytes of DT_INIT_ARRAY */
const Elf64_Sxword DT_FINI_ARRAYSZ = 28;                /* Size in bytes of DT_FINI_ARRAY */
const Elf64_Sxword DT_RUNPATH      = 29;                /* Library search path */
const Elf64_Sxword DT_FLAGS        = 30;                /* Flags for the object being loaded */
const Elf64_Sxword DT_ENCODING     = 32;                /* Start of encoded range */
const Elf64_Sxword DT_PREINIT_ARRAY   =32;                /* Array with addresses of preinit fct*/
const Elf64_Sxword DT_PREINIT_ARRAYSZ =33;                /* size in bytes of DT_PREINIT_ARRAY */
const Elf64_Sxword DT_SYMTAB_SHNDX    =34;                /* Address of SYMTAB_SHNDX section */
const Elf64_Sxword DT_VALRNGLO        =0x6ffffd00;
const Elf64_Sxword DT_GNU_PRELINKED   =0x6ffffdf5;        /* Prelinking timestamp */
const Elf64_Sxword DT_GNU_CONFLICTSZ  =0x6ffffdf6;        /* Size of conflict section */
const Elf64_Sxword DT_GNU_LIBLISTSZ   =0x6ffffdf7;        /* Size of library list */
const Elf64_Sxword DT_CHECKSUM        =0x6ffffdf8;
const Elf64_Sxword DT_PLTPADSZ        =0x6ffffdf9;
const Elf64_Sxword DT_MOVEENT         =0x6ffffdfa;
const Elf64_Sxword DT_MOVESZ          =0x6ffffdfb;
const Elf64_Sxword DT_FEATURE_1       =0x6ffffdfc;       /* Feature selection (DTF_*).  */
const Elf64_Sxword DT_POSFLAG_1       =0x6ffffdfd;       /* Flags for DT_* entries, effecting the following DT_* entry. */
const Elf64_Sxword DT_SYMINSZ         =0x6ffffdfe;       /* Size of syminfo table (in bytes) */
const Elf64_Sxword DT_SYMINENT        =0x6ffffdff;       /* Entry size of syminfo */
const Elf64_Sxword DT_VALRNGHI        =0x6ffffdff;
//const Elf64_Sxword DT_VALTAGIDX(tag)  (DT_VALRNGHI - (tag))        /* Reverse order! */
//const Elf64_Sxword DT_VALNUM      12
/* DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
   Dyn.d_un.d_ptr field of the Elf*_Dyn structure.
   If any adjustment is made to the ELF object after it has been
   built these entries will need to be adjusted.  */
const Elf64_Sxword DT_ADDRRNGLO      = 0x6ffffe00;
const Elf64_Sxword DT_GNU_HASH       = 0x6ffffef5;        /* GNU-style hash table.  */
const Elf64_Sxword DT_TLSDESC_PLT    = 0x6ffffef6;
const Elf64_Sxword DT_TLSDESC_GOT    = 0x6ffffef7;
const Elf64_Sxword DT_GNU_CONFLICT   = 0x6ffffef8;        /* Start of conflict section */
const Elf64_Sxword DT_GNU_LIBLIST    = 0x6ffffef9;        /* Library list */
const Elf64_Sxword DT_CONFIG         = 0x6ffffefa;        /* Configuration information.  */
const Elf64_Sxword DT_DEPAUDIT       = 0x6ffffefb;        /* Dependency auditing.  */
const Elf64_Sxword DT_AUDIT          = 0x6ffffefc;        /* Object auditing.  */
const Elf64_Sxword DT_PLTPAD         = 0x6ffffefd;        /* PLT padding.  */
const Elf64_Sxword DT_MOVETAB        = 0x6ffffefe;        /* Move table.  */
const Elf64_Sxword DT_SYMINFO        = 0x6ffffeff;        /* Syminfo table.  */
const Elf64_Sxword DT_ADDRRNGHI      = 0x6ffffeff;
// const Elf64_Sxword DT_ADDRTAGIDX(tag) (DT_ADDRRNGHI - (tag))        /* Reverse order! */
// const Elf64_Sxword DT_ADDRNUM 11
/* The versioning entry types.  The next are defined as part of the
   GNU extension.  */
const Elf64_Sxword DT_VERSYM         = 0x6ffffff0;
const Elf64_Sxword DT_RELACOUNT      = 0x6ffffff9;
const Elf64_Sxword DT_RELCOUNT       = 0x6ffffffa;
const Elf64_Sxword DT_FLAGS_1        = 0x6ffffffb;        /* State flags, see DF_1_* below.  */
const Elf64_Sxword DT_VERDEF         = 0x6ffffffc;        /* Address of version definition table */
const Elf64_Sxword DT_VERDEFNUM      = 0x6ffffffd;        /* Number of version definitions */
const Elf64_Sxword DT_VERNEED        = 0x6ffffffe;        /* Address of table with needed versions */
const Elf64_Sxword DT_VERNEEDNUM     = 0x6fffffff;        /* Number of needed versions */
// const Elf64_Sxword DT_VERSIONTAGIDX(tag)  (DT_VERNEEDNUM - (tag))        /* Reverse order! */
// const Elf64_Sxword DT_VERSIONTAGNUM 16
const Elf64_Sxword DT_AUXILIARY      = 0x7ffffffd;      /* Shared object to load before self */
const Elf64_Sxword DT_FILTER         = 0x7fffffff;      /* Shared object to get values from */
// const Elf64_Sxword DT_EXTRATAGIDX(tag) ((Elf32_Word)-((Elf32_Sword) (tag) <<1>>1)-1)
// const Elf64_Sxword DT_EXTRANUM        3


/* Values of `d_val' in the DT_FLAGS entry.  */
const Elf64_Sxword DF_ORIGIN         = 0x00000001;        /* Object may use DF_ORIGIN */
const Elf64_Sxword DF_SYMBOLIC       = 0x00000002;        /* Symbol resolutions starts here */
const Elf64_Sxword DF_TEXTREL        = 0x00000004;        /* Object contains text relocations */
const Elf64_Sxword DF_BIND_NOW       = 0x00000008;        /* No lazy binding for this object */
const Elf64_Sxword DF_STATIC_TLS     = 0x00000010;        /* Module uses the static TLS model */

/* State flags selectable in the `d_val' element of the DT_FLAGS_1 entry */
const Elf64_Sxword DF_1_NOW          = 0x00000001;        /* Set RTLD_NOW for this object.  */
const Elf64_Sxword DF_1_GLOBAL       = 0x00000002;        /* Set RTLD_GLOBAL for this object.  */
const Elf64_Sxword DF_1_GROUP        = 0x00000004;        /* Set RTLD_GROUP for this object.  */
const Elf64_Sxword DF_1_NODELETE     = 0x00000008;        /* Set RTLD_NODELETE for this object.*/
const Elf64_Sxword DF_1_LOADFLTR     = 0x00000010;        /* Trigger filtee loading at runtime.*/
const Elf64_Sxword DF_1_INITFIRST    = 0x00000020;        /* Set RTLD_INITFIRST for this object*/
const Elf64_Sxword DF_1_NOOPEN       = 0x00000040;        /* Set RTLD_NOOPEN for this object.  */
const Elf64_Sxword DF_1_ORIGIN       = 0x00000080;        /* $ORIGIN must be handled.  */
const Elf64_Sxword DF_1_DIRECT       = 0x00000100;        /* Direct binding enabled.  */
const Elf64_Sxword DF_1_TRANS        = 0x00000200;
const Elf64_Sxword DF_1_INTERPOSE    = 0x00000400;        /* Object is used to interpose.  */
const Elf64_Sxword DF_1_NODEFLIB     = 0x00000800;        /* Ignore default lib search path.  */
const Elf64_Sxword DF_1_NODUMP       = 0x00001000;        /* Object can't be dldump'ed.  */
const Elf64_Sxword DF_1_CONFALT      = 0x00002000;        /* Configuration alternative created.*/
const Elf64_Sxword DF_1_ENDFILTEE    = 0x00004000;        /* Filtee terminates filters search. */
const Elf64_Sxword DF_1_DISPRELDNE   = 0x00008000;        /* Disp reloc applied at build time. */
const Elf64_Sxword DF_1_DISPRELPND   = 0x00010000;        /* Disp reloc applied at run-time.  */
const Elf64_Sxword DF_1_NODIRECT     = 0x00020000;        /* Object has no-direct binding. */
const Elf64_Sxword DF_1_IGNMULDEF    = 0x00040000;
const Elf64_Sxword DF_1_NOKSYMS      = 0x00080000;
const Elf64_Sxword DF_1_NOHDR        = 0x00100000;
const Elf64_Sxword DF_1_EDITED       = 0x00200000;        /* Object is modified after built.  */
const Elf64_Sxword DF_1_NORELOC      = 0x00400000;
const Elf64_Sxword DF_1_SYMINTPOSE   = 0x00800000;        /* Object has individual interposers.  */
const Elf64_Sxword DF_1_GLOBAUDIT    = 0x01000000;        /* Global auditing required.  */
const Elf64_Sxword DF_1_SINGLETON    = 0x02000000;        /* Singleton symbols are used.  */
const Elf64_Sxword DF_1_STUB         = 0x04000000;
const Elf64_Sxword DF_1_PIE          = 0x08000000;
const Elf64_Sxword DF_1_KMOD         = 0x10000000;
const Elf64_Sxword DF_1_WEAKFILTER   = 0x20000000;
const Elf64_Sxword DF_1_NOCOMMON     = 0x40000000;

/* Flags for the feature selection in DT_FEATURE_1.  */
const Elf64_Sxword DTF_1_PARINIT     = 0x00000001;
const Elf64_Sxword DTF_1_CONFEXP     = 0x00000002;

/* Flags in the DT_POSFLAG_1 entry effecting only the next DT_* entry. */
const Elf64_Sxword DF_P1_LAZYLOAD    = 0x00000001;        /* Lazyload following object. */
const Elf64_Sxword DF_P1_GROUPPERM   = 0x00000002;        /* Symbols from next object are not generally available.  */

//---------------------------------------------------------------------
