#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>

// Window dimensions
#define WIDTH 960
#define HEIGHT 960

// L-System rules
const char* RULE_A = "ABA";
const char* RULE_B = "BBB";
const char* INITIAL_STATE = "A";

// --- L-System String Generation Functions ---
const char* get_rule_expansion(char c) {
    if (c == 'A') return RULE_A;
    if (c == 'B') return RULE_B;
    return "";
}

char* expand_lsystem_iteration(const char* current_str) {
    if (!current_str) return NULL;

    size_t current_len = strlen(current_str);
    size_t estimated_new_len = current_len * 3 + 1;
    char* new_str = (char*)malloc(estimated_new_len);
    if (!new_str) {
        perror("Failed to allocate memory for new L-system string");
        return NULL;
    }
    new_str[0] = '\0';

    size_t current_pos = 0;
    for (size_t i = 0; i < current_len; ++i) {
        const char* expansion = get_rule_expansion(current_str[i]);
        size_t expansion_len = strlen(expansion);

        while (current_pos + expansion_len + 1 >= estimated_new_len) {
            estimated_new_len += 500; 
            char* temp = (char*)realloc(new_str, estimated_new_len);
            if (!temp) {
                perror("Failed to reallocate memory for new L-system string");
                free(new_str);
                return NULL;
            }
            new_str = temp;
        }
        strcpy(new_str + current_pos, expansion);
        current_pos += expansion_len;
    }
    return new_str;
}

// --- Main Drawing Function ---

void draw_lsystem_fractal(SDL_Renderer* renderer, int num_iterations) {
    int draw_origin_x = WIDTH / 2;
    int draw_origin_y = HEIGHT / 2; 

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    double band_height = (WIDTH / 2.0) / num_iterations;
    char* current_lsystem_str = strdup(INITIAL_STATE);
    if (!current_lsystem_str) {
        fprintf(stderr, "Failed to duplicate initial state string.\n");
        return;
    }

    for (int j = 0; j < num_iterations; ++j) {
        printf("Generating and drawing iteration %d (current string length: %zu)\n", j, strlen(current_lsystem_str));

        double inner_radius = j * band_height;
        double outer_radius = inner_radius + band_height + 1;

        size_t str_length = strlen(current_lsystem_str);

        for (size_t i = 0; i < str_length; ++i) {
            if (current_lsystem_str[i] == 'A') {
                double start_angle_rad = (double)i / str_length * (2 * M_PI) - M_PI;
                double end_angle_rad = (double)(i + 1) / str_length * (2 * M_PI) - M_PI;
                double angle_span = end_angle_rad - start_angle_rad;

                int num_steps_for_segment = fmax(2, (int)(fabs(angle_span) * 500.0 / M_PI));
                num_steps_for_segment = fmin(num_steps_for_segment, 50);

                for (double r = inner_radius; r <= outer_radius; r += 1.0) {
                    for (int k = 0; k < num_steps_for_segment; ++k) {
                        double current_angle = start_angle_rad + (double)k / num_steps_for_segment * angle_span;
                        int x_coord = (int)(draw_origin_x + r * cos(current_angle));
                        int y_coord = (int)(draw_origin_y + r * sin(current_angle));
                        SDL_RenderDrawPoint(renderer, x_coord, y_coord);
                    }
                }
            }
        }

        if (j < num_iterations - 1) {
            char* next_lsystem_str = expand_lsystem_iteration(current_lsystem_str);
            free(current_lsystem_str);
            current_lsystem_str = next_lsystem_str;
            if (!current_lsystem_str) {
                fprintf(stderr, "Failed to expand L-system string for next iteration.\n");
                break;
            }
        }
    }

    free(current_lsystem_str);

    SDL_RenderPresent(renderer);
}

// --- Main Program ---
int main() {
    int num_iterations;

    printf("Welcome to the L-System Fractal Generator!\n");
    printf("This fractal's complexity grows exponentially with iterations.\n");
    printf("Higher numbers will take significantly longer to generate and may consume a lot of memory.\n");
    printf("Recommended max iterations: 10-11 for quick results.\n");
    printf("Values above 12-13 can be very slow or crash due to memory usage.\n");

    while (true) {
        printf("Enter the number of iterations (0 to ~13 recommended): ");
        if (scanf("%d", &num_iterations) == 1) {
            if (num_iterations >= 0) {
                break;
            } else {
                printf("Please enter a non-negative number.\n");
            }
        } else {
            printf("Invalid input. Please enter an integer.\n");
            while (getchar() != '\n');
        }
    }

    printf("Generating L-System Fractal with %d iterations...\n", num_iterations);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* pwindow = SDL_CreateWindow(
        "L-System Fractal (User Iterations)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN
    );
    if (pwindow == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(pwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(pwindow);
        SDL_Quit();
        return 1;
    }

    draw_lsystem_fractal(renderer, num_iterations);

    // --- Event Loop to keep window open ---
    bool application_running = true;
    SDL_Event event;

    while (application_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                application_running = false;
            }
        }
        SDL_Delay(10);
    }

    // --- Cleanup ---
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();

    return 0;
}
