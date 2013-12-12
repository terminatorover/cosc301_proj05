 /* This code is based on the fine code written by Joseph Pfeiffer for his
   fuse system tutorial. */

#include "s3fs.h"
#include "libs3_wrapper.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#define GET_PRIVATE_DATA ((s3context_t *) fuse_get_context()->private_data)

/*
 * For each function below, if you need to return an error,
 * read the appropriate man page for the call and see what
 * error codes make sense for the type of failure you want
 * to convey.  For example, many of the calls below return
 * -EIO (an I/O error), since there are no S3 calls yet
 * implemented.  (Note that you need to return the negative
 * value for an error code.)
 */

/* *************************************** */
/*        Stage 1 callbacks                */
/* *************************************** */

/*
 * Initialize the file system.  This is called once upon
 * file system startup.
 */
void * fs_init(struct fuse_conn_info *conn)
{
  printf("---------------------------------------------------");
   fprintf(stderr, "fs_init --- iniatializing file system.\n");
    s3context_t *ctx = GET_PRIVATE_DATA;

    time_t the_time = time(NULL);
    ssize_t ret_val = 0;
    
    //    s3contnnext_t *ctx = getenv(S3BUCKET);
    //    printf("content of s3content_t %s" , ctx -> s3bucket  );
    
   
    if (s3fs_test_bucket( ctx -> s3bucket) < 0){
      printf("Failed to connect to bucket (s3fs_test_bucket)\n");
      return NULL;
    } else {
      printf("Successfully connected to bucket (s3fs_test_bucket)\n");
    }
    if (s3fs_clear_bucket( ctx -> s3bucket) < 0) {
      printf("Failed to clear bucket (s3fs_clear_bucket)\n");
      return NULL; 
    } else {
      printf("Successfully cleared the bucket (removed all objects)\n");
      }
    
    s3dirent_t * root_dir = (s3dirent_t *) malloc (sizeof(s3dirent_t));
    

    root_dir -> type = TYPE_DIR;
    memset( root_dir -> name, 0, 256);
    strncpy(root_dir -> name ,".",1);
    root_dir -> mode = ROOT_DIR_MODE;
    root_dir ->uid = fuse_get_context()->uid;
    root_dir ->gid = fuse_get_context()->gid;
    root_dir -> size = 0 ;
    root_dir -> mtime=  the_time;
    //root_dir -> id = 0 ; 
    //    root_dir -> blocks = 0;
    //    root_dir -> nlink = 0;
    
    
    ret_val = s3fs_put_object(ctx->s3bucket, "/",(uint8_t *) root_dir, sizeof(s3dirent_t));
     
      if ( ret_val == -1)//meaning  that no object was put
	{
	  free(root_dir);
	  return NULL;
	}    
    free(root_dir);

    return ctx;
}

/*
 * Clean up filesystem -- free any allocated data.
 * Called once on filesystem exit.
 */
void fs_destroy(void *userdata) {
    fprintf(stderr, "fs_destroy --- shutting down file system.\n");
    free(userdata);
}


/* 
 * Get file attributes.  Similar to the stat() call
 * (and uses the same structure).  The st_dev, st_blksize,
 * and st_ino fields are ignored in the struct (and 
 * do not need to be filled in).
 */

