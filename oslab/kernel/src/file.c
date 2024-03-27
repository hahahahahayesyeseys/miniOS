#include "klib.h"
#include "file.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE];

static file_t *falloc() {
    Log("falloc\n");
  //  find a file whose ref==0, init it, inc ref and return it, return NULL if none
   for(int i=0;i<TOTAL_FILE;++i){
    if(files[i].ref==0){
      files[i].type=TYPE_NONE;
      files[i].ref++;
      return &files[i];
    }
   }
   return NULL;
}

file_t *fopen(const char *path, int mode) {
   
  file_t *fp = falloc();
  inode_t *ip = NULL;
  if (!fp) goto bad;
 
  int open_type = 114514;
  if(!(mode&O_CREATE)){
    open_type=TYPE_NONE;
  }
  else{
    if(mode&O_DIR){
      open_type=TYPE_DIR;
    }
    else{
      open_type=TYPE_FILE;
    }
  }
  ip = iopen(path, open_type);
  if (!ip) goto bad;
  int type = itype(ip);
  if (type == TYPE_FILE || type == TYPE_DIR) {
    // if type is not DIR, go bad if mode&O_DIR
      if(type!=TYPE_DIR&&(mode&O_DIR)){
        goto bad;
      }
    // if type is DIR, go bad if mode WRITE or TRUNC
      if(type==TYPE_DIR&&(mode&O_WRONLY||mode&O_RDWR||mode&O_TRUNC)){
        goto bad;
      }
    // if mode&O_TRUNC, trunc the file
    if(type==TYPE_FILE&&(mode&O_TRUNC)){
      itrunc(ip);
    }
    fp->type = TYPE_FILE; // file_t don't and needn't distingush between file and dir
    fp->inode = ip;
    fp->offset = 0;
  } else if (type == TYPE_DEV) {
    fp->type = TYPE_DEV;
    fp->dev_op = dev_get(idevid(ip));
    iclose(ip);
    ip = NULL;
  } else assert(0);
  fp->readable = !(mode & O_WRONLY);
  fp->writable = (mode & O_WRONLY) || (mode & O_RDWR);
  return fp;
bad:
  if (fp) fclose(fp);
  if (ip) iclose(ip);
  return NULL;
}

int fread(file_t *file, void *buf, uint32_t size) {
    Log("fread\n");
  //  distribute read operation by file's type
  // remember to add offset if type is FILE (check if iread return value >= 0!)
  //if (!file->readable) return -1;
  Log("size:%d\n",size);
  assert(size>=0);
  int read_size=0;
  if(file->type==TYPE_FILE){
    
    int ret=iread(file->inode,file->offset,buf,size);
    Log("ret:%d\n",ret);
    if(ret>=0){
      read_size=ret;
      file->offset+=read_size;
      return read_size;
    }
    else{
       return -1;
    }
  }
  else if(file->type==TYPE_DEV){
    
     
  
      read_size=file->dev_op->read(buf,size);
       Log("read_size:%d\n",read_size);
      if(read_size<0){
        return -1;
        }
      else{
        return read_size;
      }
    }

  return -1;
}

int fwrite(file_t *file, const void *buf, uint32_t size) {
    Log("fwrite\n");
  // distribute write operation by file's type
  // remember to add offset if type is FILE (check if iwrite return value >= 0!)
  if (!file->writable) return -1;
  if(file->type==TYPE_FILE){
    int write_num=iwrite(file->inode,file->offset,buf,size);
    if(write_num>0){
      file->offset+=write_num;
    }
    return write_num;
  }
  else if(file->type==TYPE_DEV){
    return file->dev_op->write(buf,size);
  }
  return -1;
}

uint32_t fseek(file_t *file, uint32_t off, int whence) {
    Log("fseek\n");
  //  change file's offset, do not let it cross file's size
  if (file->type == TYPE_FILE) {
    uint32_t new_off=0;
    uint32_t file_size=isize(file->inode);
    if(whence==SEEK_SET){
      new_off=off;
    }
    else if(whence==SEEK_CUR){
      new_off=file->offset+off;
    }
    else if(whence==SEEK_END){
      new_off=file_size+off;
    }
    //处理越界
    if(new_off>file_size||new_off<0){
      return -1;
    }
  
    file->offset=new_off;
    return new_off;
  }

  return -1;
}

file_t *fdup(file_t *file) {
 

    Log("fdup\n");
if(file==NULL){
    return NULL;
    }
  file->ref++;
  return file;
}

void fclose(file_t *file) {
  Log("fclose\n");
  //dec file's ref, if ref==0 and it's a file, call iclose
  file->ref--;
  if(file->ref==0&&file->type==TYPE_FILE){
    iclose(file->inode);
  }
}
