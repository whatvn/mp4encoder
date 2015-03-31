/* 
 * File:   worker.c
 * Copyright : Ethic Consultant @2014
 * Author: hungnv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini.h"
#include <libgearman/gearman.h>
#include <pthread.h>
#include <errno.h>
#include "mp4_encoder.h"
#include "libftp/ftplib.h"
#include <sys/stat.h>

typedef struct {
    int gearman_port;
    const char* gearman_host;
    const char* source;
    const char* dest;
    const char* job_name;
    const char* ftp_server;
    const char* ftp_username;
    const char* ftp_password;
    int workers;
} configuration;

static netbuf *conn = NULL;
static int use_ftp = 0;
static void *convert_worker(gearman_job_st *job, void *cb_arg, size_t *result_size,
        gearman_return_t *ret_ptr);
char *replace_str(char *str, char *old, char *new);
int split(char *str, char c, char ***arr);

int split(char *str, char c, char ***arr) {
    int count = 1;
    int token_len = 1;
    int i = 0;
    char *p;
    char *t;

    p = str;
    while (*p != '\0') {
        if (*p == c)
            count++;
        p++;
    }
    *arr = (char**) malloc(sizeof (char*) * count);
    if (*arr == NULL)
        exit(1);

    p = str;
    while (*p != '\0') {
        if (*p == c) {
            (*arr)[i] = (char*) malloc(sizeof (char) * token_len);
            if ((*arr)[i] == NULL)
                exit(1);

            token_len = 0;
            i++;
        }
        p++;
        token_len++;
    }
    (*arr)[i] = (char*) malloc(sizeof (char) * token_len);
    if ((*arr)[i] == NULL)
        exit(1);

    i = 0;
    p = str;
    t = ((*arr)[i]);
    while (*p != '\0') {
        if (*p != c && *p != '\0') {
            *t = *p;
            t++;
        } else {
            *t = '\0';
            i++;
            t = ((*arr)[i]);
        }
        p++;
    }
    return count;
}

char *replace_str(char *str, char *old, char *new) {
    char *ret, *r;
    char *p, *q;
    size_t oldlen = strlen(old);
    size_t count, retlen, newlen = strlen(new);

    if (oldlen != newlen) {
        for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
            count++;
        /* this is undefined if p - str > PTRDIFF_MAX */
        retlen = p - str + strlen(p) + count * (newlen - oldlen);
    } else
        retlen = strlen(str);

    if ((ret = malloc(retlen + 1)) == NULL)
        return NULL;

    for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
        /* this is undefined if q - p > PTRDIFF_MAX */
        ptrdiff_t l = q - p;
        memcpy(r, p, l);
        r += l;
        memcpy(r, new, newlen);
        r += newlen;
    }
    strcpy(r, p);

    return ret;
}

static int ftp_mkdir(const char *dir, netbuf *conn) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof (tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            FtpMkdir(tmp, conn);
            *p = '/';
        }
    return FtpMkdir(tmp, conn);
}

static void local_mkdir(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof (tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU );
            *p = '/';
        }
    mkdir(tmp, S_IRWXU);
}


static int handler(void* user, const char* section, const char* name,
        const char* value) {
    configuration* pconfig = (configuration*) user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("file", "source")) {
        pconfig->source = strdup(value);
    } else if (MATCH("file", "destination")) {
        pconfig->dest = strdup(value);
    } else if (MATCH("gearman", "host")) {
        pconfig->gearman_host = strdup(value);
    } else if (MATCH("gearman", "port")) {
        pconfig->gearman_port = atoi(value);
    } else if (MATCH("gearman", "worker")) {
        pconfig->workers = atoi(value);
    } else if (MATCH("gearman", "jobname")) {
        pconfig->job_name = strdup(value);
    } else if (MATCH("ftp", "host")) {
        pconfig->ftp_server = strdup(value);
    } else if (MATCH("ftp", "user")) {
        pconfig->ftp_username = strdup(value);
    } else if (MATCH("ftp", "password")) {
        pconfig->ftp_password = strdup(value);
    } else {
        return 0; /* unknown section/name, error */
    }
    return 1;
}

void* gm_worker(void *args) {

    configuration *config = (configuration *) args;

    gearman_return_t ret;
    gearman_worker_st worker;
    if (gearman_worker_create(&worker) == NULL) {
        fprintf(stderr, "Memory allocation failure on worker creation\n");
        exit(1);
    }
    ret = gearman_worker_add_server(&worker, config->gearman_host,
            config->gearman_port);
    if (ret != GEARMAN_SUCCESS) {
        fprintf(stderr, "%s\n", gearman_worker_error(&worker));
        exit(1);
    }
//    printf("jobname: %s\n", config->job_name);
    ret = gearman_worker_add_function(&worker, config->job_name, 0, convert_worker, NULL);
    if (ret != GEARMAN_SUCCESS) {
        fprintf(stderr, "%s\n", gearman_worker_error(&worker));
        exit(1);
    }
    while (1) {
        ret = gearman_worker_work(&worker);
        if (ret != GEARMAN_SUCCESS) {
            fprintf(stderr, "%s\n", gearman_worker_error(&worker));
            break;
        }
    }

    gearman_worker_free(&worker);
    return;
}

