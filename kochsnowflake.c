#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 800
#define HEIGHT 800

#define MAX_RECURSION_DEPTH 5

// Global variables for zoom and pan
double g_zoom = 1.0;
double g_offset_x = 0.0;
double g_offset_y = 0.0;

// For mouse drag panning
int g_mouse_down_x = 0;
int g_mouse_down_y = 0;
bool g_is_panning = false;

// Structure to represent a 2D point
typedef struct {
    double x;
    double y;
} Point;

void drawLine(SDL_Renderer* renderer, Point p1, Point p2) {
    SDL_RenderDrawLine(renderer,
                       (int)((p1.x * g_zoom) + g_offset_x),
                       (int)((p1.y * g_zoom) + g_offset_y),
                       (int)((p2.x * g_zoom) + g_offset_x),
                       (int)((p2.y * g_zoom) + g_offset_y));
}

void drawKochCurve(SDL_Renderer* renderer, Point p1, Point p2, int depth) {
    if (depth == 0) {
        drawLine(renderer, p1, p2);
    } else {
        // Calculate the four intermediate points that form the new segments
        // p1 (A) -------- p_ab (B) --- p_cd (D) -------- p2 (E)
        //                   \         /
        //                    \       /
        //                     p_c (C)

        // Segment length
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;

        // Point B (1/3 of the way from A to E)
        Point p_ab = {p1.x + dx / 3.0, p1.y + dy / 3.0};

        // Point D (2/3 of the way from A to E)
        Point p_cd = {p1.x + 2.0 * dx / 3.0, p1.y + 2.0 * dy / 3.0};

        // Point C (tip of the equilateral triangle)
        // Rotate vector BD by 60 degrees (M_PI / 3) around B for point C
        double angle = M_PI / 3.0;
        Point p_c;
        p_c.x = p_ab.x + (p_cd.x - p_ab.x) * cos(angle) - (p_cd.y - p_ab.y) * sin(angle);
        p_c.y = p_ab.y + (p_cd.x - p_ab.x) * sin(angle) + (p_cd.y - p_ab.y) * cos(angle);

        // Recursively call drawKochCurve for the four new segments
        drawKochCurve(renderer, p1, p_ab, depth - 1); // Segment AB
        drawKochCurve(renderer, p_ab, p_c, depth - 1); // Segment BC
        drawKochCurve(renderer, p_c, p_cd, depth - 1); // Segment CD
        drawKochCurve(renderer, p_cd, p2, depth - 1); // Segment DE
    }
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
    printf("Koch Snowflake Viewer\n");
    printf("Press 'Up' arrow to increase recursion depth.\n");
    printf("Press 'Down' arrow to decrease recursion depth.\n");
    printf("Use Mouse Wheel to zoom in/out.\n");
    printf("Click and Drag with Left Mouse Button to pan.\n");
    printf("Press 'R' to reset zoom and pan.\n");
    printf("Click 'Screenshot' button in top-right to save an image.\n");
    printf("Current Depth: %d\n", MAX_RECURSION_DEPTH);

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

    SDL_Window *pwindow = SDL_CreateWindow(
        "Koch Snowflake",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
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

    // Load a font for displaying text and button
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
    }

    SDL_Rect screenshotButtonRect = {WIDTH - 120, 10, 110, 30};

    // Initial recursion depth
    int current_depth = MAX_RECURSION_DEPTH;

    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    application_running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_UP) {
                        if (current_depth < 7) {
                            current_depth++;
                            printf("Current Depth: %d\n", current_depth);
                        } else {
                            printf("Max depth reached (7).\n");
                        }
                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        if (current_depth > 0) {
                            current_depth--;
                            printf("Current Depth: %d\n", current_depth);
                        } else {
                            printf("Min depth reached (0).\n");
                        }
                    } else if (event.key.keysym.sym == SDLK_r) {
                        // Reset zoom and pan
                        g_zoom = 1.0;
                        g_offset_x = 0.0;
                        g_offset_y = 0.0;
                        current_depth = MAX_RECURSION_DEPTH;
                        printf("Resetting zoom, pan, and depth.\n");
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        double zoom_factor = 1.2;
                        int mouse_x, mouse_y;
                        SDL_GetMouseState(&mouse_x, &mouse_y);

                        if (event.wheel.y > 0) {
                            g_offset_x = mouse_x - ((mouse_x - g_offset_x) * zoom_factor);
                            g_offset_y = mouse_y - ((mouse_y - g_offset_y) * zoom_factor);
                            g_zoom *= zoom_factor;
                        } else if (event.wheel.y < 0) {
                            g_offset_x = mouse_x - ((mouse_x - g_offset_x) / zoom_factor);
                            g_offset_y = mouse_y - ((mouse_y - g_offset_y) / zoom_factor);
                            g_zoom /= zoom_factor;
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (event.button.x >= screenshotButtonRect.x &&
                            event.button.x <= screenshotButtonRect.x + screenshotButtonRect.w &&
                            event.button.y >= screenshotButtonRect.y &&
                            event.button.y <= screenshotButtonRect.y + screenshotButtonRect.h) {
                            saveScreenshot(renderer, "koch_snowflake_screenshot.bmp");
                        } else {
                            g_is_panning = true;
                            g_mouse_down_x = event.button.x;
                            g_mouse_down_y = event.button.y;
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
                        g_offset_x += (event.motion.x - g_mouse_down_x);
                        g_offset_y += (event.motion.y - g_mouse_down_y);
                        g_mouse_down_x = event.motion.x;
                        g_mouse_down_y = event.motion.y;
                    }
                    break;
            }
        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Set draw color for the snowflake
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        // Define the initial equilateral triangle points for the snowflake
        double side_length = 600.0;
        Point p1 = {WIDTH / 2.0 - side_length / 2.0, HEIGHT / 2.0 + side_length / (2.0 * sqrt(3.0)) - 50};
        Point p2 = {WIDTH / 2.0 + side_length / 2.0, HEIGHT / 2.0 + side_length / (2.0 * sqrt(3.0)) - 50};
        Point p3 = {WIDTH / 2.0, HEIGHT / 2.0 - side_length * sqrt(3.0) / 2.0 + side_length / (2.0 * sqrt(3.0)) - 50};

        // Draw the three sides of the snowflake by calling Koch curve for each side
        drawKochCurve(renderer, p1, p2, current_depth);
        drawKochCurve(renderer, p2, p3, current_depth);
        drawKochCurve(renderer, p3, p1, current_depth);

        // Render current depth and zoom level
        if (font != NULL) {
            char text_buffer[100];
            SDL_Color textColor = {255, 255, 255, 255};

            snprintf(text_buffer, sizeof(text_buffer), "Depth: %d", current_depth);
            renderText(renderer, font, text_buffer, 10, 10, textColor);

            snprintf(text_buffer, sizeof(text_buffer), "Zoom: %.2fx", g_zoom);
            renderText(renderer, font, text_buffer, 10, 40, textColor);

            // Draw and render text for the screenshot button
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); 
            SDL_RenderFillRect(renderer, &screenshotButtonRect);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &screenshotButtonRect);

            SDL_Color buttonTextColor = {255, 255, 255, 255};
            renderText(renderer, font, "Save", screenshotButtonRect.x + 8, screenshotButtonRect.y + 5, buttonTextColor);
        }


        // Present the rendered content
        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    TTF_Quit(); // Quit TTF
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0;
}
