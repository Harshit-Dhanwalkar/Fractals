#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>

// Window dimensions
#define WIDTH 800
#define HEIGHT 800

typedef struct {
    double x, y, z;
} Vec3D;

typedef struct {
    double x, y, z;
} LorentzPoint;


// --- Lorentz Attractor Parameters ---
double sigma = 10.0;
double rho = 28.0;
double beta = 8.0 / 3.0;

// --- Simulation Parameters ---
double dt = 0.005;
#define MAX_LORENTZ_POINTS 200000

// Array to store the pre-calculated Lorentz points
LorentzPoint* g_lorentz_points = NULL;
int g_num_current_lorentz_points = 0;

// --- Viewing Parameters ---
double view_x_center = 0.0;
double view_y_center = 27.0;
double view_scale = 10.0;

// Panning variables
bool g_is_panning = false;
int g_last_mouse_x, g_last_mouse_y;

SDL_Texture* g_lorentz_texture = NULL;
SDL_Renderer* g_renderer = NULL;

// --- Function to compute the derivatives ---
Vec3D lorentz_deriv(Vec3D current_state) {
    Vec3D deriv;
    deriv.x = sigma * (current_state.y - current_state.x);
    deriv.y = current_state.x * (rho - current_state.z) - current_state.y;
    deriv.z = current_state.x * current_state.y - beta * current_state.z;
    return deriv;
}

// --- Runge-Kutta 4th Order Integration Step ---
void rk4_step_for_point(double* current_x, double* current_y, double* current_z, double dt_val) {
    Vec3D s = {*current_x, *current_y, *current_z};

    Vec3D k1 = lorentz_deriv(s);
    Vec3D s2 = {s.x + 0.5 * dt_val * k1.x, s.y + 0.5 * dt_val * k1.y, s.z + 0.5 * dt_val * k1.z};

    Vec3D k2 = lorentz_deriv(s2);
    Vec3D s3 = {s.x + 0.5 * dt_val * k2.x, s.y + 0.5 * dt_val * k2.y, s.z + 0.5 * dt_val * k2.z};

    Vec3D k3 = lorentz_deriv(s3);
    Vec3D s4 = {s.x + dt_val * k3.x, s.y + dt_val * k3.y, s.z + dt_val * k3.z};

    Vec3D k4 = lorentz_deriv(s4);

    *current_x += (dt_val / 6.0) * (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x);
    *current_y += (dt_val / 6.0) * (k1.y + 2.0 * k2.y + 2.0 * k3.y + k4.y);
    *current_z += (dt_val / 6.0) * (k1.z + 2.0 * k2.z + 2.0 * k3.z + k4.z);
}

// --- Coordinate Mapping Function ---
// Maps a Lorentz (x,y) point to screen pixel (px, py)
void map_lorentz_to_pixel(double lx, double ly, int* px, int* py) {
    *px = (int)(WIDTH / 2.0 + (lx - view_x_center) * view_scale);
    *py = (int)(HEIGHT / 2.0 - (ly - view_y_center) * view_scale); // Y-axis inverted for screen
}

