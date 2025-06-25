#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

// Window dimensions
int g_window_width = 800;
int g_window_height = 800;

#ifndef M_PIF
#define M_PIF 3.14159265358979323846f
#endif

#define DRAW_BOUND_MULTIPLIER 2.0f
#define MAX_AIZAWA_POINTS 200000 
#define SKIP_INITIAL_POINTS 20000 

typedef struct {
    float x, y, z;
} Vec3D;

// --- Aizawa Attractor Parameters ---
// Equations:
// dx/dt = (z - b)x - d y
// dy/dt = d x + (z - b)y
// dz/dt = c + a z - z^3/3 - (x^2 + y^2)(1 + e z) + f x z
float g_a = 0.95f;
float g_b = 0.7f;
float g_c = 0.6f;
float g_d = 3.5f;
float g_e = 0.25f;
float g_f = 0.1f;

// Simulation time step (dt) for the RK4 integration
float g_dt = 0.01f;

// Initial state for Aizawa attractor
Vec3D g_initial_aizawa_state = {0.1f, 0.0f, 0.0f}; 

// Array to store the pre-calculated Aizawa points
Vec3D* g_aizawa_points = NULL;
int g_num_current_aizawa_points = 0;

// --- 2D Viewing Parameters ---
float g_view_x_center = 0.0f; 
float g_view_y_center = 0.0f; 
float g_view_scale = 50.0f;

// Panning variables
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

// Global SDL components
SDL_Texture* g_aizawa_texture = NULL;
SDL_Renderer* g_renderer = NULL;
TTF_Font* g_font = NULL;
SDL_Window* g_window = NULL;


// --- Forward Declarations ---
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height);
void calculateAizawaPoints();
void drawAizawaToTexture();
Vec3D aizawa_deriv(Vec3D current_state);
void rk4_step_for_point(Vec3D* s, float dt_val); 
void map_aizawa_to_pixel(float ax, float ay, int* px, int* py);
void map_pixel_to_aizawa(int px, int py, float* ax, float* ay);
void reset_aizawa_view_and_params();


Vec3D aizawa_deriv(Vec3D current_state) {
    Vec3D deriv;
    float x = current_state.x;
    float y = current_state.y;
    float z = current_state.z;

    deriv.x = (z - g_b) * x - g_d * y;
    deriv.y = g_d * x + (z - g_b) * y;
    // The z equation uses powf(z, 3.0f) for z^3, and powf(x, 2.0f) and powf(y, 2.0f) for x^2 and y^2
    deriv.z = g_c + g_a * z - powf(z, 3.0f) / 3.0f - (powf(x, 2.0f) + powf(y, 2.0f)) * (1.0f + g_e * z) + g_f * x * z;
    return deriv;
}

void rk4_step_for_point(Vec3D* s, float dt_val) {
    Vec3D k1 = aizawa_deriv(*s);
    Vec3D s2 = {s->x + 0.5f * dt_val * k1.x, s->y + 0.5f * dt_val * k1.y, s->z + 0.5f * dt_val * k1.z};

    Vec3D k2 = aizawa_deriv(s2);
    Vec3D s3 = {s->x + 0.5f * dt_val * k2.x, s->y + 0.5f * dt_val * k2.y, s->z + 0.5f * dt_val * k2.z};

    Vec3D k3 = aizawa_deriv(s3);
    Vec3D s4 = {s->x + dt_val * k3.x, s->y + dt_val * k3.y, s->z + dt_val * k3.z};

    Vec3D k4 = aizawa_deriv(s4);

    s->x += (dt_val / 6.0f) * (k1.x + 2.0f * k2.x + 2.0f * k3.x + k4.x);
    s->y += (dt_val / 6.0f) * (k1.y + 2.0f * k2.y + 2.0f * k3.y + k4.y);
    s->z += (dt_val / 6.0f) * (k1.z + 2.0f * k2.z + 2.0f * k3.z + k4.z);
}

void map_aizawa_to_pixel(float ax, float ay, int* px, int* py) {
    // Translate, scale, and center the point on the screen.
    *px = (int)(g_window_width / 2.0f + (ax - g_view_x_center) * g_view_scale);
    *py = (int)(g_window_height / 2.0f - (ay - g_view_y_center) * g_view_scale);
}

