/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <fmt/omf/omf.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

//================================== PROGRAM ================================

static char*                            in_file = NULL;
static char*                            out_file = NULL;

struct omf_context_t*                   omf_state = NULL;

int my_fixupp_patch_code_16ofs_fixup(const struct omf_context_t * const ctx,unsigned char *base,unsigned int fixup,unsigned int len) {
    // base+fixup refers directly to the 16-bit segbase fixup the linker would patch.
    // we look around it at the x86 opcode.

    // "MOV WORD PTR [reg],seg symbol"
    //   which becomes
    // "MOV WORD PTR [reg],<imm>"
    //   change it to
    // "MOV WORD PTR [reg],cs" + "NOP" + "NOP"
    if (fixup >= 2 && (fixup+2) <= len && base[fixup-2] == 0xC7 && base[fixup-1] <= 0x07) {
        if (ctx->flags.verbose)
            fprintf(stderr,"Patching: MOV WORD PTR [reg],seg ... to refer to CS\n");

        base[fixup-2] = 0x8C;
        base[fixup-1] = (1/*CS*/ << 3) | (base[fixup-1] & 7);
        base[fixup+0] = 0x90;
        base[fixup+1] = 0x90;

        return 0;
    }
    // "MOV WORD PTR [reg+disp8],seg symbol"
    //   which becomes
    // "MOV WORD PTR [reg+disp8],<imm>"
    //   change it to
    // "MOV WORD PTR [reg+disp8],cs" + "NOP" + "NOP" + "NOP"
    else if (fixup >= 3 && (fixup+3) <= len && base[fixup-3] == 0xC7 && base[fixup-2] >= 0x40 && base[fixup-2] <= 0x47) { // [fixup-1] is displacement
        if (ctx->flags.verbose)
            fprintf(stderr,"Patching: MOV WORD PTR [reg+disp8],seg ... to refer to CS\n");

        base[fixup-3] = 0x8C;
        base[fixup-2] = (1/*mod==1*/ << 6) | (1/*CS*/ << 3) | (base[fixup-2] & 7);
//      base[fixup-1]    do not overwrite displacement
        base[fixup+0] = 0x90;
        base[fixup+1] = 0x90;

        return 0;
    }
    // "MOV <reg>,seg symbol"
    //   which becomes
    // "MOV <reg>,<imm>"
    //   change it to
    // "MOV <reg>,cs" + NOP
    else if (fixup >= 1 && (fixup+2) <= len && (base[fixup-1]&0xF8) == 0xB8) {
        if (ctx->flags.verbose)
            fprintf(stderr,"Patching: MOV reg,seg ... to refer to CS\n");

        base[fixup+0] = 0xC0 | (1/*CS*/ << 3) | (base[fixup-1] & 7);
        base[fixup-1] = 0x8C;
        base[fixup+1] = 0x90;

        return 0;
    }

    return -1;
}

const struct omf_pubdef_t *lookup_pubdef(const struct omf_context_t * const ctx,const char *name) {
    const struct omf_pubdef_t *pubdef;
    unsigned int i;

    for (i=1;i <= omf_pubdefs_context_get_highest_index(&ctx->PUBDEFs);i++) {
        pubdef = omf_pubdefs_context_get_pubdef(&ctx->PUBDEFs,i);
        if (pubdef == NULL) continue;

        if (!strcmp(pubdef->name_string,name))
            return pubdef;
    }

    return NULL;
}

int segdef_in_DGROUP(struct omf_context_t * const ctx,unsigned int segment_index) {
    const struct omf_grpdef_t *grpdef;
    unsigned int gi,si;
    const char *name;

    for (gi=1;gi <= omf_grpdefs_context_get_highest_index(&ctx->GRPDEFs);gi++) {
        name = omf_lnames_context_get_name(&ctx->LNAMEs,gi);
        if (name == NULL) continue;

        if (!strcmp(name,"DGROUP")) {
            grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,gi);
            if (grpdef == NULL) continue;

            for (si=0;si < grpdef->count;si++) {
                int segdef_i;

                segdef_i = omf_grpdefs_context_get_grpdef_segdef(&ctx->GRPDEFs,grpdef,si);
                if (segdef_i <= 0) continue;

                if ((unsigned int)segdef_i == segment_index)
                    return 1;
            }
        }
    }

    return 0;
}

