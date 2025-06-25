#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

#ifndef M_PIF
#define M_PIF 3.14159265358979323846f
#endif

// Window dimensions
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800

// Maximum number of points to simulate for the attractor trajectory
#define MAX_ATTRACTOR_POINTS 50000 

// Number of initial points to skip
#define SKIP_INITIAL_POINTS 5000 

// Define a reasonable boundary multiplier for projected points
#define DRAW_BOUND_MULTIPLIER 2.0f

// --- Chen-Lee Attractor Parameters ---
float g_a = 5.0f;
float g_b = -10.0f;
float g_c = -0.38f;

// Simulation time step (dt) for the RK4 integration
float g_dt = 0.01f; 

// Structure to represent a 3D vector or point
typedef struct {
    float x, y, z;
} Vec3D;

// Initial state of the attractor trajectory
Vec3D g_current_state = {1.0f, 0.0f, 4.5f};

// Dynamic array to store the pre-calculated attractor points
Vec3D* g_attractor_points = NULL;
int g_num_current_attractor_points = 0;

// --- 3D Viewing Parameters ---
float g_camera_x = -15.0f;
float g_camera_y = -15.0f;
float g_camera_z = 90.0f;

// Initial rotations set for a good default view of this specific Chen-Lee attractor
float g_rotation_x = M_PIF;
float g_rotation_y = -M_PIF;

float g_view_scale = 10.0f; 

// Attractor's calculated centroid
Vec3D g_attractor_centroid = {0.0f, 0.0f, 0.0f};

// --- Interaction State Variables ---
bool g_is_rotating = false;
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// Global SDL components
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
TTF_Font* g_font = NULL;

// --- Forward Declarations ---
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height);
void calculateAttractorPoints();
void drawAttractor();
Vec3D chen_lee_deriv(Vec3D current_state);
Vec3D apply_rotation(Vec3D p);
Vec3D project_to_2d(Vec3D p);


Vec3D chen_lee_deriv(Vec3D current_state) {
    Vec3D deriv;
    deriv.x = g_a * current_state.x - current_state.y * current_state.z;
    deriv.y = g_b * current_state.y + current_state.x * current_state.z;
    deriv.z = g_c * current_state.z + (current_state.x * current_state.y) / 3.0f;
    return deriv;
}

// Calculates and stores the trajectory points of the Chen-Lee attractor using RK4.
// Also calculates the centroid of the stable part of the attractor.
void calculateAttractorPoints() {
    printf("Calculating Chen-Lee Attractor points...\n");

    if (g_attractor_points != NULL) {
        free(g_attractor_points);
        g_attractor_points = NULL;
    }

    g_attractor_points = (Vec3D*)malloc(MAX_ATTRACTOR_POINTS * sizeof(Vec3D));
    if (g_attractor_points == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for attractor points!\n");
        g_num_current_attractor_points = 0;
        return;
    }
    g_num_current_attractor_points = 0; 
    printf("Allocated %zu bytes for %d points.\n",
           MAX_ATTRACTOR_POINTS * sizeof(Vec3D), MAX_ATTRACTOR_POINTS);

    Vec3D Yn = g_current_state; 
    g_attractor_points[g_num_current_attractor_points++] = Yn;

    // Variables for centroid calculation
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    int points_for_centroid = 0;

    for (int i = 1; i < MAX_ATTRACTOR_POINTS; ++i) {
        Vec3D k1 = chen_lee_deriv(Yn);

        Vec3D Y_temp_k2;
        Y_temp_k2.x = Yn.x + 0.5f * g_dt * k1.x;
        Y_temp_k2.y = Yn.y + 0.5f * g_dt * k1.y;
        Y_temp_k2.z = Yn.z + 0.5f * g_dt * k1.z;
        Vec3D k2 = chen_lee_deriv(Y_temp_k2);

        Vec3D Y_temp_k3;
        Y_temp_k3.x = Yn.x + 0.5f * g_dt * k2.x;
        Y_temp_k3.y = Yn.y + 0.5f * g_dt * k2.y;
        Y_temp_k3.z = Yn.z + 0.5f * g_dt * k2.z;
        Vec3D k3 = chen_lee_deriv(Y_temp_k3);

        Vec3D Y_temp_k4;
        Y_temp_k4.x = Yn.x + g_dt * k3.x;
        Y_temp_k4.y = Yn.y + g_dt * k3.y;
        Y_temp_k4.z = Yn.z + g_dt * k3.z;
        Vec3D k4 = chen_lee_deriv(Y_temp_k4);

        Yn.x = Yn.x + (g_dt / 6.0f) * (k1.x + 2*k2.x + 2*k3.x + k4.x);
        Yn.y = Yn.y + (g_dt / 6.0f) * (k1.y + 2*k2.y + 2*k3.y + k4.y);
        Yn.z = Yn.z + (g_dt / 6.0f) * (k1.z + 2*k2.z + 2*k3.z + k4.z);

        g_attractor_points[g_num_current_attractor_points++] = Yn;

        // Accumulate points for centroid calculation, only after the skip period
        if (i >= SKIP_INITIAL_POINTS) {
            sum_x += Yn.x;
            sum_y += Yn.y;
            sum_z += Yn.z;
            points_for_centroid++;
        }
    }

    // Calculate centroid
    if (points_for_centroid > 0) {
        g_attractor_centroid.x = sum_x / points_for_centroid;
        g_attractor_centroid.y = sum_y / points_for_centroid;
        g_attractor_centroid.z = sum_z / points_for_centroid;
        printf("Attractor Centroid: (%.2f, %.2f, %.2f)\n", 
               g_attractor_centroid.x, g_attractor_centroid.y, g_attractor_centroid.z);
    } else {
        g_attractor_centroid = (Vec3D){0.0f, 0.0f, 0.0f};
        printf("Could not calculate centroid (not enough points after skip).\n");
    }

    printf("Finished calculating %d points.\n", g_num_current_attractor_points);
}


