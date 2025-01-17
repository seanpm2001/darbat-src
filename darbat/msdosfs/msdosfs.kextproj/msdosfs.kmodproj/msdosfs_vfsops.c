/*
 * @NICTA_MODIFICATIONS_START@
 * 
 * This source code is licensed under Apple Public Source License Version 2.0.
 * Portions copyright Apple Computer, Inc.
 * Portions copyright National ICT Australia.
 *
 * All rights reserved.
 *
 * This code was modified 2006-06-20.
 *
 * @NICTA_MODIFICATIONS_END@
 */
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* $FreeBSD: src/sys/msdosfs/msdosfs_vfsops.c,v 1.63 2000/05/05 09:58:36 phk Exp $ */
/*	$NetBSD: msdosfs_vfsops.c,v 1.51 1997/11/17 15:36:58 ws Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h> 				/* defines ALLPERMS */
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <sys/disk.h>
#include <mach/kmod.h>
#include <kern/thread.h>

#include <miscfs/specfs/specdev.h>

#include <igcompat/compat.h>

#include "bpb.h"
#include "bootsect.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"

#define MSDOSFS_DFLTBSIZE       4096

extern u_int16_t dos2unicode[32];

extern long msdos_secondsWest;	/* In msdosfs_conv.c */

static int	update_mp __P((struct mount *mp, struct msdosfs_args *argp));
static int	mountmsdosfs __P((vnode_t devvp, struct mount *mp,
				  vfs_context_t context, struct msdosfs_args *argp));
int	msdosfs_mount __P((struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t));
static int	msdosfs_root __P((struct mount *, vnode_t *, vfs_context_t));
static int	msdosfs_statfs __P((struct mount *, struct vfsstatfs *, vfs_context_t));
static int	msdosfs_vfs_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context);
static int	msdosfs_vfs_setattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context);
int	msdosfs_sync __P((struct mount *, int, vfs_context_t));
int	msdosfs_unmount __P((struct mount *, int, vfs_context_t));

static int	get_root_label(struct mount *mp, vfs_context_t context);

/*ARGSUSED*/
static int msdosfs_init(struct vfsconf *vfsp);
static int 
msdosfs_init(vfsp)
	struct vfsconf *vfsp;
{
	msdosfs_hash_init();
	msdosfs_fat_init();
	return 0;
}


static int msdosfs_uninit(struct vfsconf *vfsp);
static int
msdosfs_uninit(vfsp)
	struct vfsconf *vfsp;
{
	msdosfs_fat_uninit();
	msdosfs_hash_uninit();
	return 0;
}


static int
update_mp(mp, argp)
	struct mount *mp;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);

	pmp->pm_gid = argp->gid;
	pmp->pm_uid = argp->uid;
	pmp->pm_mask = (argp->mask & ALLPERMS);
	pmp->pm_flags |= argp->flags & MSDOSFSMNT_MNTOPT;
	if (argp->flags & MSDOSFSMNT_SECONDSWEST)
		msdos_secondsWest = argp->secondsWest;

	if (argp->flags & MSDOSFSMNT_LABEL)
		bcopy(argp->label, pmp->pm_label, sizeof(pmp->pm_label));

	return 0;
}


/*
 * mp - path - addr in user space of mount point (ie /usr or whatever)
 * data - addr in user space of mount params including the name of the block
 * special file to treat as a filesystem.
 */
int
msdosfs_mount(mp, devvp, data, context)
	struct mount *mp;
	vnode_t devvp;
	user_addr_t data;
	vfs_context_t context;
{
	struct msdosfs_args args; /* will hold data from mount request */
	/* msdosfs specific mount control block */
	struct msdosfsmount *pmp = NULL;
	int error, flags;

	error = copyin(data, &args, sizeof(struct msdosfs_args));
	if (error)
		return (error);
	if (args.magic != MSDOSFS_ARGSMAGIC)
		args.flags = 0;

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (vfs_isupdate(mp)) {
		pmp = VFSTOMSDOSFS(mp);
		error = 0;
		if (!(pmp->pm_flags & MSDOSFSMNT_RONLY) && vfs_isrdonly(mp)) {
			/* Downgrading from read/write to read-only */
			/* � Is vflush() sufficient?  Is there more we should flush out? */
			flags = WRITECLOSE;
			if (vfs_isforce(mp))
				flags |= FORCECLOSE;
			error = vflush(mp, NULLVP, flags);
		}
		if (!error && vfs_isreload(mp))
			/* not yet implemented */
			error = ENOTSUP;
		if (error)
			return (error);
		if ((pmp->pm_flags & MSDOSFSMNT_RONLY) && vfs_iswriteupgrade(mp)) {
			/* � Assuming that VFS has verified we can write to the device */

			pmp->pm_flags &= ~MSDOSFSMNT_RONLY;

			/* Now that the volume is modifiable, mark it dirty */
			error = markvoldirty(pmp, 1, context);
			if (error) {
				pmp->pm_flags |= MSDOSFSMNT_RONLY;
				return error;
			}
		}
	}

	if ( !vfs_isupdate(mp)) {
		error = mountmsdosfs(devvp, mp, context, &args);
		if (error)
			return error;	/* mountmsdosfs cleaned up already */
	}

	if (error == 0)
		error = update_mp(mp, &args);

	if (error == 0)
		(void) msdosfs_statfs(mp, vfs_statfs(mp), context);

	if (error)
		msdosfs_unmount(mp, MNT_FORCE, context);

	return error;
}

