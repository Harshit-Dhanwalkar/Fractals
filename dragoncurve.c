#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// PI for degrees to radians conversion
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Initial Window dimensions
#define INITIAL_WIDTH 800
#define INITIAL_HEIGHT 800

// Turtle graphics state
typedef struct {
    double x, y;
    double angle;
} TurtleState;

// L-System parameters
const char* AXIOM = "FX";
const char* RULE_X = "X+YF+";
const char* RULE_Y = "-FX-Y";
const double ANGLE_INCREMENT = M_PI / 2.0;

// Number of iterations
#define MAX_L_SYSTEM_ITERATIONS 20

// Maximum string length to prevent excessive memory allocation
#define MAX_L_SYSTEM_STRING_BUFFER_LENGTH 400000

// Global renderer, window, and font pointers
SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;
SDL_Texture* g_dragon_curve_texture = NULL;
TTF_Font* g_font = NULL;

// --- Viewing Parameters ---
double g_view_x_center = 0.0;
double g_view_y_center = 0.0;
double g_view_scale = 1.0;

// Panning variables
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// Global variable for the L-System string
char* g_l_system_string = NULL;
int g_current_iterations = 14;

// --- FORWARD DECLARATIONS ---
char* generateLSystemString(int iterations);
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);


void getDragonCurveBoundingBox(int iterations, double* min_x, double* max_x, double* min_y, double* max_y) {
    *min_x = *min_y = 1e10;
    *max_x = *max_y = -1e10;

    TurtleState turtle = {
        .x = 0.0,
        .y = 0.0,
        .angle = 0.0
    };

    char* temp_l_system_string = generateLSystemString(iterations);
    if (temp_l_system_string == NULL) return;

    // Approximate step size for bounding box calculation.
    double step_size = 1.0;

    for (size_t i = 0; i < strlen(temp_l_system_string); ++i) {
        char symbol = temp_l_system_string[i];

        switch (symbol) {
            case 'F':
                turtle.x += step_size * cos(turtle.angle);
                turtle.y += step_size * sin(turtle.angle);
                break;
            case '+':
                turtle.angle += ANGLE_INCREMENT;
                break;
            case '-':
                turtle.angle -= ANGLE_INCREMENT;
                break;
        }
        *min_x = fmin(*min_x, turtle.x);
        *max_x = fmax(*max_x, turtle.x);
        *min_y = fmin(*min_y, turtle.y);
        *max_y = fmax(*max_y, turtle.y);
    }
    free(temp_l_system_string);
}


// --- Function to generate the L-System string ---
char* generateLSystemString(int iterations) {
    char* current_s = (char*)malloc(MAX_L_SYSTEM_STRING_BUFFER_LENGTH);
    if (current_s == NULL) {
        fprintf(stderr, "Memory allocation failed for L-System string.\n");
        return NULL;
    }
    strcpy(current_s, AXIOM);

    for (int i = 0; i < iterations; ++i) {
        char* next_s = (char*)malloc(MAX_L_SYSTEM_STRING_BUFFER_LENGTH);
        if (next_s == NULL) {
            fprintf(stderr, "Memory allocation failed for next L-System string.\n");
            free(current_s);
            return NULL;
        }
        next_s[0] = '\0'; // Ensure it's an empty string

        size_t current_len = strlen(current_s);
        for (size_t j = 0; j < current_len; ++j) {
            char symbol = current_s[j];
            if (symbol == 'X') {
                if (strlen(next_s) + strlen(RULE_X) >= MAX_L_SYSTEM_STRING_BUFFER_LENGTH) {
                    fprintf(stderr, "L-System string buffer full during X expansion. Truncating.\n");
                    goto end_generation;
                }
                strcat(next_s, RULE_X);
            } else if (symbol == 'Y') {
                if (strlen(next_s) + strlen(RULE_Y) >= MAX_L_SYSTEM_STRING_BUFFER_LENGTH) {
                    fprintf(stderr, "L-System string buffer full during Y expansion. Truncating.\n");
                    goto end_generation;
                }
                strcat(next_s, RULE_Y);
            } else {
                if (strlen(next_s) + 1 >= MAX_L_SYSTEM_STRING_BUFFER_LENGTH) {
                    fprintf(stderr, "L-System string buffer full during copy. Truncating.\n");
                    goto end_generation;
                }
                size_t next_len = strlen(next_s);
                next_s[next_len] = symbol;
                next_s[next_len + 1] = '\0';
            }
        }
        free(current_s);
        current_s = next_s;
    }

end_generation:
    return current_s;
}