Vec3D apply_rotation(Vec3D p) {
    Vec3D rotated_p = p;
    float temp_y, temp_z;

    temp_y = rotated_p.y * cosf(g_rotation_x) - rotated_p.z * sinf(g_rotation_x);
    temp_z = rotated_p.y * sinf(g_rotation_x) + rotated_p.z * cosf(g_rotation_x);
    rotated_p.y = temp_y;
    rotated_p.z = temp_z;

    float temp_x, temp_z2;
    temp_x = rotated_p.x * cosf(g_rotation_y) + rotated_p.z * sinf(g_rotation_y);
    temp_z2 = -rotated_p.x * sinf(g_rotation_y) + rotated_p.z * cosf(g_rotation_y);
    rotated_p.x = temp_x;
    rotated_p.z = temp_z2;

    return rotated_p;
}

Vec3D project_to_2d(Vec3D p) {
    // Offset the point by the attractor's centroid to effectively center it.
    p.x -= g_attractor_centroid.x;
    p.y -= g_attractor_centroid.y;
    p.z -= g_attractor_centroid.z;

    // Translate the point relative to the camera's position
    p.x -= g_camera_x;
    p.y -= g_camera_y;
    p.z -= g_camera_z;

    p = apply_rotation(p);

    float perspective_denominator = (1.0f + p.z / 300.0f); 
    float perspective_factor;

    if (fabsf(perspective_denominator) < 0.001f) { 
        perspective_factor = (perspective_denominator >= 0) ? 1000.0f : -1000.0f; 
    } else {
        perspective_factor = 1.0f / perspective_denominator;
    }

    if (perspective_factor > 100.0f) perspective_factor = 100.0f;
    if (perspective_factor < -100.0f) perspective_factor = -100.0f;

    Vec3D projected_p;
    projected_p.x = p.x * g_view_scale * perspective_factor + WINDOW_WIDTH / 2.0f;
    projected_p.y = p.y * g_view_scale * perspective_factor + WINDOW_HEIGHT / 2.0f;
    projected_p.z = p.z; 

    if (isnan(projected_p.x) || isinf(projected_p.x) ||
        isnan(projected_p.y) || isinf(projected_p.y)) {
        return (Vec3D){ -99999.0f, -99999.0f, 0.0f }; 
    }

    return projected_p;
}


void drawAttractor() {
    if (g_attractor_points == NULL || g_num_current_attractor_points < 2) {
        return;
    }

    SDL_SetRenderDrawColor(g_renderer, 200, 200, 255, 255); 

    Vec3D prev_projected_p;
    bool first_valid_point = true; 

    float min_x_draw = -WINDOW_WIDTH * DRAW_BOUND_MULTIPLIER;
    float max_x_draw = WINDOW_WIDTH * DRAW_BOUND_MULTIPLIER;
    float min_y_draw = -WINDOW_HEIGHT * DRAW_BOUND_MULTIPLIER;
    float max_y_draw = WINDOW_HEIGHT * DRAW_BOUND_MULTIPLIER;

    for (int i = SKIP_INITIAL_POINTS; i < g_num_current_attractor_points; ++i) {
        Vec3D current_point_3d = g_attractor_points[i];
        Vec3D current_projected_p = project_to_2d(current_point_3d);

        bool current_point_is_valid = 
            current_projected_p.x >= min_x_draw && current_projected_p.x <= max_x_draw &&
            current_projected_p.y >= min_y_draw && current_projected_p.y <= max_y_draw;

        if (first_valid_point) {
            if (current_point_is_valid) {
                prev_projected_p = current_projected_p;
                first_valid_point = false;
            }
        } else {
            if (current_point_is_valid) {
                SDL_RenderDrawLine(g_renderer,
                                   (int)prev_projected_p.x, (int)prev_projected_p.y,
                                   (int)current_projected_p.x, (int)current_projected_p.y);
                prev_projected_p = current_projected_p;
            } else {
                first_valid_point = true;
            }
        }
    }
}


