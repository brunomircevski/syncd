#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <dirent.h>
#include <time.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/mman.h>

int wake_signal = 0;
int quit_signal = 0;
int recursive_flag = 0;
int big_file_size = 134217728; //128MB 134217728 
int sleep_seconds = 300;
int buffer_size = 1048576; //1MB 1048576
int *buffer = NULL;
char process_name[] = "filesyncd";

void print_usage(char* name) {
    printf("UZYCIE: %s katalog_zrodlowy katalog_docelowy [-R] [-t CZAS] [-m PRÓG_MB]\n", name);
}

void wake_sig_handler(int signum){
    wake_signal = 1;
    syslog (LOG_NOTICE, "Odebrano sygnal. Wybudzanie");
}

void quit_sig_handler(int signum){
    wake_signal = 1;
    quit_signal = 1;
    syslog (LOG_NOTICE, "Odebrano sygnal. Zakonczenie dzialania.");
}

static void create_daemon() {
    pid_t pid;

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGUSR1, wake_sig_handler);
    signal(SIGUSR2, quit_sig_handler);

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    openlog (process_name, LOG_PID, LOG_DAEMON);
}

int copy_small(char* src_filename, char* dst_filename) {
    syslog (LOG_NOTICE, "Kopiowanie %s do %s", src_filename, dst_filename);

    int file = open(src_filename, O_RDONLY);

    int new_file = open(dst_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if(!file) {
        syslog (LOG_ERR, "Nie udalo sie otworzyc pliku zrodlowego %s", src_filename);
        return EXIT_FAILURE;
    }

    if(!new_file) {
        syslog (LOG_ERR, "Nie udalo sie utworzyc pliku %s", dst_filename);
        return EXIT_FAILURE;
    }

    struct stat st;
    stat(src_filename, &st);
    int file_size = st.st_size;

    int i = file_size / buffer_size; 
    int j = file_size % buffer_size;

    while(i>0) {
        read(file, buffer, buffer_size);
        write(new_file, buffer, buffer_size);
        i--;
    }

    if(j>0) {
        read(file, buffer, j);
        write(new_file, buffer, j);
    }

    close(file);
    close(new_file);

    return EXIT_SUCCESS;
}

int remove_dir(char * path) {
    syslog (LOG_NOTICE, "Usuwanie podkatalogu %s", path);
    if(remove(path)) {
        syslog (LOG_NOTICE, "Rekurencyjne usuwanie %s", path);

        struct dirent *dirent;
        struct stat dst_statbuf;
        DIR *dir;

        dir = opendir (path);
        if (dir == NULL) {
            syslog (LOG_ERR, "Nie udalo sie usunac katalogu %s", path);
            return EXIT_FAILURE;
        }

        while ((dirent = readdir(dir)) != NULL) {

            if(strcmp(dirent->d_name,"..") && strcmp(dirent->d_name,".")) {

                char* name = dirent->d_name;

                char a_path[256];
                strcpy(a_path, path);
                strcat(a_path, "/");
                strcat(a_path, name);

                if(dirent->d_type == 4) {
                    remove_dir(a_path);
                } else if(dirent->d_type == 8) {
                    remove(a_path);
                }  
            }
        }    

        closedir (dir);

        if(remove(path)) {
            syslog (LOG_ERR, "Nie udalo sie usunac katalogu %s", path);
            return EXIT_FAILURE;
        };
    }
    return EXIT_SUCCESS;
}

int copy_big(char* src_filename, char* dst_filename) {
    syslog (LOG_NOTICE, "Kopiowanie duzego pliku %s do %s", src_filename, dst_filename);

    char *src, *dest;
    size_t filesize;

    int file = open(src_filename, O_RDONLY);
    int new_file = open(dst_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);

    if(!file) {
        syslog (LOG_ERR, "Nie udalo sie otworzyc pliku zrodlowego %s", src_filename);
        return EXIT_FAILURE;
    }

    if(!new_file) {
        syslog (LOG_ERR, "Nie udalo sie utworzyc pliku %s", dst_filename);
        return EXIT_FAILURE;
    }

    filesize = lseek(file, 0, SEEK_END);

    src = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, file, 0);
 
    ftruncate(new_file, filesize);

    dest = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, new_file, 0);

    memcpy(dest, src, filesize);

    munmap(src, filesize);
    munmap(dest, filesize);

    close(file);
    close(new_file);

    return EXIT_SUCCESS;
}

void copy_file(char* src_filename, char* dst_filename, int size) {
    if(size > big_file_size) {
        copy_big(src_filename, dst_filename);
    } else {
        copy_small(src_filename, dst_filename);
    }
}

