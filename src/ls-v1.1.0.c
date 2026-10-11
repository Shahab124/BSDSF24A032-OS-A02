/*
* Programming Assignment 02: ls v1.1.0
* Feature added: -l option (long listing format)
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls -l /home /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>  /* major(), minor() for device files */

extern int errno;

int long_flag = 0;      /* becomes 1 when the user gives -l */

void do_ls(const char *dir);
void long_listing(const char *dir);
void print_permissions(mode_t mode);
void print_time(time_t mtime);
const char *user_name(uid_t uid);
const char *group_name(gid_t gid);
int num_len(long n);

int main(int argc, char *argv[])
{
    int opt;

    /* getopt() reads options one by one; "l" means we only accept -l */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        if (opt == 'l')
            long_flag = 1;
        else
        {
            /* getopt already printed "invalid option" */
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            exit(1);
        }
    }

    /* optind = index of the first argument that is NOT an option */
    if (optind == argc)
    {
        do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir)
{
    /* -l given: use the new long listing function */
    if (long_flag)
    {
        long_listing(dir);
        return;
    }

    /* otherwise: original simple display */
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/*
 * Prints every file of 'dir' like "ls -l".
 * We read the directory twice:
 *   pass 1: find the widest value of each column (for alignment) and the total blocks
 *   pass 2: print each line using those widths
 */
void long_listing(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[1024];
    int w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    int w_major = 0, w_minor = 0;   /* for device files */
    long total = 0;

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* ---- pass 1: column widths ---- */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        /* full path "dir/name" so lstat works for any directory */
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        total += st.st_blocks;
        if (num_len(st.st_nlink) > w_links)  w_links = num_len(st.st_nlink);
        if ((int)strlen(user_name(st.st_uid)) > w_user)   w_user = strlen(user_name(st.st_uid));
        if ((int)strlen(group_name(st.st_gid)) > w_group) w_group = strlen(group_name(st.st_gid));
        if (num_len(st.st_size) > w_size)    w_size = num_len(st.st_size);

        /* device files show "major, minor" instead of a size */
        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        {
            if (num_len(major(st.st_rdev)) > w_major) w_major = num_len(major(st.st_rdev));
            if (num_len(minor(st.st_rdev)) > w_minor) w_minor = num_len(minor(st.st_rdev));
        }
    }
    /* the size column must be wide enough for "major, minor" too */
    if (w_major > 0 && w_major + 2 + w_minor > w_size)
        w_size = w_major + 2 + w_minor;

    /* st_blocks counts 512-byte blocks, ls shows 1024-byte blocks */
    printf("total %ld\n", total / 2);

    /* ---- pass 2: print ---- */
    rewinddir(dp);      /* go back to the first entry */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        print_permissions(st.st_mode);
        printf(" %*ld", w_links, (long)st.st_nlink);
        printf(" %-*s", w_user, user_name(st.st_uid));
        printf(" %-*s", w_group, group_name(st.st_gid));
        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        {
            char dev[64];
            snprintf(dev, sizeof(dev), "%*u, %*u", w_major, major(st.st_rdev), w_minor, minor(st.st_rdev));
            printf(" %*s ", w_size, dev);
        }
        else
            printf(" %*ld ", w_size, (long)st.st_size);
        print_time(st.st_mtime);
        printf(" %s", entry->d_name);

        /* symbolic link: also show where it points */
        if (S_ISLNK(st.st_mode))
        {
            char target[1024];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }
    closedir(dp);
}

/* Converts st_mode into a string like "drwxr-xr-x" */
void print_permissions(mode_t mode)
{
    char p[11] = "----------";

    /* first character = file type */
    if (S_ISDIR(mode))       p[0] = 'd';
    else if (S_ISLNK(mode))  p[0] = 'l';
    else if (S_ISCHR(mode))  p[0] = 'c';
    else if (S_ISBLK(mode))  p[0] = 'b';
    else if (S_ISFIFO(mode)) p[0] = 'p';
    else if (S_ISSOCK(mode)) p[0] = 's';

    /* owner, group, others: test each bit with & */
    if (mode & S_IRUSR) p[1] = 'r';
    if (mode & S_IWUSR) p[2] = 'w';
    if (mode & S_IXUSR) p[3] = 'x';
    if (mode & S_IRGRP) p[4] = 'r';
    if (mode & S_IWGRP) p[5] = 'w';
    if (mode & S_IXGRP) p[6] = 'x';
    if (mode & S_IROTH) p[7] = 'r';
    if (mode & S_IWOTH) p[8] = 'w';
    if (mode & S_IXOTH) p[9] = 'x';

    /* special bits replace the x: small letter if x is also set, capital if not */
    if (mode & S_ISUID) p[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) p[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) p[9] = (mode & S_IXOTH) ? 't' : 'T';

    printf("%s", p);
}

/*
 * ctime() gives "Sun Oct 11 16:01:23 2026\n"
 *                    ^4         ^20
 * Like ls: files from the last 6 months show the time, older ones show the year.
 */
void print_time(time_t mtime)
{
    char *t = ctime(&mtime);
    time_t now = time(NULL);
    long six_months = 15778476;     /* seconds in half a year */

    if (mtime <= now && now - mtime < six_months)
        printf("%.12s", t + 4);                 /* "Oct 11 16:01" */
    else
        printf("%.6s  %.4s", t + 4, t + 20);    /* "Oct 11  2025" */
}

/* uid number -> user name (or the number if no such user) */
const char *user_name(uid_t uid)
{
    static char buf[32];
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        return pw->pw_name;
    snprintf(buf, sizeof(buf), "%d", uid);
    return buf;
}

/* gid number -> group name (or the number if no such group) */
const char *group_name(gid_t gid)
{
    static char buf[32];
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        return gr->gr_name;
    snprintf(buf, sizeof(buf), "%d", gid);
    return buf;
}

/* how many characters a number takes when printed, e.g. 4096 -> 4 */
int num_len(long n)
{
    char buf[32];
    return snprintf(buf, sizeof(buf), "%ld", n);
}