void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font) {
        return;
    }
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (surface == NULL) {
        fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect renderQuad = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &renderQuad);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height) {
    SDL_Surface* screenshot = NULL;

    screenshot = SDL_CreateRGBSurfaceWithFormat(0, window_width, window_height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (screenshot == NULL) {
        fprintf(stderr, "Failed to create surface for screenshot: %s\n", SDL_GetError());
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels for screenshot: %s\n", SDL_GetError());
        SDL_FreeSurface(screenshot);
        return;
    }

    if (SDL_SaveBMP(screenshot, filename) != 0) {
        fprintf(stderr, "Failed to save BMP: %s\n", SDL_GetError());
    } else {
        printf("Screenshot saved to %s\n", filename);
    }

    SDL_FreeSurface(screenshot);
}


int main(int argc, char* argv[]) {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    // --- VSync/Rendering Hints for smoother output ---
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");


    printf("Initializing SDL Video...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL Video Initialized.\n");

    printf("Initializing SDL_ttf...\n");
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit(); 
        return 1;
    }
    printf("SDL_ttf Initialized.\n");

    printf("Creating window...\n");
    window = SDL_CreateWindow(
        "Chen-Lee Attractor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN |  SDL_WINDOW_OPENGL // OpenGL flag for potentially better VSync
    );
    if (window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit(); 
        SDL_Quit();
        return 1;
    }
    g_window = window; 

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    g_renderer = renderer; 

    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (g_font == NULL) {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
    } else {
        printf("Font loaded.\n");
    }

    SDL_Rect screenshotButtonRect = {WINDOW_WIDTH - 120, 10, 110, 30};

    calculateAttractorPoints();

    g_camera_x = -g_attractor_centroid.x;
    g_camera_y = -g_attractor_centroid.y;


    bool application_running = true;
    SDL_Event event; 
    bool needs_redraw = true;

    printf("Entering main loop.\n");
    while (application_running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        event.button.x >= screenshotButtonRect.x &&
                        event.button.x <= screenshotButtonRect.x + screenshotButtonRect.w &&
                        event.button.y >= screenshotButtonRect.y &&
                        event.button.y <= screenshotButtonRect.y + screenshotButtonRect.h) {
                        int current_window_width, current_window_height;
                        SDL_GetWindowSize(g_window, &current_window_width, &current_window_height);
                        saveScreenshot(g_renderer, "chenlee_attractor_screenshot.bmp", current_window_width, current_window_height);
                    } else if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_rotating = true;
                        g_last_mouse_x = event.button.x;
                        g_last_mouse_y = event.button.y;
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                         g_is_panning = true;
                         g_last_mouse_x = event.button.x;
                         g_last_mouse_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_rotating = false;
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                        g_is_panning = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (g_is_rotating) {
                        float delta_x = (float)(event.motion.x - g_last_mouse_x);
                        float delta_y = (float)(event.motion.y - g_last_mouse_y);

                        g_rotation_y += delta_x * 0.005f;
                        g_rotation_x += delta_y * 0.005f;

                        g_last_mouse_x = event.motion.x;
                        g_last_mouse_y = event.motion.y;
                        needs_redraw = true;
                    } else if (g_is_panning) {
                        float delta_x = (float)(event.motion.x - g_last_mouse_x);
                        float delta_y = (float)(event.motion.y - g_last_mouse_y);

                        float pan_scale_factor = 1.0f / (g_view_scale * (1.0f + g_camera_z / 300.0f));
                        g_camera_x -= delta_x * pan_scale_factor;
                        g_camera_y += delta_y * pan_scale_factor;

                        g_last_mouse_x = event.motion.x;
                        g_last_mouse_y = event.motion.y;
                        needs_redraw = true;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    if (event.wheel.y > 0) {
                        g_camera_z -= 10.0f;
                    } else if (event.wheel.y < 0) {
                        g_camera_z += 10.0f;
                    }
                    if (g_camera_z < -500.0f) g_camera_z = -500.0f;
                    if (g_camera_z > 500.0f) g_camera_z = 500.0f;
                    needs_redraw = true;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_r: 
                            // Reset camera based on centroid
                            g_camera_x = -g_attractor_centroid.x; 
                            g_camera_y = -g_attractor_centroid.y; 
                            g_camera_z = 90.0f;
                            g_rotation_x = M_PIF; // Reset to new default rotation
                            g_rotation_y = -M_PIF; // Reset to new default rotation
                            g_view_scale = 10.0f;
                            g_a = 5.0f;
                            g_b = -10.0f;
                            g_c = -0.38f;
                            g_dt = 0.01f; 
                            g_current_state = (Vec3D){1.0f, 0.0f, 4.5f}; 
                            calculateAttractorPoints(); 
                            needs_redraw = true;
                            break;
                        case SDLK_w: g_camera_y -= 5.0f; needs_redraw = true; break;
                        case SDLK_s: g_camera_y += 5.0f; needs_redraw = true; break;
                        case SDLK_a: g_camera_x -= 5.0f; needs_redraw = true; break;
                        case SDLK_d: g_camera_x += 5.0f; needs_redraw = true; break;
                        case SDLK_q: g_view_scale += 1.0f; needs_redraw = true; break;
                        case SDLK_e: g_view_scale -= 1.0f; if (g_view_scale < 1.0f) g_view_scale = 1.0f; needs_redraw = true; break; 
                        case SDLK_KP_PLUS: 
                        case SDLK_PLUS:
                             g_dt += 0.001f; 
                             if (g_dt > 0.1f) g_dt = 0.1f; 
                             printf("Calling calculateAttractorPoints from dt+...\n");
                             calculateAttractorPoints(); 
                             needs_redraw = true;
                             break;
                        case SDLK_KP_MINUS: 
                        case SDLK_MINUS:
                             g_dt -= 0.001f; 
                             if (g_dt < 0.0001f) g_dt = 0.0001f; 
                             printf("Calling calculateAttractorPoints from dt-...\n");
                             calculateAttractorPoints(); 
                             needs_redraw = true;
                             break;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        needs_redraw = true;
                    }
                    break;
            }
        }

        if (needs_redraw) {
            SDL_SetRenderDrawColor(g_renderer, 20, 20, 30, 255); 
            SDL_RenderClear(g_renderer);
            drawAttractor();
            needs_redraw = false;
        }

        int current_window_width, current_window_height;
        SDL_GetWindowSize(g_window, &current_window_width, &current_window_height);
        screenshotButtonRect.x = current_window_width - 120; 

        SDL_Color textColor = {255, 255, 255, 255}; 
        char text_buffer[200];

        snprintf(text_buffer, sizeof(text_buffer), "A: %.2f, B: %.2f, C: %.2f", g_a, g_b, g_c);
        renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "dt: %.4f, Points: %d", g_dt, g_num_current_attractor_points);
        renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "Cam (X:%.0f, Y:%.0f, Z:%.0f)",
                 g_camera_x + g_attractor_centroid.x,
                 g_camera_y + g_attractor_centroid.y,
                 g_camera_z + g_attractor_centroid.z);
        renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "Cam Rot (X:%.1f, Y:%.1f)", g_rotation_x * 180.0f / M_PIF, g_rotation_y * 180.0f / M_PIF);
        renderText(g_renderer, g_font, text_buffer, 10, 70, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.1f", g_view_scale);
        renderText(g_renderer, g_font, text_buffer, 10, 90, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "Left Drag: Rotate, Middle Drag: Pan, Wheel: Zoom Z");
        renderText(g_renderer, g_font, text_buffer, 10, current_window_height - 70, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "WASD: Move Cam, Q/E: Scale, +/-: dt");
        renderText(g_renderer, g_font, text_buffer, 10, current_window_height - 50, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "R: Reset View & Params");
        renderText(g_renderer, g_font, text_buffer, 10, current_window_height - 30, textColor);

        SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(g_renderer, &screenshotButtonRect);
        SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(g_renderer, &screenshotButtonRect);

        SDL_Color buttonTextColor = {255, 255, 255, 255}; 
        snprintf(text_buffer, sizeof(text_buffer), "Save");
        renderText(g_renderer, g_font, text_buffer, screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);

        SDL_RenderPresent(g_renderer); 
    }

    if (g_attractor_points != NULL) {
        free(g_attractor_points);
        g_attractor_points = NULL;
        printf("Freed g_attractor_points.\n");
    }
    if (g_font != NULL) {
        TTF_CloseFont(g_font);
        g_font = NULL;
        printf("Closed font.\n");
    }
    TTF_Quit();
    printf("SDL_ttf Quit.\n");
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
        printf("Destroyed renderer.\n");
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        printf("Destroyed window.\n");
    }
    SDL_Quit();
    return 0;
}
