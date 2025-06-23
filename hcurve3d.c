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
#define INITIAL_LENGTH 150.0 // Initial size of the H-curve

// Global SDL variables
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
TTF_Font* g_font = NULL;

// --- 3D Camera Parameters ---
double g_camera_x = 0.0;
double g_camera_y = 0.0;
double g_camera_z = 300.0;

double g_rotation_x = 0.0;
double g_rotation_y = 0.0;
double g_rotation_z = 0.0;

// --- Interaction State ---
bool g_is_rotating = false;
int g_last_mouse_x, g_last_mouse_y;
int g_max_depth = 4;

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    double m[4][4];
} Matrix4x4;

// --- Forward Declarations ---
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void saveScreenshot(SDL_Renderer* renderer, const char* filename, int window_width, int window_height);

// --- Matrix Operations ---
Matrix4x4 matrix_identity() {
    Matrix4x4 m = {0};
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0;
    return m;
}

Matrix4x4 matrix_multiply(Matrix4x4 m1, Matrix4x4 m2) {
    Matrix4x4 result = {0};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            result.m[r][c] = m1.m[r][0] * m2.m[0][c] +
                             m1.m[r][1] * m2.m[1][c] +
                             m1.m[r][2] * m2.m[2][c] +
                             m1.m[r][3] * m2.m[3][c];
        }
    }
    return result;
}

Vec3 matrix_multiply_vec3(Vec3 v, Matrix4x4 m) {
    Vec3 result;
    double w = v.x * m.m[3][0] + v.y * m.m[3][1] + v.z * m.m[3][2] + m.m[3][3];
    if (w == 0.0) w = 1.0; // Avoid division by zero

    result.x = (v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3]) / w;
    result.y = (v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3]) / w;
    result.z = (v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2] + m.m[2][3]) / w;
    return result;
}

Matrix4x4 matrix_create_rotation_x(double angle_rad) {
    Matrix4x4 m = matrix_identity();
    m.m[1][1] = cos(angle_rad);
    m.m[1][2] = sin(angle_rad);
    m.m[2][1] = -sin(angle_rad);
    m.m[2][2] = cos(angle_rad);
    return m;
}

Matrix4x4 matrix_create_rotation_y(double angle_rad) {
    Matrix4x4 m = matrix_identity();
    m.m[0][0] = cos(angle_rad);
    m.m[0][2] = -sin(angle_rad);
    m.m[2][0] = sin(angle_rad);
    m.m[2][2] = cos(angle_rad);
    return m;
}

Matrix4x4 matrix_create_rotation_z(double angle_rad) {
    Matrix4x4 m = matrix_identity();
    m.m[0][0] = cos(angle_rad);
    m.m[0][1] = sin(angle_rad);
    m.m[1][0] = -sin(angle_rad);
    m.m[1][1] = cos(angle_rad);
    return m;
}

Matrix4x4 matrix_create_translation(double tx, double ty, double tz) {
    Matrix4x4 m = matrix_identity();
    m.m[0][3] = tx;
    m.m[1][3] = ty;
    m.m[2][3] = tz;
    return m;
}

Matrix4x4 matrix_create_projection(int window_width, int window_height) {
    Matrix4x4 m = {0};
    double fov_degrees = 90.0;
    double aspect_ratio = (double)window_width / window_height;
    double z_near = 0.1;
    double z_far = 1000.0;

    double fov_rad = 1.0 / tan(fov_degrees * 0.5 / 180.0 * M_PI);
    m.m[0][0] = aspect_ratio * fov_rad;
    m.m[1][1] = fov_rad;
    m.m[2][2] = z_far / (z_far - z_near);
    m.m[3][2] = 1.0;
    m.m[2][3] = (-z_far * z_near) / (z_far - z_near);
    return m;
}

// --- Drawing Utilities ---
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

// --- Project and Draw 3D Line ---
bool project_point(Vec3 world_pos, int* screen_x, int* screen_y, Matrix4x4 transform_matrix, int window_width, int window_height) {
    Vec3 projected_pos = matrix_multiply_vec3(world_pos, transform_matrix);

    if (projected_pos.z < 0.0 || projected_pos.z > 1.0) {
        return false;
    }

    *screen_x = (int)(projected_pos.x * (window_width / 2.0) + (window_width / 2.0));
    *screen_y = (int)(-projected_pos.y * (window_height / 2.0) + (window_height / 2.0));

    if (*screen_x < -window_width || *screen_x > 2 * window_width ||
        *screen_y < -window_height || *screen_y > 2 * window_height) {
        return false;
    }

    return true;
}

// Draws a line between two 3D points on the screen
void draw_3d_line(Vec3 p1, Vec3 p2, Matrix4x4 total_transform, int window_width, int window_height) {
    int sx1, sy1, sx2, sy2;
    bool p1_visible = project_point(p1, &sx1, &sy1, total_transform, window_width, window_height);
    bool p2_visible = project_point(p2, &sx2, &sy2, total_transform, window_width, window_height);

    if (p1_visible && p2_visible) {
        // printf("Drawing line from (%d, %d) to (%d, %d)\n", sx1, sy1, sx2, sy2);
        SDL_RenderDrawLine(g_renderer, sx1, sy1, sx2, sy2);
    } else {
        // printf("Line culled. P1 visible: %d, P2 visible: %d\n", p1_visible, p2_visible);
    }
}

