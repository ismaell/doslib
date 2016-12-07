
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;

static void help(void) {
    fprintf(stderr,"EXEHDMP -i <exe file>\n");
}

int main(int argc,char **argv) {
    char *a;
    int i;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown switch %s\n",a);
            return 1;
        }
    }

    if (sizeof(exehdr) != 0x1C) {
        fprintf(stderr,"EXE header sizeof error\n");
        return 1;
    }

    if (src_file == NULL) {
        fprintf(stderr,"No source file specified\n");
        return 1;
    }

    src_fd = open(src_file,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Unable to open '%s', %s\n",src_file,strerror(errno));
        return 1;
    }

    if (read(src_fd,&exehdr,sizeof(exehdr)) != (int)sizeof(exehdr)) {
        fprintf(stderr,"EXE header read error\n");
        return 1;
    }

    if (exehdr.magic != 0x5A4DU/*MZ*/) {
        fprintf(stderr,"EXE header signature missing\n");
        return 1;
    }

    printf("MS-DOS EXE header:\n");
    printf("    last_block_bytes:             %u bytes\n",
        exehdr.last_block_bytes);
    printf("    exe_file_blocks:              %u bytes\n",
        exehdr.exe_file_blocks);
    printf("  * resident size:                %lu bytes\n",
        (unsigned long)exe_dos_header_file_resident_size(&exehdr));
    printf("    number_of_relocations:        %u entries\n",
        exehdr.number_of_relocations);
    printf("    header_size:                  %u paragraphs\n",
        exehdr.header_size_paragraphs);
    printf("  * header_size:                  %lu bytes\n",
        (unsigned long)exe_dos_header_file_header_size(&exehdr));
    printf("    min_additional_paragraphs:    %u paragraphs\n",
        exehdr.min_additional_paragraphs);
    printf("  * min_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_size(&exehdr));
    printf("    max_additional_paragraphs:    %u paragraphs\n",
        exehdr.max_additional_paragraphs);
    printf("  * max_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_max_size(&exehdr));
    printf("    init stack pointer:           base_seg+0x%04X:0x%04X\n",
        exehdr.init_stack_segment,
        exehdr.init_stack_pointer);
    printf("    checksum:                     0x%04X\n",
        exehdr.checksum);
    printf("    init instruction pointer:     base_seg+0x%04X:0x%04X\n",
        exehdr.init_code_segment,
        exehdr.init_instruction_pointer);
    printf("    relocation_table_offset:      %u bytes\n",
        exehdr.relocation_table_offset);
    printf("    overlay number:               %u\n",
        exehdr.overlay_number);

    if (exe_dos_header_file_resident_size(&exehdr) > exe_dos_header_file_header_size(&exehdr)) {
        printf("  * resident portion:             %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exe_dos_header_file_header_size(&exehdr),
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) - 1UL,
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
        printf("  * minimum memory footprint:     %lu bytes\n",
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
        printf("  * maximum memory footprint:     %lu bytes\n",
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_max_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
    }
    else {
        printf("  * no resident portion\n");
    }

    close(src_fd);
    return 0;
}
