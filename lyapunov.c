#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 800
#define MAX_ITER 1000
#define ZOOM_FACTOR 2.0

double g_r_min = 3.81;
double g_r_max = 3.87;
const char *pattern = "AB";

SDL_Color getColor(double lambda) {
    SDL_Color color;
    if (lambda < 0.0) {
        // Stable (λ < 0): blue-toned gradient
        double t = fmax(-lambda, 0.0) * 100; // scale for effect
        color.r = (Uint8)fmod(t * 2, 255);
        color.g = (Uint8)fmod(t * 4, 255);
        color.b = (Uint8)(128 + fmod(t * 6, 127));
    } else {
        // Chaotic (λ ≥ 0): red-yellow gradient
        double t = fmin(lambda, 1.0) * 100;
        color.r = (Uint8)(128 + fmod(t * 8, 127));
        color.g = (Uint8)fmod(t * 4, 255);
        color.b = (Uint8)fmod(t * 2, 255);
    }

    color.a = 255;
    return color;
}

double lyapunov(double ra, double rb) {
    double x = 0.5;
    double lyap = 0.0;
    int len = strlen(pattern);
    for (int i = 0; i < MAX_ITER; i++) {
        double r = pattern[i % len] == 'A' ? ra : rb;
        x = r * x * (1.0 - x);
        if (x <= 0.0 || x >= 1.0) return 1.0;
        lyap += log(fabs(r * (1.0 - 2.0 * x)));
    }
    return lyap / MAX_ITER;
}

void renderFractal(SDL_Renderer *renderer) {
    for (int px = 0; px < WIDTH; px++) {
        for (int py = 0; py < HEIGHT; py++) {
            double ra = g_r_min + (g_r_max - g_r_min) * px / WIDTH;
            double rb = g_r_min + (g_r_max - g_r_min) * py / HEIGHT;
            double lambda = lyapunov(ra, rb);
            SDL_Color c = getColor(lambda);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    }
}

void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void saveScreenshot(SDL_Renderer *renderer, const char *filename) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return;
    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch);
    SDL_SaveBMP(surface, filename);
    SDL_FreeSurface(surface);
    printf("Screenshot saved to %s\n", filename);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow("Lyapunov Swallow", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (!font) {
        printf("Font load failed: %s\n", TTF_GetError());
    }

    SDL_Rect screenshotBtn = {WIDTH - 120, 10, 110, 30};
    bool running = true;
    SDL_Event e;
    bool needs_redraw = true;

    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    int x = e.button.x;
                    int y = e.button.y;

                    if (x >= screenshotBtn.x && x <= screenshotBtn.x + screenshotBtn.w &&
                        y >= screenshotBtn.y && y <= screenshotBtn.y + screenshotBtn.h &&
                        e.button.button == SDL_BUTTON_LEFT) {
                        saveScreenshot(renderer, "lyapunov_swallow.bmp");
                    } else {
                        double r_click = g_r_min + (g_r_max - g_r_min) * x / WIDTH;
                        double range = g_r_max - g_r_min;
                        double new_range = (e.button.button == SDL_BUTTON_LEFT)
                                           ? range / ZOOM_FACTOR
                                           : range * ZOOM_FACTOR;

                        g_r_min = r_click - new_range / 2.0;
                        g_r_max = r_click + new_range / 2.0;
                        needs_redraw = true;
                    }
                    break;
                }
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_r) {
                        g_r_min = 3.81;
                        g_r_max = 3.87;
                        needs_redraw = true;
                    }
                    break;
            }
        }

        if (needs_redraw) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            renderFractal(renderer);

            // Overlay: Screenshot button
            SDL_Color white = {255, 255, 255, 255};
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &screenshotBtn);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &screenshotBtn);
            renderText(renderer, font, "Save", screenshotBtn.x + 10, screenshotBtn.y + 5, white);

            // Text overlays
            char buf[128];
            snprintf(buf, sizeof(buf), "Range: [%.5f, %.5f]", g_r_min, g_r_max);
            renderText(renderer, font, buf, 10, 10, white);
            snprintf(buf, sizeof(buf), "Pattern: %s", pattern);
            renderText(renderer, font, buf, 10, 30, white);

            SDL_RenderPresent(renderer);
            needs_redraw = false;
        }

        SDL_Delay(10);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