// --- H-Curve Generation ---
void h_curve_3d(Vec3 center, double length, int depth, Matrix4x4 total_transform, int window_width, int window_height) {
    if (depth <= 0) {
        return;
    }

    double half_length = length / 2.0;

    Vec3 corners[8];
    corners[0] = (Vec3){center.x - half_length, center.y - half_length, center.z - half_length}; // Back-bottom-left
    corners[1] = (Vec3){center.x + half_length, center.y - half_length, center.z - half_length}; // Back-bottom-right
    corners[2] = (Vec3){center.x - half_length, center.y + half_length, center.z - half_length}; // Back-top-left
    corners[3] = (Vec3){center.x + half_length, center.y + half_length, center.z - half_length}; // Back-top-right
    corners[4] = (Vec3){center.x - half_length, center.y - half_length, center.z + half_length}; // Front-bottom-left
    corners[5] = (Vec3){center.x + half_length, center.y - half_length, center.z + half_length}; // Front-bottom-right
    corners[6] = (Vec3){center.x - half_length, center.y + half_length, center.z + half_length}; // Front-top-left
    corners[7] = (Vec3){center.x + half_length, center.y + half_length, center.z + half_length}; // Front-top-right

    // Center points of each face
    Vec3 center_x_neg = {center.x - half_length, center.y, center.z};
    Vec3 center_x_pos = {center.x + half_length, center.y, center.z};
    Vec3 center_y_neg = {center.x, center.y - half_length, center.z};
    Vec3 center_y_pos = {center.x, center.y + half_length, center.z};
    Vec3 center_z_neg = {center.x, center.y, center.z - half_length};
    Vec3 center_z_pos = {center.x, center.y, center.z + half_length};

    // Connecting lines in the center
    draw_3d_line(center_x_neg, center_x_pos, total_transform, window_width, window_height);
    draw_3d_line(center_y_neg, center_y_pos, total_transform, window_width, window_height);
    draw_3d_line(center_z_neg, center_z_pos, total_transform, window_width, window_height);

    // H on XY planes (front/back)
    draw_3d_line(corners[0], corners[2], total_transform, window_width, window_height); // Back-left vertical
    draw_3d_line(corners[1], corners[3], total_transform, window_width, window_height); // Back-right vertical
    draw_3d_line(corners[0], corners[1], total_transform, window_width, window_height); // Back-bottom horizontal
    draw_3d_line(corners[2], corners[3], total_transform, window_width, window_height); // Back-top horizontal

    draw_3d_line(corners[4], corners[6], total_transform, window_width, window_height); // Front-left vertical
    draw_3d_line(corners[5], corners[7], total_transform, window_width, window_height); // Front-right vertical
    draw_3d_line(corners[4], corners[5], total_transform, window_width, window_height); // Front-bottom horizontal
    draw_3d_line(corners[6], corners[7], total_transform, window_width, window_height); // Front-top horizontal

    // H on XZ planes (top/bottom)
    draw_3d_line(corners[0], corners[4], total_transform, window_width, window_height); // Bottom-left Z
    draw_3d_line(corners[1], corners[5], total_transform, window_width, window_height); // Bottom-right Z
    draw_3d_line(corners[2], corners[6], total_transform, window_width, window_height); // Top-left Z
    draw_3d_line(corners[3], corners[7], total_transform, window_width, window_height); // Top-right Z

    double new_length = length / 2.0;
    
    for (int i = 0; i < 8; ++i) {
        h_curve_3d(corners[i], new_length, depth - 1, total_transform, window_width, window_height);
    }
}

