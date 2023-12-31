#include "error.h"
#include <time.h>

#include "SDL2/SDL.h"

void _get_err_string(char r_dest[1024], const char * p_fmt, ...) {
    va_list args;
    va_start( args, p_fmt );
    vsnprintf(r_dest, 1024, p_fmt, args);
    va_end( args );
}

void _print_error(const char *p_function, const char *p_file, int p_line, SDL_LogPriority p_priority, const char *p_error, const char *p_user_message) {

    switch(p_priority) {
        case SDL_LOG_PRIORITY_VERBOSE: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_VERBOSE, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        case SDL_LOG_PRIORITY_DEBUG: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_DEBUG, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        default:
        case SDL_LOG_PRIORITY_INFO: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_INFO, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        case SDL_LOG_PRIORITY_WARN: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_ERROR, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        case SDL_LOG_PRIORITY_ERROR: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_ERROR, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        case SDL_LOG_PRIORITY_CRITICAL: {
            SDL_LogMessage(0, SDL_LOG_PRIORITY_CRITICAL, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
        case SDL_NUM_LOG_PRIORITIES: {
            SDL_LogMessage(0, SDL_NUM_LOG_PRIORITIES, "%s:%s:%i - %s", p_file, p_function, p_line, p_error);
        } break;
    };

    if (p_user_message) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VK renderer", p_user_message, NULL);
    }
}