int deliver(const char *src_file, netbuf *conn) {
    int ret;
    char **arr = NULL;
    int len = split((char *) src_file, '/', &arr);
    char* directory = replace_str((char *) src_file, arr[len - 1], "");
    ret = ftp_mkdir(directory, conn);
    if (!ret) return -1;
    printf("mkdir: %s\n", FtpLastResponse(conn));
    ret = FtpPut(src_file, src_file, 'I', conn);
    printf("put: %s\n", FtpLastResponse(conn));
    return ret;
}

int main(int argc, char* argv[]) {
    configuration *config = malloc(sizeof (configuration));
//    config = malloc(sizeof (configuration));
    av_register_all();
    int ret;
    int i;
    if (ini_parse("config.ini", handler, config) < 0) {
        printf("Can't load 'config.ini'\n");
        return 1;
    }

    if (config->ftp_server != NULL) {
        if (config->ftp_username == NULL || config->ftp_password == NULL) {
            fprintf(stderr, "FTP configuration error, application will be exited\n");
            exit(2);
        } else {
            use_ftp = 1;
        }
    } else {
        config->ftp_server = "";
        config->ftp_username= "";
        config->ftp_password = "";
    }
    printf("Config loaded from 'config.ini': source=%s, dest=%s, host=%s, "
            "port=%d, jobname=%s,  workers=%d, ftp server = %s, ftp user = %s\n",
            config->source, config->dest, config->gearman_host, config->gearman_port,
            config->job_name, config->workers, config->ftp_server, config->ftp_username);

    // init ftp connection
    if (use_ftp == 1) {
        FtpInit();
        ret = FtpConnect(config->ftp_server, &conn);
        printf("Connecting...\n");
        if (ret == 0) {
            printf("cannot connect to ftp server, return code: %d\n", ret);
            exit(EXIT_FAILURE);
        }
        ret = FtpLogin(config->ftp_username, config->ftp_password, conn);
        printf("Login...\n");
        if (ret == 0) {
            printf("cannot login into ftp server, return code: %d\n", ret);
            exit(EXIT_FAILURE);
        }
    }
    pthread_t *thread_id = malloc(config->workers * sizeof (pthread_t));
    for (i = 0; i < config->workers; i++) {
        printf("Start worker id #%d \n", i);
        pthread_create(&thread_id[i], NULL, gm_worker, config);
    }
    for (i = 0; i < config->workers; i++) {
        pthread_join(thread_id[i], NULL);
    }
    pthread_exit(NULL);
    return 0;
}

static void *convert_worker(gearman_job_st *job, void *cb_arg, size_t *result_size,
        gearman_return_t *ret_ptr) {
    const uint8_t *workload;
    int element = 0;
    char **arr = NULL;
    uint8_t *result;
    (void) cb_arg;
   
    workload = gearman_job_workload(job);
    *result_size = gearman_job_workload_size(job);
     char workload_data[200] = "";
    strncpy(workload_data, workload, *result_size);
    printf("Job information: %s\n", workload_data);
    element = split((char*) &workload_data, ':', &arr);
    if (element != 2) {
        printf("Invalid input format\n");
        *ret_ptr = GEARMAN_FAIL;
        return NULL;
    }
    char* src = arr[0];
    char* dst = arr[1];
    if (src == NULL || dst == NULL) {
        printf("Invalid input format: src: %s, dst: %s\n", src, dst);
        *ret_ptr = GEARMAN_FAIL;
        return NULL;
    }
    int res = convert(src, dst);
    if (res < 0) {
        *ret_ptr = GEARMAN_FAIL;
        printf("%s : Convert fail\n", workload_data);
        return NULL;
    }
    if (use_ftp == 1) {
        res = deliver(dst, conn);
        if (res < 0) {
            printf("cannot deliver file: %s to ftp server\n", dst);
        } else {
            printf("deliver file %s to ftp server successfully\n", dst);
        }
    }
    result = malloc(2);
    snprintf((char *) result, element, "%d", element);
    printf("Job=%s data input=%.*s Result=%d\n", gearman_job_handle(job),
            (int) *result_size, workload, element);
    *result_size = element;
    *ret_ptr = GEARMAN_SUCCESS;
    return result;
}