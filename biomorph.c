#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 800
#define HEIGHT 800
#define ZOOM_FACTOR 2.0

// Global variables for the fractal view
double g_real_min = -2.0;
double g_real_max = 2.0;
double g_imag_min = -2.0;
double g_imag_max = 2.0;
int g_current_max_iterations = 100;

// The constant 'c' for the Biomorph equation: z_n+1 = z_n^5 + c
// c = 1 + i
double complex g_biomorph_c = 1.0 + 1.0 * I;

// Global SDL components
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Texture* g_fractal_texture = NULL;
uint32_t* g_pixels = NULL;
TTF_Font* g_font = NULL;

// For mouse dragging
bool g_is_panning = false;
int g_mouse_down_x = 0;
int g_mouse_down_y = 0;

// Function to map iterations and final z value to a color
SDL_Color getColor(int iterations, int current_max_iterations_limit, double complex final_z) {
    SDL_Color color;
    if (iterations == current_max_iterations_limit) {
        color.r = 0;
        color.g = 0;
        color.b = 0;
        color.a = 255;
    } else {
        double mu = (double)iterations + 1.0 - log(log(cabs(final_z))) / log(5.0);
        
        double t = fmod(mu * 0.1, 1.0); 

        color.r = (Uint8)(128 + 127 * sin(2 * M_PI * t + M_PI * 0.2));
        color.g = (Uint8)(128 + 127 * sin(2 * M_PI * t + M_PI * 0.90));
        color.b = (Uint8)(128 + 127 * sin(2 * M_PI * t + M_PI * 1.41));
        color.a = 255;
    }
    return color;
}

// Function to render text on the screen
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font) {
        // If font is not loaded, skip text rendering
        return;
    }
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) {
        fprintf(stderr, "TTF_RenderText_Solid Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
    } else {
        SDL_Rect dst_rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst_rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

// Function to save a screenshot of the current window content
void saveScreenshot(SDL_Renderer* renderer, const char* filename) {
    SDL_Surface* screenshot_surface = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (screenshot_surface == NULL) {
        fprintf(stderr, "Failed to create surface for screenshot: %s\n", SDL_GetError());
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, screenshot_surface->format->format, screenshot_surface->pixels, screenshot_surface->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels for screenshot: %s\n", SDL_GetError());
        SDL_FreeSurface(screenshot_surface);
        return;
    }

    if (SDL_SaveBMP(screenshot_surface, filename) != 0) {
        fprintf(stderr, "Failed to save BMP: %s\n", SDL_GetError());
    } else {
        printf("Screenshot saved to %s\n", filename);
    }

    SDL_FreeSurface(screenshot_surface);
}

// Function to calculate and render the Biomorph fractal
void calculateAndRenderBiomorph(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t* pixels) {
    int pitch;
    SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);

    double real_width = g_real_max - g_real_min;
    double imag_height = g_imag_max - g_imag_min;

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            // Map pixel coordinates to complex plane coordinates
            double z_real_initial = g_real_min + (double)x / WIDTH * real_width;
            double z_imag_initial = g_imag_min + (double)y / HEIGHT * imag_height;

            double complex z = z_real_initial + z_imag_initial * I;

            int iterations = 0;
            double complex final_z_at_escape = 0.0 + 0.0 * I;

            while (cabs(z) < 2.0 && iterations < g_current_max_iterations) {
                // z_n+1 = z_n^5 + c
                // Calculate z_n^5
                double complex z_sq = z * z;
                double complex z_cube = z_sq * z;
                double complex z_fourth = z_cube * z;
                z = z * z_fourth + g_biomorph_c;

                iterations++;
            }
            final_z_at_escape = z;

            SDL_Color color = getColor(iterations, g_current_max_iterations, final_z_at_escape);
            pixels[y * (pitch / sizeof(uint32_t)) + x] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
        }
    }
    SDL_UnlockTexture(texture);
}