static int
mountmsdosfs(devvp, mp, context, argp)
	vnode_t devvp;
	struct mount *mp;
	vfs_context_t context;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp;
	struct buf *bp;
	dev_t dev = vnode_specrdev(devvp);
	union bootsector *bsp;
	struct byte_bpb33 *b33;
	struct byte_bpb50 *b50;
	struct byte_bpb710 *b710;
	u_int8_t SecPerClust;
	u_long clusters;
	int	ronly, error;
	struct vfsstatfs *vfsstatfs;
	
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 *
	 *� Obsolete?

	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	 */

	error = buf_invalidateblks(devvp, BUF_WRITE_DATA, 0, 0);
	if (error)
		return (error);

	ronly = vfs_isrdonly(mp);

	bp  = NULL; /* both used in error_exit */
	pmp = NULL;

	/*
	 * Read the boot sector of the filesystem, and then check the
	 * boot signature.  If not a dos boot sector then error out.
	 *
	 * NOTE: 2048 is a maximum sector size in current...
	 */
	error = (int)buf_meta_bread(devvp, 0, 2048, vfs_context_ucred(context), &bp);
	if (error)
		goto error_exit;
	buf_markaged(bp);
	bsp = (union bootsector *)buf_dataptr(bp);
	b33 = (struct byte_bpb33 *)bsp->bs33.bsBPB;
	b50 = (struct byte_bpb50 *)bsp->bs50.bsBPB;
	b710 = (struct byte_bpb710 *)bsp->bs710.bsBPB;


	/* [2699033]
	 *
	 * The first three bytes are an Intel x86 jump instruction.  It should be one
	 * of the following forms:
	 *    0xE9 0x?? 0x??
	 *    0xEC 0x?? 0x90
	 * where 0x?? means any byte value is OK.
	 */
	if (bsp->bs50.bsJump[0] != 0xE9
		&& (bsp->bs50.bsJump[0] != 0xEB || bsp->bs50.bsJump[2] != 0x90))
	{
		error = EINVAL;
		goto error_exit;
	}

	MALLOC(pmp, struct msdosfsmount *, sizeof(*pmp), M_TEMP, M_WAITOK);
	bzero((caddr_t)pmp, sizeof *pmp);
	pmp->pm_mountp = mp;

	/*
	 * Compute several useful quantities from the bpb in the
	 * bootsector.  Copy in the dos 5 variant of the bpb then fix up
	 * the fields that are different between dos 5 and dos 3.3.
	 */
	SecPerClust = b50->bpbSecPerClust;
	pmp->pm_BytesPerSec = getushort(b50->bpbBytesPerSec);
	pmp->pm_ResSectors = getushort(b50->bpbResSectors);
	pmp->pm_FATs = b50->bpbFATs;
	pmp->pm_RootDirEnts = getushort(b50->bpbRootDirEnts);
	pmp->pm_Sectors = getushort(b50->bpbSectors);
	pmp->pm_FATsecs = getushort(b50->bpbFATsecs);
	pmp->pm_SecPerTrack = getushort(b50->bpbSecPerTrack);
	pmp->pm_Heads = getushort(b50->bpbHeads);
	pmp->pm_Media = b50->bpbMedia;
	pmp->pm_label_cluster = CLUST_EOFE;	/* Assume there is no label in the root */

	/* calculate the ratio of sector size to device block size */
	error = VNOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t) &pmp->pm_BlockSize, 0, context);
	if (error) {
		error = ENXIO;
		goto error_exit;
	}
	pmp->pm_BlocksPerSec = pmp->pm_BytesPerSec / pmp->pm_BlockSize;

	if (pmp->pm_Sectors == 0) {
		pmp->pm_HugeSectors = getulong(b50->bpbHugeSectors);
	} else {
		pmp->pm_HugeSectors = pmp->pm_Sectors;
	}

	if (pmp->pm_RootDirEnts == 0) {
		if (pmp->pm_Sectors != 0
		    || pmp->pm_FATsecs != 0
		    || getushort(b710->bpbFSVers) != 0) {
			error = EINVAL;
			printf("mountmsdosfs(): bad FAT32 filesystem\n");
			goto error_exit;
		}
		pmp->pm_fatmask = FAT32_MASK;
		pmp->pm_fatmult = 4;
		pmp->pm_fatdiv = 1;
		pmp->pm_FATsecs = getulong(b710->bpbBigFATsecs);
		if (getushort(b710->bpbExtFlags) & FATMIRROR)
			pmp->pm_curfat = getushort(b710->bpbExtFlags) & FATNUM;
		else
			pmp->pm_flags |= MSDOSFS_FATMIRROR;
	} else
		pmp->pm_flags |= MSDOSFS_FATMIRROR;

	/*
	 * Check a few values (could do some more):
	 * - logical sector size: power of 2, >= block size
	 * - sectors per cluster: power of 2, >= 1
	 * - number of sectors:   >= 1, <= size of partition
	 */
	if ( (SecPerClust == 0)
	  || (SecPerClust & (SecPerClust - 1))
	  || (pmp->pm_BytesPerSec < DEV_BSIZE)
	  || (pmp->pm_BytesPerSec & (pmp->pm_BytesPerSec - 1))
	  || (pmp->pm_HugeSectors == 0)
	) {
		error = EINVAL;
		goto error_exit;
	}

	if (FAT32(pmp)) {
		pmp->pm_rootdirblk = getulong(b710->bpbRootClust);
		pmp->pm_firstcluster = pmp->pm_ResSectors
			+ (pmp->pm_FATs * pmp->pm_FATsecs);
		pmp->pm_fsinfo = getushort(b710->bpbFSInfo);
	} else {
                /*
                 * Compute the root directory and first cluster as sectors
                 * so that pm_maxcluster will be correct, below.
                 */
		pmp->pm_rootdirblk = (pmp->pm_ResSectors + (pmp->pm_FATs * pmp->pm_FATsecs));
		pmp->pm_rootdirsize = (pmp->pm_RootDirEnts * sizeof(struct dosdirentry)
				       + pmp->pm_BytesPerSec - 1)
			/ pmp->pm_BytesPerSec; /* in sectors */
		pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
                
                /* Change the root directory values to physical (device) blocks */
                pmp->pm_rootdirblk *= pmp->pm_BlocksPerSec;
                pmp->pm_rootdirsize *= pmp->pm_BlocksPerSec;
	}

	pmp->pm_maxcluster = (pmp->pm_HugeSectors - pmp->pm_firstcluster) /
	    SecPerClust + 1;

        pmp->pm_firstcluster *= pmp->pm_BlocksPerSec;	/* Convert to physical (device) blocks */

	if (pmp->pm_fatmask == 0) {
		if (pmp->pm_maxcluster
		    <= ((CLUST_RSRVD - CLUST_FIRST) & FAT12_MASK)) {
			/*
			 * This will usually be a floppy disk. This size makes
			 * sure that one fat entry will not be split across
			 * multiple blocks.
			 */
			pmp->pm_fatmask = FAT12_MASK;
			pmp->pm_fatmult = 3;
			pmp->pm_fatdiv = 2;
		} else {
			pmp->pm_fatmask = FAT16_MASK;
			pmp->pm_fatmult = 2;
			pmp->pm_fatdiv = 1;
		}
	}

	/* Compute number of clusters this FAT could hold based on its total size */
	clusters = pmp->pm_FATsecs * pmp->pm_BytesPerSec;	/* Size of FAT in bytes */
	clusters *= pmp->pm_fatdiv;
	clusters /= pmp->pm_fatmult;				/* Max number of clusters, rounded down */
        
	if (pmp->pm_maxcluster >= clusters) {
		printf("Warning: number of clusters (%ld) exceeds FAT "
		    "capacity (%ld)\n", pmp->pm_maxcluster + 1, clusters);
		pmp->pm_maxcluster = clusters - 1;
	}


	if (FAT12(pmp))
		pmp->pm_fatblocksize = 3 * pmp->pm_BytesPerSec;
	else
		pmp->pm_fatblocksize = MSDOSFS_DFLTBSIZE;

	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;
	pmp->pm_bnshift = ffs(pmp->pm_BlockSize) - 1;

	/*
	 * Compute mask and shift value for isolating cluster relative byte
	 * offsets and cluster numbers from a file offset.
	 */
	pmp->pm_bpcluster = (u_int32_t) SecPerClust * (u_int32_t) pmp->pm_BytesPerSec;
	pmp->pm_crbomask = pmp->pm_bpcluster - 1;
	pmp->pm_cnshift = ffs(pmp->pm_bpcluster) - 1;

	/*
	 * Check for valid cluster size
	 * must be a power of 2
	 */
	if (pmp->pm_bpcluster ^ (1 << pmp->pm_cnshift)) {
		error = EINVAL;
		goto error_exit;
	}

	/* Copy volume label from boot sector into mount point */
	{
		struct extboot *extboot;
		int i;
		u_char uc;
		
		/* Start out assuming no label (empty string) */
		pmp->pm_label[0] = '\0';

		if (FAT32(pmp)) {
			extboot = (struct extboot *) bsp->bs710.bsExt;
		} else {
			extboot = (struct extboot *) bsp->bs50.bsExt;
		}
		
		if (extboot->exBootSignature == EXBOOTSIG) {
			/*
			 * Copy the label from the boot sector into the mount point.
			 *
			 * We don't call dos2unicodefn() because it assumes the last three
			 * characters are an extension, and it will put a period before the
			 * extension.
			 */
			for (i=0; i<11; i++) {
				uc = extboot->exVolumeLabel[i];
				if (i==0 && uc == SLOT_E5)
					uc = 0xE5;
				pmp->pm_label[i] = (uc < 0x80 || uc > 0x9F ? uc : dos2unicode[uc - 0x80]);
			}

			/* Remove trailing spaces, add NUL terminator */
			for (i=10; i>=0 && pmp->pm_label[i]==' '; --i)
				;
			pmp->pm_label[i+1] = '\0';
		}
	}
        
	/*
	 * Release the bootsector buffer.
	 */
	buf_brelse(bp);
	bp = NULL;

	/*
	 * Check FSInfo.
	 */
	if (pmp->pm_fsinfo) {
		struct fsinfo *fp;

		/*
		 * The FSInfo sector occupies pm_BytesPerSec bytes on disk,
		 * but only 512 of those have meaningful contents.  There's no point
		 * in reading all pm_BytesPerSec bytes if the device block size is
		 * smaller.  So just use the device block size here.
		 */
		error = buf_meta_bread(devvp, pmp->pm_fsinfo, pmp->pm_BlockSize, vfs_context_ucred(context), &bp);
		if (error)
			goto error_exit;
		fp = (struct fsinfo *)buf_dataptr(bp);
		if (!bcmp(fp->fsisig1, "RRaA", 4)
		    && !bcmp(fp->fsisig2, "rrAa", 4)
		    && !bcmp(fp->fsisig3, "\0\0\125\252", 4))
			pmp->pm_nxtfree = getulong(fp->fsinxtfree);
		else
			pmp->pm_fsinfo = 0;
		buf_brelse(bp);
		bp = NULL;
	}

	/*
	 * Check and validate (or perhaps invalidate?) the fsinfo structure?		XXX
	 */

	/*
	 * If they want fat updates to be synchronous then let them suffer
	 * the performance degradation in exchange for the on disk copy of
	 * the fat being correct just about all the time.  I suppose this
	 * would be a good thing to turn on if the kernel is still flakey.
	 */
	if (vfs_issynchronous(mp))
		pmp->pm_flags |= MSDOSFSMNT_WAITONFAT;

	/*
	 * msdosfs_fat_init_vol() needs pm_devvp.
	 */
	pmp->pm_dev = dev;
	pmp->pm_devvp = devvp;

	/*
	 * Set up the per-volume FAT structures, including
	 * the in-use map.
	 */
	error = msdosfs_fat_init_vol(pmp, context);
	if (error != 0)
		goto error_exit;

	/*
	 * Finish up.
	 */
	if (ronly)
		pmp->pm_flags |= MSDOSFSMNT_RONLY;
	else {
		/* [2753891] Mark the volume dirty while it is mounted read/write */
		if ((error = markvoldirty(pmp, 1, context)) != 0)
			goto error_exit;

		pmp->pm_fmod = 1;
	}
	vfs_setfsprivate(mp, (void *)pmp);

	/*
	 * Fill in the statvfs fields that are constant (not updated by msdosfs_statfs)
	 */
	vfsstatfs = vfs_statfs(mp);
	vfsstatfs->f_bsize = pmp->pm_bpcluster;
	vfsstatfs->f_iosize = pmp->pm_bpcluster;
	vfsstatfs->f_blocks = pmp->pm_maxcluster + 1;
	vfsstatfs->f_fsid.val[0] = (long)dev;
	vfsstatfs->f_fsid.val[1] = vfs_typenum(mp);

	vnode_setmountedon(devvp);

	vfs_setflags(mp, MNT_IGNORE_OWNERSHIP);
	
	(void) get_root_label(mp, context);

	return 0;