// Maps a pixel (px, py) to Lorentz (lx, ly) for zoom/pan calculations
void map_pixel_to_lorentz(int px, int py, double* lx, double* ly) {
    *lx = view_x_center + (px - WIDTH / 2.0) / view_scale;
    *ly = view_y_center - (py - HEIGHT / 2.0) / view_scale; // Y-axis inverted for screen
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

// Function to calculate and render the entire Lorentz attractor trajectory to g_lorentz_texture
void calculateAndRenderAttractorToTexture() {
    if (!g_renderer || !g_lorentz_texture) {
        printf("Renderer or Lorentz Texture not initialized.\n");
        return;
    }

    printf("Calculating and rendering Lorentz Attractor to texture...\n");

    SDL_SetRenderTarget(g_renderer, g_lorentz_texture);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_SetRenderTarget(g_renderer, g_lorentz_texture);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);

    // Initialize Lorentz state for trajectory calculation
    double current_x = 0.1, current_y = 0.0, current_z = 0.0;

    // Allocate memory for points if not already allocated or needs resizing
    if (g_lorentz_points == NULL) {
        g_lorentz_points = (LorentzPoint*)malloc(MAX_LORENTZ_POINTS * sizeof(LorentzPoint));
        if (g_lorentz_points == NULL) {
            fprintf(stderr, "Failed to allocate memory for Lorentz points!\n");
            return;
        }
    }
    g_num_current_lorentz_points = 0;

    // Simulate and store points
    int warmup_steps = 1000;
    for (int i = 0; i < warmup_steps; ++i) {
        rk4_step_for_point(&current_x, &current_y, &current_z, dt);
    }


    for (int i = 0; i < MAX_LORENTZ_POINTS; ++i) {
        g_lorentz_points[i] = (LorentzPoint){current_x, current_y, current_z};
        g_num_current_lorentz_points++;

        // Perform one RK4 step
        rk4_step_for_point(&current_x, &current_y, &current_z, dt);
    }

    if (g_num_current_lorentz_points > 1) {
        int prev_px, prev_py;
        map_lorentz_to_pixel(g_lorentz_points[0].x, g_lorentz_points[0].y, &prev_px, &prev_py);

        for (int i = 1; i < g_num_current_lorentz_points; ++i) {
            int current_px, current_py;
            map_lorentz_to_pixel(g_lorentz_points[i].x, g_lorentz_points[i].y, &current_px, &current_py);

            if (fabs(current_px - prev_px) < WIDTH * 2 && fabs(current_py - prev_py) < HEIGHT * 2) {
                 SDL_RenderDrawLine(g_renderer, prev_px, prev_py, current_px, current_py);
            }

            prev_px = current_px;
            prev_py = current_py;
        }
    }
    SDL_SetRenderTarget(g_renderer, NULL);
    printf("Lorentz Attractor rendering to texture complete. Points: %d\n", g_num_current_lorentz_points);
}


// --- Reset Function ---
void reset_lorentz_view_and_params() {
    sigma = 10.0;
    rho = 28.0;
    beta = 8.0 / 3.0;
    dt = 0.005;

    // Reset view
    view_x_center = 0.0;
    view_y_center = 27.0;
    view_scale = 10.0;

    calculateAndRenderAttractorToTexture();
}

