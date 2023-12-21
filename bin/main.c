#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(NULL);

    SDL_Window* window = SDL_CreateWindow("hello world!", 0, 0, 800, 600, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetViewport(renderer, NULL );
    SDL_RenderSetLogicalSize(renderer, 800, 600);

    bool running = true;
    while(running) {
        SDL_RenderPresent(renderer);
        SDL_RenderClear(renderer);

        SDL_Event windowEvent;
        while(SDL_PollEvent(&windowEvent)) {
            if(windowEvent.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    return 0;
}
