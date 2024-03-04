#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#define FTW_F 1
#define FTW_D 2
#define FTW_DNR 3
#define FTW_NS 4

/* тип функции, которая будет вызываться для каждого встреченного файла */

typedef int Myfunc(const char *, const struct stat *, int, int);
static Myfunc myfunc;
static int myftw(char *, Myfunc *);
static int dopath(Myfunc *, int);
static int mydopath(Myfunc *, int);

#ifdef PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif
static long posix_version = 0;
static long xsi_version = 0;
/* Если константа PATH_MAX не определена */
#define PATH_MAX_GUESS 1024
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
char *path_alloc(size_t *sizep) /* если удалось выделить память, */
{
    /* возвращает также выделенной объем */
    char *ptr;
    size_t size;
    if (posix_version == 0)
        posix_version = sysconf(_SC_VERSION);
    if (xsi_version == 0)
        xsi_version = sysconf(_SC_XOPEN_VERSION);
    if (pathmax == 0)
    { /* первый вызов функции */
        errno = 0;
        if ((pathmax = pathconf("/", _PC_PATH_MAX)) < 0)
        {
            if (errno == 0)
                pathmax = PATH_MAX_GUESS; /* если константа не определена */
            else
            {
                printf("ошибка вызова pathconf с параметром _PC_PATH_MAX");
                exit(1);
            }
        }
        else
        {
            pathmax++; /* добавить 1, так как путь относительно корня */
        }
    }
    /*
     * До версии POSIX.1-2001 не гарантируется, что PATH_MAX включает
     * завершающий нулевой байт. То же для XPG3.
     */
    if ((posix_version < 200112L) && (xsi_version < 4))
        size = pathmax + 1;
    else
        size = pathmax;
    if ((ptr = malloc(size)) == NULL)
    {
        printf("malloc error for pathname");
        exit(1);
    }
    if (sizep != NULL)
        *sizep = size;
    return (ptr);
}
int main(int argc, char *argv[])
{
    clock_t start, end;
    double cpu_time;
    int ret;
    if (argc != 2)
    {
        printf("Использование: ftw <начальный_каталог>");
        return 1;
    }
    start = clock();
    ret = myftw(argv[1], myfunc); /* выполняет всю работу */
    end = clock();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Время выполнения: %f с.\n", cpu_time);
    exit(ret);
}

/*
 * Выполняет обход дерева каталогов, начиная с каталога "pathname".
 * Для каждого встреченного файла вызывает пользовательскую функцию func().
 */

static char *fullpath; /* полный путь к каждому из файлов */
static size_t pathlen;

static int /* возвращает то, что вернула функция func() */
myftw(char *pathname, Myfunc *func)
{
    clock_t start, end;
    double cpu_time;
    start = clock();
    fullpath = path_alloc(&pathlen); /* выделить память для PATH_MAX+1 байт */
    end = clock();
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Время выполнения path_alloc: %f с.\n", cpu_time);
    /* (листинг 2.3) */
    if (pathlen <= strlen(pathname))
    {
        pathlen = strlen(pathname) * 2;
        if ((fullpath = realloc(fullpath, pathlen)) == NULL)
        {
            printf("ошибка вызова realloc");
            return 1;
        }
    }
    strcpy(fullpath, pathname);
    return (dopath(func, 0));
}

/*
 * Выполняет обход дерева каталогов, начиная с "fullpath".
 * Если "fullpath" не является каталогом, для него вызывается lstat(),
 * func() и затем выполняется возврат.
 * Для каталогов производится рекурсивный вызов функции.
 */
static int /* возвращает то, что вернула функция func() */
dopath(Myfunc *func, int depth)
{
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;
    if (lstat(fullpath, &statbuf) < 0) /* ошибка вызова функции stat */
        return (func(fullpath, &statbuf, depth, FTW_NS));
    if (S_ISDIR(statbuf.st_mode) == 0) /* не каталог */
        return (func(fullpath, &statbuf, depth, FTW_F));
    /*
     * Это каталог. Сначала вызвать функцию func(),
     * а затем обработать все файлы в этом каталоге.
     */
    if ((ret = func(fullpath, &statbuf, depth, FTW_D)) != 0)
        return (ret);
    n = strlen(fullpath);
    if (n + NAME_MAX + 2 > pathlen)
    { /* увеличить размер буфера */
        pathlen *= 2;
        if ((fullpath = realloc(fullpath, pathlen)) == NULL)
        {
            printf("ошибка вызова realloc");
            return 1;
        }
    }
    fullpath[n++] = '/';
    fullpath[n] = 0;
    depth++;
    if ((dp =
             opendir(fullpath)) == NULL) /* каталог недоступен */
        return (func(fullpath, &statbuf, depth, FTW_DNR));

    while ((dirp = readdir(dp)) != NULL)
    {
        if (strcmp(dirp->d_name, ".") == 0 ||
            strcmp(dirp->d_name, "..") == 0)
            continue;                       /* пропустить каталоги "." и ".." */
        strcpy(&fullpath[n], dirp->d_name); /* добавить имя после слеша */
        if ((ret = dopath(func, depth)) != 0)
            /* рекурсия */
            break; /* выход по ошибке */
    }
    fullpath[n - 1] = 0; /* стереть часть строки от слеша и до конца */

    if (closedir(dp) < 0)
    {
        printf("невозможно закрыть каталог %s", fullpath);
        return 1;
    }
    depth--;
    return (ret);
}