error_exit:
	if (bp)
		buf_brelse(bp);
	if (pmp) {
		msdosfs_fat_uninit_vol(pmp);

		FREE(pmp, M_TEMP);

		vfs_setfsprivate(mp, (void *)NULL);
	}
	return (error);
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int msdosfs_start(struct mount *mp, int flags, vfs_context_t context);
int
msdosfs_start(mp, flags, context)
	struct mount *mp;
	int flags;
	vfs_context_t context;
{
	return (0);
}

/*
 * Unmount the filesystem described by mp.
 */
int
msdosfs_unmount(mp, mntflags, context)
	struct mount *mp;
	int mntflags;
	vfs_context_t context;
{
	struct msdosfsmount *pmp;
	int error, flags;
	int force;

	flags = 0;
	force = 0;
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
		force = 1;
	}

	error = vflush(mp, NULLVP, flags);
	if (error && !force)
		return (error);
	pmp = VFSTOMSDOSFS(mp);

	/* [2753891] If the volume was mounted read/write, mark it clean now */
	if ((pmp->pm_flags & MSDOSFSMNT_RONLY) == 0) {
		error = markvoldirty(pmp, 0, context);
		if (error && !force)
			return (error);
	}
	
	/*� Flush the device here? */
	
	msdosfs_fat_uninit_vol(pmp);
	FREE(pmp, M_TEMP);

	vfs_setfsprivate(mp, (void *)NULL);

	return (0);
}