void map_pixel_to_aizawa(int px, int py, float* ax, float* ay) {
    *ax = g_view_x_center + (px - g_window_width / 2.0f) / g_view_scale;
    *ay = g_view_y_center - (py - g_window_height / 2.0f) / g_view_scale;
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

void calculateAizawaPoints() {
    if (g_aizawa_points != NULL) {
        free(g_aizawa_points);
        g_aizawa_points = NULL;
    }

    g_aizawa_points = (Vec3D*)malloc(MAX_AIZAWA_POINTS * sizeof(Vec3D));
    if (g_aizawa_points == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for Aizawa points!\n");
        g_num_current_aizawa_points = 0;
        return;
    }
    g_num_current_aizawa_points = 0; 
    // Current state for Aizawa attractor simulation
    Vec3D s = g_initial_aizawa_state; 

    for (int i = 0; i < MAX_AIZAWA_POINTS; ++i) {
        g_aizawa_points[g_num_current_aizawa_points++] = s;
        rk4_step_for_point(&s, g_dt);
    }

    printf("Finished calculating %d points.\n", g_num_current_aizawa_points);
}

void drawAizawaToTexture() {
    if (!g_renderer || !g_aizawa_texture || g_aizawa_points == NULL || g_num_current_aizawa_points < 2) {
        fprintf(stderr, "Error: Renderer, Texture, or Aizawa points not ready for drawing.\n");
        return;
    }

    SDL_SetRenderTarget(g_renderer, g_aizawa_texture);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0); 
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);

    int prev_px, prev_py;
    bool first_valid_point = true;

    float min_x_draw = -g_window_width * DRAW_BOUND_MULTIPLIER; 
    float max_x_draw = g_window_width * DRAW_BOUND_MULTIPLIER;
    float min_y_draw = -g_window_height * DRAW_BOUND_MULTIPLIER;
    float max_y_draw = g_window_height * DRAW_BOUND_MULTIPLIER;

    for (int i = SKIP_INITIAL_POINTS; i < g_num_current_aizawa_points; ++i) {
        Vec3D current_point_3d = g_aizawa_points[i];
        int current_px, current_py;
        map_aizawa_to_pixel(current_point_3d.x, current_point_3d.y, &current_px, &current_py);

        bool current_point_is_valid = 
            current_px >= min_x_draw && current_px <= max_x_draw &&
            current_py >= min_y_draw && current_py <= max_y_draw;

        if (first_valid_point) {
            if (current_point_is_valid) {
                prev_px = current_px;
                prev_py = current_py;
                first_valid_point = false; 
            }
        } else {
            if (current_point_is_valid) {
                SDL_RenderDrawLine(g_renderer, prev_px, prev_py, current_px, current_py);
                prev_px = current_px;
                prev_py = current_py;
            } else {
                first_valid_point = true; 
            }
        }
    }
    SDL_SetRenderTarget(g_renderer, NULL);
}


void reset_aizawa_view_and_params() {
    g_a = 0.95f;
    g_b = 0.7f;
    g_c = 0.6f;
    g_d = 3.5f;
    g_e = 0.25f;
    g_f = 0.1f;
    g_dt = 0.01f;
    g_initial_aizawa_state = (Vec3D){0.1f, 0.0f, 0.0f};

    // Reset 2D view
    g_view_x_center = 0.0f;
    g_view_y_center = 0.0f;
    g_view_scale = 50.0f;

    calculateAizawaPoints(); 
    drawAizawaToTexture(); 
}

