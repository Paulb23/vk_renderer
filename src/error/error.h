#ifndef ERROR_H_
#define ERROR_H_

#include "SDL2/SDL.h"

enum Error {
    OK,
    FAILED,
};

void _get_err_string(char r_dest[1024], const char * p_fmt, ...);
void _print_error(const char *p_function, const char *p_file, int p_line, SDL_LogPriority p_priority, const char *p_error, const char *p_user_message);

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#ifndef _STR
#define _STR(m_x) #m_x
#define _MKSTR(m_x) _STR(m_x)
#endif

#ifdef __GNUC__
//#define FUNCTION_STR __PRETTY_FUNCTION__ - too annoying
#define FUNCTION_STR __FUNCTION__
#else
#define FUNCTION_STR __FUNCTION__
#endif

#define INFO_MSG(m_fmt, ...)                                                                                                                    \
    do {                                                                                                                                        \
        char err_msg_macro_str[1024];                                                                                                           \
        _get_err_string(err_msg_macro_str, m_fmt, __VA_ARGS__);                                                                                 \
        _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_INFO, err_msg_macro_str, NULL);                                         \
    } while(0)                                                                                                                                  \

#define ERR_FAIL_COND(m_cond)                                                                                                                   \
    do {                                                                                                                                        \
        if (unlikely(m_cond)) {                                                                                                                 \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_ERROR, "Error: Condition \"" _STR(m_cond) "\" is true.", NULL);     \
            return;                                                                                                                             \
        }                                                                                                                                       \
    } while(0)                                                                                                                                  \

#define ERR_FAIL_COND_V(m_cond, m_retval)                                                                                                       \
    do {                                                                                                                                        \
        if (unlikely(m_cond)) {                                                                                                                 \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_ERROR, "Error: Condition \"" _STR(m_cond) "\" is true.", NULL);     \
            return m_retval;                                                                                                                    \
        }                                                                                                                                       \
    } while(0)

#define ERR_FAIL_INDEX(m_index, m_size)                                                                                                            \
    do {                                                                                                                                           \
        if (unlikely((m_index) < 0) || unlikely((m_index) >= (m_size))) {                                                                          \
            char err_msg_macro_str[1024];                                                                                                          \
            _get_err_string(err_msg_macro_str, "Error: Index %s = %i is out of bounds. (%s = %i) ", _STR(m_index), m_index, _STR(m_size), m_size); \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_ERROR, err_msg_macro_str, NULL);                                       \
            return;                                                                                                                                \
        }                                                                                                                                          \
    } while(0)                                                                                                                                     \

#define ERR_FAIL_UINDEX(m_index, m_size)                                                                                                           \
    do {                                                                                                                                           \
        if (unlikely((m_index) >= (m_size))) {                                                                                                     \
            char err_msg_macro_str[1024];                                                                                                          \
            _get_err_string(err_msg_macro_str, "Error: Index %s = %i is out of bounds. (%s = %i) ", _STR(m_index), m_index, _STR(m_size), m_size); \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_ERROR, err_msg_macro_str, NULL);                                       \
            return;                                                                                                                                \
        }                                                                                                                                          \
    } while(0)

#define CRASH_NULL_MSG(m_param, m_fmt, ...)                                                                                                        \
    do {                                                                                                                                           \
        if (unlikely(m_param == NULL)) {                                                                                                           \
            char err_msg_macro_str[1024];                                                                                                          \
            _get_err_string(err_msg_macro_str, m_fmt, __VA_ARGS__);                                                                                \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_CRITICAL, "FATAL: \"" _STR(m_param) "\" is NULL.", err_msg_macro_str); \
        }                                                                                                                                          \
    } while(0)                                                                                                                                     \

#define CRASH_COND_MSG(m_cond, m_fmt, ...)                                                                                                                  \
    do {                                                                                                                                                    \
        if (unlikely(m_cond)) {                                                                                                                             \
            char err_msg_macro_str[1024];                                                                                                                   \
            _get_err_string(err_msg_macro_str, m_fmt, __VA_ARGS__);                                                                                         \
            _print_error(FUNCTION_STR, __FILE__, __LINE__, SDL_LOG_PRIORITY_CRITICAL, "FATAL: Condition \"" _STR(m_cond) "\" is true.", err_msg_macro_str); \
            exit(1);                                                                                                                                        \
        }                                                                                                                                                   \
    } while(0)                                                                                                                                              \

#endif
