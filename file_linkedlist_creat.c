#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <stdint.h>
#include "config.h"
void file_linkedlist_creat(void);
void node_creat(const char *name,struct f_linkedlist** p_head_node);
void dir_proc(const char *dirname);
static int singlefile_proc(const char *fpath,const struct stat *sb,int tflag,struct FTW *ftwbuf);


struct f_linkedlist* head_node;

struct added_config_t added_config = {
    131072,
    NULL,
    NULL,
    NULL,
    0,
    0,
    0
};

void file_linkedlist_creat(void)
{
    if(added_config.p_filename!=NULL)
    {
        if(strchr(added_config.p_filename,'/')==NULL)
        {
            char path[256]={0};
            getcwd(path,256);
            strcat(path,"/");
            strcat(path,added_config.p_filename);
            node_creat(path,&head_node);
        }else
            node_creat(added_config.p_filename,&head_node);
        
    }
    if(added_config.p_listname!=NULL)
    {
        FILE *fp_list=fopen(added_config.p_listname,"rt");
        char filename_temp[256];
        if(fp_list==NULL)
        {
            perror("open the list file error");
            exit(1);
        }
        char *s=NULL;
        while((s=fgets(filename_temp,256,fp_list))!=NULL)
        {
            while (1) {
                int len;
                char ch;
                len = strlen(s);
                ch = *(s + len - 1);
                if (ch == '\r' || ch == '\n')
                    *(s + len - 1) = '\0';
                else
                    break;
            }
            node_creat(filename_temp,&head_node);
        }
    }

    if(added_config.p_dir!=NULL)
    {
        const char *dirname=added_config.p_dir;
        dir_proc(dirname);
    }
    //printf("head_node\tfilename:%s\n",head_node->f_name);
    //printf("code run the end of creat function\n");
}

void dir_proc(const char *dirname)
{
    int flags=0;
    flags|=FTW_DEPTH;
    flags|=FTW_PHYS;
    if(nftw(dirname,singlefile_proc,20,flags)==-1) 
    {
        perror("error to call ntfw");
        exit(5);
    }   
}

static int singlefile_proc(const char* fpath,const struct stat *sb,int tflag,struct FTW *ftwbuf)
{
    if(tflag==FTW_F)
    {
        node_creat(fpath,&head_node);
        //printf("fpath:%s\n",fpath);
    }
    return 0;
}

void node_creat(const char *name,struct f_linkedlist** p_head_node)
{
    static struct f_linkedlist* p=NULL;
    //printf("static p:%p\n",p);
    //printf("filename:%s\n",name);
    struct stat st;
    struct f_linkedlist *p_temp =(struct f_linkedlist*)malloc(sizeof(struct f_linkedlist));
    if(p_temp==NULL)
    {
        perror("error to malloc p_temp");
        exit(2);
    }
    memset(p_temp,0,sizeof(struct f_linkedlist));
    
    strcpy(p_temp->f_name,name);
    if(stat(name,&st)<0)
    {
        perror("error to call stat()");
        exit(3);
    }
    p_temp->f_size=st.st_size;
    p_temp->f_mode=st.st_mode;

    if((*p_head_node)!=NULL)
    {
        p->next=p_temp;
        //printf("p and p_temp point addr:%p,%p\n",p->next,p_temp);

    }else
        *p_head_node=p_temp;
        p=p_temp;
}

void file_linkedlist_del(void)
{
    struct f_linkedlist* rc=head_node;
    struct f_linkedlist* temp;
    while(rc!=NULL)
    {
        temp=rc->next;
        free(rc);
        rc=temp;
    }
    head_node=NULL;
}
