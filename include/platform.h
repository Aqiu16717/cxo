/*
 * platform.h - Cross-platform compatibility layer
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#ifndef CXO_PLATFORM_H
#define CXO_PLATFORM_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>

/* Directory creation */
#define cxo_mkdir(path) _mkdir(path)
#define cxo_chdir(path) _chdir(path)
#define cxo_rmdir(path) _rmdir(path)
#define cxo_getpid() _getpid()

/* File access */
#define cxo_access(path, mode) _access((path), (mode))
#ifndef F_OK
#define F_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

/* Stat type macros */
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

/* Socket abstractions */
typedef SOCKET cxo_socket_t;
typedef int cxo_socklen_t;
typedef int cxo_ssize_t;

#define CXO_INVALID_SOCKET INVALID_SOCKET
#define cxo_close_socket(s) closesocket(s)
#define cxo_errno WSAGetLastError()
#define CXO_EINTR WSAEINTR
#define CXO_EAGAIN WSAEWOULDBLOCK
#define CXO_EWOULDBLOCK WSAEWOULDBLOCK

/* Process spawning */
#define cxo_spawnvp(mode, file, argv) \
    _spawnvp((mode), (file), (argv))
#define CXO_P_WAIT _P_WAIT

/* Environment */
#define cxo_setenv(key, val) _putenv_s((key), (val))

/* Pipe operations */
#define cxo_popen(cmd, mode) _popen((cmd), (mode))
#define cxo_pclose(stream) _pclose(stream)

/* C runtime compatibility */
#define cxo_open(path, flags) _open((path), (flags))
#define cxo_read(fd, buf, size) _read((fd), (buf), (unsigned int)(size))
#define cxo_close_file(fd) _close(fd)
#define cxo_fstat(fd, st) _fstat((fd), (st))
#define cxo_strcasecmp(a, b) _stricmp((a), (b))

#else /* POSIX */

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define cxo_mkdir(path) mkdir((path), 0755)
#define cxo_chdir(path) chdir(path)
#define cxo_rmdir(path) rmdir(path)
#define cxo_getpid() getpid()
#define cxo_access(path, mode) access((path), (mode))

typedef int cxo_socket_t;
typedef socklen_t cxo_socklen_t;
typedef ssize_t cxo_ssize_t;

#define CXO_INVALID_SOCKET -1
#define cxo_close_socket(s) close(s)
#define cxo_errno errno
#define CXO_EINTR EINTR
#define CXO_EAGAIN EAGAIN
#define CXO_EWOULDBLOCK EWOULDBLOCK

#define cxo_spawnvp(mode, file, argv) \
    cxo_posix_spawnvp((file), (argv))
#define CXO_P_WAIT 0

#define cxo_setenv(key, val) setenv((key), (val), 1)

#define cxo_popen(cmd, mode) popen((cmd), (mode))
#define cxo_pclose(stream) pclose(stream)

#define cxo_open(path, flags) open((path), (flags))
#define cxo_read(fd, buf, size) read((fd), (buf), (size))
#define cxo_close_file(fd) close(fd)
#define cxo_fstat(fd, st) fstat((fd), (st))
#define cxo_strcasecmp(a, b) strcasecmp((a), (b))

static inline int cxo_posix_spawnvp(const char* file, char* const argv[])
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    execvp(file, argv);
    _exit(127);
  }

  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

#endif /* _WIN32 */

/* Compiler attributes */
#ifdef __GNUC__
#define CXO_UNUSED __attribute__((unused))
#else
#define CXO_UNUSED
#endif

#endif /* CXO_PLATFORM_H */