static int
msdosfs_root(mp, vpp, context)
	struct mount *mp;
	vnode_t *vpp;
	vfs_context_t context;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct denode *ndep;
	int error;

	error = deget(pmp, MSDOSFSROOT, MSDOSFSROOT_OFS, NULLVP, NULL, &ndep, context);
	if (error)
		return (error);
	*vpp = DETOV(ndep);
	return (0);
}


static int
msdosfs_statfs(mp, sbp, context)
	struct mount *mp;
	struct vfsstatfs *sbp;
	vfs_context_t context;
{
	struct msdosfsmount *pmp;

	pmp = VFSTOMSDOSFS(mp);
	/*
	 * � VFS fills in everything from a cached copy.
	 * We only need to fill in fields that can change.
	 */
	sbp->f_bfree = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_bused = sbp->f_blocks - sbp->f_bavail;
	sbp->f_files = pmp->pm_RootDirEnts;			/* XXX */
	sbp->f_ffree = 0;	/* what to put in here? */
	vfs_name(mp, sbp->f_fstypename);
	
	/* Subtypes (flavors) for MSDOS
		0 - FAT12 
		1 - FAT16
		2 - FAT32
	*/
	if (pmp->pm_fatmask == FAT12_MASK) {
		 sbp->f_fssubtype = 0;	/* FAT12 */ 
	} else if (pmp->pm_fatmask == FAT16_MASK) {
		sbp->f_fssubtype = 1;	/* FAT16 */
	} else {
		sbp->f_fssubtype = 2;	/* FAT32 */
	}

	return (0);
}


