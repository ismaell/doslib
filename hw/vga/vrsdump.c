
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vrl.h"
#include "vrs.h"
#include "pcxfmt.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

unsigned char		*buffer = NULL;
unsigned char		*fence = NULL;

struct vrs_header	*vrshdr = NULL;

int main(int argc,char **argv) {
	unsigned long sz,offs;
	unsigned int entry;
	int fd;

	if (argc < 2) {
		fprintf(stderr,"vrsdump <vrs file>\n");
		return 1;
	}

	fd = open(argv[1],O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"cannot open %s\n",argv[1]);
		return 1;
	}

	sz = lseek(fd,0,SEEK_END);
#if TARGET_MSDOS == 16
	if (sz > 0xFFFFUL) {
		fprintf(stderr,"Too large\n");
		return 1;
	}
#endif
	if (sz == 0) {
		fprintf(stderr,"Zero length file\n");
		return 1;
	}

	/* VRS sheets are supposed to be loaded into memory as one binary blob.
	 * All file offsets become offsets relative to the base memory address of the blob. */
	buffer = malloc(sz);
	if (buffer == NULL) {
		fprintf(stderr,"Failed to alloc buffer\n");
		return 1;
	}
	fence = buffer + sz;
	lseek(fd,0,SEEK_SET);
	if (read(fd,buffer,sz) != (int)sz) {
		fprintf(stderr,"Failed to read all data\n");
		return 1;
	}

	vrshdr = (struct vrs_header*)buffer;
	if (memcmp(vrshdr->vrs_sig,"VRS1",4)) {
		fprintf(stderr,"Not a VRS file\n");
		return 1;
	}

	printf("VRS header:\n");
	printf("    Resident size:          %lu\n",(unsigned long)vrshdr->resident_size);
	printf("    Offset of VRS list:     %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_VRS_LIST]);
	printf("    Offset of sprite IDs:   %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST]);
	printf("    Offset of sprite names: %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST]);
	printf("    Offset of anim list:    %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST]);
	printf("    Offset of anim IDs:     %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST]);
	printf("    Offset of anim names:   %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST]);

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_VRS_LIST]) != 0UL) {
		if ((offs+4UL) <= sz) {
			uint32_t *lst = (uint32_t*)(buffer + offs);
			uint32_t *fnc = (uint32_t*)(fence + 1 - sizeof(uint32_t));
			unsigned char *chk;
			unsigned int c=0;

			printf("*VRS offsets: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 16) {
					c = 0;
					printf("\n");
				}
				printf("%lu ",(unsigned long)(*lst));

				if ((*lst) >= (sz - 8UL)) {
					printf("*ERROR offset out of bounds\n");
					c = 0;
				}
				else {
					chk = buffer + *lst;
					if (memcmp(chk,"VRL1",4)) {
						printf("*ERROR entry not a VRL sprite\n");
						c = 0;
					}
				}

				lst++;
			} while (1);
			printf("\n");
		}
		else {
			printf("*VRS list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			uint16_t *lst = (uint16_t*)(buffer + offs);
			uint16_t *fnc = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			unsigned int c=0;

			printf("*Sprite IDs: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 32) {
					c = 0;
					printf("\n");
				}
				printf("%u ",(unsigned int)(*lst++));
			} while (1);
			printf("\n");
		}
		else {
			printf("*sprite ID list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			/* an array of C strings terminated by a NUL string (0x00 0x00) */
			char *s = (char*)buffer + offs;
			char *f = (char*)fence;
			unsigned int c=0;
			char *name;

			printf("*Sprite names: ");
			do {
				if (s > f) printf("*ERROR list overflow\n");
				if (s >= f) break;

				name = s;
				if (*name == 0) break;

				/* scan until NUL */
				while (s < f && *s != 0) s++;
				if (s == f) {
					printf("*ERROR list overflow, string not terminated\n");
					break;
				}
				if (*s == 0) s++; // then step over NUL

				if ((++c) >= 8) {
					c = 0;
					printf("\n");
				}
				printf("\"%s\" ",name);
			} while (1);
			printf("\n");
		}
		else {
			printf("*sprite name list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST]) != 0UL) {
		if ((offs+4UL) <= sz) {
			uint32_t *lst = (uint32_t*)(buffer + offs);
			uint32_t *fnc = (uint32_t*)(fence + 1 - sizeof(uint32_t));
			unsigned int c=0;

			printf("*Animation list offsets: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 16) {
					c = 0;
					printf("\n");
				}
				printf("%lu ",(unsigned long)(*lst));

				if ((*lst) >= (sz - 8UL)) {
					printf("*ERROR offset out of bounds\n");
					c = 0;
				}

				lst++;
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			uint16_t *lst = (uint16_t*)(buffer + offs);
			uint16_t *fnc = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			unsigned int c=0;

			printf("*Animation IDs: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 32) {
					c = 0;
					printf("\n");
				}
				printf("%u ",(unsigned int)(*lst++));
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation ID list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			/* an array of C strings terminated by a NUL string (0x00 0x00) */
			char *s = (char*)buffer + offs;
			char *f = (char*)fence;
			unsigned int c=0;
			char *name;

			printf("*Animation names: ");
			do {
				if (s > f) printf("*ERROR list overflow\n");
				if (s >= f) break;

				name = s;
				if (*name == 0) break;

				/* scan until NUL */
				while (s < f && *s != 0) s++;
				if (s == f) {
					printf("*ERROR list overflow, string not terminated\n");
					break;
				}
				if (*s == 0) s++; // then step over NUL

				if ((++c) >= 8) {
					c = 0;
					printf("\n");
				}
				printf("\"%s\" ",name);
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation name list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_VRS_LIST]) != 0UL && (offs+4UL) <= sz) {
		uint32_t *vrl_list_end = (uint32_t*)(fence - 1 + sizeof(uint32_t));
		char *namelist_fence = NULL,*namelist_scan = NULL;
		uint32_t *vrl_list = (uint32_t*)(buffer + offs);
		uint16_t *idlist = NULL,*idlist_end = NULL;
		struct vrl1_vgax_header *vrl1;
		char *sprite_name=NULL;
		uint32_t vrl_offset=0;
		uint16_t sprite_id=0;

		if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST]) != 0UL) {
			if ((offs+2UL) <= sz) {
				idlist = (uint16_t*)(buffer + offs);
				idlist_end = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			}
		}

		if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST]) != 0UL) {
			if ((offs+2UL) <= sz) {
				/* an array of C strings terminated by a NUL string (0x00 0x00) */
				namelist_scan = (char*)buffer + offs;
				namelist_fence = (char*)fence;
			}
		}

		printf("Sprite summary:\n");
		for (entry=0;;entry++) {
			if ((vrl_list+entry) >= vrl_list_end) break;

			vrl_offset = vrl_list[entry];
			if (vrl_offset == 0) break;

			sprite_name = NULL;
			sprite_id = 0;

			if (namelist_scan != NULL) {
				if (*namelist_scan != 0) {
					sprite_name = namelist_scan;
					while (namelist_scan < namelist_fence && *namelist_scan != 0) namelist_scan++;

					if (namelist_scan < namelist_fence && *namelist_scan == 0) {
						namelist_scan++;
					}
					else {
						/* string is not NULL terminated. forget it. */
						sprite_name = NULL;
						namelist_scan = NULL;
					}
				}
			}

			if ((buffer+vrl_offset+sizeof(*vrl1)) >= fence) continue;
			vrl1 = (struct vrl1_vgax_header*)(buffer + vrl_offset);

			if (idlist != NULL) {
				if ((idlist+entry) < idlist_end)
					sprite_id = idlist[entry];
			}

			printf("  [entry %u]\n",entry);
			if (sprite_id != 0)
				printf("     Sprite ID: %u\n",sprite_id);
			if (sprite_name != NULL)
				printf("     Sprite name: \"%s\"\n",sprite_name);

			if (!memcmp(vrl1->vrl_sig,"VRL1",4)) {
				char tmp[5];

				memcpy(tmp,vrl1->fmt_sig,4); tmp[4]=0;
				printf("     Sprite type: %s\n",tmp);

				if (!memcmp(vrl1->fmt_sig,"VGAX",4)) {
					printf("     Sprite is %u x %u hotspot %d x %d\n",
						vrl1->width,vrl1->height,vrl1->hotspot_x,vrl1->hotspot_y);
				}
			}
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST]) != 0UL && (offs+4UL) <= sz) {
		struct vrs_animation_list_entry_t *anilist,*anilist_end;
		uint32_t *anim_list_end = (uint32_t*)(fence - 1 + sizeof(uint32_t));
		uint32_t *anim_list = (uint32_t*)(buffer + offs);
		char *namelist_fence = NULL,*namelist_scan = NULL;
		uint16_t *idlist = NULL,*idlist_end = NULL;
		char *anim_name=NULL;
		uint32_t anim_offset=0;
		uint16_t anim_id=0;

		if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST]) != 0UL) {
			if ((offs+2UL) <= sz) {
				idlist = (uint16_t*)(buffer + offs);
				idlist_end = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			}
		}

		if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST]) != 0UL) {
			if ((offs+2UL) <= sz) {
				/* an array of C strings terminated by a NUL string (0x00 0x00) */
				namelist_scan = (char*)buffer + offs;
				namelist_fence = (char*)fence;
			}
		}

		printf("Animation summary:\n");
		for (entry=0;;entry++) {
			if ((anim_list+entry) >= anim_list_end) break;

			anim_offset = anim_list[entry];
			if (anim_offset == 0) break;

			anim_name = NULL;
			anim_id = 0;

			if (namelist_scan != NULL) {
				if (*namelist_scan != 0) {
					anim_name = namelist_scan;
					while (namelist_scan < namelist_fence && *namelist_scan != 0) namelist_scan++;

					if (namelist_scan < namelist_fence && *namelist_scan == 0) {
						namelist_scan++;
					}
					else {
						/* string is not NULL terminated. forget it. */
						anim_name = NULL;
						namelist_scan = NULL;
					}
				}
			}

			if ((buffer+anim_offset+sizeof(*anilist)) >= fence) continue;
			anilist = (struct vrs_animation_list_entry_t *)(buffer + anim_offset);
			anilist_end = (struct vrs_animation_list_entry_t *)(fence - 1 + sizeof(*anilist));

			if (idlist != NULL) {
				if ((idlist+entry) < idlist_end)
					anim_id = idlist[entry];
			}

			printf("  [entry %u]\n",entry);
			if (anim_id != 0)
				printf("     Animation ID: %u\n",anim_id);
			if (anim_name != NULL)
				printf("     Animation name: \"%s\"\n",anim_name);

			printf("     Animation sequence:\n");
			do {
				if (anilist > anilist_end) {
					printf("*ERROR LIST OVERFLOW\n");
					break;
				}

				if (anilist->sprite_id == 0)
					break;

				printf("        Frame:\n");
				printf("           Sprite ID:      %u\n",anilist->sprite_id);
				printf("           Delay:          %u ticks\n",anilist->delay);
				printf("           Event ID:       %u\n",anilist->event_id);

				anilist++;
			} while (1);
		}
	}

	close(fd);
	return 0;
}