int main(int argc, char* argv[]) {
    // --- SDL Initialization ---
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    g_window = SDL_CreateWindow("Biomorph Fractal",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                WIDTH,
                                HEIGHT,
                                SDL_WINDOW_SHOWN);
    if (g_window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (g_renderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Create a texture to store fractal pixels
    g_fractal_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (g_fractal_texture == NULL) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Allocate pixel buffer
    g_pixels = (uint32_t*)malloc(WIDTH * HEIGHT * sizeof(uint32_t));
    if (g_pixels == NULL) {
        fprintf(stderr, "Failed to allocate pixel buffer!\n");
        SDL_DestroyTexture(g_fractal_texture);
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Load a font for text rendering
    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (!g_font) {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
    }

    // Initial fractal calculation and render
    calculateAndRenderBiomorph(g_renderer, g_fractal_texture, g_pixels);

    // --- Event Loop ---
    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // Check for screenshot button click
                    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        event.button.x >= screenshotButtonRect.x &&
                        event.button.x <= screenshotButtonRect.x + screenshotButtonRect.w &&
                        event.button.y >= screenshotButtonRect.y &&
                        event.button.y <= screenshotButtonRect.y + screenshotButtonRect.h) {
                        saveScreenshot(g_renderer, "biomorph_fractal_screenshot.bmp");
                    } else if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_panning = true;
                        g_mouse_down_x = event.button.x;
                        g_mouse_down_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_panning = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (g_is_panning) {
                        double delta_x = (double)(event.motion.x - g_mouse_down_x);
                        double delta_y = (double)(event.motion.y - g_mouse_down_y);

                        double real_width = g_real_max - g_real_min;
                        double imag_height = g_imag_max - g_imag_min;

                        g_real_min -= delta_x / WIDTH * real_width;
                        g_real_max -= delta_x / WIDTH * real_width;
                        g_imag_min -= delta_y / HEIGHT * imag_height;
                        g_imag_max -= delta_y / HEIGHT * imag_height;

                        g_mouse_down_x = event.motion.x;
                        g_mouse_down_y = event.motion.y;

                        calculateAndRenderBiomorph(g_renderer, g_fractal_texture, g_pixels);
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        double zoom_factor_val = (event.wheel.y > 0) ? (1.0 / ZOOM_FACTOR) : ZOOM_FACTOR;

                        double center_r = g_real_min + (g_real_max - g_real_min) / 2.0;
                        double center_i = g_imag_min + (g_imag_max - g_imag_min) / 2.0;

                        double current_width = g_real_max - g_real_min;
                        double current_height = g_imag_max - g_imag_min;

                        g_real_min = center_r - (current_width * zoom_factor_val / 2.0);
                        g_real_max = center_r + (current_width * zoom_factor_val / 2.0);
                        g_imag_min = center_i - (current_height * zoom_factor_val / 2.0);
                        g_imag_max = center_i + (current_height * zoom_factor_val / 2.0);

                        if (event.wheel.y > 0) {
                            g_current_max_iterations = fmin(5000, g_current_max_iterations * 1.2);
                        } else {
                            g_current_max_iterations = fmax(100, g_current_max_iterations / 1.2);
                        }

                        calculateAndRenderBiomorph(g_renderer, g_fractal_texture, g_pixels);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        // Reset view
                        g_real_min = -2.0;
                        g_real_max = 2.0;
                        g_imag_min = -2.0;
                        g_imag_max = 2.0;
                        g_current_max_iterations = 100;
                        g_biomorph_c = 1.0 + 1.0 * I;

                        calculateAndRenderBiomorph(g_renderer, g_fractal_texture, g_pixels);
                    }
                    break;
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_fractal_texture, NULL, NULL);

        // Render text overlays
        if (g_font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            // Display Iterations
            snprintf(text_buffer, sizeof(text_buffer), "Iterations: %d", g_current_max_iterations);
            renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

            // Display Biomorph Constant C
            snprintf(text_buffer, sizeof(text_buffer), "C: %.5f + %.5fi", creal(g_biomorph_c), cimag(g_biomorph_c));
            renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

            // Display Current View Bounds
            snprintf(text_buffer, sizeof(text_buffer), "Real: [%.5f, %.5f]", g_real_min, g_real_max);
            renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);
            snprintf(text_buffer, sizeof(text_buffer), "Imag: [%.5f, %.5f]", g_imag_min, g_imag_max);
            renderText(g_renderer, g_font, text_buffer, 10, 70, textColor);

            // Draw and render text for the screenshot button
            SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};
            SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(g_renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(g_renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(g_renderer, g_font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 7, buttonTextColor);
        }

        SDL_RenderPresent(g_renderer);
    }

    // --- Cleanup ---
    if (g_pixels != NULL) {
        free(g_pixels);
    }
    if (g_fractal_texture != NULL) {
        SDL_DestroyTexture(g_fractal_texture);
    }
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
    }
    if (g_font != NULL) {
        TTF_CloseFont(g_font);
    }
    TTF_Quit();
    SDL_Quit();

    return 0;
}
