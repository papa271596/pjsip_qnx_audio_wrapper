#ifndef __PJ_COMPAT_OS_QNX_H__
#define __PJ_COMPAT_OS_QNX_H__

#define PJ_OS_NAME                     "qnx"
#define PJ_HAS_HIGH_RES_TIMER         1
#define PJ_EMULATE_RWMUTEX            1
#define PJ_THREAD_SET_STACK_SIZE      1
#define PJ_THREAD_ALLOCATE_STACK      1

#define PJ_HAS_ERRNO                  1
#define PJ_HAS_UNISTD_H               1
#define PJ_HAS_POLL                   1
#define PJ_HAS_SELECT                 1
#define PJ_HAS_SYS_TIME_H             1
#define PJ_HAS_SYS_SOCKET_H           1
#define PJ_HAS_NETINET_IN_H           1
#define PJ_HAS_ARPA_INET_H            1
#define PJ_HAS_FLOATING_POINT         1
#define PJ_HAS_ERROR_STRING           1
#define PJ_NATIVE_ERR_POSITIVE        1
#define PJ_MAXPATH                    260
#define PJ_MAX_HOSTNAME               128
#define PJ_IOQUEUE_MAX_HANDLES        64
#define PJ_IOQUEUE_HAS_SAFE_UNREG     1
#define PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY 1
#define PJ_LOG_MAX_SIZE               4000
#define PJ_LOG_USE_STACK_BUFFER       1
#define PJ_LOG_ENABLE_INDENT          1
#define PJ_LOG_INDENT_SIZE            1
#define PJ_LOG_INDENT_CHAR            '.'
#define PJ_LOG_SENDER_WIDTH           14
#define PJ_LOG_THREAD_WIDTH           12
#define PJ_THREAD_DEFAULT_STACK_SIZE  8192

#define PJ_DEBUG                      1
#define PJ_DEBUG_MUTEX                0
#define PJ_TIMER_DEBUG                1
#define PJ_GRP_LOCK_DEBUG             0
#define PJ_POOL_DEBUG                 0
#define PJ_SAFE_POOL                  0

// Additional macros to fix missing types and platform features
#define PJ_ATOMIC_VALUE_TYPE          long
#define PJ_HAS_INET_ATON              1
#define PJ_HAS_IPV6                   1
#define PJ_HAS_LIMITS_H               1

#endif  // __PJ_COMPAT_OS_QNX_H__
