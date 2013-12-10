#ifndef __USERSPACEFS_H__
#define __USERSPACEFS_H__

#include <sys/stat.h>
#include <stdint.h>   // for uint32_t, etc.
#include <sys/time.h> // for struct timeval

/* This code is based on the fine code written by Joseph Pfeiffer for his
   fuse system tutorial. */

/* Declare to the FUSE API which version we're willing to speak */
#define FUSE_USE_VERSION 26

#define S3ACCESSKEY "S3_ACCESS_KEY_ID"
#define S3SECRETKEY "S3_SECRET_ACCESS_KEY"
#define S3BUCKET "S3_BUCKET"

#define BUFFERSIZE 1024
#define ROOT_DIR_MODE (S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR)

#define TYPE_DIR     0
#define TYPE_FILE    1
#define S3FS_TYPE_UNUSED  2
// store filesystem state information in this struct
typedef struct {
    char s3bucket[BUFFERSIZE];
} s3context_t;

/*
 * Other data type definitions (e.g., a directory entry
 * type) should go here.
 */

typedef struct __s3dirent {
  unsigned char type ;//dir or regurlar file
  char name[256];
  time_t mtime ; //last modified time
  mode_t mode ; //protection
  uid_t uid;
  gid_t gid;
  dev_t id;
  off_t size; 
  
  
} s3dirent_t ; 



#endif // __USERSPACEFS_H__
