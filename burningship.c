#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <SDL2/SDL_ttf.h>

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
        color.a = 255;
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


int main() {
    printf("Burning Ship Fractal Viewer\n");
    printf("Left click to zoom in.\n");
    printf("Right click to zoom out.\n");
    printf("Press 'R' to reset view.\n");
    printf("Click 'Screenshot' button in top-right to save an image.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

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

    // Load a font for displaying text and button
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
    }

    // Create a texture to store the Burning Ship pixels
    SDL_Texture* fractalTexture = SDL_CreateTexture(renderer,
                                                       SDL_PIXELFORMAT_ARGB8888,
                                                       SDL_TEXTUREACCESS_STREAMING,
                                                       WIDTH, HEIGHT);
    if (fractalTexture == NULL) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        if (font != NULL) TTF_CloseFont(font);
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(pwindow);
        SDL_Quit();
        return 1;
    }

    // Pixel buffer for the texture
    uint32_t pixels[WIDTH * HEIGHT];

    // Initial calculation and render
    calculateAndRenderBurningShip(renderer, fractalTexture, pixels);

    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};

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

                    // Check if screenshot button was clicked
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        mouseX >= screenshotButtonRect.x &&
                        mouseX <= screenshotButtonRect.x + screenshotButtonRect.w &&
                        mouseY >= screenshotButtonRect.y &&
                        mouseY <= screenshotButtonRect.y + screenshotButtonRect.h) {
                        saveScreenshot(renderer, "burningship_screenshot.bmp");
                    } else {
                        // Original fractal zoom/pan logic
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

                            // Decrease max iterations when zooming out
                            g_current_max_iterations = fmax(100, g_current_max_iterations / 1.2);

                            calculateAndRenderBurningShip(renderer, fractalTexture, pixels);
                        }
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

        // Render current view information
        if (font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255}; // White color for text

            snprintf(text_buffer, sizeof(text_buffer), "R: [%.3f, %.3f]", g_real_min, g_real_max);
            renderText(renderer, font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "I: [%.3f, %.3f]", g_imag_min, g_imag_max);
            renderText(renderer, font, text_buffer, 10, 40, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Iterations: %d", g_current_max_iterations);
            renderText(renderer, font, text_buffer, 10, 70, textColor);

            // Draw and render text for the screenshot button
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(renderer, font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);
        }

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    SDL_DestroyTexture(fractalTexture);
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0; // Indicate successful execution
}