static int
msdosfs_vfs_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context)
{
	struct vfsstatfs *stats;
	struct msdosfsmount *pmp;

	stats = vfs_statfs(mp);
	pmp = VFSTOMSDOSFS(mp);

	/* FAT doesn't track the object counts */
	
	VFSATTR_RETURN(attr, f_bsize,  pmp->pm_bpcluster);
	VFSATTR_RETURN(attr, f_iosize, pmp->pm_bpcluster);
	VFSATTR_RETURN(attr, f_blocks, pmp->pm_maxcluster + 1);
	VFSATTR_RETURN(attr, f_bfree,  pmp->pm_freeclustercount);
	VFSATTR_RETURN(attr, f_bavail, pmp->pm_freeclustercount);
	VFSATTR_RETURN(attr, f_bused,  attr->f_blocks - attr->f_bfree);
	
	/* FAT doesn't have a fixed limit on the number of file nodes */
	
	if (VFSATTR_IS_ACTIVE(attr, f_fsid)) {
		attr->f_fsid.val[0] = (long)pmp->pm_dev;
		attr->f_fsid.val[1] = vfs_typenum(mp);
		VFSATTR_SET_SUPPORTED(attr, f_fsid);
	}
	
	if (VFSATTR_IS_ACTIVE(attr, f_capabilities)) {
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] = 
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_FAST_STATFS ;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] = 
			VOL_CAP_INT_VOL_RENAME |
			VOL_CAP_INT_ADVLOCK |
			VOL_CAP_INT_FLOCK ;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

		attr->f_capabilities.valid[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_PERSISTENTOBJECTIDS |
			VOL_CAP_FMT_SYMBOLICLINKS |
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_JOURNAL |
			VOL_CAP_FMT_JOURNAL_ACTIVE |
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_SPARSE_FILES |
			VOL_CAP_FMT_ZERO_RUNS |
			VOL_CAP_FMT_CASE_SENSITIVE |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_FAST_STATFS ;
		attr->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] =
			VOL_CAP_INT_SEARCHFS |
			VOL_CAP_INT_ATTRLIST |
			VOL_CAP_INT_NFSEXPORT |
			VOL_CAP_INT_READDIRATTR |
			VOL_CAP_INT_EXCHANGEDATA |
			VOL_CAP_INT_COPYFILE |
			VOL_CAP_INT_ALLOCATE |
			VOL_CAP_INT_VOL_RENAME |
			VOL_CAP_INT_ADVLOCK |
			VOL_CAP_INT_FLOCK ;
		attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
		attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(attr, f_capabilities);
	}

	/* FAT doesn't have volume dates */
	
	if (VFSATTR_IS_ACTIVE(attr, f_fssubtype)) {
		/* Subtypes (flavors) for MSDOS
			0 - FAT12 
			1 - FAT16
			2 - FAT32
		*/
		if (pmp->pm_fatmask == FAT12_MASK) {
			attr->f_fssubtype = 0;	/* FAT12 */ 
		} else if (pmp->pm_fatmask == FAT16_MASK) {
			attr->f_fssubtype = 1;	/* FAT16 */
		} else {
			attr->f_fssubtype = 2;	/* FAT32 */
		}
		VFSATTR_SET_SUPPORTED(attr, f_fssubtype);
	}
	
	/* f_bsize returned above */
	
	if (VFSATTR_IS_ACTIVE(attr, f_vol_name)) {
		strcpy(attr->f_vol_name, pmp->pm_label);
		VFSATTR_SET_SUPPORTED(attr, f_vol_name);
	}
	
	return 0;
}

