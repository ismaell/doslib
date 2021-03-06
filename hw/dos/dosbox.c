/* dosbox.c
 *
 * Detect whether or not we're running under the DOSBox emulator
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>

#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
#else
static const char *dosbox_fakebios = "DOSBox FakeBIOS v1.0";
#endif

/* the ROM area doesn't change. so remember our last result */
#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
#else
static signed char dosbox_detect_cache = -1;
#endif

int detect_dosbox_emu() {
#if defined(TARGET_OS2)
	/* TODO: So... how does one read the ROM BIOS area from OS/2? */
	return 0;
#elif defined(TARGET_WINDOWS)
	/* TODO: I know that from within Windows there are various ways to scan the ROM BIOS area.
	 *       Windows 1.x and 2.x (if real mode) we can just use MK_FP as we do under DOS, but
	 *       we have to use alternate means if Windows is in protected mode. Windows 2.x/3.x protected
	 *       mode (and because of compatibility, Windows 95/98/ME), there are two methods open to us:
	 *       One is to use selector constants that are hidden away in KRNL386.EXE with names like _A0000,
	 *       _B0000, etc. They are data selectors that when loaded into the segment registers point to
	 *       their respective parts of DOS adapter ROM and BIOS ROM. Another way is to use the Win16
	 *       API to create a data selector that points to the BIOS. Windows 386 Enhanced mode may map
	 *       additional things over the unused parts of adapter ROM, but experience shows that it never
	 *       relocates or messes with the VGA BIOS or with the ROM BIOS,
	 *
	 *       My memory is foggy at this point, but I remember that under Windows XP SP2, one of the
	 *       above Win16 methods still worked even from under the NT kernel.
	 *
	 *       For Win32 applications, if the host OS is Windows 3.1 Win32s or Windows 95/98/ME, we can
	 *       take advantage of a strange quirk in the way the kernel maps the lower 1MB. For whatever
	 *       reason, the VGA RAM, adapter ROM, and ROM BIOS areas are left open even for Win32 applications
	 *       with no protection. Thus, a Win32 programmer can just make a pointer like
	 *       char *a = (char*)0xA0000 and scribble on legacy VGA RAM to his heart's content (though on
	 *       modern PCI SVGA hardware the driver probably instructs the card to disable VGA compatible
	 *       mapping). In fact, this ability to scribble on RAM directly is at the heart of one of Microsoft's
	 *       earliest and hackiest "Direct Draw" interfaces known as "DISPDIB.DLL", a not-to-well documented
	 *       library responsible for those Windows 3.1 multimedia apps and games that were somehow able to
	 *       run full-screen 320x200x256 color VGA despite being Windows GDI-based apps. Ever wonder how the
	 *       MCI AVI player was able to go full-screen when DirectX and WinG were not invented yet? Now you
	 *       know :)
	 *
	 *       There are some VFW codecs out there as well, that also like to abuse DISPDIB.DLL for "fullscreen"
	 *       modes. One good example is the old "Motion Pixels" codec, that when asked to go fullscreen,
	 *       uses DISPDIB.DLL and direct VGA I/O port trickery to effectively set up a 320x480 256-color mode,
	 *       which it then draws on "fake hicolor" style to display the video (though a bit dim since you're
	 *       sort of watching a video through a dithered mesh, but...)
	 *       
	 *       In case you were probably wondering, no, Windows NT doesn't allow Win32 applications the same
	 *       privilege. Win32 apps writing to 0xA0000 would page fault and crash. Curiously enough though,
	 *       NTVDM.EXE does seem to open up the 0xA0000-0xFFFFF memory area to Win16 applications if they
	 *       use the right selectors and API calls. */
	return 0;
#else
	int i;
# if TARGET_MSDOS == 32
	const char *scan;
# else
	const char far *scan;
# endif

	probe_dos();
	if (dosbox_detect_cache >= 0)
		return (int)dosbox_detect_cache;

# if TARGET_MSDOS == 32
	if (dos_flavor == DOS_FLAVOR_FREEDOS) {
		/* FIXME: I have no idea why but FreeDOS 1.0 has a strange conflict with DOS32a where if this code
		 *        tries to read the ROM BIOS it causes a GPF and crashes (or sometimes runs off into the
		 *        weeds leaving a little garbage on the screen). DOS32a's register dump seems to imply that
		 *        at one point our segment limits were suddenly limited to 1MB (0xFFFFF). I have no clue
		 *        what the hell is triggering it, but I know from testing we can avoid that crash by not
		 *        scanning. */
		if (freedos_kernel_version == 0x000024UL) /* 0.0.36 */
			return (dosbox_detect_cache=0);
	}
# endif

/* signs that we're running under DOSBOX:
 *   - the majority of the adapter ROM area is 0x00 (not 0xFF or any other value). Here we check 0xE000:0xFF00 and 0xF000:0x000.
 *   - an ASCII string is visible at 0xF000:0xE061: "DOSBox FakeBIOS v1.0" */

# if TARGET_MSDOS == 32
	scan = (const char*)0xEFF00UL;
# else
	scan = (const char far*)MK_FP(0xE000,0xFF00);
# endif

	for (i=0;i < 256;i++) {
		if (scan[i] != 0)
			return (dosbox_detect_cache=0);
	}

# if TARGET_MSDOS == 32
	scan = (const char*)0xF0000UL;
# else
	scan = (const char far*)MK_FP(0xF000,0x0000);
# endif
	for (i=0;i < 256;i++) {
		if (scan[i] != 0)
			return (dosbox_detect_cache=0);
	}

# if TARGET_MSDOS == 32
	scan = (const char*)0xFE061UL;
# else
	scan = (const char far*)MK_FP(0xF000,0xE061);
# endif
	i = 0;
	do {
		if (dosbox_fakebios[i] != scan[i])
			return (dosbox_detect_cache=0);
	} while (dosbox_fakebios[i++] != 0);

	return (dosbox_detect_cache=1);
#endif
}

