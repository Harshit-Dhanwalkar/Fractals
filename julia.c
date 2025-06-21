#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 800

double g_real_min = -2.0;
double g_real_max = 2.0;
double g_imag_min = -2.0;
double g_imag_max = 2.0;
int g_current_max_iterations = 100;

// The constant 'c' for the Julia set equation: z_n+1 = z_n^2 + c
double complex g_julia_c = -0.7 + 0.27015 * I;

SDL_Color getColor(int iterations, int current_max_iterations_limit) {
    SDL_Color color;
    if (iterations == current_max_iterations_limit) {
        color.r = 0;
        color.g = 0;
        color.b = 0;
        color.a = 255;
    } else {
        double t = (double)iterations / current_max_iterations_limit;

        color.r = (int)(9 * (1 - t) * t * t * t * 255);
        color.g = (int)(15 * (1 - t) * (1 - t) * t * t * 255);
        color.b = (int)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);

        color.r = fmin(255, fmax(0, color.r));
        color.g = fmin(255, fmax(0, color.g));
        color.b = fmin(255, fmax(0, color.b));
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
    if (surface == NULL) {
        printf("Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        printf("Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect renderQuad = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &renderQuad);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// Function to save the current renderer content as a BMP image
void saveScreenshot(SDL_Renderer* renderer, const char* filename) {
    SDL_Surface* screenshot = NULL;
    int w, h;
    SDL_RenderGetLogicalSize(renderer, &w, &h);
    if (w == 0 || h == 0) {
        SDL_GetRendererOutputSize(renderer, &w, &h);
    }

    screenshot = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (screenshot == NULL) {
        printf("Failed to create surface for screenshot: %s\n", SDL_GetError());
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch) != 0) {
        printf("Failed to read pixels for screenshot: %s\n", SDL_GetError());
        SDL_FreeSurface(screenshot);
        return;
    }

    if (SDL_SaveBMP(screenshot, filename) != 0) {
        printf("Failed to save BMP: %s\n", SDL_GetError());
    } else {
        printf("Screenshot saved to %s\n", filename);
    }

    SDL_FreeSurface(screenshot);
}


void calculateAndRenderJulia(SDL_Renderer* renderer, SDL_Texture* fractalTexture, Uint32* pixels) {
    int w, h;
    SDL_QueryTexture(fractalTexture, NULL, NULL, &w, &h);

    double real_width = g_real_max - g_real_min;
    double imag_height = g_imag_max - g_imag_min;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double z_real = g_real_min + (double)x / w * real_width;
            double z_imag = g_imag_min + (double)y / h * imag_height;

            double complex z = z_real + z_imag * I;
            int iterations = 0;

            while (cabs(z) < 2.0 && iterations < g_current_max_iterations) {
                z = z * z + g_julia_c;
                iterations++;
            }

            SDL_Color color = getColor(iterations, g_current_max_iterations);
            pixels[y * w + x] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
        }
    }
    SDL_UpdateTexture(fractalTexture, NULL, pixels, w * sizeof(Uint32));
}

int main() {
    printf("Use Mouse Wheel to zoom in/out.\n");
    printf("Click and Drag with Left Mouse Button to pan.\n");
    printf("Press 'R' to reset zoom, pan, and constant C.\n");
    printf("Click 'Screenshot' button in top-right to save an image.\n");
    printf("Current Constant C: %.5f + %.5fi\n", creal(g_julia_c), cimag(g_julia_c));
    printf("Current Max Iterations: %d\n", g_current_max_iterations);

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland"); // Or "x11" or empty string for default

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *pwindow = SDL_CreateWindow(
        "Julia Set",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (pwindow == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(pwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* fractalTexture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    WIDTH, HEIGHT);
    if (fractalTexture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    Uint32* pixels = (Uint32*)malloc(WIDTH * HEIGHT * sizeof(Uint32));
    if (pixels == NULL) {
        printf("Pixel buffer could not be allocated!\n");
        SDL_DestroyTexture(fractalTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load a font for displaying text and button
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        // Application can still run without font, but text won't display
    }

    // Define the screenshot button's position and size
    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};

    // Initial fractal calculation and render
    calculateAndRenderJulia(renderer, fractalTexture, pixels);

    bool application_running = true;
    SDL_Event event;

    // For mouse drag panning
    int g_mouse_down_x = 0;
    int g_mouse_down_y = 0;
    bool g_is_panning = false;

    while (application_running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        // Check if screenshot button was clicked
                        if (event.button.x >= screenshotButtonRect.x &&
                            event.button.x <= screenshotButtonRect.x + screenshotButtonRect.w &&
                            event.button.y >= screenshotButtonRect.y &&
                            event.button.y <= screenshotButtonRect.y + screenshotButtonRect.h) {
                            saveScreenshot(renderer, "julia_screenshot.bmp");
                        } else {
                            // Start panning
                            g_is_panning = true;
                            g_mouse_down_x = event.button.x;
                            g_mouse_down_y = event.button.y;
                        }
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
                        calculateAndRenderJulia(renderer, fractalTexture, pixels);
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        double zoom_factor_val = (event.wheel.y > 0) ? 0.8 : 1.2; // Zoom in or out

                        double center_r = g_real_min + (g_real_max - g_real_min) / 2.0;
                        double center_i = g_imag_min + (g_imag_max - g_imag_min) / 2.0;

                        double current_width = g_real_max - g_real_min;
                        double current_height = g_imag_max - g_imag_min;

                        g_real_min = center_r - (current_width * zoom_factor_val / 2.0);
                        g_real_max = center_r + (current_width * zoom_factor_val / 2.0);
                        g_imag_min = center_i - (current_height * zoom_factor_val / 2.0);
                        g_imag_max = center_i + (current_height * zoom_factor_val / 2.0);

                        if (event.wheel.y > 0) { // Zooming in
                            g_current_max_iterations = fmin(2000, g_current_max_iterations * 1.2);
                        } else { // Zooming out
                            g_current_max_iterations = fmax(100, g_current_max_iterations / 1.2);
                        }
                        calculateAndRenderJulia(renderer, fractalTexture, pixels);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        g_real_min = -2.0;
                        g_real_max = 2.0;
                        g_imag_min = -2.0;
                        g_imag_max = 2.0;
                        g_current_max_iterations = 100;
                        g_julia_c = -0.7 + 0.27015 * I;
                        calculateAndRenderJulia(renderer, fractalTexture, pixels);
                    }
                    break;
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, fractalTexture, NULL, NULL);

        // Render text overlays
        if (font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            // Display Iterations
            snprintf(text_buffer, sizeof(text_buffer), "Iterations: %d", g_current_max_iterations);
            renderText(renderer, font, text_buffer, 10, 10, textColor);

            // Display Julia Constant C
            snprintf(text_buffer, sizeof(text_buffer), "C: %.5f + %.5fi", creal(g_julia_c), cimag(g_julia_c));
            renderText(renderer, font, text_buffer, 10, 30, textColor);

            // Draw and render text for the screenshot button
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(renderer, font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 7, buttonTextColor);
        }

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    free(pixels);
    SDL_DestroyTexture(fractalTexture);
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0;
}
