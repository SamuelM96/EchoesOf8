# EchoesOf8 - A CHIP-8 Emulator

## Usage

```bash
# Install dependencies
## Ubuntu
sudo apt install libsdl2 libsdl2-dev
## Fedora
sudo dnf install SDL2-devel
## macOS
brew install SDL2

# Build the project
./build.sh

# Run the emulator
./build/eo8 <rom>
```

> ![NOTE]
> On macOS, you'll likely get a security error about the SDL2 framework.
> You can accept the warning by going to `Settings > Privacy & Security`,
> scrolling down, and clicking `Allow` for the security warning.
> When you re-run the executable, you'll be allowed to proceed.

## Acknowledgements

- Test ROM from [corax89/chip8-test-rom](https://github.com/corax89/chip8-test-rom)
- [CHIP-8 Instruction Set](https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Instruction-Set)
- [Cowgod's Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite)
- [Octo](https://github.com/JohnEarnest/Octo)
- [Lazy Foo' Productions - Beginning Game Programming v2.0](https://lazyfoo.net/tutorials/SDL/)
