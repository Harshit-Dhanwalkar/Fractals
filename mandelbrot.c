#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 800
#define ZOOM_FACTOR 2.0

double g_real_min = -2.0;
double g_real_max = 1.0;
double g_imag_min = -1.5;
double g_imag_max = 1.5;
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
        color.r = (iterations * 9) % 255;
        color.g = (iterations * 5) % 255;
        color.b = (iterations * 3) % 255;
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


void calculateAndRenderMandelbrot(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t* pixels) {
    printf("Calculating Mandelbrot for view: R:[%f, %f], I:[%f, %f], Iterations: %d\n",
           g_real_min, g_real_max, g_imag_min, g_imag_max, g_current_max_iterations);

    double complex_width = g_real_max - g_real_min;
    double complex_height = g_imag_max - g_imag_min;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double cr = g_real_min + (x / (double)WIDTH) * complex_width;
            double ci = g_imag_min + (y / (double)HEIGHT) * complex_height;

            double zr = 0.0;
            double zi = 0.0;
            int iterations = 0;

            // Mandelbrot iteration: z_n+1 = z_n^2 + c
            // Keep iterating as long as the magnitude of Z squared is less than 4.0
            while ((zr * zr + zi * zi < 4.0) && (iterations < g_current_max_iterations)) {
                double temp_zr = zr * zr - zi * zi + cr; // New real part
                zi = 2.0 * zr * zi + ci;                 // New imaginary part
                zr = temp_zr;
                iterations++;
            }

            // Get the color for the current pixel based on iterations and the dynamic limit
            SDL_Color pixel_color = getColor(iterations, g_current_max_iterations);

            // Store the color in the pixel buffer (ARGB format)
            pixels[y * WIDTH + x] = (pixel_color.a << 24) |
                                     (pixel_color.r << 16) |
                                     (pixel_color.g << 8)  |
                                     (pixel_color.b);
        }
    }
    // Update the SDL texture with the new pixel data
    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
    printf("Mandelbrot calculation complete.\n");
}


int main() {
    printf("Left click to zoom in.\n");
    printf("Right click to zoom out.\n");
    printf("Press 'R' to reset view.\n");
    printf("Click 'Screenshot' button in top-right to save an image.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

    // Initialize SDL's video subsystem
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


    // Create an SDL window
    SDL_Window *pwindow = SDL_CreateWindow(
        "Mandelbrot Set (Zoomable)",  // Window title
        SDL_WINDOWPOS_CENTERED,       // Initial X position
        SDL_WINDOWPOS_CENTERED,       // Initial Y position
        WIDTH,                        // Width of the window
        HEIGHT,                       // Height of the window
        SDL_WINDOW_SHOWN              // Window flag to show it immediately
    );

    // Check if window creation failed
    if (pwindow == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create an SDL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(pwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* mandelbrotTexture = SDL_CreateTexture(renderer,
                                                         SDL_PIXELFORMAT_ARGB8888,
                                                         SDL_TEXTUREACCESS_STREAMING,
                                                         WIDTH, HEIGHT);
    if (mandelbrotTexture == NULL) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    uint32_t pixels[WIDTH * HEIGHT];

    // Load a font for displaying text and button
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
    }

    // Define the screenshot button's position and size
    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};


    calculateAndRenderMandelbrot(renderer, mandelbrotTexture, pixels);

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
                    // Check if screenshot button was clicked
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        event.button.x >= screenshotButtonRect.x &&
                        event.button.x <= screenshotButtonRect.x + screenshotButtonRect.w &&
                        event.button.y >= screenshotButtonRect.y &&
                        event.button.y <= screenshotButtonRect.y + screenshotButtonRect.h) {
                        saveScreenshot(renderer, "mandelbrot_screenshot.bmp");
                    } else {
                        // Handle Mandelbrot zooming
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        double current_complex_real = g_real_min + (mouseX / (double)WIDTH) * (g_real_max - g_real_min);
                        double current_complex_imag = g_imag_min + (mouseY / (double)HEIGHT) * (g_imag_max - g_imag_min);

                        if (event.button.button == SDL_BUTTON_LEFT) {
                            double new_real_width = (g_real_max - g_real_min) / ZOOM_FACTOR;
                            double new_imag_height = (g_imag_max - g_imag_min) / ZOOM_FACTOR;

                            g_real_min = current_complex_real - (new_real_width / 2.0);
                            g_real_max = current_complex_real + (new_real_width / 2.0);
                            g_imag_min = current_complex_imag - (new_imag_height / 2.0);
                            g_imag_max = current_complex_imag + (new_imag_height / 2.0);

                            g_current_max_iterations = fmin(5000, g_current_max_iterations * 1.2);
                            if (g_current_max_iterations < 100) g_current_max_iterations = 100;

                            calculateAndRenderMandelbrot(renderer, mandelbrotTexture, pixels);
                        } else if (event.button.button == SDL_BUTTON_RIGHT) {
                            double center_real = (g_real_min + g_real_max) / 2.0;
                            double center_imag = (g_imag_min + g_imag_max) / 2.0;

                            double new_real_width = (g_real_max - g_real_min) * ZOOM_FACTOR;
                            double new_imag_height = (g_imag_max - g_imag_min) * ZOOM_FACTOR;

                            g_real_min = center_real - (new_real_width / 2.0);
                            g_real_max = center_real + (new_real_width / 2.0);
                            g_imag_min = center_imag - (new_imag_height / 2.0);
                            g_imag_max = center_imag + (new_imag_height / 2.0);

                            g_current_max_iterations = fmax(100, g_current_max_iterations / 1.2);

                            calculateAndRenderMandelbrot(renderer, mandelbrotTexture, pixels);
                        }
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        g_real_min = -2.0;
                        g_real_max = 1.0;
                        g_imag_min = -1.5;
                        g_imag_max = 1.5;
                        g_current_max_iterations = 100;
                        calculateAndRenderMandelbrot(renderer, mandelbrotTexture, pixels);
                    }
                    break;
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, mandelbrotTexture, NULL, NULL);

        // Render text overlays
        if (font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            // Display Iterations
            snprintf(text_buffer, sizeof(text_buffer), "Iterations: %d", g_current_max_iterations);
            renderText(renderer, font, text_buffer, 10, 10, textColor);

            // Display Current View Bounds
            snprintf(text_buffer, sizeof(text_buffer), "Real: [%.5f, %.5f]", g_real_min, g_real_max);
            renderText(renderer, font, text_buffer, 10, 30, textColor);
            snprintf(text_buffer, sizeof(text_buffer), "Imag: [%.5f, %.5f]", g_imag_min, g_imag_max);
            renderText(renderer, font, text_buffer, 10, 50, textColor);


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
    SDL_DestroyTexture(mandelbrotTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_Quit();

    return 0; // Indicate successful execution
}
