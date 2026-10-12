/* Parallel independent-archive driver. Each child owns its UnZip state.
 * Output isolation is intentional: never race over one extraction directory.
 */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
extern char **environ;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static size_t next_job, job_count;
static int failures, test_only;
static char **archives;
static const char *root;
static char unzip_bin[PATH_MAX];
static void *worker(void *unused)
{
    (void)unused;
    for (;;) {
        size_t index;
        int rc, status;
        pid_t pid;
        char output[PATH_MAX];
        pthread_mutex_lock(&lock);
        index = next_job++;
        pthread_mutex_unlock(&lock);
        if (index >= job_count) break;
        char *args[] = {unzip_bin, test_only ? "-t" : "-o", "-q",
                        archives[index], "-d", output, NULL};
        if (test_only) args[4] = NULL;
        else {
            rc = snprintf(output, sizeof output, "%s/archive-%06zu", root, index + 1);
            if (rc < 0 || (size_t)rc >= sizeof output || mkdir(output, 0700)) {
                fprintf(stderr, "Cannot create isolated output for %s: %s\n",
                        archives[index], strerror(errno));
                goto failed;
            }
        }
        rc = posix_spawn(&pid, unzip_bin, NULL, NULL, args, environ);
        if (rc) {
            fprintf(stderr, "Cannot launch unzip: %s\n", strerror(rc));
            goto failed;
        }
        do { rc = waitpid(pid, &status, 0); } while (rc < 0 && errno == EINTR);
        if (rc < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Archive failed: %s\n", archives[index]);
            goto failed;
        }
        continue;
failed:
        pthread_mutex_lock(&lock);
        failures = 1;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
int main(int argc, char **argv)
{
    long jobs = 0;
    int opt;
    ssize_t n;
    while ((opt = getopt(argc, argv, "j:d:t")) != -1) {
        switch (opt) {
        case 'j': {
            char *end;
            errno = 0;
            jobs = strtol(optarg, &end, 10);
            if (errno || !*optarg || *end || jobs < 1 || jobs > 256) goto usage;
            break;
        }
        case 'd': root = optarg; break;
        case 't': test_only = 1; break;
        default: goto usage;
        }
    }
    if (optind == argc || (!test_only && !root) || (test_only && root)) goto usage;
    n = readlink("/proc/self/exe", unzip_bin, sizeof unzip_bin - 1);
    if (n < 0) { perror("/proc/self/exe"); return 1; }
    unzip_bin[n] = 0;
    char *slash = strrchr(unzip_bin, '/');
    if (!slash || (size_t)(slash - unzip_bin) + sizeof("/unzip") > sizeof unzip_bin) return 1;
    strcpy(slash + 1, "unzip");
    if (access(unzip_bin, X_OK)) { perror(unzip_bin); return 1; }
    if (!test_only) {
        struct stat st;
        if (lstat(root, &st) || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Output root must be an existing real directory\n");
            return 1;
        }
    }
    archives = argv + optind;
    job_count = (size_t)(argc - optind);
    if (!jobs) jobs = sysconf(_SC_NPROCESSORS_ONLN);
    if (jobs < 1) jobs = 1;
    if (jobs > 256) jobs = 256;
    if ((size_t)jobs > job_count) jobs = (long)job_count;
    pthread_t threads[256];
    long started = 0;
    for (; started < jobs; started++) {
        int rc = pthread_create(&threads[started], NULL, worker, NULL);
        if (rc) { fprintf(stderr, "pthread_create: %s\n", strerror(rc)); break; }
    }
    /* Main thread drains remaining jobs if thread creation is constrained. */
    if (started < jobs) worker(NULL);
    for (long i = 0; i < started; i++) pthread_join(threads[i], NULL);
    return failures;
usage:
    fprintf(stderr, "Usage: %s [-j 1..256] {-t | -d existing-directory} archive...\n", argv[0]);
    return 2;
}
