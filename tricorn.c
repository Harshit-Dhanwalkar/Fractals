#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Initial Window dimensions
#define INITIAL_WIDTH 800
#define INITIAL_HEIGHT 800

#define MAX_ITERATIONS 200
#define BAILOUT_RADIUS_SQUARED 4.0

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;
SDL_Texture* g_fractal_texture = NULL;
TTF_Font* g_font = NULL;

// --- Viewing Parameters ---
double g_view_center_re = 0.0;
double g_view_center_im = 0.0;
double g_view_scale = 200.0;

// Panning variables
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// --- Forward Declarations ---
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height);
void drawTricornToTexture();

void map_pixel_to_complex(int px, int py, double* c_re, double* c_im, int texture_width, int texture_height) {
    double rel_x = px - texture_width / 2.0;
    double rel_y = py - texture_height / 2.0;

    *c_re = g_view_center_re + rel_x / g_view_scale;
    *c_im = g_view_center_im + rel_y / g_view_scale;
}

void map_complex_to_pixel(double c_re, double c_im, int* px, int* py, int texture_width, int texture_height) {
    *px = (int)round(texture_width / 2.0 + (c_re - g_view_center_re) * g_view_scale);
    *py = (int)round(texture_height / 2.0 + (c_im - g_view_center_im) * g_view_scale);
}

SDL_Color getColor(int iterations) {
    if (iterations == MAX_ITERATIONS) {
        return (SDL_Color){0, 0, 0, 255};
    }

    int color_index = iterations % 16;

    switch (color_index) {
        case 0: return (SDL_Color){66, 30, 15, 255};   // Dark brown
        case 1: return (SDL_Color){25, 7, 26, 255};    // Dark violet
        case 2: return (SDL_Color){9, 1, 47, 255};     // Deep blue
        case 3: return (SDL_Color){4, 4, 73, 255};     // Dark blue
        case 4: return (SDL_Color){0, 7, 100, 255};    // Blue
        case 5: return (SDL_Color){12, 44, 138, 255};  // Medium blue
        case 6: return (SDL_Color){24, 82, 177, 255};  // Light blue
        case 7: return (SDL_Color){57, 125, 209, 255}; // Sky blue
        case 8: return (SDL_Color){134, 181, 229, 255};// Light cyan
        case 9: return (SDL_Color){211, 236, 248, 255};// Pale blue
        case 10: return (SDL_Color){241, 233, 191, 255};// Light yellow
        case 11: return (SDL_Color){248, 201, 95, 255}; // Gold
        case 12: return (SDL_Color){255, 170, 0, 255};  // Orange
        case 13: return (SDL_Color){204, 128, 0, 255};  // Dark orange
        case 14: return (SDL_Color){153, 87, 0, 255};   // Brown
        case 15: return (SDL_Color){106, 52, 3, 255};   // Dark brown
        default: return (SDL_Color){0, 0, 0, 255};      // Fallback
    }
}

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
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height) {
    SDL_Surface* screenshot = SDL_CreateRGBSurfaceWithFormat(0, window_width, window_height, 32, SDL_PIXELFORMAT_ARGB8888);
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

// --- Function to draw the Tricorn fractal onto g_fractal_texture ---
void drawTricornToTexture() {
    if (!g_renderer || !g_fractal_texture) {
        printf("Renderer or texture not initialized. Skipping drawing.\n");
        return;
    }

    printf("Drawing Tricorn fractal (Center: %.3f, %.3f, Scale: %.2f)...\n",
           g_view_center_re, g_view_center_im, g_view_scale);

    int texture_width, texture_height;
    SDL_QueryTexture(g_fractal_texture, NULL, NULL, &texture_width, &texture_height);

    SDL_SetRenderTarget(g_renderer, g_fractal_texture);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    // Iterate over each pixel in the texture
    for (int py = 0; py < texture_height; ++py) {
        for (int px = 0; px < texture_width; ++px) {
            double c_re, c_im;
            map_pixel_to_complex(px, py, &c_re, &c_im, texture_width, texture_height);

            double z_re = 0.0; // Initial z_re (a)
            double z_im = 0.0; // Initial z_im (b)
            double z_re_squared; // For optimization: z_re*z_re
            double z_im_squared; // For optimization: z_im*z_im

            int iterations = 0;
            while (iterations < MAX_ITERATIONS) {
                z_re_squared = z_re * z_re;
                z_im_squared = z_im * z_im;

                if (z_re_squared + z_im_squared > BAILOUT_RADIUS_SQUARED) {
                    break;
                }

                double next_z_re = z_re_squared - z_im_squared + c_re;
                double next_z_im = -2.0 * z_re * z_im + c_im;

                z_re = next_z_re;
                z_im = next_z_im;

                iterations++;
            }

            SDL_Color color = getColor(iterations);
            SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, color.a);
            SDL_RenderDrawPoint(g_renderer, px, py);
        }
    }

    // Restore default render target
    SDL_SetRenderTarget(g_renderer, NULL);
    printf("Tricorn fractal drawing to texture complete.\n");
}