void my_fixupp_patch_segrefs(struct omf_context_t * const ctx,struct omf_record_t *ledata) {
    unsigned char update_le_chk = 0;
    struct omf_ledata_info_t info;
    unsigned char is_code = 0;
    unsigned int i;

    omf_record_lseek(ledata,0);
    if (omf_context_parse_LEDATA(ctx,&info,ledata) < 0) {
        fprintf(stderr,"Unable to parse LEDATA\n");
        return;
    }

    // is this LEDATA for a code segment?
    {
        const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,info.segment_index);
        if (segdef != NULL) {
            const char *segname = omf_lnames_context_get_name(&ctx->LNAMEs,segdef->segment_name_index);
            const char *classname = omf_lnames_context_get_name(&ctx->LNAMEs,segdef->class_name_index);

            if (segname != NULL) {
                if (!strcmp(segname,"TEXT") || !strcmp(segname,"_TEXT"))
                    is_code = 1;
            }
            if (classname != NULL) {
                if (!strcmp(classname,"CODE"))
                    is_code = 1;
            }
        }
    }

    for (i=1;i <= omf_fixupps_context_get_highest_index(&ctx->FIXUPPs);i++) {
        struct omf_fixupp_t *fixupp;

        fixupp = (struct omf_fixupp_t*)omf_fixupps_context_get_fixupp(&ctx->FIXUPPs,i);
        if (fixupp == NULL) continue;
        if (!fixupp->alloc) continue;

        if (fixupp->location == OMF_FIXUPP_LOCATION_16BIT_SEGMENT_OFFSET) {
            fprintf(stderr,"WARNING: ignoring segment:offset fixup\n");
            continue;
        }

        if (fixupp->location != OMF_FIXUPP_LOCATION_16BIT_SEGMENT_BASE)
            continue;

        if ((fixupp->data_record_offset+2U) > info.data_length)
            continue;

        if (!is_code) {
            fprintf(stderr,"WARNING: cannot patch 16-bit segbase fixup because segment is not code\n");
            continue;
        }

        // is this a fixup we care about?
        // we're interested in segment fixups that refer to DGROUP.
        // the purpose of this program is to patch .obj files so that instead of
        // needing segment relocations to refer to DGROUP, the code instead just
        // loads CS or DS to get the segment value needed. if we want convenient
        // development to compile to "tiny" models like .COM this patching is
        // needed, Watcom's linker ignores segment relocations for .COM targets
        // and the program will crash otherwise.
        //
        // but to use this patching tool properly, you have to use Watcom C's
        // small memory model, and you have to tell the linker and compiler to
        // associate everything with DGROUP so that the .COM target will pack
        // everything into one segment (DGROUP).
        //
        // for completeness, we assume all EXTERNs are also in the same DGROUP.
        if (fixupp->frame_method == OMF_FIXUPP_FRAME_METHOD_TARGET) {
            if (fixupp->target_method == OMF_FIXUPP_TARGET_METHOD_EXTDEF) {
                const char *name = omf_context_get_extdef_name_safe(ctx,fixupp->target_index);

                // wait... sometimes Watcom C will declare if EXTDEF, then declare the same
                // symbol PUBDEF. if that's the case, then we can validate whether the
                // extern is within DGROUP.
                if (name != NULL) {
                    const struct omf_pubdef_t *pubdef = lookup_pubdef(ctx,name);
                    if (pubdef) {
                        const char *pubdef_group = omf_context_get_grpdef_name_safe(ctx,pubdef->group_index);

                        if (pubdef->group_index != 0 && !strcmp(pubdef_group,"DGROUP")) {
                            // yes, do it
                        }
                        else if (pubdef->group_index == 0 && segdef_in_DGROUP(ctx,pubdef->segment_index)) {
                            // yes, do it
                        }
                        else {
                            // don't
                            fprintf(stderr,"WARNING: Unable to determine how to patch EXTDEF by pubdef\n");
                            continue;
                        }
                    }
                }

                // good, do it
            }
            else {
                // don't
                fprintf(stderr,"WARNING: Target method not EXTDEF\n");
                continue;
            }
        }
        else if (fixupp->frame_method == OMF_FIXUPP_FRAME_METHOD_GRPDEF) {
            const char *group = omf_context_get_grpdef_name_safe(ctx,fixupp->frame_index);

            if (!strcmp(group,"DGROUP")) {
                // yes, do it
            }
            else {
                // don't
                fprintf(stderr,"WARNING: Unable to patch GRPDEF, because it's not DGROUP\n");
                continue;
            }
        }
        else if (fixupp->frame_method == OMF_FIXUPP_FRAME_METHOD_SEGDEF) {
            if (segdef_in_DGROUP(ctx,fixupp->frame_index)) {
                // yes, do it
            }
            else {
                // don't
                fprintf(stderr,"WARNING: Unable to patch SEGDEF, not part of DGROUP\n");
                continue;
            }
        }

        if (my_fixupp_patch_code_16ofs_fixup(ctx,info.data,fixupp->data_record_offset,info.data_length) == 0) {
            fixupp->alloc = 0;//it worked
            update_le_chk = 1;
        }
        else {
            fprintf(stderr,"WARNING: unable to patch 16-bit segbase fixup\n");
        }
    }

    // if we changed bytes in LEDATA we have to fix checksum
    if (update_le_chk)
        omf_record_write_update_checksum(ledata);
}