int main() {
    printf("Lorentz Attractor Viewer\n");
    printf("Left Click + Drag: Pan the view\n");
    printf("Mouse Wheel: Zoom in/out (centered on mouse cursor)\n");
    printf("Up/Down Arrows: Adjust 'rho' parameter\n");
    printf("Left/Right Arrows: Adjust 'sigma' parameter\n");
    printf("+/-: Adjust 'dt' (time step)\n");
    printf("R: Reset view and parameters\n");
    printf("Click 'Save' button to save an image.\n");

    // Set SDL hint for Wayland if you're on a Wayland desktop
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
        "Lorentz Attractor",
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

    g_renderer = SDL_CreateRenderer(pwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(pwindow);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
    }

    // Create the texture for the Lorentz attractor plot
    g_lorentz_texture = SDL_CreateTexture(g_renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          WIDTH, HEIGHT);
    if (g_lorentz_texture == NULL) {
        printf("Failed to create Lorentz texture: %s\n", SDL_GetError());
        if (font != NULL) TTF_CloseFont(font);
        TTF_Quit();
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(pwindow);
        SDL_Quit();
        return 1;
    }

    // Initial calculation and render of the attractor to its texture
    calculateAndRenderAttractorToTexture();

    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        bool re_render_attractor = false;

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
                            saveScreenshot(g_renderer, "lorentz_attractor_screenshot.bmp");
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
                        re_render_attractor = true;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (g_is_panning) {
                        int mouseX = event.motion.x;
                        int mouseY = event.motion.y;

                        // Calculate pixel difference
                        int dx = mouseX - g_last_mouse_x;
                        int dy = mouseY - g_last_mouse_y;

                        // Convert pixel difference to Lorentz coordinate difference
                        view_x_center -= (double)dx / view_scale;
                        view_y_center += (double)dy / view_scale; // Y-axis inverted

                        g_last_mouse_x = mouseX;
                        g_last_mouse_y = mouseY;
                        re_render_attractor = true; // Needs re-rendering on motion while panning
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

                        // Get Lorentz coordinates under the mouse before zooming
                        double lx_mouse, ly_mouse;
                        map_pixel_to_lorentz(mouseX, mouseY, &lx_mouse, &ly_mouse);

                        view_scale *= zoom_factor;

                        view_x_center = lx_mouse - (mouseX - WIDTH / 2.0) / view_scale;
                        view_y_center = ly_mouse + (mouseY - HEIGHT / 2.0) / view_scale; // Y-axis inverted
                        re_render_attractor = true;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_r) {
                        reset_lorentz_view_and_params();
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        rho += 0.5;
                        printf("Rho increased to: %.2f\n", rho);
                        re_render_attractor = true;
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        rho -= 0.5;
                        if (rho < 0.1) rho = 0.1;
                        printf("Rho decreased to: %.2f\n", rho);
                        re_render_attractor = true;
                    } else if (event.key.keysym.sym == SDLK_LEFT) {
                        sigma -= 0.5;
                        if (sigma < 0.1) sigma = 0.1;
                        printf("Sigma decreased to: %.2f\n", sigma);
                        re_render_attractor = true;
                    } else if (event.key.keysym.sym == SDLK_RIGHT) {
                        sigma += 0.5;
                        printf("Sigma increased to: %.2f\n", sigma);
                        re_render_attractor = true;
                    } else if (event.key.keysym.sym == SDLK_EQUALS) {
                        dt *= 1.1;
                        printf("dt increased to: %.4f\n", dt);
                        re_render_attractor = true;
                    } else if (event.key.keysym.sym == SDLK_MINUS) {
                        dt /= 1.1;
                        if (dt < 0.0001) dt = 0.0001;
                        printf("dt decreased to: %.4f\n", dt);
                        re_render_attractor = true;
                    }
                    break;
            }
        }

        if (re_render_attractor) {
            calculateAndRenderAttractorToTexture();
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_lorentz_texture, NULL, NULL);

        if (font != NULL) {
            char text_buffer[200];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Sigma: %.2f Rho: %.2f Beta: %.2f", sigma, rho, beta);
            renderText(g_renderer, font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "dt: %.4f Points: %d", dt, g_num_current_lorentz_points);
            renderText(g_renderer, font, text_buffer, 10, 30, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "View Scale: %.2f (px/unit)", view_scale);
            renderText(g_renderer, font, text_buffer, 10, 50, textColor);
            
            snprintf(text_buffer, sizeof(text_buffer), "View Center: (%.2f, %.2f)", view_x_center, view_y_center);
            renderText(g_renderer, font, text_buffer, 10, 70, textColor);


            renderText(g_renderer, font, "Left Drag: Pan, Wheel: Zoom, Arrows: Params", 10, HEIGHT - 50, textColor);
            renderText(g_renderer, font, "R: Reset, +/-: dt", 10, HEIGHT - 20, textColor);


            // Draw and render text for the screenshot button
            SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(g_renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(g_renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(g_renderer, font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);
        }

        SDL_RenderPresent(g_renderer);
    }

    // --- Cleanup ---
    if (g_lorentz_points != NULL) {
        free(g_lorentz_points);
        g_lorentz_points = NULL;
    }
    if (g_lorentz_texture != NULL) {
        SDL_DestroyTexture(g_lorentz_texture);
    }
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
    }
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0;
}