// --- Reset View Function ---
void reset_view() {
    g_view_center_re = 0.0;
    g_view_center_im = 0.0;
    g_view_scale = 200.0;

    drawTricornToTexture();
}


int main() {
    printf("Left Click + Drag: Pan the view\n");
    printf("Mouse Wheel: Zoom in/out\n");
    printf("R: Reset View\n");
    printf("Click 'Save' button to save an image.\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    g_window = SDL_CreateWindow(
        "Tricorn Fractal",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        INITIAL_WIDTH,
        INITIAL_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (g_window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (g_font == NULL) {
        fprintf(stderr, "Failed to load font! Please check font path: /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf\nSDL_ttf Error: %s\n", TTF_GetError());
    }

    // Create the texture for the fractal plot
    int window_width, window_height;
    SDL_GetWindowSize(g_window, &window_width, &window_height);
    g_fractal_texture = SDL_CreateTexture(g_renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          window_width, window_height);
    if (g_fractal_texture == NULL) {
        fprintf(stderr, "Failed to create fractal texture: %s\n", SDL_GetError());
        if (g_font != NULL) TTF_CloseFont(g_font);
        TTF_Quit();
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    reset_view();

    SDL_Rect screenshotButtonRect;

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        bool re_draw_fractal_texture = false;
        int current_window_width, current_window_height;
        SDL_GetWindowSize(g_window, &current_window_width, &current_window_height);

        screenshotButtonRect = (SDL_Rect){current_window_width - 120, 10, 110, 30};

        while (SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        int new_width = event.window.data1;
                        int new_height = event.window.data2;

                        if (g_fractal_texture != NULL) {
                            SDL_DestroyTexture(g_fractal_texture);
                        }
                        g_fractal_texture = SDL_CreateTexture(g_renderer,
                                                              SDL_PIXELFORMAT_ARGB8888,
                                                              SDL_TEXTUREACCESS_TARGET,
                                                              new_width, new_height);
                        if (g_fractal_texture == NULL) {
                            fprintf(stderr, "Failed to recreate fractal texture after resize: %s\n", SDL_GetError());
                        }
                        re_draw_fractal_texture = true;
                    }
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
                            saveScreenshot(g_renderer, "tricorn_fractal_screenshot.bmp", current_window_width, current_window_height);
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

                        int dx = mouseX - g_last_mouse_x;
                        int dy = mouseY - g_last_mouse_y;

                        g_view_center_re -= (double)dx / g_view_scale;
                        g_view_center_im -= (double)dy / g_view_scale; // Y-axis aligned for complex plane

                        g_last_mouse_x = mouseX;
                        g_last_mouse_y = mouseY;
                        re_draw_fractal_texture = true;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        double zoom_factor;
                        if (event.wheel.y > 0) {
                            zoom_factor = 1.1;
                        } else if (event.wheel.y < 0) {
                            zoom_factor = 1.0 / 1.1;
                        } else {
                            break;
                        }

                        double c_re_mouse, c_im_mouse;
                        map_pixel_to_complex(mouseX, mouseY, &c_re_mouse, &c_im_mouse, current_window_width, current_window_height);

                        g_view_scale *= zoom_factor;

                        int new_px, new_py;
                        map_complex_to_pixel(c_re_mouse, c_im_mouse, &new_px, &new_py, current_window_width, current_window_height);

                        g_view_center_re += (mouseX - new_px) / g_view_scale;
                        g_view_center_im += (mouseY - new_py) / g_view_scale;

                        re_draw_fractal_texture = true;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        reset_view();
                    }
                    break;
            }
        }

        if (re_draw_fractal_texture) {
            drawTricornToTexture();
        }

        // --- Rendering ---
        SDL_RenderCopy(g_renderer, g_fractal_texture, NULL, NULL);

        // Render UI elements on top
        if (g_font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Max Iterations: %d", MAX_ITERATIONS);
            renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.2f (px/unit)", g_view_scale);
            renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Center: (%.3f, %.3f)", g_view_center_re, g_view_center_im);
            renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);

            renderText(g_renderer, g_font, "Left Drag: Pan, Wheel: Zoom", 10, current_window_height - 50, textColor);
            renderText(g_renderer, g_font, "R: Reset View", 10, current_window_height - 20, textColor);

            SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(g_renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(g_renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(g_renderer, g_font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);
        }

        SDL_RenderPresent(g_renderer);
    }

    // --- Cleanup ---
    if (g_fractal_texture != NULL) {
        SDL_DestroyTexture(g_fractal_texture);
    }
    if (g_font != NULL) {
        TTF_CloseFont(g_font);
    }
    TTF_Quit();
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
    }
    SDL_Quit();

    return 0;
}
