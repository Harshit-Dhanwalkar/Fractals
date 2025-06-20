#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>

#define WIDTH 800
#define HEIGHT 800
#define ZOOM_FACTOR 2.0

double g_real_min = -1.8;
double g_real_max = -0.0;
double g_imag_min = -2.0;
double g_imag_max = -0.0;
int g_current_max_iterations = 100;

// Function to map iterations to a color
SDL_Color getColor(int iterations, int current_max_iterations_limit) {
    SDL_Color color;
    if (iterations == current_max_iterations_limit) {
        color.r = 0;
        color.g = 0;
        color.b = 0;
        color.a = 255;
    } else {
        // Fiery coloring based on the number of iterations
        double t = (double)iterations / current_max_iterations_limit;

        int r = (int)(255 * pow(t, 0.5));
        int g = (int)(255 * pow(t, 1.5));
        int b = (int)(255 * pow(t, 3.0));

        r = fmin(255, fmax(0, r));
        g = fmin(255, fmax(0, g));
        b = fmin(255, fmax(0, b));

        color.r = r;
        color.g = g;
        color.b = b;
        color.a = 255; // Fully opaque
    }
    return color;
}

// Function to calculate and render the Burning Ship fractal
void calculateAndRenderBurningShip(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t* pixels) {
    printf("Calculating Burning Ship for view: R:[%f, %f], I:[%f, %f], Iterations: %d\n",
           g_real_min, g_real_max, g_imag_min, g_imag_max, g_current_max_iterations);

    double complex_width = g_real_max - g_real_min;
    double complex_height = g_imag_max - g_imag_min;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            // Map pixel coordinates to fractal coordinates (c)
            double cr = g_real_min + (x / (double)WIDTH) * complex_width;
            double ci = g_imag_min + (y / (double)HEIGHT) * complex_height;

            double complex c = cr + I * ci;

            double zr = 0.0; // Real part of z
            double zi = 0.0; // Imaginary part of z
            int iterations = 0;

            // Burning Ship iteration: z_n+1 = (|re(z_n)| + i * |im(z_n)|)^2 + c
            // Keep iterating as long as the magnitude of Z squared is less than 4.0
            while ((zr * zr + zi * zi < 4.0) && (iterations < g_current_max_iterations)) {
                double abs_zr = fabs(zr);
                double abs_zi = fabs(zi);

                double temp_zr = abs_zr * abs_zr - abs_zi * abs_zi + cr; // New real part
                zi = 2.0 * abs_zr * abs_zi + ci;                          // New imaginary part
                zr = temp_zr;
                iterations++;
            }

            // Get the color for the current pixel
            SDL_Color pixel_color = getColor(iterations, g_current_max_iterations);

            // Store the color in the pixel buffer (ARGB format)
            pixels[y * WIDTH + x] = (pixel_color.a << 24) |
                                    (pixel_color.r << 16) |
                                    (pixel_color.g << 8)  |
                                    (pixel_color.b);
        }
    }
    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
    printf("Burning Ship calculation complete.\n");
}


int main() {
    printf("Burning Ship Fractal Viewer\n");
    printf("Left click to zoom in.\n");
    printf("Right click to zoom out.\n");
    printf("Press 'R' to reset view.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create an SDL window
    SDL_Window *pwindow = SDL_CreateWindow(
        "Burning Ship Fractal (Zoomable)",
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

    // Create a texture to store the Burning Ship pixels
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

    // Pixel buffer for the texture
    uint32_t pixels[WIDTH * HEIGHT];

    // Initial calculation and render
    calculateAndRenderBurningShip(renderer, fractalTexture, pixels);

    // --- Event Loop ---
    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    // Calculate the complex coordinate where the mouse clicked
                    double current_complex_real = g_real_min + (mouseX / (double)WIDTH) * (g_real_max - g_real_min);
                    double current_complex_imag = g_imag_min + (mouseY / (double)HEIGHT) * (g_imag_max - g_imag_min);

                    if (event.button.button == SDL_BUTTON_LEFT) {
                        // Zoom in
                        double new_real_width = (g_real_max - g_real_min) / ZOOM_FACTOR;
                        double new_imag_height = (g_imag_max - g_imag_min) / ZOOM_FACTOR;

                        g_real_min = current_complex_real - (new_real_width / 2.0);
                        g_real_max = current_complex_real + (new_real_width / 2.0);
                        g_imag_min = current_complex_imag - (new_imag_height / 2.0);
                        g_imag_max = current_complex_imag + (new_imag_height / 2.0);

                        // Increase max iterations for more detail when zooming in
                        g_current_max_iterations = fmin(5000, g_current_max_iterations * 1.2);
                        // Ensure a minimum number of iterations
                        if (g_current_max_iterations < 100) g_current_max_iterations = 100;

                        calculateAndRenderBurningShip(renderer, fractalTexture, pixels);
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        // Zoom out
                        double center_real = (g_real_min + g_real_max) / 2.0;
                        double center_imag = (g_imag_min + g_imag_max) / 2.0;

                        double new_real_width = (g_real_max - g_real_min) * ZOOM_FACTOR;
                        double new_imag_height = (g_imag_max - g_imag_min) * ZOOM_FACTOR;

                        g_real_min = center_real - (new_real_width / 2.0);
                        g_real_max = center_real + (new_real_width / 2.0);
                        g_imag_min = center_imag - (new_imag_height / 2.0);
                        g_imag_max = center_imag + (new_imag_height / 2.0);

                        // Decrease max iterations when zooming out (less detail needed)
                        g_current_max_iterations = fmax(100, g_current_max_iterations / 1.2);

                        calculateAndRenderBurningShip(renderer, fractalTexture, pixels);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        // Reset view to initial parameters
                        g_real_min = -1.8;
                        g_real_max = -0.0;
                        g_imag_min = -2.0;
                        g_imag_max = -0.0;
                        g_current_max_iterations = 100;
                        calculateAndRenderBurningShip(renderer, fractalTexture, pixels);
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
