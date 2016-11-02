
#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hw/dos/exeload.h>
#include <hw/dos/execlsg.h>

#include "clsgexm1.h" // generated by build script, symbols as enum

struct exeload_ctx      final_exe = exeload_ctx_INIT;

int main() {
    if (!exeload_load(&final_exe,"clsgexm1.dlm")) {
        fprintf(stdout,"Load failed\n");
        return 1;
    }
    if (!exeload_clsg_validate_header(&final_exe)) {
        fprintf(stdout,"DLM is not valid CLSG\n");
        exeload_free(&final_exe);
        return 1;
    }

    fprintf(stdout,"DLM image loaded to %04x:0000 residentlen=%lu\n",final_exe.base_seg,(unsigned long)final_exe.len_seg << 4UL);
    {
        unsigned int i,m=exeload_clsg_function_count(&final_exe);

        fprintf(stdout,"%u functions:\n",m);
        for (i=0;i < m;i++)
            fprintf(stdout,"  [%u]: %04x (%Fp)\n",i,exeload_clsg_function_offset(&final_exe,i),exeload_clsg_function_ptr(&final_exe,i));
    }

    /* let's call some! */
    {
        /* const char far * CLSG_EXPORT_PROC get_message(void); */
        const char far * (CLSG_EXPORT_PROC *get_message)(void) = exeload_clsg_function_ptr(&final_exe,CLSGEXM1_get_message);
        const char far *msg;

        fprintf(stdout,"Calling entry %u (get_message) now.\n",CLSGEXM1_get_message);
        msg = get_message();
        fprintf(stdout,"Result: %Fp = %Fs\n",msg,msg);
    }

    {
        /* unsigned int CLSG_EXPORT_PROC get_value(void);
           void CLSG_EXPORT_PROC set_value1(const unsigned int v);
           void CLSG_EXPORT_PROC set_value2(const unsigned int v); */
        unsigned int (CLSG_EXPORT_PROC *get_value)(void) = exeload_clsg_function_ptr(&final_exe,CLSGEXM1_get_value);
        void (CLSG_EXPORT_PROC *set_value1)(const unsigned int v) = exeload_clsg_function_ptr(&final_exe,CLSGEXM1_set_value1);
        void (CLSG_EXPORT_PROC *set_value2)(const unsigned int v) = exeload_clsg_function_ptr(&final_exe,CLSGEXM1_set_value2);

        fprintf(stdout,"Playing with data + bss values.\n");
        fprintf(stdout,"Initial data value should be 0x1234,bss value should be 0x0000,value=data+bss\n");
        fprintf(stdout,"value = %04x\n",get_value());
        fprintf(stdout,"set value 0xABCD\n");
        set_value1(0xABCD);
        fprintf(stdout,"value = %04x\n",get_value());
        fprintf(stdout,"set bss value 0x1111\n");
        set_value2(0x1111);
        fprintf(stdout,"value = %04x\n",get_value());
    }

    exeload_free(&final_exe);
    return 0;
}

