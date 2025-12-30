/**
 * =============================================================================
 * ASTERON SYSCALL INTERFACE v1.0
 * =============================================================================
 * 
 * Interface direta para syscalls do sistema operacional.
 * Zero overhead - chamadas diretas ao kernel.
 * 
 * Suporta:
 * - Linux x86_64
 * - Linux ARM64  
 * - macOS x86_64 (Darwin)
 * - Windows x64 (via ntdll)
 * 
 * =============================================================================
 */

#ifndef ASTERON_SYSCALL_H
#define ASTERON_SYSCALL_H

#include "../core/memory/ownership.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DETECÇÃO DE PLATAFORMA
 * ============================================================================= */

#if defined(__linux__)
    #define ASTERON_OS_LINUX 1
    #if defined(__x86_64__)
        #define ASTERON_ARCH_X86_64 1
    #elif defined(__aarch64__)
        #define ASTERON_ARCH_ARM64 1
    #endif
#elif defined(__APPLE__)
    #define ASTERON_OS_DARWIN 1
    #define ASTERON_ARCH_X86_64 1
#elif defined(_WIN32) || defined(_WIN64)
    #define ASTERON_OS_WINDOWS 1
    #define ASTERON_ARCH_X86_64 1
#endif

/* =============================================================================
 * NÚMEROS DE SYSCALL (Linux x86_64)
 * ============================================================================= */

#ifdef ASTERON_OS_LINUX
#ifdef ASTERON_ARCH_X86_64

#define SYS_READ            0
#define SYS_WRITE           1
#define SYS_OPEN            2
#define SYS_CLOSE           3
#define SYS_STAT            4
#define SYS_FSTAT           5
#define SYS_LSTAT           6
#define SYS_POLL            7
#define SYS_LSEEK           8
#define SYS_MMAP            9
#define SYS_MPROTECT        10
#define SYS_MUNMAP          11
#define SYS_BRK             12
#define SYS_IOCTL           16
#define SYS_ACCESS          21
#define SYS_PIPE            22
#define SYS_SELECT          23
#define SYS_SCHED_YIELD     24
#define SYS_MREMAP          25
#define SYS_MSYNC           26
#define SYS_MINCORE         27
#define SYS_MADVISE         28
#define SYS_DUP             32
#define SYS_DUP2            33
#define SYS_PAUSE           34
#define SYS_NANOSLEEP       35
#define SYS_GETITIMER       36
#define SYS_ALARM           37
#define SYS_SETITIMER       38
#define SYS_GETPID          39
#define SYS_SOCKET          41
#define SYS_CONNECT         42
#define SYS_ACCEPT          43
#define SYS_SENDTO          44
#define SYS_RECVFROM        45
#define SYS_SENDMSG         46
#define SYS_RECVMSG         47
#define SYS_SHUTDOWN        48
#define SYS_BIND            49
#define SYS_LISTEN          50
#define SYS_GETSOCKNAME     51
#define SYS_GETPEERNAME     52
#define SYS_SOCKETPAIR      53
#define SYS_CLONE           56
#define SYS_FORK            57
#define SYS_VFORK           58
#define SYS_EXECVE          59
#define SYS_EXIT            60
#define SYS_WAIT4           61
#define SYS_KILL            62
#define SYS_UNAME           63
#define SYS_FCNTL           72
#define SYS_FLOCK           73
#define SYS_FSYNC           74
#define SYS_FDATASYNC       75
#define SYS_TRUNCATE        76
#define SYS_FTRUNCATE       77
#define SYS_GETDENTS        78
#define SYS_GETCWD          79
#define SYS_CHDIR           80
#define SYS_FCHDIR          81
#define SYS_RENAME          82
#define SYS_MKDIR           83
#define SYS_RMDIR           84
#define SYS_CREAT           85
#define SYS_LINK            86
#define SYS_UNLINK          87
#define SYS_SYMLINK         88
#define SYS_READLINK        89
#define SYS_CHMOD           90
#define SYS_FCHMOD          91
#define SYS_CHOWN           92
#define SYS_FCHOWN          93
#define SYS_LCHOWN          94
#define SYS_UMASK           95
#define SYS_GETTIMEOFDAY    96
#define SYS_GETRLIMIT       97
#define SYS_GETRUSAGE       98
#define SYS_SYSINFO         99
#define SYS_TIMES           100
#define SYS_GETUID          102
#define SYS_GETGID          104
#define SYS_SETUID          105
#define SYS_SETGID          106
#define SYS_GETEUID         107
#define SYS_GETEGID         108
#define SYS_GETPPID         110
#define SYS_GETPGRP         111
#define SYS_SETSID          112
#define SYS_GETGROUPS       115
#define SYS_SETGROUPS       116
#define SYS_RT_SIGACTION    13
#define SYS_RT_SIGPROCMASK  14
#define SYS_RT_SIGRETURN    15
#define SYS_CLOCK_GETTIME   228
#define SYS_CLOCK_NANOSLEEP 230
#define SYS_EXIT_GROUP      231
#define SYS_EPOLL_CREATE    213
#define SYS_EPOLL_CTL       233
#define SYS_EPOLL_WAIT      232
#define SYS_OPENAT          257
#define SYS_MKDIRAT         258
#define SYS_FSTATAT         262
#define SYS_UNLINKAT        263
#define SYS_RENAMEAT        264
#define SYS_READLINKAT      267
#define SYS_FCHMODAT        268
#define SYS_FACCESSAT       269
#define SYS_ACCEPT4         288
#define SYS_EPOLL_CREATE1   291
#define SYS_PIPE2           293
#define SYS_GETRANDOM       318