// --- Coordinate Mapping Functions ---
void map_world_to_pixel(double wx, double wy, int* px, int* py, int texture_width, int texture_height) {
    *px = (int)(round(texture_width / 2.0 + (wx - g_view_x_center) * g_view_scale));
    *py = (int)(round(texture_height / 2.0 - (wy - g_view_y_center) * g_view_scale));
}

// Maps a pixel (px, py) to world (wx, wy) for zoom/pan calculations
void map_pixel_to_world(int px, int py, double* wx, double* wy, int texture_width, int texture_height) {
    *wx = g_view_x_center + (px - texture_width / 2.0) / g_view_scale;
    *wy = g_view_y_center - (py - texture_height / 2.0) / g_view_scale;
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

// --- Function to draw the Dragon Curve onto g_dragon_curve_texture ---
void drawDragonCurveToTexture() {
    if (!g_renderer || !g_dragon_curve_texture || g_l_system_string == NULL) {
        printf("Renderer, texture, or L-System string not initialized. Skipping drawing.\n");
        return;
    }

    printf("Drawing Dragon Curve to texture (Iterations: %d)...\n", g_current_iterations);

    int texture_width, texture_height;
    SDL_QueryTexture(g_dragon_curve_texture, NULL, NULL, &texture_width, &texture_height);

    // Set render target to our texture
    SDL_SetRenderTarget(g_renderer, g_dragon_curve_texture);

    // Clear the texture
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    // Set drawing color for the curve
    SDL_SetRenderDrawColor(g_renderer, 0, 150, 255, 255);

    TurtleState turtle = {
        .x = 0.0,
        .y = 0.0,
        .angle = 0.0
    };

    double segment_length = 1.0;

    for (size_t i = 0; i < strlen(g_l_system_string); ++i) {
        char symbol = g_l_system_string[i];
        double prev_wx = turtle.x;
        double prev_wy = turtle.y;

        switch (symbol) {
            case 'F':
                turtle.x += segment_length * cos(turtle.angle);
                turtle.y += segment_length * sin(turtle.angle);
                break;
            case '+':
                turtle.angle += ANGLE_INCREMENT;
                break;
            case '-':
                turtle.angle -= ANGLE_INCREMENT;
                break;
        }

        if (symbol == 'F') {
            int prev_px, prev_py, current_px, current_py;
            map_world_to_pixel(prev_wx, prev_wy, &prev_px, &prev_py, texture_width, texture_height);
            map_world_to_pixel(turtle.x, turtle.y, &current_px, &current_py, texture_width, texture_height);

            if (fabs(current_px - prev_px) < texture_width * 2 && fabs(current_py - prev_py) < texture_height * 2 &&
                (prev_px >= -texture_width && prev_px <= texture_width * 2) &&
                (prev_py >= -texture_height && prev_py <= texture_height * 2) )
            {
                 SDL_RenderDrawLine(g_renderer, prev_px, prev_py, current_px, current_py);
            }
        }
    }

    // Restore default render target
    SDL_SetRenderTarget(g_renderer, NULL);
}

// --- Reset Function ---
void reset_view_and_curve() {
    double min_x, max_x, min_y, max_y;
    getDragonCurveBoundingBox(g_current_iterations, &min_x, &max_x, &min_y, &max_y);

    int window_width, window_height;
    SDL_GetWindowSize(g_window, &window_width, &window_height);

    double curve_width = max_x - min_x;
    double curve_height = max_y - min_y;

    if (curve_width == 0.0) curve_width = 1.0;
    if (curve_height == 0.0) curve_height = 1.0;

    double scale_x = (double)window_width / curve_width * 0.9;
    double scale_y = (double)window_height / curve_height * 0.9;

    g_view_scale = fmin(scale_x, scale_y);
    g_view_x_center = min_x + curve_width / 2.0;
    g_view_y_center = min_y + curve_height / 2.0;

    if (g_l_system_string != NULL) {
        free(g_l_system_string);
    }
    g_l_system_string = generateLSystemString(g_current_iterations);
    drawDragonCurveToTexture();
}


int main() {
    printf("Left Click + Drag: Pan the view\n");
    printf("Mouse Wheel: Zoom in/out\n");
    printf("Up/Down Arrows: Adjust iterations\n");
    printf("R: Reset view and iterations\n");
    printf("Click 'Save' button to save an image.\n");

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "wayland");

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
        "Dragon Curve",
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

    // Create the texture for the Dragon Curve plot
    int window_width, window_height;
    SDL_GetWindowSize(g_window, &window_width, &window_height);
    g_dragon_curve_texture = SDL_CreateTexture(g_renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          window_width, window_height);
    if (g_dragon_curve_texture == NULL) {
        fprintf(stderr, "Failed to create Dragon Curve texture: %s\n", SDL_GetError());
        if (g_font != NULL) TTF_CloseFont(g_font);
        TTF_Quit();
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Initial generation and rendering of the curve to its texture
    g_l_system_string = generateLSystemString(g_current_iterations);
    if (g_l_system_string == NULL) {
        fprintf(stderr, "Failed to generate initial L-System string. Exiting.\n");
        if (g_font != NULL) TTF_CloseFont(g_font);
        TTF_Quit();
        SDL_DestroyTexture(g_dragon_curve_texture);
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }
    reset_view_and_curve();

    SDL_Rect screenshotButtonRect;

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        bool re_draw_curve = false;
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

                        // Recreate the texture with the new dimensions
                        if (g_dragon_curve_texture != NULL) {
                            SDL_DestroyTexture(g_dragon_curve_texture);
                        }
                        g_dragon_curve_texture = SDL_CreateTexture(g_renderer,
                                                              SDL_PIXELFORMAT_ARGB8888,
                                                              SDL_TEXTUREACCESS_TARGET,
                                                              new_width, new_height);
                        if (g_dragon_curve_texture == NULL) {
                            fprintf(stderr, "Failed to recreate Dragon Curve texture after resize: %s\n", SDL_GetError());
                        }
                        re_draw_curve = true;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    {
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        // Check if screenshot button was clicked
                        if (event.button.button == SDL_BUTTON_LEFT &&
                            mouseX >= screenshotButtonRect.x &&
                            mouseX <= screenshotButtonRect.x + screenshotButtonRect.w &&
                            mouseY >= screenshotButtonRect.y &&
                            mouseY <= screenshotButtonRect.y + screenshotButtonRect.h) {
                            saveScreenshot(g_renderer, "dragon_curve_screenshot.bmp", current_window_width, current_window_height);
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
                        re_draw_curve = true;
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
                        re_draw_curve = true;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        reset_view_and_curve();
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        if (g_current_iterations < MAX_L_SYSTEM_ITERATIONS) {
                            g_current_iterations++;
                            printf("Iterations: %d\n", g_current_iterations);
                            free(g_l_system_string);
                            g_l_system_string = generateLSystemString(g_current_iterations);
                            if (g_l_system_string == NULL) {
                                fprintf(stderr, "Failed to re-generate L-System string after iteration increase. Exiting.\n");
                                application_running = false;
                            }
                            re_draw_curve = true;
                        } else {
                            printf("Max iterations (%d) reached.\n", MAX_L_SYSTEM_ITERATIONS);
                        }
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        if (g_current_iterations > 0) {
                            g_current_iterations--;
                            printf("Iterations: %d\n", g_current_iterations);
                            free(g_l_system_string);
                            g_l_system_string = generateLSystemString(g_current_iterations);
                            if (g_l_system_string == NULL) {
                                fprintf(stderr, "Failed to re-generate L-System string after iteration decrease. Exiting.\n");
                                application_running = false; // Critical error
                            }
                            re_draw_curve = true;
                        } else {
                            printf("Min iterations (0) reached.\n");
                        }
                    }
                    break;
            }
        }

        if (re_draw_curve) {
            drawDragonCurveToTexture();
        }

        // --- Rendering ---
        SDL_RenderCopy(g_renderer, g_dragon_curve_texture, NULL, NULL);

        if (g_font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Iterations: %d", g_current_iterations);
            renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.2f (px/unit)", g_view_scale);
            renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Center: (%.2f, %.2f)", g_view_x_center, g_view_y_center);
            renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);

            renderText(g_renderer, g_font, "Left Drag: Pan, Wheel: Zoom, Arrows: Iterations", 10, current_window_height - 50, textColor);
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
    if (g_l_system_string != NULL) {
        free(g_l_system_string);
    }
    if (g_dragon_curve_texture != NULL) {
        SDL_DestroyTexture(g_dragon_curve_texture);
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