int main() {
    printf("Left Click + Drag: Pan the view\n");
    printf("Mouse Wheel: Zoom in/out (centered on mouse cursor)\n");
    printf("Up/Down Arrows: Adjust 'A' parameter\n");
    printf("Left/Right Arrows: Adjust 'B' parameter\n");
    printf("C/V Keys: Adjust 'C' parameter\n");
    printf("N/M Keys: Adjust 'D' parameter\n");
    printf("E/W Keys: Adjust 'E' parameter\n");
    printf("F/G Keys: Adjust 'F' parameter\n");
    printf("Equals/Plus (+): Increase 'dt' (time step)\n");
    printf("Minus (-): Decrease 'dt' (time step)\n");
    printf("R: Reset view and parameters\n");
    printf("Click 'Save' button to save an image.\n");

    // --- VSync/Rendering Hints for smoother output ---
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");


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
        "Aizawa Attractor (2D)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        g_window_width,
        g_window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL 
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

    if (SDL_GL_SetSwapInterval(1) < 0) {
        fprintf(stderr, "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
    } else {
        printf("VSync requested.\n");
    }

    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
        if (g_font == NULL) {
            fprintf(stderr, "Failed to load font from fallback path. Text rendering will be disabled! SDL_ttf Error: %s\n", TTF_GetError());
    }

    // Create the texture for the Aizawa attractor plot
    g_aizawa_texture = SDL_CreateTexture(g_renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          g_window_width, g_window_height);
    if (g_aizawa_texture == NULL) {
        fprintf(stderr, "Failed to create Aizawa texture: %s\n", SDL_GetError());
        if (g_font != NULL) TTF_CloseFont(g_font);
        TTF_Quit();
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Initial calculation of points and rendering to the texture
    calculateAizawaPoints();
    drawAizawaToTexture();

    SDL_Rect screenshotButtonRect = {g_window_width - 120, 10, 110, 30};

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        bool re_calculate_points = false;
        bool re_draw_texture = false;

        while (SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
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
                            saveScreenshot(g_renderer, "aizawa_attractor_screenshot.bmp", g_window_width, g_window_height);
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
                    // Redraw texture when panning stops for final smooth update
                    re_draw_texture = true; 
                    break;
                case SDL_MOUSEMOTION:
                    if (g_is_panning) {
                        int mouseX = event.motion.x;
                        int mouseY = event.motion.y;

                        float dx = (float)(mouseX - g_last_mouse_x);
                        float dy = (float)(mouseY - g_last_mouse_y);

                        // Adjust view center based on mouse movement and current scale
                        g_view_x_center -= dx / g_view_scale;
                        g_view_y_center += dy / g_view_scale;

                        g_last_mouse_x = mouseX;
                        g_last_mouse_y = mouseY;
                        re_draw_texture = true;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        float zoom_factor;
                        if (event.wheel.y > 0) {
                            zoom_factor = 1.2f;
                            printf("Zooming IN. ");
                        } else if (event.wheel.y < 0) {
                            zoom_factor = 1.0f / 1.2f;
                            printf("Zooming OUT. ");
                        } else {
                            break;
                        }

                        float ax_mouse, ay_mouse;
                        map_pixel_to_aizawa(mouseX, mouseY, &ax_mouse, &ay_mouse);

                        // Apply zoom to the scale
                        g_view_scale *= zoom_factor;

                        // Re-center the view to keep the point under the mouse fixed
                        g_view_x_center = ax_mouse - (mouseX - g_window_width / 2.0f) / g_view_scale;
                        g_view_y_center = ay_mouse - (mouseY - g_window_height / 2.0f) / g_view_scale; 

                        printf("New Scale: %.2f, Center: (%.2f, %.2f)\n", g_view_scale, g_view_x_center, g_view_y_center);

                        re_draw_texture = true;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        reset_aizawa_view_and_params();
                        re_calculate_points = false; 
                        re_draw_texture = false;
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        g_a += 0.01f;
                        printf("A increased to: %.2f\n", g_a);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        g_a -= 0.01f;
                        if (g_a < 0.01f) g_a = 0.01f;
                        printf("A decreased to: %.2f\n", g_a);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_LEFT) {
                        g_b -= 0.01f;
                        printf("B decreased to: %.2f\n", g_b);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_RIGHT) {
                        g_b += 0.01f;
                        printf("B increased to: %.2f\n", g_b);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_c) {
                        g_c += 0.01f;
                        printf("C increased to: %.2f\n", g_c);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_v) {
                        g_c -= 0.01f;
                        printf("C decreased to: %.2f\n", g_c);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_n) {
                        g_d += 0.01f;
                        printf("D increased to: %.2f\n", g_d);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_m) {
                        g_d -= 0.01f;
                        printf("D decreased to: %.2f\n", g_d);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_e) {
                        g_e += 0.01f;
                        printf("E increased to: %.2f\n", g_e);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_w) {
                        g_e -= 0.01f;
                        printf("E decreased to: %.2f\n", g_e);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_f) {
                        g_f += 0.01f;
                        printf("F increased to: %.2f\n", g_f);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_g) {
                        g_f -= 0.01f;
                        printf("F decreased to: %.2f\n", g_f);
                        re_calculate_points = true;
                    }
                     else if (event.key.keysym.sym == SDLK_EQUALS || event.key.keysym.sym == SDLK_KP_PLUS) {
                        g_dt *= 1.1f;
                        printf("dt increased to: %.4f\n", g_dt);
                        re_calculate_points = true;
                    } else if (event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS) {
                        g_dt /= 1.1f;
                        if (g_dt < 0.0001f) g_dt = 0.0001f;
                        printf("dt decreased to: %.4f\n", g_dt);
                        re_calculate_points = true;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        // Update global window dimensions
                        g_window_width = event.window.data1;
                        g_window_height = event.window.data2;

                        // Recreate the texture to match the new window size
                        if (g_aizawa_texture != NULL) {
                            SDL_DestroyTexture(g_aizawa_texture);
                        }
                        g_aizawa_texture = SDL_CreateTexture(g_renderer,
                                                              SDL_PIXELFORMAT_ARGB8888,
                                                              SDL_TEXTUREACCESS_TARGET,
                                                              g_window_width, g_window_height);
                        if (g_aizawa_texture == NULL) {
                            fprintf(stderr, "Failed to re-create Aizawa texture on resize: %s\n", SDL_GetError());
                        }
                        re_draw_texture = true; // Redraw texture to fill new size
                    }
                    break;
            }
        }

        if (re_calculate_points) {
            calculateAizawaPoints();
            re_draw_texture = true;
        }

        if (re_draw_texture) {
            drawAizawaToTexture();
        }

        // --- Rendering to screen ---
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_aizawa_texture, NULL, NULL);

        // Render info texts
        SDL_Rect screenshotButtonRect = {g_window_width - 120, 10, 110, 30};

        char text_buffer[200];
        SDL_Color textColor = {255, 255, 255, 255};

        snprintf(text_buffer, sizeof(text_buffer), "A: %.2f B: %.2f C: %.2f", g_a, g_b, g_c);
        renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "D: %.2f E: %.2f F: %.2f", g_d, g_e, g_f);
        renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "dt: %.4f Points: %d", g_dt, g_num_current_aizawa_points);
        renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "View Center: (%.2f, %.2f)", g_view_x_center, g_view_y_center);
        renderText(g_renderer, g_font, text_buffer, 10, 70, textColor);

        snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.1f", g_view_scale);
        renderText(g_renderer, g_font, text_buffer, 10, 90, textColor);

        renderText(g_renderer, g_font, "Left Drag: Pan, Wheel: Zoom (to mouse cursor)", 10, g_window_height - 70, textColor);
        renderText(g_renderer, g_font, "Up/Down: A, Left/Right: B, C/V: C, N/M: D, E/W: E, F/G: F", 10, g_window_height - 50, textColor);
        renderText(g_renderer, g_font, "+/-: dt, R: Reset View & Params", 10, g_window_height - 30, textColor);


        // Draw and render text for the screenshot button
        SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(g_renderer, &screenshotButtonRect);
        SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(g_renderer, &screenshotButtonRect);

        SDL_Color buttonTextColor = {255, 255, 255, 255};
        snprintf(text_buffer, sizeof(text_buffer), "Save");
        renderText(g_renderer, g_font, text_buffer, screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);

        SDL_RenderPresent(g_renderer);
    }

    // --- Cleanup ---
    if (g_aizawa_points != NULL) {
        free(g_aizawa_points);
        g_aizawa_points = NULL;
    }
    if (g_aizawa_texture != NULL) {
        SDL_DestroyTexture(g_aizawa_texture);
        g_aizawa_texture = NULL;
    }
    if (g_font != NULL) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    TTF_Quit();
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    SDL_Quit();

    return 0;
}