#endif /* ASTERON_ARCH_X86_64 */
#endif /* ASTERON_OS_LINUX */

/* =============================================================================
 * SYSCALL INLINE (ZERO OVERHEAD)
 * ============================================================================= */

#ifdef ASTERON_OS_LINUX
#ifdef ASTERON_ARCH_X86_64

static inline i64 syscall0(i64 n) {
    i64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall1(i64 n, i64 a1) {
    i64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall2(i64 n, i64 a1, i64 a2) {
    i64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall3(i64 n, i64 a1, i64 a2, i64 a3) {
    i64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall4(i64 n, i64 a1, i64 a2, i64 a3, i64 a4) {
    i64 ret;
    register i64 r10 __asm__("r10") = a4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall5(i64 n, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5) {
    i64 ret;
    register i64 r10 __asm__("r10") = a4;
    register i64 r8 __asm__("r8") = a5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline i64 syscall6(i64 n, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5, i64 a6) {
    i64 ret;
    register i64 r10 __asm__("r10") = a4;
    register i64 r8 __asm__("r8") = a5;
    register i64 r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif /* ASTERON_ARCH_X86_64 */
#endif /* ASTERON_OS_LINUX */

/* =============================================================================
 * FALLBACK PARA OUTRAS PLATAFORMAS
 * ============================================================================= */

#ifndef ASTERON_OS_LINUX
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define syscall0(n) syscall(n)
#define syscall1(n, a) syscall(n, a)
#define syscall2(n, a, b) syscall(n, a, b)
#define syscall3(n, a, b, c) syscall(n, a, b, c)
#define syscall4(n, a, b, c, d) syscall(n, a, b, c, d)
#define syscall5(n, a, b, c, d, e) syscall(n, a, b, c, d, e)
#define syscall6(n, a, b, c, d, e, f) syscall(n, a, b, c, d, e, f)
#endif

/* =============================================================================
 * API DE ALTO NÍVEL (ZERO-COST WRAPPERS)
 * ============================================================================= */

/* File I/O */
typedef i32 Fd;

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

/* Open flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_CLOEXEC   0x80000

/* Mode flags */
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

/* Memory protection */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

/* mmap flags */
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20
#define MAP_FAILED      ((void*)-1)

/**
 * Abre arquivo
 */
static inline Fd sys_open(const char* path, i32 flags, i32 mode) {
    #ifdef ASTERON_OS_LINUX
    return (Fd)syscall3(SYS_OPEN, (i64)path, flags, mode);
    #else
    return open(path, flags, mode);
    #endif
}

/**
 * Fecha arquivo
 */
static inline i32 sys_close(Fd fd) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall1(SYS_CLOSE, fd);
    #else
    return close(fd);
    #endif
}

/**
 * Lê de arquivo
 */
static inline isize sys_read(Fd fd, void* buf, usize count) {
    #ifdef ASTERON_OS_LINUX
    return (isize)syscall3(SYS_READ, fd, (i64)buf, count);
    #else
    return read(fd, buf, count);
    #endif
}

/**
 * Escreve em arquivo
 */
static inline isize sys_write(Fd fd, const void* buf, usize count) {
    #ifdef ASTERON_OS_LINUX
    return (isize)syscall3(SYS_WRITE, fd, (i64)buf, count);
    #else
    return write(fd, buf, count);
    #endif
}

/**
 * Mapeia memória
 */
static inline void* sys_mmap(void* addr, usize length, i32 prot, 
                              i32 flags, Fd fd, i64 offset) {
    #ifdef ASTERON_OS_LINUX
    return (void*)syscall6(SYS_MMAP, (i64)addr, length, prot, flags, fd, offset);
    #else
    return mmap(addr, length, prot, flags, fd, offset);
    #endif
}

/**
 * Desmapeia memória
 */
static inline i32 sys_munmap(void* addr, usize length) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall2(SYS_MUNMAP, (i64)addr, length);
    #else
    return munmap(addr, length);
    #endif
}

/**
 * Obtém PID do processo atual
 */
static inline i32 sys_getpid(void) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall0(SYS_GETPID);
    #else
    return getpid();
    #endif
}

/**
 * Sai do processo
 */
static inline void sys_exit(i32 status) {
    #ifdef ASTERON_OS_LINUX
    syscall1(SYS_EXIT_GROUP, status);
    #else
    _exit(status);
    #endif
    __builtin_unreachable();
}

/**
 * Cria diretório
 */
static inline i32 sys_mkdir(const char* path, i32 mode) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall2(SYS_MKDIR, (i64)path, mode);
    #else
    return mkdir(path, mode);
    #endif
}

/**
 * Remove arquivo
 */
static inline i32 sys_unlink(const char* path) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall1(SYS_UNLINK, (i64)path);
    #else
    return unlink(path);
    #endif
}

/**
 * Executa programa
 */
static inline i32 sys_execve(const char* path, char* const argv[], char* const envp[]) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall3(SYS_EXECVE, (i64)path, (i64)argv, (i64)envp);
    #else
    return execve(path, argv, envp);
    #endif
}

/**
 * Fork processo
 */
static inline i32 sys_fork(void) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall0(SYS_FORK);
    #else
    return fork();
    #endif
}

