FROM archlinux:latest

WORKDIR /app

COPY . .

RUN pacman -Sy --noconfirm base-devel make git clang argparse opencv sdl2 sdl2_mixer xxd pkg-config  
RUN pacman -Sy --noconfirm qt6-base qt6-wayland qt6-tools
RUN pacman -Sy --noconfirm vtk hdf5


CMD ["make"]