extern u_char l2u[256];
extern u_char unicode2dos(u_int16_t uc);

static int	msdosfs_vfs_setattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context)
{
    int error = 0;
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);

	if (VFSATTR_IS_ACTIVE(attr, f_vol_name))
	{
	    struct buf *bp = NULL;
	    int i;
	    int len;
	    size_t unichars;
		u_int16_t c;
	    u_int16_t volName[11];
	    u_char label[11];

		len = strlen(attr->f_vol_name);
        if (len > 63)
        	return EINVAL;

		/* Convert the UTF-8 to UTF-16 */
        error = utf8_decodestr(attr->f_vol_name, len, volName,
        	&unichars, sizeof(volName), 0, UTF_PRECOMPOSED);
        if (error)
            return error;
        unichars /= 2;	/* Bytes to characters */
		if (unichars > 11)
			return EINVAL;

        /*
         * Convert from UTF-16 to local encoding (like a short name).
         * We can't call unicode2dosfn here because it assumes a dot
         * between the first 8 and last 3 characters.
         *
         * The specification doesn't say what syntax limitations exist
         * for volume labels.  By experimentation, they appear to be
         * upper case only.  I am assuming they are like short names,
         * but no period is assumed/required after the 8th character.
         */
        
        /* Name is trailing space padded, so init to all spaces. */
        for (i=0; i<11; ++i)
            label[i] = ' ';

        for (i=0; i<unichars; ++i) {
            c = volName[i];
            if (c < 0x100)
                c = l2u[c];			/* Convert to upper case */
            if (c != ' ')			/* Allow space to pass unchanged */
                c = unicode2dos(c);	/* Convert to local encoding */
            if (c < 3)
                return EINVAL;		/* Illegal char in name */
            label[i] = c;
        }

        /* Copy the UTF-8 to pmp->pm_label */
        bcopy(attr->f_vol_name, pmp->pm_label, len);
        pmp->pm_label[len] = '\0';

        /* Update label in boot sector */
        error = (int)buf_meta_bread(pmp->pm_devvp, 0, pmp->pm_BlockSize, vfs_context_ucred(context), &bp);
        if (!error) {
            if (FAT32(pmp))
                bcopy(label, (char*)buf_dataptr(bp)+71, 11);
            else
                bcopy(label, (char*)buf_dataptr(bp)+43, 11);
            buf_bdwrite(bp);
            bp = NULL;
        }
        if (bp)
            buf_brelse(bp);
        bp = NULL;

        /*
         * Update label in root directory, if any.  For now, don't
         * create one if it doesn't exist (in case devices like
         * cameras don't understand them).
         */
        if (pmp->pm_label_cluster != CLUST_EOFE) {
        	error = readep(pmp, pmp->pm_label_cluster, pmp->pm_label_offset, &bp, NULL, context);
            if (!error) {
                bcopy(label, (char *)buf_dataptr(bp) + pmp->pm_label_offset, 11);
                buf_bdwrite(bp);
                bp = NULL;
            }
            if (bp)
                buf_brelse(bp);
            bp=NULL;
        }
        
        if (error == 0)
        	VFSATTR_SET_SUPPORTED(attr, f_vol_name);
	}
	
	return error;
}