int sync_dirs(char* src, char* dst) {

    struct dirent *srcDirent;
    DIR *srcDir;

    srcDir = opendir (src);
    if (srcDir == NULL) {
        syslog (LOG_ERR, "Nie udalo sie otworzyc katalogu %s", src);
        return EXIT_FAILURE;
    }

    struct stat dst_statbuf;
    struct stat src_statbuf;

    if(stat(dst, &dst_statbuf) == -1) {
        mkdir(dst, 0777);
        syslog (LOG_NOTICE, "Tworzenie podkatalogu %s", dst);
    }

    while ((srcDirent = readdir(srcDir)) != NULL) {

        if(strcmp(srcDirent->d_name,"..") && strcmp(srcDirent->d_name,".")) {

            if(srcDirent->d_type == 4 && recursive_flag) {

                char* dirname = srcDirent->d_name;

                char src_path[256];
                strcpy(src_path, src);
                strcat(src_path, "/");
                strcat(src_path, dirname);

                char dst_path[256];
                strcpy(dst_path, dst);
                strcat(dst_path, "/");
                strcat(dst_path, dirname);

                syslog (LOG_NOTICE, "Przeszukiwanie podkatalogu %s", src_path);
                sync_dirs(src_path, dst_path);

            } else if(srcDirent->d_type == 8) {

                char* filename = srcDirent->d_name;

                char dst_path[256];
                strcpy(dst_path, dst);
                strcat(dst_path, "/");
                strcat(dst_path, filename);

                char src_path[256];
                strcpy(src_path, src);
                strcat(src_path, "/");
                strcat(src_path, filename);

                if (stat(src_path, &src_statbuf) == 0) {

                    time_t mtime = src_statbuf.st_mtime;
                    
                    int status = stat(dst_path, &dst_statbuf);

                    if(status == 0) {
                        time_t* mtime_dst = &dst_statbuf.st_mtime;

                        if(*mtime_dst < mtime) {
                            copy_file(src_path, dst_path, src_statbuf.st_size);
                        }
                    } else {
                        copy_file(src_path, dst_path, src_statbuf.st_size);
                    }
                } else {
                    syslog (LOG_ERR, "Nie udalo sie odczytac informacji o pliku %s", src_path);
                }

            }
                
        }
    }    

    closedir (srcDir);

    struct dirent *dstDirent;
    DIR *dstDir;

    dstDir = opendir (dst);
    if (dstDir == NULL) {
        syslog (LOG_ERR, "Nie udalo sie otworzyc katalogu %s", dst);
        return EXIT_FAILURE;
    }

    while ((dstDirent = readdir(dstDir)) != NULL) {

        if(strcmp(dstDirent->d_name,"..") && strcmp(dstDirent->d_name,".")) {

            if(dstDirent->d_type == 4  && recursive_flag) {

                char* dirname = dstDirent->d_name;

                char src_path[256];
                strcpy(src_path, src);
                strcat(src_path, "/");
                strcat(src_path, dirname);

                if(stat(src_path, &src_statbuf) == -1) {

                    char dst_path[256];
                    strcpy(dst_path, dst);
                    strcat(dst_path, "/");
                    strcat(dst_path, dirname);

                    remove_dir(dst_path);
                }

            } else if(dstDirent->d_type == 8) {
                char* filename = dstDirent->d_name;

                char src_path[256]; 
                strcpy(src_path, src);
                strcat(src_path, "/");
                strcat(src_path, filename);

                char dst_path[256];
                strcpy(dst_path, dst);
                strcat(dst_path, "/");
                strcat(dst_path, filename);
        
                if(stat(src_path, &src_statbuf) != 0) {
                    syslog (LOG_NOTICE, "Usuwanie pliku %s", dst_path);
                    remove(dst_path);
                }
            }
        }
    }

    closedir (dstDir);

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    if(argc<3) {
        printf("Podano nieprawidlowa ilosc argumentow!\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    struct stat path1, path2;

    if(stat(argv[1], &path1) == 0) {
        if(!(path1.st_mode & S_IFDIR)) {
            printf("Sciezka zrodlowa nie jest katalogiem!\n");
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        printf("Sciezka zrodlowa nie jest katalogiem!\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if(stat(argv[2], &path2) == 0) {
        if(!(path2.st_mode & S_IFDIR)) {
            printf("Sciezka docelowa nie jest katalogiem!\n");
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        printf("Sciezka docelowa nie jest katalogiem!\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int i = 3;
    while(i<argc) {
        if(!strcmp(argv[i], "-R")) recursive_flag = 1;

        if(!strcmp(argv[i], "-m")) {
            if(i+1 >= argc) break;
            big_file_size = atoi(argv[i+1]);

            if(big_file_size < 1) big_file_size = 1;
            big_file_size *= 1048576;
            i++;
        }

        if(!strcmp(argv[i], "-t")) {
            if(i+1 >= argc) break;
            sleep_seconds = atoi(argv[i+1]);
            i++;
        }

        i++;
    }

    if(sleep_seconds < 2) {
        printf("Czas uspienia nie moze byc krotszy niz 2 sekundy!\n");
        return EXIT_FAILURE;
    }

    if(big_file_size < 0 || big_file_size > 1073741824) {
        printf("Zbyt duzy prog rozmiaru pliku!\n");
        return EXIT_FAILURE;
    }

    char src_path[256], dst_path[256];
    realpath(argv[1], src_path);
    realpath(argv[2], dst_path);

    if(strcmp(src_path, dst_path) == 0) {
        printf("Sciezka zrodlowa i docelowa sa takie same!\n");
        return EXIT_FAILURE; 
    }

    printf("src: %s\ndst: %s\nR: %d\nbig_file_size: %d\nsleep_time: %d\n", src_path, dst_path, recursive_flag, big_file_size, sleep_seconds);

    create_daemon();

    buffer = malloc(buffer_size);

    if(recursive_flag) syslog (LOG_NOTICE, "Synchronizacja rekurencyjna (podkatalogi).");

    while (1)
    {
        syslog (LOG_NOTICE, "Synchronizacja %s z %s", src_path, dst_path);

        sync_dirs(src_path, dst_path);

        syslog (LOG_NOTICE, "Synchronizacja zakonczona. Czekaj %d sekund", sleep_seconds);
        
        int i;
        for (i = 0; i < sleep_seconds; i++) {
            sleep(1);
            if(wake_signal) break;
        }

        if(wake_signal) {
            wake_signal = 0;
        } else {
            syslog (LOG_NOTICE, "Minelo %d sekund. Wybudzanie.", sleep_seconds);
        }

        if(quit_signal) break;
    }

    syslog (LOG_NOTICE, "Usługa poprawnie zatrzymana.");
    free(buffer);
    closelog();

    return EXIT_SUCCESS;
}