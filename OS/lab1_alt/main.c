#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ftw.h>
#include <stdlib.h>
#include <libgen.h>
#include <time.h>

#ifdef PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif
static long posix_version = 0;
static long xsi_version = 0;
#define PATH_MAX_GUESS 1024
#define NAME_MAX 255 

static char *fullpath;
static size_t pathlen;

typedef int Myfunc(const char *, const struct stat *, int);
static Myfunc myfunc;
static int full_path_tree(const char *, Myfunc *);
static int do_full_path(Myfunc *, int depth);

static int myfunc(const char *pathname, const struct stat *statptr, int type)
{
    if (type == FTW_F || type == FTW_D)
    {
        printf("├── %s\n", basename((char *)pathname));
        return EXIT_SUCCESS;
    }
    else if (type == FTW_DNR)
    {
        printf("закрыт доступ к каталогу %s", pathname);
        return EXIT_FAILURE;
    }
    else if (type == FTW_NS)
    {
        printf("ошибка вызова функции stat для %s", pathname);
        return EXIT_FAILURE;
    }
    else
    {
        printf("неизвестный тип %d для файла %s", type, pathname);
        return EXIT_FAILURE;
    }

    return 0;
}

char *path_alloc(size_t *sizep)
{                              
    int errno;
    char *ptr;
    size_t size;
    if (posix_version == 0)
        posix_version = sysconf(_SC_VERSION); 
    if (xsi_version == 0)
        xsi_version = sysconf(_SC_XOPEN_VERSION);
    if (pathmax == 0)
    {
        errno = 0;
        if ((pathmax = pathconf("/", _PC_PATH_MAX)) < 0)
        {
            if (errno == 0)
                pathmax = PATH_MAX_GUESS;
            else
                perror("ошибка вызова pathconf с параметром _PC_PATH_MAX");
        }
        else
        {
            pathmax++; 
        }
    }
    if ((posix_version < 200112L) && (xsi_version < 4))
        size = pathmax + 1;
    else
        size = pathmax;
    if ((ptr = (char *)(malloc(size))) == NULL)
        perror("malloc error for pathname");
    if (sizep != NULL)
        *sizep = size;
    return (ptr);
}

static int do_full_path(Myfunc *func, int depth)
{
    for (int i = 0; i < depth - 1; i++)
        printf("│   ");
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;
    if (lstat(fullpath, &statbuf) < 0)
        return (func(fullpath, &statbuf, FTW_NS));
    if (S_ISDIR(statbuf.st_mode) == 0)
        return (func(fullpath, &statbuf, FTW_F));
    if ((ret = func(fullpath, &statbuf, FTW_D)) != 0)
        return (ret);
    n = strlen(fullpath);
    if (n + NAME_MAX + 2 > pathlen)
    {
        pathlen *= 2;
        if ((fullpath = (char *)(realloc(fullpath, pathlen))) == NULL)
            printf("ошибка вызова realloc");
    }
    fullpath[n++] = '/';
    fullpath[n] = 0;
    if ((dp = opendir(fullpath)) == NULL) 
        return (func(fullpath, &statbuf, FTW_DNR));

    while ((dirp = readdir(dp)) != NULL)
    {
        if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
            continue;                                  
        strcpy(&fullpath[n], dirp->d_name);            
        if ((ret = do_full_path(func, depth + 1)) != 0) 
            break;                                      
    }
    fullpath[n - 1] = 0; 
    if (closedir(dp) < 0)
        printf("невозможно закрыть каталог %s", fullpath);
    return (ret);
}

static int full_path_tree(const char *pathname, Myfunc *func)
{
    size_t len;
    fullpath = NULL;                
    size_t alloc_calls = 0;       
    clock_t alloc_start, alloc_end; 
    alloc_start = clock();
    fullpath = path_alloc(&len);
    alloc_end = clock();
    alloc_calls++; 
    if (pathlen <= strlen(pathname))
    {
        pathlen = strlen(pathname) * 2;
        if ((fullpath = (char *)(realloc(fullpath, pathlen))) == NULL)
        {
            perror("error calling realloc");
        }
    }
    strcpy(fullpath, pathname);
    int result = do_full_path(func, 0);
    double alloc_time = (double)(alloc_end - alloc_start) / CLOCKS_PER_SEC;
    printf("Number of path_alloc calls: %lu\n", alloc_calls);
    printf("Time taken for path_alloc function: %.6f seconds\n\n", alloc_time);
    return result;
}

static int do_short_path(const char *filename, Myfunc *func, int depth) 
{
    for (int i = 0; i < depth - 1; i++)
        printf("│   ");
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;
    if (lstat(filename, &statbuf) < 0)
        return (func(filename, &statbuf, FTW_NS));
    if (S_ISDIR(statbuf.st_mode) == 0)
        return (func(filename, &statbuf, FTW_F));
    if ((ret = func(filename, &statbuf, FTW_D)) != 0)
        return (ret);

    if ((dp = opendir(filename)) == NULL) 
        return (func(filename, &statbuf, FTW_DNR));
    chdir(filename);
    int dir = (strcmp(fullpath, ".") == 0 || strcmp(fullpath, "..") == 0);
    while (!dir && (dirp = readdir(dp)) != NULL)
    {
        if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
            continue; 
        if ((ret = do_short_path(dirp->d_name, func, depth + 1)) != 0)
            break;                                                    
    }
    chdir("..");
    if (closedir(dp) < 0)
        printf("невозможно закрыть каталог %s", filename);
    return (ret);
}

static int short_path_tree(const char *pathname, Myfunc *func)
{
    size_t len;
    fullpath = path_alloc(&len); 
    if (pathlen <= strlen(pathname))
    {
        pathlen = strlen(pathname) * 2;
        if ((fullpath = (char *)(realloc(fullpath, pathlen))) == NULL)
            perror("ошибка вызова realloc");

        fullpath = path_alloc(&len);
    }
    strcpy(fullpath, pathname);

    return (do_short_path(pathname, func, 0));
}

double measure_time_fullpath(const char *path, Myfunc *func, double *result)
{
    int ret;
    int start = clock();
    ret = full_path_tree(path, func);
    int end = clock();
    *result = (double)(end - start) / CLOCKS_PER_SEC;
    return ret;
}

double measure_time_shortpath(const char *path, Myfunc *func, double *result)
{
    int ret;
    int start = clock();
    ret = short_path_tree(path, func);
    int end = clock();
    *result = (double)(end - start) / CLOCKS_PER_SEC;
    return ret;
}

int main(int argc, char *argv[])
{
    int ret;
    double time_fullpath;
    double time_shortpath;
    if (argc != 2)
    {
        perror("Incorrect input");
    }
    printf("\nFull path\n");
    ret = measure_time_fullpath(argv[1], myfunc, &time_fullpath);
    if (ret != 0)
    {
        perror("Full path ftw failed");
        exit(ret);
    }
    printf("\nShort path\n");
    ret = measure_time_shortpath(argv[1], myfunc, &time_shortpath);
    if (ret != 0)
    {
        perror("Short path ftw failed");
        exit(ret);
    }

    printf("Fullpath secounds: %f\n", time_fullpath);
    printf("Shortpath secounds: %f\n", time_shortpath);

    return 0;
}