struct msdosfs_sync_cargs {
	vfs_context_t context;
	int		waitfor;
	int		error;
};


static int
msdosfs_sync_callback(vnode_t vp, void *cargs)
{
	struct denode *dep;
	struct msdosfs_sync_cargs *args;
	int error;

	args = (struct msdosfs_sync_cargs *)cargs;

	dep = VTODE(vp);

	if ((dep->de_flag & (DE_ACCESS | DE_CREATE | DE_UPDATE | DE_MODIFIED)) || vnode_hasdirtyblks(vp)) {
		error = msdosfs_fsync_internal(vp, args->waitfor==MNT_WAIT, args->context);

		if (error)
			args->error = error;
	}
	return (VNODE_RETURNED);
}


int
msdosfs_sync(mp, waitfor, context)
	struct mount *mp;
	int waitfor;
	vfs_context_t context;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error, allerror = 0;
	struct msdosfs_sync_cargs args;
        
	/*
	 * If we ever switch to not updating all of the fats all the time,
	 * this would be the place to update them from the first one.
	 */
	if (pmp->pm_fmod != 0) {
		if (pmp->pm_flags & MSDOSFSMNT_RONLY)
			panic("msdosfs_sync: rofs mod");
		else {
			/* update fats here */
		}
	}
	/*
	 * Write back each (modified) denode.
	 */
	args.context = context;
	args.waitfor = waitfor;
	args.error = 0;
	/*
	 * msdosfs_sync_callback will be called for each vnode
	 * hung off of this mount point... the vnode will be
	 * properly referenced and unreferenced around the callback
	 */
	vnode_iterate(mp, VNODE_ITERATE_ACTIVE, msdosfs_sync_callback, (void *)&args);

	if (args.error)
		allerror = args.error;

	/*� Do we need to flush the FATs? */
	
	/*� Do we need to flush the boot sector or FSInfo sector? */

	/*
	 * Flush filesystem control info.
	 */
	error = VNOP_FSYNC(pmp->pm_devvp, waitfor, context);
	if (error)
		allerror = error;

	return (allerror);
}


struct vfsops msdosfs_vfsops = {
	msdosfs_mount,
	msdosfs_start,
	msdosfs_unmount,
	msdosfs_root,
	NULL, /* msdosfs_quotactl */
	msdosfs_vfs_getattr,
	msdosfs_sync,
	NULL, /* msdosfs_vget */
	NULL, /* msdosfs_fhtovp */
	NULL, /* msdosfs_vptofh */
	msdosfs_init,
	NULL, /* msdosfs_sysctl */
	msdosfs_vfs_setattr,
	{0}
};

extern struct vnodeopv_desc msdosfs_vnodeop_opv_desc;
static struct vnodeopv_desc *msdosfs_vnodeop_opv_desc_list[1] =
{
	&msdosfs_vnodeop_opv_desc
};


static vfstable_t msdosfs_vfsconf;

