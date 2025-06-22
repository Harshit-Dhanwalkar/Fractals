#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 800

#define MAX_RECURSION_DEPTH 5

typedef struct {
    double x;
    double y;
} Point;

double g_view_x_min = 0.0;
double g_view_y_min = 0.0;
double g_view_x_max = (double)WIDTH;
double g_view_y_max = (double)HEIGHT;
int g_current_depth = MAX_RECURSION_DEPTH;
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// Function to render text on the screen
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font) {
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

// Recursive function to draw the Vicsek fractal into the pixel buffer
void drawVicsekFractalRecursive(uint32_t* pixels, Point top_left_fractal_coord, double size_fractal_coord, int depth, SDL_Color color) {
    // Determine the pixel coordinates corresponding to the current fractal square
    int px_start = (int)(((top_left_fractal_coord.x - g_view_x_min) / (g_view_x_max - g_view_x_min)) * WIDTH);
    int py_start = (int)(((top_left_fractal_coord.y - g_view_y_min) / (g_view_y_max - g_view_y_min)) * HEIGHT);
    int px_end = (int)(((top_left_fractal_coord.x + size_fractal_coord - g_view_x_min) / (g_view_x_max - g_view_x_min)) * WIDTH);
    int py_end = (int)(((top_left_fractal_coord.y + size_fractal_coord - g_view_y_min) / (g_view_y_max - g_view_y_min)) * HEIGHT);

    // Calculate pixel dimensions
    int pixel_width = px_end - px_start;
    int pixel_height = py_end - py_start;

    if (depth == 0 || pixel_width <= 1 || pixel_height <= 1) {
        if (px_end <= 0 || py_end <= 0 || px_start >= WIDTH || py_start >= HEIGHT) {
            return;
        }

        // Clip to screen bounds
        int draw_px_start = fmax(0, px_start);
        int draw_py_start = fmax(0, py_start);
        int draw_px_end = fmin(WIDTH, px_end);
        int draw_py_end = fmin(HEIGHT, py_end);

        uint32_t pixel_color_val = (color.a << 24) | (color.r << 16) | (color.g << 8) | (color.b);

        for (int y_pixel = draw_py_start; y_pixel < draw_py_end; y_pixel++) {
            for (int x_pixel = draw_px_start; x_pixel < draw_px_end; x_pixel++) {
                if (x_pixel >= 0 && x_pixel < WIDTH && y_pixel >= 0 && y_pixel < HEIGHT) {
                    pixels[y_pixel * WIDTH + x_pixel] = pixel_color_val;
                }
            }
        }
    } else {
        double new_size = size_fractal_coord / 3.0;

        // Draw the 5 squares that form the cross pattern
        // Center square
        drawVicsekFractalRecursive(pixels, (Point){top_left_fractal_coord.x + new_size, top_left_fractal_coord.y + new_size}, new_size, depth - 1, color);
        // Top middle square
        drawVicsekFractalRecursive(pixels, (Point){top_left_fractal_coord.x + new_size, top_left_fractal_coord.y}, new_size, depth - 1, color);
        // Bottom middle square
        drawVicsekFractalRecursive(pixels, (Point){top_left_fractal_coord.x + new_size, top_left_fractal_coord.y + 2 * new_size}, new_size, depth - 1, color);
        // Left middle square
        drawVicsekFractalRecursive(pixels, (Point){top_left_fractal_coord.x, top_left_fractal_coord.y + new_size}, new_size, depth - 1, color);
        // Right middle square
        drawVicsekFractalRecursive(pixels, (Point){top_left_fractal_coord.x + 2 * new_size, top_left_fractal_coord.y + new_size}, new_size, depth - 1, color);
    }
}

// Function to orchestrate the Vicsek fractal calculation and rendering to texture
void calculateAndRenderVicsek(SDL_Texture* texture, uint32_t* pixels) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        pixels[i] = 0x00000000;
    }

    Point initial_fractal_top_left = {0.0, 0.0};
    double initial_fractal_size = (double)WIDTH;

    // White color for the fractal
    SDL_Color fractal_color = {255, 255, 255, 255};

    printf("Rendering Vicsek Fractal with Depth: %d, View X: [%.2f, %.2f], Y: [%.2f, %.2f]\n", g_current_depth, g_view_x_min, g_view_x_max, g_view_y_min, g_view_y_max);

    drawVicsekFractalRecursive(pixels, initial_fractal_top_left, initial_fractal_size, g_current_depth, fractal_color);

    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
    printf("Vicsek Fractal rendering complete.\n");
}


