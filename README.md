# Fractals

This repository explores various fascinating fractals coded in pure C using [SDL2](https://www.libsdl.org/) for graphics rendering. Each fractal implementation aims to be interactive, allowing for zooming, panning, and screenshot capabilities.

---

## Implemented Fractals

| Fractal                      | Image                                                 | Wikipedia                                                                         |
| :--------------------------- | :---------------------------------------------------- | :-------------------------------------------------------------------------------- |
| **Mandelbrot Set**           | ![Mandelbrot Set](assets/mandelbrot.png)              | https://en.wikipedia.org/wiki/Mandelbrot_set                                      |
| **Cantor Set**               | ![Cantor Set](assets/contor.png)                      | https://en.wikipedia.org/wiki/Cantor_set                                          |
| **Burning Ship**             | ![Burning Ship](assets/burningship.png)               | https://en.wikipedia.org/wiki/Burning_Ship_fractal                                |
| **Julia Set**                | ![Julia Set](assets/julia.png)                        | https://en.wikipedia.org/wiki/Julia_set                                           |
| **Koch Snowflake**           | ![Koch Snowflake](assets/kochsnowflake.png)           | https://en.wikipedia.org/wiki/Koch_snowflake                                      |
| **Sierpinski triangle**      | ![Sierpinski Triangle](assets/sierpinskitriangle.png) | https://en.wikipedia.org/wiki/Sierpi%C5%84ski_triangle                            |
| **Newton fractal**           | ![Newton Fractal](assets/newton.png)                  | https://en.wikipedia.org/wiki/Newton_fractal                                      |
| **Lyapunov swallow Fractal** | ![Lyapunov fractal](assets/lyapunov-swallow.png)      | https://en.wikipedia.org/wiki/Lyapunov_fractal                                    |
| **Vicsek Fractal**           | ![Vicsek Fractal](assets/vicsek-fractal.png)          | https://en.wikipedia.org/wiki/Vicsek_fractal                                      |
| **Lorenz Attractor**         | ![Lorenz Attractor](assets/lorentzattractor.png)      | https://en.wikipedia.org/wiki/Lorenz_system                                       |
| **Dragon Curve**             | ![Dragon Curve](assets/dragoncurve.png)               | https://en.wikipedia.org/wiki/Dragon_curve                                        |
| **Barnsley Fern Fractal**    | ![Barnsley Fern Fractal](assets/barnsleyfern.png)     | https://en.wikipedia.org/wiki/Barnsley_fern                                       |
| **Tricorn Fractal**          | ![Tricorn Fractal](assets/tricorn.png)                | https://en.wikipedia.org/wiki/Tricorn_(mathematics)                               |
| **Hilbert curve 3D Fractal** | ![Hcurve 3D](assets/hcurve3d.png)                     | https://en.wikipedia.org/wiki/https://en.wikipedia.org/wiki/Hilbert_curve         |
| **Biomorph Fractal**         | ![Biomorph Fractal](assets/biomorph.png)              | https://en.wikipedia.org/wiki/Biomorph, https://paulbourke.net/fractals/biomorph/ |
| **Phoenix Fractal**          | ![Phoenix Fractal](assets/phoenix.png)                | https://www.ultrafractal.com/help/index.html?/help/formulas/standard/phoenix.html |

---

## Dependencies

To compile and run these fractal programs, you'll need the SDL2 development libraries, specifically `libsdl2-dev` and `libsdl2-ttf-dev`.

**On Debian/Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install libsdl2-dev libsdl2-ttf-dev
```

On Fedora/RHEL:

```bash
sudo dnf install SDL2-devel SDL2_ttf-devel
```

On macOS (using Homebrew):

```bash
brew install sdl2 sdl2_ttf
```

---

## How to Compile and Run

This project uses a Makefile to simplify the compilation process for all fractal programs.

1. Compile the programs using `make`:

- To compile all fractal programs:

```bash
make all
```

This will compile all `.c` files listed in the `Makefile` and place the executables in the `bin/` directory.

- To compile a specific fractal program (e.g., `tricorn`):

```bash
make tricorn
```

### Makefile Commands

- `make all`: Compiles all fractal programs.
- make `<program_name>`: Compiles a specific fractal program (e.g., `make julia`).
- `make clean`: Removes all compiled executables from `bin/` and any `.bmp` screenshot files from the project root. It also attempts to remove the `bin/` directory if empty.
- `make help`: Displays a summary of `Makefile` commands.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