__private_extern__ int
msdosfs_module_start(/*struct kmod_info_t *ki*/void *ki, void *data);
__private_extern__ int
msdosfs_module_start(/*struct kmod_info_t *ki*/void *ki, void *data)
{
//#pragma unused(data)
	errno_t error;
	struct vfs_fsentry vfe;
	
	vfe.vfe_vfsops = &msdosfs_vfsops;
	vfe.vfe_vopcnt = 1;		/* We just have vnode operations for regular files and directories */
	vfe.vfe_opvdescs = msdosfs_vnodeop_opv_desc_list;
	strcpy(vfe.vfe_fsname, "msdos");
	vfe.vfe_flags = VFS_TBLNOTYPENUM | VFS_TBLLOCALVOL | VFS_TBL64BITREADY;
	vfe.vfe_reserv[0] = 0;
	vfe.vfe_reserv[1] = 0;
	
	error = vfs_fsadd(&vfe, &msdosfs_vfsconf);
	return error ? KERN_FAILURE : KERN_SUCCESS;
}

__private_extern__ int  
msdosfs_module_stop(/*kmod_info_t *ki*/void *ki, void *data);
__private_extern__ int  
msdosfs_module_stop(/*kmod_info_t *ki*/void *ki, void *data)
{
//#pragma unused(ki)
//#pragma unused(data)
	errno_t error;

	error = vfs_fsremove(msdosfs_vfsconf);
	if (error == 0)
		msdosfs_uninit((struct vfsconf *)NULL);

	return error ? KERN_FAILURE : KERN_SUCCESS;
}



/*
 * Look through the root directory for a volume label entry.
 * If found, use it to replace the label in the mount point.
 */
static int get_root_label(struct mount *mp, vfs_context_t context)
{
    int error;
    struct msdosfsmount *pmp;
    vnode_t rootvp = NULL;
    struct buf *bp = NULL;
    u_long frcn;	/* file relative cluster number in root directory */
    daddr64_t bn;		/* block number of current dir block */
    u_long cluster;	/* cluster number of current dir block */
    u_long blsize;	/* size of current dir block */
    int blkoff;		/* dir entry offset within current dir block */
    struct dosdirentry *dep = NULL;
    struct denode *root;
    u_int16_t unichars;
    u_int16_t ucfn[12];
    u_char uc;
    int i;
    size_t outbytes;
    char *bdata;

    pmp = VFSTOMSDOSFS(mp);

    error = msdosfs_root(mp, &rootvp, context);
    if (error)
        return error;
    root = VTODE(rootvp);
    
    for (frcn=0; ; frcn++) {
        error = pcbmap(root, frcn, 1, &bn, &cluster, &blsize, context);
        if (error)
            goto not_found;

        error = (int)buf_meta_bread(pmp->pm_devvp, bn, blsize, vfs_context_ucred(context), &bp);
        if (error) {
            goto not_found;
        }
		bdata = (char *)buf_dataptr(bp);

        for (blkoff = 0; blkoff < blsize; blkoff += sizeof(struct dosdirentry)) {
            dep = (struct dosdirentry *) (bdata + blkoff);

            /* Skip deleted directory entries */
            if (dep->deName[0] == SLOT_DELETED)
                continue;
            
            /* Stop if we hit the end of the directory (a never used entry) */
            if (dep->deName[0] == SLOT_EMPTY) {
                goto not_found;
            }

            /* Skip long name entries */
            if (dep->deAttributes == ATTR_WIN95)
                continue;
            
            if (dep->deAttributes & ATTR_VOLUME) {
                pmp->pm_label_cluster = cluster;
                pmp->pm_label_offset = blkoff;

                /*
                 * Copy the dates from the label to the root vnode.
                 */
                root->de_CHun = dep->deCHundredth;
                root->de_CTime = getushort(dep->deCTime);
                root->de_CDate = getushort(dep->deCDate);
                root->de_ADate = getushort(dep->deADate);
                root->de_MTime = getushort(dep->deMTime);
                root->de_MDate = getushort(dep->deMDate);

				/*
                 * We don't call dos2unicodefn() because it assumes the last three
                 * characters are an extension, and it will put a period before the
                 * extension.
                 */
				for (i=0; i<11; i++) {
					uc = dep->deName[i];
					if (i==0 && uc == SLOT_E5)
						uc = 0xE5;
					ucfn[i] = (uc < 0x80 || uc > 0x9F ? (u_int16_t)uc : dos2unicode[uc - 0x80]);
				}
				for (i=10; i>=0 && ucfn[i]==' '; --i)
					;
				unichars = i+1;
				
				/* translate the name in ucfn into UTF-8 */
				error = utf8_encodestr(ucfn, unichars * 2,
								pmp->pm_label, &outbytes,
								sizeof(pmp->pm_label), 0, UTF_DECOMPOSED);
                goto found;
            }
        }
        
        buf_brelse(bp);
        bp = NULL;
    }

found:
not_found:
    if (bp)
        buf_brelse(bp);

    if (rootvp)
        vnode_put(rootvp);

    return error;
}
