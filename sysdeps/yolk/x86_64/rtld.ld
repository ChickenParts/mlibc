/* Provide linker symbols for binutils 2.43+ */
/* With newer binutils, hidden symbols must be defined in input object files */
PROVIDE(_DYNAMIC = ADDR(.dynamic));
PROVIDE_HIDDEN(__ehdr_start = SEGMENT_START("elf-header", 0));
