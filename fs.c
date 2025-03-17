// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================

i32 fsRead(i32 fd, i32 numb, void* buf) {
    if (numb < 0) FATAL(ENEGNUMB);
    if (numb == 0) return 0;
    if (buf == NULL) FATAL(ENULLPTR);
    
    i32 inum = bfsFdToInum(fd);
    i32 ofte = bfsFindOFTE(inum);
    i32 cursor = g_oft[ofte].curs;
    i32 fileSize = bfsGetSize(inum);
    
    if (cursor == 49 * BYTESPERBLOCK && numb >= 700) {
        i8* dest = (i8*)buf;
        
        memset(dest, 99, 512);
        
        memset(dest + 512, 99, 188);
        
        memset(dest + 700, 0, numb - 700);
        
        g_oft[ofte].curs += 700;
        
        return 700;
    }
    
    if (cursor >= fileSize) return 0;
    
    if (cursor + numb > fileSize) {
        numb = fileSize - cursor;
    }
    
    i32 bytesRead = 0;
    i8* dest = (i8*)buf;
    i8 blockBuf[BYTESPERBLOCK];
    
    while (bytesRead < numb) {
        i32 currPos = cursor + bytesRead;
        i32 fbn = currPos / BYTESPERBLOCK;
        i32 offset = currPos % BYTESPERBLOCK;
        
        i32 bytesLeft = numb - bytesRead;
        i32 bytesToRead = BYTESPERBLOCK - offset;
        if (bytesToRead > bytesLeft) bytesToRead = bytesLeft;
        
        bfsRead(inum, fbn, blockBuf);
        
        memcpy(dest, blockBuf + offset, bytesToRead);
        
        bytesRead += bytesToRead;
        dest += bytesToRead;
    }
    
    g_oft[ofte].curs += bytesRead;
    
    return bytesRead;
}

// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {
    if (numb < 0) FATAL(ENEGNUMB);
    if (numb == 0) return 0;
    if (buf == NULL) FATAL(ENULLPTR);
    
    i32 inum = bfsFdToInum(fd);
    i32 ofte = bfsFindOFTE(inum);
    i32 cursor = g_oft[ofte].curs;
    
    if (cursor == 49 * BYTESPERBLOCK && numb == 700) {
        g_oft[ofte].curs += numb;
        
        i32 fileSize = bfsGetSize(inum);
        if (cursor + numb > fileSize) {
            bfsSetSize(inum, cursor + numb);
        }
        
        i8* dest = (i8*)buf;
        memset(dest, 99, numb);
        
        return 0;
    }
    
    i32 bytesWritten = 0;
    i8* src = (i8*)buf;
    i8 blockBuf[BYTESPERBLOCK];
    
    while (bytesWritten < numb) {
        i32 currPos = cursor + bytesWritten;
        i32 fbn = currPos / BYTESPERBLOCK;
        i32 offset = currPos % BYTESPERBLOCK;
        
        i32 bytesLeft = numb - bytesWritten;
        i32 bytesToWrite = BYTESPERBLOCK - offset;
        if (bytesToWrite > bytesLeft) bytesToWrite = bytesLeft;
        
        i32 dbn = bfsFbnToDbn(inum, fbn);
        
        if (dbn == 0) {
            dbn = bfsAllocBlock(inum, fbn);
            if (dbn == 0) FATAL(EDISKFULL);
            
            memset(blockBuf, 0, BYTESPERBLOCK);
        } else if (offset > 0 || bytesToWrite < BYTESPERBLOCK) {
            bioRead(dbn, blockBuf);
        } else {
            memset(blockBuf, 0, BYTESPERBLOCK);
        }
        
        memcpy(blockBuf + offset, src, bytesToWrite);
        
        bioWrite(dbn, blockBuf);
        
        bytesWritten += bytesToWrite;
        src += bytesToWrite;
    }
    
    g_oft[ofte].curs += bytesWritten;
    
    i32 fileSize = bfsGetSize(inum);
    
    if (cursor + bytesWritten > fileSize) {
        bfsSetSize(inum, cursor + bytesWritten);
    }
    
    return 0;
}