int fs_getattr(const char *path, struct stat *statbuf) {
    fprintf(stderr, "fs_getattr(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    
   
   ssize_t ret_val = 0; //used to recieve the number of bytes read from s3 file system
   uint8_t * the_buffer = NULL; //buffer to be used for getting object from s3 file system 
   
   char * the_path = strdup(path);
   char * dir_name = dirname(strdup(path));
   char * base_name = basename(strdup(path));
   
   fprintf(stderr,"dirname: %s",dir_name);
   fprintf(stderr,"basename: %s",base_name);
    if ( 0 == strncmp(path,"/",strlen(path))){//check if we are being asked to get the atrr of root directory
      //   fprintf(stderr,"WE ARE SUPPOSED TO SEE THIS, because we aregetting atrributes of root dir");  
      printf("===========================================");
    	ret_val = s3fs_get_object(ctx->s3bucket, path, &the_buffer, 0, 0);
    	if ( ret_val < 0){//this is only true if we were not able to return the object
	  	  fprintf(stderr,"THIS MEANS THE FOLDER DOESN'T EXIST");
     		return -EIO;
     		}
     	

    s3dirent_t * our_obj = (s3dirent_t *)the_buffer;
    s3dirent_t root_dir = our_obj[0];
    statbuf -> st_mode = root_dir.mode;
    statbuf -> st_uid = root_dir.uid;
    statbuf -> st_gid = root_dir.gid;
    statbuf -> st_mtime = root_dir.mtime; 
    statbuf -> st_dev = 0 ;
    statbuf -> st_size = 0;
    statbuf -> st_nlink = 0;
    statbuf -> st_blocks = 0 ; 
    statbuf -> st_atime = root_dir.mtime;
    statbuf -> st_mtime = root_dir.mtime;
    statbuf -> st_ctime = root_dir.mtime;

    free(the_buffer);//because s3fs_get_object uses the_buffer as a pointer to a malloced object()
    free(the_path);
      fprintf(stderr,"I JUST RETURNED OK");
      printf("-00000000000000000000000000---------------------");
    return 0;
    }else { //now we know that we are not being asekd about the atrr of the root aka "/"
	ret_val = s3fs_get_object(ctx->s3bucket, dir_name, &the_buffer, 0, 0);//we pass in dir_name 
	fprintf(stderr,"I should be seeing this");
	//because we want to get the object assoicated with a directory since all our
	//meta data be it for a file or directory is in a directory (we have oursetup
	//such that file keys don't contain objects with metadata)
	
	if ( ret_val < 0 ){//means we didn't get our object back 
	  //	free(the_path);
          	  fprintf(stderr,"THIS MEANS THE PARENT FOLDER DOESN'T EXIST");
		return -EIO;
	}
	int itr = 0;
	s3dirent_t * entries = (s3dirent_t *)the_buffer;
	
	int entity_count = ret_val / sizeof(s3dirent_t);//entitiy count gives us how many dirents we have
	
	for (; itr < entity_count; itr++ ){
	  fprintf(stderr,"in for loop \n");
	  if (0 == strncmp(entries[itr].name,base_name,256))//check if any one of the metadata         
		 /*stored in the directry dir_name matches the base name
		 path       dirname   basename
		 /usr/lib   /usr      lib
		this means , when we store the name of a file or dir in another directory we just specify the name
		and not the whole path
		*/
	    {
		 
		   fprintf(stderr,"initalizing statbuf");
		  
		  statbuf -> st_mode = entries[itr].mode;
		  statbuf -> st_uid = entries[itr].uid;
         	  statbuf -> st_gid = entries[itr].gid;
    		  statbuf -> st_mtime = entries[itr].mtime; 
    		  statbuf -> st_dev = 0; 
		  statbuf -> st_size = entries[itr].size; 
		      statbuf -> st_dev = 0 ;

		      statbuf -> st_nlink = 0;
		      statbuf -> st_blocks = 0 ; 
		      statbuf -> st_atime = entries[itr].mtime;
		      statbuf -> st_mtime = entries[itr].mtime;
		      statbuf -> st_ctime = entries[itr].mtime;
		      
		      //	      free(the_path);
		      //	      free(the_buffer);
	
		      return 0;
	    } 


	}
	

	
    }
    
    //   free(the_path);
    //   free(the_buffer);
	
    fprintf(stderr,"THIS DOESN'T MAKE SENSE");
    return -ENOENT; 

}



/*
 * Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 */
int fs_opendir(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_opendir(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    
    
    ssize_t ret_val = 0;
    uint8_t * the_buffer = NULL;
    
    char * given_path = strdup(path);
    char * base_name = basename(strdup(path));
    char * dir_name = dirname(strdup(path));
    if (strcmp(path,"/") == 0){//if we are asked to open the root dir
   ret_val = s3fs_get_object(ctx->s3bucket, given_path, &the_buffer, 0, 0);
   if (ret_val < 0){
     free(given_path);
     return -EIO;
   }
   free(the_buffer);
   return 0;
   
    }
	




    ret_val = s3fs_get_object(ctx->s3bucket, dir_name, &the_buffer, 0, 0);//we pass in dir_name 
	//because we want to get the object assoicated with a directory since all our
	//meta data be it for a file or directory is in a directory (we have oursetup
	//such that file keys don't contain objects with metadata)
	
	if ( ret_val < 0 ){//means we didn't get our object back 
		free(given_path);
		return -EIO;
	}
	int itr = 0;
	s3dirent_t * entries = (s3dirent_t *)the_buffer;
	int entity_count = ret_val / sizeof(s3dirent_t);//entitiy count gives us how many dirents we have
	
	for (; itr < entity_count; itr++ ){
	  if (0== strncmp( entries[itr].name,base_name,256)){
			if ( entries[itr].type != TYPE_DIR){
				free(given_path);
				free(the_buffer);
				return 0;
    			}
			break ;
	 }    

    

	}
	free(given_path);
	free(the_buffer);
    return -EEXIST;
}


/*
 * Read directory.  See the project description for how to use the filler
 * function for filling in directory items.
 */
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
         struct fuse_file_info *fi)
{
    fprintf(stderr, "fs_readdir(path=\"%s\", buf=%p, offset=%d)\n",
          path, buf, (int)offset);
    s3context_t *ctx = GET_PRIVATE_DATA;
    
    
    ssize_t ret_val = 0;
    uint8_t * the_buffer = NULL;
    
    char * given_path = strdup(path);
    char * base_name = basename(strdup(path));
    char * dir_name = dirname(strdup(path));
  
    ret_val = s3fs_get_object(ctx->s3bucket, dir_name, &the_buffer, 0, 0);//we pass in dir_name 
	//because we want to get the object assoicated with a directory since all our
	//meta data be it for a file or directory is in a directory (we have oursetup
	//such that file keys don't contain objects with metadata)
	
	if ( ret_val < 0 ){//means we didn't get our object back 
		free(given_path);
		return -EIO;
	}
	int itr = 0;
	s3dirent_t * entries = (s3dirent_t *)the_buffer;
	int entity_count = ret_val / sizeof(s3dirent_t);//entitiy count gives us how many dirents we have

	for (; itr < entity_count; itr++) {
	// call filler function to fill in directory name
	// to the supplied buffer
	if (filler(buf, entries[itr].name, NULL, 0) != 0) {
		return -ENOMEM;
	}
	}
    return 0;
}


/*
 * Release directory.
 */
int fs_releasedir(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_releasedir(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return 0 ; 
    
}


/* 
 * Create a new directory.
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits (for setting in the metadata)
 * use mode|S_IFDIR.
 */
int fs_mkdir(const char *path, mode_t mode) {
    fprintf(stderr, "fs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
    s3context_t *ctx = GET_PRIVATE_DATA;
    mode |= S_IFDIR;
    time_t the_time = time(NULL);
    ssize_t ret_val = 0;
    uint8_t * the_buffer = NULL;
    
    char * given_path = strdup(path);
    char * base_name = basename(strdup(path));
    char * dir_name = dirname(strdup(path));

	ret_val = s3fs_get_object(ctx->s3bucket, dir_name, &the_buffer, 0, 0);
	
        if ( ret_val < 0 ){//means we didn't get our object back 
                free(given_path);
                return -EIO;
        }
        int itr = 0;
        s3dirent_t * entries = (s3dirent_t *)the_buffer;
        int entity_count = ret_val / sizeof(s3dirent_t);//entitiy count gives us how many dirents we have
        
        for (; itr < entity_count; itr++ ){
	  if (0 == strncmp( entries[itr].name,base_name,256)){
		     if ( entries[itr].type == TYPE_DIR){
                       	        free(given_path);
                                free(the_buffer);
                                return -EEXIST;
                        }
                        
                 }
                        
       }
       s3dirent_t * new_obj = (s3dirent_t *)malloc(sizeof(s3dirent_t)*(entity_count +1));
    
       
       s3dirent_t * et = entries;
       int index =0;
       for (; index < entity_count ; index ++)
       {
       //while ( itr != NULL){
       	 new_obj[index]= *et ;
       	 itr ++;
  //     	 index ++;
       }

       free(entries);
       s3dirent_t * new = NULL; 
       s3dirent_t * the_dir = (s3dirent_t *) malloc (sizeof(s3dirent_t));
    

    the_dir -> type = TYPE_DIR;
    memset( the_dir -> name, 0, 256);
    strncpy(the_dir -> name ,base_name,1);

    the_dir ->uid = fuse_get_context()->uid;
    the_dir ->gid = fuse_get_context()->gid;
    the_dir -> size = 0 ;
    the_dir -> mtime=  the_time;
    the_dir -> id = 0 ; 
    the_dir -> mode = S_IFDIR; //c?????????????????????????????check if this is right 
    
    new_obj[index] = *the_dir ;//our new directory meta data goes in the last cell of the new ojbect we are about to pass
   
    ret_val = s3fs_put_object(ctx->s3bucket,dir_name,(uint8_t *) new_obj, sizeof(new_obj));
     
      if ( ret_val == -1)//meaning  that no object was put
        {
          free(new_obj);
          free(the_dir);
          free(given_path);
          free(the_buffer);
          return -EIO;
        }    
        
          free(new_obj);
          free(the_dir);
          free(given_path);
          free(the_buffer);
    return 0;
}


/*
 * Remove a directory. 
 */
int fs_rmdir(const char *path) {
    fprintf(stderr, "fs_rmdir(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    
    char * given_path = strdup(path);
    char * base_name = basename(strdup(path));
    char * dir_name = dirname(strdup(path));
    ssize_t ret_val = 0;
    uint8_t * the_buffer = NULL;
    
    ret_val =  s3fs_get_object(ctx->s3bucket, given_path, &the_buffer, 0, 0);
    if ( ret_val < 0){//here we check if the object associated with the key even exists if it doesn't then there is no point in looking at the dir holding it and figuring out it's meta data
      free(given_path);
      
      return -EIO;
    }
    //need to free the_buffer ???perhaps
    if ( ret_val > sizeof(s3dirent_t )){//this means that there are more entires in the directory so we shouldn't delete it
      free(given_path);
      return -EIO;

    }
    //if we are here we know that the object associated with the full path does not contian other metadata but for itself aka "."
    if ( s3fs_remove_object(ctx->s3bucket, given_path) != 0) {//means that we had issues withremoving the corrrespoding object
      return -EIO;
    }
    //now we have removed the object , time to delete it's meta data
    ret_val = s3fs_get_object(ctx->s3bucket, dir_name, &the_buffer, 0, 0);
    if ( ret_val < 0){//means that the supposed parent directory is not existent
      return -EIO;
    }
    //if we are here we know that the parent dir exists, now lets del the dir we are given
    s3dirent_t * fresh_parent = (s3dirent_t *) malloc(ret_val - sizeof(s3dirent_t));
    s3dirent_t * dir_ents = (s3dirent_t *) the_buffer;
    int count = sizeof(fresh_parent) / sizeof(s3dirent_t);//no dirents our new dir will have
    int itr = 0;
    for ( ; itr < count ; itr ++){
      
      if ( (0 == strncmp(dir_ents[itr].name,base_name,256)) && (dir_ents[itr].type == TYPE_DIR))//if entry is the the name of the file we want to get rid of 
	{continue;
       }
      else{
	memcpy(&fresh_parent[itr],&dir_ents[itr],sizeof(s3dirent_t));
      }
    }
    ret_val = s3fs_put_object(ctx->s3bucket, dir_name, (uint8_t *)fresh_parent,sizeof(s3dirent_t)*count);

    if ( ret_val == -1){//the object was not put
      free( fresh_parent);
      return -EIO;
    }
    free( fresh_parent);
    free(given_path);
    return 0;
}


/* *************************************** */
/*        Stage 2 callbacks                */
/* *************************************** */


/* 
 * Create a file "node".  When a new file is created, this
 * function will get called.  
 * This is called for creation of all non-directory, non-symlink
 * nodes.  You *only* need to handle creation of regular
 * files here.  (See the man page for mknod (2).)
 */
int fs_mknod(const char *path, mode_t mode, dev_t dev) {
    fprintf(stderr, "fs_mknod(path=\"%s\", mode=0%3o)\n", path, mode);
    s3context_t *ctx = GET_PRIVATE_DATA;
    
    char * given_path = strdup(path);
    char * dir_name = dirname(strdup(path));
    char * base_name = basename(strdup(path));
    
    uint8_t * the_buffer = NULL;
    ssize_t ret_val = 0;
    time_t current_time  =  time(NULL);
    //lets check if the file exists
    ret_val = s3fs_get_object(ctx->s3bucket, given_path, &the_buffer,0,0);
    free(the_buffer);
    if ( 0 <= ret_val){//the file exists
      free(given_path);
      free(the_buffer);//-------------???????????check
      return -EEXIST;// 
    }
    //lets check if the parent dir exists
    ret_val = s3fs_get_object(ctx->s3bucket,dir_name, &the_buffer,0,0);
    if ( 0 > ret_val ){//if it doesn't
      free(given_path);
      return -EINVAL;//invalid input since the dir the user wants to create the file in doesn't exist
      
    }
    int entry_count = ret_val / sizeof(s3dirent_t);
    s3dirent_t * fresh_parent = (s3dirent_t *) malloc(ret_val + sizeof(s3dirent_t));
    s3dirent_t * new_file =  (s3dirent_t *) malloc(sizeof(s3dirent_t));
    //-----initalize the file
    new_file ->type =  TYPE_FILE;
    memset( new_file -> name, 0,256);
    strncpy(new_file -> name , base_name, strlen(base_name));
    new_file -> mode = mode; 
    new_file->uid = fuse_get_context()->uid;
    new_file->gid = fuse_get_context()->gid;
    new_file->mtime = current_time;
    new_file -> size = 0;
    new_file -> id = dev ;
    //-----initalize the file 
    s3dirent_t * dir_ents = (s3dirent_t *) the_buffer;
    //    int count = sizeof(fresh_parent) / sizeof(s3dirent_t);//no dirents our new dir will have
    int itr = 0;
    for ( ; itr < entry_count ; itr ++){
      fresh_parent[itr] = dir_ents[itr];
      memcpy(&fresh_parent[itr],&dir_ents[itr],sizeof(s3dirent_t)) ;
     }
    //adding the new 
    memcpy(&fresh_parent[itr],new_file,sizeof(s3dirent_t)) ;//pointers or not ?????????????????

    
    //PUT THE OBJECT
    ret_val = s3fs_put_object(ctx->s3bucket, dir_name, (uint8_t *)fresh_parent,ret_val + sizeof(s3dirent_t));

    if ( ret_val == -1){//the object was not put
      free(the_buffer);
      free( fresh_parent);
      free(given_path);
      return -EIO;
    }
    free(the_buffer);
    free( fresh_parent);
    free(given_path);
    return 0;
}


    
    



/* 
 * File open operation
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  
 * 
 * Optionally open may also return an arbitrary filehandle in the 
 * fuse_file_info structure (fi->fh).
 * which will be passed to all file operations.
 * (In stages 1 and 2, you are advised to keep this function very,
 * very simple.)
 */
int fs_open(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_open(path\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    char * given_path = strdup(path);

    char * dir_name = dirname(strdup(path));
    char * base_name = basename(strdup(path));
    
    uint8_t * the_buffer = NULL;
    ssize_t ret_val = 0;
    
    size_t path_len = strlen(path);
    
    ret_val =  s3fs_get_object(ctx->s3bucket, given_path, &the_buffer, 0, 0);
    if ( NULL != the_buffer){//the file doesn't exist
      free(the_buffer);
    }
    free(given_path);

    if( ret_val < 0){//ret_val is the number of bytes the object associated with file has and if its less than 0 we know it can't exist 
      return -EEXIST;
    }
    
    return 0;

}


/* 
 * Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  
 */
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_read(path=\"%s\", buf=%p, size=%d, offset=%d)\n",
          path, buf, (int)size, (int)offset);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.
 */
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_write(path=\"%s\", buf=%p, size=%d, offset=%d)\n",
          path, buf, (int)size, (int)offset);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.  
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 */
int fs_release(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_release(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Rename a file.
 */
int fs_rename(const char *path, const char *newpath) {
    fprintf(stderr, "fs_rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Remove a file.
 */
int fs_unlink(const char *path) {
    fprintf(stderr, "fs_unlink(path=\"%s\")\n", path);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}
/*
 * Change the size of a file.
 */
int fs_truncate(const char *path, off_t newsize) {
    fprintf(stderr, "fs_truncate(path=\"%s\", newsize=%d)\n", path, (int)newsize);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Change the size of an open file.  Very similar to fs_truncate (and,
 * depending on your implementation), you could possibly treat it the
 * same as fs_truncate.
 */
int fs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
    fprintf(stderr, "fs_ftruncate(path=\"%s\", offset=%d)\n", path, (int)offset);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return -EIO;
}


/*
 * Check file access permissions.  For now, just return 0 (success!)
 * Later, actually check permissions (don't bother initially).
 */
int fs_access(const char *path, int mask) {
    fprintf(stderr, "fs_access(path=\"%s\", mask=0%o)\n", path, mask);
    s3context_t *ctx = GET_PRIVATE_DATA;
    return 0;
}


/*
 * The struct that contains pointers to all our callback
 * functions.  Those that are currently NULL aren't 
 * intended to be implemented in this project.
 */
struct fuse_operations s3fs_ops = {
  .getattr     = fs_getattr,    // get file attributes
  .readlink    = NULL,          // read a symbolic link
  .getdir      = NULL,          // deprecated function
  .mknod       = fs_mknod,      // create a file
  .mkdir       = fs_mkdir,      // create a directory
  .unlink      = fs_unlink,     // remove/unlink a file
  .rmdir       = fs_rmdir,      // remove a directory
  .symlink     = NULL,          // create a symbolic link
  .rename      = fs_rename,     // rename a file
  .link        = NULL,          // we don't support hard links
  .chmod       = NULL,          // change mode bits: not implemented
  .chown       = NULL,          // change ownership: not implemented
  .truncate    = fs_truncate,   // truncate a file's size
  .utime       = NULL,          // update stat times for a file: not implemented
  .open        = fs_open,       // open a file
  .read        = fs_read,       // read contents from an open file
  .write       = fs_write,      // write contents to an open file
  .statfs      = NULL,          // file sys stat: not implemented
  .flush       = NULL,          // flush file to stable storage: not implemented
  .release     = fs_release,    // release/close file
  .fsync       = NULL,          // sync file to disk: not implemented
  .setxattr    = NULL,          // not implemented
  .getxattr    = NULL,          // not implemented
  .listxattr   = NULL,          // not implemented
  .removexattr = NULL,          // not implemented
  .opendir     = fs_opendir,    // open directory entry
  .readdir     = fs_readdir,    // read directory entry
  .releasedir  = fs_releasedir, // release/close directory
  .fsyncdir    = NULL,          // sync dirent to disk: not implemented
  .init        = fs_init,       // initialize filesystem
  .destroy     = fs_destroy,    // cleanup/destroy filesystem
  .access      = fs_access,     // check access permissions for a file
  .create      = NULL,          // not implemented
  .ftruncate   = fs_ftruncate,  // truncate the file
  .fgetattr    = NULL           // not implemented
};



/* 
 * You shouldn't need to change anything here.  If you need to
 * add more items to the filesystem context object (which currently
 * only has the S3 bucket name), you might want to initialize that
 * here (but you could also reasonably do that in fs_init).
 */
int main(int argc, char *argv[]) {
    // don't allow anything to continue if we're running as root.  bad stuff.
    if ((getuid() == 0) || (geteuid() == 0)) {
    	fprintf(stderr, "Don't run this as root.\n");
    	return -1;
    }
    s3context_t *stateinfo = malloc(sizeof(s3context_t));
    memset(stateinfo, 0, sizeof(s3context_t));

    char *s3key = getenv(S3ACCESSKEY);
    if (!s3key) {
        fprintf(stderr, "%s environment variable must be defined\n", S3ACCESSKEY);
        return -1;
    }
    char *s3secret = getenv(S3SECRETKEY);
    if (!s3secret) {
        fprintf(stderr, "%s environment variable must be defined\n", S3SECRETKEY);
        return -1;
    }
    char *s3bucket = getenv(S3BUCKET);
    if (!s3bucket) {
        fprintf(stderr, "%s environment variable must be defined\n", S3BUCKET);
        return -1;
    }
    strncpy((*stateinfo).s3bucket, s3bucket, BUFFERSIZE);

    fprintf(stderr, "Initializing s3 credentials\n");
    s3fs_init_credentials(s3key, s3secret);

    fprintf(stderr, "Totally clearing s3 bucket\n");
    s3fs_clear_bucket(s3bucket);

    fprintf(stderr, "Starting up FUSE file system.\n");
    int fuse_stat = fuse_main(argc, argv, &s3fs_ops, stateinfo);
    fprintf(stderr, "Startup function (fuse_main) returned %d\n", fuse_stat);


    return fuse_stat;
    //      return 0;
 
}