// --- Main Program ---
int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));

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
        "3D H-Curve Fractal",
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

    // Load font for UI text
    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (g_font == NULL) {
        fprintf(stderr, "Failed to load font! Please check font path: /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf\nSDL_ttf Error: %s\n", TTF_GetError());
    }

    // --- Main Application Loop ---
    bool application_running = true;
    SDL_Event event;

    SDL_Rect screenshot_button_rect;

    while (application_running) {
        int current_window_width, current_window_height;
        SDL_GetWindowSize(g_window, &current_window_width, &current_window_height);

        screenshot_button_rect = (SDL_Rect){current_window_width - 120, 10, 110, 30};

        // --- Event Handling ---
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                {
                    int mouse_x = event.button.x;
                    int mouse_y = event.button.y;

                    if (event.button.button == SDL_BUTTON_LEFT &&
                        mouse_x >= screenshot_button_rect.x && mouse_x <= screenshot_button_rect.x + screenshot_button_rect.w &&
                        mouse_y >= screenshot_button_rect.y && mouse_y <= screenshot_button_rect.y + screenshot_button_rect.h) {
                        saveScreenshot(g_renderer, "h_curve_3d_screenshot.bmp", current_window_width, current_window_height);
                    }
                    else if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_rotating = true;
                        g_last_mouse_x = mouse_x;
                        g_last_mouse_y = mouse_y;
                    }
                    break;
                }
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        g_is_rotating = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (g_is_rotating) {
                        int mouse_x = event.motion.x;
                        int mouse_y = event.motion.y;

                        // Calculate change in mouse position
                        int dx = mouse_x - g_last_mouse_x;
                        int dy = mouse_y - g_last_mouse_y;

                        // Rotate based on mouse movement
                        g_rotation_y += dx * 0.01; // Yaw (around Y-axis)
                        g_rotation_x += dy * 0.01; // Pitch (around X-axis)

                        g_last_mouse_x = mouse_x;
                        g_last_mouse_y = mouse_y;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                {
                    if (event.wheel.y > 0) { // Scroll up (zoom in)
                        g_camera_z -= 10.0;
                    } else if (event.wheel.y < 0) { // Scroll down (zoom out)
                        g_camera_z += 10.0;
                    }
                    break;
                }
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: // Quit on ESC
                            application_running = false;
                            break;
                        case SDLK_w: // Move camera forward
                            g_camera_z -= 10.0;
                            break;
                        case SDLK_s: // Move camera backward
                            g_camera_z += 10.0;
                            break;
                        case SDLK_a: // Move camera left
                            g_camera_x -= 10.0;
                            break;
                        case SDLK_d: // Move camera right
                            g_camera_x += 10.0;
                            break;
                        case SDLK_q: // Move camera up
                            g_camera_y += 10.0;
                            break;
                        case SDLK_e: // Move camera down
                            g_camera_y -= 10.0;
                            break;
                        case SDLK_r: // Reset view/camera
                            g_camera_x = 0.0;
                            g_camera_y = 0.0;
                            g_camera_z = 200.0;
                            g_rotation_x = 0.0;
                            g_rotation_y = 0.0;
                            g_rotation_z = 0.0;
                            g_max_depth = 4;
                            break;
                        case SDLK_PLUS: // Increase depth
                        case SDLK_KP_PLUS:
                            if (g_max_depth < 10) g_max_depth++;
                            printf("Current Depth: %d\n", g_max_depth);
                            break;
                        case SDLK_MINUS: // Decrease depth
                        case SDLK_KP_MINUS:
                            if (g_max_depth > 0) g_max_depth--;
                            printf("Current Depth: %d\n", g_max_depth);
                            break;
                    }
                    break;
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

        // Build the total transformation matrix for the fractal
        Matrix4x4 proj_matrix = matrix_create_projection(current_window_width, current_window_height);
        Matrix4x4 mat_rot_x = matrix_create_rotation_x(g_rotation_x);
        Matrix4x4 mat_rot_y = matrix_create_rotation_y(g_rotation_y);
        Matrix4x4 mat_rot_z = matrix_create_rotation_z(g_rotation_z);
        Matrix4x4 mat_trans_camera = matrix_create_translation(g_camera_x, g_camera_y, g_camera_z);

        Matrix4x4 model_rot_matrix = matrix_multiply(mat_rot_x, matrix_multiply(mat_rot_y, mat_rot_z));
        Matrix4x4 view_projection_matrix = matrix_multiply(proj_matrix, mat_trans_camera);
        Matrix4x4 total_transform = matrix_multiply(view_projection_matrix, model_rot_matrix);

        h_curve_3d((Vec3){0.0, 0.0, 0.0}, INITIAL_LENGTH, g_max_depth, total_transform, current_window_width, current_window_height);
        
        // Render UI elements on top
        if (g_font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Depth: %d", g_max_depth); // Use g_max_depth
            renderText(g_renderer, g_font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Cam Pos: (%.0f, %.0f, %.0f)", g_camera_x, g_camera_y, g_camera_z);
            renderText(g_renderer, g_font, text_buffer, 10, 30, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Cam Rot: (X:%.1f, Y:%.1f)", g_rotation_x, g_rotation_y);
            renderText(g_renderer, g_font, text_buffer, 10, 50, textColor);

            renderText(g_renderer, g_font, "Left Drag: Rotate, Wheel: Zoom Z", 10, current_window_height - 70, textColor);
            renderText(g_renderer, g_font, "WASDQE: Move Cam, R: Reset View", 10, current_window_height - 50, textColor);
            renderText(g_renderer, g_font, "+/-: Change Depth", 10, current_window_height - 30, textColor);


            SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(g_renderer, &screenshot_button_rect);
            SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(g_renderer, &screenshot_button_rect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(g_renderer, g_font, "Save", screenshot_button_rect.x + 8, screenshot_button_rect.y + 5, buttonTextColor);
        }

        SDL_RenderPresent(g_renderer);
        SDL_Delay(1);
    }

    // --- Cleanup ---
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