static void help(void) {
    fprintf(stderr,"omfsegdg [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to dump\n");
    fprintf(stderr,"  -o <file>    OMF file to output\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
}

void my_dumpstate(const struct omf_context_t * const ctx) {
    unsigned int i;
    const char *p;

    printf("OBJ dump state:\n");

    if (ctx->THEADR != NULL)
        printf("* THEADR: \"%s\"\n",ctx->THEADR);

    if (ctx->LNAMEs.omf_LNAMES != NULL) {
        printf("* LNAMEs:\n");
        for (i=1;i <= ctx->LNAMEs.omf_LNAMES_count;i++) {
            p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

            if (p != NULL)
                printf("   [%u]: \"%s\"\n",i,p);
            else
                printf("   [%u]: (null)\n",i);
        }
    }

    if (ctx->SEGDEFs.omf_SEGDEFS != NULL) {
        for (i=1;i <= ctx->SEGDEFs.omf_SEGDEFS_count;i++)
            dump_SEGDEF(stdout,omf_state,i);
    }

    if (ctx->GRPDEFs.omf_GRPDEFS != NULL) {
        for (i=1;i <= ctx->GRPDEFs.omf_GRPDEFS_count;i++)
            dump_GRPDEF(stdout,omf_state,i);
    }

    if (ctx->EXTDEFs.omf_EXTDEFS != NULL)
        dump_EXTDEF(stdout,omf_state,1);

    if (ctx->PUBDEFs.omf_PUBDEFS != NULL)
        dump_PUBDEF(stdout,omf_state,1);

    if (ctx->FIXUPPs.omf_FIXUPPS != NULL)
        dump_FIXUPP(stdout,omf_state,1);

    printf("----END-----\n");
}

int main(int argc,char **argv) {
    struct omf_record_t last_ledata;
    unsigned char dumpstate = 0;
    unsigned char diddump = 0;
    unsigned char verbose = 0;
    unsigned char outself = 0;
    int i,fd,ret,ofd;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                in_file = argv[i++];
                if (in_file == NULL) return 1;
            }
            else if (!strcmp(a,"o")) {
                out_file = argv[i++];
                if (out_file == NULL) return 1;
            }
            else if (!strcmp(a,"v")) {
                verbose = 1;
            }
            else if (!strcmp(a,"d")) {
                dumpstate = 1;
            }
            else {
                help();
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    // prepare parsing
    if ((omf_state=omf_context_create()) == NULL) {
        fprintf(stderr,"Failed to init OMF parsing state\n");
        return 1;
    }
    omf_state->flags.verbose = (verbose > 0);

    if (in_file == NULL || out_file == NULL) {
        help();
        return 1;
    }

    if (!strcmp(in_file,out_file)) {
        // caller wants us to write to a temporary file, then
        // rename over the input file.
        outself = 1;
        out_file = strdup(in_file);
        if (out_file == NULL) return 1;

        // change something about the output file name string to make it unique
        {
            char *ext = strrchr(out_file,'.');
            if (ext == NULL) return 1;

            if (!strcasecmp(ext,".obj"))
                strcpy(ext,".obt");
            else
                return 1;
        }
    }

    fd = open(in_file,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
        return 1;
    }

    ofd = open(out_file,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
    if (ofd < 0) {
        fprintf(stderr,"Failed to open output file %s\n",strerror(errno));
        return 1;
    }

    /* first pass: read OMF symbols, segdefs, and groupdefs */
    omf_context_begin_file(omf_state);

    do {
        ret = omf_context_read_fd(omf_state,fd);
        if (ret == 0) {
            break;
        }
        else if (ret < 0) {
            fprintf(stderr,"Error: %s\n",strerror(errno));
            if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
            break;
        }

        switch (omf_state->record.rectype) {
            case OMF_RECTYPE_THEADR:/*0x80*/
                if (omf_context_parse_THEADR(omf_state,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing THEADR\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_THEADR(stdout,omf_state);

                break;
            case OMF_RECTYPE_EXTDEF:/*0x8C*/
            case OMF_RECTYPE_LEXTDEF:/*0xB4*/
            case OMF_RECTYPE_LEXTDEF32:/*0xB5*/{
                int first_new_extdef;

                if ((first_new_extdef=omf_context_parse_EXTDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing EXTDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_EXTDEF(stdout,omf_state,(unsigned int)first_new_extdef);

                } break;
            case OMF_RECTYPE_PUBDEF:/*0x90*/
            case OMF_RECTYPE_PUBDEF32:/*0x91*/
            case OMF_RECTYPE_LPUBDEF:/*0xB6*/
            case OMF_RECTYPE_LPUBDEF32:/*0xB7*/{
                int first_new_pubdef;

                if ((first_new_pubdef=omf_context_parse_PUBDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing PUBDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_PUBDEF(stdout,omf_state,(unsigned int)first_new_pubdef);

                } break;
            case OMF_RECTYPE_LNAMES:/*0x96*/{
                int first_new_lname;

                if ((first_new_lname=omf_context_parse_LNAMES(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing LNAMES\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_LNAMES(stdout,omf_state,(unsigned int)first_new_lname);

                } break;
            case OMF_RECTYPE_SEGDEF:/*0x98*/
            case OMF_RECTYPE_SEGDEF32:/*0x99*/{
                int first_new_segdef;

                if ((first_new_segdef=omf_context_parse_SEGDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing SEGDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_SEGDEF(stdout,omf_state,(unsigned int)first_new_segdef);

                } break;
            case OMF_RECTYPE_GRPDEF:/*0x9A*/
            case OMF_RECTYPE_GRPDEF32:/*0x9B*/{
                int first_new_grpdef;

                if ((first_new_grpdef=omf_context_parse_GRPDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing GRPDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_GRPDEF(stdout,omf_state,(unsigned int)first_new_grpdef);

                } break;
            case 0xF0:/*LIBHEAD*/
                fprintf(stderr,"This program does not handle .LIB files\n");
                return 1;
        }
    } while (1);

    if (dumpstate && !diddump) {
        my_dumpstate(omf_state);
        diddump = 1;
    }

    /* begin second pass */
    if (omf_state->flags.verbose)
        printf("Starting 2nd pass\n");

    if (lseek(fd,0,SEEK_SET) != 0) {
        fprintf(stderr,"lseek(0) failed\n");
        return 1;
    }

    // FIXME HACK
    omf_state->record.rectype = 0;

    // clear last LEDATA record
    memset(&last_ledata,0,sizeof(last_ledata));

    do {
        ret = omf_context_read_fd(omf_state,fd);
        if (ret == 0) {
            break;
        }
        else if (ret < 0) {
            fprintf(stderr,"Error: %s\n",strerror(errno));
            if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
            break;
        }

        // process and copy only FIXUPP, LEDATA, and MODEND
        switch (omf_state->record.rectype) {
            case OMF_RECTYPE_FIXUPP:/*0x9C*/
            case OMF_RECTYPE_FIXUPP32:/*0x9D*/
                // parse FIXUPP
                omf_fixupps_context_free_entries(&omf_state->FIXUPPs);
                if (omf_context_parse_FIXUPP(omf_state,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing FIXUPP\n");
                    return 1;
                }

                // patch segment refs, and then remove those FIXUPPs
                if (last_ledata.data == NULL) {
                    fprintf(stderr,"FIXUPP with no prior LEDATA\n");
                    return 1;
                }
                my_fixupp_patch_segrefs(omf_state,&last_ledata);

                // write parsed FIXUPPs back to record
                if (omf_context_generate_FIXUPP(&omf_state->record,omf_state,omf_state->record.rectype & 1) < 0) {
                    fprintf(stderr,"Error generating FIXUPP\n");
                    return 1;
                }

                /* flush out LEDATA (possibly modified) */
                if (last_ledata.data != NULL) {
                    if (omf_context_record_write_fd(ofd,&last_ledata) < 0) {
                        fprintf(stderr,"Failed to write OMF record\n");
                        return 1;
                    }
                    omf_record_free(&last_ledata);
                }
                /* flush out FIXUPP (possibly modified) */
                if (omf_context_record_write_fd(ofd,&omf_state->record) < 0) {
                    fprintf(stderr,"Failed to write OMF record\n");
                    return 1;
                }
                break;
            case 0x8A://MODEND
            case 0x8B://MODEND32
                if (omf_context_record_write_fd(ofd,&omf_state->record) < 0) {
                    fprintf(stderr,"Failed to write OMF record\n");
                    return 1;
                }
                break;
            case OMF_RECTYPE_LEDATA:/*0xA0*/
            case OMF_RECTYPE_LEDATA32:/*0xA1*/
                /* flush out last LEDATA record if any, store the new one */
                if (last_ledata.data != NULL) {
                    if (omf_state->flags.verbose)
                        fprintf(stderr,"LEDATA flushing out prior for new one\n");

                    if (omf_context_record_write_fd(ofd,&last_ledata) < 0) {
                        fprintf(stderr,"Failed to write OMF record\n");
                        return 1;
                    }
                    omf_record_free(&last_ledata);
                }
                last_ledata = omf_state->record;//FIXME: the omf context should offer a "take record" to change ownership instead of this
                omf_record_init(&omf_state->record);
                omf_state->record.data_alloc = last_ledata.data_alloc;
                break;
            default:
                /* flush out LEDATA (possibly modified) */
                if (last_ledata.data != NULL) {
                    if (omf_context_record_write_fd(ofd,&last_ledata) < 0) {
                        fprintf(stderr,"Failed to write OMF record\n");
                        return 1;
                    }
                    omf_record_free(&last_ledata);
                }
                if (omf_context_record_write_fd(ofd,&omf_state->record) < 0) {
                    fprintf(stderr,"Failed to write OMF record\n");
                    return 1;
                }
                break;
        }
    } while (1);

    if (last_ledata.data != NULL) {
        if (omf_context_record_write_fd(ofd,&last_ledata) < 0) {
            fprintf(stderr,"Failed to write OMF record\n");
            return 1;
        }
        omf_record_free(&last_ledata);
    }

    omf_context_clear(omf_state);
    omf_state = omf_context_destroy(omf_state);
    close(ofd);
    close(fd);

    if (outself) {
        char *x = strdup(in_file);
        if (x == NULL)
            return 1;

        {
            char *e = strrchr(x,'.');
            if (e == NULL)
                return 1;

            if (!strcasecmp(e,".obj"))
                strcpy(e,".obo");
            else
                return 1;
        }

        unlink(x); // remove previous .obo file, if there
        if (rename(in_file,x) < 0) {
            fprintf(stderr,"Failed to rename file, %s\n",strerror(errno));
            return 1;
        }
        if (rename(out_file,in_file) < 0) {
            fprintf(stderr,"Failed to rename file, %s\n",strerror(errno));
            return 1;
        }

        free(x);
        free(out_file);
        out_file = NULL;
        outself = 0;
    }

    return 0;
}