int main() {
    printf("Vicsek Fractal Viewer\n");
    printf("Left Click + Drag: Pan the view\n");
    printf("Mouse Wheel: Zoom in/out (centered on mouse cursor)\n");
    printf("Up/Down Arrows: Adjust recursion depth\n");
    printf("R: Reset view\n");
    printf("Click 'Screenshot' button to save an image.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *pwindow = SDL_CreateWindow(
        "Vicsek Fractal",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
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

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
    }

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

    uint32_t pixels[WIDTH * HEIGHT];

    // Initial calculation and render
    calculateAndRenderVicsek(fractalTexture, pixels);

    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    {
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        if (event.button.button == SDL_BUTTON_LEFT &&
                            mouseX >= screenshotButtonRect.x &&
                            mouseX <= screenshotButtonRect.x + screenshotButtonRect.w &&
                            mouseY >= screenshotButtonRect.y &&
                            mouseY <= screenshotButtonRect.y + screenshotButtonRect.h) {
                            saveScreenshot(renderer, "vicsek_fractal_screenshot.bmp");
                        } else if (event.button.button == SDL_BUTTON_LEFT) {
                            g_is_panning = true;
                            g_last_mouse_x = mouseX;
                            g_last_mouse_y = mouseY;
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
                        int mouseX = event.motion.x;
                        int mouseY = event.motion.y;

                        // Calculate pixel difference
                        int dx = mouseX - g_last_mouse_x;
                        int dy = mouseY - g_last_mouse_y;

                        // Map pixel difference to fractal coordinate difference
                        double fractal_width = g_view_x_max - g_view_x_min;
                        double fractal_height = g_view_y_max - g_view_y_min;

                        double dx_fractal = (dx / (double)WIDTH) * fractal_width;
                        double dy_fractal = (dy / (double)HEIGHT) * fractal_height;

                        g_view_x_min -= dx_fractal;
                        g_view_x_max -= dx_fractal;
                        g_view_y_min -= dy_fractal;
                        g_view_y_max -= dy_fractal;

                        g_last_mouse_x = mouseX;
                        g_last_mouse_y = mouseY;

                        calculateAndRenderVicsek(fractalTexture, pixels);
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // Map mouse position to current fractal coordinates
                        double current_fractal_x = g_view_x_min + (mouseX / (double)WIDTH) * (g_view_x_max - g_view_x_min);
                        double current_fractal_y = g_view_y_min + (mouseY / (double)HEIGHT) * (g_view_y_max - g_view_y_min);

                        double zoom_factor;
                        if (event.wheel.y > 0) {
                            zoom_factor = 0.8;
                        } else if (event.wheel.y < 0) {
                            zoom_factor = 1.25;
                        } else {
                            break;
                        }

                        // Calculate new view dimensions
                        double new_width = (g_view_x_max - g_view_x_min) * zoom_factor;
                        double new_height = (g_view_y_max - g_view_y_min) * zoom_factor;

                        // Adjust view limits to zoom around the mouse cursor
                        g_view_x_min = current_fractal_x - (new_width / 2.0);
                        g_view_x_max = current_fractal_x + (new_width / 2.0);
                        g_view_y_min = current_fractal_y - (new_height / 2.0);
                        g_view_y_max = current_fractal_y + (new_height / 2.0);

                        calculateAndRenderVicsek(fractalTexture, pixels);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        g_view_x_min = 0.0;
                        g_view_y_min = 0.0;
                        g_view_x_max = (double)WIDTH;
                        g_view_y_max = (double)HEIGHT;
                        g_current_depth = MAX_RECURSION_DEPTH;
                        calculateAndRenderVicsek(fractalTexture, pixels);
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        if (g_current_depth < MAX_RECURSION_DEPTH) {
                            g_current_depth++;
                            calculateAndRenderVicsek(fractalTexture, pixels);
                        }
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        if (g_current_depth > 0) {
                            g_current_depth--;
                            calculateAndRenderVicsek(fractalTexture, pixels);
                        }
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
            SDL_Color textColor = {255, 255, 255, 255};

            double initial_width = (double)WIDTH;
            double current_width = g_view_x_max - g_view_x_min;
            double current_zoom_level = initial_width / current_width;

            snprintf(text_buffer, sizeof(text_buffer), "View X: [%.2f, %.2f]", g_view_x_min, g_view_x_max);
            renderText(renderer, font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Y: [%.2f, %.2f]", g_view_y_min, g_view_y_max);
            renderText(renderer, font, text_buffer, 10, 40, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Depth: %d (Max %d)", g_current_depth, MAX_RECURSION_DEPTH);
            renderText(renderer, font, text_buffer, 10, 70, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Zoom: %.2fx", current_zoom_level);
            renderText(renderer, font, text_buffer, 10, 100, textColor);

            renderText(renderer, font, "Left Click + Drag: Pan, Mouse Wheel: Zoom, Up/Down: Depth, R: Reset", 10, HEIGHT - 20, textColor);


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

    return 0;
}
