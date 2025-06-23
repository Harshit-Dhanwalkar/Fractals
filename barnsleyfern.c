#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// Initial Window dimensions
#define INITIAL_WIDTH 800
#define INITIAL_HEIGHT 800

#define NUM_FERN_POINTS 8000000

#define SKIP_INITIAL_POINTS 20

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;
SDL_Texture* g_fern_texture = NULL;
TTF_Font* g_font = NULL;

// --- Viewing Parameters ---
double g_world_x_min = -2.5;
double g_world_x_max = 2.5;
double g_world_y_min = 0.0;
double g_world_y_max = 10.0;

double g_view_x_center = 0.0;
double g_view_y_center = 0.0;
double g_view_scale = 1.0;

bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// --- Forward Declarations ---
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height);
void drawBarnsleyFernToTexture();


void map_world_to_pixel(double wx, double wy, int* px, int* py, int texture_width, int texture_height) {
    *px = (int)round(texture_width / 2.0 + (wx - g_view_x_center) * g_view_scale);
    *py = (int)round(texture_height / 2.0 - (wy - g_view_y_center) * g_view_scale);
}

void map_pixel_to_world(int px, int py, double* wx, double* wy, int texture_width, int texture_height) {
    *wx = g_view_x_center + (px - texture_width / 2.0) / g_view_scale;
    *wy = g_view_y_center - (py - texture_height / 2.0) / g_view_scale;
}

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


// --- Function to draw the Barnsley Fern onto g_fern_texture ---
void drawBarnsleyFernToTexture() {
    if (!g_renderer || !g_fern_texture) {
        printf("Renderer or texture not initialized. Skipping drawing.\n");
        return;
    }

    printf("Generating and drawing Barnsley Fern with %d points...\n", NUM_FERN_POINTS);

    int texture_width, texture_height;
    SDL_QueryTexture(g_fern_texture, NULL, NULL, &texture_width, &texture_height);

    SDL_SetRenderTarget(g_renderer, g_fern_texture);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 0, 180, 0, 255);

    double x = 0.0;
    double y = 0.0;
    double next_x, next_y;
    int px, py;

    srand(time(NULL));

    for (long i = 0; i < NUM_FERN_POINTS; ++i) {
        int r = rand() % 100;

        if (r == 0) {
            next_x = 0;
            next_y = 0.16 * y;
        } else if (r >= 1 && r <= 85) {
            next_x = 0.85 * x + 0.04 * y;
            next_y = -0.04 * x + 0.85 * y + 1.6;
        } else if (r >= 86 && r <= 92) {
            next_x = 0.20 * x - 0.26 * y;
            next_y = 0.23 * x + 0.22 * y + 1.6;
        } else {
            next_x = -0.15 * x + 0.28 * y;
            next_y = 0.26 * x + 0.24 * y + 0.44;
        }

        x = next_x;
        y = next_y;

        if (i >= SKIP_INITIAL_POINTS) {
            map_world_to_pixel(x, y, &px, &py, texture_width, texture_height);
            if (px >= -1 && px < texture_width + 1 && py >= -1 && py < texture_height + 1) {
                SDL_RenderDrawPoint(g_renderer, px, py);
            }
        }
    }

    SDL_SetRenderTarget(g_renderer, NULL);
    printf("Finished drawing Barnsley Fern to texture.\n");
}

// --- Reset View Function ---
void reset_view() {
    int window_width, window_height;
    SDL_GetWindowSize(g_window, &window_width, &window_height);

    double fern_world_width = g_world_x_max - g_world_x_min;
    double fern_world_height = g_world_y_max - g_world_y_min;

    double padding_factor = 0.9;
    double scale_x = window_width / fern_world_width * padding_factor;
    double scale_y = window_height / fern_world_height * padding_factor;

    g_view_scale = fmin(scale_x, scale_y);
    g_view_x_center = g_world_x_min + fern_world_width / 2.0;
    g_view_y_center = g_world_y_min + fern_world_height / 2.0;

    drawBarnsleyFernToTexture();
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
        "Barnsley Fern",
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

    // Create the texture for the Barnsley Fern plot
    int window_width, window_height;
    SDL_GetWindowSize(g_window, &window_width, &window_height);
    g_fern_texture = SDL_CreateTexture(g_renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          window_width, window_height);
    if (g_fern_texture == NULL) {
        fprintf(stderr, "Failed to create Barnsley Fern texture: %s\n", SDL_GetError());
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
        bool re_draw_fern_texture = false;
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

                        if (g_fern_texture != NULL) {
                            SDL_DestroyTexture(g_fern_texture);
                        }
                        g_fern_texture = SDL_CreateTexture(g_renderer,
                                                              SDL_PIXELFORMAT_ARGB8888,
                                                              SDL_TEXTUREACCESS_TARGET,
                                                              new_width, new_height);
                        if (g_fern_texture == NULL) {
                            fprintf(stderr, "Failed to recreate fern texture after resize: %s\n", SDL_GetError());
                        }
                        re_draw_fern_texture = true;
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
                            saveScreenshot(g_renderer, "barnsley_fern_screenshot.bmp", current_window_width, current_window_height);
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

                        g_view_x_center -= (double)dx / g_view_scale;
                        g_view_y_center += (double)dy / g_view_scale;

                        g_last_mouse_x = mouseX;
                        g_last_mouse_y = mouseY;
                        re_draw_fern_texture = true;
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

                        double wx_mouse, wy_mouse;
                        map_pixel_to_world(mouseX, mouseY, &wx_mouse, &wy_mouse, current_window_width, current_window_height);

                        g_view_scale *= zoom_factor;

                        g_view_x_center = wx_mouse - (mouseX - current_window_width / 2.0) / g_view_scale;
                        g_view_y_center = wy_mouse + (mouseY - current_window_height / 2.0) / g_view_scale;
                        re_draw_fern_texture = true;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        reset_view();
                    }
                    break;
            }
        }

        if (re_draw_fern_texture) {
            drawBarnsleyFernToTexture();
        }

        // --- Rendering ---
        SDL_RenderCopy(g_renderer, g_fern_texture, NULL, NULL);

        // Render UI elements on top
        if (g_font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Points: %dM", (int)(NUM_FERN_POINTS / 1000000));
            renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.2f (px/unit)", g_view_scale);
            renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Center: (%.2f, %.2f)", g_view_x_center, g_view_y_center);
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
    if (g_fern_texture != NULL) {
        SDL_DestroyTexture(g_fern_texture);
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
