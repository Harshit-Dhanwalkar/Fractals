#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>

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

// Function to calculate and render the Julia Set
void calculateAndRenderJulia(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t* pixels) {
    printf("Calculating Julia Set for view: R:[%f, %f], I:[%f, %f], Iterations: %d, C: %f + %fi\n",
           g_real_min, g_real_max, g_imag_min, g_imag_max, g_current_max_iterations,
           creal(g_julia_c), cimag(g_julia_c));

    double complex_width = g_real_max - g_real_min;
    double complex_height = g_imag_max - g_imag_min;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double z0_r = g_real_min + (x / (double)WIDTH) * complex_width;
            double z0_i = g_imag_min + (y / (double)HEIGHT) * complex_height;

            double complex z = z0_r + I * z0_i;
            int iterations = 0;

            while ((cabs(z) < 2.0) && (iterations < g_current_max_iterations)) {
                z = z * z + g_julia_c;
                iterations++;
            }

            SDL_Color pixel_color = getColor(iterations, g_current_max_iterations);

            pixels[y * WIDTH + x] = (pixel_color.a << 24) |
                                    (pixel_color.r << 16) |
                                    (pixel_color.g << 8)  |
                                    (pixel_color.b);
        }
    }
    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
    printf("Julia Set calculation complete.\n");
}


int main() {
    printf("Julia Set Viewer\n");
    printf("Left click to change 'c' based on mouse position.\n");
    printf("Right click to reset 'c'.\n");
    printf("Mouse wheel to zoom in/out.\n");
    printf("Press 'R' to reset view and 'c' value.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

    // Initialize SDL's video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create an SDL window
    SDL_Window *pwindow = SDL_CreateWindow(
        "Julia Set (Zoomable and Interactive C)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,                 
        HEIGHT,                
        SDL_WINDOW_SHOWN       
    );

    // Check if window creation failed
    if (pwindow == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create an SDL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(pwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(pwindow);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* fractalTexture = SDL_CreateTexture(renderer,
                                                       SDL_PIXELFORMAT_ARGB8888,
                                                       SDL_TEXTUREACCESS_STREAMING,
                                                       WIDTH, HEIGHT);
    if (fractalTexture == NULL) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(pwindow);
        SDL_Quit();
        return 1;
    }

    uint32_t pixels[WIDTH * HEIGHT];

    calculateAndRenderJulia(renderer, fractalTexture, pixels);

    // --- Event Loop ---
    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        if (event.button.button == SDL_BUTTON_LEFT) {
                            double new_cr = -2.0 + (mouseX / (double)WIDTH) * 3.0;
                            double new_ci = -1.5 + (mouseY / (double)HEIGHT) * 3.0;

                            g_julia_c = new_cr + new_ci * I;
                            printf("Julia 'c' changed to: %f + %fi\n", creal(g_julia_c), cimag(g_julia_c));
                            calculateAndRenderJulia(renderer, fractalTexture, pixels);
                        } else if (event.button.button == SDL_BUTTON_RIGHT) {
                            g_julia_c = -0.7 + 0.27015 * I;
                            printf("Julia 'c' reset to default: %f + %fi\n", creal(g_julia_c), cimag(g_julia_c));
                            calculateAndRenderJulia(renderer, fractalTexture, pixels);
                        }
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                        double zoom_factor_val = 1.2;
                        double center_r = (g_real_min + g_real_max) / 2.0;
                        double center_i = (g_imag_min + g_imag_max) / 2.0;

                        double current_width = g_real_max - g_real_min;
                        double current_height = g_imag_max - g_imag_min;

                        if (event.wheel.y > 0) {
                            g_real_min = center_r - (current_width / (2.0 * zoom_factor_val));
                            g_real_max = center_r + (current_width / (2.0 * zoom_factor_val));
                            g_imag_min = center_i - (current_height / (2.0 * zoom_factor_val));
                            g_imag_max = center_i + (current_height / (2.0 * zoom_factor_val));
                            g_current_max_iterations = fmin(5000, g_current_max_iterations * 1.2);
                        } else if (event.wheel.y < 0) {
                            g_real_min = center_r - (current_width * (2.0 * zoom_factor_val) / 2.0);
                            g_real_max = center_r + (current_width * (2.0 * zoom_factor_val) / 2.0);
                            g_imag_min = center_i - (current_height * (2.0 * zoom_factor_val) / 2.0);
                            g_imag_max = center_i + (current_height * (2.0 * zoom_factor_val) / 2.0);
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
        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    SDL_DestroyTexture(fractalTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0; // Indicate successful execution
}