/**
 * Nanosleep
 */
struct TimeSpec {
    i64 tv_sec;
    i64 tv_nsec;
};

static inline i32 sys_nanosleep(const struct TimeSpec* req, struct TimeSpec* rem) {
    #ifdef ASTERON_OS_LINUX
    return (i32)syscall2(SYS_NANOSLEEP, (i64)req, (i64)rem);
    #else
    return nanosleep((struct timespec*)req, (struct timespec*)rem);
    #endif
}

/* =============================================================================
 * WRAPPERS DE CONVENIÊNCIA
 * ============================================================================= */

/**
 * Escreve string em stdout
 */
static inline isize print_str(const char* s) {
    usize len = 0;
    while (s[len]) len++;
    return sys_write(FD_STDOUT, s, len);
}

/**
 * Escreve string em stderr
 */
static inline isize eprint_str(const char* s) {
    usize len = 0;
    while (s[len]) len++;
    return sys_write(FD_STDERR, s, len);
}

/**
 * Aloca memória anônima
 */
static inline void* alloc_pages(usize size) {
    void* p = sys_mmap(0, size, PROT_READ | PROT_WRITE, 
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? 0 : p;
}

/**
 * Libera memória alocada com alloc_pages
 */
static inline void free_pages(void* ptr, usize size) {
    sys_munmap(ptr, size);
}

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_SYSCALL_H */

